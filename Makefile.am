bin_PROGRAMS = bin/bijson
bin_bijson_SOURCES = bin/bijson.c
bin_bijson_LDADD = lib/libbijson.la

noinst_PROGRAMS = tests/bijson
tests_bijson_SOURCES = tests/bijson.c
tests_bijson_LDADD = lib/libbijson.la

LOG_DRIVER = AM_TAP_AWK='$(AWK)' $(top_srcdir)/tap-driver.sh
TESTS = tests/wrapper

lib_LTLIBRARIES = lib/libbijson.la
lib_libbijson_la_SOURCES = lib/common.c lib/error.c lib/io.c lib/reader.c lib/reader/parse.c lib/writer/buffer.c lib/writer/bytes.c lib/writer.c lib/writer/constants.c lib/writer/container.c lib/writer/decimal.c lib/writer/string.c
lib_libbijson_la_LDFLAGS = -Wl,--version-script,$(srcdir)/libbijson.ver
lib_libbijson_la_LIBADD = -lxxhash

includefiles_HEADERS = include/common.h include/reader.h include/writer.h
includefilesdir = $(pkgincludedir)

EXTRA_DIST = bijson.ver
