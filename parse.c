#include <string.h>

#include <bijson/writer.h>

#include "buffer.h"
#include "common.h"


// For internal use only
static const char bijson_error_incomplete[] = "incomplete escape sequence";

struct bijson_parser;

typedef bijson_error_t (*_bijson_parser_continue_t)(
	struct bijson_parser *parser,
	const uint8_t *buffer,
	size_t buffer_len,
	size_t *processed_len
);

typedef struct bijson_parser {
	bijson_writer_t *writer;
	_bijson_buffer_t buffer;
	_bijson_parser_continue_t continue_function;
	uint8_t partial_string_escapes[12]; // enough for \uXXXX\uXXXX
	size_t partial_string_escapes_len;
} bijson_parser_t;

static inline bijson_error_t _bijson_parse_unichar(const uint8_t *hex, uint16_t *result) {
	uint16_t value = 0;
	for(unsigned int u = 0; u < 4; u++) {
		uint8_t c = hex[u];
		value <<= 4;  // Make room for the next 4 bits

		if (c >= '0' && c <= '9')
			value |= (c - '0');
		else if (c >= 'A' && c <= 'F')
			value |= (c - 'A' + 10);
		else if (c >= 'a' && c <= 'f')
			value |= (c - 'a' + 10);
		else
			return bijson_error_invalid_json_syntax;
	}

	*result = value;
	return NULL;
}

static bijson_error_t _bijson_parser_append_unichar(bijson_parser_t *parser, uint32_t unichar) {
	uint8_t utf8[4];
	size_t len;
	if(unichar <= UINT32_C(0x7F)) {
		len = 1;
		utf8[0] = (uint8_t)unichar;
	} else if(unichar <= UINT32_C(0x7FF)) {
		len = 2;
		utf8[0] = UINT8_C(0xC0) | (uint8_t)(unichar >> 6);
		utf8[1] = UINT8_C(0x80) | (uint8_t)(unichar & UINT32_C(0x3F));
	} else if(unichar <= UINT32_C(0xFFFF)) {
		len = 3;
		utf8[0] = UINT8_C(0xC0) | (uint8_t)(unichar >> 12);
		utf8[1] = UINT8_C(0x80) | (uint8_t)((unichar >> 6) & UINT32_C(0x3F));
		utf8[2] = UINT8_C(0x80) | (uint8_t)(unichar & UINT32_C(0x3F));
	} else if(unichar <= UINT32_C(0x1FFFFF)) {
		len = 4;
		utf8[0] = UINT8_C(0xC0) | (uint8_t)(unichar >> 18);
		utf8[1] = UINT8_C(0x80) | (uint8_t)((unichar >> 12) & UINT32_C(0x3F));
		utf8[2] = UINT8_C(0x80) | (uint8_t)((unichar >> 6) & UINT32_C(0x3F));
		utf8[3] = UINT8_C(0x80) | (uint8_t)(unichar & UINT32_C(0x3F));
	} else {
		return bijson_error_invalid_json_syntax;
	}

	// We could check for invalid ranges here, but that'll happen later as well
	// _BIJSON_ERROR_RETURN(_bijson_check_valid_utf8((const char *)utf8, len));

	return _bijson_buffer_append(&parser->buffer, utf8, len);
}

static bijson_error_t _bijson_parser_parse_string_escape(
	bijson_parser_t *parser,
	const uint8_t *buffer,
	size_t len,
	size_t *processed_len
) {
	assert(buffer);
	assert(len);
	assert(*buffer == '\\');

	if(len < SIZE_C(2))
		return bijson_error_incomplete;

	uint8_t buffer_1 = buffer[SIZE_C(1)];
	switch(buffer_1) {
			case '"':
			case '\\':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, &buffer_1, sizeof buffer_1));
				*processed_len = SIZE_C(2);
				break;
			case '\b':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\b", 1));
				*processed_len = SIZE_C(2);
				break;
			case '\f':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\f", 1));
				*processed_len = SIZE_C(2);
				break;
			case '\n':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\n", 1));
				*processed_len = SIZE_C(2);
				break;
			case '\r':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\r", 1));
				*processed_len = SIZE_C(2);
				break;
			case '\t':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\t", 1));
				*processed_len = SIZE_C(2);
				break;
			case 'u':
			case 'U':
				if(len < SIZE_C(6))
					return bijson_error_incomplete;
				uint16_t unichar;
				_BIJSON_ERROR_RETURN(_bijson_parse_unichar(buffer + SIZE_C(2), &unichar));
				if((unichar & UINT16_C(0xFC00)) == UINT16_C(0xD800)) {
					if(len < SIZE_C(12))
						return bijson_error_incomplete;
					if(buffer[SIZE_C(6)] != '\\')
						return bijson_error_invalid_json_syntax;
					uint8_t buffer_7 = buffer[SIZE_C(7)];
					if(buffer_7 != 'u' && buffer_7 != 'U')
						return bijson_error_invalid_json_syntax;
					uint16_t unichar2;
					_BIJSON_ERROR_RETURN(_bijson_parse_unichar(buffer + SIZE_C(8), &unichar2));
					if((unichar2 & UINT16_C(0xFC00)) != UINT16_C(0xDC00))
						return bijson_error_invalid_json_syntax;

					// combine unichar and unichar and append in UTF-8
					_BIJSON_ERROR_RETURN(_bijson_parser_append_unichar(
						parser,
						(unichar << 10) | (unichar2 & UINT16_C(0x3FF))
					));

					*processed_len = SIZE_C(12);
				} else {
					// append unichar in UTF-8
					_BIJSON_ERROR_RETURN(_bijson_parser_append_unichar(parser, unichar));
					*processed_len = SIZE_C(6);
				}
				break;
			default:
				return bijson_error_invalid_json_syntax;
	}

	return NULL;
}

