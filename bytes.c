#include <stdbool.h>

#include "common.h"
#include "string.h"
#include "writer.h"

bool bijson_writer_add_bytes(bijson_writer_t *writer, const void *bytes, size_t len) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_spool_type_scalar, sizeof _bijson_spool_type_scalar));
	size_t size = len + SIZE_C(1);
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &size, sizeof size));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, "\x09", SIZE_C(1)));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, bytes, len));
	return true;
}
