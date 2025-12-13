/*
 * testparser.c: Additional parser tests
 *
 * See Copyright for the status of this software.
 */

#define XML_DEPRECATED

#include "libxml.h"
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/uri.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlsave.h>
#include <libxml/xmlwriter.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>

#include <string.h>

#ifdef LIBXML_SAX1_ENABLED
static void
ignoreError(void *ctxt ATTRIBUTE_UNUSED,
            const xmlError *error ATTRIBUTE_UNUSED) {
}
#endif

static int
testNewDocNode(void) {
    xmlNodePtr node;
    int err = 0;

    node = xmlNewDocNode(NULL, NULL, BAD_CAST "c", BAD_CAST "");
    if (node->children != NULL) {
        fprintf(stderr, "empty node has children\n");
        err = 1;
    }
    xmlFreeNode(node);

    return err;
}

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
    if (error == NULL ||
        error->code != XML_ERR_UNSUPPORTED_ENCODING ||
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

static int
testCFileIO(void) {
    xmlDocPtr doc;
    int err = 0;

    /* Deprecated FILE-based API */
    xmlRegisterInputCallbacks(xmlFileMatch, xmlFileOpen, xmlFileRead,
                              xmlFileClose);
    doc = xmlReadFile("test/ent1", NULL, 0);

    if (doc == NULL) {
        err = 1;
    } else {
        xmlNodePtr root = xmlDocGetRootElement(doc);

        if (root == NULL || !xmlStrEqual(root->name, BAD_CAST "EXAMPLE"))
            err = 1;
    }

    xmlFreeDoc(doc);
    xmlPopInputCallbacks();

    if (err)
        fprintf(stderr, "xmlReadFile failed with FILE input callbacks\n");

    return err;
}

/*
 * The exact rules when undeclared entities are a fatal error
 * depend on some conditions that aren't recovered from the
 * context document when parsing XML content. This test case
 * demonstrates such an asymmetry.
 */
static int
testUndeclEntInContent(void) {
    const char xml[] = "<!DOCTYPE doc SYSTEM 'my.dtd'><doc>&undecl;</doc>";
    const char content[] = "<doc>&undecl;</doc>";
    xmlDocPtr doc;
    xmlNodePtr root, list;
    int options = XML_PARSE_NOENT | XML_PARSE_NOERROR;
    int err = 0;
    int res;

    /* Parsing the document succeeds because of the external DTD. */
    doc = xmlReadDoc(BAD_CAST xml, NULL, NULL, options);
    root = xmlDocGetRootElement(doc);

    /* Parsing content fails. */

    res = xmlParseInNodeContext(root, content, sizeof(content) - 1, options,
                                &list);
    if (res != XML_ERR_UNDECLARED_ENTITY || list != NULL) {
        fprintf(stderr, "Wrong result from xmlParseInNodeContext\n");
        err = 1;
    }
    xmlFreeNodeList(list);

#ifdef LIBXML_SAX1_ENABLED
    xmlSetStructuredErrorFunc(NULL, ignoreError);
    res = xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, BAD_CAST content,
                                      &list);
    if (res != XML_ERR_UNDECLARED_ENTITY || list != NULL) {
        fprintf(stderr, "Wrong result from xmlParseBalancedChunkMemory\n");
        err = 1;
    }
    xmlFreeNodeList(list);
    xmlSetStructuredErrorFunc(NULL, NULL);
#endif /* LIBXML_SAX1_ENABLED */

    xmlFreeDoc(doc);

    return err;
}

static int
testInvalidCharRecovery(void) {
    const char *xml = "<doc>&#x10;</doc>";
    xmlDoc *doc;
    int err = 0;

    doc = xmlReadDoc(BAD_CAST xml, NULL, NULL,
                     XML_PARSE_RECOVER | XML_PARSE_NOERROR);

    if (strcmp((char *) doc->children->children->content, "\x10") != 0) {
        fprintf(stderr, "Failed to recover from invalid char ref\n");
        err = 1;
    }

    xmlFreeDoc(doc);

    return err;
}

