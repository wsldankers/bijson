#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "common.h"

static inline byte_compute_t _bijson_optimal_storage_size(uint64_t len) {
	if(len > UINT64_C(65535))
		return len > UINT64_C(4294967295) ? BYTE_C(3) : BYTE_C(2);
	else
		return len > UINT64_C(255) ? BYTE_C(1) : BYTE_C(0);
}

static inline byte_compute_t _bijson_optimal_storage_size1(uint64_t len) {
	if(len > UINT64_C(65536))
		return len > UINT64_C(4294967296) ? BYTE_C(3) : BYTE_C(2);
	else
		return len > UINT64_C(256) ? BYTE_C(1) : BYTE_C(0);
}

static inline size_t _bijson_optimal_storage_size_bytes(byte_compute_t storage_size) {
	return (size_t)1 << storage_size;
}

static inline size_t _bijson_fit_uint64(uint64_t value) {
	return value > UINT64_C(0xFFFFFFFFF)
		? value > UINT64_C(0xFFFFFFFFFFFFF)
			? value > UINT64_C(0xFFFFFFFFFFFFFFF)
				? 8
				: 7
			: value > UINT64_C(0xFFFFFFFFFF)
				? 6
				: 5
		: value > UINT64_C(0xFFFF)
			? value > UINT64_C(0xFFFFFF)
				? 4
				: 3
			: value > UINT64_C(0xFF)
				? 2
				: 1;
}
