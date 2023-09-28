#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/dict.h>

/* #define WITH_PRINT */

static const char *seeds1[] = {
   "a", "b", "c",
   "d", "e", "f",
   "g", "h", "i",
   "j", "k", "l",

   NULL
};

static const char *seeds2[] = {
   "m", "n", "o",
   "p", "q", "r",
   "s", "t", "u",
   "v", "w", "x",

   NULL
};

#define NB_STRINGS_MAX 100000
#define NB_STRINGS_NS  10000
#define NB_STRINGS_PREFIX (NB_STRINGS_NS / 20)
#define NB_STRINGS_MIN 10

static xmlChar **strings1;
static xmlChar **strings2;
static const xmlChar **test1;
static const xmlChar **test2;
static int nbErrors = 0;

static void
fill_string_pool(xmlChar **strings, const char **seeds) {
    int i, j, k;
    int start_ns = NB_STRINGS_MAX - NB_STRINGS_NS;

    /*
     * That's a bit nasty but the output is fine and it doesn't take hours
     * there is a small but sufficient number of duplicates, and we have
     * ":xxx" and full QNames in the last NB_STRINGS_NS values
     */
    for (i = 0; seeds[i] != NULL; i++) {
        strings[i] = xmlStrdup((const xmlChar *) seeds[i]);
	if (strings[i] == NULL) {
	    fprintf(stderr, "Out of memory while generating strings\n");
	    exit(1);
	}
    }
    for (j = 0, k = 0; i < start_ns; i++) {
        strings[i] = xmlStrncatNew(strings[j], strings[k], -1);
	if (strings[i] == NULL) {
	    fprintf(stderr, "Out of memory while generating strings\n");
	    exit(1);
	}
        if (xmlStrlen(strings[i]) > 30) {
            fprintf(stderr, "### %s %s\n", strings[start_ns+j], strings[k]);
            abort();
        }
        j++;
	if (j >= 50) {
	    j = 0;
	    k++;
	}
    }
    for (j = 0, k = 0; (j < NB_STRINGS_PREFIX) && (i < NB_STRINGS_MAX);
         i++, j++) {
        strings[i] = xmlStrncatNew(strings[k], (const xmlChar *) ":", -1);
	if (strings[i] == NULL) {
	    fprintf(stderr, "Out of memory while generating strings\n");
	    exit(1);
	}
        k += 1;
        if (k >= start_ns) k = 0;
    }
    for (j = 0, k = 0; i < NB_STRINGS_MAX; i++) {
        strings[i] = xmlStrncatNew(strings[start_ns+j], strings[k], -1);
	if (strings[i] == NULL) {
	    fprintf(stderr, "Out of memory while generating strings\n");
	    exit(1);
	}
        j++;
        if (j >= NB_STRINGS_PREFIX) j = 0;
	k += 5;
        if (k >= start_ns) k = 0;
    }
}

#ifdef WITH_PRINT
static void print_strings(void) {
    int i;

    for (i = 0; i < NB_STRINGS_MAX;i++) {
        printf("%s\n", strings1[i]);
    }
    for (i = 0; i < NB_STRINGS_MAX;i++) {
        printf("%s\n", strings2[i]);
    }
}
#endif

static void clean_strings(void) {
    int i;

    for (i = 0; i < NB_STRINGS_MAX; i++) {
        if (strings1[i] != NULL) /* really should not happen */
	    xmlFree(strings1[i]);
    }
    for (i = 0; i < NB_STRINGS_MAX; i++) {
        if (strings2[i] != NULL) /* really should not happen */
	    xmlFree(strings2[i]);
    }
}

/*
 * This tests the sub-dictionary support
 */
