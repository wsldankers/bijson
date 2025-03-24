#include <string.h>

#include <bijson/writer.h>
#include <bijson/common.h>

#include "common.h"
#include "writer.h"

typedef struct _bijson_json_parser {
	bijson_writer_t *writer;
	const byte *buffer_pos;
	const byte *buffer_end;
} _bijson_json_parser_t;

typedef bijson_error_t (*_bijson_appender_t)(bijson_writer_t *writer, const char *buffer, size_t len);

static inline bijson_error_t _bijson_parse_json_unichar(const byte *hex, uint16_t *result) {
	uint16_t value = 0;
	for(unsigned int u = 0; u < 4; u++) {
		byte c = hex[u];
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

static bijson_error_t _bijson_parser_append_unichar(_bijson_appender_t append, bijson_writer_t *writer, uint32_t unichar) {
	byte utf8[4];
	size_t len;
	if(unichar <= UINT32_C(0x7F)) {
		len = 1;
		utf8[0] = (byte)unichar;
	} else if(unichar <= UINT32_C(0x7FF)) {
		len = 2;
		utf8[0] = BYTE_C(0xC0) | (byte)(unichar >> 6);
		utf8[1] = BYTE_C(0x80) | (byte)(unichar & UINT32_C(0x3F));
	} else if(unichar <= UINT32_C(0xFFFF)) {
		len = 3;
		utf8[0] = BYTE_C(0xE0) | (byte)(unichar >> 12);
		utf8[1] = BYTE_C(0x80) | (byte)((unichar >> 6) & UINT32_C(0x3F));
		utf8[2] = BYTE_C(0x80) | (byte)(unichar & UINT32_C(0x3F));
	} else if(unichar <= UINT32_C(0x10FFFF)) {
		len = 4;
		utf8[0] = BYTE_C(0xF0) | (byte)(unichar >> 18);
		utf8[1] = BYTE_C(0x80) | (byte)((unichar >> 12) & UINT32_C(0x3F));
		utf8[2] = BYTE_C(0x80) | (byte)((unichar >> 6) & UINT32_C(0x3F));
		utf8[3] = BYTE_C(0x80) | (byte)(unichar & UINT32_C(0x3F));
	} else {
		return bijson_error_invalid_json_syntax;
	}

	// We could check for invalid ranges here, but that'll happen later as well
	// _BIJSON_ERROR_RETURN(_bijson_check_valid_utf8((const char *)utf8, len));

	return append(writer, (const char *)utf8, len);
}

static inline bijson_error_t _bijson_parse_json_string_escape(_bijson_json_parser_t *parser, _bijson_appender_t append) {
	const byte * const buffer_pos = parser->buffer_pos;
	size_t len = parser->buffer_end - buffer_pos;
	assert(len);
	assert(*buffer_pos == '\\');
	if(len < SIZE_C(2))
		return bijson_error_invalid_json_syntax;

	byte buffer_1 = buffer_pos[SIZE_C(1)];
	switch(buffer_1) {
		case '"':
		case '\\':
			_BIJSON_ERROR_RETURN(append(parser->writer, (const char *)&buffer_1, sizeof buffer_1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 'b':
			_BIJSON_ERROR_RETURN(append(parser->writer, (const char *)"\b", 1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 'f':
			_BIJSON_ERROR_RETURN(append(parser->writer, (const char *)"\f", 1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 'n':
			_BIJSON_ERROR_RETURN(append(parser->writer, (const char *)"\n", 1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 'r':
			_BIJSON_ERROR_RETURN(append(parser->writer, (const char *)"\r", 1));
			parser->buffer_pos = buffer_pos + SIZE_C(2);
			break;
		case 't':
			_BIJSON_ERROR_RETURN(append(parser->writer, (const char *)"\t", 1));
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
				byte buffer_7 = buffer_pos[SIZE_C(7)];
				if(buffer_7 != 'u' && buffer_7 != 'U')
					return bijson_error_invalid_json_syntax;
				uint16_t unichar2;
				_BIJSON_ERROR_RETURN(_bijson_parse_json_unichar(buffer_pos + SIZE_C(8), &unichar2));
				if((unichar2 & UINT16_C(0xFC00)) != UINT16_C(0xDC00))
					return bijson_error_invalid_json_syntax;

				// combine unichar and unichar and append in UTF-8
				_BIJSON_ERROR_RETURN(_bijson_parser_append_unichar(
					append, parser->writer,
					(unichar << 10) | (unichar2 & UINT16_C(0x3FF))
				));

				parser->buffer_pos = buffer_pos + SIZE_C(12);
			} else {
				// append unichar in UTF-8
				_BIJSON_ERROR_RETURN(_bijson_parser_append_unichar(append, parser->writer, unichar));
				parser->buffer_pos = buffer_pos + SIZE_C(6);
			}
			break;
		default:
			return bijson_error_invalid_json_syntax;
	}

	return NULL;
}

static inline bijson_error_t _bijson_check_control_chars(const byte *buffer_pos, const byte *buffer_end) {
	while(buffer_pos != buffer_end)
		if(!(*buffer_pos++ & BYTE_C(0xE0)))
			return bijson_error_invalid_json_syntax;
	return NULL;
}

static bijson_error_t _bijson_parse_json_string(_bijson_json_parser_t *parser, bool is_object_key) {
	const byte *buffer_pos = parser->buffer_pos;
	const byte *buffer_end = parser->buffer_end;
	assert(buffer_end - buffer_pos);
	assert(*buffer_pos == '"');
	buffer_pos++;

	const byte *next_quote = memchr(buffer_pos, '"', buffer_end - buffer_pos);
	if(!next_quote)
		return bijson_error_invalid_json_syntax;
	const byte *next_escape = memchr(buffer_pos, '\\', next_quote - buffer_pos);
	if(!next_escape) {
		// short-circuit common case
		_BIJSON_ERROR_RETURN(_bijson_check_control_chars(buffer_pos, next_quote));
		parser->buffer_pos = next_quote + SIZE_C(1);
		if(is_object_key)
			return bijson_writer_add_key(parser->writer, (const char *)buffer_pos, next_quote - buffer_pos);
		else
			return bijson_writer_add_string(parser->writer, (const char *)buffer_pos, next_quote - buffer_pos);
	}

	_bijson_appender_t append;
	if(is_object_key) {
		_BIJSON_ERROR_RETURN(bijson_writer_begin_key(parser->writer));
		append = bijson_writer_append_key;
	} else {
		_BIJSON_ERROR_RETURN(bijson_writer_begin_string(parser->writer));
		append = bijson_writer_append_string;
	}

	for(;;) {
		_BIJSON_ERROR_RETURN(_bijson_check_control_chars(buffer_pos, next_escape));
		_BIJSON_ERROR_RETURN(append(parser->writer, (const char *)buffer_pos, next_escape - buffer_pos));
		parser->buffer_pos = next_escape;
		_BIJSON_ERROR_RETURN(_bijson_parse_json_string_escape(parser, append));
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
			_BIJSON_ERROR_RETURN(append(parser->writer, (const char *)buffer_pos, next_quote - buffer_pos));
			break;
		}
	}

	if(is_object_key)
		_BIJSON_ERROR_RETURN(bijson_writer_end_key(parser->writer));
	else
		_BIJSON_ERROR_RETURN(bijson_writer_end_string(parser->writer));

	return NULL;
}

static bijson_error_t _bijson_find_json_number_end(_bijson_json_parser_t *parser) {
	const byte *buffer_pos = parser->buffer_pos;
	const byte *buffer_end = parser->buffer_end;
	assert(buffer_end - buffer_pos);
	byte c = *buffer_pos;
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
	const byte *buffer_pos = parser->buffer_pos;
	_BIJSON_ERROR_RETURN(_bijson_find_json_number_end(parser));
	return bijson_writer_add_decimal_from_string(parser->writer, (const char *)buffer_pos, parser->buffer_pos - buffer_pos);
}

static inline bool _bijson_is_json_ws(byte c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline bijson_error_t _bijson_skip_json_ws(_bijson_json_parser_t *parser) {
	const byte *buffer_end = parser->buffer_end;
	const byte *buffer_pos = parser->buffer_pos;
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
	byte c;
	const byte *buffer_end = parser->buffer_end;

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
					const byte *buffer_next = parser->buffer_pos + SIZE_C(4);
					if(buffer_next >= buffer_end || memcmp(parser->buffer_pos, "true", SIZE_C(4)))
						return bijson_error_invalid_json_syntax;
					_BIJSON_ERROR_RETURN(bijson_writer_add_true(parser->writer));
					parser->buffer_pos = buffer_next;
					break;
				}
				case 'f': {
					const byte *buffer_next = parser->buffer_pos + SIZE_C(5);
					if(buffer_next > buffer_end || memcmp(parser->buffer_pos, "false", SIZE_C(5)))
						return bijson_error_invalid_json_syntax;
					_BIJSON_ERROR_RETURN(bijson_writer_add_false(parser->writer));
					parser->buffer_pos = buffer_next;
					break;
				}
				case 'n': {
					const byte *buffer_next = parser->buffer_pos + SIZE_C(4);
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
			_bijson_writer_expect_t expect = parser->writer->expect_after_value;
			if(expect == _bijson_writer_expect_value) {
				_BIJSON_ERROR_RETURN(_bijson_skip_json_ws(parser));
				c = *parser->buffer_pos++;
				if(c == ']') {
					_BIJSON_ERROR_RETURN(bijson_writer_end_array(parser->writer));
				} else {
					if(c != ',')
						return bijson_error_invalid_json_syntax;
					// Leave this for-loop and go back to parsing a new value:
					break;
				}
			} else if(expect == _bijson_writer_expect_key) {
				_BIJSON_ERROR_RETURN(_bijson_skip_json_ws(parser));
				c = *parser->buffer_pos++;
				if(c == '}') {
					_BIJSON_ERROR_RETURN(bijson_writer_end_object(parser->writer));
				} else {
					if(c != ',')
						return bijson_error_invalid_json_syntax;
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
				assert(expect == _bijson_writer_expect_none);
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

	bijson_error_t error = _bijson_parse_json(&parser);

	if(parse_end)
		*parse_end = parser.buffer_pos - (const byte *)buffer;

	return error;
}
