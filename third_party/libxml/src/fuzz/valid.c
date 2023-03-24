/*
 * valid.c: a libFuzzer target to test DTD validation.
 *
 * See Copyright for the status of this software.
 */

#include <libxml/catalog.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlreader.h>
#include "fuzz.h"

int
LLVMFuzzerInitialize(int *argc ATTRIBUTE_UNUSED,
                     char ***argv ATTRIBUTE_UNUSED) {
    xmlFuzzMemSetup();
    xmlInitParser();
#ifdef LIBXML_CATALOG_ENABLED
    xmlInitializeCatalog();
#endif
    xmlSetGenericErrorFunc(NULL, xmlFuzzErrorFunc);
    xmlSetExternalEntityLoader(xmlFuzzEntityLoader);

    return 0;
}

int
LLVMFuzzerTestOneInput(const char *data, size_t size) {
    static const size_t maxChunkSize = 128;
    xmlDocPtr doc;
    xmlParserCtxtPtr ctxt;
    xmlValidCtxtPtr vctxt;
    xmlTextReaderPtr reader;
    const char *docBuffer, *docUrl;
    size_t maxAlloc, docSize, consumed, chunkSize;
    int opts;

    xmlFuzzDataInit(data, size);
    opts = (int) xmlFuzzReadInt(4);
    opts &= ~XML_PARSE_XINCLUDE;
    opts |= XML_PARSE_DTDVALID;
    maxAlloc = xmlFuzzReadInt(4) % (size + 1);

    xmlFuzzReadEntities();
    docBuffer = xmlFuzzMainEntity(&docSize);
    docUrl = xmlFuzzMainUrl();
    if (docBuffer == NULL)
        goto exit;

    /* Pull parser */

    xmlFuzzMemSetLimit(maxAlloc);
    doc = xmlReadMemory(docBuffer, docSize, docUrl, NULL, opts);
    xmlFreeDoc(doc);

    /* Post validation */

    xmlFuzzMemSetLimit(maxAlloc);
    doc = xmlReadMemory(docBuffer, docSize, docUrl, NULL, opts & ~XML_PARSE_DTDVALID);
    vctxt = xmlNewValidCtxt();
    xmlValidateDocument(vctxt, doc);
    xmlFreeValidCtxt(vctxt);
    xmlFreeDoc(doc);

    /* Push parser */

    xmlFuzzMemSetLimit(maxAlloc);
    ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, docUrl);
    if (ctxt == NULL)
        goto exit;
    xmlCtxtUseOptions(ctxt, opts);

    for (consumed = 0; consumed < docSize; consumed += chunkSize) {
        chunkSize = docSize - consumed;
        if (chunkSize > maxChunkSize)
            chunkSize = maxChunkSize;
        xmlParseChunk(ctxt, docBuffer + consumed, chunkSize, 0);
    }

    xmlParseChunk(ctxt, NULL, 0, 1);
    xmlFreeDoc(ctxt->myDoc);
    xmlFreeParserCtxt(ctxt);

    /* Reader */

    xmlFuzzMemSetLimit(maxAlloc);
    reader = xmlReaderForMemory(docBuffer, docSize, NULL, NULL, opts);
    if (reader == NULL)
        goto exit;
    while (xmlTextReaderRead(reader) == 1) {
        if (xmlTextReaderNodeType(reader) == XML_ELEMENT_NODE) {
            int i, n = xmlTextReaderAttributeCount(reader);
            for (i=0; i<n; i++) {
                xmlTextReaderMoveToAttributeNo(reader, i);
                while (xmlTextReaderReadAttributeValue(reader) == 1);
            }
        }
    }
    xmlFreeTextReader(reader);

exit:
    xmlFuzzMemSetLimit(0);
    xmlFuzzDataCleanup();
    xmlResetLastError();
    return(0);
}

