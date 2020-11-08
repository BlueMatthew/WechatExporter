
AC_DEFUN([AC_PROG_CYTHON],[
        AC_PATH_PROG([CYTHON],[cython])
        if test -z "$CYTHON" ; then
                AC_MSG_WARN([cannot find 'cython' program. You should look at http://www.cython.org] or install your distribution specific cython package.)
                CYTHON=false
        elif test -n "$1" ; then
                AC_MSG_CHECKING([for Cython version])
                [cython_version=`$CYTHON --version 2>&1 | sed 's/Cython version \(.*\)$/\1/g'`]
                AC_MSG_RESULT([$cython_version])

                # Setup extra version string for parsing
                [cython_version_stripped=`echo $cython_version | sed 's/\([0-9]\+\)\.\([0-9]\+\)[^\.]*\(\.\([0-9]\+\)\)\?.*/0\1.0\2.0\4/g'`]
                if test -n "$cython_version" ; then
                        # Calculate the required version number components
                        [required=$1]
                        [required_major=`echo $required | sed 's/[^0-9].*//'`]
                        if test -z "$required_major" ; then
                                [required_major=0]
                        fi
                        [required=`echo $required | sed 's/[0-9]*[^0-9]//'`]
                        [required_minor=`echo $required | sed 's/[^0-9].*//'`]
                        if test -z "$required_minor" ; then
                                [required_minor=0]
                        fi
                        [required=`echo $required | sed 's/[0-9]*[^0-9]//'`]
                        [required_patch=`echo $required | sed 's/[^0-9].*//'`]
                        if test -z "$required_patch" ; then
                                [required_patch=0]
                        fi

                        # Calculate the available version number components
                        [available=$cython_version_stripped]
                        [available_major=`echo $available | sed 's/[^0-9].*//'`]
                        if test -z "$available_major" ; then
                                [available_major=0]
                        fi
                        [available=`echo $available | sed 's/[0-9]*[^0-9]//'`]
                        [available_minor=`echo $available | sed 's/[^0-9].*//'`]
                        if test -z "$available_minor" ; then
                                [available_minor=0]
                        fi
                        [available=`echo $available | sed 's/[0-9]*[^0-9]//'`]
                        [available_patch=`echo $available | sed 's/[^0-9].*//'`]
                        if test -z "$available_patch" ; then
                                [available_patch=0]
                        fi

                        if test $available_major -gt $required_major || \
                                ( test $available_major -eq $required_major && \
                                      test $available_minor -gt $required_minor ) || \
                                ( test $available_major -eq $required_major && \
                                        test $available_minor -eq $required_minor && \
                                        test $available_patch -ge $required_patch ) ; then

                                AC_MSG_NOTICE([Cython executable is '$CYTHON'])
                        else
                                AC_MSG_WARN([Cython version >= $1 is required.  You have $cython_version.  You should look at http://www.cython.org])
                                CYTHON='echo "Error: Cython version >= $1 is required.  You have '"$cython_version"'.  You should look at http://www.cython.org" ; false'
                        fi
                else
                        AC_MSG_WARN([cannot determine Cython version])
                        CYTHON=false
                fi
        fi
        AC_SUBST([CYTHON_LIB])
])
