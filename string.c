#include <stdbool.h>

#include "common.h"
#include "string.h"
#include "writer.h"

bijson_error_t bijson_writer_add_string(bijson_writer_t *writer, const char *string, size_t len) {
	_BIJSON_WRITER_ERROR_RETURN(_bijson_check_valid_utf8(string, len));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &_bijson_spool_type_scalar, sizeof _bijson_spool_type_scalar));
	size_t size = len + 1;
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &size, sizeof size));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, "\x08", 1));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, string, len));
	return NULL;
}
