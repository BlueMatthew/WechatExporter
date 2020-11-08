AC_DEFUN([CYTHON_PYTHON],[
        AC_REQUIRE([AC_PROG_CYTHON])
        AC_REQUIRE([AC_PYTHON_DEVEL])
        test "x$1" != "xno" || cython_shadow=" -noproxy"
        AC_SUBST([CYTHON_PYTHON_OPT],[-python$cython_shadow])
        AC_SUBST([CYTHON_PYTHON_CPPFLAGS],[$PYTHON_CPPFLAGS])
])
