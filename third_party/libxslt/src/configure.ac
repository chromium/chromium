dnl Process this file with autoconf to produce a configuration script.
AC_PREREQ(2.63)

m4_define([MAJOR_VERSION], [1])
m4_define([MINOR_VERSION], [1])
m4_define([MICRO_VERSION], [33])

AC_INIT([libxslt], [MAJOR_VERSION.MINOR_VERSION.MICRO_VERSION])
AC_CONFIG_SRCDIR([libxslt/xslt.c])
AC_CONFIG_HEADERS(config.h)
AC_CANONICAL_HOST
AC_USE_SYSTEM_EXTENSIONS

dnl
dnl libxslt is the main part of the package
dnl libexslt is an extension
dnl
LIBXSLT_MAJOR_VERSION=MAJOR_VERSION
LIBXSLT_MINOR_VERSION=MINOR_VERSION
LIBXSLT_MICRO_VERSION=MICRO_VERSION
LIBEXSLT_MAJOR_VERSION=0
LIBEXSLT_MINOR_VERSION=8
LIBEXSLT_MICRO_VERSION=20
LIBXML_REQUIRED_VERSION=2.6.27


LIBXSLT_VERSION=$LIBXSLT_MAJOR_VERSION.$LIBXSLT_MINOR_VERSION.$LIBXSLT_MICRO_VERSION
LIBXSLT_VERSION_INFO=`expr $LIBXSLT_MAJOR_VERSION + $LIBXSLT_MINOR_VERSION`:$LIBXSLT_MICRO_VERSION:$LIBXSLT_MINOR_VERSION

LIBXSLT_VERSION_NUMBER=`expr $LIBXSLT_MAJOR_VERSION \* 10000 + $LIBXSLT_MINOR_VERSION \* 100 + $LIBXSLT_MICRO_VERSION`
LIBXSLT_MAJOR_MINOR_VERSION=$LIBXSLT_MAJOR_VERSION.$LIBXSLT_MINOR_VERSION

if test -f CVS/Entries; then
  extra=`grep ChangeLog CVS/Entries | grep -v LIBXSLT | sed -e s\%/ChangeLog/1\.%% -e s\%/.*$%%`
  echo extra=$extra
  if test "$extra" != ""
  then
      LIBXSLT_VERSION_EXTRA="-CVS$extra"
  fi
else if test -d .svn ; then
  extra=`svn info | grep Revision | sed 's+Revision: ++'`
  echo extra=$extra
  if test "$extra" != ""
  then
      LIBXSLT_VERSION_EXTRA="-SVN$extra"
  fi
else if test -d .git ; then
  extra=`git describe | sed 's+LIBXSLT[[0-9.]]*-++'`
  echo extra=$extra
  if test "$extra" != ""
  then
      LIBXSLT_VERSION_EXTRA="-GIT$extra"
  fi
fi
fi
fi

AC_SUBST(LIBXSLT_MAJOR_VERSION)
AC_SUBST(LIBXSLT_MINOR_VERSION)
AC_SUBST(LIBXSLT_MICRO_VERSION)
AC_SUBST(LIBXSLT_VERSION)
AC_SUBST(LIBXSLT_VERSION_INFO)
AC_SUBST(LIBXSLT_VERSION_NUMBER)
AC_SUBST(LIBXSLT_VERSION_EXTRA)
AC_SUBST(LIBXSLT_MAJOR_MINOR_VERSION)

dnl
dnl libexslt is an extension library
dnl

LIBEXSLT_VERSION=$LIBEXSLT_MAJOR_VERSION.$LIBEXSLT_MINOR_VERSION.$LIBEXSLT_MICRO_VERSION
LIBEXSLT_VERSION_INFO=`expr $LIBEXSLT_MAJOR_VERSION + $LIBEXSLT_MINOR_VERSION`:$LIBEXSLT_MICRO_VERSION:$LIBEXSLT_MINOR_VERSION

