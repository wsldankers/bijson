#pragma once

#include "../../include/writer.h"

#include "../writer.h"

extern size_t _bijson_writer_size_container(bijson_writer_t *writer, size_t spool_offset);
extern bijson_error_t _bijson_writer_write_object(bijson_writer_t *writer, bijson_output_callback_t write, void *write_data, const byte_t *spool);
extern bijson_error_t _bijson_writer_write_array(bijson_writer_t *writer, bijson_output_callback_t write, void *write_data, const byte_t *spool);