static void
testCtxtInputGetterError(void *errCtxt, const xmlError *error) {
    int *err = errCtxt;
    xmlParserCtxt *ctxt = error->ctxt;
    const char *filename;
    int line, col;
    unsigned long bytePos;
    const xmlChar *start;
    int size, offset;

    xmlCtxtGetInputPosition(ctxt, 0, &filename, &line, &col, &bytePos);

    if (strcmp(filename, "test.xml") != 0 ||
        line != 4 || col != 11 || bytePos != 62) {
        fprintf(stderr, "unexpected position: %s %d %d %lu\n",
                filename, line, col, bytePos);
        *err = 1;
    }

    size = 80;
    xmlCtxtGetInputWindow(ctxt, 0, &start, &size, &offset);

    if (strncmp((char *) start, "<doc>&ent;", 10) != 0 ||
        size != 16 || offset != 10) {
        fprintf(stderr, "unexpected window: %.10s %d %d\n",
                start, size, offset);
        *err = 1;
    }

    xmlCtxtGetInputPosition(ctxt, -1, &filename, &line, &col, &bytePos);

    if (filename != NULL ||
        line != 1 || col != 11 || bytePos != 10) {
        fprintf(stderr, "unexpected position: %s %d %d %lu\n",
                filename, line, col, bytePos);
        *err = 1;
    }

    size = 80;
    xmlCtxtGetInputWindow(ctxt, -1, &start, &size, &offset);

    if (strncmp((char *) start, "xxx &fail;", 10) != 0 ||
        size != 14 || offset != 10) {
        fprintf(stderr, "unexpected window: %.10s %d %d\n",
                start, size, offset);
        *err = 1;
    }
}

static int
testCtxtInputGetters(void) {
    const char *xml =
        "<!DOCTYPE doc [\n"
        "  <!ENTITY ent 'xxx &fail; xxx'>\n"
        "]>\n"
        "<doc>&ent;</doc>\n";
    xmlParserCtxt *ctxt;
    xmlDoc *doc;
    int err = 0;

    ctxt = xmlNewParserCtxt();
    xmlCtxtSetErrorHandler(ctxt, testCtxtInputGetterError, &err);
    doc = xmlCtxtReadDoc(ctxt, BAD_CAST xml, "test.xml", NULL, 0);
    xmlFreeDoc(doc);
    xmlFreeParserCtxt(ctxt);

    return err;
}

#ifdef LIBXML_VALID_ENABLED
static void
testSwitchDtdExtSubset(void *vctxt, const xmlChar *name ATTRIBUTE_UNUSED,
                       const xmlChar *externalId ATTRIBUTE_UNUSED,
                       const xmlChar *systemId ATTRIBUTE_UNUSED) {
    xmlParserCtxtPtr ctxt = vctxt;

    ctxt->myDoc->extSubset = ctxt->_private;
}

static int
testSwitchDtd(void) {
    const char dtdContent[] =
        "<!ENTITY test '<elem1/><elem2/>'>\n";
    const char docContent[] =
        "<!DOCTYPE doc SYSTEM 'entities.dtd'>\n"
        "<doc>&test;</doc>\n";
    xmlParserInputBufferPtr input;
    xmlParserCtxtPtr ctxt;
    xmlDtdPtr dtd;
    xmlDocPtr doc;
    xmlEntityPtr ent;
    int err = 0;

    input = xmlParserInputBufferCreateStatic(dtdContent,
                                             sizeof(dtdContent) - 1,
                                             XML_CHAR_ENCODING_NONE);
    dtd = xmlIOParseDTD(NULL, input, XML_CHAR_ENCODING_NONE);

    ctxt = xmlNewParserCtxt();
    ctxt->_private = dtd;
    ctxt->sax->externalSubset = testSwitchDtdExtSubset;
    doc = xmlCtxtReadMemory(ctxt, docContent, sizeof(docContent) - 1, NULL,
                            NULL, XML_PARSE_NOENT | XML_PARSE_DTDLOAD);
    xmlFreeParserCtxt(ctxt);

    ent = xmlGetDocEntity(doc, BAD_CAST "test");
    if (ent->children->doc != NULL) {
        fprintf(stderr, "Entity content should have NULL doc\n");
        err = 1;
    }

    /* Free doc before DTD */
    doc->extSubset = NULL;
    xmlFreeDoc(doc);
    xmlFreeDtd(dtd);

    return err;
}
#endif /* LIBXML_VALID_ENABLED */

#ifdef LIBXML_OUTPUT_ENABLED
static xmlChar *
dumpNodeList(xmlNodePtr list) {
    xmlBufferPtr buffer;
    xmlSaveCtxtPtr save;
    xmlNodePtr cur;
    xmlChar *ret;

    buffer = xmlBufferCreate();
    save = xmlSaveToBuffer(buffer, "UTF-8", 0);
    for (cur = list; cur != NULL; cur = cur->next)
        xmlSaveTree(save, cur);
    xmlSaveClose(save);

    ret = xmlBufferDetach(buffer);
    xmlBufferFree(buffer);
    return ret;
}

