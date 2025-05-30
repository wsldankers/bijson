#include <xxhash.h>

#include "container.h"
#include "object.h"

bijson_error_t bijson_writer_begin_object(bijson_writer_t *writer) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	_BIJSON_RETURN_ON_ERROR(_bijson_writer_check_expect_value(writer));
	writer->expect = writer->expect_after_value = _bijson_writer_expect_key;
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, _bijson_spool_type_object));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->stack, writer->current_container));
	writer->current_container = writer->spool.used;
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push(&writer->spool, &_bijson_container_0, sizeof _bijson_container_0));
	return NULL;
}

bijson_error_t bijson_writer_add_key(bijson_writer_t *writer, const void *key, size_t len) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	if(writer->expect != _bijson_writer_expect_key)
		return writer->expect_after_value == _bijson_writer_expect_key
			? bijson_error_value_expected
			: bijson_error_unmatched_end;

	_BIJSON_RETURN_ON_ERROR(_bijson_check_valid_utf8((const byte_t *)key, len));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->spool, len));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push(&writer->spool, key, len));

	writer->expect = _bijson_writer_expect_value;
	return NULL;
}

bijson_error_t bijson_writer_begin_key(bijson_writer_t *writer) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	if(writer->expect != _bijson_writer_expect_key)
		return writer->expect_after_value == _bijson_writer_expect_key
			? bijson_error_value_expected
			: bijson_error_unmatched_end;

	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->stack, writer->spool.used));

	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->spool, SIZE_C(0)));

	writer->expect = _bijson_writer_expect_more_key;
	return NULL;
}

bijson_error_t bijson_writer_append_key(bijson_writer_t *writer, const void *key, size_t len) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	if(writer->expect != _bijson_writer_expect_more_key)
		_BIJSON_RETURN_ERROR(bijson_error_unmatched_end);

	return _bijson_buffer_push(&writer->spool, key, len);
}

bijson_error_t bijson_writer_end_key(bijson_writer_t *writer) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	if(writer->expect != _bijson_writer_expect_more_key)
		_BIJSON_RETURN_ERROR(bijson_error_unmatched_end);

	size_t spool_used = _bijson_buffer_pop_size(&writer->stack);

	size_t total_len = writer->spool.used - spool_used - sizeof total_len;
	_bijson_buffer_write_size(&writer->spool, spool_used, total_len);

	_BIJSON_WRITER_ERROR_RETURN(_bijson_check_valid_utf8(
		_bijson_buffer_access(&writer->spool, spool_used + sizeof total_len, total_len),
		total_len
	));

	writer->expect = _bijson_writer_expect_value;
	return NULL;
}

bijson_error_t bijson_writer_end_object(bijson_writer_t *writer) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	if(writer->expect != _bijson_writer_expect_key)
		_BIJSON_RETURN_ERROR(bijson_error_unmatched_end);

	size_t spool_used = writer->spool.used;
	size_t current_container = writer->current_container;
	_bijson_container_t container = _bijson_container_0;
	container.spool_size = spool_used - current_container - sizeof container.spool_size;

	size_t spool_offset = current_container + sizeof container;

	size_t count = 0;
	size_t keys_output_size = 0;
	size_t values_output_size = 0;

	// We keep track of the item that will come last, since it's treated
	// specially:
	XXH128_hash_t highest_key_hash = {0};
	size_t highest_key_size = 0;
	const void *highest_key_data = NULL;
	size_t highest_value_output_size = 0;

	size_t object_item_offset = spool_offset;
	while(object_item_offset < spool_used) {
		size_t key_size = _bijson_buffer_read_size(&writer->spool, object_item_offset);

		keys_output_size += key_size;
		object_item_offset += sizeof key_size;

		const void *key_data = _bijson_buffer_access(&writer->spool, object_item_offset, key_size);
		XXH128_hash_t key_hash = XXH3_128bits(key_data, key_size);

		object_item_offset += key_size;
		size_t value_output_size = _bijson_writer_size_value(writer, object_item_offset);
		values_output_size += value_output_size;

		// Determine the last key/value pair that would result from stable sorting:
		int c = 0;
		if(count) {
			c = XXH128_cmp(&key_hash, &highest_key_hash);
			if(!c)
				c = key_size == highest_key_size
					? memcmp(key_data, highest_key_data, key_size)
					: key_size < highest_key_size ? -1 : 1;
		}
		if(c >= 0) {
			highest_key_hash = key_hash;
			highest_key_size = key_size;
			highest_key_data = key_data;
			highest_value_output_size = value_output_size;
		}

		object_item_offset++;
		object_item_offset += _bijson_buffer_read_size(&writer->spool, object_item_offset);
		object_item_offset += sizeof(size_t);

		count++;
	}

	size_t count_1 = count - SIZE_C(1);

	container.output_size = count
		? 1 + _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size1(count))
			+ count * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(keys_output_size))
			+ count_1 * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(
				values_output_size - highest_value_output_size - count_1
			))
			+ keys_output_size + values_output_size
		: 1;

	_bijson_buffer_write(&writer->spool, current_container, &container, sizeof container);

	writer->current_container = _bijson_buffer_pop_size(&writer->stack);
	_bijson_container_restore_expect(writer);

	return NULL;
}

