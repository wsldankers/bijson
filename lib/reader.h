#pragma once

#include "../include/reader.h"

#include "common.h"

static inline bijson_error_t _bijson_check_bijson(const bijson_t *bijson) {
	if(!bijson || !bijson->buffer)
		_BIJSON_RETURN_ERROR(bijson_error_parameter_is_null);
	if(!bijson->size)
		_BIJSON_RETURN_ERROR(bijson_error_parameter_is_zero);
	return NULL;
}

static inline uint64_t _bijson_read_minimal_int(const byte_t *buffer, size_t nbytes) {
	uint64_t r = 0;
	for(size_t u = 0; u < nbytes; u++)
		r |= (uint64_t)buffer[u] << (u * SIZE_C(8));
	return r;
}
