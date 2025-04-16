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

/*
__attribute__((format(printf, 2, 3)))
static void xfprintf(FILE *stream, const char *format, ...) {
	va_list args;
	va_start(args, format);
	if(vfprintf(stream, format, args) < 0)
		abort();
	va_end(args);
}
*/

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

static uint64_t test_index;

static void test_check_valid_utf8(void) {
	uint64_t failures = 0;

	if(_bijson_check_valid_utf8((const byte_t *)"\302\221", 2))
		xprintf("not ok %"PRIu64" - PU1 UTF-8 sequence was not accepted\n", test_index++);
	else
		xprintf("ok %"PRIu64" - PU1 UTF-8 sequence was accepted\n", test_index++);

	xprintf("# Please wait, the next test takes a while.\n");

	// reference: https://stackoverflow.com/questions/6555015/check-for-invalid-utf8
	union {
		uint32_t u;
		byte_t b[sizeof(uint32_t)];
	} x;
	x.u = 0;
	do {
		size_t len = 0;
		if((byte_compute_t)x.b[0] <= BYTE_C(0x7F))
			len = 1;
		else if((byte_compute_t)x.b[0] >= BYTE_C(0xC2) && (byte_compute_t)x.b[0] <= BYTE_C(0xDF) && (byte_compute_t)x.b[1] >= BYTE_C(0x80) && (byte_compute_t)x.b[1] <= BYTE_C(0xBF))
			len = 2;
		else if((byte_compute_t)x.b[0] == BYTE_C(0xE0)                                           && (byte_compute_t)x.b[1] >= BYTE_C(0xA0) && (byte_compute_t)x.b[1] <= BYTE_C(0xBF) && (byte_compute_t)x.b[2] >= BYTE_C(0x80) && (byte_compute_t)x.b[2] <= BYTE_C(0xBF))
			len = 3;
		else if((byte_compute_t)x.b[0] >= BYTE_C(0xE1) && (byte_compute_t)x.b[0] <= BYTE_C(0xEC) && (byte_compute_t)x.b[1] >= BYTE_C(0x80) && (byte_compute_t)x.b[1] <= BYTE_C(0xBF) && (byte_compute_t)x.b[2] >= BYTE_C(0x80) && (byte_compute_t)x.b[2] <= BYTE_C(0xBF))
			len = 3;
		else if((byte_compute_t)x.b[0] == BYTE_C(0xED)                                           && (byte_compute_t)x.b[1] >= BYTE_C(0x80) && (byte_compute_t)x.b[1] <= BYTE_C(0x9F) && (byte_compute_t)x.b[2] >= BYTE_C(0x80) && (byte_compute_t)x.b[2] <= BYTE_C(0xBF))
			len = 3;
		else if((byte_compute_t)x.b[0] >= BYTE_C(0xEE) && (byte_compute_t)x.b[0] <= BYTE_C(0xEF) && (byte_compute_t)x.b[1] >= BYTE_C(0x80) && (byte_compute_t)x.b[1] <= BYTE_C(0xBF) && (byte_compute_t)x.b[2] >= BYTE_C(0x80) && (byte_compute_t)x.b[2] <= BYTE_C(0xBF))
			len = 3;
		else if((byte_compute_t)x.b[0] == BYTE_C(0xF0)                                           && (byte_compute_t)x.b[1] >= BYTE_C(0x90) && (byte_compute_t)x.b[1] <= BYTE_C(0xBF) && (byte_compute_t)x.b[2] >= BYTE_C(0x80) && (byte_compute_t)x.b[2] <= BYTE_C(0xBF) && (byte_compute_t)x.b[3] >= BYTE_C(0x80) && (byte_compute_t)x.b[3] <= BYTE_C(0xBF))
			len = 4;
		else if((byte_compute_t)x.b[0] >= BYTE_C(0xF1) && (byte_compute_t)x.b[0] <= BYTE_C(0xF3) && (byte_compute_t)x.b[1] >= BYTE_C(0x80) && (byte_compute_t)x.b[1] <= BYTE_C(0xBF) && (byte_compute_t)x.b[2] >= BYTE_C(0x80) && (byte_compute_t)x.b[2] <= BYTE_C(0xBF) && (byte_compute_t)x.b[3] >= BYTE_C(0x80) && (byte_compute_t)x.b[3] <= BYTE_C(0xBF))
			len = 4;
		else if((byte_compute_t)x.b[0] == BYTE_C(0xF4)                                           && (byte_compute_t)x.b[1] >= BYTE_C(0x80) && (byte_compute_t)x.b[1] <= BYTE_C(0x8F) && (byte_compute_t)x.b[2] >= BYTE_C(0x80) && (byte_compute_t)x.b[2] <= BYTE_C(0xBF) && (byte_compute_t)x.b[3] >= BYTE_C(0x80) && (byte_compute_t)x.b[3] <= BYTE_C(0xBF))
			len = 4;

		if(len) {
			if(_bijson_check_valid_utf8(x.b, len)) {
				xprintf("not ok %"PRIu64" - %02X %02X %02X %02X (%zu) should not have failed\n", test_index++, (byte_compute_t)x.b[0], (byte_compute_t)x.b[1], (byte_compute_t)x.b[2], (byte_compute_t)x.b[3], len);
				failures++;
			}
		} else for(len = 1; len <= 4; len++) {
			if(!_bijson_check_valid_utf8(x.b, len)) {
				xprintf("not ok %"PRIu64" - %02X %02X %02X %02X (%zu) should have failed\n", test_index++, (byte_compute_t)x.b[0], (byte_compute_t)x.b[1], (byte_compute_t)x.b[2], (byte_compute_t)x.b[3], len);
				failures++;
			}
		}
	} while(x.u++ != UINT32_MAX);

	if(failures)
		xprintf("not ok %"PRIu64" - some UTF-8 tests were invalid\n", test_index++);
	else
		xprintf("ok %"PRIu64" - all UTF-8 tests were valid\n", test_index++);
}

