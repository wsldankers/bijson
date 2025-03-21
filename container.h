#pragma once

#include <bijson/writer.h>

#include "writer.h"

extern size_t _bijson_writer_size_container(bijson_writer_t *writer, size_t spool_offset);
extern bijson_error_t _bijson_writer_write_object(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool);
extern bijson_error_t _bijson_writer_write_array(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool);

// Whether the writer is ready to accept a new value:
static inline bool _bijson_writer_accepts_value(bijson_writer_t *writer) {
	_bijson_writer_type_t current_type = writer->current_type;
	return current_type == _BIJSON_WRITER_TYPE_ARRAY
		|| current_type == _BIJSON_WRITER_TYPE_OBJECT
		|| current_type == _BIJSON_WRITER_TYPE_NONE;
}

// Check whether the writer is ready to accept a new value:
static inline bijson_error_t _bijson_writer_check_accepts_value(bijson_writer_t *writer) {
	return _bijson_writer_accepts_value(writer)
		? NULL
		: bijson_error_unmatched_end;
}

extern void _bijson_container_restore_current_type(bijson_writer_t *writer);
