#pragma once

#include "../common.h"
#include "../writer.h"

extern bijson_error_t _bijson_writer_write_object(bijson_writer_t *writer, bijson_output_callback_t write, void *write_data, const byte_t *spool);
