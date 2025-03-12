#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bijson/reader.h>

#include "common.h"
#include "io.h"

static inline uint64_t _bijson_read_minimal_int(const uint8_t *buffer, size_t nbytes) {
	uint64_t r = 0;
	for(size_t u = 0; u < nbytes; u++)
		r |= buffer[u] << (u * SIZE_C(8));
	return r;
}

static const unsigned char hex[16] = "0123456789ABCDEF";

static bool _bijson_raw_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const uint8_t *string = bijson->buffer;
	size_t len = bijson->size;
	const uint8_t *previously_written = string;
	uint8_t escape[2] = "\\x";
	uint8_t unicode_escape[6] = "\\u00XX";
	if(!callback(callback_data, "\"", 1))
		return false;
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
			if(!callback(callback_data, previously_written, string_pos - previously_written))
				return false;
		previously_written = string;
		if(plain_escape) {
			escape[1] = plain_escape;
			if(!callback(callback_data, escape, sizeof escape))
				return false;
		} else {
			unicode_escape[4] = hex[c >> 4];
			unicode_escape[5] = hex[c & UINT8_C(0xF)];
			if(!callback(callback_data, unicode_escape, sizeof unicode_escape))
				return false;
		}
	}
	if(string > previously_written)
		if(!callback(callback_data, previously_written, string - previously_written))
			return false;
	if(!callback(callback_data, "\"", 1))
		return false;
	return true;
}

static bool _bijson_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *userdata) {
	bijson_t raw_string = { .buffer = bijson->buffer + SIZE_C(1), .size = bijson->size - SIZE_C(1) };
	if(!_bijson_is_valid_utf8((const char *)raw_string.buffer, raw_string.size))
		return false;
	return _bijson_raw_string_to_json(&raw_string, callback, userdata);
}

static inline bool _bijson_decimal_part_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const uint8_t *buffer = bijson->buffer;
	size_t size = bijson->size;

	size_t last_word_size = ((size - SIZE_C(1)) & SIZE_C(0x7)) + SIZE_C(1);

	const uint8_t *last_word_start = buffer + size - last_word_size;
	uint64_t last_word = _bijson_read_minimal_int(last_word_start, last_word_size);
	if(last_word > UINT64_C(9999999999999999998))
		return false;
	last_word++;

	char word_chars[20];
	if(!callback(callback_data, word_chars, sprintf(word_chars, "%"PRIu64, last_word)))
		return false;

	const uint8_t *word_start = last_word_start;
	while(word_start > buffer) {
		word_start -= sizeof(uint64_t);
		uint64_t word = _bijson_read_minimal_int(word_start, sizeof(uint64_t));
		if(word > UINT64_C(9999999999999999999))
			return false;
		if(!callback(callback_data, word_chars, sprintf(word_chars, "%019"PRIu64, word)))
			return false;
	}

	return true;
}

static bool _bijson_decimal_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const uint8_t *buffer = bijson->buffer;
	const uint8_t *buffer_end = buffer + bijson->size;
	if(!buffer || buffer == buffer_end)
		return false;

	uint8_t type = *buffer;

	const uint8_t *exponent_size_location = buffer + SIZE_C(1);
	if(exponent_size_location == buffer_end)
		return false;

	size_t exponent_size_size = SIZE_C(1) << (type & UINT8_C(0x3));
	const uint8_t *exponent_start = exponent_size_location + exponent_size_size;
	if(exponent_start + SIZE_C(1) > buffer_end)
		return false;
	uint64_t raw_exponent_size = _bijson_read_minimal_int(exponent_size_location, exponent_size_size);
	if(raw_exponent_size > SIZE_MAX - SIZE_C(1))
		return false;
	size_t exponent_size = (size_t)raw_exponent_size + SIZE_C(1);
	if(exponent_size > buffer_end - exponent_start - SIZE_C(1))
		return false;

	if(type & UINT8_C(0x4) && !callback(callback_data, "-", SIZE_C(1)))
		return false;

	const uint8_t *significand_start = exponent_start + exponent_size;
	bijson_t significand = {
		significand_start,
		buffer_end - significand_start,
	};
	if(!_bijson_decimal_part_to_json(&significand, callback, callback_data))
		return false;

	if(!callback(callback_data, "e", SIZE_C(1)))
		return false;

	if(type & UINT8_C(0x8) && !callback(callback_data, "-", SIZE_C(1)))
		return false;

	bijson_t exponent = {
		exponent_start,
		exponent_size,
	};
	if(!_bijson_decimal_part_to_json(&exponent, callback, callback_data))
		return false;

	return true;
}

