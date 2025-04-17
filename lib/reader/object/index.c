#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <xxhash.h>

#include "../../../include/reader.h"

#include "../../common.h"
#include "../../reader.h"
#include "../object.h"
#include "index.h"

bijson_error_t _bijson_analyzed_object_get_index(
	const _bijson_object_analysis_t *analysis,
	size_t index,
	const void **key_buffer_result,
	size_t *key_size_result,
	bijson_t *value_result
) {
	size_t count = analysis->count;
	if(index >= count)
		return bijson_error_index_out_of_range;

	const byte_t *key_index = analysis->key_index;
	size_t key_index_item_size = analysis->key_index_item_size;
	size_t last_key_end_offset = analysis->last_key_end_offset;

	uint64_t raw_key_start_offset = index
		? _bijson_read_minimal_int(key_index + key_index_item_size * (index - SIZE_C(1)), key_index_item_size)
		: UINT64_C(0);
	if(raw_key_start_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t key_start_offset = (size_t)raw_key_start_offset;
	if(key_start_offset > last_key_end_offset)
		return bijson_error_file_format_error;

	size_t count_1 = analysis->count_1;
	uint64_t raw_key_end_offset = index == count_1
		? last_key_end_offset
		: _bijson_read_minimal_int(key_index + key_index_item_size * index, key_index_item_size);
	if(raw_key_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t key_end_offset = (size_t)raw_key_end_offset;
	if(key_end_offset > last_key_end_offset)
		return bijson_error_file_format_error;

	if(key_start_offset > key_end_offset)
		return bijson_error_file_format_error;

	const byte_t *value_index = analysis->value_index;
	size_t value_index_item_size = analysis->value_index_item_size;
	size_t value_data_size = analysis->value_data_size;

	// This is for comparing the raw offsets to, so it doesn't include
	// the implicit "+ index" term yet.
	size_t highest_valid_value_offset = value_data_size - count;

	uint64_t raw_value_start_offset = index
		? _bijson_read_minimal_int(value_index + value_index_item_size * (index - SIZE_C(1)), value_index_item_size)
		: UINT64_C(0);
	if(raw_value_start_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t value_start_offset = (size_t)raw_value_start_offset;
	if(value_start_offset > highest_valid_value_offset)
		return bijson_error_file_format_error;
	value_start_offset += index;

	uint64_t raw_value_end_offset = index == count_1
		? highest_valid_value_offset
		: _bijson_read_minimal_int(value_index + value_index_item_size * index, value_index_item_size);
	if(raw_value_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t value_end_offset = (size_t)raw_value_end_offset;

	if(value_end_offset > highest_valid_value_offset)
		return bijson_error_file_format_error;

	value_end_offset += index + SIZE_C(1);
	if(index == count_1)
		value_end_offset = value_data_size;

	if(value_start_offset >= value_end_offset)
		return bijson_error_file_format_error;

	const byte_t *key_buffer = analysis->key_data_start + key_start_offset;
	size_t key_size = key_end_offset - key_start_offset;

	// We could return an UTF-8 error but it's the file that's at fault here:
	bijson_error_t error = _bijson_check_valid_utf8(key_buffer, key_size);
	if(error == bijson_error_invalid_utf8)
		return bijson_error_file_format_error;
	else if(error)
		return error;

	*key_buffer_result = key_buffer;
	*key_size_result = key_size;
	value_result->buffer = analysis->value_data_start + value_start_offset;
	value_result->size = value_end_offset - value_start_offset;

	return NULL;
}

bijson_error_t bijson_object_get_index(
	const bijson_t *bijson,
	size_t index,
	const void **key_buffer_result,
	size_t *key_size_result,
	bijson_t *value_result
) {
	_bijson_object_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_object_analyze(bijson, &analysis));
	return _bijson_analyzed_object_get_index(
		&analysis,
		index,
		key_buffer_result,
		key_size_result,
		value_result
	);
}

bijson_error_t bijson_analyzed_object_get_index(
	const bijson_object_analysis_t *analysis,
	size_t index,
	const void **key_buffer_result,
	size_t *key_size_result,
	bijson_t *value_result
) {
	return _bijson_analyzed_object_get_index(
		(const _bijson_object_analysis_t *)analysis,
		index,
		key_buffer_result,
		key_size_result,
		value_result
	);
}
