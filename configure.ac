AC_PREREQ(2.59)
AC_INIT(bijson, m4_esyscmd_s([
	read -r debian_version <debian/changelog
	debian_version=${debian_version#*\(}
	debian_version=${debian_version%%\)*}
	echo "${debian_version%-*}"
]), [wsl-bijson-bugs@fruit.je])
AM_INIT_AUTOMAKE([foreign subdir-objects dist-xz no-dist-gzip])

# libtool is annoyingly verbose without this:
AM_SILENT_RULES([yes])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([lib/io.c])
AC_CONFIG_HEADERS([config.h])

AC_DEFINE(_GNU_SOURCE, 1, [needed for some calls])
AC_DEFINE(_FILE_OFFSET_BITS, 64, [gnu libc has inconvenient defaults])
AC_DEFINE(_XOPEN_SOURCE, 500, [needed for some calls])

# Checks for typedefs, structures, and compiler characteristics.
AC_PROG_CC
# This adds -std=gnu99 which we do not want:
#AC_PROG_CC_C99
AC_PROG_INSTALL
AC_SYS_LARGEFILE

AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_FUNC_MALLOC
AC_FUNC_MMAP

# Checks for header files.
AC_CHECK_HEADERS([stdint.h inttypes.h stdlib.h unistd.h fcntl.h], , [
	AC_MSG_FAILURE([Required system header files not found.])
	exit 1
])

MY_CHECK_BUILTIN(clzll, 0)
MY_CHECK_BUILTIN(expect, [0, 0])

LT_INIT

MY_REMOVE_FLAG([-O2])
AX_APPEND_FLAG([-O3])
AX_APPEND_FLAG([-std=c99])
AX_APPEND_FLAG([-pipe])
AX_APPEND_FLAG([-Wall])
AX_APPEND_FLAG([-Wextra])
AX_APPEND_FLAG([-Wno-missing-field-initializers])
AX_APPEND_FLAG([-Wno-unused-parameter])
AX_APPEND_FLAG([-Wstrict-aliasing])
AX_APPEND_FLAG([-include config.h])
AX_APPEND_FLAG([-flto=auto])
AX_APPEND_FLAG([-flto=auto],[LDFLAGS])

AC_REQUIRE_AUX_FILE([tap-driver.sh])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