static bool _bijson_decimal_integer_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	const uint8_t *buffer = bijson->buffer;
	size_t size = bijson->size;
	if(!buffer || !size)
		return false;

	uint8_t type = *buffer;
	if(type & UINT8_C(0x1) && !callback(callback_data, "-", SIZE_C(1)))
		return false;

	if(size == SIZE_C(1))
		return callback(callback_data, "0", SIZE_C(1));

	bijson_t integer = {
		buffer + SIZE_C(1),
		size - SIZE_C(1),
	};
	return _bijson_decimal_part_to_json(&integer, callback, callback_data);
}

bool bijson_object_count(const bijson_t *bijson, size_t *result) {
	if(!bijson)
		return false;
	const uint8_t *buffer = bijson->buffer;
	size_t size = bijson->size;
	if(!buffer || !size)
		return false;
	uint8_t type = *buffer++;
	size--;
	size_t count = SIZE_C(0);
	if(size) {
		size_t count_size = SIZE_C(1) << (type & UINT8_C(0x3));
		if(count_size > size)
			return false;
		uint64_t raw_count = _bijson_read_minimal_int(buffer, count_size);
		if(raw_count > SIZE_MAX - SIZE_C(1))
			return false;
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
			return false;
	}

	if(result)
		*result = count;
	return true;
}

