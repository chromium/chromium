AUTOMAKE_OPTIONS = -Wno-syntax
EXTRA_PROGRAMS = genSeed html regexp schema uri valid xinclude xml xpath
check_PROGRAMS = testFuzzer
EXTRA_DIST = html.dict regexp.dict schema.dict xml.dict xpath.dict \
	     static_seed/uri static_seed/regexp fuzz.h
CLEANFILES = $(EXTRA_PROGRAMS)
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include
DEPENDENCIES = $(top_builddir)/libxml2.la
LDADD = $(top_builddir)/libxml2.la

XML_MAX_LEN = 80000
# Single quotes to avoid wildcard expansion by the shell
XML_SEED_CORPUS_SRC = \
    '$(top_srcdir)/test/*' \
    '$(top_srcdir)/test/errors/*.xml' \
    '$(top_srcdir)/test/errors10/*.xml' \
    '$(top_srcdir)/test/namespaces/*' \
    '$(top_srcdir)/test/recurse/*.xml' \
    '$(top_srcdir)/test/SVG/*.xml' \
    '$(top_srcdir)/test/valid/*.xml' \
    '$(top_srcdir)/test/VC/*' \
    '$(top_srcdir)/test/VCM/*' \
    '$(top_srcdir)/test/xmlid/*'

testFuzzer_SOURCES = testFuzzer.c fuzz.c

.PHONY: corpus clean-corpus

corpus: seed/html.stamp seed/regexp.stamp seed/schema.stamp seed/uri.stamp \
    seed/valid.stamp seed/xinclude.stamp seed/xml.stamp seed/xpath.stamp

check-local: corpus
	./testFuzzer$(EXEEXT)

clean-corpus:
	rm -rf seed

clean-local: clean-corpus

# Seed corpus

genSeed_SOURCES = genSeed.c fuzz.c

# XML fuzzer

seed/xml.stamp: genSeed$(EXEEXT)
	@mkdir -p seed/xml
	./genSeed$(EXEEXT) xml $(XML_SEED_CORPUS_SRC)
	@touch seed/xml.stamp

xml_SOURCES = xml.c fuzz.c
xml_LDFLAGS = $(AM_LDFLAGS) -fsanitize=fuzzer

fuzz-xml: xml$(EXEEXT) seed/xml.stamp
	@mkdir -p corpus/xml
	./xml$(EXEEXT) \
	    -dict=xml.dict \
	    -max_len=$(XML_MAX_LEN) \
	    -timeout=20 \
	    corpus/xml seed/xml

# DTD validation fuzzer

seed/valid.stamp: genSeed$(EXEEXT)
	@mkdir -p seed/valid
	./genSeed$(EXEEXT) valid $(XML_SEED_CORPUS_SRC)
	@touch seed/valid.stamp

valid_SOURCES = valid.c fuzz.c
valid_LDFLAGS = $(AM_LDFLAGS) -fsanitize=fuzzer

fuzz-valid: valid$(EXEEXT) seed/valid.stamp
	@mkdir -p corpus/valid
	./valid$(EXEEXT) \
	    -dict=xml.dict \
	    -max_len=$(XML_MAX_LEN) \
	    -timeout=20 \
	    corpus/valid seed/valid

# XInclude fuzzer

seed/xinclude.stamp: genSeed$(EXEEXT)
	@mkdir -p seed/xinclude
	./genSeed$(EXEEXT) xinclude \
	    '$(top_srcdir)/test/XInclude/docs/*' \
	    '$(top_srcdir)/test/XInclude/without-reader/*'
	@touch seed/xinclude.stamp

xinclude_SOURCES = xinclude.c fuzz.c
xinclude_LDFLAGS = $(AM_LDFLAGS) -fsanitize=fuzzer

fuzz-xinclude: xinclude$(EXEEXT) seed/xinclude.stamp
	@mkdir -p corpus/xinclude
	./xinclude$(EXEEXT) \
	    -dict=xml.dict \
	    -max_len=$(XML_MAX_LEN) \
	    -timeout=20 \
	    corpus/xinclude seed/xinclude

# HTML fuzzer

seed/html.stamp: genSeed$(EXEEXT)
	@mkdir -p seed/html
	./genSeed$(EXEEXT) html '$(top_srcdir)/test/HTML/*'
	@touch seed/html.stamp

html_SOURCES = html.c fuzz.c
html_LDFLAGS = $(AM_LDFLAGS) -fsanitize=fuzzer

fuzz-html: html$(EXEEXT) seed/html.stamp
	@mkdir -p corpus/html
	./html$(EXEEXT) \
	    -dict=html.dict \
	    -max_len=1000000 \
	    -timeout=10 \
	    corpus/html seed/html

# Regexp fuzzer

seed/regexp.stamp:
	@mkdir -p seed/regexp
	cp -r $(srcdir)/static_seed/regexp seed
	@touch seed/regexp.stamp

regexp_SOURCES = regexp.c fuzz.c
regexp_LDFLAGS = $(AM_LDFLAGS) -fsanitize=fuzzer

fuzz-regexp: regexp$(EXEEXT) seed/regexp.stamp
	@mkdir -p corpus/regexp
	./regexp$(EXEEXT) \
	    -dict=regexp.dict \
	    -max_len=200 \
	    -timeout=5 \
	    corpus/regexp seed/regexp

# URI fuzzer

seed/uri.stamp:
	@mkdir -p seed/uri
	cp -r $(srcdir)/static_seed/uri seed
	@touch seed/uri.stamp

uri_SOURCES = uri.c fuzz.c
uri_LDFLAGS = $(AM_LDFLAGS) -fsanitize=fuzzer

fuzz-uri: uri$(EXEEXT) seed/uri.stamp
	@mkdir -p corpus/uri
	./uri$(EXEEXT) \
	    -max_len=10000 \
	    -timeout=2 \
	    corpus/uri seed/uri

# XML Schema fuzzer

seed/schema.stamp: genSeed$(EXEEXT)
	@mkdir -p seed/schema
	./genSeed$(EXEEXT) schema '$(top_srcdir)/test/schemas/*.xsd'
	@touch seed/schema.stamp

schema_SOURCES = schema.c fuzz.c
schema_LDFLAGS = $(AM_LDFLAGS) -fsanitize=fuzzer

fuzz-schema: schema$(EXEEXT) seed/schema.stamp
	@mkdir -p corpus/schema
	./schema$(EXEEXT) \
	    -dict=schema.dict \
	    -max_len=$(XML_MAX_LEN) \
	    -timeout=20 \
	    corpus/schema seed/schema

# XPath fuzzer

seed/xpath.stamp: genSeed$(EXEEXT)
	@mkdir -p seed/xpath
	./genSeed$(EXEEXT) xpath '$(top_srcdir)/test/XPath'
	@touch seed/xpath.stamp

xpath_SOURCES = xpath.c fuzz.c
xpath_LDFLAGS = $(AM_LDFLAGS) -fsanitize=fuzzer

fuzz-xpath: xpath$(EXEEXT) seed/xpath.stamp
	@mkdir -p corpus/xpath
	./xpath$(EXEEXT) \
	    -dict=xpath.dict \
	    -max_len=10000 \
	    -timeout=20 \
	    corpus/xpath seed/xpath

