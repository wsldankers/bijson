#pragma once

#include <stdbool.h>
#include <stdlib.h>

typedef struct bijson_writer bijson_writer_t;

extern void bijson_writer_free(bijson_writer_t *writer);
extern bijson_writer_t *bijson_writer_alloc(void);
extern bool bijson_writer_write_to_fd(bijson_writer_t *writer, int fd);
extern bool bijson_writer_write_to_buffer(bijson_writer_t *writer, void **result_buffer, size_t *result_size);

extern bool bijson_writer_begin_object(bijson_writer_t *writer);
extern bool bijson_writer_begin_array(bijson_writer_t *writer);
extern bool bijson_writer_add_key(bijson_writer_t *writer, const char *key, size_t len);
extern bool bijson_writer_end_object(bijson_writer_t *writer);
extern bool bijson_writer_end_array(bijson_writer_t *writer);

extern bool bijson_writer_add_decimal_from_string(bijson_writer_t *writer, const char *string, size_t len);
extern bool bijson_writer_add_bytes(bijson_writer_t *writer, const void *bytes, size_t len);
extern bool bijson_writer_add_string(bijson_writer_t *writer, const char *string, size_t len);
extern bool bijson_writer_add_null(bijson_writer_t *writer);
extern bool bijson_writer_add_false(bijson_writer_t *writer);
extern bool bijson_writer_add_true(bijson_writer_t *writer);
