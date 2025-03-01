#include <stdbool.h>

#include "string.h"
#include "writer.h"

bool bijson_writer_add_string(bijson_writer_t *writer, const char *string, size_t len) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_spool_type_scalar, sizeof _bijson_spool_type_scalar));
	size_t size = len + 1;
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &size, sizeof size));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, "\x08", 1));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, string, len));
	return true;
}