static int
testCtxtParseContent(void) {
    xmlParserCtxtPtr ctxt;
    xmlParserInputPtr input;
    xmlDocPtr doc;
    xmlNodePtr node, list;
    const char *content;
    xmlChar *output;
    int i, j;
    int err = 0;

    static const char *const tests[] = {
        "<!-- c -->\xF0\x9F\x98\x84<a/><b/>end",
        "text<a:foo><b:foo/></a:foo>text<!-- c -->"
    };

    doc = xmlReadDoc(BAD_CAST "<doc xmlns:a='a'><elem xmlns:b='b'/></doc>",
                     NULL, NULL, 0);
    node = doc->children->children;

    ctxt = xmlNewParserCtxt();

    for (i = 0; (size_t) i < sizeof(tests) / sizeof(tests[0]); i++) {
        content = tests[i];

        for (j = 0; j < 2; j++) {
            if (j == 0) {
                input = xmlNewInputFromString(NULL, content,
                                              XML_INPUT_BUF_STATIC);
                list = xmlCtxtParseContent(ctxt, input, node, 0);
            } else {
                xmlParseInNodeContext(node, content, strlen(content), 0,
                                      &list);
            }

            output = dumpNodeList(list);

            if ((j == 0 && ctxt->nsWellFormed == 0) ||
                strcmp((char *) output, content) != 0) {
                fprintf(stderr, "%s failed test %d, got:\n%s\n",
                        j == 0 ?
                            "xmlCtxtParseContent" :
                            "xmlParseInNodeContext",
                        i, output);
                err = 1;
            }

            xmlFree(output);
            xmlFreeNodeList(list);
        }
    }

    xmlFreeParserCtxt(ctxt);
    xmlFreeDoc(doc);

    return err;
}

static int
testNoBlanks(void) {
    const xmlChar xml[] =
        "<refentry>\n"
        "  <refsect1>\n"
        "    <para>\n"
        "      Run <command>tester --help</command> for more options.\n"
        "    </para>\n"
        "  </refsect1>\n"
        "</refentry>\n";
    const xmlChar expect[] =
        "<?xml version=\"1.0\"?>\n"
        "<refentry><refsect1><para>\n"
        "      Run <command>tester --help</command> for more options.\n"
        "    </para></refsect1></refentry>\n";
    xmlDocPtr doc;
    xmlChar *out;
    int size;
    int err = 0;

    doc = xmlReadDoc(xml, NULL, NULL, XML_PARSE_NOBLANKS);
    xmlDocDumpMemory(doc, &out, &size);
    xmlFreeDoc(doc);

    if (!xmlStrEqual(out, expect)) {
        fprintf(stderr, "parsing with XML_PARSE_NOBLANKS failed\n");
        err = 1;
    }
    xmlFree(out);

    return err;
}

static int
testSaveNullEncDoc(const char *xml, const char *expect) {
    xmlDocPtr doc;
    xmlBufferPtr buffer;
    xmlSaveCtxtPtr save;
    const xmlChar *result;
    int err = 0;

    doc = xmlReadDoc(BAD_CAST xml, NULL, NULL, 0);

    buffer = xmlBufferCreate();
    save = xmlSaveToBuffer(buffer, NULL, 0);
    xmlSaveDoc(save, doc);
    xmlSaveClose(save);

    result = xmlBufferContent(buffer);
    if (strcmp((char *) result, expect) != 0) {
        fprintf(stderr, "xmlSave with NULL encodíng failed\n");
        err = 1;
    }

    xmlBufferFree(buffer);
    xmlFreeDoc(doc);

    return err;
}

static int
testSaveNullEnc(void) {
    int err = 0;

    err |= testSaveNullEncDoc(
        "<?xml version=\"1.0\"?><doc>\xC3\x98</doc>",
        "<?xml version=\"1.0\"?>\n<doc>&#xD8;</doc>\n");
    err |= testSaveNullEncDoc(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?><doc>\xC3\x98</doc>",
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<doc>\xC3\x98</doc>\n");
    err |= testSaveNullEncDoc(
        "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?><doc>\xD8</doc>",
        "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n<doc>\xD8</doc>\n");

    return err;
}

static int
testDocDumpFormatMemoryEnc(void) {
    const char *xml = "<doc>\xC3\x98</doc>";
    const char *expect =
        "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"
        "<doc>\xD8</doc>\n";
    xmlDocPtr doc;
    xmlChar *text;
    int len;
    int err = 0;

    doc = xmlReadDoc(BAD_CAST xml, NULL, NULL, 0);
    xmlDocDumpFormatMemoryEnc(doc, &text, &len, "iso-8859-1", 0);

    if (strcmp((char *) text, expect) != 0) {
        fprintf(stderr, "xmlDocDumpFormatMemoryEnc failed\n");
        err = 1;
    }

    xmlFree(text);
    xmlFreeDoc(doc);
    return err;
}
#endif /* LIBXML_OUTPUT_ENABLED */

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
    int err = 0, i;

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

    if (!ctxt->wellFormed)
        err = 1;
    xmlFreeDoc(ctxt->myDoc);
    xmlFreeParserCtxt(ctxt);
    xmlFree(chunk);

    /*
     * Test the push parser with
     *
     * - a single call to xmlParseChunk,
     * - a non-UTF8 encoding,
     * - a chunk larger then MINLEN (4000 bytes).
     *
     * This verifies that the whole buffer is processed in the initial
     * charset conversion.
     */
    buf = xmlBufferCreate();
    xmlBufferCat(buf,
            BAD_CAST "<?xml version='1.0' encoding='ISO-8859-1'?>\n");
    xmlBufferCat(buf, BAD_CAST "<doc>");
    /* 20,000 characters */
    for (i = 0; i < 2000; i++)
        xmlBufferCat(buf, BAD_CAST "0123456789");
    xmlBufferCat(buf, BAD_CAST "</doc>");
    chunk = xmlBufferDetach(buf);
    xmlBufferFree(buf);

    ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL);

    xmlParseChunk(ctxt, (char *) chunk, xmlStrlen(chunk), 1);

    if (!ctxt->wellFormed)
        err = 1;
    xmlFreeDoc(ctxt->myDoc);
    xmlFreeParserCtxt(ctxt);
    xmlFree(chunk);

    return err;
}

