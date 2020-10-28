/*
 * html.c: a libFuzzer target to test several HTML parser interfaces.
 *
 * See Copyright for the status of this software.
 */

#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include "fuzz.h"

int
LLVMFuzzerInitialize(int *argc ATTRIBUTE_UNUSED,
                     char ***argv ATTRIBUTE_UNUSED) {
    xmlInitParser();
    xmlSetGenericErrorFunc(NULL, xmlFuzzErrorFunc);

    return 0;
}

int
LLVMFuzzerTestOneInput(const char *data, size_t size) {
    static const size_t maxChunkSize = 128;
    htmlDocPtr doc;
    htmlParserCtxtPtr ctxt;
    xmlChar *out;
    const char *docBuffer;
    size_t docSize, consumed, chunkSize;
    int opts, outSize;

    xmlFuzzDataInit(data, size);
    opts = xmlFuzzReadInt();

    docBuffer = xmlFuzzReadRemaining(&docSize);
    if (docBuffer == NULL) {
        xmlFuzzDataCleanup();
        return(0);
    }

    /* Pull parser */

    doc = htmlReadMemory(docBuffer, docSize, NULL, NULL, opts);
    /* Also test the serializer. */
    htmlDocDumpMemory(doc, &out, &outSize);
    xmlFree(out);
    xmlFreeDoc(doc);

    /* Push parser */

    ctxt = htmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL,
                                    XML_CHAR_ENCODING_NONE);
    htmlCtxtUseOptions(ctxt, opts);

    for (consumed = 0; consumed < docSize; consumed += chunkSize) {
        chunkSize = docSize - consumed;
        if (chunkSize > maxChunkSize)
            chunkSize = maxChunkSize;
        htmlParseChunk(ctxt, docBuffer + consumed, chunkSize, 0);
    }

    htmlParseChunk(ctxt, NULL, 0, 1);
    xmlFreeDoc(ctxt->myDoc);
    htmlFreeParserCtxt(ctxt);

    /* Cleanup */

    xmlFuzzDataCleanup();

    return(0);
}

