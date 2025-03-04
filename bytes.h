#pragma once

#include <stdbool.h>
#include <stdlib.h>

#include "writer.h"

extern bool bijson_writer_add_bytes(bijson_writer_t *writer, const void *bytes, size_t len);