static int run_test2(xmlDictPtr parent) {
    int i, j;
    xmlDictPtr dict;
    int ret = 0;
    xmlChar prefix[40];
    xmlChar *cur, *pref;
    const xmlChar *tmp;

    dict = xmlDictCreateSub(parent);
    if (dict == NULL) {
	fprintf(stderr, "Out of memory while creating sub-dictionary\n");
	exit(1);
    }
    /* Cast to avoid buggy warning on MSVC. */
    memset((void *) test2, 0, sizeof(test2));

    /*
     * Fill in NB_STRINGS_MIN, at this point the dictionary should not grow
     * and we allocate all those doing the fast key computations
     * All the strings are based on a different seeds subset so we know
     * they are allocated in the main dictionary, not coming from the parent
     */
    for (i = 0;i < NB_STRINGS_MIN;i++) {
        test2[i] = xmlDictLookup(dict, strings2[i], -1);
	if (test2[i] == NULL) {
	    fprintf(stderr, "Failed lookup for '%s'\n", strings2[i]);
	    ret = 1;
	    nbErrors++;
	}
    }
    j = NB_STRINGS_MAX - NB_STRINGS_NS;
    /* ":foo" like strings2 */
    for (i = 0;i < NB_STRINGS_MIN;i++, j++) {
        test2[j] = xmlDictLookup(dict, strings2[j], xmlStrlen(strings2[j]));
	if (test2[j] == NULL) {
	    fprintf(stderr, "Failed lookup for '%s'\n", strings2[j]);
	    ret = 1;
	    nbErrors++;
	}
    }
    /* "a:foo" like strings2 */
    j = NB_STRINGS_MAX - NB_STRINGS_MIN;
    for (i = 0;i < NB_STRINGS_MIN;i++, j++) {
        test2[j] = xmlDictLookup(dict, strings2[j], xmlStrlen(strings2[j]));
	if (test2[j] == NULL) {
	    fprintf(stderr, "Failed lookup for '%s'\n", strings2[j]);
	    ret = 1;
	    nbErrors++;
	}
    }

    /*
     * At this point allocate all the strings
     * the dictionary will grow in the process, reallocate more string tables
     * and switch to the better key generator
     */
    for (i = 0;i < NB_STRINGS_MAX;i++) {
        if (test2[i] != NULL)
	    continue;
	test2[i] = xmlDictLookup(dict, strings2[i], -1);
	if (test2[i] == NULL) {
	    fprintf(stderr, "Failed lookup for '%s'\n", strings2[i]);
	    ret = 1;
	    nbErrors++;
	}
    }

    /*
     * Now we can start to test things, first that all strings2 belongs to
     * the dict, and that none of them was actually allocated in the parent
     */
    for (i = 0;i < NB_STRINGS_MAX;i++) {
        if (!xmlDictOwns(dict, test2[i])) {
	    fprintf(stderr, "Failed ownership failure for '%s'\n",
	            strings2[i]);
	    ret = 1;
	    nbErrors++;
	}
        if (xmlDictOwns(parent, test2[i])) {
	    fprintf(stderr, "Failed parent ownership failure for '%s'\n",
	            strings2[i]);
	    ret = 1;
	    nbErrors++;
	}
    }

    /*
     * Also verify that all strings from the parent are seen from the subdict
     */
    for (i = 0;i < NB_STRINGS_MAX;i++) {
        if (!xmlDictOwns(dict, test1[i])) {
	    fprintf(stderr, "Failed sub-ownership failure for '%s'\n",
	            strings1[i]);
	    ret = 1;
	    nbErrors++;
	}
    }

    /*
     * Then that another lookup to the string in sub will return the same
     */
    for (i = 0;i < NB_STRINGS_MAX;i++) {
        if (xmlDictLookup(dict, strings2[i], -1) != test2[i]) {
	    fprintf(stderr, "Failed re-lookup check for %d, '%s'\n",
	            i, strings2[i]);
	    ret = 1;
	    nbErrors++;
	}
    }
    /*
     * But also that any lookup for a string in the parent will be provided
     * as in the parent
     */
    for (i = 0;i < NB_STRINGS_MAX;i++) {
        if (xmlDictLookup(dict, strings1[i], -1) != test1[i]) {
	    fprintf(stderr, "Failed parent string lookup check for %d, '%s'\n",
	            i, strings1[i]);
	    ret = 1;
	    nbErrors++;
	}
    }

    /*
     * check the QName lookups
     */
    for (i = NB_STRINGS_MAX - NB_STRINGS_NS;i < NB_STRINGS_MAX;i++) {
        cur = strings2[i];
	pref = &prefix[0];
	while (*cur != ':') *pref++ = *cur++;
	cur++;
	*pref = 0;
	tmp = xmlDictQLookup(dict, &prefix[0], cur);
	if (tmp != test2[i]) {
	    fprintf(stderr, "Failed lookup check for '%s':'%s'\n",
	            &prefix[0], cur);
            ret = 1;
	    nbErrors++;
	}
    }
    /*
     * check the QName lookups for strings from the parent
     */
    for (i = NB_STRINGS_MAX - NB_STRINGS_NS;i < NB_STRINGS_MAX;i++) {
        cur = strings1[i];
	pref = &prefix[0];
	while (*cur != ':') *pref++ = *cur++;
	cur++;
	*pref = 0;
	tmp = xmlDictQLookup(dict, &prefix[0], cur);
	if (xmlDictQLookup(dict, &prefix[0], cur) != test1[i]) {
	    fprintf(stderr, "Failed parent lookup check for '%s':'%s'\n",
	            &prefix[0], cur);
            ret = 1;
	    nbErrors++;
	}
    }

    xmlDictFree(dict);
    return(ret);
}

/*
 * Test a single dictionary
 */
