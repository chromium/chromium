AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/libxslt

lib_LTLIBRARIES = libxslt.la

xsltincdir = $(includedir)/libxslt

xsltinc_HEADERS = 			\
	xslt.h				\
	xsltutils.h			\
	pattern.h			\
	templates.h			\
	variables.h			\
	keys.h				\
	numbersInternals.h		\
	extensions.h			\
	extra.h				\
	functions.h			\
	namespaces.h			\
	imports.h			\
	attributes.h			\
	documents.h			\
	preproc.h			\
	transform.h			\
	security.h			\
	xsltInternals.h			\
	xsltexports.h			\
	xsltlocale.h
nodist_xsltinc_HEADERS = 		\
	xsltconfig.h

libxslt_la_SOURCES = 			\
	attrvt.c			\
	xslt.c				\
	xsltlocale.c			\
	xsltutils.c			\
	pattern.c			\
	templates.c			\
	variables.c			\
	keys.c				\
	numbers.c			\
	extensions.c			\
	extra.c				\
	functions.c			\
	namespaces.c			\
	imports.c			\
	attributes.c			\
	documents.c			\
	preproc.c			\
	transform.c			\
	security.c			\
	win32config.h			\
	libxslt.h

if USE_VERSION_SCRIPT
LIBXSLT_VERSION_SCRIPT = $(VERSION_SCRIPT_FLAGS)$(srcdir)/libxslt.syms
else
LIBXSLT_VERSION_SCRIPT =
endif

libxslt_la_LIBADD = $(LIBXML_LIBS) $(EXTRA_LIBS) $(LIBM)
libxslt_la_LDFLAGS =					\
		$(AM_LDFLAGS) -no-undefined		\
		$(LIBXSLT_VERSION_SCRIPT)		\
		-version-info $(LIBXSLT_VERSION_INFO)

man_MANS = libxslt.3

EXTRA_DIST = $(man_MANS) trio.h triodef.h libxslt.syms

xsltproc: all
	@(cd ../xsltproc ; $(MAKE))

install-exec-hook:
	$(MKDIR_P) "$(DESTDIR)$(libdir)/libxslt-plugins"
