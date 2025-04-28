#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "../include/reader.h"

#include "common.h"
#include "io.h"
#include "reader.h"
#include "reader/array.h"
#include "reader/decimal.h"
#include "reader/object.h"
#include "reader/string.h"

bijson_error_t bijson_get_value_type(const bijson_t *bijson, bijson_value_type_t *result) {
	_BIJSON_RETURN_ON_ERROR(_bijson_check_bijson(bijson));
	const byte_t *buffer = bijson->buffer;
	const byte_compute_t type = *buffer;

	switch(type & BYTE_C(0xF0)) {
		case BYTE_C(0x00):
			switch(type) {
				case BYTE_C(0x00):
					_BIJSON_RETURN_ERROR(bijson_error_file_format_error);
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

	_BIJSON_RETURN_ERROR(bijson_error_unsupported_data_type);
}

bijson_error_t bijson_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	_BIJSON_RETURN_ON_ERROR(_bijson_check_bijson(bijson));
	const byte_t *buffer = bijson->buffer;

	const byte_compute_t type = *buffer;

	switch(type & BYTE_C(0xF0)) {
		case BYTE_C(0x00):
			switch(type) {
				case BYTE_C(0x00):
					_BIJSON_RETURN_ERROR(bijson_error_file_format_error);
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

	_BIJSON_RETURN_ERROR(bijson_error_unsupported_data_type);
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
