#pragma once

#include <stdbool.h>
#include <stdlib.h>

#include "writer.h"

extern bool bijson_writer_add_null(bijson_writer_t *writer);
extern bool bijson_writer_add_false(bijson_writer_t *writer);
extern bool bijson_writer_add_true(bijson_writer_t *writer);
