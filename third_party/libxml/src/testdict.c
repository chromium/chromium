#include "libxml.h"

#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/dict.h>


/**** dictionary tests ****/

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
static int
test_subdict(xmlDictPtr parent) {
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
static int
test_dict(xmlDict *dict) {
    int i, j;
    int ret = 0;
    xmlChar prefix[40];
    xmlChar *cur, *pref;
    const xmlChar *tmp;

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

    return(ret);
}

static int
testall_dict(void) {
    xmlDictPtr dict;
    int ret = 0;

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

    dict = xmlDictCreate();
    if (dict == NULL) {
	fprintf(stderr, "Out of memory while creating dictionary\n");
	exit(1);
    }
    if (test_dict(dict) != 0) {
        ret = 1;
    }
    if (test_subdict(dict) != 0) {
        ret = 1;
    }
    xmlDictFree(dict);

    clean_strings();
    xmlFree(strings1);
    xmlFree(strings2);
    xmlFree(test1);
    xmlFree(test2);

    return ret;
}


/**** Hash table tests ****/

static unsigned
rng_state[2] = { 123, 456 };

#define HASH_ROL(x,n) ((x) << (n) | ((x) & 0xFFFFFFFF) >> (32 - (n)))

ATTRIBUTE_NO_SANITIZE_INTEGER
static unsigned
my_rand(unsigned max) {
    unsigned s0 = rng_state[0];
    unsigned s1 = rng_state[1];
    unsigned result = HASH_ROL(s0 * 0x9E3779BB, 5) * 5;

    s1 ^= s0;
    rng_state[0] = HASH_ROL(s0, 26) ^ s1 ^ (s1 << 9);
    rng_state[1] = HASH_ROL(s1, 13);

    return((result & 0xFFFFFFFF) % max);
}

static xmlChar *
gen_random_string(xmlChar id) {
    unsigned size = my_rand(64) + 1;
    unsigned id_pos = my_rand(size);
    size_t j;

    xmlChar *str = xmlMalloc(size + 1);
    for (j = 0; j < size; j++) {
        str[j] = 'a' + my_rand(26);
    }
    str[id_pos] = id;
    str[size] = 0;

    /* Generate QName in 75% of cases */
    if (size > 3 && my_rand(4) > 0) {
        unsigned colon_pos = my_rand(size - 3) + 1;

        if (colon_pos >= id_pos)
            colon_pos++;
        str[colon_pos] = ':';
    }

    return str;
}

typedef struct {
    xmlChar **strings;
    size_t num_entries;
    size_t num_keys;
    size_t num_strings;
    size_t index;
    xmlChar id;
} StringPool;

static StringPool *
pool_new(size_t num_entries, size_t num_keys, xmlChar id) {
    StringPool *ret;
    size_t num_strings;

    ret = xmlMalloc(sizeof(*ret));
    ret->num_entries = num_entries;
    ret->num_keys = num_keys;
    num_strings = num_entries * num_keys;
    ret->strings = xmlMalloc(num_strings * sizeof(ret->strings[0]));
    memset(ret->strings, 0, num_strings * sizeof(ret->strings[0]));
    ret->num_strings = num_strings;
    ret->index = 0;
    ret->id = id;

    return ret;
}

static void
pool_free(StringPool *pool) {
    size_t i;

    for (i = 0; i < pool->num_strings; i++) {
        xmlFree(pool->strings[i]);
    }
    xmlFree(pool->strings);
    xmlFree(pool);
}

static int
pool_done(StringPool *pool) {
    return pool->index >= pool->num_strings;
}

static void
pool_reset(StringPool *pool) {
    pool->index = 0;
}

static int
pool_bulk_insert(StringPool *pool, xmlHashTablePtr hash, size_t num) {
    size_t i, j;
    int ret = 0;

    for (i = pool->index, j = 0; i < pool->num_strings && j < num; j++) {
        xmlChar *str[3];
        size_t k;

        while (1) {
            xmlChar tmp_key[1];
            int res;

            for (k = 0; k < pool->num_keys; k++)
                str[k] = gen_random_string(pool->id);

            switch (pool->num_keys) {
                case 1:
                    res = xmlHashAddEntry(hash, str[0], tmp_key);
                    if (res == 0 &&
                        xmlHashUpdateEntry(hash, str[0], str[0], NULL) != 0)
                        ret = -1;
                    break;
                case 2:
                    res = xmlHashAddEntry2(hash, str[0], str[1], tmp_key);
                    if (res == 0 &&
                        xmlHashUpdateEntry2(hash, str[0], str[1], str[0],
                                            NULL) != 0)
                        ret = -1;
                    break;
                case 3:
                    res = xmlHashAddEntry3(hash, str[0], str[1], str[2],
                                           tmp_key);
                    if (res == 0 &&
                        xmlHashUpdateEntry3(hash, str[0], str[1], str[2],
                                            str[0], NULL) != 0)
                        ret = -1;
                    break;
            }

            if (res == 0)
                break;
            for (k = 0; k < pool->num_keys; k++)
                xmlFree(str[k]);
        }

        for (k = 0; k < pool->num_keys; k++)
            pool->strings[i++] = str[k];
    }

    pool->index = i;
    return ret;
}

static xmlChar *
hash_qlookup(xmlHashTable *hash, xmlChar **names, size_t num_keys) {
    xmlChar *prefix[3];
    const xmlChar *local[3];
    xmlChar *res;
    size_t i;

    for (i = 0; i < 3; ++i) {
        if (i >= num_keys) {
            prefix[i] = NULL;
            local[i] = NULL;
        } else {
            const xmlChar *name = names[i];
            const xmlChar *colon = BAD_CAST strchr((const char *) name, ':');

            if (colon == NULL) {
                prefix[i] = NULL;
                local[i] = name;
            } else {
                prefix[i] = xmlStrndup(name, colon - name);
                local[i] = &colon[1];
            }
        }
    }

    res = xmlHashQLookup3(hash, prefix[0], local[0], prefix[1], local[1],
                          prefix[2], local[2]);

    for (i = 0; i < 3; ++i)
        xmlFree(prefix[i]);

    return res;
}

static int
pool_bulk_lookup(StringPool *pool, xmlHashTablePtr hash, size_t num,
                 int existing) {
    size_t i, j;
    int ret = 0;

    for (i = pool->index, j = 0; i < pool->num_strings && j < num; j++) {
        xmlChar **str = &pool->strings[i];
        int q;

        for (q = 0; q < 2; q++) {
            xmlChar *res = NULL;

            if (q) {
                res = hash_qlookup(hash, str, pool->num_keys);
            } else {
                switch (pool->num_keys) {
                    case 1:
                        res = xmlHashLookup(hash, str[0]);
                        break;
                    case 2:
                        res = xmlHashLookup2(hash, str[0], str[1]);
                        break;
                    case 3:
                        res = xmlHashLookup3(hash, str[0], str[1], str[2]);
                        break;
                }
            }

            if (existing) {
                if (res != str[0])
                    ret = -1;
            } else {
                if (res != NULL)
                    ret = -1;
            }
        }

        i += pool->num_keys;
    }

    pool->index = i;
    return ret;
}

static int
pool_bulk_remove(StringPool *pool, xmlHashTablePtr hash, size_t num) {
    size_t i, j;
    int ret = 0;

    for (i = pool->index, j = 0; i < pool->num_strings && j < num; j++) {
        xmlChar **str = &pool->strings[i];
        int res = -1;

        switch (pool->num_keys) {
            case 1:
                res = xmlHashRemoveEntry(hash, str[0], NULL);
                break;
            case 2:
                res = xmlHashRemoveEntry2(hash, str[0], str[1], NULL);
                break;
            case 3:
                res = xmlHashRemoveEntry3(hash, str[0], str[1], str[2], NULL);
                break;
        }

        if (res != 0)
            ret = -1;

        i += pool->num_keys;
    }

    pool->index = i;
    return ret;
}

static int
test_hash(size_t num_entries, size_t num_keys, int use_dict) {
    xmlDict *dict = NULL;
    xmlHashTable *hash;
    StringPool *pool1, *pool2;
    int ret = 0;

    if (use_dict) {
        dict = xmlDictCreate();
        hash = xmlHashCreateDict(0, dict);
    } else {
        hash = xmlHashCreate(0);
    }
    pool1 = pool_new(num_entries, num_keys, '1');
    pool2 = pool_new(num_entries, num_keys, '2');

    /* Insert all strings from pool2 and about half of pool1. */
    while (!pool_done(pool2)) {
        if (pool_bulk_insert(pool1, hash, my_rand(50)) != 0) {
            fprintf(stderr, "pool1: hash insert failed\n");
            ret = 1;
        }
        if (pool_bulk_insert(pool2, hash, my_rand(100)) != 0) {
            fprintf(stderr, "pool1: hash insert failed\n");
            ret = 1;
        }
    }

    /* Check existing entries */
    pool_reset(pool2);
    if (pool_bulk_lookup(pool2, hash, pool2->num_entries, 1) != 0) {
        fprintf(stderr, "pool2: hash lookup failed\n");
        ret = 1;
    }

    /* Remove all strings from pool2 and insert the rest of pool1. */
    pool_reset(pool2);
    while (!pool_done(pool1) || !pool_done(pool2)) {
        if (pool_bulk_insert(pool1, hash, my_rand(50)) != 0) {
            fprintf(stderr, "pool1: hash insert failed\n");
            ret = 1;
        }
        if (pool_bulk_remove(pool2, hash, my_rand(100)) != 0) {
            fprintf(stderr, "pool2: hash remove failed\n");
            ret = 1;
        }
    }

    /* Check existing entries */
    pool_reset(pool1);
    if (pool_bulk_lookup(pool1, hash, pool1->num_entries, 1) != 0) {
        fprintf(stderr, "pool1: hash lookup failed\n");
        ret = 1;
    }

    /* Check removed entries */
    pool_reset(pool2);
    if (pool_bulk_lookup(pool2, hash, pool2->num_entries, 0) != 0) {
        fprintf(stderr, "pool2: hash lookup succeeded unexpectedly\n");
        ret = 1;
    }

    pool_free(pool1);
    pool_free(pool2);
    xmlHashFree(hash, NULL);
    xmlDictFree(dict);

    return ret;
}

static int
testall_hash(void) {
    size_t num_keys;

    for (num_keys = 1; num_keys <= 3; num_keys++) {
        size_t num_strings;
        size_t max_strings = num_keys == 1 ? 100000 : 1000;

        for (num_strings = 10; num_strings <= max_strings; num_strings *= 10) {
            size_t reps, i;

            reps = 1000 / num_strings;
            if (reps == 0)
                reps = 1;

            for (i = 0; i < reps; i++) {
                if (test_hash(num_strings, num_keys, /* use_dict */ 0) != 0)
                    return(1);
            }

            if (test_hash(num_strings, num_keys, /* use_dict */ 1) != 0)
                return(1);
        }
    }

    return(0);
}


/**** main ****/

int
main(void) {
    int ret = 0;

    LIBXML_TEST_VERSION

    if (testall_dict() != 0) {
        fprintf(stderr, "dictionary tests failed\n");
        ret = 1;
    }
    if (testall_hash() != 0) {
        fprintf(stderr, "hash tests failed\n");
        ret = 1;
    }

    xmlCleanupParser();
    return(ret);
}
