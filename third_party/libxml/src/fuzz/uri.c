/*
 * uri.c: a libFuzzer target to test the URI module.
 *
 * See Copyright for the status of this software.
 */

#include <libxml/uri.h>
#include "fuzz.h"

int
LLVMFuzzerInitialize(int *argc ATTRIBUTE_UNUSED,
                     char ***argv ATTRIBUTE_UNUSED) {
    xmlFuzzMemSetup();
    xmlSetGenericErrorFunc(NULL, xmlFuzzErrorFunc);

    return 0;
}

int
LLVMFuzzerTestOneInput(const char *data, size_t size) {
    xmlURIPtr uri;
    size_t maxAlloc;
    const char *str1, *str2;
    char *copy;

    if (size > 10000)
        return(0);

    xmlFuzzDataInit(data, size);
    maxAlloc = xmlFuzzReadInt(4) % (size * 8 + 1);
    str1 = xmlFuzzReadString(NULL);
    str2 = xmlFuzzReadString(NULL);

    xmlFuzzMemSetLimit(maxAlloc);

    uri = xmlParseURI(str1);
    xmlFree(xmlSaveUri(uri));
    xmlFreeURI(uri);

    uri = xmlParseURIRaw(str1, 1);
    xmlFree(xmlSaveUri(uri));
    xmlFreeURI(uri);

    xmlFree(xmlURIUnescapeString(str1, -1, NULL));
    xmlFree(xmlURIEscape(BAD_CAST str1));
    xmlFree(xmlCanonicPath(BAD_CAST str1));
    xmlFree(xmlPathToURI(BAD_CAST str1));

    xmlFree(xmlBuildURI(BAD_CAST str2, BAD_CAST str1));
    xmlFree(xmlBuildRelativeURI(BAD_CAST str2, BAD_CAST str1));
    xmlFree(xmlURIEscapeStr(BAD_CAST str1, BAD_CAST str2));

    copy = (char *) xmlCharStrdup(str1);
    xmlNormalizeURIPath(copy);
    xmlFree(copy);

    xmlFuzzMemSetLimit(0);
    xmlFuzzDataCleanup();

    return 0;
}

