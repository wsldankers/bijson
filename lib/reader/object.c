#include <string.h>

#include "../../include/reader.h"

#include "../common.h"
#include "../reader.h"
#include "string.h"
#include "object/index.h"
#include "object.h"

static inline bijson_error_t _bijson_object_analyze_count(const bijson_t *bijson, _bijson_object_analysis_t *analysis) {
	_BIJSON_ERROR_RETURN(_bijson_check_bijson(bijson));

	IF_DEBUG(memset(analysis, 'A', sizeof *analysis));

	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;

	byte_compute_t type = *buffer;
	if((type & BYTE_C(0xC0)) != BYTE_C(0x40))
		_BIJSON_RETURN_ERROR(bijson_error_type_mismatch);

	const byte_t *count_location = buffer + SIZE_C(1);
	if(count_location == buffer_end) {
		analysis->count = SIZE_C(0);
		return NULL;
	}

	size_t count_size = SIZE_C(1) << (type & BYTE_C(0x3));
	const byte_t *key_index = count_location + count_size;
	if(key_index > buffer_end)
		_BIJSON_RETURN_ERROR(bijson_error_file_format_error);
	uint64_t raw_count = _bijson_read_minimal_int(count_location, count_size);
	if(raw_count > SIZE_MAX - SIZE_C(1))
		_BIJSON_RETURN_ERROR(bijson_error_file_format_error);
	size_t count_1 = (size_t)raw_count;

	analysis->count = count_1 + SIZE_C(1);
	analysis->count_1 = count_1;
	analysis->key_index = key_index;

	return NULL;
}

static inline bijson_error_t _bijson_object_count(const bijson_t *bijson, size_t *result) {
	_bijson_object_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_object_analyze_count(bijson, &analysis));
	*result = analysis.count;
	return NULL;
}

bijson_error_t bijson_object_count(const bijson_t *bijson, size_t *result) {
	return _bijson_object_count(bijson, result);
}

bijson_error_t bijson_analyzed_object_count(const bijson_object_analysis_t *analysis, size_t *result) {
	*result = ((const _bijson_object_analysis_t *)analysis)->count;
	return NULL;
}

bijson_error_t _bijson_object_analyze(const bijson_t *bijson, _bijson_object_analysis_t *analysis) {
	_BIJSON_ERROR_RETURN(_bijson_object_analyze_count(bijson, analysis));
	if(!analysis->count)
		return NULL;

	const byte_t *buffer = bijson->buffer;
	const byte_t *buffer_end = buffer + bijson->size;
	byte_compute_t type = *buffer;

	size_t count = analysis->count;
	size_t count_1 = analysis->count_1;
	size_t index_and_data_size = _bijson_ptrdiff(buffer_end, analysis->key_index);
	size_t key_index_item_size = SIZE_C(1) << ((type >> 2U) & BYTE_C(0x3));
	size_t value_index_item_size = SIZE_C(1) << ((type >> 4U) & BYTE_C(0x3));
	// We need at least one key_index_item, one value_index_item, and one
	// type byte for each item, but the first item does not have a value
	// index entry, so fake that.
	if(count > (index_and_data_size + value_index_item_size)
		/ (key_index_item_size + value_index_item_size + SIZE_C(1))
	)
		_BIJSON_RETURN_ERROR(bijson_error_file_format_error);

	const byte_t *value_index = analysis->key_index + count * key_index_item_size;
	const byte_t *key_data_start = value_index + count_1 * value_index_item_size;

	uint64_t raw_last_key_end_offset =
		_bijson_read_minimal_int(analysis->key_index + key_index_item_size * count_1, key_index_item_size);
	if(raw_last_key_end_offset > SIZE_MAX)
		_BIJSON_RETURN_ERROR(bijson_error_file_format_error);
	size_t last_key_end_offset = (size_t)raw_last_key_end_offset;

	if(last_key_end_offset > _bijson_ptrdiff(buffer_end, key_data_start) - count)
		_BIJSON_RETURN_ERROR(bijson_error_file_format_error);

	const byte_t *value_data_start = key_data_start + last_key_end_offset;

	analysis->key_index_item_size = key_index_item_size;
	analysis->last_key_end_offset = last_key_end_offset;
	analysis->key_data_start = key_data_start;
	analysis->value_index = value_index;
	analysis->value_index_item_size = value_index_item_size;
	analysis->value_data_start = value_data_start;
	analysis->value_data_size = _bijson_ptrdiff(buffer_end, value_data_start);

	return NULL;
}

bijson_error_t bijson_object_analyze(const bijson_t *bijson, bijson_object_analysis_t *result) {
	assert(sizeof(_bijson_object_analysis_t) <= sizeof(*result));
	return _bijson_object_analyze(bijson, (_bijson_object_analysis_t *)result);
}

bijson_error_t _bijson_object_to_json(const bijson_t *bijson, bijson_output_callback_t callback, void *callback_data) {
	_bijson_object_analysis_t analysis;
	_BIJSON_ERROR_RETURN(_bijson_object_analyze(bijson, &analysis));

	_BIJSON_ERROR_RETURN(callback(callback_data, "{", 1));

	for(size_t u = 0; u < analysis.count; u++) {
		if(u)
			_BIJSON_ERROR_RETURN(callback(callback_data, ",", 1));

		bijson_t key, value;
		_BIJSON_ERROR_RETURN(_bijson_analyzed_object_get_index(&analysis, u, &key.buffer, &key.size, &value));
		_BIJSON_ERROR_RETURN(_bijson_raw_string_to_json(&key, callback, callback_data));
		_BIJSON_ERROR_RETURN(callback(callback_data, ":", 1));
		_BIJSON_ERROR_RETURN(bijson_to_json(&value, callback, callback_data));
	}

	return callback(callback_data, "}", 1);
}
