#include <string.h>

#include <bijson/writer.h>

#include "buffer.h"
#include "common.h"


// For internal use only
static const char bijson_error_incomplete[] = "incomplete escape sequence";

typedef struct bijson_parser {
	bijson_writer_t *writer;
	_bijson_buffer_t buffer;
	char partial_string_escapes[16]; // plenty for \uXXXX\uXXXX
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

static bijson_error_t _bijson_parser_parse_string_escape(bijson_parser_t *parser, const uint8_t *buffer, size_t len) {
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
				break;
			case '\b':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\b", 1));
				break;
			case '\f':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\f", 1));
				break;
			case '\n':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\n", 1));
				break;
			case '\r':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\r", 1));
				break;
			case '\t':
				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\t", 1));
				break;
			case 'u':
			case 'U':
				if(len < SIZE_C(6))
					return bijson_error_incomplete;
				uint16_t unichar;
				_BIJSON_ERROR_RETURN(_bijson_parse_unichar(buffer + SIZE_C(2), &unichar));
				if(unichar /* is the first of a surrogate pair */) {
					if(len < SIZE_C(7))
						return bijson_error_incomplete;
					if(buffer[SIZE_C(6)] != '\\')
						return bijson_error_invalid_json_syntax;
					if(len < SIZE_C(8))
						return bijson_error_incomplete;
					uint8_t buffer_7 = buffer[SIZE_C(7)];
					if(buffer_7 != 'u' && buffer_7 != 'U')
						return bijson_error_invalid_json_syntax;
					if(len < SIZE_C(12))
						return bijson_error_incomplete;
					uint16_t unichar2;
					_BIJSON_ERROR_RETURN(_bijson_parse_unichar(buffer + SIZE_C(8), &unichar2));
					// combine unichar and unichar and append in UTF-8
				} else {
					// append unichar in UTF-8
				}

				_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->buffer, "\t", 1));
				break;
			default:
				return bijson_error_invalid_json_syntax;

	}
}

static bijson_error_t _bijson_parser_continue_string(bijson_parser_t *parser, const uint8_t *buffer, size_t len) {
	size_t partial_string_escapes_len = parser->partial_string_escapes_len;
	if(partial_string_escapes_len) {
		memcpy(
			parser->partial_string_escapes + partial_string_escapes_len,
			buffer,
			_bijson_size_min(sizeof parser->partial_string_escapes - partial_string_escapes_len, len)
		);
	}
	return NULL;
}

bijson_error_t bijson_parser_push_chunk(bijson_parser_t *parser, const void *buffer, size_t len) {
	return NULL;
}
