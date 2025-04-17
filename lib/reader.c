#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "../include/reader.h"

#include "common.h"
#include "io.h"
#include "reader.h"
#include "reader/array.h"
#include "reader/object.h"

static const byte_t _bijson_hex[16] = "0123456789ABCDEF";

bijson_error_t _bijson_raw_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *string = bijson->buffer;
	size_t len = bijson->size;
	const byte_t *previously_written = string;
	byte_t escape[2] = "\\";
	byte_t unicode_escape[6] = "\\u00";
	_BIJSON_ERROR_RETURN(callback(callback_data, "\"", 1));
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
			_BIJSON_ERROR_RETURN(callback(callback_data, previously_written, _bijson_ptrdiff(string_pos, previously_written)));
		previously_written = string;
		if(plain_escape) {
			escape[1] = plain_escape;
			_BIJSON_ERROR_RETURN(callback(callback_data, escape, sizeof escape));
		} else {
			unicode_escape[4] = _bijson_hex[(size_t)c >> 4U];
			unicode_escape[5] = _bijson_hex[(size_t)c & SIZE_C(0xF)];
			_BIJSON_ERROR_RETURN(callback(callback_data, unicode_escape, sizeof unicode_escape));
		}
	}
	if(string > previously_written)
		_BIJSON_ERROR_RETURN(callback(callback_data, previously_written, (size_t)(string - previously_written)));
	return callback(callback_data, "\"", SIZE_C(1));
}

static inline bijson_error_t _bijson_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *userdata) {
	bijson_t raw_string = { .buffer = (const byte_t *)bijson->buffer + SIZE_C(1), .size = bijson->size - SIZE_C(1) };
	_BIJSON_ERROR_RETURN(_bijson_check_valid_utf8(raw_string.buffer, raw_string.size));
	return _bijson_raw_string_to_json(&raw_string, callback, userdata);
}

static inline bijson_error_t _bijson_decimal_part_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *buffer = bijson->buffer;
	size_t size = bijson->size;

	size_t last_word_size = ((size - SIZE_C(1)) & SIZE_C(0x7)) + SIZE_C(1);

	const byte_t *last_word_start = buffer + size - last_word_size;
	uint64_t last_word = _bijson_read_minimal_int(last_word_start, last_word_size);
	if(last_word > UINT64_C(9999999999999999998))
		return bijson_error_file_format_error;
	last_word++;

	byte_t word_chars[20];
	_BIJSON_ERROR_RETURN(callback(
		callback_data,
		word_chars,
		_bijson_uint64_str(word_chars, last_word)
	));

	const byte_t *word_start = last_word_start;
	while(word_start > buffer) {
		word_start -= sizeof(uint64_t);
		uint64_t word = _bijson_read_minimal_int(word_start, sizeof(uint64_t));
		if(word > UINT64_C(9999999999999999999))
			return bijson_error_file_format_error;
		_BIJSON_ERROR_RETURN(callback(
			callback_data,
			word_chars,
			_bijson_uint64_str_padded(word_chars, word)
		));
	}

	return NULL;
}

static inline bijson_error_t _bijson_decimal_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;

	byte_compute_t type = *buffer;

	const byte_t *exponent_size_location = buffer + SIZE_C(1);
	if(exponent_size_location == buffer_end)
		return bijson_error_file_format_error;

	size_t exponent_size_size = SIZE_C(1) << (type & BYTE_C(0x3));
	const byte_t *exponent_start = exponent_size_location + exponent_size_size;
	if(exponent_start + SIZE_C(1) > buffer_end)
		return bijson_error_file_format_error;
	uint64_t raw_exponent_size = _bijson_read_minimal_int(exponent_size_location, exponent_size_size);
	if(raw_exponent_size > SIZE_MAX - SIZE_C(1))
		return bijson_error_file_format_error;
	size_t exponent_size = (size_t)raw_exponent_size + SIZE_C(1);
	if(exponent_size > _bijson_ptrdiff(buffer_end, exponent_start) - SIZE_C(1))
		return bijson_error_file_format_error;

	if(type & BYTE_C(0x4))
		_BIJSON_ERROR_RETURN(callback(callback_data, "-", SIZE_C(1)));

	const byte_t *significand_start = exponent_start + exponent_size;
	bijson_t significand = {
		significand_start,
		_bijson_ptrdiff(buffer_end, significand_start),
	};
	_BIJSON_ERROR_RETURN(_bijson_decimal_part_to_json(&significand, callback, callback_data));

	_BIJSON_ERROR_RETURN(callback(callback_data, "e", SIZE_C(1)));

	if(type & BYTE_C(0x8))
		_BIJSON_ERROR_RETURN(callback(callback_data, "-", SIZE_C(1)));

	bijson_t exponent = {
		exponent_start,
		exponent_size,
	};
	return _bijson_decimal_part_to_json(&exponent, callback, callback_data);
}

static inline bijson_error_t _bijson_decimal_integer_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const byte_t *buffer = bijson->buffer;
	size_t size = bijson->size;

	byte_compute_t type = *buffer;
	if(type & BYTE_C(0x1))
		_BIJSON_ERROR_RETURN(callback(callback_data, "-", SIZE_C(1)));

	if(size == SIZE_C(1))
		return callback(callback_data, "0", SIZE_C(1));

	bijson_t integer = {
		buffer + SIZE_C(1),
		size - SIZE_C(1),
	};
	return _bijson_decimal_part_to_json(&integer, callback, callback_data);
}