bool bijson_object_get_index(const bijson_t *bijson, size_t index, bijson_t *key_result, bijson_t *value_result) {
	if(!bijson)
		return false;
	const uint8_t *buffer = bijson->buffer;
	const uint8_t *buffer_end = buffer + bijson->size;
	if(!buffer || buffer == buffer_end)
		return false;

	uint8_t type = *buffer;

	const uint8_t *count_location = buffer + SIZE_C(1);
	if(count_location == buffer_end)
		return false;

	size_t count_size = SIZE_C(1) << (type & UINT8_C(0x3));
	const uint8_t *key_index = count_location + count_size;
	if(key_index > buffer_end)
		return false;
	uint64_t raw_count = _bijson_read_minimal_int(count_location, count_size);
	if(raw_count > SIZE_MAX - SIZE_C(1))
		return false;
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
		return false;

	if(index >= count)
		return false;

	const uint8_t *value_index = key_index + count * key_index_item_size;
	const uint8_t *key_data_start = value_index + count_1 * value_index_item_size;

	uint64_t raw_last_key_end_offset =
		_bijson_read_minimal_int(key_index + key_index_item_size * count_1, key_index_item_size);
	if(raw_last_key_end_offset > SIZE_MAX)
		return false;
	size_t last_key_end_offset = (size_t)raw_last_key_end_offset;

	if(last_key_end_offset > buffer_end - key_data_start - count)
		return false;

	uint64_t raw_key_start_offset = index
		? _bijson_read_minimal_int(key_index + key_index_item_size * (index - SIZE_C(1)), key_index_item_size)
		: UINT64_C(0);
	if(raw_key_start_offset > SIZE_MAX)
		return false;
	size_t key_start_offset = (size_t)raw_key_start_offset;
	if(key_start_offset > last_key_end_offset)
		return false;

	uint64_t raw_key_end_offset = index == count_1
		? raw_last_key_end_offset
		: _bijson_read_minimal_int(key_index + key_index_item_size * index, key_index_item_size);
	if(raw_key_end_offset > SIZE_MAX)
		return false;
	size_t key_end_offset = (size_t)raw_key_end_offset;
	if(key_end_offset > last_key_end_offset)
		return false;

	if(key_start_offset > key_end_offset)
		return false;

	const uint8_t *value_data_start = key_data_start + last_key_end_offset;
	size_t value_data_size = buffer_end - value_data_start;

	// This is for comparing the raw offsets to, so it doesn't include
	// the implicit "+ index" term yet.
	size_t highest_valid_value_offset = value_data_size - count;

	uint64_t raw_value_start_offset = index
		? _bijson_read_minimal_int(value_index + value_index_item_size * (index - SIZE_C(1)), value_index_item_size)
		: UINT64_C(0);
	if(raw_value_start_offset > SIZE_MAX)
		return false;
	size_t value_start_offset = (size_t)raw_value_start_offset;
	if(value_start_offset > highest_valid_value_offset)
		return false;
	value_start_offset += index;

	uint64_t raw_value_end_offset = index == count_1
		? highest_valid_value_offset
		: _bijson_read_minimal_int(value_index + value_index_item_size * index, value_index_item_size);
	if(raw_value_end_offset > SIZE_MAX)
		return false;
	size_t value_end_offset = (size_t)raw_value_end_offset;

	if(value_end_offset > highest_valid_value_offset)
		return false;

	value_end_offset += index + SIZE_C(1);
	if(index == count_1)
		value_end_offset = value_data_size;

	if(value_start_offset >= value_end_offset)
		return false;

	const uint8_t *key_buffer = key_data_start + key_start_offset;
	size_t key_size = key_end_offset - key_start_offset;

	if(!_bijson_is_valid_utf8((const char *)key_buffer, key_size))
		return false;

	if(key_result) {
		key_result->buffer = key_buffer;
		key_result->size = key_size;
	}

	if(value_result) {
		value_result->buffer = value_data_start + value_start_offset;
		value_result->size = value_end_offset - value_start_offset;
	}

	return true;
}

bool bijson_array_count(const bijson_t *bijson, size_t *result) {
	if(!bijson)
		return false;
	const uint8_t *buffer = bijson->buffer;
	size_t size = bijson->size;
	if(!buffer || !size)
		return false;
	uint8_t type = *buffer++;
	size--;
	size_t count = SIZE_C(0);
	if(size) {
		size_t count_size = SIZE_C(1) << (type & UINT8_C(0x3));
		if(count_size > size)
			return false;
		uint64_t raw_count = _bijson_read_minimal_int(buffer, count_size);
		if(raw_count > SIZE_MAX - SIZE_C(1))
			return false;
		count = raw_count + 1;
		size -= count_size;

		size_t index_item_size = SIZE_C(1) << ((type >> 2) & UINT8_C(0x3));
		// We need at least one index_item and one type byte for each item,
		// but the first item does not have an index entry, so fake that.
		if(count > (size + index_item_size) / (index_item_size + SIZE_C(1)))
			return false;
	}

	if(result)
		*result = count;
	return true;
}