LIBEXSLT_VERSION_NUMBER=`expr $LIBEXSLT_MAJOR_VERSION \* 10000 + $LIBEXSLT_MINOR_VERSION \* 100 + $LIBEXSLT_MICRO_VERSION`

if test -f CVS/Entries; then
  LIBEXSLT_VERSION_EXTRA=-CVS`grep ChangeLog CVS/Entries | sed -e s\%/ChangeLog/1\.%% -e s\%/.*$%%`
fi

AC_SUBST(LIBEXSLT_MAJOR_VERSION)
AC_SUBST(LIBEXSLT_MINOR_VERSION)
AC_SUBST(LIBEXSLT_MICRO_VERSION)
AC_SUBST(LIBEXSLT_VERSION)
AC_SUBST(LIBEXSLT_VERSION_INFO)
AC_SUBST(LIBEXSLT_VERSION_NUMBER)
AC_SUBST(LIBEXSLT_VERSION_EXTRA)

AM_INIT_AUTOMAKE([foreign])

# AM_MAINTAINER_MODE

# Support silent build rules, requires at least automake-1.11. Disable
# by either passing --disable-silent-rules to configure or passing V=1
# to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl
dnl Specific dir for HTML output ?
dnl

AC_ARG_WITH(html-dir, AS_HELP_STRING([--with-html-dir=path],
            [path to base html directory, default $datadir/doc/html]),
            [HTML_DIR=$withval], [HTML_DIR='$(datadir)/doc'])

AC_ARG_WITH(html-subdir, AS_HELP_STRING([--with-html-subdir=path],
            [directory used under html-dir, default $PACKAGE-$VERSION/html]),
            [test "x$withval" != "x" && HTML_DIR="$HTML_DIR/$withval"],
            [HTML_DIR="$HTML_DIR/\$(PACKAGE)-\$(VERSION)/html"])

AC_SUBST(HTML_DIR)

dnl
dnl Check the environment
dnl

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_CPP
AC_PATH_PROG(XMLLINT, xmllint, /usr/bin/xmllint)
AC_PATH_PROG(XSLTPROC, xsltproc, /usr/bin/xsltproc)
PKG_PROG_PKG_CONFIG

AC_HEADER_STDC

LT_INIT(win32-dll)


AC_CHECK_HEADERS(sys/types.h sys/time.h stdlib.h unistd.h string.h)

dnl
dnl if the system support linker version scripts for symbol versioning
dnl then add it
dnl
AC_MSG_CHECKING([how to pass version script to the linker ($LD)])
VERSION_SCRIPT_FLAGS=none
if $LD --help 2>&1 | grep "version-script" >/dev/null 2>/dev/null; then
    VERSION_SCRIPT_FLAGS=-Wl,--version-script=
elif $LD --help 2>&1 | grep "M mapfile" >/dev/null 2>/dev/null; then
    VERSION_SCRIPT_FLAGS="-Wl,-M -Wl,"
fi
AC_MSG_RESULT([$VERSION_SCRIPT_FLAGS])
AC_SUBST(VERSION_SCRIPT_FLAGS)
AM_CONDITIONAL([USE_VERSION_SCRIPT], [test "$VERSION_SCRIPT_FLAGS" != none])

dnl Look for pthread.h, needed for testThreads
case $host in
  *-mingw*) ;;
  *)
THREAD_LIBS=""
AC_CHECK_HEADER(pthread.h,
    AC_CHECK_LIB(pthread, pthread_join,[
       AC_DEFINE([HAVE_LIBPTHREAD], [], [Define if pthread library is there (-lpthread)])
       AC_DEFINE([HAVE_PTHREAD_H], [], [Define if <pthread.h> is there])
       THREAD_LIBS="-lpthread"]))
  ;;
esac

AC_SUBST(THREAD_LIBS)

dnl
dnl Detect supported locale
dnl

