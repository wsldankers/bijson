#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"

typedef struct bijson_writer bijson_writer_t;

extern void bijson_writer_free(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_alloc(bijson_writer_t **result);

extern bijson_error_t bijson_writer_write_to_fd(bijson_writer_t *writer, int fd);
extern bijson_error_t bijson_writer_write_to_FILE(bijson_writer_t *writer, FILE *file);
extern bijson_error_t bijson_writer_write_to_malloc(bijson_writer_t *writer, void **result_buffer, size_t *result_size);

extern bijson_error_t bijson_writer_begin_array(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_end_array(bijson_writer_t *writer);

extern bijson_error_t bijson_writer_begin_object(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_add_key(bijson_writer_t *writer, const char *key, size_t len);
extern bijson_error_t bijson_writer_end_object(bijson_writer_t *writer);

extern bijson_error_t bijson_writer_add_decimal_from_string(bijson_writer_t *writer, const char *string, size_t len);
extern bijson_error_t bijson_writer_add_bytes(bijson_writer_t *writer, const void *bytes, size_t len);
extern bijson_error_t bijson_writer_add_string(bijson_writer_t *writer, const char *string, size_t len);
extern bijson_error_t bijson_writer_add_null(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_add_false(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_add_true(bijson_writer_t *writer);
