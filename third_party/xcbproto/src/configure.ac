#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([XCB Proto],
        1.17.0,
        [xcb@lists.freedesktop.org])
AC_CONFIG_SRCDIR([xcb-proto.pc.in])
AM_INIT_AUTOMAKE([foreign dist-xz])

AC_PATH_PROG(XMLLINT, xmllint, no)
AM_CONDITIONAL(HAVE_XMLLINT, test "x$XMLLINT" != "xno")
if test "$XMLLINT" = "no"; then
   AC_MSG_WARN([xmllint not found; unable to validate against schema.])
fi

AM_PATH_PYTHON([2.5])

xcbincludedir='${datadir}/xcb'
AC_SUBST(xcbincludedir)

AC_CONFIG_FILES([Makefile src/Makefile xcbgen/Makefile xcb-proto.pc])
AC_OUTPUT
