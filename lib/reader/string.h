#pragma once

#include "../common.h"

extern bijson_error_t _bijson_raw_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data);
extern bijson_error_t _bijson_string_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *userdata);
