#include <string.h>

#include "../../include/writer.h"
#include "../../include/common.h"

#include "../common.h"
#include "../io.h"
#include "../writer.h"

typedef struct _bijson_json_parser {
	bijson_writer_t *writer;
	const byte_t *buffer_pos;
	const byte_t *buffer_end;
} _bijson_json_parser_t;

typedef bijson_error_t (*_bijson_appender_t)(bijson_writer_t *writer, const void *buffer, size_t len);

static inline bijson_error_t _bijson_parse_json_unichar(const byte_t *hex, uint16_compute_t *result) {
	uint16_compute_t value = 0;
	for(unsigned int u = 0; u < 4; u++) {
		int c = hex[u];
		value <<= 4U;  // Make room for the next 4 bits

		if (c >= '0' && c <= '9')
			value |= (uint16_compute_t)(c - '0');
		else if (c >= 'A' && c <= 'F')
			value |= (uint16_compute_t)(c - 'A' + 10);
		else if (c >= 'a' && c <= 'f')
			value |= (uint16_compute_t)(c - 'a' + 10);
		else
			_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
	}

	*result = value;
	return NULL;
}

static bijson_error_t _bijson_parser_append_unichar(
	_bijson_appender_t append,
	bijson_writer_t *writer,
	uint32_compute_t unichar
) {
	byte_t utf8[4];
	size_t len;
	if(unichar <= UINT32_C(0x7F)) {
		len = SIZE_C(1);
		utf8[0] = (byte_t)unichar;
	} else if(unichar <= UINT32_C(0x7FF)) {
		len = SIZE_C(2);
		utf8[0] = BYTE_C(0xC0) | ((unichar >> 6U) & UINT32_C(0x1F));
		utf8[1] = BYTE_C(0x80) | (unichar & UINT32_C(0x3F));
	} else if(unichar <= UINT32_C(0xFFFF)) {
		len = SIZE_C(3);
		utf8[0] = BYTE_C(0xE0) | ((unichar >> 12U) & UINT32_C(0x0F));
		utf8[1] = BYTE_C(0x80) | ((unichar >> 6U) & UINT32_C(0x3F));
		utf8[2] = BYTE_C(0x80) | (unichar & UINT32_C(0x3F));
	} else if(unichar <= UINT32_C(0x10FFFF)) {
		len = SIZE_C(4);
		utf8[0] = BYTE_C(0xF0) | ((unichar >> 18U) & UINT32_C(0x07));
		utf8[1] = BYTE_C(0x80) | ((unichar >> 12U) & UINT32_C(0x3F));
		utf8[2] = BYTE_C(0x80) | ((unichar >> 6U) & UINT32_C(0x3F));
		utf8[3] = BYTE_C(0x80) | (unichar & UINT32_C(0x3F));
	} else {
		_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
	}

	// We could check for invalid ranges here, but that'll happen later as well
	// _BIJSON_RETURN_ON_ERROR(_bijson_check_valid_utf8((const char *)utf8, len));

	return append(writer, (const char *)utf8, len);
}

