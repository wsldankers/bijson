#include <stdbool.h>

#include "common.h"
#include "container.h"
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

bijson_error_t bijson_writer_begin_string(bijson_writer_t *writer) {
	if(writer->failed)
		return bijson_error_writer_failed;
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &_bijson_spool_type_scalar, sizeof _bijson_spool_type_scalar));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->stack, &writer->current_container, sizeof writer->current_container));
	writer->current_container = writer->spool.used;
	size_t size = SIZE_C(1);
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, &size, sizeof size));
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, "\x08", 1));
	writer->current_type = _BIJSON_WRITER_TYPE_SCALAR;
	return NULL;
}

static inline bijson_error_t _bijson_check_if_in_string(bijson_writer_t *writer) {
	if(writer->failed)
		return bijson_error_writer_failed;

	if(writer->current_type != _BIJSON_WRITER_TYPE_SCALAR)
		return bijson_error_unmatched_end;

	uint8_t type;
	_bijson_buffer_read(&writer->spool, writer->current_container + sizeof(size_t), &type, sizeof type);
	if(type != UINT8_C(0x08))
		return bijson_error_unmatched_end;
	return NULL;
}

bijson_error_t bijson_writer_append_string(bijson_writer_t *writer, const char *string, size_t len) {
	_BIJSON_ERROR_RETURN(_bijson_check_if_in_string(writer));
	size_t current_container = writer->current_container;
	size_t size;
	_bijson_buffer_read(&writer->spool, current_container, &size, sizeof size);
	if(writer->spool.used - current_container - sizeof size != size)
		return bijson_error_unmatched_end;
	size += len;
	_bijson_buffer_write(&writer->spool, current_container, &size, sizeof size);
	_BIJSON_WRITER_ERROR_RETURN(_bijson_buffer_append(&writer->spool, string, len));
	return NULL;
}

bijson_error_t bijson_writer_end_string(bijson_writer_t *writer) {
	_BIJSON_ERROR_RETURN(_bijson_check_if_in_string(writer));
	size_t current_container = writer->current_container;
	size_t size;
	_bijson_buffer_read(&writer->spool, current_container, &size, sizeof size);
	if(writer->spool.used - current_container - sizeof size != size)
		return bijson_error_unmatched_end;

	size--;
	_BIJSON_WRITER_ERROR_RETURN(_bijson_check_valid_utf8(
		_bijson_buffer_access(&writer->spool, current_container + sizeof size, size),
		size
	));

	_bijson_buffer_pop(&writer->stack, &writer->current_container, sizeof writer->current_container);
	_bijson_container_restore_current_type(writer);
	return NULL;
}
