#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <bijson/reader.h>

#include "reader.h"
#include "common.h"


static inline uint64_t _bijson_read_minimal_int(const uint8_t *buffer, size_t nbytes) {
	uint64_t r = 0;
	for(size_t u = 0; u < nbytes; u++)
		r |= buffer[u] << (u * SIZE_C(8));
	return r;
}

static const unsigned char hex[16] = "0123456789ABCDEF";

static bool _bijson_raw_string_to_json(const bijson_t *bijson, bijson_output_callback callback, void *userdata) {
	const uint8_t *string = bijson->buffer;
	size_t len = bijson->size;
	const uint8_t *previously_written = string;
	uint8_t escape[2] = "\\x";
	uint8_t unicode_escape[6] = "\\u00XX";
	if(!callback("\"", 1, userdata))
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
			if(!callback(previously_written, string_pos - previously_written, userdata))
				return false;
		previously_written = string;
		if(plain_escape) {
			escape[1] = plain_escape;
			if(!callback(escape, sizeof escape, userdata))
				return false;
		} else {
			unicode_escape[4] = hex[c >> 4];
			unicode_escape[5] = hex[c & UINT8_C(0xF)];
			if(!callback(unicode_escape, sizeof unicode_escape, userdata))
				return false;
		}
	}
	if(string > previously_written)
		if(!callback(previously_written, string - previously_written, userdata))
			return false;
	if(!callback("\"", 1, userdata))
		return false;
	return true;
}

