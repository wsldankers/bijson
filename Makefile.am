bin_PROGRAMS = bin/bijson bin/mkbijson
bin_bijson_SOURCES = src/bijson.c
bin_bijson_LDADD = lib/libbijson.la
bin_mkbijson_SOURCES = src/mkbijson.c
bin_mkbijson_LDADD = lib/libbijson.la

noinst_PROGRAMS = tests/bijson
tests_bijson_SOURCES = tests/bijson.c
tests_bijson_LDADD = lib/libbijson.la

LOG_DRIVER = AM_TAP_AWK='$(AWK)' $(top_srcdir)/tap-driver.sh
TESTS = tests/wrapper

lib_LTLIBRARIES = lib/libbijson.la
lib_libbijson_la_SOURCES = src/hashtable.c src/hashtable.h src/layout.h src/maker.c src/maker.h src/reader.c src/reader.h src/murmur3.c src/murmur3.h src/readerimpl.h
lib_libbijson_la_LDFLAGS = -Wl,--version-script,$(srcdir)/libbijson.ver
lib_libbijson_la_LIBADD = -lrt

if !HAVE_QSORT_R
lib_libbijson_la_SOURCES += src/qsort_r.c
endif

hh_HEADERS = src/layout.h src/reader.h src/hashtable.h src/maker.h
hhdir = $(pkgincludedir)

EXTRA_DIST = libbijson.ver
