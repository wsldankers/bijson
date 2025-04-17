#pragma once

#include "../common.h"

typedef struct _bijson_object_analysis {
	size_t count;
	size_t count_1;
	const byte_t *key_index;
	size_t key_index_item_size;
	size_t last_key_end_offset;
	const byte_t *key_data_start;
	const byte_t *value_index;
	size_t value_index_item_size;
	const byte_t *value_data_start;
	size_t value_data_size;
} _bijson_object_analysis_t;

extern bijson_error_t _bijson_object_analyze(const bijson_t *bijson, _bijson_object_analysis_t *analysis);
extern bijson_error_t _bijson_object_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data);