static int _bijson_writer_object_object_item_cmp(const void *a, const void *b) {
	const byte_t *a_item = *(byte_t * const *)a;
	const byte_t *b_item = *(byte_t * const *)b;
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
	c = a_size == b_size
		? memcmp(a_item, b_item, a_size)
		: a_size < b_size ? -1 : 1;
	if(c)
		return c;
	// stable sorting (which we need because we need to end up with the exact
	// same last item as as bijson_writer_end_object()):
	return a_item < b_item ? -1 : a_item != b_item;
}

bijson_error_t _bijson_writer_write_object(bijson_writer_t *writer, bijson_output_callback_t write, void *write_data, const byte_t *spool) {
	_bijson_container_t container;
	memcpy(&container, spool, sizeof container);
	spool += sizeof container;

	if(container.spool_size == sizeof container.output_size)
		return write(write_data, "\x40", SIZE_C(1));

	const byte_t *spool_end = spool + container.spool_size - sizeof container.spool_size;
	size_t stack_used = writer->stack.used;

	size_t count = 0;
	size_t keys_output_size = 0;
	size_t values_output_size = 0;
	size_t key_size;

	// Build the array for sorting by going through the memory buffer and
	// compute the largest value offset that we'll actually store
	const byte_t *object_item = spool;
	while(object_item != spool_end) {
		_BIJSON_RETURN_ON_ERROR(_bijson_buffer_push(&writer->stack, &object_item, sizeof object_item));
		memcpy(&key_size, object_item, sizeof key_size);
		keys_output_size += key_size;
		object_item += sizeof key_size;
		object_item += key_size;
		values_output_size += _bijson_writer_size_value(writer, _bijson_buffer_offset(&writer->spool, object_item));
		object_item++;
		size_t value_spool_size;
		memcpy(&value_spool_size, object_item, sizeof value_spool_size);
		object_item += sizeof value_spool_size;
		object_item += value_spool_size;
		count++;
	}

	const byte_t **object_items;
	size_t object_items_size = count * sizeof *object_items;
	object_items = _bijson_buffer_access(&writer->stack, stack_used, object_items_size);
	qsort(object_items, count, sizeof *object_items, _bijson_writer_object_object_item_cmp);

	// Now that we know the order, subtract the size of the last item from
	// values_output_size, since we won't actually store that (it's computed
	// from the bounding size).
	size_t count_1 = count - SIZE_C(1);
	object_item = object_items[count_1];
	memcpy(&key_size, object_item, sizeof key_size);
	object_item += sizeof key_size;
	object_item += key_size;
	values_output_size -= _bijson_writer_size_value(writer, _bijson_buffer_offset(&writer->spool, object_item));

	// We do not include the type bytes in the offsets (they're implicit)
	values_output_size -= count_1;

	byte_compute_t count_width = _bijson_optimal_storage_size(count_1);
	byte_compute_t key_offsets_width = _bijson_optimal_storage_size(keys_output_size);
	byte_compute_t value_offsets_width = _bijson_optimal_storage_size(values_output_size);
	byte_t output_type = (byte_t)(BYTE_C(0x40) | (value_offsets_width << 4U) | (key_offsets_width << 2U) | count_width);

	_BIJSON_RETURN_ON_ERROR(write(write_data, &output_type, sizeof output_type));
	_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_compact_int(write, write_data, count_1, count_width));

	// Write the key offsets
	size_t key_offset = 0;
	for(size_t z = 0; z < count; z++) {
		object_item = object_items[z];
		memcpy(&key_size, object_item, sizeof key_size);
		key_offset += key_size;
		_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_compact_int(write, write_data, key_offset, key_offsets_width));
	}

	// Write the value offsets
	size_t value_output_offset = 0;
	for(size_t z = 0; z < count_1; z++) {
		object_item = object_items[z];
		memcpy(&key_size, object_item, sizeof key_size);
		object_item += sizeof key_size;
		object_item += key_size;
		value_output_offset += _bijson_writer_size_value(writer, _bijson_buffer_offset(&writer->spool, object_item)) - SIZE_C(1);
		_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_compact_int(write, write_data, value_output_offset, value_offsets_width));
	}

	// Write the keys
	for(size_t z = 0; z < count; z++) {
		object_item = object_items[z];
		memcpy(&key_size, object_item, sizeof key_size);
		object_item += sizeof key_size;
		_BIJSON_RETURN_ON_ERROR(write(write_data, object_item, key_size));
	}

	// Write the values
	for(size_t z = 0; z < count; z++) {
		// object_items may move during _bijson_writer_write_value()
		_bijson_buffer_read(&writer->stack, stack_used + z * sizeof object_item, &object_item, sizeof object_item);
		memcpy(&key_size, object_item, sizeof key_size);
		object_item += sizeof key_size;
		object_item += key_size;
		_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_value(writer, write, write_data, object_item));
	}

	_bijson_buffer_pop(&writer->stack, NULL, object_items_size);
	return NULL;
}