AC_CHECK_HEADERS([locale.h xlocale.h])
AC_CHECK_FUNCS([strxfrm_l])

dnl
dnl Math detection
dnl

AC_CHECK_HEADERS(ieeefp.h nan.h math.h fp_class.h float.h)
AC_CHECK_HEADERS(sys/timeb.h time.h sys/stat.h sys/select.h stdarg.h)
AC_CHECK_HEADERS(errno.h)
AC_CHECK_FUNCS(stat _stat)
AC_CHECK_FUNC(pow, , AC_CHECK_LIB(m, pow,
  [M_LIBS="-lm"; AC_DEFINE([HAVE_POW],[], [Define if pow is there])]))

AC_CHECK_FUNC(floor, , AC_CHECK_LIB(m, floor,
  [M_LIBS="-lm"; AC_DEFINE([HAVE_FLOOR],[], [Define if floor is there])]))

AC_CHECK_FUNC(fabs, , AC_CHECK_LIB(m, fabs,
  [M_LIBS="-lm"; AC_DEFINE([HAVE_FABS],[], [Define if fabs is there])]))


AC_CHECK_FUNCS(gettimeofday)
AC_CHECK_FUNCS(localtime localtime_r time gmtime gmtime_r ftime)

dnl Checking the standard string functions availability
AC_CHECK_FUNCS(printf sprintf fprintf snprintf vfprintf vsprintf vsnprintf sscanf,,
               NEED_TRIO=1)

dnl Checking for POSIX timers
AC_CHECK_FUNCS(clock_gettime, [], [
  AC_CHECK_LIB(rt, clock_gettime, [
    AC_DEFINE(HAVE_CLOCK_GETTIME, 1)
    EXTRA_LIBS="$EXTRA_LIBS -lrt"
  ])
])

dnl
dnl Check for trio string functions
dnl

if test "${NEED_TRIO}" = "1" ; then
    echo Reusing trio library for string functions
    WITH_TRIO=1
else    
    WITH_TRIO=0
fi
AC_SUBST(WITH_TRIO)

dnl
dnl Some packages need to be checked against version numbers so we
dnl define a function here for later use
dnl
AC_DEFUN([VERSION_TO_NUMBER],
[`$1 | sed -e 's/libxml //' | awk 'BEGIN { FS = "."; } { printf "%d", ([$]1 * 1000 + [$]2) * 1000 + [$]3;}'`])

dnl
dnl Perl is just needed for generating some data for XSLtmark
dnl

AC_CHECK_PROG(PERL, perl, perl, false)
AM_CONDITIONAL(WITH_PERL, test "$PERL" != "false")

dnl
dnl check for python
dnl