static int
testPushCDataEnd(void) {
    int err = 0;
    int k;

    for (k = 0; k < 4; k++) {
        xmlBufferPtr buf;
        xmlChar *chunk;
        xmlParserCtxtPtr ctxt;
        int i;

        ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL);
        xmlCtxtSetOptions(ctxt, XML_PARSE_NOERROR);

        /*
         * Push parse text data with ']]>' split across chunks.
         */
        buf = xmlBufferCreate();
        xmlBufferCCat(buf, "<doc>");

        /*
         * Also test xmlParseCharDataCopmlex
         */
        if (k & 1)
            xmlBufferCCat(buf, "x");
        else
            xmlBufferCCat(buf, "\xC3\xA4");

        /*
         * Create enough data to trigger a "characters" SAX callback.
         * (XML_PARSER_BIG_BUFFER_SIZE = 300)
         */
        for (i = 0; i < 2000; i++)
            xmlBufferCCat(buf, "x");

        if (k & 2)
            xmlBufferCCat(buf, "]");
        else
            xmlBufferCCat(buf, "]]");

        chunk = xmlBufferDetach(buf);
        xmlBufferFree(buf);

        xmlParseChunk(ctxt, (char *) chunk, xmlStrlen(chunk), 0);
        if (k & 2)
            xmlParseChunk(ctxt, "]>xxx</doc>", 11, 1);
        else
            xmlParseChunk(ctxt, ">xxx</doc>", 10, 1);

        if (ctxt->errNo != XML_ERR_MISPLACED_CDATA_END) {
            fprintf(stderr, "xmlParseChunk failed to detect CData end: %d\n",
                    ctxt->errNo);
            err = 1;
        }

        xmlFree(chunk);
        xmlFreeDoc(ctxt->myDoc);
        xmlFreeParserCtxt(ctxt);
    }

    return err;
}
#endif /* PUSH */

#ifdef LIBXML_HTML_ENABLED
static int
testHtmlIds(void) {
    const char *htmlContent =
        "<html><body><div id='myId'>Hello, World!</div></body></html>";
    htmlDocPtr doc;
    xmlAttrPtr node;

    doc = htmlReadDoc(BAD_CAST htmlContent, NULL, NULL, 0);
    if (doc == NULL) {
        fprintf(stderr, "could not parse HTML content\n");
        return 1;
    }

    node = xmlGetID(doc, BAD_CAST "myId");
    if (node == NULL) {
        fprintf(stderr, "xmlGetID doesn't work on HTML\n");
        return 1;
    }

    xmlFreeDoc(doc);
    return 0;
}

#define MHE "meta http-equiv=\"Content-Type\""

#ifdef LIBXML_OUTPUT_ENABLED
static int
testHtmlInsertMetaEncoding(void) {
    /* We currently require a head element to be present. */
    const char *html =
        "<html>"
        "<head></head>"
        "<body>text</body>"
        "</html>\n";
    const char *expect =
        "<html>"
        "<head><meta charset=\"utf-8\"></head>"
        "<body>text</body>"
        "</html>\n";
    htmlDocPtr doc;
    xmlBufferPtr buf;
    xmlSaveCtxtPtr save;
    xmlChar *out;
    int size, err = 0;


    doc = htmlReadDoc(BAD_CAST html, NULL, NULL, HTML_PARSE_NODEFDTD);

    /* xmlSave updates meta tags */
    buf = xmlBufferCreate();
    save = xmlSaveToBuffer(buf, "utf-8", 0);
    xmlSaveDoc(save, doc);
    xmlSaveClose(save);
    if (!xmlStrEqual(xmlBufferContent(buf), BAD_CAST expect)) {
        fprintf(stderr, "meta tag insertion failed when serializing\n");
        err = 1;
    }
    xmlBufferFree(buf);

    htmlSetMetaEncoding(doc, BAD_CAST "utf-8");
    /* htmlDocDumpMemoryFormat doesn't update meta tags */
    htmlDocDumpMemoryFormat(doc, &out, &size, 0);
    if (!xmlStrEqual(out, BAD_CAST expect)) {
        fprintf(stderr, "htmlSetMetaEncoding insertion failed\n");
        err = 1;
    }
    xmlFree(out);

    xmlFreeDoc(doc);
    return err;
}