bijson_error_t bijson_get_value_type(const bijson_t *bijson, bijson_value_type_t *result) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	const byte_t *buffer = bijson->buffer;
	const byte_compute_t type = *buffer;

	switch(type & BYTE_C(0xF0)) {
		case BYTE_C(0x00):
			switch(type) {
				case BYTE_C(0x00):
					return bijson_error_file_format_error;
				case BYTE_C(0x01):
					return *result = bijson_value_type_null, NULL;
				case BYTE_C(0x05):
					return *result = bijson_value_type_undefined, NULL;
				case BYTE_C(0x02):
					return *result = bijson_value_type_false, NULL;
				case BYTE_C(0x03):
					return *result = bijson_value_type_true, NULL;
				case BYTE_C(0x08):
					return *result = bijson_value_type_string, NULL;
				case BYTE_C(0x09):
					return *result = bijson_value_type_bytes, NULL;
				case BYTE_C(0x0A):
					return *result = bijson_value_type_iee754_2008_float, NULL;
				case BYTE_C(0x0B):
					return *result = bijson_value_type_iee754_2008_decimal_float, NULL;
				case BYTE_C(0x0C):
					return *result = bijson_value_type_iee754_2008_decimal_float_dpd, NULL;
			}
			break;
		case BYTE_C(0x10):
			switch(type & BYTE_C(0xFE)) {
				case BYTE_C(0x10):
					return *result = bijson_value_type_snan, NULL;
				case BYTE_C(0x12):
					return *result = bijson_value_type_qnan, NULL;
				case BYTE_C(0x14):
					return *result = bijson_value_type_inf, NULL;
				case BYTE_C(0x18):
					return *result = bijson_value_type_integer, NULL;
				case BYTE_C(0x1A):
					return *result = bijson_value_type_decimal, NULL;
			}
			break;
		case BYTE_C(0x20):
			return *result = bijson_value_type_decimal, NULL;
		case BYTE_C(0x30):
			return *result = bijson_value_type_array, NULL;
		case BYTE_C(0x40):
		case BYTE_C(0x50):
		case BYTE_C(0x60):
		case BYTE_C(0x70):
			return *result = bijson_value_type_object, NULL;
	}

	return bijson_error_unsupported_data_type;
}

bijson_error_t bijson_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	const byte_t *buffer = bijson->buffer;

	const byte_compute_t type = *buffer;

	switch(type & BYTE_C(0xF0)) {
		case BYTE_C(0x00):
			switch(type) {
				case BYTE_C(0x00):
					return bijson_error_file_format_error;
				case BYTE_C(0x01):
				case BYTE_C(0x05): // undefined
					return callback(callback_data, "null", 4);
				case BYTE_C(0x02):
					return callback(callback_data, "false", 5);
				case BYTE_C(0x03):
					return callback(callback_data, "true", 4);
				case BYTE_C(0x08):
					return _bijson_string_to_json(bijson, callback, callback_data);
			}
			break;
		case BYTE_C(0x10):
			switch(type & BYTE_C(0xFE)) {
				case BYTE_C(0x1A):
					return _bijson_decimal_integer_to_json(bijson, callback, callback_data);
			}
			break;
		case BYTE_C(0x20):
			return _bijson_decimal_to_json(bijson, callback, callback_data);
			break;
		case BYTE_C(0x30):
			return _bijson_array_to_json(bijson, callback, callback_data);
			break;
		case BYTE_C(0x40):
		case BYTE_C(0x50):
		case BYTE_C(0x60):
		case BYTE_C(0x70):
			return _bijson_object_to_json(bijson, callback, callback_data);
			break;
	}

	return bijson_error_unsupported_data_type;
}

typedef struct _bijson_to_json_state {
	const bijson_t *bijson;
} _bijson_to_json_state_t;

static bijson_error_t _bijson_to_json_callback(
	void *action_callback_data,
	bijson_output_callback_t output_callback,
	void *output_callback_data
) {
	return bijson_to_json(
		((_bijson_to_json_state_t *)action_callback_data)->bijson,
		output_callback,
		output_callback_data
	);
}

bijson_error_t bijson_to_json_fd(const bijson_t *bijson, int fd) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_fd(_bijson_to_json_callback, &state, fd, NULL);
}

bijson_error_t bijson_to_json_FILE(const bijson_t *bijson, FILE *file) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_FILE(_bijson_to_json_callback, &state, file, NULL);
}

bijson_error_t bijson_to_json_malloc(
	const bijson_t *bijson,
	const void **result_buffer,
	size_t *result_size
) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_malloc(
		_bijson_to_json_callback,
		&state,
		result_buffer,
		result_size
	);
}

bijson_error_t bijson_to_json_tempfile(
	const bijson_t *bijson,
	const void **result_buffer,
	size_t *result_size
) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_tempfile(
		_bijson_to_json_callback,
		&state,
		result_buffer,
		result_size
	);
}

bijson_error_t bijson_to_json_bytecounter(
	const bijson_t *bijson,
	size_t *result_size
) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_bytecounter(
		_bijson_to_json_callback,
		&state,
		result_size
	);
}

bijson_error_t bijson_to_json_filename(const bijson_t *bijson, const char *filename) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_filename(_bijson_to_json_callback, &state, filename, NULL);
}

bijson_error_t bijson_open_filename(bijson_t *bijson, const char *filename) {
	return _bijson_io_read_from_filename(NULL, bijson, filename);
}

void bijson_close(bijson_t *bijson) {
	_bijson_io_close(bijson);
}

void bijson_free(bijson_t *bijson) {
	if(bijson) {
		free(_bijson_no_const(bijson->buffer));
		*bijson = bijson_0;
	}
}
