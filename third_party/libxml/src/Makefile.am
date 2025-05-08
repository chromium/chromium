## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include . doc example xstc
if WITH_PYTHON
SUBDIRS += python
endif
if WITH_GLOB
SUBDIRS += fuzz
endif

DIST_SUBDIRS = include . doc example fuzz python xstc

AM_CPPFLAGS = -I$(top_builddir)/include -I$(srcdir)/include -DSYSCONFDIR='"$(sysconfdir)"'

check_PROGRAMS = \
	runsuite \
	runtest \
	runxmlconf \
	testModule \
	testapi \
	testchar \
	testdict \
	testlimits \
	testparser \
	testrecurse

bin_PROGRAMS = xmllint

bin_SCRIPTS = xml2-config

lib_LTLIBRARIES = libxml2.la
libxml2_la_CFLAGS = $(AM_CFLAGS) $(XML_PRIVATE_CFLAGS)
libxml2_la_LIBADD = $(XML_PRIVATE_LIBS)

libxml2_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined \
		     -version-info $(LIBXML_VERSION_INFO)

libxml2_la_SOURCES = buf.c chvalid.c dict.c entities.c encoding.c error.c \
		     globals.c hash.c list.c parser.c parserInternals.c \
		     SAX2.c threads.c tree.c uri.c valid.c xmlIO.c \
		     xmlmemory.c xmlstring.c
if WITH_C14N_SOURCES
libxml2_la_SOURCES += c14n.c
endif
if WITH_CATALOG_SOURCES
if WITH_OUTPUT_SOURCES
bin_PROGRAMS += xmlcatalog

xmlcatalog_SOURCES = xmlcatalog.c
xmlcatalog_CFLAGS = $(AM_CFLAGS) $(RDL_CFLAGS) $(ICONV_CFLAGS)
xmlcatalog_DEPENDENCIES = $(DEPS)
xmlcatalog_LDADD = $(RDL_LIBS) $(LDADDS)
endif

libxml2_la_SOURCES += catalog.c
endif
if WITH_DEBUG_SOURCES
libxml2_la_SOURCES += debugXML.c
endif
if WITH_HTML_SOURCES
libxml2_la_SOURCES += HTMLparser.c HTMLtree.c
endif
if WITH_HTTP_SOURCES
libxml2_la_SOURCES += nanohttp.c
endif
if WITH_LZMA_SOURCES
libxml2_la_SOURCES += xzlib.c
endif
if WITH_MODULES_SOURCES
libxml2_la_SOURCES += xmlmodule.c
endif
if WITH_OUTPUT_SOURCES
libxml2_la_SOURCES += xmlsave.c
endif
if WITH_PATTERN_SOURCES
libxml2_la_SOURCES += pattern.c
endif
if WITH_READER_SOURCES
libxml2_la_SOURCES += xmlreader.c
endif
if WITH_REGEXPS_SOURCES
libxml2_la_SOURCES += xmlregexp.c xmlunicode.c
endif
if WITH_RELAXNG_SOURCES
libxml2_la_SOURCES += relaxng.c
endif
if WITH_SCHEMAS_SOURCES
libxml2_la_SOURCES += xmlschemas.c xmlschemastypes.c
endif
if WITH_SCHEMATRON_SOURCES
libxml2_la_SOURCES += schematron.c
endif
if WITH_WRITER_SOURCES
libxml2_la_SOURCES += xmlwriter.c
endif
if WITH_XINCLUDE_SOURCES
libxml2_la_SOURCES += xinclude.c
endif
if WITH_XPATH_SOURCES
libxml2_la_SOURCES += xpath.c
endif
if WITH_XPTR_SOURCES
libxml2_la_SOURCES += xlink.c xpointer.c
endif

DEPS = libxml2.la
LDADDS = libxml2.la

runtest_SOURCES=runtest.c
runtest_DEPENDENCIES = $(DEPS)
runtest_LDADD= $(THREAD_LIBS) $(LDADDS)

testrecurse_SOURCES=testrecurse.c
testrecurse_DEPENDENCIES = $(DEPS)
testrecurse_LDADD= $(LDADDS)

testlimits_SOURCES=testlimits.c
testlimits_DEPENDENCIES = $(DEPS)
testlimits_LDADD= $(LDADDS)

testchar_SOURCES=testchar.c
testchar_DEPENDENCIES = $(DEPS)
testchar_LDADD= $(LDADDS)

testdict_SOURCES=testdict.c
testdict_DEPENDENCIES = $(DEPS)
testdict_LDADD= $(LDADDS)

testparser_SOURCES=testparser.c
testparser_DEPENDENCIES = $(DEPS)
testparser_LDADD= $(LDADDS)

runsuite_SOURCES=runsuite.c
runsuite_DEPENDENCIES = $(DEPS)
runsuite_LDADD= $(LDADDS)

xmllint_SOURCES = xmllint.c shell.c lintmain.c
xmllint_CFLAGS = $(AM_CFLAGS) $(RDL_CFLAGS) $(ICONV_CFLAGS)
xmllint_DEPENDENCIES = $(DEPS)
xmllint_LDADD=  $(RDL_LIBS) $(LDADDS)

testModule_SOURCES=testModule.c
testModule_DEPENDENCIES = $(DEPS)
testModule_LDADD= $(LDADDS)

