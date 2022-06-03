/*
 * schema.c: a libFuzzer target to test the XML Schema processor.
 *
 * See Copyright for the status of this software.
 */

#include <libxml/catalog.h>
#include <libxml/xmlschemas.h>
#include "fuzz.h"

int
LLVMFuzzerInitialize(int *argc ATTRIBUTE_UNUSED,
                     char ***argv ATTRIBUTE_UNUSED) {
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
    xmlSchemaParserCtxtPtr pctxt;

    if (size > 50000)
        return(0);

    xmlFuzzDataInit(data, size);
    xmlFuzzReadEntities();

    pctxt = xmlSchemaNewParserCtxt(xmlFuzzMainUrl());
    xmlSchemaSetParserErrors(pctxt, xmlFuzzErrorFunc, xmlFuzzErrorFunc, NULL);
    xmlSchemaFree(xmlSchemaParse(pctxt));
    xmlSchemaFreeParserCtxt(pctxt);

    xmlFuzzDataCleanup();
    xmlResetLastError();

    return(0);
}

