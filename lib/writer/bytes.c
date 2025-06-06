#include <stdbool.h>

#include "../../include/writer.h"

#include "../common.h"
#include "../writer.h"

bijson_error_t bijson_writer_add_bytes(bijson_writer_t *writer, const void *bytes, size_t len) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	_BIJSON_RETURN_ON_ERROR(_bijson_writer_check_expect_value(writer));

	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, _bijson_spool_type_scalar));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->spool, len + SIZE_C(1)));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, BYTE_C(0x09)));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push(&writer->spool, bytes, len));

	writer->expect = writer->expect_after_value;
	return NULL;
}

bijson_error_t bijson_writer_begin_bytes(bijson_writer_t *writer) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	_BIJSON_RETURN_ON_ERROR(_bijson_writer_check_expect_value(writer));

	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, _bijson_spool_type_scalar));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->stack, writer->spool.used));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->spool, SIZE_C(0)));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, BYTE_C(0x09)));

	writer->expect = _bijson_writer_expect_more_bytes;
	return NULL;
}

bijson_error_t bijson_writer_append_bytes(bijson_writer_t *writer, const void *bytes, size_t len) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	if(writer->expect != _bijson_writer_expect_more_bytes)
		_BIJSON_RETURN_ERROR(bijson_error_unmatched_end);

	return _bijson_buffer_push(&writer->spool, bytes, len);
}

bijson_error_t bijson_writer_end_bytes(bijson_writer_t *writer) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	if(writer->expect != _bijson_writer_expect_more_bytes)
		_BIJSON_RETURN_ERROR(bijson_error_unmatched_end);

	size_t spool_used = _bijson_buffer_pop_size(&writer->stack);
	size_t data_len = writer->spool.used - spool_used - sizeof data_len;
	_bijson_buffer_write_size(&writer->spool, spool_used, data_len);

	writer->expect = writer->expect_after_value;
	return NULL;
}