static inline bijson_error_t _bijson_parse_json_string_escape(_bijson_json_parser_t *parser, _bijson_appender_t append) {
	const byte_t * const buffer_pos = parser->buffer_pos;
	size_t len = _bijson_ptrdiff(parser->buffer_end, buffer_pos);
	assert(len);
	assert(*buffer_pos == '\\');
	if(len < SIZE_C(2))
		_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);

	const byte_t * const buffer_pos1 = buffer_pos + SIZE_C(1);
	switch(*buffer_pos1) {
		case '"':
		case '/':
		case '\\':
			_BIJSON_RETURN_ON_ERROR(append(parser->writer, buffer_pos1, SIZE_C(1)));
			parser->buffer_pos = buffer_pos1 + SIZE_C(1);
			break;
		case 'b':
			_BIJSON_RETURN_ON_ERROR(append(parser->writer, "\b", SIZE_C(1)));
			parser->buffer_pos = buffer_pos1 + SIZE_C(1);
			break;
		case 'f':
			_BIJSON_RETURN_ON_ERROR(append(parser->writer, "\f", SIZE_C(1)));
			parser->buffer_pos = buffer_pos1 + SIZE_C(1);
			break;
		case 'n':
			_BIJSON_RETURN_ON_ERROR(append(parser->writer, "\n", SIZE_C(1)));
			parser->buffer_pos = buffer_pos1 + SIZE_C(1);
			break;
		case 'r':
			_BIJSON_RETURN_ON_ERROR(append(parser->writer, "\r", SIZE_C(1)));
			parser->buffer_pos = buffer_pos1 + SIZE_C(1);
			break;
		case 't':
			_BIJSON_RETURN_ON_ERROR(append(parser->writer, "\t", SIZE_C(1)));
			parser->buffer_pos = buffer_pos1 + SIZE_C(1);
			break;
		case 'u':
			if(len < SIZE_C(6))
				_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
			uint16_compute_t unichar;
			_BIJSON_RETURN_ON_ERROR(_bijson_parse_json_unichar(buffer_pos + SIZE_C(2), &unichar));
			if((unichar & UINT16_C(0xFC00)) == UINT16_C(0xD800)) {
				if(len < SIZE_C(12))
					_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
				if(buffer_pos[SIZE_C(6)] != '\\')
					_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
				int buffer_7 = buffer_pos[SIZE_C(7)];
				if(buffer_7 != 'u' && buffer_7 != 'U')
					_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
				uint16_compute_t unichar2;
				_BIJSON_RETURN_ON_ERROR(_bijson_parse_json_unichar(buffer_pos + SIZE_C(8), &unichar2));
				if((unichar2 & UINT16_C(0xFC00)) != UINT16_C(0xDC00))
					_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);

				// combine unichar and unichar and append in UTF-8
				_BIJSON_RETURN_ON_ERROR(_bijson_parser_append_unichar(
					append, parser->writer,
					(((uint32_compute_t)unichar & UINT32_C(0x3FF)) << 10U)
					| ((uint32_compute_t)unichar2 & UINT32_C(0x3FF))
				));

				parser->buffer_pos = buffer_pos + SIZE_C(12);
			} else {
				// append unichar in UTF-8
				_BIJSON_RETURN_ON_ERROR(_bijson_parser_append_unichar(append, parser->writer, unichar));
				parser->buffer_pos = buffer_pos + SIZE_C(6);
			}
			break;
		default:
			_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
	}

	return NULL;
}

static inline bijson_error_t _bijson_check_control_chars(const byte_t *buffer_pos, const byte_t *buffer_end) {
	while(buffer_pos != buffer_end)
		if(!(*buffer_pos++ & BYTE_C(0xE0)))
			_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
	return NULL;
}

static bijson_error_t _bijson_parse_json_string(_bijson_json_parser_t *parser, bool is_object_key) {
	const byte_t *buffer_pos = parser->buffer_pos;
	const byte_t *buffer_end = parser->buffer_end;
	assert(buffer_end - buffer_pos);
	assert(*buffer_pos == '"');
	buffer_pos++;

	const byte_t *next_quote = memchr(buffer_pos, '"', _bijson_ptrdiff(buffer_end, buffer_pos));
	if(!next_quote)
		_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
	const byte_t *next_escape = memchr(buffer_pos, '\\', _bijson_ptrdiff(next_quote, buffer_pos));
	if(!next_escape) {
		// short-circuit common case
		_BIJSON_RETURN_ON_ERROR(_bijson_check_control_chars(buffer_pos, next_quote));
		parser->buffer_pos = next_quote + SIZE_C(1);
		if(is_object_key)
			return bijson_writer_add_key(parser->writer, (const char *)buffer_pos, _bijson_ptrdiff(next_quote, buffer_pos));
		else
			return bijson_writer_add_string(parser->writer, (const char *)buffer_pos, _bijson_ptrdiff(next_quote, buffer_pos));
	}

	_bijson_appender_t append;
	if(is_object_key) {
		_BIJSON_RETURN_ON_ERROR(bijson_writer_begin_key(parser->writer));
		append = bijson_writer_append_key;
	} else {
		_BIJSON_RETURN_ON_ERROR(bijson_writer_begin_string(parser->writer));
		append = bijson_writer_append_string;
	}

	for(;;) {
		_BIJSON_RETURN_ON_ERROR(_bijson_check_control_chars(buffer_pos, next_escape));
		_BIJSON_RETURN_ON_ERROR(append(parser->writer, (const char *)buffer_pos, _bijson_ptrdiff(next_escape, buffer_pos)));
		parser->buffer_pos = next_escape;
		_BIJSON_RETURN_ON_ERROR(_bijson_parse_json_string_escape(parser, append));
		buffer_pos = parser->buffer_pos;
		if(next_quote < buffer_pos) {
			next_quote = memchr(buffer_pos, '"', _bijson_ptrdiff(buffer_end, buffer_pos));
			if(!next_quote)
				_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
		}
		next_escape = memchr(buffer_pos, '\\', _bijson_ptrdiff(next_quote, buffer_pos));
		if(!next_escape) {
			parser->buffer_pos = next_quote + SIZE_C(1);
			_BIJSON_RETURN_ON_ERROR(_bijson_check_control_chars(buffer_pos, next_quote));
			_BIJSON_RETURN_ON_ERROR(append(parser->writer, (const char *)buffer_pos, _bijson_ptrdiff(next_quote, buffer_pos)));
			break;
		}
	}

	if(is_object_key)
		_BIJSON_RETURN_ON_ERROR(bijson_writer_end_key(parser->writer));
	else
		_BIJSON_RETURN_ON_ERROR(bijson_writer_end_string(parser->writer));

	return NULL;
}

