/*
 * uri.c: a libFuzzer target to test the URI module.
 *
 * See Copyright for the status of this software.
 */

#include <libxml/uri.h>
#include "fuzz.h"

int
LLVMFuzzerTestOneInput(const char *data, size_t size) {
    xmlURIPtr uri;
    char *str[2] = { NULL, NULL };
    size_t numStrings;

    if (size > 10000)
        return(0);

    numStrings = xmlFuzzExtractStrings(data, size, str, 2);

    uri = xmlParseURI(str[0]);
    xmlFree(xmlSaveUri(uri));
    xmlFreeURI(uri);

    uri = xmlParseURIRaw(str[0], 1);
    xmlFree(xmlSaveUri(uri));
    xmlFreeURI(uri);

    xmlFree(xmlURIUnescapeString(str[0], -1, NULL));
    xmlFree(xmlURIEscape(BAD_CAST str[0]));
    xmlFree(xmlCanonicPath(BAD_CAST str[0]));
    xmlFree(xmlPathToURI(BAD_CAST str[0]));

    if (numStrings >= 2) {
        xmlFree(xmlBuildURI(BAD_CAST str[1], BAD_CAST str[0]));
        xmlFree(xmlBuildRelativeURI(BAD_CAST str[1], BAD_CAST str[0]));
        xmlFree(xmlURIEscapeStr(BAD_CAST str[0], BAD_CAST str[1]));
    }

    /* Modifies string, so must come last. */
    xmlNormalizeURIPath(str[0]);

    xmlFree(str[0]);
    xmlFree(str[1]);

    return 0;
}

