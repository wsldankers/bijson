#include <stdbool.h>

#include "common.h"
#include "error.h"
#include "string.h"
#include "writer.h"

bijson_error_t bijson_writer_add_bytes(bijson_writer_t *writer, const void *bytes, size_t len) {
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &_bijson_spool_type_scalar, sizeof _bijson_spool_type_scalar));
	size_t size = len + SIZE_C(1);
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &size, sizeof size));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, "\x09", SIZE_C(1)));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, bytes, len));
	return NULL;
}