static void test_uint64_str(void) {
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
			xprintf("ok %"PRIu64" - length for %zu (%zu) matches reference length\n", test_index++, number, test_len);
			test[test_len] = '\0';
			if(memcmp(test, ref, test_len))
				xprintf("not ok %"PRIu64" - value for %zu (%s) does not match reference\n", test_index++, number, test);
			else
				xprintf("ok %"PRIu64" - value for %zu matches reference\n", test_index++, number);
		} else {
			xprintf("not ok %"PRIu64" - length for %zu (%zu) does not match reference length (%d)\n", test_index++, number, test_len, ref_len);
		}

		char ref_padded[21];
		int ref_padded_len = xsprintf(ref_padded, "%020"PRIu64, number);
		byte_t test_padded[21] = "AAAAAAAAAAAAAAAAAAAAA";
		size_t test_padded_len = _bijson_uint64_str_padded(test_padded, number);

		if((size_t)ref_padded_len == test_padded_len) {
			xprintf("ok %"PRIu64" - padded length for %zu (%zu) matches reference length\n", test_index++, number, test_padded_len);
			test_padded[test_padded_len] = '\0';
			if(memcmp(test_padded, ref_padded, test_padded_len))
				xprintf("not ok %"PRIu64" - padded value for %zu (%s) does not match reference (%s)\n", test_index++, number, test_padded, ref_padded);
			else
				xprintf("ok %"PRIu64" - padded value for %zu matches reference\n", test_index++, number);
		} else {
			xprintf("not ok %"PRIu64" - padded length for %zu (%zu) does not match reference length\n", test_index++, number, test_padded_len);
		}

		char ref_raw[21] = "";
		int ref_raw_len = number ? xsprintf(ref_raw, "%"PRIu64, number) : 0;
		byte_t test_raw[21] = "AAAAAAAAAAAAAAAAAAAAA";
		size_t test_raw_len = _bijson_uint64_str_raw(test_raw, number);

		if((size_t)ref_raw_len == test_raw_len) {
			xprintf("ok %"PRIu64" - raw length for %zu (%zu) matches reference length\n", test_index++, number, test_raw_len);
			test_raw[test_raw_len] = '\0';
			if(memcmp(test_raw, ref_raw, test_raw_len))
				xprintf("not ok %"PRIu64" - raw value for %zu (%s) does not match reference (%s)\n", test_index++, number, test_raw, ref_raw);
			else
				xprintf("ok %"PRIu64" - raw value for %zu matches reference\n", test_index++, number);
		} else {
			xprintf("not ok %"PRIu64" - raw length for %zu (%zu) does not match reference length\n", test_index++, number, test_raw_len);
		}
	}
}

int main(void) {
	test_check_valid_utf8();
	test_uint64_str();

	xprintf("1..%"PRIu64"\n", test_index);

	return 0;
}
