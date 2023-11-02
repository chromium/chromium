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
	testThreads \
	testapi \
	testchar \
	testdict \
	testlimits \
	testparser \
	testrecurse

bin_PROGRAMS = xmllint xmlcatalog

bin_SCRIPTS = xml2-config

lib_LTLIBRARIES = libxml2.la
libxml2_la_CFLAGS = $(AM_CFLAGS) $(XML_PRIVATE_CFLAGS)
libxml2_la_LIBADD = $(XML_PRIVATE_LIBS)

libxml2_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined \
		     -version-info $(LIBXML_VERSION_INFO) \
		     $(MODULE_PLATFORM_LIBS)
if USE_VERSION_SCRIPT
libxml2_la_LDFLAGS += $(VERSION_SCRIPT_FLAGS)$(srcdir)/libxml2.syms
endif

libxml2_la_SOURCES = buf.c chvalid.c dict.c entities.c encoding.c error.c \
		     globals.c hash.c list.c parser.c parserInternals.c \
		     SAX2.c threads.c tree.c uri.c valid.c xmlIO.c \
		     xmlmemory.c xmlstring.c
if WITH_C14N_SOURCES
libxml2_la_SOURCES += c14n.c
endif
if WITH_CATALOG_SOURCES
libxml2_la_SOURCES += catalog.c
endif
if WITH_DEBUG_SOURCES
libxml2_la_SOURCES += debugXML.c
endif
if WITH_FTP_SOURCES
libxml2_la_SOURCES += nanoftp.c
endif
if WITH_HTML_SOURCES
libxml2_la_SOURCES += HTMLparser.c HTMLtree.c
endif
if WITH_HTTP_SOURCES
libxml2_la_SOURCES += nanohttp.c
endif
if WITH_LEGACY_SOURCES
libxml2_la_SOURCES += legacy.c
if WITH_SAX1_SOURCES
libxml2_la_SOURCES += SAX.c
endif
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
if WITH_SCHEMAS_SOURCES
libxml2_la_SOURCES += relaxng.c xmlschemas.c xmlschemastypes.c
if !WITH_XPATH_SOURCES
libxml2_la_SOURCES += xpath.c
endif
endif
if WITH_SCHEMATRON_SOURCES
libxml2_la_SOURCES += schematron.c
endif
if WITH_TRIO_SOURCES
libxml2_la_SOURCES += triostr.c trio.c
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

DEPS = $(top_builddir)/libxml2.la
LDADDS = $(top_builddir)/libxml2.la


m4datadir = $(datadir)/aclocal
dist_m4data_DATA = libxml.m4

runtest_SOURCES=runtest.c
runtest_CFLAGS = $(AM_CFLAGS) $(THREAD_CFLAGS)
runtest_DEPENDENCIES = $(DEPS)
runtest_LDADD= $(BASE_THREAD_LIBS) $(THREAD_LIBS) $(LDADDS)

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

xmllint_SOURCES=xmllint.c
xmllint_CFLAGS = $(AM_CFLAGS) $(RDL_CFLAGS) $(ICONV_CFLAGS)
xmllint_DEPENDENCIES = $(DEPS)
xmllint_LDADD=  $(RDL_LIBS) $(LDADDS)

xmlcatalog_SOURCES=xmlcatalog.c
xmlcatalog_CFLAGS = $(AM_CFLAGS) $(RDL_CFLAGS) $(ICONV_CFLAGS)
xmlcatalog_DEPENDENCIES = $(DEPS)
xmlcatalog_LDADD = $(RDL_LIBS) $(LDADDS)

testThreads_SOURCES = testThreads.c
testThreads_CFLAGS = $(AM_CFLAGS) $(THREAD_CFLAGS)
testThreads_DEPENDENCIES = $(DEPS)
testThreads_LDADD= $(BASE_THREAD_LIBS) $(THREAD_LIBS) $(LDADDS)

testModule_SOURCES=testModule.c
testModule_DEPENDENCIES = $(DEPS)
testModule_LDADD= $(LDADDS)

noinst_LTLIBRARIES = testdso.la
testdso_la_SOURCES = testdso.c
testdso_la_LDFLAGS = $(AM_LDFLAGS) \
		     -module -no-undefined -avoid-version -rpath $(libdir)