bool bijson_array_get_index(const bijson_t *bijson, size_t index, bijson_t *result) {
	if(!bijson)
		return false;
	const uint8_t *buffer = bijson->buffer;
	const uint8_t *buffer_end = buffer + bijson->size;
	if(!buffer || buffer == buffer_end)
		return false;

	uint8_t type = *buffer;

	const uint8_t *count_location = buffer + SIZE_C(1);
	if(count_location == buffer_end)
		return false;

	size_t count_size = SIZE_C(1) << (type & UINT8_C(0x3));
	const uint8_t *item_index = count_location + count_size;
	if(item_index > buffer_end)
		return false;
	uint64_t raw_count = _bijson_read_minimal_int(count_location, count_size);
	if(raw_count > SIZE_MAX - SIZE_C(1))
		return false;
	size_t count_1 = (size_t)raw_count;
	size_t count = count_1 + SIZE_C(1);

	size_t index_and_data_size = buffer_end - item_index;

	size_t index_item_size = SIZE_C(1) << ((type >> 2) & UINT8_C(0x3));
	// We need at least one index_item and one type byte for each item,
	// but the first item does not have an index entry, so fake that.
	if(count > (index_and_data_size + index_item_size) / (index_item_size + SIZE_C(1)))
		return false;

	if(index >= count)
		return false;

	const uint8_t *item_data_start = item_index + (count - SIZE_C(1)) * index_item_size;
	size_t item_data_size = buffer_end - item_data_start;

	// This is for comparing the raw offsets to, so it doesn't include
	// the implicit "+ index" term yet.
	size_t highest_valid_offset = item_data_size - count;

	uint64_t raw_start_offset = index
		? _bijson_read_minimal_int(item_index + index_item_size * (index - SIZE_C(1)), index_item_size)
		: UINT64_C(0);
	if(raw_start_offset > SIZE_MAX)
		return false;
	size_t start_offset = (size_t)raw_start_offset;
	if(start_offset > highest_valid_offset)
		return false;
	start_offset += index;

	uint64_t raw_end_offset = index == count_1
		? highest_valid_offset
		: _bijson_read_minimal_int(item_index + index_item_size * index, index_item_size);
	if(raw_end_offset > SIZE_MAX)
		return false;
	size_t end_offset = (size_t)raw_end_offset;
	if(end_offset > highest_valid_offset)
		return false;
	end_offset += index + SIZE_C(1);

	if(start_offset >= end_offset)
		return false;

	if(result) {
		result->buffer = item_data_start + start_offset;
		result->size = end_offset - start_offset;
	}

	return true;
}

static bool _bijson_object_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	size_t count;
	if(!bijson_array_count(bijson, &count))
		return false;
	if(!callback(callback_data, "{", 1))
		return false;

	for(size_t u = 0; u < count; u++) {
		if(u && !callback(callback_data, ",", 1))
			return false;

		bijson_t key, value;
		if(!bijson_object_get_index(bijson, u, &key, &value))
			return false;
		if(!_bijson_raw_string_to_json(&key, callback, callback_data))
			return false;
		if(!callback(callback_data, ":", 1))
			return false;
		if(!bijson_to_json(&value, callback, callback_data))
			return false;
	}

	return callback(callback_data, "}", 1);
}

static bool _bijson_array_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	size_t count;
	if(!bijson_array_count(bijson, &count))
		return false;

	if(!callback(callback_data, "[", 1))
		return false;

	for(size_t u = 0; u < count; u++) {
		if(u && !callback(callback_data, ",", 1))
			return false;

		bijson_t item;
		if(!bijson_array_get_index(bijson, u, &item))
			return false;
		if(!bijson_to_json(&item, callback, callback_data))
			return false;
	}

	return callback(callback_data, "]", 1);
}

bool bijson_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	if(!bijson)
		return false;
	const uint8_t *buffer = bijson->buffer;
	if(!buffer || !bijson->size)
		return false;

	const uint8_t type = *buffer;

	switch(type & UINT8_C(0xF0)) {
		case UINT8_C(0x00):
			switch(type) {
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

	return false;
}

typedef struct _bijson_to_json_state {
	const bijson_t *bijson;
} _bijson_to_json_state_t;

bool _bijson_to_json_callback(
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

bool bijson_to_json_fd(const bijson_t *bijson, int fd) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_fd(_bijson_to_json_callback, &state, fd);
}

bool bijson_to_json_FILE(const bijson_t *bijson, FILE *file) {
	_bijson_to_json_state_t state = {bijson};
	return _bijson_io_write_to_FILE(_bijson_to_json_callback, &state, file);
}

bool bijson_to_json_malloc(
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

bool bijson_to_json_bytecounter(
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
