#pragma once

#include <bijson/writer.h>

#include "writer.h"

extern size_t _bijson_writer_size_container(bijson_writer_t *writer, size_t spool_offset);
extern bijson_error_t _bijson_writer_write_object(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool);
extern bijson_error_t _bijson_writer_write_array(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool);

static inline _bijson_spool_type_t _bijson_container_get_current_type(bijson_writer_t *writer) {
	if(!writer->current_container)
		return _bijson_spool_type_scalar;
	_bijson_spool_type_t type;
	_bijson_buffer_read(&writer->spool, writer->current_container - sizeof type, &type, sizeof type);
	return type;
}