static bijson_error_t _bijson_parser_continue_string(
	bijson_parser_t *parser,
	const uint8_t *buffer,
	size_t buffer_len,
	size_t *processed_len
) {
	assert(parser);
	assert(buffer_len);
	assert(buffer);
	assert(processed_len);

	size_t everything = buffer_len;
	const uint8_t *buffer_pos = buffer;
	size_t partial_string_escapes_len = parser->partial_string_escapes_len;

	if(parser->continue_function) {
		assert(parser->continue_function == _bijson_parser_continue_string);
	} else {
		assert(!partial_string_escapes_len);
		assert(*buffer == '"');
		buffer_pos++;
		buffer_len--;
	}

	if(partial_string_escapes_len) {
		size_t partial_string_escapes_extra_len = _bijson_size_min(
			sizeof parser->partial_string_escapes - partial_string_escapes_len,
			buffer_len
		);
		memcpy(
			parser->partial_string_escapes + partial_string_escapes_len,
			buffer_pos,
			partial_string_escapes_extra_len
		);
		partial_string_escapes_len += partial_string_escapes_extra_len;
		size_t processed_escape_len;
		bijson_error_t error = _bijson_parser_parse_string_escape(
			parser,
			parser->partial_string_escapes,
			partial_string_escapes_len,
			&processed_escape_len
		);
		if(error == bijson_error_incomplete) {
			parser->partial_string_escapes_len = partial_string_escapes_len;
			*processed_len = everything;
			parser->continue_function = _bijson_parser_continue_string;
			return NULL;
		}
		_BIJSON_ERROR_RETURN(error);
		parser->partial_string_escapes_len = SIZE_C(0);
		buffer_pos += processed_escape_len;
		buffer_len -= processed_escape_len;
	}

	while(buffer_len) {
		const uint8_t *next_escape = memchr(buffer_pos, '\\', buffer_len);
		const uint8_t *next_quote = memchr(buffer_pos, '"', buffer_len);
		if(next_escape && next_escape < next_quote) {
			size_t prefix_len = next_escape - buffer_pos;
			if(prefix_len)
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, buffer_pos, prefix_len));
			buffer_pos += prefix_len;
			buffer_len -= prefix_len;
			size_t processed_escape_len;
			bijson_error_t error = _bijson_parser_parse_string_escape(
				parser,
				buffer_pos,
				buffer_len,
				&processed_escape_len
			);
			if(error == bijson_error_incomplete) {
				memcpy(parser->partial_string_escapes, buffer_pos, buffer_len);
				parser->partial_string_escapes_len = buffer_len;
				*processed_len = everything;
				parser->continue_function = _bijson_parser_continue_string;
				return NULL;
			}
			_BIJSON_ERROR_RETURN(error);
			buffer_pos += processed_escape_len;
			buffer_len -= processed_escape_len;
		} else if(next_quote) {
			size_t prefix_len = next_quote - buffer_pos;
			if(prefix_len)
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, buffer_pos, prefix_len));
			prefix_len++;
			buffer_pos += prefix_len;
			*processed_len = buffer_pos - buffer;
			parser->continue_function = NULL;
			return NULL;
		} else {
			*processed_len = everything;
			parser->continue_function = _bijson_parser_continue_string;
			return _bijson_buffer_append(&parser->buffer, buffer_pos, buffer_len);
		}
	}

	*processed_len = buffer_pos - buffer;
	parser->continue_function = _bijson_parser_continue_string;
	return NULL;
}

bijson_error_t bijson_parser_push_chunk(bijson_parser_t *parser, const void *buffer, size_t len) {
	const uint8_t *buffer_pos = buffer;

	while(len) {
		if(parser->continue_function) {
			size_t processed_len;
			_BIJSON_ERROR_RETURN(parser->continue_function(parser, buffer_pos, len, &processed_len));
			buffer_pos += processed_len;
			len -= processed_len;
		} else {
			switch(*buffer_pos) {
				case '{':
					_BIJSON_ERROR_RETURN(bijson_writer_begin_object(parser->writer));
					break;
				case '}':
					_BIJSON_ERROR_RETURN(bijson_writer_end_object(parser->writer));
					break;
				case '[':
					_BIJSON_ERROR_RETURN(bijson_writer_begin_array(parser->writer));
					break;
				case ']':
					_BIJSON_ERROR_RETURN(bijson_writer_end_array(parser->writer));
					break;
				case '"':
					parser->continue_function = _bijson_parser_continue_string;
					buffer_pos++;
					len--;
					continue;
				default:
					// Handle other cases
					break;
			}
			buffer_pos++;
			len--;
		}

	return NULL;
}
