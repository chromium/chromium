/*
 * testparser.c: Additional parser tests
 *
 * See Copyright for the status of this software.
 */

#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>
#include <libxml/HTMLparser.h>

#include <string.h>

static int
testStandaloneWithEncoding(void) {
    xmlDocPtr doc;
    const char *str =
        "<?xml version=\"1.0\" standalone=\"yes\"?>\n"
        "<doc></doc>\n";
    int err = 0;

    xmlResetLastError();

    doc = xmlReadDoc(BAD_CAST str, NULL, "UTF-8", 0);
    if (doc == NULL) {
        fprintf(stderr, "xmlReadDoc failed\n");
        err = 1;
    }
    xmlFreeDoc(doc);

    return err;
}

static int
testUnsupportedEncoding(void) {
    xmlDocPtr doc;
    const xmlError *error;
    int err = 0;

    xmlResetLastError();

    doc = xmlReadDoc(BAD_CAST "<doc/>", NULL, "#unsupported",
                     XML_PARSE_NOWARNING);
    if (doc == NULL) {
        fprintf(stderr, "xmlReadDoc failed with unsupported encoding\n");
        err = 1;
    }
    xmlFreeDoc(doc);

    error = xmlGetLastError();
    if (error->code != XML_ERR_UNSUPPORTED_ENCODING ||
        error->level != XML_ERR_WARNING ||
        strcmp(error->message, "Unsupported encoding: #unsupported\n") != 0)
    {
        fprintf(stderr, "xmlReadDoc failed to raise correct error\n");
        err = 1;
    }

    return err;
}

static int
testNodeGetContent(void) {
    xmlDocPtr doc;
    xmlChar *content;
    int err = 0;

    doc = xmlReadDoc(BAD_CAST "<doc/>", NULL, NULL, 0);
    xmlAddChild(doc->children, xmlNewReference(doc, BAD_CAST "lt"));
    content = xmlNodeGetContent((xmlNodePtr) doc);
    if (strcmp((char *) content, "<") != 0) {
        fprintf(stderr, "xmlNodeGetContent failed\n");
        err = 1;
    }
    xmlFree(content);
    xmlFreeDoc(doc);

    return err;
}

#ifdef LIBXML_SAX1_ENABLED
static int
testBalancedChunk(void) {
    xmlNodePtr list;
    xmlNodePtr elem;
    int ret;
    int err = 0;

    ret = xmlParseBalancedChunkMemory(NULL, NULL, NULL, 0,
            BAD_CAST "start <node xml:lang='en'>abc</node> end", &list);

    if ((ret != XML_ERR_OK) ||
        (list == NULL) ||
        ((elem = list->next) == NULL) ||
        (elem->type != XML_ELEMENT_NODE) ||
        (elem->nsDef == NULL) ||
        (!xmlStrEqual(elem->nsDef->href, XML_XML_NAMESPACE))) {
        fprintf(stderr, "xmlParseBalancedChunkMemory failed\n");
        err = 1;
    }

    xmlFreeNodeList(list);

    return(err);
}
#endif

#ifdef LIBXML_PUSH_ENABLED
static int
testHugePush(void) {
    xmlParserCtxtPtr ctxt;
    int err, i;

    ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL);

    /*
     * Push parse a document larger than XML_MAX_LOOKUP_LIMIT
     * (10,000,000 bytes). This mainly tests whether shrinking the
     * buffer works when push parsing.
     */
    xmlParseChunk(ctxt, "<doc>", 5, 0);
    for (i = 0; i < 1000000; i++)
        xmlParseChunk(ctxt, "<elem>text</elem>", 17, 0);
    xmlParseChunk(ctxt, "</doc>", 6, 1);

    err = ctxt->wellFormed ? 0 : 1;
    xmlFreeDoc(ctxt->myDoc);
    xmlFreeParserCtxt(ctxt);

    return err;
}

