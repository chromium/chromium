/*
 * fuzz.h: Common functions and macros for fuzzing.
 *
 * See Copyright for the status of this software.
 */

#ifndef __XML_FUZZERCOMMON_H__
#define __XML_FUZZERCOMMON_H__

#include <stddef.h>
#include <stdio.h>
#include <libxml/parser.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(LIBXML_HTML_ENABLED) && defined(LIBXML_OUTPUT_ENABLED)
  #define HAVE_HTML_FUZZER
#endif
#if defined(LIBXML_REGEXP_ENABLED)
  #define HAVE_REGEXP_FUZZER
#endif
#if defined(LIBXML_SCHEMAS_ENABLED)
  #define HAVE_SCHEMA_FUZZER
#endif
#if 1
  #define HAVE_URI_FUZZER
#endif
#if defined(LIBXML_VALID_ENABLED) && \
    defined(LIBXML_READER_ENABLED)
  #define HAVE_VALID_FUZZER
#endif
#if defined(LIBXML_XINCLUDE_ENABLED) && \
    defined(LIBXML_READER_ENABLED)
  #define HAVE_XINCLUDE_FUZZER
#endif
#if defined(LIBXML_OUTPUT_ENABLED) && \
    defined(LIBXML_READER_ENABLED)
  #define HAVE_XML_FUZZER
#endif
#if defined(LIBXML_XPATH_ENABLED)
  #define HAVE_XPATH_FUZZER
#endif

int
LLVMFuzzerInitialize(int *argc, char ***argv);

int
LLVMFuzzerTestOneInput(const char *data, size_t size);

void
xmlFuzzErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg ATTRIBUTE_UNUSED,
                 ...);

void
xmlFuzzMemSetup(void);

void
xmlFuzzMemSetLimit(size_t limit);

void
xmlFuzzDataInit(const char *data, size_t size);

void
xmlFuzzDataCleanup(void);

void
xmlFuzzWriteInt(FILE *out, size_t v, int size);

size_t
xmlFuzzReadInt(int size);

const char *
xmlFuzzReadRemaining(size_t *size);

void
xmlFuzzWriteString(FILE *out, const char *str);

const char *
xmlFuzzReadString(size_t *size);

void
xmlFuzzReadEntities(void);

const char *
xmlFuzzMainUrl(void);

const char *
xmlFuzzMainEntity(size_t *size);

xmlParserInputPtr
xmlFuzzEntityLoader(const char *URL, const char *ID, xmlParserCtxtPtr ctxt);

char *
xmlSlurpFile(const char *path, size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* __XML_FUZZERCOMMON_H__ */

