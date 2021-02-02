SUBDIRS = \
	libxslt \
	libexslt \
	xsltproc \
	doc \
	$(PYTHON_SUBDIR) \
	tests

DIST_SUBDIRS = libxslt libexslt xsltproc python doc tests

confexecdir=$(libdir)
confexec_DATA = xsltConf.sh

bin_SCRIPTS = xslt-config

dist-hook: cleanup libxslt.spec
	touch $(distdir)/doc/*.xml
	touch $(distdir)/doc/EXSLT/*.xml
	touch $(distdir)/libxslt/*.syms
	(cd $(srcdir) ; tar -cf - --exclude CVS --exclude .svn win32 vms examples) | (cd $(distdir); tar xf -)

CVS_EXTRA_DIST =

EXTRA_DIST = xsltConf.sh.in xslt-config.in libxslt.spec libxslt.spec.in \
             FEATURES TODO Copyright libxslt.m4 \
	     win32/libxslt/libxslt.def win32/libxslt/libxslt.dsw \
	     win32/libxslt/libxslt_so.dsp win32/libxslt/xsltproc.dsp \
	     CMakeLists.txt config.h.cmake.in FindGcrypt.cmake libxslt-config.cmake.in libxslt-config.cmake.cmake.in \
	     $(CVS_EXTRA_DIST)

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

check-local: tests

dummy:

tests: dummy
	@echo '## Running the regression test suite'
	@(cd tests ; $(MAKE) -s tests)
	@(cd xsltproc ; $(MAKE) -s tests)
	@(if [ "$(PYTHON_SUBDIR)" != "" ] ; then cd python ; $(MAKE) -s tests ; fi)

valgrind:
	@echo '## Running the regression tests under Valgrind'
	@echo '## Go get a cup of coffee it is gonna take a while ...'
	@(cd tests ; $(MAKE) CHECKER='libtool --mode=execute valgrind -q --leak-check=full' tests)
	@(cd xsltproc ; $(MAKE) CHECKER='libtool --mode=execute valgrind -q --leak-check=full' tests)

cleanup:
	-@(find . -name .\#\* -exec rm {} \;)

cleantar:
	@(rm -f libxslt*.tar.gz)

rpm: cleantar
	@(unset CDPATH ; $(MAKE) dist && rpmbuild -ta $(distdir).tar.gz)


pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libxslt.pc libexslt.pc

cmakedir = $(libdir)/cmake/libxslt
cmake_DATA = FindGcrypt.cmake libxslt-config.cmake

m4datadir = $(datadir)/aclocal
m4data_DATA = libxslt.m4

