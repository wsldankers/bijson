#pragma once

#include <stdlib.h>

#include <bijson/common.h>

typedef bool (*bijson_output_action_callback)(
    void *action_callback_data,
    bijson_output_callback output_callback,
    void *output_callback_data
);


extern bool _bijson_io_bytecounter_output_callback(void *output_callback_data, const void *data, size_t len);

extern bool _bijson_io_write_to_fd(
    bijson_output_action_callback action_callback,
    void *action_callback_data,
    int fd
);

extern bool _bijson_io_write_to_malloc(
    bijson_output_action_callback action_callback,
    void *action_callback_data,
    void **result_buffer,
    size_t *result_size
);

extern bool _bijson_io_write_bytecounter(
    bijson_output_action_callback action_callback,
    void *action_callback_data,
    size_t *result_size
);