static int
testHtmlUpdateMetaEncoding(void) {
    /* We rely on the implementation adjusting all meta tags */
    const char *html =
        "<html>\n"
        "    <head>\n"
        "        <meta charset=\"utf-8\">\n"
        "        <meta charset=\"  foo  \">\n"
        "        <meta charset=\"\">\n"
        "        <" MHE " content=\"text/html; ChArSeT=foo\">\n"
        "        <" MHE " content=\"text/html; charset = \">\n"
        "        <" MHE " content=\"text/html; charset = '  foo  '\">\n"
        "        <" MHE " content=\"text/html; charset = '  foo  \">\n"
        "        <" MHE " content='text/html; charset = \"  foo  \"'>\n"
        "        <" MHE " content='text/html; charset = \"  foo  '>\n"
        "        <" MHE " content=\"charset ; charset = bar; baz\">\n"
        "        <" MHE " content=\"text/html\">\n"
        "        <" MHE " content=\"\">\n"
        "        <" MHE ">\n"
        "    </head>\n"
        "    <body></body>\n"
        "</html>\n";
    const char *expect =
        "<html>\n"
        "    <head>\n"
        "        <meta charset=\"utf-8\">\n"
        "        <meta charset=\"  utf-8  \">\n"
        "        <meta charset=\"utf-8\">\n"
        "        <" MHE " content=\"text/html; ChArSeT=utf-8\">\n"
        "        <" MHE " content=\"text/html; charset = \">\n"
        "        <" MHE " content=\"text/html; charset = '  utf-8  '\">\n"
        "        <" MHE " content=\"text/html; charset = '  foo  \">\n"
        "        <" MHE " content=\"text/html; charset = &quot;  utf-8  &quot;\">\n"
        "        <" MHE " content=\"text/html; charset = &quot;  foo  \">\n"
        "        <" MHE " content=\"charset ; charset = utf-8; baz\">\n"
        "        <" MHE " content=\"text/html\">\n"
        "        <" MHE " content=\"\">\n"
        "        <" MHE ">\n"
        "    </head>\n"
        "    <body></body>\n"
        "</html>\n";
    htmlDocPtr doc;
    xmlBufferPtr buf;
    xmlSaveCtxtPtr save;
    xmlChar *out;
    int size, err = 0;

    doc = htmlReadDoc(BAD_CAST html, NULL, NULL, HTML_PARSE_NODEFDTD);

    /* xmlSave updates meta tags */
    buf = xmlBufferCreate();
    save = xmlSaveToBuffer(buf, NULL, 0);
    xmlSaveDoc(save, doc);
    xmlSaveClose(save);
    if (!xmlStrEqual(xmlBufferContent(buf), BAD_CAST expect)) {
        fprintf(stderr, "meta tag update failed when serializing\n");
        err = 1;
    }
    xmlBufferFree(buf);

    xmlFree((xmlChar *) doc->encoding);
    doc->encoding = NULL;
    htmlSetMetaEncoding(doc, BAD_CAST "utf-8");
    /* htmlDocDumpMemoryFormat doesn't update meta tags */
    htmlDocDumpMemoryFormat(doc, &out, &size, 0);
    if (!xmlStrEqual(out, BAD_CAST expect)) {
        fprintf(stderr, "htmlSetMetaEncoding update failed\n");
        err = 1;
    }
    xmlFree(out);

    xmlFreeDoc(doc);
    return err;
}
#endif /* LIBXML_OUTPUT_ENABLED */

#ifdef LIBXML_PUSH_ENABLED
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

    node = xmlDocGetRootElement(doc)->children->children;
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

#ifdef LIBXML_OUTPUT_ENABLED
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
#endif /* LIBXML_OUTPUT_ENABLED */