static int
testHugeEncodedChunk(void) {
    xmlBufferPtr buf;
    xmlChar *chunk;
    xmlParserCtxtPtr ctxt;
    int err, i;

    /*
     * Test the push parser with a built-in encoding handler like ISO-8859-1
     * and a chunk larger than the initial decoded buffer (currently 4 KB).
     */
    buf = xmlBufferCreate();
    xmlBufferCat(buf,
            BAD_CAST "<?xml version='1.0' encoding='ISO-8859-1'?>\n");
    xmlBufferCat(buf, BAD_CAST "<doc><!-- ");
    for (i = 0; i < 2000; i++)
        xmlBufferCat(buf, BAD_CAST "0123456789");
    xmlBufferCat(buf, BAD_CAST " --></doc>");
    chunk = xmlBufferDetach(buf);
    xmlBufferFree(buf);

    ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL);

    xmlParseChunk(ctxt, (char *) chunk, xmlStrlen(chunk), 0);
    xmlParseChunk(ctxt, NULL, 0, 1);

    err = ctxt->wellFormed ? 0 : 1;
    xmlFreeDoc(ctxt->myDoc);
    xmlFreeParserCtxt(ctxt);
    xmlFree(chunk);

    return err;
}

#ifdef LIBXML_HTML_ENABLED
static int
testHtmlPushWithEncoding(void) {
    htmlParserCtxtPtr ctxt;
    htmlDocPtr doc;
    htmlNodePtr node;
    int err = 0;

    ctxt = htmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL,
                                    XML_CHAR_ENCODING_UTF8);
    htmlParseChunk(ctxt, "-\xC3\xA4-", 4, 1);

    doc = ctxt->myDoc;
    if (!xmlStrEqual(doc->encoding, BAD_CAST "UTF-8")) {
        fprintf(stderr, "testHtmlPushWithEncoding failed\n");
        err = 1;
    }

    node = xmlDocGetRootElement(doc)->children->children->children;
    if (!xmlStrEqual(node->content, BAD_CAST "-\xC3\xA4-")) {
        fprintf(stderr, "testHtmlPushWithEncoding failed\n");
        err = 1;
    }

    xmlFreeDoc(doc);
    htmlFreeParserCtxt(ctxt);
    return err;
}
#endif
#endif

#ifdef LIBXML_READER_ENABLED
static int
testReaderEncoding(void) {
    xmlBuffer *buf;
    xmlTextReader *reader;
    xmlChar *xml;
    const xmlChar *encoding;
    int err = 0;
    int i;

    buf = xmlBufferCreate();
    xmlBufferCCat(buf, "<?xml version='1.0' encoding='ISO-8859-1'?>\n");
    xmlBufferCCat(buf, "<doc>");
    for (i = 0; i < 8192; i++)
        xmlBufferCCat(buf, "x");
    xmlBufferCCat(buf, "</doc>");
    xml = xmlBufferDetach(buf);
    xmlBufferFree(buf);

    reader = xmlReaderForDoc(BAD_CAST xml, NULL, NULL, 0);
    xmlTextReaderRead(reader);
    encoding = xmlTextReaderConstEncoding(reader);

    if (!xmlStrEqual(encoding, BAD_CAST "ISO-8859-1")) {
        fprintf(stderr, "testReaderEncoding failed\n");
        err = 1;
    }

    xmlFreeTextReader(reader);
    xmlFree(xml);
    return err;
}

static int
testReaderContent(void) {
    xmlTextReader *reader;
    const xmlChar *xml = BAD_CAST "<d>x<e>y</e><f>z</f></d>";
    xmlChar *string;
    int err = 0;

    reader = xmlReaderForDoc(xml, NULL, NULL, 0);
    xmlTextReaderRead(reader);

    string = xmlTextReaderReadOuterXml(reader);
    if (!xmlStrEqual(string, xml)) {
        fprintf(stderr, "xmlTextReaderReadOuterXml failed\n");
        err = 1;
    }
    xmlFree(string);

    string = xmlTextReaderReadInnerXml(reader);
    if (!xmlStrEqual(string, BAD_CAST "x<e>y</e><f>z</f>")) {
        fprintf(stderr, "xmlTextReaderReadInnerXml failed\n");
        err = 1;
    }
    xmlFree(string);

    string = xmlTextReaderReadString(reader);
    if (!xmlStrEqual(string, BAD_CAST "xyz")) {
        fprintf(stderr, "xmlTextReaderReadString failed\n");
        err = 1;
    }
    xmlFree(string);

    xmlFreeTextReader(reader);
    return err;
}

#ifdef LIBXML_XINCLUDE_ENABLED
typedef struct {
    char *message;
    int code;
} testReaderErrorCtxt;

static void
testReaderError(void *arg, const char *msg,
                xmlParserSeverities severity ATTRIBUTE_UNUSED,
                xmlTextReaderLocatorPtr locator ATTRIBUTE_UNUSED) {
    testReaderErrorCtxt *ctxt = arg;

    if (ctxt->message != NULL)
        xmlFree(ctxt->message);
    ctxt->message = xmlMemStrdup(msg);
}

