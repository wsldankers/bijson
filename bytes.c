#include <stdbool.h>

#include <bijson/writer.h>

#include "common.h"
#include "writer.h"

bijson_error_t bijson_writer_add_bytes(bijson_writer_t *writer, const void *bytes, size_t len) {
	if(writer->failed)
		return bijson_error_writer_failed;
	_BIJSON_ERROR_RETURN(_bijson_writer_check_expect_value(writer));

	size_t size = len + SIZE_C(1);
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &_bijson_spool_type_scalar, sizeof _bijson_spool_type_scalar));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &size, sizeof size));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, "\x09", SIZE_C(1)));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, bytes, len));

	writer->expect = writer->expect_after_value;
	return NULL;
}

bijson_error_t bijson_writer_begin_bytes(bijson_writer_t *writer) {
	if(writer->failed)
		return bijson_error_writer_failed;
	_BIJSON_ERROR_RETURN(_bijson_writer_check_expect_value(writer));

	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &_bijson_spool_type_scalar, sizeof _bijson_spool_type_scalar));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->stack, &writer->spool.used, sizeof writer->spool.used));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, NULL, sizeof(size_t)));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, "\x09", SIZE_C(1)));

	writer->expect = _BIJSON_WRITER_EXPECT_MORE_BYTES;
	return NULL;
}

bijson_error_t bijson_writer_append_bytes(bijson_writer_t *writer, const void *bytes, size_t len) {
	if(writer->failed)
		return bijson_error_writer_failed;
	if(writer->expect != _BIJSON_WRITER_EXPECT_MORE_BYTES)
		return bijson_error_unmatched_end;

	return _bijson_buffer_append(&writer->spool, bytes, len);
}

bijson_error_t bijson_writer_end_bytes(bijson_writer_t *writer) {
	if(writer->failed)
		return bijson_error_writer_failed;
	if(writer->expect != _BIJSON_WRITER_EXPECT_MORE_BYTES)
		return bijson_error_unmatched_end;

	size_t spool_used;
	_bijson_buffer_pop(&writer->stack, &spool_used, sizeof spool_used);

	size_t total_len = writer->spool.used - spool_used - sizeof total_len;
	_bijson_buffer_write(&writer->spool, spool_used, &total_len, sizeof total_len);

	writer->expect = writer->expect_after_value;
	return NULL;
}