static int
testReaderNode(xmlTextReader *reader) {
    xmlChar *string;
    int type;
    int err = 0;

    type = xmlTextReaderNodeType(reader);
    string = xmlTextReaderReadString(reader);

    if (type == XML_READER_TYPE_ELEMENT) {
        xmlNodePtr node = xmlTextReaderCurrentNode(reader);

        if ((node->children == NULL) != (string == NULL))
            err = 1;
    } else if (type == XML_READER_TYPE_TEXT ||
               type == XML_READER_TYPE_CDATA ||
               type == XML_READER_TYPE_WHITESPACE ||
               type == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {
        if (string == NULL)
            err = 1;
    } else {
        if (string != NULL)
            err = 1;
    }

    if (err)
        fprintf(stderr, "xmlTextReaderReadString failed for %d\n", type);

    xmlFree(string);

    return err;
}

static int
testReader(void) {
    xmlTextReader *reader;
    const xmlChar *xml = BAD_CAST
        "<d>\n"
        "  x<e a='v'>y</e><f>z</f>\n"
        "  <![CDATA[cdata]]>\n"
        "  <!-- comment -->\n"
        "  <?pi content?>\n"
        "  <empty/>\n"
        "</d>";
    int err = 0;

    reader = xmlReaderForDoc(xml, NULL, NULL, 0);

    while (xmlTextReaderRead(reader) > 0) {
        if (testReaderNode(reader) > 0) {
            err = 1;
            break;
        }

        if (xmlTextReaderMoveToFirstAttribute(reader) > 0) {
            do {
                if (testReaderNode(reader) > 0) {
                    err = 1;
                    break;
                }
            } while (xmlTextReaderMoveToNextAttribute(reader) > 0);

            xmlTextReaderMoveToElement(reader);
        }
    }

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

typedef struct {
    const char *uri;
    const char *base;
    const char *result;
} xmlRelativeUriTest;

static int
testBuildRelativeUri(void) {
    xmlChar *res;
    int err = 0;
    int i;

    static const xmlRelativeUriTest tests[] = {
        {
            "/a/b1/c1",
            "/a/b2/c2",
            "../b1/c1"
        }, {
            "a/b1/c1",
            "a/b2/c2",
            "../b1/c1"
        }, {
            "a/././b1/x/y/../z/../.././c1",
            "./a/./b2/././b2",
            "../b1/c1"
        }, {
            "file:///a/b1/c1",
            "/a/b2/c2",
            NULL
        }, {
            "/a/b1/c1",
            "file:///a/b2/c2",
            NULL
        }, {
            "a/b1/c1",
            "/a/b2/c2",
            NULL
        }, {
            "/a/b1/c1",
            "a/b2/c2",
            NULL
        }, {
            "http://example.org/a/b1/c1",
            "http://example.org/a/b2/c2",
            "../b1/c1"
        }, {
            "http://example.org/a/b1/c1",
            "https://example.org/a/b2/c2",
            NULL
        }, {
            "http://example.org/a/b1/c1",
            "http://localhost/a/b2/c2",
            NULL
        }, {
            "with space/x x/y y",
            "with space/b2/c2",
            "../x%20x/y%20y"
        }, {
            "with space/x x/y y",
            "/b2/c2",
            "with%20space/x%20x/y%20y"
        }
#if defined(_WIN32) || defined(__CYGWIN__)
        , {
            "\\a\\b1\\c1",
            "\\a\\b2\\c2",
            "../b1/c1"
        }, {
            "\\a\\b1\\c1",
            "/a/b2/c2",
            "../b1/c1"
        }, {
            "a\\b1\\c1",
            "a/b2/c2",
            "../b1/c1"
        }, {
            "file://server/a/b1/c1",
            "\\\\?\\UNC\\server\\a\\b2\\c2",
            "../b1/c1"
        }, {
            "file://server/a/b1/c1",
            "\\\\server\\a\\b2\\c2",
            "../b1/c1"
        }, {
            "file:///x:/a/b1/c1",
            "x:\\a\\b2\\c2",
            "../b1/c1"
        }, {
            "file:///x:/a/b1/c1",
            "\\\\?\\x:\\a\\b2\\c2",
            "../b1/c1"
        }, {
            "file:///x:/a/b1/c1",
            "file:///y:/a/b2/c2",
            NULL
        }, {
            "x:/a/b1/c1",
            "y:/a/b2/c2",
            "file:///x:/a/b1/c1"
        }, {
            "/a/b1/c1",
            "y:/a/b2/c2",
            NULL
        }, {
            "\\\\server\\a\\b1\\c1",
            "a/b2/c2",
            "file://server/a/b1/c1"
        }
#endif
    };

    for (i = 0; (size_t) i < sizeof(tests) / sizeof(tests[0]); i++) {
        const xmlRelativeUriTest *test = tests + i;
        const char *expect;

        res = xmlBuildRelativeURI(BAD_CAST test->uri, BAD_CAST test->base);
        expect = test->result ? test->result : test->uri;
        if (!xmlStrEqual(res, BAD_CAST expect)) {
            fprintf(stderr, "xmlBuildRelativeURI failed uri=%s base=%s "
                    "result=%s expected=%s\n", test->uri, test->base,
                    res, expect);
            err = 1;
        }
        xmlFree(res);
    }

    return err;
}

#if defined(_WIN32) || defined(__CYGWIN__)
static int
testWindowsUri(void) {
    const char *url = "c:/a%20b/file.txt";
    xmlURIPtr uri;
    xmlChar *res;
    int err = 0;
    int i;

    static const xmlRelativeUriTest tests[] = {
        {
            "c:/a%20b/file.txt",
            "base.xml",
            "c:/a b/file.txt"
        }, {
            "file:///c:/a%20b/file.txt",
            "base.xml",
            "file:///c:/a%20b/file.txt"
        }, {
            "Z:/a%20b/file.txt",
            "http://example.com/",
            "Z:/a b/file.txt"
        }, {
            "a%20b/b1/c1",
            "C:/a/b2/c2",
            "C:/a/b2/a b/b1/c1"
        }, {
            "a%20b/b1/c1",
            "\\a\\b2\\c2",
            "/a/b2/a b/b1/c1"
        }, {
            "a%20b/b1/c1",
            "\\\\?\\a\\b2\\c2",
            "//?/a/b2/a b/b1/c1"
        }, {
            "a%20b/b1/c1",
            "\\\\\\\\server\\b2\\c2",
            "//server/b2/a b/b1/c1"
        }
    };

    uri = xmlParseURI(url);
    if (uri == NULL) {
        fprintf(stderr, "xmlParseURI failed\n");
        err = 1;
    } else {
        if (uri->scheme != NULL) {
            fprintf(stderr, "invalid scheme: %s\n", uri->scheme);
            err = 1;
        }
        if (uri->path == NULL || strcmp(uri->path, "c:/a b/file.txt") != 0) {
            fprintf(stderr, "invalid path: %s\n", uri->path);
            err = 1;
        }

        xmlFreeURI(uri);
    }

    for (i = 0; (size_t) i < sizeof(tests) / sizeof(tests[0]); i++) {
        const xmlRelativeUriTest *test = tests + i;

        res = xmlBuildURI(BAD_CAST test->uri, BAD_CAST test->base);
        if (res == NULL || !xmlStrEqual(res, BAD_CAST test->result)) {
            fprintf(stderr, "xmlBuildURI failed uri=%s base=%s "
                    "result=%s expected=%s\n", test->uri, test->base,
                    res, test->result);
            err = 1;
        }
        xmlFree(res);
    }

    return err;
}
#endif /* WIN32 */

#if defined(LIBXML_ICONV_ENABLED) || defined(LIBXML_ICU_ENABLED)
static int
testTruncatedMultiByte(void) {
    const char xml[] =
        "<?xml version=\"1.0\" encoding=\"EUC-JP\"?>\n"
        "<doc/>\xC3";
#ifdef LIBXML_HTML_ENABLED
    const char html[] =
        "<meta charset=\"EUC-JP\">\n"
        "<div/>\xC3";
#endif
    xmlDocPtr doc;
    const xmlError *error;
    int err = 0;

    xmlResetLastError();
    doc = xmlReadDoc(BAD_CAST xml, NULL, NULL, XML_PARSE_NOERROR);
    error = xmlGetLastError();
    if (error == NULL || error->code != XML_ERR_INVALID_ENCODING) {
        fprintf(stderr, "xml, pull: expected XML_ERR_INVALID_ENCODING\n");
        err = 1;
    }
    xmlFreeDoc(doc);

#ifdef LIBXML_HTML_ENABLED
    xmlResetLastError();
    doc = htmlReadDoc(BAD_CAST html, NULL, NULL, XML_PARSE_NOERROR);
    error = xmlGetLastError();
    if (error == NULL || error->code != XML_ERR_INVALID_ENCODING) {
        fprintf(stderr, "html, pull: expected XML_ERR_INVALID_ENCODING\n");
        err = 1;
    }
    xmlFreeDoc(doc);
#endif /* LIBXML_HTML_ENABLED */

#ifdef LIBXML_PUSH_ENABLED
    {
        xmlParserCtxtPtr ctxt;

        ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL);
        xmlCtxtSetOptions(ctxt, XML_PARSE_NOERROR);

        xmlParseChunk(ctxt, xml, sizeof(xml) - 1, 0);
        xmlParseChunk(ctxt, "", 0, 1);

        if (ctxt->errNo != XML_ERR_INVALID_ENCODING) {
            fprintf(stderr, "xml, push: expected XML_ERR_INVALID_ENCODING\n");
            err = 1;
        }

        xmlFreeDoc(ctxt->myDoc);
        xmlFreeParserCtxt(ctxt);

#ifdef LIBXML_HTML_ENABLED
        ctxt = htmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL,
                                        XML_CHAR_ENCODING_NONE);
        xmlCtxtSetOptions(ctxt, XML_PARSE_NOERROR);

        htmlParseChunk(ctxt, html, sizeof(html) - 1, 0);
        htmlParseChunk(ctxt, "", 0, 1);

        if (ctxt->errNo != XML_ERR_INVALID_ENCODING) {
            fprintf(stderr, "html, push: expected XML_ERR_INVALID_ENCODING\n");
            err = 1;
        }

        xmlFreeDoc(ctxt->myDoc);
        htmlFreeParserCtxt(ctxt);
#endif /* LIBXML_HTML_ENABLED */
    }
#endif /* LIBXML_PUSH_ENABLED */

    return err;
}
#endif /* iconv || icu */