PYTHON_VERSION=
PYTHON_INCLUDES=
PYTHON_SITE_PACKAGES=
pythondir=
AC_ARG_WITH(python, [  --with-python[=DIR]    Build Python bindings if found])
if test "$with_python" != "no" ; then
    if test -x "$with_python/bin/python"
    then
        echo Found python in $with_python/bin/python
        PYTHON="$with_python/bin/python"
    else
	if test -x "$with_python"
	then
	    echo Found python in $with_python
	    PYTHON="$with_python"
	else
            if test -x "$PYTHON"
            then
                echo Found python in environment PYTHON=$PYTHON
                with_python=`$PYTHON -c "import sys; print sys.exec_prefix"`
	    else
	        AC_PATH_PROG(PYTHON, python python2.4 python2.3 python2.2 python2.1 python2.0 python1.6 python1.5)
	    fi
	fi
    fi
    if test "$PYTHON" != ""
    then
        echo "PYTHON is pointing at $PYTHON"
        PYTHON_VERSION=`$PYTHON -c "import sys; print sys.version[[0:3]]"`
	echo Found Python version $PYTHON_VERSION
	LIBXML2_PYTHON=`$PYTHON -c "try : import libxml2 ; print 1
except: print 0"`
	if test "$LIBXML2_PYTHON" = "1"
	then
	    echo Found libxml2-python module
	else
	    echo Warning: Missing libxml2-python
	fi
    fi
    if test "$PYTHON_VERSION" != ""
    then
	if test -r $with_python/include/python$PYTHON_VERSION/Python.h -a \
	   -d $with_python/lib/python$PYTHON_VERSION/site-packages
	then
	    PYTHON_INCLUDES=$with_python/include/python$PYTHON_VERSION
	    PYTHON_SITE_PACKAGES='$(libdir)/python$(PYTHON_VERSION)/site-packages'
	else
	    if test -r $prefix/include/python$PYTHON_VERSION/Python.h
	    then
	        PYTHON_INCLUDES=$prefix/include/python$PYTHON_VERSION
		PYTHON_SITE_PACKAGES='$(libdir)/python$(PYTHON_VERSION)/site-packages'
	    else
		if test -r /usr/include/python$PYTHON_VERSION/Python.h
		then
		    PYTHON_INCLUDES=/usr/include/python$PYTHON_VERSION
		    PYTHON_SITE_PACKAGES='$(libdir)/python$(PYTHON_VERSION)/site-packages'
		else
		    echo could not find python$PYTHON_VERSION/Python.h
		fi
	    fi
	    if test ! -d "$PYTHON_SITE_PACKAGES"
	    then
		    PYTHON_SITE_PACKAGES=`$PYTHON -c "from distutils import sysconfig; print sysconfig.get_python_lib()"`
	    fi
	fi
        PYTHON_LIBS=`python$PYTHON_VERSION-config --libs`
    fi
    if test "$with_python" != ""
    then
        pythondir='$(PYTHON_SITE_PACKAGES)'
    else
        pythondir='$(libdir)/python$(PYTHON_VERSION)/site-packages'
    fi
fi
AM_CONDITIONAL(WITH_PYTHON, test "$PYTHON_INCLUDES" != "")
if test "$PYTHON_INCLUDES" != ""
then
    PYTHON_SUBDIR=python
else
    PYTHON_SUBDIR=
fi
AC_SUBST(pythondir)
AC_SUBST(PYTHON_SUBDIR)
AC_SUBST(PYTHON_LIBS)

AC_ARG_WITH(crypto, [  --with-crypto           Add crypto support to exslt (on)])
WITH_CRYPTO=0
CRYPTO_TESTDIR=
if test "$with_crypto" = "no" ; then
    echo Disabling crypto support
    LIBGCRYPT_CFLAGS=""
    LIBGCRYPT_LIBS=""
else
case $host in
  *-mingw*)
    dnl Use only native crypto-API for mingw* hosts
    dnl TODO: to implement --with-crypto=foo to allow switch between
    dnl crypto libraries
    WITH_CRYPTO=1
    CRYPTO_TESTDIR=crypto
    ;;
  *)
    AC_PATH_TOOL(LIBGCRYPT_CONFIG, libgcrypt-config, no)
    if test "$LIBGCRYPT_CONFIG" != "no" ; then
      LIBGCRYPT_VERSION=`$LIBGCRYPT_CONFIG --version`
      if test VERSION_TO_NUMBER(echo $LIBGCRYPT_VERSION) -lt VERSION_TO_NUMBER(echo "1.1.42")
      then
        LIBGCRYPT_CFLAGS=""
        LIBGCRYPT_LIBS=""
        echo 'gcrypt library version < 1.1.42 - Crypto extensions will not be available.'
      else
        LIBGCRYPT_CFLAGS=`$LIBGCRYPT_CONFIG $libgcrypt_config_args --cflags`
        LIBGCRYPT_LIBS=`$LIBGCRYPT_CONFIG $libgcrypt_config_args --libs`
        AC_DEFINE(HAVE_GCRYPT, 1, [Define if gcrypt library is available.])
        echo 'Crypto extensions will be available.'
        WITH_CRYPTO=1
        CRYPTO_TESTDIR=crypto
      fi
    else
      LIBGCRYPT_CFLAGS=""
      LIBGCRYPT_LIBS=""
      echo 'Crypto extensions will not be available. Install libgcrypt and reconfigure to make available.'
    fi
