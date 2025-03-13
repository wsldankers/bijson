#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef const char *bijson_error_t;

extern const char bijson_error_system[];
extern const char bijson_error_index_out_of_range[];
extern const char bijson_error_key_not_found[];
extern const char bijson_error_invalid_decimal_syntax[];
extern const char bijson_error_invalid_json_syntax[];
extern const char bijson_error_invalid_utf8[];
extern const char bijson_error_parameter_is_zero[];
extern const char bijson_error_parameter_is_null[];
extern const char bijson_error_internal_error[];
extern const char bijson_error_unsupported_feature[];
extern const char bijson_error_unsupported_data_type[];
extern const char bijson_error_file_format_error[];
extern const char bijson_error_value_out_of_range[];
extern const char bijson_error_key_before_value[];
extern const char bijson_error_value_before_key[];
extern const char bijson_error_writer_failed[];
extern const char bijson_error_unmatched_end[];
extern const char bijson_error_bad_root[];

typedef bijson_error_t (*bijson_output_callback_t)(
	void *output_callback_data,
	const void *data,
	size_t len
);