static bijson_error_t _bijson_find_json_number_end(_bijson_json_parser_t *parser) {
	const byte_t *buffer_pos = parser->buffer_pos;
	const byte_t *buffer_end = parser->buffer_end;
	assert(buffer_end - buffer_pos);
	int c = *buffer_pos;
	if(c == '-') {
		if(++buffer_pos == buffer_end)
			_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
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
			_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
		c = *buffer_pos;
		while(c >= '0' && c <= '9') {
			if(++buffer_pos == buffer_end)
				return parser->buffer_pos = buffer_pos, NULL;
			c = *buffer_pos;
		}
	}

	if(c == 'e' || c == 'E') {
		if(++buffer_pos == buffer_end)
			_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
		c = *buffer_pos;
		if(c == '-' || c == '+') {
			if(++buffer_pos == buffer_end)
				_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
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
	const byte_t *buffer_pos = parser->buffer_pos;
	_BIJSON_RETURN_ON_ERROR(_bijson_find_json_number_end(parser));
	return bijson_writer_add_decimal_from_string(parser->writer, (const char *)buffer_pos, _bijson_ptrdiff(parser->buffer_pos, buffer_pos));
}

static inline bool _bijson_is_json_ws(int c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline bijson_error_t _bijson_skip_json_ws(_bijson_json_parser_t *parser) {
	const byte_t *buffer_end = parser->buffer_end;
	const byte_t *buffer_pos = parser->buffer_pos;
	while(buffer_pos != buffer_end) {
		if(!_bijson_is_json_ws(*buffer_pos)) {
			parser->buffer_pos = buffer_pos;
			return NULL;
		}
		buffer_pos++;
	}
	_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
}

static inline bijson_error_t _bijson_parse_json(_bijson_json_parser_t *parser) {
	int c;
	const byte_t *buffer_end = parser->buffer_end;
	size_t nesting = 0;

	for(;;) {
		// Parse a value:
		for(;;) {
			_BIJSON_RETURN_ON_ERROR(_bijson_skip_json_ws(parser));
			// fprintf(stderr, "%zu '%c'\n", parser->buffer_end - parser->buffer_pos, *parser->buffer_pos);
			switch(*parser->buffer_pos) {
				case '[':
					_BIJSON_RETURN_ON_ERROR(bijson_writer_begin_array(parser->writer));
					parser->buffer_pos++;
					_BIJSON_RETURN_ON_ERROR(_bijson_skip_json_ws(parser));
					c = *parser->buffer_pos;
					if(c == ']') {
						// This array is empty, so we parsed a complete value and that means we're done:
						parser->buffer_pos++;
						_BIJSON_RETURN_ON_ERROR(bijson_writer_end_array(parser->writer));
						break;
					} else {
						nesting++;
						// go back and parse a value (which will be the first item in our array):
						continue;
					}
				case '{':
					_BIJSON_RETURN_ON_ERROR(bijson_writer_begin_object(parser->writer));
					parser->buffer_pos++;
					_BIJSON_RETURN_ON_ERROR(_bijson_skip_json_ws(parser));
					c = *parser->buffer_pos;
					if(c == '}') {
						// This object is empty, so we parsed a complete value and that means we're done:
						parser->buffer_pos++;
						_BIJSON_RETURN_ON_ERROR(bijson_writer_end_object(parser->writer));
						break;
					} else {
						nesting++;
						// The object is not empty so parse the key for our first value:
						if(c != '"')
							_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
						_BIJSON_RETURN_ON_ERROR(_bijson_parse_json_string(parser, true));
						_BIJSON_RETURN_ON_ERROR(_bijson_skip_json_ws(parser));
						if(*parser->buffer_pos++ != ':')
							_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
						// go back and parse a value (which will be the first item in our object):
						continue;
					}
				case '"':
					_BIJSON_RETURN_ON_ERROR(_bijson_parse_json_string(parser, false));
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
					_BIJSON_RETURN_ON_ERROR(_bijson_parse_json_number(parser));
					break;
				case 't': {
					const byte_t *buffer_next = parser->buffer_pos + SIZE_C(4);
					if(buffer_next >= buffer_end || memcmp(parser->buffer_pos, "true", SIZE_C(4)))
						_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
					_BIJSON_RETURN_ON_ERROR(bijson_writer_add_true(parser->writer));
					parser->buffer_pos = buffer_next;
					break;
				}
				case 'f': {
					const byte_t *buffer_next = parser->buffer_pos + SIZE_C(5);
					if(buffer_next > buffer_end || memcmp(parser->buffer_pos, "false", SIZE_C(5)))
						_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
					_BIJSON_RETURN_ON_ERROR(bijson_writer_add_false(parser->writer));
					parser->buffer_pos = buffer_next;
					break;
				}
				case 'n': {
					const byte_t *buffer_next = parser->buffer_pos + SIZE_C(4);
					if(buffer_next > buffer_end || memcmp(parser->buffer_pos, "null", SIZE_C(4)))
						_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
					_BIJSON_RETURN_ON_ERROR(bijson_writer_add_null(parser->writer));
					parser->buffer_pos = buffer_next;
					break;
				}
				default:
					_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
			}
			break;
		}

		// Parse any close tags or object/array continuations:
		for(;;) {
			if(!nesting)
				// No open arrays or objects and we parsed a complete value.
				// That means we're done.
				return NULL;

			_bijson_writer_expect_t expect = parser->writer->expect_after_value;
			if(expect == _bijson_writer_expect_value) {
				_BIJSON_RETURN_ON_ERROR(_bijson_skip_json_ws(parser));
				c = *parser->buffer_pos++;
				if(c == ']') {
					nesting--;
					_BIJSON_RETURN_ON_ERROR(bijson_writer_end_array(parser->writer));
				} else {
					if(c != ',')
						_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
					// Leave this for-loop and go back to parsing a new value:
					break;
				}
			} else if(expect == _bijson_writer_expect_key) {
				_BIJSON_RETURN_ON_ERROR(_bijson_skip_json_ws(parser));
				c = *parser->buffer_pos++;
				if(c == '}') {
					nesting--;
					_BIJSON_RETURN_ON_ERROR(bijson_writer_end_object(parser->writer));
				} else {
					if(c != ',')
						_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
					_BIJSON_RETURN_ON_ERROR(_bijson_skip_json_ws(parser));
					if(*parser->buffer_pos != '"')
						_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
					_BIJSON_RETURN_ON_ERROR(_bijson_parse_json_string(parser, true));
					_BIJSON_RETURN_ON_ERROR(_bijson_skip_json_ws(parser));
					if(*parser->buffer_pos++ != ':')
						_BIJSON_RETURN_ERROR(bijson_error_invalid_json_syntax);
					// Leave this for-loop and go back to parsing a new value:
					break;
				}
			} else {
				assert(expect == _bijson_writer_expect_value
					|| expect == _bijson_writer_expect_key);
				abort();
			}
		}
	}
}

bijson_error_t bijson_parse_json(bijson_writer_t *writer, const void *buffer, size_t len, size_t *parse_end) {
	_bijson_json_parser_t parser = {
		.writer = writer,
		.buffer_pos = buffer,
		.buffer_end = (const byte_t *)buffer + len,
	};

	bijson_error_t error = _bijson_parse_json(&parser);

	if(parse_end)
		*parse_end = _bijson_ptrdiff(parser.buffer_pos, buffer);

	return error;
}

typedef struct _bijson_parse_json_action_data {
	bijson_writer_t *writer;
	size_t *parse_end;
} _bijson_parse_json_action_data_t;

static bijson_error_t _bijson_parse_json_action(void *action_data, const void *buffer, size_t len) {
	return bijson_parse_json(
		((_bijson_parse_json_action_data_t *)action_data)->writer,
		buffer,
		len,
		((_bijson_parse_json_action_data_t *)action_data)->parse_end
	);
}

bijson_error_t bijson_parse_json_filename(bijson_writer_t *writer, const char *filename, size_t *parse_end) {
	_bijson_parse_json_action_data_t action_data = {
		.writer = writer,
		.parse_end = parse_end,
	};
	return _bijson_io_read_from_filename(_bijson_parse_json_action, &action_data, filename);
}

bijson_error_t bijson_parse_json_filename_at(bijson_writer_t *writer, int dir_fd, const char *filename, size_t *parse_end) {
	_bijson_parse_json_action_data_t action_data = {
		.writer = writer,
		.parse_end = parse_end,
	};
	return _bijson_io_read_from_filename_at(_bijson_parse_json_action, &action_data, dir_fd, filename);
}