static int run_test1(void) {
    int i, j;
    xmlDictPtr dict;
    int ret = 0;
    xmlChar prefix[40];
    xmlChar *cur, *pref;
    const xmlChar *tmp;

    dict = xmlDictCreate();
    if (dict == NULL) {
	fprintf(stderr, "Out of memory while creating dictionary\n");
	exit(1);
    }
    /* Cast to avoid buggy warning on MSVC. */
    memset((void *) test1, 0, sizeof(test1));

    /*
     * Fill in NB_STRINGS_MIN, at this point the dictionary should not grow
     * and we allocate all those doing the fast key computations
     */
    for (i = 0;i < NB_STRINGS_MIN;i++) {
        test1[i] = xmlDictLookup(dict, strings1[i], -1);
	if (test1[i] == NULL) {
	    fprintf(stderr, "Failed lookup for '%s'\n", strings1[i]);
	    ret = 1;
	    nbErrors++;
	}
    }
    j = NB_STRINGS_MAX - NB_STRINGS_NS;
    /* ":foo" like strings1 */
    for (i = 0;i < NB_STRINGS_MIN;i++, j++) {
        test1[j] = xmlDictLookup(dict, strings1[j], xmlStrlen(strings1[j]));
	if (test1[j] == NULL) {
	    fprintf(stderr, "Failed lookup for '%s'\n", strings1[j]);
	    ret = 1;
	    nbErrors++;
	}
    }
    /* "a:foo" like strings1 */
    j = NB_STRINGS_MAX - NB_STRINGS_MIN;
    for (i = 0;i < NB_STRINGS_MIN;i++, j++) {
        test1[j] = xmlDictLookup(dict, strings1[j], xmlStrlen(strings1[j]));
	if (test1[j] == NULL) {
	    fprintf(stderr, "Failed lookup for '%s'\n", strings1[j]);
	    ret = 1;
	    nbErrors++;
	}
    }

    /*
     * At this point allocate all the strings
     * the dictionary will grow in the process, reallocate more string tables
     * and switch to the better key generator
     */
    for (i = 0;i < NB_STRINGS_MAX;i++) {
        if (test1[i] != NULL)
	    continue;
	test1[i] = xmlDictLookup(dict, strings1[i], -1);
	if (test1[i] == NULL) {
	    fprintf(stderr, "Failed lookup for '%s'\n", strings1[i]);
	    ret = 1;
	    nbErrors++;
	}
    }

    /*
     * Now we can start to test things, first that all strings1 belongs to
     * the dict
     */
    for (i = 0;i < NB_STRINGS_MAX;i++) {
        if (!xmlDictOwns(dict, test1[i])) {
	    fprintf(stderr, "Failed ownership failure for '%s'\n",
	            strings1[i]);
	    ret = 1;
	    nbErrors++;
	}
    }

    /*
     * Then that another lookup to the string will return the same
     */
    for (i = 0;i < NB_STRINGS_MAX;i++) {
        if (xmlDictLookup(dict, strings1[i], -1) != test1[i]) {
	    fprintf(stderr, "Failed re-lookup check for %d, '%s'\n",
	            i, strings1[i]);
	    ret = 1;
	    nbErrors++;
	}
    }

    /*
     * More complex, check the QName lookups
     */
    for (i = NB_STRINGS_MAX - NB_STRINGS_NS;i < NB_STRINGS_MAX;i++) {
        cur = strings1[i];
	pref = &prefix[0];
	while (*cur != ':') *pref++ = *cur++;
	cur++;
	*pref = 0;
	tmp = xmlDictQLookup(dict, &prefix[0], cur);
	if (tmp != test1[i]) {
	    fprintf(stderr, "Failed lookup check for '%s':'%s'\n",
	            &prefix[0], cur);
            ret = 1;
	    nbErrors++;
	}
    }

    run_test2(dict);

    xmlDictFree(dict);
    return(ret);
}

int main(void)
{
    int ret;

    LIBXML_TEST_VERSION

    strings1 = xmlMalloc(NB_STRINGS_MAX * sizeof(strings1[0]));
    memset(strings1, 0, NB_STRINGS_MAX * sizeof(strings1[0]));
    strings2 = xmlMalloc(NB_STRINGS_MAX * sizeof(strings2[0]));
    memset(strings2, 0, NB_STRINGS_MAX * sizeof(strings2[0]));
    test1 = xmlMalloc(NB_STRINGS_MAX * sizeof(test1[0]));
    memset(test1, 0, NB_STRINGS_MAX * sizeof(test1[0]));
    test2 = xmlMalloc(NB_STRINGS_MAX * sizeof(test2[0]));
    memset(test2, 0, NB_STRINGS_MAX * sizeof(test2[0]));

    fill_string_pool(strings1, seeds1);
    fill_string_pool(strings2, seeds2);
#ifdef WITH_PRINT
    print_strings();
#endif
    ret = run_test1();
    if (ret == 0) {
        printf("dictionary tests succeeded %d strings\n", 2 * NB_STRINGS_MAX);
    } else {
        printf("dictionary tests failed with %d errors\n", nbErrors);
    }
    clean_strings();
    xmlFree(strings1);
    xmlFree(strings2);
    xmlFree(test1);
    xmlFree(test2);
    xmlCleanupParser();
    return(ret);
}
