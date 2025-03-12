#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bijson/reader.h>

#include "common.h"
#include "io.h"

static inline bijson_error_t _bijson_check_bijson(const bijson_t *bijson) {
	if(!bijson || !bijson->buffer)
		return bijson_error_parameter_is_null;
	if(!bijson->size)
		return bijson_error_parameter_is_zero;
	return NULL;
}

static inline uint64_t _bijson_read_minimal_int(const uint8_t *buffer, size_t nbytes) {
	uint64_t r = 0;
	for(size_t u = 0; u < nbytes; u++)
		r |= buffer[u] << (u * SIZE_C(8));
	return r;
}

static const unsigned char hex[16] = "0123456789ABCDEF";

static bijson_error_t _bijson_raw_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const uint8_t *string = bijson->buffer;
	size_t len = bijson->size;
	const uint8_t *previously_written = string;
	uint8_t escape[2] = "\\x";
	uint8_t unicode_escape[6] = "\\u00XX";
	_BIJSON_ERROR_RETURN(callback(callback_data, "\"", 1));
	const uint8_t *string_end = string + len;
	while(string < string_end) {
		const uint8_t *string_pos = string;
		uint8_t c = *string++;
		uint8_t plain_escape = 0;
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
				if(c >= UINT8_C(32))
					continue;
		}
		if(string_pos > previously_written)
			_BIJSON_ERROR_RETURN(callback(callback_data, previously_written, string_pos - previously_written));
		previously_written = string;
		if(plain_escape) {
			escape[1] = plain_escape;
			_BIJSON_ERROR_RETURN(callback(callback_data, escape, sizeof escape));
		} else {
			unicode_escape[4] = hex[c >> 4];
			unicode_escape[5] = hex[c & UINT8_C(0xF)];
			_BIJSON_ERROR_RETURN(callback(callback_data, unicode_escape, sizeof unicode_escape));
		}
	}
	if(string > previously_written)
		_BIJSON_ERROR_RETURN(callback(callback_data, previously_written, string - previously_written));
	return callback(callback_data, "\"", 1);
}

static bijson_error_t _bijson_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *userdata) {
	bijson_t raw_string = { .buffer = bijson->buffer + SIZE_C(1), .size = bijson->size - SIZE_C(1) };
	_BIJSON_ERROR_RETURN(_bijson_check_valid_utf8((const char *)raw_string.buffer, raw_string.size));
	return _bijson_raw_string_to_json(&raw_string, callback, userdata);
}

static inline bijson_error_t _bijson_decimal_part_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const uint8_t *buffer = bijson->buffer;
	size_t size = bijson->size;

	size_t last_word_size = ((size - SIZE_C(1)) & SIZE_C(0x7)) + SIZE_C(1);

	const uint8_t *last_word_start = buffer + size - last_word_size;
	uint64_t last_word = _bijson_read_minimal_int(last_word_start, last_word_size);
	if(last_word > UINT64_C(9999999999999999998))
		return bijson_error_file_format_error;
	last_word++;

	char word_chars[20];
	_BIJSON_ERROR_RETURN(callback(callback_data, word_chars, sprintf(word_chars, "%"PRIu64, last_word)));

	const uint8_t *word_start = last_word_start;
	while(word_start > buffer) {
		word_start -= sizeof(uint64_t);
		uint64_t word = _bijson_read_minimal_int(word_start, sizeof(uint64_t));
		if(word > UINT64_C(9999999999999999999))
			return bijson_error_file_format_error;
		_BIJSON_ERROR_RETURN(callback(callback_data, word_chars, sprintf(word_chars, "%019"PRIu64, word)));
	}

	return NULL;
}

