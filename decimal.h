#pragma once

#include "writer.h"

#include <stdbool.h>
#include <stdlib.h>

extern bool bijson_writer_add_decimal_from_string(bijson_writer_t *writer, const char *string, size_t len);