static int charEncConvImplError;

static xmlCharEncError
rot13Convert(void *vctxt, unsigned char *out, int *outlen,
             const unsigned char *in, int *inlen,
             int flush ATTRIBUTE_UNUSED) {
    int *ctxt = vctxt;
    int inSize = *inlen;
    int outSize = *outlen;
    int rot, i;

    rot = *ctxt;

    for (i = 0; i < inSize && i < outSize; i++) {
        int c = in[i];

        if (c >= 'A' && c <= 'Z')
            c = 'A' + (c - 'A' + rot) % 26;
        else if (c >= 'a' && c <= 'z')
            c = 'a' + (c - 'a' + rot) % 26;

        out[i] = c;
    }

    *inlen = i;
    *outlen = i;

    return XML_ENC_ERR_SUCCESS;
}

static void
rot13ConvCtxtDtor(void *vctxt) {
    xmlFree(vctxt);
}

static xmlParserErrors
rot13ConvImpl(void *vctxt ATTRIBUTE_UNUSED, const char *name,
              xmlCharEncFlags flags, xmlCharEncodingHandler **out) {
    int *inputCtxt;

    if (strcmp(name, "rot13") != 0)
        return xmlCreateCharEncodingHandler(name, flags, NULL, NULL, out);

    if (flags & XML_ENC_OUTPUT)
        return XML_ERR_UNSUPPORTED_ENCODING;

    inputCtxt = xmlMalloc(sizeof(*inputCtxt));
    *inputCtxt = 13;

    return xmlCharEncNewCustomHandler(name, rot13Convert, NULL,
                                      rot13ConvCtxtDtor, inputCtxt, NULL,
                                      out);
}

