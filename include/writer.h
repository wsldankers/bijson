#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"

typedef struct bijson_writer bijson_writer_t;

extern void bijson_writer_free(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_alloc(bijson_writer_t **result);

extern bool bijson_writer_expects_value(const bijson_writer_t *writer)  __attribute__((pure));
extern bool bijson_writer_expects_key(const bijson_writer_t *writer) __attribute__((pure));

extern bijson_error_t bijson_parse_json(bijson_writer_t *writer, const void *buffer, size_t len, size_t *parse_end);
extern bijson_error_t bijson_parse_json_filename(bijson_writer_t *writer, const char *filename, size_t *parse_end);

extern bijson_error_t bijson_writer_write_to_fd(bijson_writer_t *writer, int fd);
extern bijson_error_t bijson_writer_write_to_FILE(bijson_writer_t *writer, FILE *file);
extern bijson_error_t bijson_writer_write_to_malloc(bijson_writer_t *writer, bijson_t *bijson);
extern bijson_error_t bijson_writer_write_to_filename(bijson_writer_t *writer, const char *filename);
extern bijson_error_t bijson_writer_write_to_tempfile(bijson_writer_t *writer, bijson_t *bijson);
extern bijson_error_t bijson_writer_write_bytecounter(bijson_writer_t *writer, size_t *result_size);

extern bijson_error_t bijson_writer_begin_array(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_end_array(bijson_writer_t *writer);

extern bijson_error_t bijson_writer_begin_object(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_end_object(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_add_key(bijson_writer_t *writer, const void *key, size_t len);

extern bijson_error_t bijson_writer_begin_key(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_append_key(bijson_writer_t *writer, const void *key, size_t len);
extern bijson_error_t bijson_writer_end_key(bijson_writer_t *writer);

extern bijson_error_t bijson_writer_add_decimal_from_string(bijson_writer_t *writer, const void *string, size_t len);
extern bijson_error_t bijson_writer_add_bytes(bijson_writer_t *writer, const void *bytes, size_t len);
extern bijson_error_t bijson_writer_add_string(bijson_writer_t *writer, const void *string, size_t len);
extern bijson_error_t bijson_writer_add_null(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_add_false(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_add_true(bijson_writer_t *writer);

extern bijson_error_t bijson_writer_begin_string(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_append_string(bijson_writer_t *writer, const void *string, size_t len);
extern bijson_error_t bijson_writer_end_string(bijson_writer_t *writer);

extern bijson_error_t bijson_writer_begin_bytes(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_append_bytes(bijson_writer_t *writer, const void *bytes, size_t len);
extern bijson_error_t bijson_writer_end_bytes(bijson_writer_t *writer);

extern bijson_error_t bijson_writer_begin_decimal_from_string(bijson_writer_t *writer);
extern bijson_error_t bijson_writer_append_decimal_from_string(bijson_writer_t *writer, const void *string, size_t len);
extern bijson_error_t bijson_writer_end_decimal_from_string(bijson_writer_t *writer);