# that one forces the rebuild when "make rebuild" is run on doc/
rebuild_testapi:
	-@(if [ "$(PYTHON)" != "" ] ; then \
	    $(PYTHON) $(srcdir)/gentest.py $(srcdir) ; fi )

testapi_SOURCES=testapi.c
testapi_DEPENDENCIES = $(DEPS)
testapi_LDADD= $(LDADDS)

runxmlconf_SOURCES=runxmlconf.c
runxmlconf_DEPENDENCIES = $(DEPS)
runxmlconf_LDADD= $(LDADDS)

#testOOM_SOURCES=testOOM.c testOOMlib.h testOOMlib.c
#testOOM_DEPENDENCIES = $(DEPS)
#testOOM_LDADD= $(LDADDS)

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
	$(CHECKER) ./testThreads$(EXEEXT)
	$(CHECKER) ./runxmlconf$(EXEEXT)
	$(CHECKER) ./runsuite$(EXEEXT)

# Compatibility name of the check target
runtests: check

check-valgrind valgrind:
	@echo '## Running the regression tests under Valgrind'
	@echo '## Go get a cup of coffee it is gonna take a while ...'
	$(MAKE) CHECKER='valgrind -q' check

asan:
	@echo '## rebuilding for ASAN'
	./configure CFLAGS="-fsanitize=address,undefined -Wformat -Werror=format-security -Werror=array-bounds -g" CXXFLAGS="-fsanitize=address,undefined -Wformat -Werror=format-security -Werror=array-bounds -g" LDFLAGS="-fsanitize=address,undefined" CC="clang" CXX="clang++" --disable-shared ; OptimOff  ; $(MAKE) clean ; $(MAKE)

# Old test suite. This should be ported to C.

OLD_TESTS = Timingtests
if WITH_CATALOG_SOURCES
OLD_TESTS += Catatests
endif
if WITH_DEBUG_SOURCES
OLD_TESTS += Scripttests
endif
if WITH_SCHEMAS_SOURCES
if WITH_PYTHON
OLD_TESTS += RelaxNGPythonTests SchemasPythonTests
endif
endif
if WITH_SCHEMATRON_SOURCES
OLD_TESTS += Schematrontests
endif
if WITH_VALID_SOURCES
OLD_TESTS += VTimingtests
endif

tests: $(OLD_TESTS)

