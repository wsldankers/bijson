#pragma once

#include <assert.h>
#include <stdlib.h>

void *xalloc(size_t len);
void xfree(void *buf);

static inline size_t _bijson_size_min(size_t a, size_t b) {
	return a < b ? a : b;
}

static inline size_t _bijson_size_max(size_t a, size_t b) {
	return a > b ? a : b;
}

static inline size_t _bijson_size_clamp(size_t min, size_t x, size_t max) {
	assert(min <= max);
	return x < min ? min : x > max ? max : x;
}

#if __SIZEOF_SIZE_T__ == 8
#define SIZE_C(x) UINT64_C(x)
#elif __SIZEOF_SIZE_T__ == 4
#define SIZE_C(x) UINT32_C(x)
#else
#define SIZE_C(x) ((size_t)(x))
#endif
