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
    xmlSetGenericErrorFunc(NULL, xmlFuzzErrorFunc);

    return 0;
}

int
LLVMFuzzerTestOneInput(const char *data, size_t size) {
    xmlRegexpPtr regexp;
    char *str[2] = { NULL, NULL };
    size_t numStrings;

    if (size > 200)
        return(0);

    numStrings = xmlFuzzExtractStrings(data, size, str, 2);

    /* CUR_SCHAR doesn't handle invalid UTF-8 and may cause infinite loops. */
    if (xmlCheckUTF8(BAD_CAST str[0]) != 0) {
        regexp = xmlRegexpCompile(BAD_CAST str[0]);
        /* xmlRegexpExec has pathological performance in too many cases. */
#if 0
        if ((regexp != NULL) && (numStrings >= 2)) {
            xmlRegexpExec(regexp, BAD_CAST str[1]);
        }
#endif
        xmlRegFreeRegexp(regexp);
    }

    xmlFree(str[0]);
    xmlFree(str[1]);
    xmlResetLastError();

    return 0;
}