static bijson_error_t _bijson_decimal_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const uint8_t *buffer = bijson->buffer;
	const uint8_t *buffer_end = buffer + bijson->size;

	uint8_t type = *buffer;

	const uint8_t *exponent_size_location = buffer + SIZE_C(1);
	if(exponent_size_location == buffer_end)
		return bijson_error_file_format_error;

	size_t exponent_size_size = SIZE_C(1) << (type & UINT8_C(0x3));
	const uint8_t *exponent_start = exponent_size_location + exponent_size_size;
	if(exponent_start + SIZE_C(1) > buffer_end)
		return bijson_error_file_format_error;
	uint64_t raw_exponent_size = _bijson_read_minimal_int(exponent_size_location, exponent_size_size);
	if(raw_exponent_size > SIZE_MAX - SIZE_C(1))
		return bijson_error_file_format_error;
	size_t exponent_size = (size_t)raw_exponent_size + SIZE_C(1);
	if(exponent_size > buffer_end - exponent_start - SIZE_C(1))
		return bijson_error_file_format_error;

	if(type & UINT8_C(0x4))
		_BIJSON_ERROR_RETURN(callback(callback_data, "-", SIZE_C(1)));

	const uint8_t *significand_start = exponent_start + exponent_size;
	bijson_t significand = {
		significand_start,
		buffer_end - significand_start,
	};
	_BIJSON_ERROR_RETURN(_bijson_decimal_part_to_json(&significand, callback, callback_data));

	_BIJSON_ERROR_RETURN(callback(callback_data, "e", SIZE_C(1)));

	if(type & UINT8_C(0x8))
		_BIJSON_ERROR_RETURN(callback(callback_data, "-", SIZE_C(1)));

	bijson_t exponent = {
		exponent_start,
		exponent_size,
	};
	return _bijson_decimal_part_to_json(&exponent, callback, callback_data);
}

static bijson_error_t _bijson_decimal_integer_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const uint8_t *buffer = bijson->buffer;
	size_t size = bijson->size;

	uint8_t type = *buffer;
	if(type & UINT8_C(0x1))
		_BIJSON_ERROR_RETURN(callback(callback_data, "-", SIZE_C(1)));

	if(size == SIZE_C(1))
		return callback(callback_data, "0", SIZE_C(1));

	bijson_t integer = {
		buffer + SIZE_C(1),
		size - SIZE_C(1),
	};
	return _bijson_decimal_part_to_json(&integer, callback, callback_data);
}

bijson_error_t bijson_object_count(const bijson_t *bijson, size_t *result) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	const uint8_t *buffer = bijson->buffer;
	size_t size = bijson->size;
	uint8_t type = *buffer++;
	size--;
	size_t count = SIZE_C(0);
	if(size) {
		size_t count_size = SIZE_C(1) << (type & UINT8_C(0x3));
		if(count_size > size)
			return bijson_error_file_format_error;
		uint64_t raw_count = _bijson_read_minimal_int(buffer, count_size);
		if(raw_count > SIZE_MAX - SIZE_C(1))
			return bijson_error_file_format_error;
		count = raw_count + 1;
		size -= count_size;

		size_t key_index_item_size = SIZE_C(1) << ((type >> 2) & UINT8_C(0x3));
		size_t value_index_item_size = SIZE_C(1) << ((type >> 4) & UINT8_C(0x3));
		// We need at least one key_index_item, one value_index_item, and one
		// type byte for each item, but the first item does not have a value
		// index entry, so fake that.
		if(count > (size + value_index_item_size)
			/ (key_index_item_size + value_index_item_size + SIZE_C(1))
		)
			return bijson_error_file_format_error;
	}

	if(result)
		*result = count;
	return NULL;
}

