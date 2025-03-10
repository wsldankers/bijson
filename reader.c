#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "reader.h"
#include "common.h"

static bool _bijson_object_to_json(const bijson_t *bijson, bijson_output_callback callback, void *userdata) {
    return false;
}

static bool _bijson_array_to_json(const bijson_t *bijson, bijson_output_callback callback, void *userdata) {
    return false;
}

static const unsigned char hex[16] = "0123456789ABCDEF";

static bool _bijson_string_to_json(const bijson_t *bijson, bijson_output_callback callback, void *userdata) {
    const uint8_t *string = bijson->buffer + 1;
    size_t len = bijson->size - SIZE_C(1);
    if(!_bijson_is_valid_utf8((const char *)string, len))
        return false;
    const uint8_t *previously_written = string;
    uint8_t escape[2] = "\\x";
    uint8_t unicode_escape[6] = "\\u00XX";
    if(!callback("\"", 1, userdata))
        return false;
    const uint8_t *string_end = string + len;
    while(string < string_end) {
        const uint8_t *string_pos = string;
        uint8_t c = *string++;
        uint8_t plain_escape = 0;
        switch(c) {
            case '"':
                plain_escape = '"';
                break;
            case '\\':
                plain_escape = '\\';
                break;
            case '\b':
                plain_escape = 'b';
                break;
            case '\f':
                plain_escape = 'f';
                break;
            case '\n':
                plain_escape = 'n';
                break;
            case '\r':
                plain_escape = 'r';
                break;
            case '\t':
                plain_escape = 't';
                break;
            default:
                if(c >= UINT8_C(32))
                    continue;
        }
        if(string_pos > previously_written)
            if(!callback(previously_written, string_pos - previously_written, userdata))
                return false;
        previously_written = string;
        if(plain_escape) {
            escape[1] = plain_escape;
            if(!callback(escape, sizeof escape, userdata))
                return false;
        } else {
            unicode_escape[4] = hex[c >> 4];
            unicode_escape[5] = hex[c & UINT8_C(0xF)];
            if(!callback(unicode_escape, sizeof unicode_escape, userdata))
                return false;
        }
    }
    if(string > previously_written)
        if(!callback(previously_written, string - previously_written, userdata))
            return false;
    if(!callback("\"", 1, userdata))
        return false;
    return true;
}

bool bijson_to_json(const bijson_t *bijson, bijson_output_callback callback, void *userdata) {
    const uint8_t *buffer = bijson->buffer;
    if(!buffer || !bijson->size)
        return false;

    const uint8_t type = *buffer;
    // fprintf(stderr, "%02X\n", type);

    if((type & UINT8_C(0xC0)) == UINT8_C(0x40))
        return _bijson_object_to_json(bijson, callback, userdata);
    if((type & UINT8_C(0xF0)) == UINT8_C(0x30))
        return _bijson_array_to_json(bijson, callback, userdata);
    if(type == UINT8_C(0x08))
        return _bijson_string_to_json(bijson, callback, userdata);

    return false;
}

static bool _bijson_stdio_output_callback(const void *data, size_t len, void *userdata) {
    return fwrite(data, sizeof *data, len, userdata) == len * sizeof *data;
}

int main(void) {
    uint8_t buffer[] = "\010f\n\010\0\001oあA\\A\"A";
    bijson_t b = {buffer, sizeof buffer - 1};
    if(!bijson_to_json(&b, _bijson_stdio_output_callback, stdout))
        return 2;
    putchar('\n');
    return 0;
}
