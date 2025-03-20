#include <string.h>

#include <bijson/writer.h>

#include "bijson/common.h"
#include "buffer.h"
#include "common.h"
#include "container.h"

typedef struct _bijson_json_parser {
	bijson_writer_t *writer;
	_bijson_buffer_t spool;
	_bijson_buffer_t stack;
	const uint8_t *buffer_pos;
	const uint8_t *buffer_end;
} _bijson_json_parser_t;

static inline bijson_error_t _bijson_parse_json_unichar(const uint8_t *hex, uint16_t *result) {
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

static bijson_error_t _bijson_parser_append_unichar(_bijson_json_parser_t *parser, uint32_t unichar) {
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
		utf8[0] = UINT8_C(0xE0) | (uint8_t)(unichar >> 12);
		utf8[1] = UINT8_C(0x80) | (uint8_t)((unichar >> 6) & UINT32_C(0x3F));
		utf8[2] = UINT8_C(0x80) | (uint8_t)(unichar & UINT32_C(0x3F));
	} else if(unichar <= UINT32_C(0x10FFFF)) {
		len = 4;
		utf8[0] = UINT8_C(0xF0) | (uint8_t)(unichar >> 18);
		utf8[1] = UINT8_C(0x80) | (uint8_t)((unichar >> 12) & UINT32_C(0x3F));
		utf8[2] = UINT8_C(0x80) | (uint8_t)((unichar >> 6) & UINT32_C(0x3F));
		utf8[3] = UINT8_C(0x80) | (uint8_t)(unichar & UINT32_C(0x3F));
	} else {
		return bijson_error_invalid_json_syntax;
	}

	// We could check for invalid ranges here, but that'll happen later as well
	// _BIJSON_ERROR_RETURN(_bijson_check_valid_utf8((const char *)utf8, len));

	return _bijson_buffer_append(&parser->spool, utf8, len);
}

static bijson_error_t _bijson_parse_json_string_escape(_bijson_json_parser_t *parser) {
	const uint8_t * const buffer_pos = parser->buffer_pos;
	size_t len = parser->buffer_end - buffer_pos;
	assert(len);
	assert(*buffer_pos == '\\');
	if(len < SIZE_C(2))
		return bijson_error_invalid_json_syntax;

	uint8_t buffer_1 = buffer_pos[SIZE_C(1)];
	switch(buffer_1) {
		case '"':
		case '\\':
			_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->spool, &buffer_1, sizeof buffer_1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 'b':
			_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->spool, "\b", 1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 'f':
			_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->spool, "\f", 1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 'n':
			_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->spool, "\n", 1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 'r':
			_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->spool, "\r", 1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 't':
			_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->spool, "\t", 1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 'u':
		case 'U':
			if(len < SIZE_C(6))
				return bijson_error_invalid_json_syntax;
			uint16_t unichar;
			_BIJSON_ERROR_RETURN(_bijson_parse_json_unichar(buffer_pos + SIZE_C(2), &unichar));
			if((unichar & UINT16_C(0xFC00)) == UINT16_C(0xD800)) {
				if(len < SIZE_C(12))
					return bijson_error_invalid_json_syntax;
				if(buffer_pos[SIZE_C(6)] != '\\')
					return bijson_error_invalid_json_syntax;
				uint8_t buffer_7 = buffer_pos[SIZE_C(7)];
				if(buffer_7 != 'u' && buffer_7 != 'U')
					return bijson_error_invalid_json_syntax;
				uint16_t unichar2;
				_BIJSON_ERROR_RETURN(_bijson_parse_json_unichar(buffer_pos + SIZE_C(8), &unichar2));
				if((unichar2 & UINT16_C(0xFC00)) != UINT16_C(0xDC00))
					return bijson_error_invalid_json_syntax;

				// combine unichar and unichar and append in UTF-8
				_BIJSON_ERROR_RETURN(_bijson_parser_append_unichar(
					parser,
					(unichar << 10) | (unichar2 & UINT16_C(0x3FF))
				));

				parser->buffer_pos = buffer_pos + SIZE_C(12);
			} else {
				// append unichar in UTF-8
				_BIJSON_ERROR_RETURN(_bijson_parser_append_unichar(parser, unichar));
				parser->buffer_pos = buffer_pos + SIZE_C(6);
			}
			break;
		default:
			return bijson_error_invalid_json_syntax;
	}

	return NULL;
}

