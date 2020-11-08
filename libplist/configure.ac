#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.64)
AC_INIT([libplist], [2.2.1], [https://github.com/libimobiledevice/libplist/issues],, [https://libimobiledevice.org])
AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip check-news])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

dnl libtool versioning
# +1 : 0 : +1  == adds new functions to the interface
# +1 : 0 : 0   == changes or removes functions (changes include both
#                 changes to the signature and the semantic)
#  ? :+1 : ?   == just internal changes
# CURRENT : REVISION : AGE
LIBPLIST_SO_VERSION=6:0:3

AC_SUBST(LIBPLIST_SO_VERSION)

# prefer clang if it is available and CC is not set
if test -z "$CC" && test -z "$CXX" && test -x "`which clang`"; then
    CC=clang
    CXX=clang++
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# AC_PROG_CXX will return "g++" even if no c++ compiler is installed
AC_LANG_PUSH([C++])
AC_MSG_CHECKING([whether $CXX is available and compiles a program])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
                  [AC_MSG_RESULT([yes])],
                  [AC_MSG_RESULT([no])
                   AC_MSG_ERROR([C++ compiler not available or unable to compile])])
AC_LANG_POP

AM_PROG_CC_C_O
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([asprintf strcasecmp strdup strerror strndup stpcpy vasprintf gmtime_r localtime_r timegm strptime memmem])

# Checking endianness
AC_C_BIGENDIAN([AC_DEFINE([__BIG_ENDIAN__], [1], [big endian])],
               [AC_DEFINE([__LITTLE_ENDIAN__], [1], [little endian])])


# Check for operating system
AC_MSG_CHECKING([wether we need platform-specific build settings for ${host_os}])
case ${host_os} in
  *mingw32*|*cygwin*)
    AC_MSG_RESULT([yes])
    win32=true
    ;;
  darwin*|*android*)
    AC_MSG_RESULT([no])
    ;;
  *)
    AC_MSG_RESULT([yes])
    AX_PTHREAD([], [AC_MSG_ERROR([pthread is required to build libplist])])
    AC_CHECK_LIB(pthread, [pthread_once], [], [AC_MSG_ERROR([pthread with pthread_once required to build libplist])])
    ;;
esac
AM_CONDITIONAL(WIN32, test x$win32 = xtrue)

# Check if we need libm for fmin
CACHED_CFLAGS="$CFLAGS"
CFLAGS="-O0"
AC_CACHE_CHECK(if fmin is a builtin function, ac_cv_fmin_builtin,
	AC_TRY_LINK([
		#include <math.h>
		#include <float.h>
	], [
		double val = 3.1415f * 0.55555f;
		double diff = fmin(val, DBL_MAX);
		if (diff > 0) return 1;
	], ac_cv_fmin_builtin=yes, ac_cv_fmin_builtin=no))
CFLAGS="$CACHED_CFLAGS"

# Check if struct tm has a tm_gmtoff member
AC_CACHE_CHECK(for tm_gmtoff in struct tm, ac_cv_struct_tm_gmtoff,
	AC_TRY_COMPILE([
		#include <time.h>
	], [
		struct tm tm;
		tm.tm_gmtoff = 1;
	], ac_cv_struct_tm_gmtoff=yes, ac_cv_struct_tm_gmtoff=no))

if (test "$ac_cv_struct_tm_gmtoff" = "yes"); then
	AC_DEFINE(HAVE_TM_TM_GMTOFF, 1, [Define if struct tm has a tm_gmtoff member])
fi

# Check if struct tm has a tm_zone member
AC_CACHE_CHECK(for tm_zone in struct tm, ac_cv_struct_tm_zone,
	AC_TRY_COMPILE([
		#include <time.h>
	], [
		struct tm tm;
		tm.tm_zone = 1;
	], ac_cv_struct_tm_zone=yes, ac_cv_struct_tm_zone=no))

if (test "$ac_cv_struct_tm_zone" = "yes"); then
	AC_DEFINE(HAVE_TM_TM_ZONE, 1, [Define if struct tm has a tm_zone member])
fi

# Cython Python Bindings
AC_ARG_WITH([cython],
            [AS_HELP_STRING([--without-cython],
            [build Python bindings using Cython (default is yes)])],
            [build_cython=false],
            [build_cython=true])
if test "$build_cython" = "true"; then
            AM_PATH_PYTHON(2.3)
            AC_PROG_CYTHON(0.17.0)
            CYTHON_PYTHON
else
            CYTHON=false
fi
if [test "x$CYTHON" != "xfalse"]; then
            PKG_PROG_PKG_CONFIG
            AC_MSG_CHECKING([for libplist Cython bindings])
            CYTHON_PLIST_INCLUDE_DIR=$($PKG_CONFIG --variable=includedir libplist-2.0)/plist/cython
            if [test ! -d "$CYTHON_PLIST_INCLUDE_DIR"]; then
                CYTHON_PLIST_INCLUDE_DIR=.
                cython_python_bindings=yes
                AC_MSG_RESULT([Using built-in libplist Cython bindings (assuming this is a first build)])
            else
                AC_SUBST([CYTHON_PLIST_INCLUDE_DIR])
                AC_MSG_RESULT([$CYTHON_PLIST_INCLUDE_DIR])
                cython_python_bindings=yes
            fi
else
            cython_python_bindings=no
fi
AM_CONDITIONAL([HAVE_CYTHON],[test "x$cython_python_bindings" = "xyes"])

