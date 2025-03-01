#pragma once

#include "writer.h"

extern bool bijson_writer_begin_object(bijson_writer_t *writer);
extern bool bijson_writer_begin_array(bijson_writer_t *writer);
extern bool bijson_writer_add_key(bijson_writer_t *writer, const char *key, size_t len);
extern bool bijson_writer_end_object(bijson_writer_t *writer);
extern bool bijson_writer_end_array(bijson_writer_t *writer);

extern size_t _bijson_writer_size_container(bijson_writer_t *writer, size_t spool_offset);
extern bool _bijson_writer_write_object(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool);
extern bool _bijson_writer_write_array(bijson_writer_t *writer, _bijson_writer_write_func_t write, void *write_data, const char *spool);
