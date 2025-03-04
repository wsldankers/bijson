#include <stdbool.h>

#include "common.h"
#include "string.h"
#include "writer.h"

static inline bool _bijson_writer_add_constant(bijson_writer_t *writer, uint8_t type) {
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &_bijson_spool_type_scalar, sizeof _bijson_spool_type_scalar));
	static const size_t size = SIZE_C(1);
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &size, sizeof size));
	_BIJSON_CHECK(_bijson_buffer_push(&writer->spool, &type, 1));
	return true;
}

bool bijson_writer_add_null(bijson_writer_t *writer) {
	return _bijson_writer_add_constant(writer, 0x01);
}

bool bijson_writer_add_false(bijson_writer_t *writer) {
	return _bijson_writer_add_constant(writer, 0x02);
}

bool bijson_writer_add_true(bijson_writer_t *writer) {
	return _bijson_writer_add_constant(writer, 0x03);
}
