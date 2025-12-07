## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include . example
if WITH_DOXYGEN
SUBDIRS += doc
endif
if WITH_PYTHON
SUBDIRS += python
endif
if WITH_GLOB
SUBDIRS += fuzz
endif

DIST_SUBDIRS = include . doc example fuzz python

AM_CPPFLAGS = -I$(builddir)/include -I$(srcdir)/include

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
xmlcatalog_CFLAGS = $(AM_CFLAGS) $(RDL_CFLAGS)
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
libxml2_la_SOURCES += xmlregexp.c
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
xmllint_CFLAGS = $(AM_CFLAGS) $(RDL_CFLAGS)
xmllint_DEPENDENCIES = $(DEPS)
xmllint_LDADD=  $(RDL_LIBS) $(LDADDS)

testModule_SOURCES=testModule.c
testModule_DEPENDENCIES = $(DEPS)
testModule_LDADD= $(LDADDS)

check_LTLIBRARIES = testdso.la
testdso_la_SOURCES = testdso.c
testdso_la_LDFLAGS = $(AM_LDFLAGS) \
		     -module -no-undefined -avoid-version -rpath $(libdir)

rebuild_testapi:
	cd $(srcdir) && python3 codegen/genTestApi.py $(abs_builddir)

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
if WITH_OUTPUT_SOURCES
if WITH_DEBUG_SOURCES
	test/scripts/test.sh ./xmllint$(EXEEXT)
endif
if WITH_CATALOG_SOURCES
	ASAN_OPTIONS=detect_leaks=0 test/catalogs/test.sh ./xmlcatalog$(EXEEXT)
	ASAN_OPTIONS=detect_leaks=0 test/catalogs/test_sgml.sh ./xmlcatalog$(EXEEXT)
endif
endif

# Compatibility name of the check target
runtests: check

check-valgrind valgrind:
	@echo '## Running the regression tests under Valgrind'
	@echo '## Go get a cup of coffee it is gonna take a while ...'
	$(MAKE) CHECKER='valgrind -q' check

dist-hook:
	(cd $(srcdir) ; tar -cf - --exclude .git win32 test result) | (cd $(distdir); tar xf -)

CLEANFILES = missing.lst runsuite.log runxmlconf.log test.out \
	     *.gcda *.gcno *.res

EXTRA_DIST = Copyright libxml2-config.cmake.in autogen.sh \
	     libxml.h \
	     codegen/charset.inc \
	     codegen/escape.inc \
	     codegen/genCharset.py \
	     codegen/genEscape.py \
	     codegen/genHtml5Ent.py \
	     codegen/genHtml5LibTests.py \
	     codegen/genRanges.py \
	     codegen/genTestApi.py \
	     codegen/genUnicode.py \
	     codegen/html5ent.inc \
	     codegen/ranges.def \
	     codegen/ranges.inc \
	     codegen/rangetab.py \
	     codegen/unicode.inc \
	     codegen/xmlmod.py \
	     timsort.h \
	     README.zOS README.md \
	     CMakeLists.txt config.h.cmake.in libxml2-config.cmake.cmake.in \
	     meson.build meson_options.txt xml2-config-meson

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libxml-2.0.pc

cmakedir = $(libdir)/cmake/libxml2
cmake_DATA = libxml2-config.cmake
