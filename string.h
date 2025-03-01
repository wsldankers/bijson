#pragma once

#include <stdbool.h>
#include <stdlib.h>

#include "writer.h"

extern bool bijson_writer_add_string(bijson_writer_t *writer, const char *string, size_t len);
