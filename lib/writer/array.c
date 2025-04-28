#include "array.h"
#include "container.h"

bijson_error_t bijson_writer_begin_array(bijson_writer_t *writer) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	_BIJSON_RETURN_ON_ERROR(_bijson_writer_check_expect_value(writer));
	writer->expect = writer->expect_after_value = _bijson_writer_expect_value;
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, _bijson_spool_type_array));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->stack, writer->current_container));
	writer->current_container = writer->spool.used;
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push(&writer->spool, &_bijson_container_0, sizeof _bijson_container_0));
	return NULL;
}

bijson_error_t bijson_writer_end_array(bijson_writer_t *writer) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	if(writer->expect != _bijson_writer_expect_value
	|| writer->expect_after_value != _bijson_writer_expect_value)
		_BIJSON_RETURN_ERROR(bijson_error_unmatched_end);

	size_t spool_used = writer->spool.used;
	size_t current_container = writer->current_container;
	_bijson_container_t container = _bijson_container_0;
	container.spool_size = spool_used - current_container - sizeof container.spool_size;

	size_t spool_offset = current_container + sizeof container;

	size_t count = 0;
	size_t items_output_size = 0;
	size_t last_item_output_size = 0;

	size_t item_offset = spool_offset;
	while(item_offset < spool_used) {
		last_item_output_size = _bijson_writer_size_value(writer, item_offset);
		items_output_size += last_item_output_size;
		item_offset++;
		size_t item_spool_size = _bijson_buffer_read_size(&writer->spool, item_offset);
		item_offset += sizeof item_spool_size;
		item_offset += item_spool_size;
		count++;
	}

	size_t count_1 = count - SIZE_C(1);

	container.output_size = count
		? 1 + _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size1(count))
			+ count_1 * _bijson_optimal_storage_size_bytes(_bijson_optimal_storage_size(
				items_output_size - last_item_output_size - count_1
			))
			+ items_output_size
		: 1;

	_bijson_buffer_write(&writer->spool, current_container, &container, sizeof container);

	writer->current_container = _bijson_buffer_pop_size(&writer->stack);
	_bijson_container_restore_expect(writer);

	return NULL;
}

bijson_error_t _bijson_writer_write_array(bijson_writer_t *writer, bijson_output_callback_t write, void *write_data, const byte_t *spool) {
	_bijson_container_t container;
	memcpy(&container, spool, sizeof container);
	spool += sizeof container;

	if(container.spool_size == sizeof container.output_size)
		return write(write_data, "\x30", SIZE_C(1));

	const byte_t *spool_end = spool + container.spool_size - sizeof container.spool_size;

	size_t count_1 = 0;
	size_t items_output_size = 0;

	const byte_t *item = spool;
	for(;;) {
		const byte_t *this_item = item;
		item++;
		size_t item_spool_size;
		memcpy(&item_spool_size, item, sizeof item_spool_size);
		item += sizeof item_spool_size;
		item += item_spool_size;
		if(item == spool_end)
			break;
		items_output_size += _bijson_writer_size_value(writer, _bijson_buffer_offset(&writer->spool, this_item));
		count_1++;
	}

	items_output_size -= count_1;

	byte_compute_t count_width = _bijson_optimal_storage_size(count_1);
	byte_compute_t item_offsets_width = _bijson_optimal_storage_size(items_output_size);
	byte_t output_type = (byte_t)(BYTE_C(0x30) | (item_offsets_width << 2U) | count_width);

	_BIJSON_RETURN_ON_ERROR(write(write_data, &output_type, sizeof output_type));
	_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_compact_int(write, write_data, count_1, count_width));

	// Write the item offsets
	item = spool;
	size_t item_output_offset = 0;
	for(size_t z = 0; z < count_1; z++) {
		item_output_offset += _bijson_writer_size_value(writer, _bijson_buffer_offset(&writer->spool, item)) - SIZE_C(1);
		item++;
		size_t item_spool_size;
		memcpy(&item_spool_size, item, sizeof item_spool_size);
		item += sizeof item_spool_size;
		item += item_spool_size;
		_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_compact_int(write, write_data, item_output_offset, item_offsets_width));
	}

	size_t count = count_1 + SIZE_C(1);

	// Write the element values
	item = spool;
	for(size_t z = 0; z < count; z++) {
		_BIJSON_RETURN_ON_ERROR(_bijson_writer_write_value(writer, write, write_data, item));
		item++;
		size_t item_spool_size;
		memcpy(&item_spool_size, item, sizeof item_spool_size);
		item += sizeof item_spool_size;
		item += item_spool_size;
	}

	return NULL;
}
