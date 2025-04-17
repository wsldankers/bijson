#include "buffer.h"
#include "container.h"

bool bijson_writer_expects_value(const bijson_writer_t *writer) {
	return writer->expect == _bijson_writer_expect_value;
}

bool bijson_writer_expects_key(const bijson_writer_t *writer) {
	return writer->expect == _bijson_writer_expect_key;
}

void _bijson_container_restore_expect(bijson_writer_t *writer) {
	size_t current_container = writer->current_container;
	if(current_container) {
		_bijson_spool_type_t type = _bijson_buffer_read_byte(&writer->spool, current_container - SIZE_C(1));
		assert(type == _bijson_spool_type_object || type == _bijson_spool_type_array);
		if(type == _bijson_spool_type_object)
			writer->expect = writer->expect_after_value = _bijson_writer_expect_key;
		else if(type == _bijson_spool_type_array)
			writer->expect = writer->expect_after_value = _bijson_writer_expect_value;
	} else {
		writer->expect = writer->expect_after_value = _bijson_writer_expect_none;
	}
}

size_t _bijson_writer_size_value(bijson_writer_t *writer, size_t spool_offset) {
	_bijson_spool_type_t spool_type = _bijson_buffer_read_byte(&writer->spool, spool_offset++);
	if(spool_type == _bijson_spool_type_scalar) {
		return _bijson_buffer_read_size(&writer->spool, spool_offset);
	} else {
		assert(spool_type == _bijson_spool_type_object
			|| spool_type == _bijson_spool_type_array);
		return _bijson_buffer_read_size(&writer->spool, spool_offset + sizeof(size_t));
	}
}
