#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef struct bijson {
	const void *buffer;
	size_t size;
} bijson_t;

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
extern const char bijson_error_value_expected[];
extern const char bijson_error_key_expected[];
extern const char bijson_error_writer_failed[];
extern const char bijson_error_unmatched_end[];
extern const char bijson_error_bad_root[];
extern const char bijson_error_out_of_virtual_memory[];
extern const char bijson_error_type_mismatch[];
extern const char bijson_error_duplicate_key[];

typedef bijson_error_t (*bijson_output_callback_t)(
	void *output_callback_data,
	const void *data,
	size_t len
);
