#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <xxhash.h>

#include <bijson/writer.h>

#include "common.h"
#include "container.h"
#include "format.h"
#include "buffer.h"
#include "writer.h"

typedef struct _bijson_container {
	size_t spool_size;
	size_t output_size;
	size_t count;
} _bijson_container_t;

static const _bijson_container_t _bijson_container_0 = {0};

static bijson_error_t _bijson_container_push(bijson_writer_t *writer) {
	_BIJSON_ERROR_RETURN(_bijson_buffer_append(&writer->stack, &writer->current_container, sizeof writer->current_container));
	writer->current_container = writer->spool.used;
	return NULL;
}

static void _bijson_container_pop(bijson_writer_t *writer) {
	_bijson_buffer_pop(&writer->stack, &writer->current_container, sizeof writer->current_container);
}

bijson_error_t bijson_writer_begin_object(bijson_writer_t *writer) {
	if(writer->failed)
		return bijson_error_writer_failed;
	_BIJSON_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &_bijson_spool_type_object, sizeof _bijson_spool_type_object));
	_BIJSON_ERROR_RETURN(_bijson_container_push(writer));
	_BIJSON_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &_bijson_container_0, sizeof _bijson_container_0));
	return NULL;
}

bijson_error_t bijson_writer_begin_array(bijson_writer_t *writer) {
	if(writer->failed)
		return bijson_error_writer_failed;
	_BIJSON_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &_bijson_spool_type_array, sizeof _bijson_spool_type_array));
	_BIJSON_ERROR_RETURN(_bijson_container_push(writer));
	_BIJSON_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &_bijson_container_0, sizeof _bijson_container_0));
	return NULL;
}

bijson_error_t bijson_writer_add_key(bijson_writer_t *writer, const char *key, size_t len) {
	if(writer->failed)
		return bijson_error_writer_failed;
	_BIJSON_ERROR_RETURN(_bijson_check_valid_utf8(key, len));
	_BIJSON_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &len, sizeof len));
	_BIJSON_ERROR_RETURN(_bijson_buffer_append(&writer->spool, key, len));
	return NULL;
}

bijson_error_t bijson_writer_end_object(bijson_writer_t *writer) {
	if(writer->failed)
		return bijson_error_writer_failed;

	size_t spool_used = writer->spool.used;
	size_t current_container = writer->current_container;
	_bijson_container_t container = _bijson_container_0;
	container.spool_size = spool_used - current_container - sizeof container.spool_size;

	size_t spool_offset = current_container + sizeof container;

	size_t keys_output_size = 0;
	size_t values_output_size = 0;
	size_t value_output_size_of_highest_key = 0;

	XXH128_hash_t highest_hash = {};

	size_t object_item_offset = spool_offset;
	while(object_item_offset < spool_used) {
		size_t key_spool_size;
		_bijson_buffer_read(&writer->spool, object_item_offset, &key_spool_size, sizeof key_spool_size);

		keys_output_size += key_spool_size;
		object_item_offset += sizeof key_spool_size;

		const void *key = _bijson_buffer_access(&writer->spool, object_item_offset, key_spool_size);
		XXH128_hash_t object_item_hash = XXH3_128bits(key, key_spool_size);

		object_item_offset += key_spool_size;
		values_output_size += _bijson_writer_size_value(writer, object_item_offset);

		object_item_offset += sizeof(_bijson_spool_type_t);
		size_t value_spool_size;
		_bijson_buffer_read(&writer->spool, object_item_offset, &value_spool_size, sizeof value_spool_size);
		object_item_offset += sizeof value_spool_size;
		object_item_offset += value_spool_size;

		if(!container.count || XXH128_cmp(&highest_hash, &object_item_hash) < 0) {
			value_output_size_of_highest_key = value_spool_size;
			highest_hash = object_item_hash;
		}

		container.count++;
	}

	size_t container_count_1 = container.count - SIZE_C(1);

	container.output_size = container.count
		? 1 + _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size1(container.count))
			+ container.count * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(keys_output_size))
			+ container_count_1 * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(
				values_output_size - value_output_size_of_highest_key - container_count_1
			))
			+ keys_output_size + values_output_size
		: 1;

	_bijson_buffer_write(&writer->spool, current_container, &container, sizeof container);

	_bijson_container_pop(writer);

	return NULL;
}

