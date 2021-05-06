/*
 * testFuzzer.c: Test program for the custom entity loader used to fuzz
 * with multiple inputs.
 *
 * See Copyright for the status of this software.
 */

#include <string.h>
#include <glob.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include "fuzz.h"

#ifdef HAVE_HTML_FUZZER
  #define LLVMFuzzerInitialize fuzzHtmlInit
  #define LLVMFuzzerTestOneInput fuzzHtml
  #include "html.c"
  #undef LLVMFuzzerInitialize
  #undef LLVMFuzzerTestOneInput
#endif

#ifdef HAVE_REGEXP_FUZZER
  #define LLVMFuzzerInitialize fuzzRegexpInit
  #define LLVMFuzzerTestOneInput fuzzRegexp
  #include "regexp.c"
  #undef LLVMFuzzerInitialize
  #undef LLVMFuzzerTestOneInput
#endif

#ifdef HAVE_SCHEMA_FUZZER
  #define LLVMFuzzerInitialize fuzzSchemaInit
  #define LLVMFuzzerTestOneInput fuzzSchema
  #include "schema.c"
  #undef LLVMFuzzerInitialize
  #undef LLVMFuzzerTestOneInput
#endif

#ifdef HAVE_URI_FUZZER
  #define LLVMFuzzerInitialize fuzzUriInit
  #define LLVMFuzzerTestOneInput fuzzUri
  #include "uri.c"
  #undef LLVMFuzzerInitialize
  #undef LLVMFuzzerTestOneInput
#endif

#ifdef HAVE_XML_FUZZER
  #define LLVMFuzzerInitialize fuzzXmlInit
  #define LLVMFuzzerTestOneInput fuzzXml
  #include "xml.c"
  #undef LLVMFuzzerInitialize
  #undef LLVMFuzzerTestOneInput
#endif

#ifdef HAVE_XPATH_FUZZER
  #define LLVMFuzzerInitialize fuzzXPathInit
  #define LLVMFuzzerTestOneInput fuzzXPath
  #include "xpath.c"
  #undef LLVMFuzzerInitialize
  #undef LLVMFuzzerTestOneInput
#endif

typedef int
(*initFunc)(int *argc, char ***argv);
typedef int
(*fuzzFunc)(const char *data, size_t size);

int numInputs;

static int
testFuzzer(initFunc init, fuzzFunc fuzz, const char *pattern) {
    glob_t globbuf;
    int ret = -1;
    int i;

    if (glob(pattern, 0, NULL, &globbuf) != 0) {
        fprintf(stderr, "pattern %s matches no files\n", pattern);
        return(-1);
    }

    if (init != NULL)
        init(NULL, NULL);

    for (i = 0; i < globbuf.gl_pathc; i++) {
        const char *path = globbuf.gl_pathv[i];
        char *data;
        size_t size;

        data = xmlSlurpFile(path, &size);
        if (data == NULL) {
            fprintf(stderr, "couldn't read %s\n", path);
            goto error;
        }
        fuzz(data, size);
        xmlFree(data);

        numInputs++;
    }

    ret = 0;
error:
    globfree(&globbuf);
    return(ret);
}

#ifdef HAVE_XML_FUZZER
static int
testEntityLoader() {
    static const char data[] =
        "doc.xml\\\n"
        "<!DOCTYPE doc SYSTEM \"doc.dtd\">\n"
        "<doc>&ent;</doc>\\\n"
        "doc.dtd\\\n"
        "<!ELEMENT doc (#PCDATA)>\n"
        "<!ENTITY ent SYSTEM \"ent.txt\">\\\n"
        "ent.txt\\\n"
        "Hello, world!\\\n";
    static xmlChar expected[] =
        "<?xml version=\"1.0\"?>\n"
        "<!DOCTYPE doc SYSTEM \"doc.dtd\">\n"
        "<doc>Hello, world!</doc>\n";
    const char *docBuffer;
    size_t docSize;
    xmlDocPtr doc;
    xmlChar *out;
    int ret = 0;

    xmlSetExternalEntityLoader(xmlFuzzEntityLoader);

    xmlFuzzDataInit(data, sizeof(data) - 1);
    xmlFuzzReadEntities();
    docBuffer = xmlFuzzMainEntity(&docSize);
    doc = xmlReadMemory(docBuffer, docSize, NULL, NULL,
                        XML_PARSE_NOENT | XML_PARSE_DTDLOAD);

    xmlDocDumpMemory(doc, &out, NULL);
    if (xmlStrcmp(out, expected) != 0) {
        fprintf(stderr, "Expected:\n%sGot:\n%s", expected, out);
        ret = 1;
    }

    xmlFree(out);
    xmlFreeDoc(doc);
    xmlFuzzDataCleanup();

    return(ret);
}
#endif

int
main() {
    int ret = 0;

#ifdef HAVE_XML_FUZZER
    if (testEntityLoader() != 0)
        ret = 1;
#endif
#ifdef HAVE_HTML_FUZZER
    if (testFuzzer(fuzzHtmlInit, fuzzHtml, "seed/html/*") != 0)
        ret = 1;
#endif
#ifdef HAVE_REGEXP_FUZZER
    if (testFuzzer(fuzzRegexpInit, fuzzRegexp, "seed/regexp/*") != 0)
        ret = 1;
#endif
#ifdef HAVE_SCHEMA_FUZZER
    if (testFuzzer(fuzzSchemaInit, fuzzSchema, "seed/schema/*") != 0)
        ret = 1;
#endif
#ifdef HAVE_URI_FUZZER
    if (testFuzzer(NULL, fuzzUri, "seed/uri/*") != 0)
        ret = 1;
#endif
#ifdef HAVE_XML_FUZZER
    if (testFuzzer(fuzzXmlInit, fuzzXml, "seed/xml/*") != 0)
        ret = 1;
#endif
#ifdef HAVE_XPATH_FUZZER
    if (testFuzzer(fuzzXPathInit, fuzzXPath, "seed/xpath/*") != 0)
        ret = 1;
#endif

    if (ret == 0)
        printf("Successfully tested %d inputs\n", numInputs);

    return(ret);
}

