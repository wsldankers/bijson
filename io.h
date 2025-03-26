#pragma once

#include <stdlib.h>
#include <stdio.h>

#include <bijson/common.h>
#include <bijson/reader.h>

typedef bijson_error_t (*_bijson_output_action_callback_t)(
	void *action_callback_data,
	bijson_output_callback_t output_callback,
	void *output_callback_data
);

bijson_error_t _bijson_io_write_nul_bytes(
	bijson_output_callback_t write,
	void *write_data,
	size_t len
);

// To accommodate an evil optimization in decimal.c
extern bijson_error_t _bijson_io_bytecounter_output_callback(
	void *output_callback_data,
	const void *data,
	size_t len
);

extern bijson_error_t _bijson_io_write_to_fd(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	int fd
);

extern bijson_error_t _bijson_io_write_to_FILE(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	FILE *file
);

extern bijson_error_t _bijson_io_write_to_malloc(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	const void **result_buffer,
	size_t *result_size
);

extern bijson_error_t _bijson_io_write_bytecounter(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	size_t *result_size
);

extern bijson_error_t _bijson_io_write_to_filename(
	_bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	const char *filename
);

typedef bijson_error_t (*bijson_input_action_t)(
	void *action_data,
	const void *buffer,
	size_t buffer_size
);

extern bijson_error_t _bijson_io_read_from_filename(
	bijson_input_action_t action,
	const void *action_data,
	const char *filename
);

extern void _bijson_io_close(bijson_t *bijson);
