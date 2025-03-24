#include <stdbool.h>

#include <bijson/writer.h>

#include "common.h"
#include "string.h"
#include "writer.h"

static inline bijson_error_t _bijson_writer_add_constant(bijson_writer_t *writer, byte type) {
	if(writer->failed)
		return bijson_error_writer_failed;
	_BIJSON_ERROR_RETURN(_bijson_writer_check_expect_value(writer));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &_bijson_spool_type_scalar, sizeof _bijson_spool_type_scalar));
	static const size_t size = SIZE_C(1);
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &size, sizeof size));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &type, SIZE_C(1)));
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