static int
testCharEncConvImpl(void) {
    xmlParserCtxtPtr ctxt;
    xmlDocPtr doc;
    xmlNodePtr root;
    int err = 0;

    ctxt = xmlNewParserCtxt();
    xmlCtxtSetCharEncConvImpl(ctxt, rot13ConvImpl, NULL);
    charEncConvImplError = 0;
    doc = xmlCtxtReadDoc(ctxt, BAD_CAST "<?kzy irefvba='1.0'?><qbp/>", NULL,
                         "rot13", 0);
    if (charEncConvImplError)
        err = 1;
    xmlFreeParserCtxt(ctxt);

    root = xmlDocGetRootElement(doc);
    if (root == NULL || strcmp((char *) root->name, "doc") != 0) {
        fprintf(stderr, "testCharEncConvImpl failed\n");
        err = 1;
    }

    xmlFreeDoc(doc);

    return err;
}

int
main(void) {
    int err = 0;

    err |= testNewDocNode();
    err |= testStandaloneWithEncoding();
    err |= testUnsupportedEncoding();
    err |= testNodeGetContent();
    err |= testCFileIO();
    err |= testUndeclEntInContent();
    err |= testInvalidCharRecovery();
    err |= testCtxtInputGetters();
#ifdef LIBXML_VALID_ENABLED
    err |= testSwitchDtd();
#endif
#ifdef LIBXML_OUTPUT_ENABLED
    err |= testCtxtParseContent();
    err |= testNoBlanks();
    err |= testSaveNullEnc();
    err |= testDocDumpFormatMemoryEnc();
#endif
#ifdef LIBXML_SAX1_ENABLED
    err |= testBalancedChunk();
#endif
#ifdef LIBXML_PUSH_ENABLED
    err |= testHugePush();
    err |= testHugeEncodedChunk();
    err |= testPushCDataEnd();
#endif
#ifdef LIBXML_HTML_ENABLED
    err |= testHtmlIds();
#ifdef LIBXML_OUTPUT_ENABLED
    err |= testHtmlInsertMetaEncoding();
    err |= testHtmlUpdateMetaEncoding();
#endif
#ifdef LIBXML_PUSH_ENABLED
    err |= testHtmlPushWithEncoding();
#endif
#endif
#ifdef LIBXML_READER_ENABLED
    err |= testReaderEncoding();
#ifdef LIBXML_OUTPUT_ENABLED
    err |= testReaderContent();
#endif
    err |= testReader();
#ifdef LIBXML_XINCLUDE_ENABLED
    err |= testReaderXIncludeError();
#endif
#endif
#ifdef LIBXML_WRITER_ENABLED
    err |= testWriterClose();
#endif
    err |= testBuildRelativeUri();
#if defined(_WIN32) || defined(__CYGWIN__)
    err |= testWindowsUri();
#endif
#if defined(LIBXML_ICONV_ENABLED) || defined(LIBXML_ICU_ENABLED)
    err |= testTruncatedMultiByte();
#endif
    err |= testCharEncConvImpl();

    return err;
}

