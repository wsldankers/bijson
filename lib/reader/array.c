#include <string.h>

#include "../../include/reader.h"

#include "../common.h"
#include "../reader.h"
#include "array.h"

typedef struct _bijson_array_analysis {
	size_t count;
	size_t count_1;
	size_t index_item_size;
	size_t highest_valid_offset;
	const byte_t *item_index;
	const byte_t *item_data_start;
} _bijson_array_analysis_t;

static inline bijson_error_t _bijson_array_analyze_count(const bijson_t *bijson, _bijson_array_analysis_t *analysis) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));
	if(!analysis)
		return bijson_error_parameter_is_null;

	IF_DEBUG(memset(analysis, 'A', sizeof *analysis));

	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;

	byte_compute_t type = *buffer;
	if((type & BYTE_C(0xF0)) != BYTE_C(0x30))
		return bijson_error_type_mismatch;

	const byte_t *count_location = buffer + SIZE_C(1);
	if(count_location == buffer_end) {
		analysis->count = SIZE_C(0);
		return NULL;
	}

	size_t count_size = SIZE_C(1) << (type & BYTE_C(0x3));
	const byte_t *item_index = count_location + count_size;
	if(item_index > buffer_end)
		return bijson_error_file_format_error;
	uint64_t raw_count = _bijson_read_minimal_int(count_location, count_size);
	if(raw_count > SIZE_MAX - SIZE_C(1))
		return bijson_error_file_format_error;
	size_t count_1 = (size_t)raw_count;

	analysis->count = count_1 + SIZE_C(1);
	analysis->count_1 = count_1;
	analysis->item_index = item_index;

	return NULL;
}

static inline bijson_error_t _bijson_array_count(const bijson_t *bijson, size_t *result) {
	_bijson_array_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_array_analyze_count(bijson, &analysis));
	*result = analysis.count;
	return NULL;
}

bijson_error_t bijson_array_count(const bijson_t *bijson, size_t *result) {
	return _bijson_array_count(bijson, result);
}

bijson_error_t bijson_analyzed_array_count(const bijson_array_analysis_t *analysis, size_t *result) {
	*result = ((const _bijson_array_analysis_t *)analysis)->count;
	return NULL;
}

static inline bijson_error_t _bijson_array_analyze(const bijson_t *bijson, _bijson_array_analysis_t *analysis) {
	_BIJSON_ERROR_RETURN(_bijson_array_analyze_count(bijson, analysis));
	if(!analysis->count)
		return NULL;

	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;

	byte_compute_t type = *buffer;

	const byte_t *item_index = analysis->item_index;
	size_t index_and_data_size = _bijson_ptrdiff(buffer_end, item_index);

	size_t index_item_size = SIZE_C(1) << ((type >> 2U) & BYTE_C(0x3));
	// We need at least one index_item and one type byte for each item,
	// but the first item does not have an index entry, so fake that.
	size_t count = analysis->count;
	if(count > (index_and_data_size + index_item_size) / (index_item_size + SIZE_C(1)))
		return bijson_error_file_format_error;

	const byte_t *item_data_start = item_index + (count - SIZE_C(1)) * index_item_size;
	size_t item_data_size = _bijson_ptrdiff(buffer_end, item_data_start);

	analysis->index_item_size = index_item_size;
	analysis->item_data_start = item_data_start;
	// This is for comparing the raw offsets to, so it doesn't include
	// the implicit "+ index" term yet.
	analysis->highest_valid_offset = item_data_size - count;

	return NULL;
}

static inline bijson_error_t _bijson_analyzed_array_get_index(const _bijson_array_analysis_t *analysis, size_t index, bijson_t *result) {
	if(!analysis)
		return bijson_error_parameter_is_null;

	if(index >= analysis->count)
		return bijson_error_index_out_of_range;

	uint64_t raw_start_offset = index
		? _bijson_read_minimal_int(analysis->item_index + analysis->index_item_size * (index - SIZE_C(1)), analysis->index_item_size)
		: UINT64_C(0);
	if(raw_start_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t start_offset = (size_t)raw_start_offset;
	if(start_offset > analysis->highest_valid_offset)
		return bijson_error_file_format_error;
	start_offset += index;

	uint64_t raw_end_offset = index == analysis->count_1
		? analysis->highest_valid_offset
		: _bijson_read_minimal_int(analysis->item_index + analysis->index_item_size * index, analysis->index_item_size);
	if(raw_end_offset > SIZE_MAX)
		return bijson_error_file_format_error;
	size_t end_offset = (size_t)raw_end_offset;
	if(end_offset > analysis->highest_valid_offset)
		return bijson_error_file_format_error;
	end_offset += index + SIZE_C(1);

	if(start_offset >= end_offset)
		return bijson_error_file_format_error;

	result->buffer = analysis->item_data_start + start_offset;
	result->size = end_offset - start_offset;

	return NULL;
}

bijson_error_t bijson_array_get_index(const bijson_t *bijson, size_t index, bijson_t *result) {
	_bijson_array_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_array_analyze(bijson, &analysis));
	return _bijson_analyzed_array_get_index(&analysis, index, result);
}

bijson_error_t bijson_analyzed_array_get_index(const bijson_array_analysis_t *analysis, size_t index, bijson_t *result) {
	return _bijson_analyzed_array_get_index((const _bijson_array_analysis_t *)analysis, index, result);
}

bijson_error_t bijson_array_analyze(const bijson_t *bijson, bijson_array_analysis_t *result) {
	assert(sizeof(_bijson_array_analysis_t) <= sizeof(*result));
	return _bijson_array_analyze(bijson, (_bijson_array_analysis_t *)result);
}

bijson_error_t _bijson_array_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	_bijson_array_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_array_analyze(bijson, &analysis));

	_BIJSON_ERROR_RETURN(callback(callback_data, "[", 1));

	for(size_t u = 0; u < analysis.count; u++) {
		if(u)
			_BIJSON_ERROR_RETURN(callback(callback_data, ",", 1));

		bijson_t item;
		_BIJSON_ERROR_RETURN(_bijson_analyzed_array_get_index(&analysis, u, &item));
		_BIJSON_ERROR_RETURN(bijson_to_json(&item, callback, callback_data));
	}

	return callback(callback_data, "]", 1);
}
