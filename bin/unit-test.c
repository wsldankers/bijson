#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../lib/common.h"

__attribute__((format(printf, 1, 2)))
static void xprintf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	if(vprintf(format, args) < 0)
		abort();
	va_end(args);
}

__attribute__((format(printf, 2, 3)))
static int xsprintf(char *dst, const char *format, ...) {
	va_list args;
	va_start(args, format);
	int r = vsprintf(dst, format, args);
	if(r < 0)
		abort();
	va_end(args);
	return r;
}

int main(void) {
	size_t test_index = 0;

	static uint64_t numbers[] = {
		UINT64_C(0),
		UINT64_C(1),
		UINT64_C(2),
		UINT64_C(3),
		UINT64_C(4),
		UINT64_C(5),
		UINT64_C(9),
		UINT64_C(10),
		UINT64_C(42),
		UINT64_C(99),
		UINT64_C(100),
		UINT64_C(999),
		UINT64_C(1000),
		UINT64_C(9999),
		UINT64_C(10000),
		UINT64_C(99999),
		UINT64_C(100000),
		UINT64_C(999999),
		UINT64_C(1000000),
		UINT64_C(9999999),
		UINT64_C(10000000),
		UINT64_C(99999999),
		UINT64_C(100000000),
		UINT64_C(999999999),
		UINT64_C(1000000000),
		UINT64_C(4294967295),
		UINT64_C(4294967296),
		UINT64_C(9999999999),
		UINT64_C(10000000000),
		UINT64_C(99999999999),
		UINT64_C(100000000000),
		UINT64_C(999999999999),
		UINT64_C(1000000000000),
		UINT64_C(9999999999999),
		UINT64_C(10000000000000),
		UINT64_C(99999999999999),
		UINT64_C(100000000000000),
		UINT64_C(999999999999999),
		UINT64_C(1000000000000000),
		UINT64_C(9999999999999999),
		UINT64_C(10000000000000000),
		UINT64_C(99999999999999999),
		UINT64_C(100000000000000000),
		UINT64_C(999999999999999999),
		UINT64_C(1000000000000000000),
		UINT64_C(9999999999999999999),
		UINT64_C(10000000000000000000),
		UINT64_C(18446744073709551615)
	};

	for(size_t u = 0; u < _BIJSON_ARRAY_COUNT(numbers); u++) {
		uint64_t number = numbers[u];

		char ref[21];
		int ref_len = xsprintf(ref, "%"PRIu64, number);
		byte_t test[21] = "AAAAAAAAAAAAAAAAAAAAA";
		size_t test_len = _bijson_uint64_str(test, number);

		if((size_t)ref_len == test_len) {
			xprintf("ok %zu - length for %zu (%zu) matches reference length\n", test_index++, number, test_len);
			test[test_len] = '\0';
			if(memcmp(test, ref, test_len))
				xprintf("not ok %zu - value for %zu (%s) does not match reference\n", test_index++, number, test);
			else
				xprintf("ok %zu - value for %zu matches reference\n", test_index++, number);
		} else {
			xprintf("not ok %zu - length for %zu (%zu) does not match reference length (%d)\n", test_index++, number, test_len, ref_len);
		}

		char ref_padded[21];
		int ref_padded_len = xsprintf(ref_padded, "%020"PRIu64, number);
		byte_t test_padded[21] = "AAAAAAAAAAAAAAAAAAAAA";
		size_t test_padded_len = _bijson_uint64_str_padded(test_padded, number);

		if((size_t)ref_padded_len == test_padded_len) {
			xprintf("ok %zu - padded length for %zu (%zu) matches reference length\n", test_index++, number, test_padded_len);
			test_padded[test_padded_len] = '\0';
			if(memcmp(test_padded, ref_padded, test_padded_len))
				xprintf("not ok %zu - padded value for %zu (%s) does not match reference (%s)\n", test_index++, number, test_padded, ref_padded);
			else
				xprintf("ok %zu - padded value for %zu matches reference\n", test_index++, number);
		} else {
			xprintf("not ok %zu - padded length for %zu (%zu) does not match reference length\n", test_index++, number, test_padded_len);
		}

		char ref_raw[21] = "";
		int ref_raw_len = number ? xsprintf(ref_raw, "%"PRIu64, number) : 0;
		byte_t test_raw[21] = "AAAAAAAAAAAAAAAAAAAAA";
		size_t test_raw_len = _bijson_uint64_str_raw(test_raw, number);

		if((size_t)ref_raw_len == test_raw_len) {
			xprintf("ok %zu - raw length for %zu (%zu) matches reference length\n", test_index++, number, test_raw_len);
			test_raw[test_raw_len] = '\0';
			if(memcmp(test_raw, ref_raw, test_raw_len))
				xprintf("not ok %zu - raw value for %zu (%s) does not match reference (%s)\n", test_index++, number, test_raw, ref_raw);
			else
				xprintf("ok %zu - raw value for %zu matches reference\n", test_index++, number);
		} else {
			xprintf("not ok %zu - raw length for %zu (%zu) does not match reference length\n", test_index++, number, test_raw_len);
		}
	}

	xprintf("1..%zu\n", test_index);

	return 0;
}