bijson_error_t bijson_object_get_index(
	const bijson_t *bijson,
	size_t index,
	const void **key_buffer_result,
	size_t *key_size_result,
	bijson_t *value_result
) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	const uint8_t *buffer = bijson->buffer;
	const uint8_t *buffer_end = buffer + bijson->size;

	uint8_t type = *buffer;

	const uint8_t *count_location = buffer + SIZE_C(1);
	if(count_location == buffer_end)
		return bijson_error_file_format_error;

	size_t count_size = SIZE_C(1) << (type & UINT8_C(0x3));
	const uint8_t *key_index = count_location + count_size;
	if(key_index > buffer_end)
		return bijson_error_file_format_error;
	uint64_t raw_count = _bijson_read_minimal_int(count_location, count_size);
	if(raw_count > SIZE_MAX - SIZE_C(1))
		return bijson_error_file_format_error;
	size_t count_1 = (size_t)raw_count;
	size_t count = count_1 + SIZE_C(1);

	size_t index_and_data_size = buffer_end - key_index;

	size_t key_index_item_size = SIZE_C(1) << ((type >> 2) & UINT8_C(0x3));
	size_t value_index_item_size = SIZE_C(1) << ((type >> 4) & UINT8_C(0x3));
	// We need at least one key_index_item, one value_index_item, and one
	// type byte for each item, but the first item does not have a value
	// index entry, so fake that.
	if(count > (index_and_data_size + value_index_item_size)
		/ (key_index_item_size + value_index_item_size + SIZE_C(1))
	)
		return bijson_error_file_format_error;

	if(index >= count)
		return bijson_error_index_out_of_range;

	const uint8_t *value_index = key_index + count * key_index_item_size;
	const uint8_t *key_data_start = value_index + count_1 * value_index_item_size;

	uint64_t raw_last_key_end_offset =
		_bijson_read_minimal_int(key_index + key_index_item_size * count_1, key_index_item_size);
	if(raw_last_key_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t last_key_end_offset = (size_t)raw_last_key_end_offset;

	if(last_key_end_offset > buffer_end - key_data_start - count)
		return bijson_error_file_format_error;

	uint64_t raw_key_start_offset = index
		? _bijson_read_minimal_int(key_index + key_index_item_size * (index - SIZE_C(1)), key_index_item_size)
		: UINT64_C(0);
	if(raw_key_start_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t key_start_offset = (size_t)raw_key_start_offset;
	if(key_start_offset > last_key_end_offset)
		return bijson_error_file_format_error;

	uint64_t raw_key_end_offset = index == count_1
		? raw_last_key_end_offset
		: _bijson_read_minimal_int(key_index + key_index_item_size * index, key_index_item_size);
	if(raw_key_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t key_end_offset = (size_t)raw_key_end_offset;
	if(key_end_offset > last_key_end_offset)
		return bijson_error_file_format_error;

	if(key_start_offset > key_end_offset)
		return bijson_error_file_format_error;

	const uint8_t *value_data_start = key_data_start + last_key_end_offset;
	size_t value_data_size = buffer_end - value_data_start;

	// This is for comparing the raw offsets to, so it doesn't include
	// the implicit "+ index" term yet.
	size_t highest_valid_value_offset = value_data_size - count;

	uint64_t raw_value_start_offset = index
		? _bijson_read_minimal_int(value_index + value_index_item_size * (index - SIZE_C(1)), value_index_item_size)
		: UINT64_C(0);
	if(raw_value_start_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t value_start_offset = (size_t)raw_value_start_offset;
	if(value_start_offset > highest_valid_value_offset)
		return bijson_error_file_format_error;
	value_start_offset += index;

	uint64_t raw_value_end_offset = index == count_1
		? highest_valid_value_offset
		: _bijson_read_minimal_int(value_index + value_index_item_size * index, value_index_item_size);
	if(raw_value_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t value_end_offset = (size_t)raw_value_end_offset;

	if(value_end_offset > highest_valid_value_offset)
		return bijson_error_file_format_error;

	value_end_offset += index + SIZE_C(1);
	if(index == count_1)
		value_end_offset = value_data_size;

	if(value_start_offset >= value_end_offset)
		return bijson_error_file_format_error;

	const uint8_t *key_buffer = key_data_start + key_start_offset;
	size_t key_size = key_end_offset - key_start_offset;

	// We could return an UTF-8 error but it's the file that's at fault here:
	bijson_error_t error = _bijson_check_valid_utf8((const char *)key_buffer, key_size);
	if(error == bijson_error_invalid_utf8)
		return bijson_error_file_format_error;
	else if(error)
		return error;

	if(key_buffer_result)
		*key_buffer_result = key_buffer;

	if(key_size_result)
		*key_size_result = key_size;

	if(value_result) {
		value_result->buffer = value_data_start + value_start_offset;
		value_result->size = value_end_offset - value_start_offset;
	}

	return NULL;
}

bijson_error_t bijson_array_count(const bijson_t *bijson, size_t *result) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	const uint8_t *buffer = bijson->buffer;
	size_t size = bijson->size;
	uint8_t type = *buffer++;
	size--;
	size_t count = SIZE_C(0);
	if(size) {
		size_t count_size = SIZE_C(1) << (type & UINT8_C(0x3));
		if(count_size > size)
			return bijson_error_file_format_error;
		uint64_t raw_count = _bijson_read_minimal_int(buffer, count_size);
		if(raw_count > SIZE_MAX - SIZE_C(1))
			return bijson_error_file_format_error;
		count = raw_count + 1;
		size -= count_size;

		size_t index_item_size = SIZE_C(1) << ((type >> 2) & UINT8_C(0x3));
		// We need at least one index_item and one type byte for each item,
		// but the first item does not have an index entry, so fake that.
		if(count > (size + index_item_size) / (index_item_size + SIZE_C(1)))
			return bijson_error_file_format_error;
	}

	if(result)
		*result = count;
	return NULL;
}

bijson_error_t bijson_array_get_index(const bijson_t *bijson, size_t index, bijson_t *result) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	const uint8_t *buffer = bijson->buffer;
	const uint8_t *buffer_end = buffer + bijson->size;

	uint8_t type = *buffer;

	const uint8_t *count_location = buffer + SIZE_C(1);
	if(count_location == buffer_end)
		return bijson_error_file_format_error;

	size_t count_size = SIZE_C(1) << (type & UINT8_C(0x3));
	const uint8_t *item_index = count_location + count_size;
	if(item_index > buffer_end)
		return bijson_error_file_format_error;
	uint64_t raw_count = _bijson_read_minimal_int(count_location, count_size);
	if(raw_count > SIZE_MAX - SIZE_C(1))
		return bijson_error_file_format_error;
	size_t count_1 = (size_t)raw_count;
	size_t count = count_1 + SIZE_C(1);

	size_t index_and_data_size = buffer_end - item_index;

	size_t index_item_size = SIZE_C(1) << ((type >> 2) & UINT8_C(0x3));
	// We need at least one index_item and one type byte for each item,
	// but the first item does not have an index entry, so fake that.
	if(count > (index_and_data_size + index_item_size) / (index_item_size + SIZE_C(1)))
		return bijson_error_file_format_error;

	if(index >= count)
		return bijson_error_index_out_of_range;

	const uint8_t *item_data_start = item_index + (count - SIZE_C(1)) * index_item_size;
	size_t item_data_size = buffer_end - item_data_start;

	// This is for comparing the raw offsets to, so it doesn't include
	// the implicit "+ index" term yet.
	size_t highest_valid_offset = item_data_size - count;

	uint64_t raw_start_offset = index
		? _bijson_read_minimal_int(item_index + index_item_size * (index - SIZE_C(1)), index_item_size)
		: UINT64_C(0);
	if(raw_start_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t start_offset = (size_t)raw_start_offset;
	if(start_offset > highest_valid_offset)
		return bijson_error_file_format_error;
	start_offset += index;

	uint64_t raw_end_offset = index == count_1
		? highest_valid_offset
		: _bijson_read_minimal_int(item_index + index_item_size * index, index_item_size);
	if(raw_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t end_offset = (size_t)raw_end_offset;
	if(end_offset > highest_valid_offset)
		return bijson_error_file_format_error;
	end_offset += index + SIZE_C(1);

	if(start_offset >= end_offset)
		return bijson_error_file_format_error;

	if(result) {
		result->buffer = item_data_start + start_offset;
		result->size = end_offset - start_offset;
	}

	return NULL;
}

static bijson_error_t _bijson_object_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	size_t count;
	_BIJSON_ERROR_RETURN(bijson_array_count(bijson, &count));
	_BIJSON_ERROR_RETURN(callback(callback_data, "{", 1));

	for(size_t u = 0; u < count; u++) {
		if(u)
			_BIJSON_ERROR_RETURN(callback(callback_data, ",", 1));

		bijson_t key, value;
		_BIJSON_ERROR_RETURN(bijson_object_get_index(bijson, u, &key.buffer, &key.size, &value));
		_BIJSON_ERROR_RETURN(_bijson_raw_string_to_json(&key, callback, callback_data));
		_BIJSON_ERROR_RETURN(callback(callback_data, ":", 1));
		_BIJSON_ERROR_RETURN(bijson_to_json(&value, callback, callback_data));
	}

	return callback(callback_data, "}", 1);
}

static bijson_error_t _bijson_array_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	size_t count;
	_BIJSON_ERROR_RETURN(bijson_array_count(bijson, &count));

	_BIJSON_ERROR_RETURN(callback(callback_data, "[", 1));

	for(size_t u = 0; u < count; u++) {
		if(u)
			_BIJSON_ERROR_RETURN(callback(callback_data, ",", 1));

		bijson_t item;
		_BIJSON_ERROR_RETURN(bijson_array_get_index(bijson, u, &item));
		_BIJSON_ERROR_RETURN(bijson_to_json(&item, callback, callback_data));
	}

	return callback(callback_data, "]", 1);
}

bijson_error_t bijson_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	const uint8_t *buffer = bijson->buffer;

	const uint8_t type = *buffer;

	switch(type & UINT8_C(0xF0)) {
		case UINT8_C(0x00):
			switch(type) {
				case UINT8_C(0x00):
					return bijson_error_file_format_error;
				case UINT8_C(0x01):
				case UINT8_C(0x05): // undefined
					return callback(callback_data, "null", 4);
				case UINT8_C(0x02):
					return callback(callback_data, "false", 5);
				case UINT8_C(0x03):
					return callback(callback_data, "true", 4);
				case UINT8_C(0x08):
					return _bijson_string_to_json(bijson, callback, callback_data);
			}
			break;
		case UINT8_C(0x10):
			switch(type & UINT8_C(0xFE)) {
				case UINT8_C(0x1A):
					return _bijson_decimal_integer_to_json(bijson, callback, callback_data);
			}
			break;
		case UINT8_C(0x20):
			return _bijson_decimal_to_json(bijson, callback, callback_data);
			break;
		case UINT8_C(0x30):
			return _bijson_array_to_json(bijson, callback, callback_data);
			break;
		case UINT8_C(0x40):
		case UINT8_C(0x50):
		case UINT8_C(0x60):
		case UINT8_C(0x70):
			return _bijson_object_to_json(bijson, callback, callback_data);
			break;
	}

	return bijson_error_unsupported_data_type;
}

typedef struct _bijson_to_json_state {
	const bijson_t *bijson;
} _bijson_to_json_state_t;

bijson_error_t _bijson_to_json_callback(
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
	return _bijson_io_write_to_fd(_bijson_to_json_callback, &state, fd);
}

bijson_error_t bijson_to_json_FILE(const bijson_t *bijson, FILE *file) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_FILE(_bijson_to_json_callback, &state, file);
}

bijson_error_t bijson_to_json_malloc(
	const bijson_t *bijson,
	void **result_buffer,
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

bijson_error_t bijson_to_json_bytecounter(
	const bijson_t *bijson,
	void **result_buffer,
	size_t *result_size
) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_bytecounter(
		_bijson_to_json_callback,
		&state,
		result_size
	);
}
