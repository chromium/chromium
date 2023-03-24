/*
 * regexp.c: a libFuzzer target to test the regexp module.
 *
 * See Copyright for the status of this software.
 */

#include <libxml/xmlregexp.h>
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
    xmlRegexpPtr regexp;
    size_t maxAlloc;
    const char *str1;

    if (size > 200)
        return(0);

    xmlFuzzDataInit(data, size);
    maxAlloc = xmlFuzzReadInt(4) % (size * 8 + 1);
    str1 = xmlFuzzReadString(NULL);

    /* CUR_SCHAR doesn't handle invalid UTF-8 and may cause infinite loops. */
    if (xmlCheckUTF8(BAD_CAST str1) != 0) {
        xmlFuzzMemSetLimit(maxAlloc);
        regexp = xmlRegexpCompile(BAD_CAST str1);
        /* xmlRegexpExec has pathological performance in too many cases. */
#if 0
        xmlRegexpExec(regexp, BAD_CAST str2);
#endif
        xmlRegFreeRegexp(regexp);
    }

    xmlFuzzMemSetLimit(0);
    xmlFuzzDataCleanup();
    xmlResetLastError();

    return 0;
}

