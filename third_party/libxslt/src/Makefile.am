ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libxslt libexslt xsltproc doc tests
if WITH_PYTHON
SUBDIRS += python
endif

DIST_SUBDIRS = libxslt libexslt xsltproc python doc tests

confexecdir=$(libdir)
confexec_DATA = xsltConf.sh

bin_SCRIPTS = xslt-config

dist-hook: cleanup
	touch $(distdir)/doc/*.xml
	touch $(distdir)/doc/EXSLT/*.xml
	touch $(distdir)/libxslt/*.syms
	(cd $(srcdir) ; tar -cf - win32 vms examples) | (cd $(distdir); tar xf -)

EXTRA_DIST = xsltConf.sh.in \
             FEATURES TODO Copyright \
	     CMakeLists.txt config.h.cmake.in libxslt-config.cmake.in libxslt-config.cmake.cmake.in

## We create xsltConf.sh here and not from configure because we want
## to get the paths expanded correctly.  Macros like srcdir are given
## the value NONE in configure if the user doesn't specify them (this
## is an autoconf feature, not a bug).

xsltConf.sh: xsltConf.sh.in Makefile
## Use sed and then mv to avoid problems if the user interrupts.
	sed -e 's?\@XSLT_LIBDIR\@?$(XSLT_LIBDIR)?g' \
	    -e 's?\@XSLT_INCLUDEDIR\@?$(XSLT_INCLUDEDIR)?g' \
	    -e 's?\@VERSION\@?$(VERSION)?g' \
	    -e 's?\@XSLT_LIBS\@?$(XSLT_LIBS) $(EXTRA_LIBS)?g' \
	    -e 's?\@XSLT_PRIVATE_LIBS\@?$(XSLT_PRIVATE_LIBS)?g' \
	       < $(srcdir)/xsltConf.sh.in > xsltConf.tmp \
	&& mv xsltConf.tmp xsltConf.sh

CLEANFILES = xsltConf.sh

cleanup:
	-@(find . -name .\#\* -exec rm {} \;)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libxslt.pc libexslt.pc

cmakedir = $(libdir)/cmake/libxslt
dist_cmake_DATA = FindGcrypt.cmake
cmake_DATA = libxslt-config.cmake

