#include <stdbool.h>

#include "../../include/writer.h"

#include "../common.h"
#include "../writer.h"

static inline bijson_error_t _bijson_writer_add_constant(bijson_writer_t *writer, byte_t type) {
	if(writer->failed)
		_BIJSON_RETURN_ERROR(bijson_error_writer_failed);
	_BIJSON_ERROR_RETURN(_bijson_writer_check_expect_value(writer));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, _bijson_spool_type_scalar));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_size(&writer->spool, SIZE_C(1)));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_push_byte(&writer->spool, type));
	writer->expect = writer->expect_after_value;
	return NULL;
}

bijson_error_t bijson_writer_add_null(bijson_writer_t *writer) {
	return _bijson_writer_add_constant(writer, BYTE_C(0x01));
}

bijson_error_t bijson_writer_add_false(bijson_writer_t *writer) {
	return _bijson_writer_add_constant(writer, BYTE_C(0x02));
}

bijson_error_t bijson_writer_add_true(bijson_writer_t *writer) {
	return _bijson_writer_add_constant(writer, BYTE_C(0x03));
}