esac
fi
AC_SUBST(WITH_CRYPTO)
AC_SUBST(CRYPTO_TESTDIR)
AC_SUBST(LIBGCRYPT_CFLAGS)
AC_SUBST(LIBGCRYPT_LIBS)

dnl
dnl Debug for DV (-Wunreachable-code)
dnl
if [[ "${LOGNAME}" = "veillard" -a "`pwd`" = "/u/veillard/XSLT" ]] || \
   [[ "${LOGNAME}" = "bill" -a "`pwd`" = "/home/bill/gnomesvn/libxslt" ]]
   then
    if test "${with_mem_debug}" = "" ;
    then
	with_mem_debug="yes"
    fi
    CFLAGS="-Wall -g -O -ansi -pedantic -W -Wunused -Wimplicit -Wreturn-type -Wswitch -Wcomment -Wtrigraphs -Wformat=2 -Wmissing-format-attribute -Wchar-subscripts -Wuninitialized -Wparentheses -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -Winline "
fi

AC_ARG_WITH(debug, [  --with-debug            Add the debugging code (on)])
if test "$with_debug" = "no" ; then
    echo Disabling debug support
    WITH_XSLT_DEBUG=0
else    
    WITH_XSLT_DEBUG=1
fi
AC_SUBST(WITH_XSLT_DEBUG)

AC_ARG_WITH(mem_debug, [  --with-mem-debug        Add the memory debugging module (off)])
if test "$with_mem_debug" = "yes" ; then
    echo Enabling memory debug support
    WITH_MEM_DEBUG=1
else    
    WITH_MEM_DEBUG=0
fi
AC_SUBST(WITH_MEM_DEBUG)

dnl 
dnl Is debugger support requested
dnl
AC_ARG_WITH(debugger, [  --with-debugger        Add the debugging support (on)])
if test "$with_debugger" = "no" ; then
    echo Disabling debugger
    WITH_DEBUGGER=0
else
    echo Enabling debugger
    WITH_DEBUGGER=1
    AC_DEFINE([WITH_DEBUGGER],[], [Define if debugging support is enabled])
fi
AC_SUBST(WITH_DEBUGGER)

dnl
dnl The following new parameters were added to offer
dnl the ability to specify the location of the libxml
dnl library during linking and compilation.
dnl
dnl original work - Mathieu Lacage 30/03/2000
dnl some tweaking - David Härdeman 30/10/2001
dnl

LIBXML_CONFIG_PREFIX=""
LIBXML_SRC=""

AC_ARG_WITH(libxml-prefix,
        [  --with-libxml-prefix=[PFX]		Specify location of libxml config],
	LIBXML_CONFIG_PREFIX=$withval
)
        
AC_ARG_WITH(libxml-include-prefix,
        [  --with-libxml-include-prefix=[PFX]	Specify location of libxml headers],
        LIBXML_CFLAGS="-I$withval"
)

AC_ARG_WITH(libxml-libs-prefix,
        [  --with-libxml-libs-prefix=[PFX]	Specify location of libxml libs],
        LIBXML_LIBS="-L$withval"
)

AC_ARG_WITH(libxml-src,
	[  --with-libxml-src=[DIR]              For libxml thats not installed yet (sets all three above)],
	LIBXML_SRC="$withval"
)
AC_SUBST(LIBXML_SRC)

dnl
dnl where is xml2-config
dnl

