#include <stdbool.h>

#include "../common.h"
#include "../writer.h"

bijson_error_t bijson_writer_add_string(bijson_writer_t *writer, const void *string, size_t len) {
	if(writer->failed)
		return bijson_error_writer_failed;
	_BIJSON_ERROR_RETURN(_bijson_writer_check_expect_value(writer));
	_BIJSON_ERROR_RETURN(_bijson_check_valid_utf8((const byte_t *)string, len));

	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, _bijson_spool_type_scalar));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->spool, len + SIZE_C(1)));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, BYTE_C(0x08)));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push(&writer->spool, string, len));

	writer->expect = writer->expect_after_value;
	return NULL;
}

bijson_error_t bijson_writer_begin_string(bijson_writer_t *writer) {
	if(writer->failed)
		return bijson_error_writer_failed;
	_BIJSON_ERROR_RETURN(_bijson_writer_check_expect_value(writer));

	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, _bijson_spool_type_scalar));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->stack, writer->spool.used));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->spool, SIZE_C(0)));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, BYTE_C(0x08)));

	writer->expect = _bijson_writer_expect_more_string;
	return NULL;
}

bijson_error_t bijson_writer_append_string(bijson_writer_t *writer, const void *string, size_t len) {
	if(writer->failed)
		return bijson_error_writer_failed;
	if(writer->expect != _bijson_writer_expect_more_string)
		return bijson_error_unmatched_end;

	return _bijson_buffer_push(&writer->spool, string, len);
}

bijson_error_t bijson_writer_end_string(bijson_writer_t *writer) {
	if(writer->failed)
		return bijson_error_writer_failed;
	if(writer->expect != _bijson_writer_expect_more_string)
		return bijson_error_unmatched_end;

	size_t spool_used = _bijson_buffer_pop_size(&writer->stack);
	size_t data_offset = spool_used + sizeof(size_t);
	size_t data_len = writer->spool.used - data_offset;
	_bijson_buffer_write_size(&writer->spool, spool_used, data_len);

	size_t string_len = data_len - SIZE_C(1);
	_BIJSON_WRITER_ERROR_RETURN(_bijson_check_valid_utf8(
		_bijson_buffer_access(&writer->spool, data_offset + SIZE_C(1), string_len),
		string_len
	));

	writer->expect = writer->expect_after_value;
	return NULL;
}