check_LTLIBRARIES = testdso.la
testdso_la_SOURCES = testdso.c
testdso_la_LDFLAGS = $(AM_LDFLAGS) \
		     -module -no-undefined -avoid-version -rpath $(libdir)

# that one forces the rebuild when "make rebuild" is run on doc/
rebuild_testapi:
	-@(if [ "$(PYTHON)" != "" ] ; then \
	    $(PYTHON) $(srcdir)/tools/gentest.py $(srcdir) ; fi )

testapi_SOURCES=testapi.c
testapi_DEPENDENCIES = $(DEPS)
testapi_LDADD= $(LDADDS)

runxmlconf_SOURCES=runxmlconf.c
runxmlconf_DEPENDENCIES = $(DEPS)
runxmlconf_LDADD= $(LDADDS)

check-local:
	[ -d test   ] || $(LN_S) $(srcdir)/test   .
	[ -d result ] || $(LN_S) $(srcdir)/result .
	$(CHECKER) ./runtest$(EXEEXT)
	$(CHECKER) ./testrecurse$(EXEEXT)
	$(CHECKER) ./testapi$(EXEEXT)
	$(CHECKER) ./testchar$(EXEEXT)
	$(CHECKER) ./testdict$(EXEEXT)
	$(CHECKER) ./testparser$(EXEEXT)
	$(CHECKER) ./testModule$(EXEEXT)
	$(CHECKER) ./runxmlconf$(EXEEXT) -d $(srcdir)/xmlconf
	$(CHECKER) ./runsuite$(EXEEXT)
if WITH_DEBUG_SOURCES
	test/scripts/test.sh ./xmllint$(EXEEXT)
endif
if WITH_CATALOG_SOURCES
if WITH_OUTPUT_SOURCES
	ASAN_OPTIONS=detect_leaks=0 test/catalogs/test.sh ./xmlcatalog$(EXEEXT)
endif
endif

# Compatibility name of the check target
runtests: check

check-valgrind valgrind:
	@echo '## Running the regression tests under Valgrind'
	@echo '## Go get a cup of coffee it is gonna take a while ...'
	$(MAKE) CHECKER='valgrind -q' check

asan:
	@echo '## rebuilding for ASAN'
	./configure CFLAGS="-fsanitize=address,undefined -Wformat -Werror=format-security -Werror=array-bounds -g" CXXFLAGS="-fsanitize=address,undefined -Wformat -Werror=format-security -Werror=array-bounds -g" LDFLAGS="-fsanitize=address,undefined" CC="clang" CXX="clang++" --disable-shared ; OptimOff  ; $(MAKE) clean ; $(MAKE)

cleanup:
	-@(find . -name .\#\* -exec rm {} \;)
	-@(find . -name \*.gcda -o -name \*.gcno -exec rm -f {} \;)
	-@(find . -name \*.orig -o -name \*.rej -o -name \*.old -exec rm -f {} \;)

dist-hook: cleanup
	(cd $(srcdir) ; tar -cf - --exclude .git win32 test result) | (cd $(distdir); tar xf -)

CLEANFILES = runsuite.log runxmlconf.log test.out *.gcda *.gcno *.res
DISTCLEANFILES = COPYING missing.lst

EXTRA_DIST = Copyright libxml2-config.cmake.in autogen.sh \
	     libxml.h \
	     html5ent.inc iso8859x.inc \
	     tools/gentest.py \
	     tools/genChRanges.py tools/genEscape.py tools/genUnicode.py \
	     timsort.h \
	     README.zOS README.md \
	     CMakeLists.txt config.h.cmake.in libxml2-config.cmake.cmake.in \
	     meson.build meson_options.txt xml2-config-meson


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libxml-2.0.pc

cmakedir = $(libdir)/cmake/libxml2
cmake_DATA = libxml2-config.cmake

tst: tst.c
	$(CC) $(CFLAGS) -Iinclude -o tst tst.c .libs/libxml2.a -lpthread -lm -lz -llzma

sparse: clean
	$(MAKE) CC=cgcc

#
# Coverage support, largely borrowed from libvirt
# Both binaries comes from the lcov package in Fedora
#
LCOV = /usr/bin/lcov
GENHTML = /usr/bin/genhtml

cov: clean-cov
	if [ "`echo $(AM_LDFLAGS) | grep coverage`" = "" ] ; then \
	    echo not configured with coverage; exit 1 ; fi
	if [ ! -x $(LCOV) -o ! -x $(GENHTML) ] ; then \
	    echo Need $(LCOV) and $(GENHTML) excecutables; exit 1 ; fi
	-@($(MAKE) check)
	-@(./runsuite$(EXEEXT))
	mkdir $(top_builddir)/coverage
	$(LCOV) -c -o $(top_builddir)/coverage/libxml2.info.tmp -d $(top_srcdir)
	$(LCOV) -r $(top_builddir)/coverage/libxml2.info.tmp -o $(top_builddir)/coverage/libxml2.info *usr*
	rm $(top_builddir)/coverage/libxml2.info.tmp
	$(GENHTML) -s -t "libxml2" -o $(top_builddir)/coverage --legend $(top_builddir)/coverage/libxml2.info
	echo "Coverage report is in $(top_builddir)/coverage/index.html"

clean-cov:
	rm -rf $(top_builddir)/coverage