static inline bijson_error_t _bijson_check_control_chars(const uint8_t *buffer_pos, const uint8_t *buffer_end) {
	while(buffer_pos != buffer_end)
		if(!(*buffer_pos++ & UINT8_C(0xE0)))
			return bijson_error_invalid_json_syntax;
	return NULL;
}

static bijson_error_t _bijson_parse_json_string(_bijson_json_parser_t *parser, bool is_object_key) {
	size_t spool_used = parser->spool.used;
	const uint8_t *buffer_pos = parser->buffer_pos;
	const uint8_t *buffer_end = parser->buffer_end;
	assert(buffer_end - buffer_pos);
	assert(*buffer_pos == '"');
	buffer_pos++;

	const uint8_t *next_quote = memchr(buffer_pos, '"', buffer_end - buffer_pos);
	if(!next_quote)
		return bijson_error_invalid_json_syntax;
	const uint8_t *next_escape = memchr(buffer_pos, '\\', next_quote - buffer_pos);
	if(!next_escape) {
		// short-circuit common case
		_BIJSON_ERROR_RETURN(_bijson_check_control_chars(buffer_pos, next_quote));
		parser->buffer_pos = next_quote + SIZE_C(1);
		if(is_object_key)
			return bijson_writer_add_key(parser->writer, (const char *)buffer_pos, next_quote - buffer_pos);
		else
			return bijson_writer_add_string(parser->writer, (const char *)buffer_pos, next_quote - buffer_pos);
	}

	for(;;) {
		_BIJSON_ERROR_RETURN(_bijson_check_control_chars(buffer_pos, next_escape));
		_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->spool, buffer_pos, next_escape - buffer_pos));
		parser->buffer_pos = next_escape;
		_BIJSON_ERROR_RETURN(_bijson_parse_json_string_escape(parser));
		buffer_pos = parser->buffer_pos;
		if(next_quote < buffer_pos) {
			next_quote = memchr(buffer_pos, '"', buffer_end - buffer_pos);
			if(!next_quote)
				return bijson_error_invalid_json_syntax;
		}
		next_escape = memchr(buffer_pos, '\\', next_quote - buffer_pos);
		if(!next_escape) {
			parser->buffer_pos = next_quote + SIZE_C(1);
			_BIJSON_ERROR_RETURN(_bijson_check_control_chars(buffer_pos, next_quote));
			_BIJSON_ERROR_RETURN(_bijson_buffer_append(&parser->spool, buffer_pos, next_quote - buffer_pos));
			break;
		}
	}

	size_t spool_len = parser->spool.used - spool_used;
	const char *spool_buffer = _bijson_buffer_access(&parser->spool, spool_used, spool_len);
	if(is_object_key)
		return bijson_writer_add_key(parser->writer, spool_buffer, spool_len);
	else
		return bijson_writer_add_string(parser->writer, spool_buffer, spool_len);

	return NULL;
}

