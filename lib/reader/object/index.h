#pragma once

#include "../../common.h"
#include "../object.h"

extern bijson_error_t _bijson_analyzed_object_get_index(
	const _bijson_object_analysis_t *analysis,
	size_t index,
	const void **key_buffer_result,
	size_t *key_size_result,
	bijson_t *value_result
);
