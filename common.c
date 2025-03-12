#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>
#include <err.h>

#include "common.h"

bijson_error_t _bijson_check_valid_utf8(const char *string, size_t len) {
	if(!string)
		return len == 0 ? NULL : bijson_error_parameter_is_null;

	const uint8_t *s = (const uint8_t *)string;
	const uint8_t *end = s + len;

	while(s != end) {
		uint8_t c = *s++;
		if(!(c & 0x80))
			continue; // short-circuit common case

		if((c & 0xE0) == 0xC0) { // 0b110.....
			// Basic characters can encode 7 bits.
			// Size 2 sequences can encode 5 + 6 = 11 bits.
			if(!(c & 0x1E))
				return bijson_error_invalid_utf8; // overlong sequence
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			if((*s++ & 0xC0) != 0x80)
				return bijson_error_invalid_utf8; // not a continuation byte
		} else if((c & 0xF0) == 0xE0) { // 0b1110....
			// Size 3 sequences can encode 5 + 6 + 6 = 17 bits.
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			uint8_t c2 = *s++;
			if((c2 & 0xC0) != 0x80)
				return bijson_error_invalid_utf8; // not a continuation byte
			if(!(c & 0x0F) && !(c2 & 0x30))
				return bijson_error_invalid_utf8; // overlong sequence
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			if((*s++ & 0xC0) != 0x80)
				return bijson_error_invalid_utf8; // not a continuation byte
		} else if((c & 0xF8) == 0xF0) { // 0b11110...
			// Size 4 sequences can encode 5 + 6 + 6 + 6 = 23 bits.
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			uint8_t c2 = *s++;
			if((c2 & 0xC0) != 0x80)
				return bijson_error_invalid_utf8; // not a continuation byte
			if(!(c & 0x07) && !(c2 & 0x38))
				return bijson_error_invalid_utf8; // overlong sequence
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			if((*s++ & 0xC0) != 0x80)
				return bijson_error_invalid_utf8; // not a continuation byte
			if(s == end)
				return bijson_error_invalid_utf8; // premature end
			if((*s++ & 0xC0) != 0x80)
				return bijson_error_invalid_utf8; // not a continuation byte
		} else {
			return bijson_error_invalid_utf8; // invalid byte
		}
	}

	return NULL;
}
