#include "../common.h"
#include "string.h"

static const byte_t _bijson_hex[16] = "0123456789ABCDEF";

bijson_error_t _bijson_raw_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *string = bijson->buffer;
	size_t len = bijson->size;
	const byte_t *previously_written = string;
	byte_t escape[2] = "\\";
	byte_t unicode_escape[6] = "\\u00";
	_BIJSON_RETURN_ON_ERROR(callback(callback_data, "\"", 1));
	const byte_t *string_end = string + len;
	while(string < string_end) {
		const byte_t *string_pos = string;
		int c = *string++;
		byte_t plain_escape = 0;
		switch(c) {
			case '"':
				plain_escape = '"';
				break;
			case '\\':
				plain_escape = '\\';
				break;
			case '\b':
				plain_escape = 'b';
				break;
			case '\f':
				plain_escape = 'f';
				break;
			case '\n':
				plain_escape = 'n';
				break;
			case '\r':
				plain_escape = 'r';
				break;
			case '\t':
				plain_escape = 't';
				break;
			default:
				if(c >= 32)
					continue;
		}
		if(string_pos > previously_written)
			_BIJSON_RETURN_ON_ERROR(callback(callback_data, previously_written, _bijson_ptrdiff(string_pos, previously_written)));
		previously_written = string;
		if(plain_escape) {
			escape[1] = plain_escape;
			_BIJSON_RETURN_ON_ERROR(callback(callback_data, escape, sizeof escape));
		} else {
			unicode_escape[4] = _bijson_hex[(size_t)c >> 4U];
			unicode_escape[5] = _bijson_hex[(size_t)c & SIZE_C(0xF)];
			_BIJSON_RETURN_ON_ERROR(callback(callback_data, unicode_escape, sizeof unicode_escape));
		}
	}
	if(string > previously_written)
		_BIJSON_RETURN_ON_ERROR(callback(callback_data, previously_written, (size_t)(string - previously_written)));
	return callback(callback_data, "\"", SIZE_C(1));
}

bijson_error_t _bijson_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *userdata) {
	bijson_t raw_string = { .buffer = (const byte_t *)bijson->buffer + SIZE_C(1), .size = bijson->size - SIZE_C(1) };
	_BIJSON_RETURN_ON_ERROR(_bijson_check_valid_utf8(raw_string.buffer, raw_string.size));
	return _bijson_raw_string_to_json(&raw_string, callback, userdata);
}