static bijson_error_t _bijson_find_json_number_end(_bijson_json_parser_t *parser) {
	const uint8_t *buffer_pos = parser->buffer_pos;
	const uint8_t *buffer_end = parser->buffer_end;
	assert(buffer_end - buffer_pos);
	uint8_t c = *buffer_pos;
	if(c == '-') {
		if(++buffer_pos == buffer_end)
			return bijson_error_invalid_json_syntax;
		c = *buffer_pos;
	}

	if(c == '0') {
		if(++buffer_pos == buffer_end)
			return parser->buffer_pos = buffer_pos, NULL;
		c = *buffer_pos;
	} else while(c >= '0' && c <= '9') {
		if(++buffer_pos == buffer_end)
			return parser->buffer_pos = buffer_pos, NULL;
		c = *buffer_pos;
	}

	if(c == '.') {
		if(++buffer_pos == buffer_end)
			return bijson_error_invalid_json_syntax;
		c = *buffer_pos;
		while(c >= '0' && c <= '9') {
			if(++buffer_pos == buffer_end)
				return parser->buffer_pos = buffer_pos, NULL;
			c = *buffer_pos;
		}
	}

	if(c == 'e' || c == 'E') {
		if(++buffer_pos == buffer_end)
			return bijson_error_invalid_json_syntax;
		c = *buffer_pos;
		if(c == '-' || c == '+') {
			if(++buffer_pos == buffer_end)
				return bijson_error_invalid_json_syntax;
			c = *buffer_pos;
		}
		while(c >= '0' && c <= '9') {
			if(++buffer_pos == buffer_end)
				return parser->buffer_pos = buffer_pos, NULL;
			c = *buffer_pos;
		}
	}

	return parser->buffer_pos = buffer_pos, NULL;
}

static bijson_error_t _bijson_parse_json_number(_bijson_json_parser_t *parser) {
	const uint8_t *buffer_pos = parser->buffer_pos;
	_BIJSON_ERROR_RETURN(_bijson_find_json_number_end(parser));
	return bijson_writer_add_decimal_from_string(parser->writer, (const char *)buffer_pos, parser->buffer_pos - buffer_pos);
}