AS_COMPILER_FLAGS(GLOBAL_CFLAGS, "-Wall -Wextra -Wredundant-decls -Wshadow -Wpointer-arith  -Wwrite-strings -Wswitch-default -Wno-unused-parameter -Wno-strict-aliasing -fvisibility=hidden $PTHREAD_CFLAGS")
GLOBAL_LDFLAGS="$PTHREAD_LIBS"

if test "x$ac_cv_fmin_builtin" != "xyes"; then
	GLOBAL_LDFLAGS+=" -lm"
fi

AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug],
               [build debug message output code (default is no)]),
[case "${enableval}" in
             yes) debug=yes ;;
             no)  debug=no ;;
             *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],
[debug=no])

if (test "x$debug" = "xyes"); then
       AC_DEFINE(DEBUG, 1, [Define if debug message output code should be built.])
       GLOBAL_CFLAGS+=" -g"
fi

AC_SUBST(GLOBAL_CFLAGS)
AC_SUBST(GLOBAL_LDFLAGS)

case "$GLOBAL_CFLAGS" in
    *-fvisibility=hidden*)
        AC_DEFINE([HAVE_FVISIBILITY], [1], [Define if compiled with -fvisibility=hidden])
esac

AC_ARG_WITH([sanitizers],
            [AS_HELP_STRING([--with-sanitizers],
            [build libplist with sanitizers (default is no)])],
            [build_sanitizers=${withval}],
            [])

AC_ARG_WITH([fuzzers],
            [AS_HELP_STRING([--with-fuzzers],
            [build fuzzers (implies --with-sanitizers, default is no)])],
            [build_fuzzers=${withval}],
            [build_fuzzers=no])

if test "x$build_fuzzers" = "xyes"; then
    if test "x$build_sanitizers" = "xno"; then
        AC_MSG_ERROR([--with-fuzzers implies --with-sanitizers, but --without-sanitizers was given. This does not work.])
    fi
    build_sanitizers=yes
fi

if test "x$build_sanitizers" = "xyes"; then
    AS_COMPILER_FLAG([-fsanitize=address], [
        SANITIZER_FLAGS+=" -fsanitize=address"
        ASAN_AVAILABLE=yes
    ], [])
    if test "$ASAN_AVAILABLE" = "yes"; then
        AS_COMPILER_FLAG([-fsanitize=address -fsanitize-address-use-after-scope], [
            SANITIZER_FLAGS+=" -fsanitize-address-use-after-scope"
        ], [])
        SANITIZERS+="ASAN "
    fi

    AS_COMPILER_FLAG([-fsanitize=undefined], [
        SANITIZER_FLAGS+=" -fsanitize=undefined"
        UBSAN_AVAILABLE=yes
    ], [])

    if test "$UBSAN_AVAILABLE" = "yes"; then
        SANITIZERS+="UBSAN "
    fi

    if test -z "$SANITIZER_FLAGS"; then
        AC_MSG_ERROR([compiler doesn't support -fsanitize=address nor -fsanitize=undefined])
    fi

    COVERAGE_CHECKS="trace-pc-guard trace-cmp edge"
    for COV_CHECK in $COVERAGE_CHECKS; do
        AS_COMPILER_FLAG([-fsanitize-coverage=$COV_CHECK], [
            if test -z "$SAN_COV_FLAGS"; then
                SAN_COV_FLAGS="$COV_CHECK"
            else
                SAN_COV_FLAGS+=",$COV_CHECK"
            fi
        ], [])
    done
    if test -n "$SAN_COV_FLAGS"; then
        SANITIZER_FLAGS+=" -fsanitize-coverage=$SAN_COV_FLAGS"
        SANITIZERS+="+coverage "
    else
        AC_MSG_WARN([No sanitizer coverage supported by compiler])
    fi

    CFLAGS="-O1"

    AS_COMPILER_FLAG([-fno-omit-frame-pointer], [
        CFLAGS+=" -fno-omit-frame-pointer"
    ], [])

    AS_COMPILER_FLAG([-gline-tables-only], [
        CFLAGS+=" -gline-tables-only"
    ],
    [
        CFLAGS+=" -g"
    ])

    EXTRA_CONF+="  Enabled sanitizers ......: $SANITIZERS
"

    CFLAGS+=" $SANITIZER_FLAGS"
    CXXFLAGS="$CFLAGS -std=c++11"
fi

if test "x$build_fuzzers" = "xyes"; then
    if test "$CXX" != "clang++"; then
        AC_MSG_WARN([building fuzzers requires clang/clang++ (continuing anyway)])
    fi

    CFLAGS+=" -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION"

    EXTRA_CONF+="  Build fuzzers ...........: yes
"
fi

AM_CONDITIONAL([BUILD_FUZZERS],[test "x$build_fuzzers" = "xyes"])

if test "x$build_fuzzers" = "xyes" || test "x$build_sanitizers" = "xyes"; then
    AS_COMPILER_FLAGS(TEST_CFLAGS, [$CFLAGS])
fi

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_OUTPUT([
Makefile
libcnary/Makefile
src/Makefile
src/libplist-2.0.pc
src/libplist++-2.0.pc
include/Makefile
tools/Makefile
docs/Makefile
cython/Makefile
test/Makefile
fuzz/Makefile
doxygen.cfg
])

echo "
Configuration for $PACKAGE $VERSION:
-------------------------------------------

  Install prefix ..........: $prefix
  Debug code ..............: $debug
  Python bindings .........: $cython_python_bindings
$EXTRA_CONF
  Now type 'make' to build $PACKAGE $VERSION,
  and then 'make install' for installation.
"