static bool _bijson_string_to_json(const bijson_t *bijson, bijson_output_callback callback, void *userdata) {
	bijson_t raw_string = { .buffer = bijson->buffer + SIZE_C(1), .size = bijson->size - SIZE_C(1) };
	if(!_bijson_is_valid_utf8((const char *)raw_string.buffer, raw_string.size))
		return false;
	return _bijson_raw_string_to_json(&raw_string, callback, userdata);
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

	fprintf(stderr, "count = %zu\n", count);
	fprintf(stderr, "count_1 = %zu\n", count_1);
	fprintf(stderr, "count_location = %zu\n", count_location - buffer);
	fprintf(stderr, "key_index = %zu\n", key_index - buffer);
	fprintf(stderr, "value_index = %zu\n", value_index - buffer);
	fprintf(stderr, "key_data_start = %zu\n", key_data_start - buffer);

	uint64_t raw_last_key_end_offset =
		_bijson_read_minimal_int(key_index + key_index_item_size * count_1, key_index_item_size);
	if(raw_last_key_end_offset > SIZE_MAX)
		return false;

	size_t last_key_end_offset = (size_t)raw_last_key_end_offset;

	fprintf(stderr, "yo!\n");

	fprintf(stderr, "last_key_end_offset = %zu\nbuffer_end - key_data_start = %zu\ncount = %zu\n",
		last_key_end_offset, (size_t)(buffer_end - key_data_start), count);

	if(last_key_end_offset > buffer_end - key_data_start - count)
		return false;

	fprintf(stderr, "yo?\n");

	const uint8_t *value_data_start = key_data_start + last_key_end_offset;
	fprintf(stderr, "value_data_start = %zu\n", value_data_start - buffer);
	size_t value_data_size = buffer_end - value_data_start;

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


	fprintf(stderr, "buffer_length = %zu\n", bijson->size);


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
	fprintf(stderr, "sup? value_end_offset=%zu highest_valid_value_offset=%zu\n",
		value_end_offset, highest_valid_value_offset);

	if(value_end_offset > highest_valid_value_offset)
		return false;
	fprintf(stderr, "sup!\n");

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

	fprintf(stderr, "value offset = %zu\n", value_data_start + value_start_offset - buffer);

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

	uint8_t type = *buffer++;
	if(buffer == buffer_end)
		return false;

	size_t count_size = SIZE_C(1) << (type & UINT8_C(0x3));
	const uint8_t *count_end = buffer + count_size;
	if(count_end > buffer_end)
		return false;
	uint64_t raw_count = _bijson_read_minimal_int(buffer, count_size);
	if(raw_count > SIZE_MAX - SIZE_C(1))
		return false;
	size_t count = (size_t)raw_count + SIZE_C(1);

	buffer = count_end;
	size_t index_and_data_size = buffer_end - buffer;

	size_t index_item_size = SIZE_C(1) << ((type >> 2) & UINT8_C(0x3));
	fprintf(stderr, "index_item_size: %zu\n", index_item_size);
	// We need at least one index_item and one type byte for each item,
	// but the first item does not have an index entry, so fake that.
	if(count > (index_and_data_size + index_item_size) / (index_item_size + SIZE_C(1)))
		return false;

	if(index >= count)
		return false;

	const uint8_t *item_index = buffer;
	const uint8_t *item_data_start = buffer + (count - SIZE_C(1)) * index_item_size;
	size_t data_size = buffer_end - buffer;

	// This is for comparing the raw offsets to, so it doesn't include
	// the implicit "+ index" term yet.
	size_t highest_valid_offset = data_size - count;

	uint64_t raw_start_offset = index
		? _bijson_read_minimal_int(item_index + index_item_size * (index - SIZE_C(1)), index_item_size)
		: UINT64_C(0);
	if(raw_start_offset > SIZE_MAX)
		return false;
	size_t start_offset = (size_t)raw_start_offset;
	if(start_offset > highest_valid_offset)
		return false;
	start_offset += index;

	fprintf(stderr, "buffer at %zu\n", item_index - (const uint8_t *)bijson->buffer);
	fprintf(stderr, "reading from %zu\n", (item_index + index_item_size * index) - (const uint8_t *)bijson->buffer);

	uint64_t raw_end_offset = index == count - SIZE_C(1)
		? data_size
		: _bijson_read_minimal_int(item_index + index_item_size * index, index_item_size);
	if(raw_end_offset > SIZE_MAX)
		return false;
	size_t end_offset = (size_t)raw_end_offset;
	fprintf(stderr, "start_offset: %zu\n", start_offset);
	fprintf(stderr, "end_offset: %zu\n", end_offset);
	fprintf(stderr, "highest_valid_offset: %zu\n", highest_valid_offset);
	fprintf(stderr, "index: %zu\n", index);
	fprintf(stderr, "data_size: %zu\n", data_size);
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

static bool _bijson_object_to_json(const bijson_t *bijson, bijson_output_callback callback, void *userdata) {
	size_t count;
	if(!bijson_array_count(bijson, &count))
		return false;
	if(!callback("{", 1, userdata))
		return false;

	for(size_t u = 0; u < count; u++) {
		if(u && !callback(",", 1, userdata))
			return false;

		bijson_t key, value;
		if(!bijson_object_get_index(bijson, u, &key, &value))
			return false;
		if(!_bijson_raw_string_to_json(&key, callback, userdata))
			return false;
		if(!callback(":", 1, userdata))
			return false;
		if(!bijson_to_json(&value, callback, userdata))
			return false;
	}

	return callback("}", 1, userdata);
}

static bool _bijson_array_to_json(const bijson_t *bijson, bijson_output_callback callback, void *userdata) {
	size_t count;
	if(!bijson_array_count(bijson, &count))
		return false;

	if(!callback("[", 1, userdata))
		return false;

	for(size_t u = 0; u < count; u++) {
		if(u && !callback(",", 1, userdata))
			return false;

		bijson_t item;
		if(!bijson_array_get_index(bijson, u, &item))
			return false;
		if(!bijson_to_json(&item, callback, userdata))
			return false;
	}

	return callback("]", 1, userdata);
}

bool bijson_to_json(const bijson_t *bijson, bijson_output_callback callback, void *userdata) {
	if(!bijson)
		return false;
	const uint8_t *buffer = bijson->buffer;
	if(!buffer || !bijson->size)
		return false;

	const uint8_t type = *buffer;
	// fprintf(stderr, "%02X\n", type);

	if((type & UINT8_C(0xC0)) == UINT8_C(0x40))
		return _bijson_object_to_json(bijson, callback, userdata);
	if((type & UINT8_C(0xF0)) == UINT8_C(0x30))
		return _bijson_array_to_json(bijson, callback, userdata);

	switch(type) {
		case UINT8_C(0x01):
		case UINT8_C(0x05): // undefined
			return callback("null", 4, userdata);
		case UINT8_C(0x02):
			return callback("false", 5, userdata);
		case UINT8_C(0x03):
			return callback("true", 4, userdata);
		case UINT8_C(0x08):
			return _bijson_string_to_json(bijson, callback, userdata);
		default:
			return false;
	}

	return false;
}

bool bijson_stdio_output_callback(const void *data, size_t len, void *userdata) {
	return fwrite(data, sizeof *data, len, userdata) == len * sizeof *data;
}