static inline bool _bijson_is_json_ws(uint8_t c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline bijson_error_t _bijson_skip_json_ws(_bijson_json_parser_t *parser) {
	const uint8_t *buffer_end = parser->buffer_end;
	const uint8_t *buffer_pos = parser->buffer_pos;
	while(buffer_pos != buffer_end) {
		if(!_bijson_is_json_ws(*buffer_pos)) {
			parser->buffer_pos = buffer_pos;
			return NULL;
		}
		buffer_pos++;
	}
	return bijson_error_invalid_json_syntax;
}

static inline bijson_error_t _bijson_parse_json(_bijson_json_parser_t *parser) {
	uint8_t c;
	const uint8_t *buffer_end = parser->buffer_end;

	for(;;) {
		// Parse a value:
		for(;;) {
			_BIJSON_ERROR_RETURN(_bijson_skip_json_ws(parser));
			// fprintf(stderr, "%zu '%c'\n", parser->buffer_end - parser->buffer_pos, *parser->buffer_pos);
			switch(*parser->buffer_pos) {
				case '[':
					_BIJSON_ERROR_RETURN(bijson_writer_begin_array(parser->writer));
					parser->buffer_pos++;
					_BIJSON_ERROR_RETURN(_bijson_skip_json_ws(parser));
					c = *parser->buffer_pos;
					if(c == ']') {
						// This array is empty, so we parsed a complete value and that means we're done:
						parser->buffer_pos++;
						_BIJSON_ERROR_RETURN(bijson_writer_end_array(parser->writer));
						break;
					} else {
						// go back and parse a value (which will be the first item in our array):
						continue;
					}
				case '{':
					_BIJSON_ERROR_RETURN(bijson_writer_begin_object(parser->writer));
					parser->buffer_pos++;
					_BIJSON_ERROR_RETURN(_bijson_skip_json_ws(parser));
					c = *parser->buffer_pos;
					if(c == '}') {
						// This object is empty, so we parsed a complete value and that means we're done:
						parser->buffer_pos++;
						_BIJSON_ERROR_RETURN(bijson_writer_end_object(parser->writer));
						break;
					} else {
						// The object is not empty so parse the key for our first value:
						if(c != '"')
							return bijson_error_invalid_json_syntax;
						_BIJSON_ERROR_RETURN(_bijson_parse_json_string(parser, true));
						_BIJSON_ERROR_RETURN(_bijson_skip_json_ws(parser));
						if(*parser->buffer_pos++ != ':')
							return bijson_error_invalid_json_syntax;
						// go back and parse a value (which will be the first item in our object):
						continue;
					}
				case '"':
					_BIJSON_ERROR_RETURN(_bijson_parse_json_string(parser, false));
					break;

				case '-':
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					_BIJSON_ERROR_RETURN(_bijson_parse_json_number(parser));
					break;
				case 't': {
					const uint8_t *buffer_next = parser->buffer_pos + SIZE_C(4);
					if(buffer_next >= buffer_end || memcmp(parser->buffer_pos, "true", SIZE_C(4)))
						return bijson_error_invalid_json_syntax;
					_BIJSON_ERROR_RETURN(bijson_writer_add_true(parser->writer));
					parser->buffer_pos = buffer_next;
					break;
				}
				case 'f': {
					const uint8_t *buffer_next = parser->buffer_pos + SIZE_C(5);
					if(buffer_next > buffer_end || memcmp(parser->buffer_pos, "false", SIZE_C(5)))
						return bijson_error_invalid_json_syntax;
					_BIJSON_ERROR_RETURN(bijson_writer_add_false(parser->writer));
					parser->buffer_pos = buffer_next;
					break;
				}
				case 'n': {
					const uint8_t *buffer_next = parser->buffer_pos + SIZE_C(4);
					if(buffer_next > buffer_end || memcmp(parser->buffer_pos, "null", SIZE_C(4)))
						return bijson_error_invalid_json_syntax;
					_BIJSON_ERROR_RETURN(bijson_writer_add_null(parser->writer));
					parser->buffer_pos = buffer_next;
					break;
				}
				default:
					return bijson_error_invalid_json_syntax;
			}
			break;
		}

		// Parse any close tags or object/array continuations:
		for(;;) {
			_bijson_spool_type_t type = _bijson_container_get_current_type(parser->writer);
			if(type == _bijson_spool_type_array) {
				_BIJSON_ERROR_RETURN(_bijson_skip_json_ws(parser));
				c = *parser->buffer_pos;
				if(c == ']') {
					parser->buffer_pos++;
					_BIJSON_ERROR_RETURN(bijson_writer_end_array(parser->writer));
				} else {
					if(c != ',')
						return bijson_error_invalid_json_syntax;
					parser->buffer_pos++;
					// Leave this for-loop and go back to parsing a new value:
					break;
				}
			} else if(type == _bijson_spool_type_object) {
				_BIJSON_ERROR_RETURN(_bijson_skip_json_ws(parser));
				c = *parser->buffer_pos;
				if(c == '}') {
					parser->buffer_pos++;
					_BIJSON_ERROR_RETURN(bijson_writer_end_object(parser->writer));
				} else {
					if(c != ',')
						return bijson_error_invalid_json_syntax;
					parser->buffer_pos++;
					_BIJSON_ERROR_RETURN(_bijson_skip_json_ws(parser));
					if(*parser->buffer_pos != '"')
						return bijson_error_invalid_json_syntax;
					_BIJSON_ERROR_RETURN(_bijson_parse_json_string(parser, true));
					_BIJSON_ERROR_RETURN(_bijson_skip_json_ws(parser));
					if(*parser->buffer_pos++ != ':')
						return bijson_error_invalid_json_syntax;
					// Leave this for-loop and go back to parsing a new value:
					break;
				}
			} else {
				// No open arrays or objects and we parsed a complete value.
				// That means we're done.
				return NULL;
			}
		}
	}
}

bijson_error_t bijson_parse_json(bijson_writer_t *writer, const void *buffer, size_t len, size_t *parse_end) {
	_bijson_json_parser_t parser = {
		.writer = writer,
		.buffer_pos = buffer,
		.buffer_end = buffer + len,
	};
	_bijson_buffer_init(&parser.spool);

	bijson_error_t error = _bijson_parse_json(&parser);

	_bijson_buffer_wipe(&parser.spool);

	if(parse_end)
		*parse_end = parser.buffer_pos - (const uint8_t *)buffer;

	return error;
}