Scripttests : xmllint$(EXEEXT)
	@echo "## Scripts regression tests"
	@echo "## Some of the base computations may be different if srcdir != ."
	-@(for i in $(srcdir)/test/scripts/*.script ; do \
	  name=`basename $$i .script`; \
	  xml=$(srcdir)/test/scripts/`basename $$i .script`.xml; \
	  if [ -f $$xml ] ; then \
	  if [ ! -f $(srcdir)/result/scripts/$$name ] ; then \
	      echo New test file $$name ; \
	      $(CHECKER) $(top_builddir)/xmllint --shell $$xml < $$i > $(srcdir)/result/scripts/$$name 2> $(srcdir)/result/scripts/$$name.err ; \
	  else \
	      log=`$(CHECKER) $(top_builddir)/xmllint --shell $$xml < $$i > result.$$name 2> result.$$name.err ; \
	      diff $(srcdir)/result/scripts/$$name result.$$name ; \
	      diff $(srcdir)/result/scripts/$$name.err result.$$name.err` ; \
	      if [ -n "$$log" ] ; then echo $$name result ; echo "$$log" ; fi ; \
	      rm result.$$name result.$$name.err ; \
	  fi ; fi ; done)

Catatests : xmlcatalog$(EXEEXT)
	@echo "## Catalog regression tests"
	-@(for i in $(srcdir)/test/catalogs/*.script ; do \
	  name=`basename $$i .script`; \
	  xml=$(srcdir)/test/catalogs/`basename $$i .script`.xml; \
	  if [ -f $$xml ] ; then \
	  if [ ! -f $(srcdir)/result/catalogs/$$name ] ; then \
	      echo New test file $$name ; \
	      $(CHECKER) $(top_builddir)/xmlcatalog --shell $$xml < $$i 2>&1 > $(srcdir)/result/catalogs/$$name ; \
	  else \
	      log=`$(CHECKER) $(top_builddir)/xmlcatalog --shell $$xml < $$i 2>&1 > result.$$name ; \
	      diff $(srcdir)/result/catalogs/$$name result.$$name` ; \
	      if [ -n "$$log" ] ; then echo $$name result ; echo "$$log" ; fi ; \
	      rm result.$$name ; \
	  fi ; fi ; done)
	-@(for i in $(srcdir)/test/catalogs/*.script ; do \
	  name=`basename $$i .script`; \
	  sgml=$(srcdir)/test/catalogs/`basename $$i .script`.sgml; \
	  if [ -f $$sgml ] ; then \
	  if [ ! -f $(srcdir)/result/catalogs/$$name ] ; then \
	      echo New test file $$name ; \
	      $(CHECKER) $(top_builddir)/xmlcatalog --shell $$sgml < $$i > $(srcdir)/result/catalogs/$$name ; \
	  else \
	      log=`$(CHECKER) $(top_builddir)/xmlcatalog --shell $$sgml < $$i > result.$$name ; \
	      diff $(srcdir)/result/catalogs/$$name result.$$name` ; \
	      if [ -n "$$log" ] ; then echo $$name result ; echo "$$log" ; fi ; \
	      rm result.$$name ; \
	  fi ; fi ; done)
	@echo "## Add and del operations on XML Catalogs"
	-@($(CHECKER) $(top_builddir)/xmlcatalog --create --noout $(srcdir)/result/catalogs/mycatalog; \
	$(CHECKER) $(top_builddir)/xmlcatalog --noout --add public Pubid sysid $(srcdir)/result/catalogs/mycatalog; \
	$(CHECKER) $(top_builddir)/xmlcatalog --noout --add public Pubid2 sysid2 $(srcdir)/result/catalogs/mycatalog; \
	$(CHECKER) $(top_builddir)/xmlcatalog --noout --add public Pubid3 sysid3 $(srcdir)/result/catalogs/mycatalog; \
	diff result/catalogs/mycatalog.full $(srcdir)/result/catalogs/mycatalog; \
	$(CHECKER) $(top_builddir)/xmlcatalog --noout --del sysid $(srcdir)/result/catalogs/mycatalog; \
	$(CHECKER) $(top_builddir)/xmlcatalog --noout --del sysid3 $(srcdir)/result/catalogs/mycatalog; \
	$(CHECKER) $(top_builddir)/xmlcatalog --noout --del sysid2 $(srcdir)/result/catalogs/mycatalog; \
	diff result/catalogs/mycatalog.empty $(srcdir)/result/catalogs/mycatalog; \
	rm -f $(srcdir)/result/catalogs/mycatalog)

dba100000.xml: dbgenattr.pl
	@echo "## generating dba100000.xml"
	@($(PERL) $(top_srcdir)/dbgenattr.pl 100000 > dba100000.xml)

Timingtests: xmllint$(EXEEXT) dba100000.xml
	@echo "## Timing tests to try to detect performance"
	@echo "## as well a memory usage breakage when streaming"
	@echo "## 1/ using the file interface"
	@echo "## 2/ using the memory interface"
	@echo "## 3/ repeated DOM parsing"
	@echo "## 4/ repeated DOM validation"
	-@($(top_builddir)/xmllint --stream --timing dba100000.xml; \
	   if [ "$$MEM" != "" ] ; then echo Using $$MEM bytes ; fi ; \
	   exit 0)
	-@($(top_builddir)/xmllint --stream --timing --memory dba100000.xml; \
	   if [ "$$MEM" != "" ] ; then echo Using $$MEM bytes ; fi ; \
	   exit 0)
	-@($(top_builddir)/xmllint --noout --timing --repeat $(srcdir)/test/valid/REC-xml-19980210.xml; \
	   if [ "$$MEM" != "" ] ; then echo Using $$MEM bytes ; fi ; \
	   exit 0)

VTimingtests: xmllint$(EXEEXT)
	-@($(top_builddir)/xmllint --noout --timing --valid --repeat $(srcdir)/test/valid/REC-xml-19980210.xml; \
	   if [ "$$MEM" != "" ] ; then echo Using $$MEM bytes ; fi ; \
	   exit 0)

Schematrontests: xmllint$(EXEEXT)
	@echo "## Schematron regression tests"
	-@(for i in $(srcdir)/test/schematron/*.sct ; do \
	  name=`basename $$i | sed 's+\.sct++'`; \
	  for j in $(srcdir)/test/schematron/"$$name"_*.xml ; do \
	      if [ -f $$j ] ; then \
	      xno=`basename $$j | sed 's+.*_\(.*\).xml+\1+'`; \
	      if [ ! -f $(srcdir)/result/schematron/"$$name"_"$$xno" ]; \
	      then \
		  echo New test file "$$name"_"$$xno" ; \
		  $(CHECKER) $(top_builddir)/xmllint$(EXEEXT) --schematron $$i $$j \
		    > $(srcdir)/result/schematron/"$$name"_"$$xno" \
		    2> $(srcdir)/result/schematron/"$$name"_"$$xno".err; \
	      else \
	          log=`$(CHECKER) $(top_builddir)/xmllint$(EXEEXT) --schematron $$i $$j \
		    > res.$$name 2> err.$$name;\
	          diff $(srcdir)/result/schematron/"$$name"_"$$xno" \
		       res.$$name;\
	          diff $(srcdir)/result/schematron/"$$name"_"$$xno".err \
		       err.$$name | grep -v "error detected at";\
		  grep Unimplemented err.$$name`; \
		  if [ -n "$$log" ] ; then echo "$$name"_"$$xno" result ; echo "$$log" ; fi ; \
	          rm res.$$name err.$$name ; \
	       fi ; fi ; \
	  done; done)

RelaxNGPythonTests:
	@(if [ -x $(PYTHON) ] ; then \
	    PYTHONPATH=$(top_builddir)/python:$(top_builddir)/python/.libs:$$PYTHONPATH ; \
	    export PYTHONPATH; \
	    LD_LIBRARY_PATH="$(top_builddir)/.libs:$$LD_LIBRARY_PATH" ; \
	    export LD_LIBRARY_PATH; \
	    echo "## Relax-NG Python based test suite 1" ; \
	    $(CHECKER) $(PYTHON) $(srcdir)/check-relaxng-test-suite.py ; \
	    echo "## Relax-NG Python based test suite 2" ; \
	    $(CHECKER) $(PYTHON) $(srcdir)/check-relaxng-test-suite2.py ; \
	  fi)

SchemasPythonTests:
	@(if [ -x $(PYTHON) ] ; then \
	    PYTHONPATH=$(top_builddir)/python:$(top_builddir)/python/.libs:$$PYTHONPATH; \
	    export PYTHONPATH; \
	    LD_LIBRARY_PATH="$(top_builddir)/.libs:$$LD_LIBRARY_PATH" ; \
	    export LD_LIBRARY_PATH; \
	    echo "## XML Schemas datatypes Python based test suite" ; \
	    echo "## It is normal to see 11 errors reported" ; \
	    $(CHECKER) $(PYTHON) $(srcdir)/check-xsddata-test-suite.py ; \
	  fi)
	@(if [ -x $(PYTHON) -a -d xstc ] ; then cd xstc ; $(MAKE) CHECKER="$(CHECKER)" pytests ; fi)

cleanup:
	-@(find . -name .\#\* -exec rm {} \;)
	-@(find . -name \*.gcda -o -name \*.gcno -exec rm -f {} \;)
	-@(find . -name \*.orig -o -name \*.rej -o -name \*.old -exec rm -f {} \;)

dist-hook: cleanup
	(cd $(srcdir) ; tar -cf - --exclude .git win32 os400 vms test result) | (cd $(distdir); tar xf -)

CLEANFILES = runsuite.log runxmlconf.log test.out *.gcda *.gcno *.res
DISTCLEANFILES = COPYING missing.lst

EXTRA_DIST = Copyright check-xml-test-suite.py gentest.py \
	     check-relaxng-test-suite.py check-relaxng-test-suite2.py \
	     check-xsddata-test-suite.py check-xinclude-test-suite.py \
             example/Makefile.am example/gjobread.c example/gjobs.xml \
	     libxml2-config.cmake.in autogen.sh \
	     trionan.c trionan.h triostr.c triostr.h trio.c trio.h \
	     triop.h triodef.h libxml.h \
	     genUnicode.py \
	     dbgen.pl dbgenattr.pl \
	     libxml2.syms timsort.h \
	     README.zOS README.md \
	     CMakeLists.txt config.h.cmake.in libxml2-config.cmake.cmake.in


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libxml-2.0.pc

cmakedir = $(libdir)/cmake/libxml2
cmake_DATA = libxml2-config.cmake

#
# Install the tests program sources as examples 
#
examplesdir = $(docdir)/examples
examples_DATA = xmllint.c

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

