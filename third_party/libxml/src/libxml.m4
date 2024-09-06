dnl AM_PATH_XML2([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for XML, and define XML_CPPFLAGS and XML_LIBS
dnl
AC_DEFUN([AM_PATH_XML2],[
  m4_warn([obsolete], [AM_PATH_XML2 is deprecated, use PKG_CHECK_MODULES instead])
  AC_REQUIRE([PKG_PROG_PKG_CONFIG])

  verdep=ifelse([$1], [], [], [">= $1"])
  PKG_CHECK_MODULES(XML, [libxml-2.0 $verdep], [$2], [$3])

  XML_CPPFLAGS=$XML_CFLAGS
  AC_SUBST(XML_CPPFLAGS)
  AC_SUBST(XML_LIBS)
])