bijson_error_t bijson_writer_end_array(bijson_writer_t *writer) {
	if(writer->failed)
		return bijson_error_writer_failed;
	size_t spool_used = writer->spool.used;
	size_t current_container = writer->current_container;
	_bijson_container_t container = _bijson_container_0;
	container.spool_size = spool_used - current_container - sizeof container.spool_size;

	size_t spool_offset = current_container + sizeof container;

	size_t items_output_size = 0;
	size_t last_item_output_size = 0;

	size_t item_offset = spool_offset;
	while(item_offset < spool_used) {
		last_item_output_size = _bijson_writer_size_value(writer, item_offset);
		items_output_size += last_item_output_size;
		item_offset += sizeof(_bijson_spool_type_t);
		size_t item_spool_size;
		_bijson_buffer_read(&writer->spool, item_offset, &item_spool_size, sizeof item_spool_size);
		item_offset += sizeof item_spool_size;
		item_offset += item_spool_size;
		container.count++;
	}

	size_t container_count_1 = container.count - SIZE_C(1);

	container.output_size = container.count
		? 1 + _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size1(container.count))
			+ container_count_1 * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(
				items_output_size - last_item_output_size - container_count_1
			))
			+ items_output_size
		: 1;

	_bijson_buffer_write(&writer->spool, current_container, &container, sizeof container);

	_bijson_container_pop(writer);

	return NULL;
}

size_t _bijson_writer_size_container(bijson_writer_t *writer, size_t spool_offset) {
	size_t output_size;
	_bijson_buffer_read(&writer->spool, spool_offset + sizeof _bijson_container_0.spool_size, &output_size, sizeof output_size);
	return output_size;
}

static int _bijson_writer_object_object_item_cmp(const void *a, const void *b) {
	const char *a_item = *(const char **)a;
	const char *b_item = *(const char **)b;
	size_t a_size, b_size;
	memcpy(&a_size, a_item, sizeof a_size);
	memcpy(&b_size, b_item, sizeof b_size);
	a_item += sizeof a_size;
	b_item += sizeof b_size;
	XXH128_hash_t a_hash = XXH3_128bits(a_item, a_size);
	XXH128_hash_t b_hash = XXH3_128bits(b_item, b_size);
	int c = XXH128_cmp(&a_hash, &b_hash);
	if(c)
		return c;
	c = a_size < b_size ? -1 : a_size != b_size;
	if(c)
		return c;
	return memcmp(a_item, b_item, a_size);
}

bijson_error_t _bijson_writer_write_object(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool) {
	_bijson_container_t container;
	memcpy(&container, spool, sizeof container);
	spool += sizeof container;

	if(container.count == 0)
		return write(write_data, "\x40", 1);

	size_t container_count_1 = container.count - 1;

	const char **object_items;
	size_t object_items_size = container.count * sizeof *object_items;
	_BIJSON_ERROR_RETURN(_bijson_buffer_push(&writer->stack, NULL, object_items_size, &object_items));

	size_t keys_output_size = 0;

	// Build the array for sorting by going through the memory buffer and
	// compute the largest value offset that we'll actually store
	size_t values_output_size = 0;
	const char *object_item = spool;
	for(size_t z = 0; z < container.count; z++) {
		object_items[z] = object_item;
		size_t key_spool_size;
		memcpy(&key_spool_size, object_item, sizeof key_spool_size);
		keys_output_size += key_spool_size;
		object_item += sizeof key_spool_size;
		object_item += key_spool_size;
		values_output_size += _bijson_writer_size_value(writer, _bijson_buffer_offset(&writer->spool, object_item));
		object_item += sizeof(_bijson_spool_type_t);
		size_t value_spool_size;
		memcpy(&value_spool_size, object_item, sizeof value_spool_size);
		object_item += sizeof value_spool_size;
		object_item += value_spool_size;
	}
	qsort(object_items, container.count, sizeof *object_items, _bijson_writer_object_object_item_cmp);

	// Now that we know the order, subtract the size of the last item from
	// values_output_size, since we won't actually store that (it's computed
	// from the bounding size).
	object_item = object_items[container_count_1];
	size_t key_spool_size;
	memcpy(&key_spool_size, object_item, sizeof key_spool_size);
	object_item += sizeof key_spool_size;
	object_item += key_spool_size;
	values_output_size -= _bijson_writer_size_value(writer, _bijson_buffer_offset(&writer->spool, object_item));

	// We do not include the type bytes in the offsets (they're implicit)
	values_output_size -= container_count_1;

	uint8_t count_width = _bijson_optimal_storage_size(container_count_1);
	uint8_t key_offsets_width = _bijson_optimal_storage_size(keys_output_size);
	uint8_t value_offsets_width = _bijson_optimal_storage_size(values_output_size);
	uint8_t final_type = UINT8_C(0x40) | count_width | (key_offsets_width << 2) | (value_offsets_width << 4);

	_BIJSON_ERROR_RETURN(write(write_data, &final_type, sizeof final_type));
	_BIJSON_ERROR_RETURN(_bijson_writer_write_compact_int(write, write_data, container_count_1, count_width));

	// Write the key offsets
	size_t key_offset = 0;
	for(size_t z = 0; z < container.count; z++) {
		object_item = object_items[z];
		size_t key_spool_size;
		memcpy(&key_spool_size, object_item, sizeof key_spool_size);
		key_offset += key_spool_size;
		_BIJSON_ERROR_RETURN(_bijson_writer_write_compact_int(write, write_data, key_offset, key_offsets_width));
	}

	// Write the value offsets
	size_t value_output_offset = 0;
	for(size_t z = 0; z < container_count_1; z++) {
		object_item = object_items[z];
		size_t key_spool_size;
		memcpy(&key_spool_size, object_item, sizeof key_spool_size);
		object_item += sizeof key_spool_size;
		object_item += key_spool_size;
		value_output_offset += _bijson_writer_size_value(writer, _bijson_buffer_offset(&writer->spool, object_item)) - SIZE_C(1);
		_BIJSON_ERROR_RETURN(_bijson_writer_write_compact_int(write, write_data, value_output_offset, value_offsets_width));
	}

	// Write the keys
	for(size_t z = 0; z < container.count; z++) {
		object_item = object_items[z];
		size_t key_spool_size;
		memcpy(&key_spool_size, object_item, sizeof key_spool_size);
		object_item += sizeof key_spool_size;
		_BIJSON_ERROR_RETURN(write(write_data, object_item, key_spool_size));
	}

	// Write the values
	for(size_t z = 0; z < container.count; z++) {
		object_item = object_items[z];
		size_t key_size;
		memcpy(&key_size, object_item, sizeof key_size);
		object_item += sizeof key_size;
		object_item += key_size;
		_BIJSON_ERROR_RETURN(_bijson_writer_write_value(writer, write, write_data, object_item));
	}

	_bijson_buffer_pop(&writer->stack, NULL, object_items_size);
	return NULL;
}