AC_SUBST(LIBXML_REQUIRED_VERSION)
if test "x$LIBXML_CONFIG_PREFIX" != "x"
then
	AC_PATH_TOOL([XML_CONFIG], [xml2-config], [false], [${LIBXML_CONFIG_PREFIX}/bin])
else
	AC_PATH_TOOL([XML_CONFIG], [xml2-config], [false])
fi

dnl
dnl imported from libxml2, c.f. #77827 
dnl
if test "${GCC}" != "yes" ; then
    case "${host}" in
          *-*-hpux* )
	       CFLAGS="${CFLAGS} -Wp,-H30000"
	       ;;
          *-dec-osf* )
               CFLAGS="${CFLAGS} -ieee"
               ;;
    esac
else
    CFLAGS="${CFLAGS} -Wall -Wformat=2 -Wmissing-format-attribute"
    case "${host}" in
          alpha*-*-linux* )
	       CFLAGS="${CFLAGS} -mieee"
	       ;;
	  alpha*-*-osf* )
	       CFLAGS="${CFLAGS} -mieee"
	       ;;
    esac
fi

dnl
dnl Override other variables if LIBXML_SRC is set
dnl

if test "x$LIBXML_SRC" != "x"
then
	CWD=`pwd`
	if cd $LIBXML_SRC
	then
		LIBXML_SRC=`pwd`
		XML_CONFIG="${LIBXML_SRC}/xml2-config"
		LIBXML_CFLAGS="-I${LIBXML_SRC}/include"
		LIBXML_LIBS="-L${LIBXML_SRC} `$XML_CONFIG --libs`"
                WITH_MODULES="`$XML_CONFIG --modules`"
		cd $CWD
	else
		AC_MSG_ERROR([libxml source dir not found (${LIBXML_SRC}), typo?])
	fi
fi

dnl
dnl Try pkg-config first if nothing is set
dnl

if test "x$LIBXML_CONFIG_PREFIX" = "x" -a "x$LIBXML_LIBS" = "x"
then
	PKG_CHECK_MODULES([LIBXML], [libxml-2.0 >= $LIBXML_REQUIRED_VERSION], [
		WITH_MODULES="`$PKG_CONFIG --variable=modules libxml-2.0`"
	], [ ])
fi

dnl
dnl make sure xml2-config is executable,
dnl test version and init our variables
dnl

if test "x$LIBXML_LIBS" = "x" && ${XML_CONFIG} --libs print > /dev/null 2>&1
then
        AC_MSG_CHECKING(for libxml libraries >= $LIBXML_REQUIRED_VERSION)
	XMLVERS=`$XML_CONFIG --version`
	if test VERSION_TO_NUMBER(echo $XMLVERS) -ge VERSION_TO_NUMBER(echo $LIBXML_REQUIRED_VERSION)
	then
		AC_MSG_RESULT($XMLVERS found)
	else
		AC_MSG_ERROR(Version $XMLVERS found. You need at least libxml2 $LIBXML_REQUIRED_VERSION for this version of libxslt)
	fi
	LIBXML_LIBS="`$XML_CONFIG --libs`"
	LIBXML_CFLAGS="`$XML_CONFIG --cflags`"
        WITH_MODULES="`$XML_CONFIG --modules`"
fi