static void
testStructuredReaderError(void *arg, const xmlError *error) {
    testReaderErrorCtxt *ctxt = arg;

    if (ctxt->message != NULL)
        xmlFree(ctxt->message);
    ctxt->message = xmlMemStrdup(error->message);
    ctxt->code = error->code;
}

static int
testReaderXIncludeError(void) {
    /*
     * Test whether XInclude errors are reported to the custom error
     * handler of a reader.
     */
    const char *doc =
        "<doc xmlns:xi='http://www.w3.org/2001/XInclude'>\n"
        "  <xi:include/>\n"
        "</doc>\n";
    xmlTextReader *reader;
    testReaderErrorCtxt errorCtxt;
    int err = 0;

    reader = xmlReaderForDoc(BAD_CAST doc, NULL, NULL, XML_PARSE_XINCLUDE);
    xmlTextReaderSetErrorHandler(reader, testReaderError, &errorCtxt);
    errorCtxt.message = NULL;
    errorCtxt.code = 0;
    while (xmlTextReaderRead(reader) > 0)
        ;

    if (errorCtxt.message == NULL ||
        strstr(errorCtxt.message, "href or xpointer") == NULL) {
        fprintf(stderr, "xmlTextReaderSetErrorHandler failed\n");
        err = 1;
    }

    xmlFree(errorCtxt.message);
    xmlFreeTextReader(reader);

    reader = xmlReaderForDoc(BAD_CAST doc, NULL, NULL, XML_PARSE_XINCLUDE);
    xmlTextReaderSetStructuredErrorHandler(reader, testStructuredReaderError,
                                           &errorCtxt);
    errorCtxt.message = NULL;
    errorCtxt.code = 0;
    while (xmlTextReaderRead(reader) > 0)
        ;

    if (errorCtxt.code != XML_XINCLUDE_NO_HREF ||
        errorCtxt.message == NULL ||
        strstr(errorCtxt.message, "href or xpointer") == NULL) {
        fprintf(stderr, "xmlTextReaderSetStructuredErrorHandler failed\n");
        err = 1;
    }

    xmlFree(errorCtxt.message);
    xmlFreeTextReader(reader);

    return err;
}
#endif
#endif

#ifdef LIBXML_WRITER_ENABLED
static int
testWriterIOWrite(void *ctxt, const char *data, int len) {
    (void) ctxt;
    (void) data;

    return len;
}

static int
testWriterIOClose(void *ctxt) {
    (void) ctxt;

    return XML_IO_ENAMETOOLONG;
}

static int
testWriterClose(void){
    xmlOutputBufferPtr out;
    xmlTextWriterPtr writer;
    int err = 0;
    int result;

    out = xmlOutputBufferCreateIO(testWriterIOWrite, testWriterIOClose,
                                  NULL, NULL);
    writer = xmlNewTextWriter(out);
    xmlTextWriterStartDocument(writer, "1.0", "UTF-8", NULL);
    xmlTextWriterStartElement(writer, BAD_CAST "elem");
    xmlTextWriterEndElement(writer);
    xmlTextWriterEndDocument(writer);
    result = xmlTextWriterClose(writer);

    if (result != XML_IO_ENAMETOOLONG) {
        fprintf(stderr, "xmlTextWriterClose reported wrong error %d\n",
                result);
        err = 1;
    }

    xmlFreeTextWriter(writer);
    return err;
}
#endif

int
main(void) {
    int err = 0;

    err |= testStandaloneWithEncoding();
    err |= testUnsupportedEncoding();
    err |= testNodeGetContent();
#ifdef LIBXML_SAX1_ENABLED
    err |= testBalancedChunk();
#endif
#ifdef LIBXML_PUSH_ENABLED
    err |= testHugePush();
    err |= testHugeEncodedChunk();
#ifdef LIBXML_HTML_ENABLED
    err |= testHtmlPushWithEncoding();
#endif
#endif
#ifdef LIBXML_READER_ENABLED
    err |= testReaderEncoding();
    err |= testReaderContent();
#ifdef LIBXML_XINCLUDE_ENABLED
    err |= testReaderXIncludeError();
#endif
#endif
#ifdef LIBXML_WRITER_ENABLED
    err |= testWriterClose();
#endif

    return err;
}