bijson_error_t _bijson_writer_write_array(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool) {
	_bijson_container_t container;
	memcpy(&container, spool, sizeof container);
	spool += sizeof container;

	if(container.count == 0)
		return write(write_data, "\x30", 1);
	size_t container_count_1 = container.count - SIZE_C(1);

	size_t items_output_size = 0;

	const char *item = spool;
	for(size_t z = 0; z < container_count_1; z++) {
		items_output_size += _bijson_writer_size_value(writer, _bijson_buffer_offset(&writer->spool, item));
		item += sizeof(_bijson_spool_type_t);
		size_t item_spool_size;
		memcpy(&item_spool_size, item, sizeof item_spool_size);
		item += sizeof item_spool_size;
		item += item_spool_size;
	}
	items_output_size -= container_count_1;

	uint8_t count_width = _bijson_optimal_storage_size(container_count_1);
	uint8_t item_offsets_width = _bijson_optimal_storage_size(items_output_size);
	uint8_t final_type = UINT8_C(0x30) | count_width | (item_offsets_width << 4);

	_BIJSON_ERROR_RETURN(write(write_data, &final_type, sizeof final_type));
	_BIJSON_ERROR_RETURN(_bijson_writer_write_compact_int(write, write_data, container_count_1, count_width));

	// Write the item offsets
	item = spool;
	size_t item_output_offset = 0;
	for(size_t z = 0; z < container_count_1; z++) {
		item_output_offset += _bijson_writer_size_value(writer, _bijson_buffer_offset(&writer->spool, item)) - SIZE_C(1);
		item += sizeof(_bijson_spool_type_t);
		size_t item_spool_size;
		memcpy(&item_spool_size, item, sizeof item_spool_size);
		item += sizeof item_spool_size;
		item += item_spool_size;
		_BIJSON_ERROR_RETURN(_bijson_writer_write_compact_int(write, write_data, item_output_offset, item_offsets_width));
	}

	// Write the element values
	item = spool;
	for(size_t z = 0; z < container.count; z++) {
		_BIJSON_ERROR_RETURN(_bijson_writer_write_value(writer, write, write_data, item));
		item += sizeof(_bijson_spool_type_t);
		size_t item_spool_size;
		memcpy(&item_spool_size, item, sizeof item_spool_size);
		item += sizeof item_spool_size;
		item += item_spool_size;
	}

	return NULL;
}