if test "x$LIBXML_LIBS" = "x"
then
	AC_MSG_ERROR([Could not find libxml2 anywhere, check ftp://xmlsoft.org/.])
fi


AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)

AC_ARG_WITH(plugins,
[  --with-plugins          Add plugin extension support (on)])
if test "$with_plugins" = ""
then
    with_plugins=yes
fi

AC_MSG_CHECKING([whether shared libraries will be built (required for plugins)])
if test "$enable_shared" = "no" -a "$with_plugins" = "yes"; then
	 AC_MSG_RESULT(no)
	 AC_MSG_WARN([Disabling plugin support.])
	 AC_MSG_WARN([Plugins require that shared libraries be built.])
	 with_plugins=no
else
	 AC_MSG_RESULT(yes)
fi

if test "$with_plugins" = "yes" ; then
  AC_MSG_CHECKING([libxml2 module support])
  if test "${WITH_MODULES}" = "1"; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
    WITH_MODULES="0"
  fi
else
  WITH_MODULES="0"
fi

AC_SUBST(WITH_MODULES)
AM_CONDITIONAL(WITH_MODULES, test "$WITH_MODULES" = "1")

dnl
dnl setup default module path
dnl
expanded_libdir=$(
    test "x$prefix" = xNONE && prefix="$ac_default_prefix"
    test "x$exec_prefix" = xNONE && exec_prefix="$prefix"
    while test "$libdir_old" != "$libdir"; do
      libdir_old="$libdir"
      eval libdir="$libdir"
    done
    echo "$libdir"
)
LIBXSLT_DEFAULT_PLUGINS_PATH="$expanded_libdir/libxslt-plugins"
AC_SUBST(LIBXSLT_DEFAULT_PLUGINS_PATH)

case "$host" in
 *-*-cygwin*|*-*-mingw*)
 LDFLAGS="$LDFLAGS -no-undefined"
 ;;
esac


AC_SUBST(XSLTPROCDV)
AC_SUBST(PYTHONSODV)
AC_SUBST(XML_CONFIG)
AC_SUBST(LIBXML_LIBS)
AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(PYTHON)
AC_SUBST(PYTHON_VERSION)
AC_SUBST(PYTHON_INCLUDES)
AC_SUBST(PYTHON_SITE_PACKAGES)

XSLT_LIBDIR='-L${libdir}'
XSLT_INCLUDEDIR='-I${includedir}'
XSLT_LIBS="-lxslt $LIBXML_LIBS $M_LIBS"
AC_SUBST(XSLT_LIBDIR)
AC_SUBST(XSLT_INCLUDEDIR)
AC_SUBST(XSLT_LIBS)

EXSLT_LIBDIR='-L${libdir}'
EXSLT_INCLUDEDIR='-I${includedir}'
EXSLT_LIBS="-lexslt $XSLT_LIBS $LIBGCRYPT_LIBS"
AC_SUBST(EXSLT_LIBDIR)
AC_SUBST(EXSLT_INCLUDEDIR)
AC_SUBST(EXSLT_LIBS)

AC_SUBST(EXTRA_LIBS)

AC_SUBST(M_LIBS)

dnl for the spec file
RELDATE=`date +'%a %b %e %Y'`
AC_SUBST(RELDATE)

rm -f COPYING.LIB COPYING 2>/dev/null && $LN_S $srcdir/Copyright COPYING


AC_CONFIG_FILES([
Makefile
libxslt.pc
libexslt.pc
libxslt/Makefile
libxslt/xsltconfig.h
libxslt/xsltwin32config.h
libexslt/Makefile
libexslt/exsltconfig.h
xsltproc/Makefile
python/Makefile
python/tests/Makefile
tests/Makefile
tests/docs/Makefile
tests/REC1/Makefile
tests/REC2/Makefile
tests/REC/Makefile
tests/general/Makefile
tests/reports/Makefile
tests/extensions/Makefile
tests/namespaces/Makefile
tests/keys/Makefile
tests/numbers/Makefile
tests/documents/Makefile
tests/xmlspec/Makefile
tests/multiple/Makefile
tests/xinclude/Makefile
tests/XSLTMark/Makefile
tests/docbook/Makefile
tests/exslt/Makefile
tests/exslt/common/Makefile
tests/exslt/functions/Makefile
tests/exslt/math/Makefile
tests/exslt/saxon/Makefile
tests/exslt/sets/Makefile
tests/exslt/strings/Makefile
tests/exslt/date/Makefile
tests/exslt/dynamic/Makefile
tests/plugins/Makefile
doc/Makefile
xslt-config
libxslt.spec
])

AC_OUTPUT
