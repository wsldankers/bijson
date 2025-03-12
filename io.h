#pragma once

#include <stdlib.h>
#include <stdio.h>

#include <bijson/common.h>

typedef bijson_error_t (*bijson_output_callback_t)(
	void *output_callback_data,
	const void *data,
	size_t len
);

typedef bijson_error_t (*bijson_output_action_callback_t)(
	void *action_callback_data,
	bijson_output_callback_t output_callback,
	void *output_callback_data
);

extern bijson_error_t _bijson_io_bytecounter_output_callback(
	void *output_callback_data,
	const void *data,
	size_t len
);

extern bijson_error_t _bijson_io_write_to_fd(
	bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	int fd
);

extern bijson_error_t _bijson_io_write_to_FILE(
	bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	FILE *file
);

extern bijson_error_t _bijson_io_write_to_malloc(
	bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	void **result_buffer,
	size_t *result_size
);

extern bijson_error_t _bijson_io_write_bytecounter(
	bijson_output_action_callback_t action_callback,
	void *action_callback_data,
	size_t *result_size
);
