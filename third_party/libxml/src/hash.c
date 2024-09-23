/*
 * hash.c: hash tables
 *
 * Hash table with open addressing, linear probing and
 * Robin Hood reordering.
 *
 * See Copyright for the status of this software.
 */

#define IN_LIBXML
#include "libxml.h"

#include <string.h>
#include <limits.h>

#include <libxml/parser.h>
#include <libxml/hash.h>
#include <libxml/dict.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>

#include "private/dict.h"

#ifndef SIZE_MAX
  #define SIZE_MAX ((size_t) -1)
#endif

#define MAX_FILL_NUM 7
#define MAX_FILL_DENOM 8
#define MIN_HASH_SIZE 8
#define MAX_HASH_SIZE (1u << 31)

/*
 * A single entry in the hash table
 */
typedef struct {
    unsigned hashValue; /* 0 means unoccupied, occupied entries have the
                         * MAX_HASH_SIZE bit set to 1 */
    xmlChar *key;
    xmlChar *key2; /* TODO: Don't allocate possibly empty keys */
    xmlChar *key3;
    void *payload;
} xmlHashEntry;

/*
 * The entire hash table
 */
struct _xmlHashTable {
    xmlHashEntry *table;
    unsigned size; /* power of two */
    unsigned nbElems;
    xmlDictPtr dict;
    unsigned randomSeed;
};

static int
xmlHashGrow(xmlHashTablePtr hash, unsigned size);

ATTRIBUTE_NO_SANITIZE_INTEGER
static unsigned
xmlHashValue(unsigned seed, const xmlChar *key, const xmlChar *key2,
             const xmlChar *key3, size_t *lengths) {
    unsigned h1, h2;
    size_t i;

    HASH_INIT(h1, h2, seed);

    for (i = 0; key[i] != 0; i++) {
        HASH_UPDATE(h1, h2, key[i]);
    }
    if (lengths)
        lengths[0] = i;

    HASH_UPDATE(h1, h2, 0);

    if (key2 != NULL) {
        for (i = 0; key2[i] != 0; i++) {
            HASH_UPDATE(h1, h2, key2[i]);
        }
        if (lengths)
            lengths[1] = i;
    }

    HASH_UPDATE(h1, h2, 0);

    if (key3 != NULL) {
        for (i = 0; key3[i] != 0; i++) {
            HASH_UPDATE(h1, h2, key3[i]);
        }
        if (lengths)
            lengths[2] = i;
    }

    HASH_FINISH(h1, h2);

    return(h2);
}

ATTRIBUTE_NO_SANITIZE_INTEGER
static unsigned
xmlHashQNameValue(unsigned seed,
                  const xmlChar *prefix, const xmlChar *name,
                  const xmlChar *prefix2, const xmlChar *name2,
                  const xmlChar *prefix3, const xmlChar *name3) {
    unsigned h1, h2, ch;

    HASH_INIT(h1, h2, seed);

    if (prefix != NULL) {
        while ((ch = *prefix++) != 0) {
            HASH_UPDATE(h1, h2, ch);
        }
        HASH_UPDATE(h1, h2, ':');
    }
    if (name != NULL) {
        while ((ch = *name++) != 0) {
            HASH_UPDATE(h1, h2, ch);
        }
    }
    HASH_UPDATE(h1, h2, 0);
    if (prefix2 != NULL) {
        while ((ch = *prefix2++) != 0) {
            HASH_UPDATE(h1, h2, ch);
        }
        HASH_UPDATE(h1, h2, ':');
    }
    if (name2 != NULL) {
        while ((ch = *name2++) != 0) {
            HASH_UPDATE(h1, h2, ch);
        }
    }
    HASH_UPDATE(h1, h2, 0);
    if (prefix3 != NULL) {
        while ((ch = *prefix3++) != 0) {
            HASH_UPDATE(h1, h2, ch);
        }
        HASH_UPDATE(h1, h2, ':');
    }
    if (name3 != NULL) {
        while ((ch = *name3++) != 0) {
            HASH_UPDATE(h1, h2, ch);
        }
    }

    HASH_FINISH(h1, h2);

    return(h2);
}

/**
 * xmlHashCreate:
 * @size: initial size of the hash table
 *
 * Create a new hash table. Set size to zero if the number of entries
 * can't be estimated.
 *
 * Returns the newly created object, or NULL if a memory allocation failed.
 */
xmlHashTablePtr
xmlHashCreate(int size) {
    xmlHashTablePtr hash;

    xmlInitParser();

    hash = xmlMalloc(sizeof(*hash));
    if (hash == NULL)
        return(NULL);
    hash->dict = NULL;
    hash->size = 0;
    hash->table = NULL;
    hash->nbElems = 0;
    hash->randomSeed = xmlRandom();
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    hash->randomSeed = 0;
#endif

    /*
     * Unless a larger size is passed, the backing table is created
     * lazily with MIN_HASH_SIZE capacity. In practice, there are many
     * hash tables which are never filled.
     */
    if (size > MIN_HASH_SIZE) {
        unsigned newSize = MIN_HASH_SIZE * 2;

        while ((newSize < (unsigned) size) && (newSize < MAX_HASH_SIZE))
            newSize *= 2;

        if (xmlHashGrow(hash, newSize) != 0) {
            xmlFree(hash);
            return(NULL);
        }
    }

    return(hash);
}

/**
 * xmlHashCreateDict:
 * @size: the size of the hash table
 * @dict: a dictionary to use for the hash
 *
 * Create a new hash table backed by a dictionary. This can reduce
 * resource usage considerably if most keys passed to API functions
 * originate from this dictionary.
 *
 * Returns the newly created object, or NULL if a memory allocation failed.
 */
xmlHashTablePtr
xmlHashCreateDict(int size, xmlDictPtr dict) {
    xmlHashTablePtr hash;

    hash = xmlHashCreate(size);
    if (hash != NULL) {
        hash->dict = dict;
        xmlDictReference(dict);
    }
    return(hash);
}

/**
 * xmlHashFree:
 * @hash: hash table
 * @dealloc: deallocator function or NULL
 *
 * Free the hash and its contents. The payload is deallocated with
 * @dealloc if provided.
 */
void
xmlHashFree(xmlHashTablePtr hash, xmlHashDeallocator dealloc) {
    if (hash == NULL)
        return;

    if (hash->table) {
        const xmlHashEntry *end = &hash->table[hash->size];
        const xmlHashEntry *entry;

        for (entry = hash->table; entry < end; entry++) {
            if (entry->hashValue == 0)
                continue;
            if ((dealloc != NULL) && (entry->payload != NULL))
                dealloc(entry->payload, entry->key);
            if (hash->dict == NULL) {
                if (entry->key)
                    xmlFree(entry->key);
                if (entry->key2)
                    xmlFree(entry->key2);
                if (entry->key3)
                    xmlFree(entry->key3);
            }
        }

        xmlFree(hash->table);
    }

    if (hash->dict)
        xmlDictFree(hash->dict);

    xmlFree(hash);
}

/**
 * xmlFastStrEqual:
 * @s1: string
 * @s2: string
 *
 * Compare two strings for equality, allowing NULL values.
 */
static int
xmlFastStrEqual(const xmlChar *s1, const xmlChar *s2) {
    if (s1 == NULL)
        return(s2 == NULL);
    else
        return((s2 != NULL) &&
               (strcmp((const char *) s1, (const char *) s2) == 0));
}

/**
 * xmlHashFindEntry:
 * @hash: hash table, non-NULL, size > 0
 * @key: first string key, non-NULL
 * @key2: second string key
 * @key3: third string key
 * @hashValue: valid hash value of keys
 * @pfound: result of search
 *
 * Try to find a matching hash table entry. If an entry was found, set
 * @found to 1 and return the entry. Otherwise, set @found to 0 and return
 * the location where a new entry should be inserted.
 */
ATTRIBUTE_NO_SANITIZE_INTEGER
static xmlHashEntry *
xmlHashFindEntry(const xmlHashTable *hash, const xmlChar *key,
                 const xmlChar *key2, const xmlChar *key3,
                 unsigned hashValue, int *pfound) {
    xmlHashEntry *entry;
    unsigned mask, pos, displ;
    int found = 0;

    mask = hash->size - 1;
    pos = hashValue & mask;
    entry = &hash->table[pos];

    if (entry->hashValue != 0) {
        /*
         * Robin hood hashing: abort if the displacement of the entry
         * is smaller than the displacement of the key we look for.
         * This also stops at the correct position when inserting.
         */
        displ = 0;
        hashValue |= MAX_HASH_SIZE;

        do {
            if (entry->hashValue == hashValue) {
                if (hash->dict) {
                    if ((entry->key == key) &&
                        (entry->key2 == key2) &&
                        (entry->key3 == key3)) {
                        found = 1;
                        break;
                    }
                }
                if ((strcmp((const char *) entry->key,
                            (const char *) key) == 0) &&
                    (xmlFastStrEqual(entry->key2, key2)) &&
                    (xmlFastStrEqual(entry->key3, key3))) {
                    found = 1;
                    break;
                }
            }

            displ++;
            pos++;
            entry++;
            if ((pos & mask) == 0)
                entry = hash->table;
        } while ((entry->hashValue != 0) &&
                 (((pos - entry->hashValue) & mask) >= displ));
    }

    *pfound = found;
    return(entry);
}

/**
 * xmlHashGrow:
 * @hash: hash table
 * @size: new size of the hash table
 *
 * Resize the hash table.
 *
 * Returns 0 in case of success, -1 if a memory allocation failed.
 */
static int
xmlHashGrow(xmlHashTablePtr hash, unsigned size) {
    const xmlHashEntry *oldentry, *oldend, *end;
    xmlHashEntry *table;
    unsigned oldsize, i;

    /* Add 0 to avoid spurious -Wtype-limits warning on 64-bit GCC */
    if ((size_t) size + 0 > SIZE_MAX / sizeof(table[0]))
        return(-1);
    table = xmlMalloc(size * sizeof(table[0]));
    if (table == NULL)
        return(-1);
    memset(table, 0, size * sizeof(table[0]));

    oldsize = hash->size;
    if (oldsize == 0)
        goto done;

    oldend = &hash->table[oldsize];
    end = &table[size];

    /*
     * Robin Hood sorting order is maintained if we
     *
     * - compute hash indices with modulo
     * - resize by an integer factor
     * - start to copy from the beginning of a probe sequence
     */
    oldentry = hash->table;
    while (oldentry->hashValue != 0) {
        if (++oldentry >= oldend)
            oldentry = hash->table;
    }

    for (i = 0; i < oldsize; i++) {
        if (oldentry->hashValue != 0) {
            xmlHashEntry *entry = &table[oldentry->hashValue & (size - 1)];

            while (entry->hashValue != 0) {
                if (++entry >= end)
                    entry = table;
            }
            *entry = *oldentry;
        }

        if (++oldentry >= oldend)
            oldentry = hash->table;
    }

    xmlFree(hash->table);

done:
    hash->table = table;
    hash->size = size;

    return(0);
}

/**
 * xmlHashUpdateInternal:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 * @key3: third string key
 * @payload: pointer to the payload
 * @dealloc: deallocator function for replaced item or NULL
 * @update: whether existing entries should be updated
 *
 * Internal function to add or update hash entries.
 */
ATTRIBUTE_NO_SANITIZE_INTEGER
static int
xmlHashUpdateInternal(xmlHashTablePtr hash, const xmlChar *key,
                      const xmlChar *key2, const xmlChar *key3,
                      void *payload, xmlHashDeallocator dealloc, int update) {
    xmlChar *copy, *copy2, *copy3;
    xmlHashEntry *entry = NULL;
    size_t lengths[3];
    unsigned hashValue;
    int found = 0;

    if ((hash == NULL) || (key == NULL))
        return(-1);

    /*
     * Check for an existing entry
     */
    hashValue = xmlHashValue(hash->randomSeed, key, key2, key3, lengths);
    if (hash->size > 0)
        entry = xmlHashFindEntry(hash, key, key2, key3, hashValue, &found);
    if (found) {
        if (update) {
            if (dealloc)
                dealloc(entry->payload, entry->key);
            entry->payload = payload;
        }

        return(0);
    }

    /*
     * Grow the hash table if needed
     */
    if (hash->nbElems + 1 > hash->size / MAX_FILL_DENOM * MAX_FILL_NUM) {
        unsigned newSize, mask, displ, pos;

        if (hash->size == 0) {
            newSize = MIN_HASH_SIZE;
        } else {
            /* This guarantees that nbElems < INT_MAX */
            if (hash->size >= MAX_HASH_SIZE)
                return(-1);
            newSize = hash->size * 2;
        }
        if (xmlHashGrow(hash, newSize) != 0)
            return(-1);

        /*
         * Find new entry
         */
        mask = hash->size - 1;
        displ = 0;
        pos = hashValue & mask;
        entry = &hash->table[pos];

        if (entry->hashValue != 0) {
            do {
                displ++;
                pos++;
                entry++;
                if ((pos & mask) == 0)
                    entry = hash->table;
            } while ((entry->hashValue != 0) &&
                     ((pos - entry->hashValue) & mask) >= displ);
        }
    }

    /*
     * Copy keys
     */
    if (hash->dict != NULL) {
        if (xmlDictOwns(hash->dict, key)) {
            copy = (xmlChar *) key;
        } else {
            copy = (xmlChar *) xmlDictLookup(hash->dict, key, -1);
            if (copy == NULL)
                return(-1);
        }

        if ((key2 == NULL) || (xmlDictOwns(hash->dict, key2))) {
            copy2 = (xmlChar *) key2;
        } else {
            copy2 = (xmlChar *) xmlDictLookup(hash->dict, key2, -1);
            if (copy2 == NULL)
                return(-1);
        }
        if ((key3 == NULL) || (xmlDictOwns(hash->dict, key3))) {
            copy3 = (xmlChar *) key3;
        } else {
            copy3 = (xmlChar *) xmlDictLookup(hash->dict, key3, -1);
            if (copy3 == NULL)
                return(-1);
        }
    } else {
        copy = xmlMalloc(lengths[0] + 1);
        if (copy == NULL)
            return(-1);
        memcpy(copy, key, lengths[0] + 1);

        if (key2 != NULL) {
            copy2 = xmlMalloc(lengths[1] + 1);
            if (copy2 == NULL) {
                xmlFree(copy);
                return(-1);
            }
            memcpy(copy2, key2, lengths[1] + 1);
        } else {
            copy2 = NULL;
        }

        if (key3 != NULL) {
            copy3 = xmlMalloc(lengths[2] + 1);
            if (copy3 == NULL) {
                xmlFree(copy);
                xmlFree(copy2);
                return(-1);
            }
            memcpy(copy3, key3, lengths[2] + 1);
        } else {
            copy3 = NULL;
        }
    }

    /*
     * Shift the remainder of the probe sequence to the right
     */
    if (entry->hashValue != 0) {
        const xmlHashEntry *end = &hash->table[hash->size];
        const xmlHashEntry *cur = entry;

        do {
            cur++;
            if (cur >= end)
                cur = hash->table;
        } while (cur->hashValue != 0);

        if (cur < entry) {
            /*
             * If we traversed the end of the buffer, handle the part
             * at the start of the buffer.
             */
            memmove(&hash->table[1], hash->table,
                    (char *) cur - (char *) hash->table);
            cur = end - 1;
            hash->table[0] = *cur;
        }

        memmove(&entry[1], entry, (char *) cur - (char *) entry);
    }

    /*
     * Populate entry
     */
    entry->key = copy;
    entry->key2 = copy2;
    entry->key3 = copy3;
    entry->payload = payload;
    /* OR with MAX_HASH_SIZE to make sure that the value is non-zero */
    entry->hashValue = hashValue | MAX_HASH_SIZE;

    hash->nbElems++;

    return(1);
}

/**
 * xmlHashDefaultDeallocator:
 * @entry: hash table entry
 * @key: the entry's string key
 *
 * Free a hash table entry with xmlFree.
 */
void
xmlHashDefaultDeallocator(void *entry, const xmlChar *key ATTRIBUTE_UNUSED) {
    xmlFree(entry);
}

/**
 * xmlHashAdd:
 * @hash: hash table
 * @key: string key
 * @payload: pointer to the payload
 *
 * Add a hash table entry. If an entry with this key already exists,
 * payload will not be updated and 0 is returned. This return value
 * can't be distinguished from out-of-memory errors, so this function
 * should be used with care.
 *
 * Available since 2.13.0.
 *
 * Returns 1 on success, 0 if an entry exists and -1 in case of error.
 */
int
xmlHashAdd(xmlHashTablePtr hash, const xmlChar *key, void *payload) {
    return(xmlHashUpdateInternal(hash, key, NULL, NULL, payload, NULL, 0));
}

/**
 * xmlHashAdd2:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 * @payload: pointer to the payload
 *
 * Add a hash table entry with two strings as key.
 *
 * See xmlHashAdd.
 *
 * Available since 2.13.0.
 *
 * Returns 1 on success, 0 if an entry exists and -1 in case of error.
 */
int
xmlHashAdd2(xmlHashTablePtr hash, const xmlChar *key,
                 const xmlChar *key2, void *payload) {
    return(xmlHashUpdateInternal(hash, key, key2, NULL, payload, NULL, 0));
}

/**
 * xmlHashAdd3:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 * @key3: third string key
 * @payload: pointer to the payload
 *
 * Add a hash table entry with three strings as key.
 *
 * See xmlHashAdd.
 *
 * Available since 2.13.0.
 *
 * Returns 1 on success, 0 if an entry exists and -1 in case of error.
 */
int
xmlHashAdd3(xmlHashTablePtr hash, const xmlChar *key,
                 const xmlChar *key2, const xmlChar *key3,
                 void *payload) {
    return(xmlHashUpdateInternal(hash, key, key2, key3, payload, NULL, 0));
}

/**
 * xmlHashAddEntry:
 * @hash: hash table
 * @key: string key
 * @payload: pointer to the payload
 *
 * Add a hash table entry. If an entry with this key already exists,
 * payload will not be updated and -1 is returned. This return value
 * can't be distinguished from out-of-memory errors, so this function
 * should be used with care.
 *
 * NOTE: This function doesn't allow to distinguish malloc failures from
 *       existing entries. Use xmlHashAdd instead.
 *
 * Returns 0 on success and -1 in case of error.
 */
int
xmlHashAddEntry(xmlHashTablePtr hash, const xmlChar *key, void *payload) {
    int res = xmlHashUpdateInternal(hash, key, NULL, NULL, payload, NULL, 0);

    if (res == 0)
        res = -1;
    else if (res == 1)
        res = 0;

    return(res);
}

/**
 * xmlHashAddEntry2:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 * @payload: pointer to the payload
 *
 * Add a hash table entry with two strings as key.
 *
 * See xmlHashAddEntry.
 *
 * Returns 0 on success and -1 in case of error.
 */
int
xmlHashAddEntry2(xmlHashTablePtr hash, const xmlChar *key,
                 const xmlChar *key2, void *payload) {
    int res = xmlHashUpdateInternal(hash, key, key2, NULL, payload, NULL, 0);

    if (res == 0)
        res = -1;
    else if (res == 1)
        res = 0;

    return(res);
}

/**
 * xmlHashAddEntry3:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 * @key3: third string key
 * @payload: pointer to the payload
 *
 * Add a hash table entry with three strings as key.
 *
 * See xmlHashAddEntry.
 *
 * Returns 0 on success and -1 in case of error.
 */
int
xmlHashAddEntry3(xmlHashTablePtr hash, const xmlChar *key,
                 const xmlChar *key2, const xmlChar *key3,
                 void *payload) {
    int res = xmlHashUpdateInternal(hash, key, key2, key3, payload, NULL, 0);

    if (res == 0)
        res = -1;
    else if (res == 1)
        res = 0;

    return(res);
}

/**
 * xmlHashUpdateEntry:
 * @hash: hash table
 * @key: string key
 * @payload: pointer to the payload
 * @dealloc: deallocator function for replaced item or NULL
 *
 * Add a hash table entry. If an entry with this key already exists,
 * the old payload will be freed and updated with the new value.
 *
 * Returns 0 in case of success, -1 if a memory allocation failed.
 */
int
xmlHashUpdateEntry(xmlHashTablePtr hash, const xmlChar *key,
                   void *payload, xmlHashDeallocator dealloc) {
    int res = xmlHashUpdateInternal(hash, key, NULL, NULL, payload,
                                    dealloc, 1);

    if (res == 1)
        res = 0;

    return(res);
}

/**
 * xmlHashUpdateEntry2:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 * @payload: pointer to the payload
 * @dealloc: deallocator function for replaced item or NULL
 *
 * Add a hash table entry with two strings as key.
 *
 * See xmlHashUpdateEntry.
 *
 * Returns 0 on success and -1 in case of error.
 */
int
xmlHashUpdateEntry2(xmlHashTablePtr hash, const xmlChar *key,
                   const xmlChar *key2, void *payload,
                   xmlHashDeallocator dealloc) {
    int res = xmlHashUpdateInternal(hash, key, key2, NULL, payload,
                                    dealloc, 1);

    if (res == 1)
        res = 0;

    return(res);
}

/**
 * xmlHashUpdateEntry3:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 * @key3: third string key
 * @payload: pointer to the payload
 * @dealloc: deallocator function for replaced item or NULL
 *
 * Add a hash table entry with three strings as key.
 *
 * See xmlHashUpdateEntry.
 *
 * Returns 0 on success and -1 in case of error.
 */
int
xmlHashUpdateEntry3(xmlHashTablePtr hash, const xmlChar *key,
                   const xmlChar *key2, const xmlChar *key3,
                   void *payload, xmlHashDeallocator dealloc) {
    int res = xmlHashUpdateInternal(hash, key, key2, key3, payload,
                                    dealloc, 1);

    if (res == 1)
        res = 0;

    return(res);
}

/**
 * xmlHashLookup:
 * @hash: hash table
 * @key: string key
 *
 * Find the entry specified by @key.
 *
 * Returns a pointer to the payload or NULL if no entry was found.
 */
void *
xmlHashLookup(xmlHashTablePtr hash, const xmlChar *key) {
    return(xmlHashLookup3(hash, key, NULL, NULL));
}

/**
 * xmlHashLookup2:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 *
 * Find the payload specified by the (@key, @key2) tuple.
 *
 * Returns a pointer to the payload or NULL if no entry was found.
 */
void *
xmlHashLookup2(xmlHashTablePtr hash, const xmlChar *key,
              const xmlChar *key2) {
    return(xmlHashLookup3(hash, key, key2, NULL));
}

/**
 * xmlHashQLookup:
 * @hash: hash table
 * @prefix: prefix of the string key
 * @name: local name of the string key
 *
 * Find the payload specified by the QName @prefix:@name or @name.
 *
 * Returns a pointer to the payload or NULL if no entry was found.
 */
void *
xmlHashQLookup(xmlHashTablePtr hash, const xmlChar *prefix,
               const xmlChar *name) {
    return(xmlHashQLookup3(hash, prefix, name, NULL, NULL, NULL, NULL));
}

/**
 * xmlHashQLookup2:
 * @hash: hash table
 * @prefix: first prefix
 * @name: first local name
 * @prefix2: second prefix
 * @name2: second local name
 *
 * Find the payload specified by the QNames tuple.
 *
 * Returns a pointer to the payload or NULL if no entry was found.
 */
void *
xmlHashQLookup2(xmlHashTablePtr hash, const xmlChar *prefix,
                const xmlChar *name, const xmlChar *prefix2,
                const xmlChar *name2) {
    return(xmlHashQLookup3(hash, prefix, name, prefix2, name2, NULL, NULL));
}

/**
 * xmlHashLookup3:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 * @key3: third string key
 *
 * Find the payload specified by the (@key, @key2, @key3) tuple.
 *
 * Returns a pointer to the payload or NULL if no entry was found.
 */
void *
xmlHashLookup3(xmlHashTablePtr hash, const xmlChar *key,
               const xmlChar *key2, const xmlChar *key3) {
    const xmlHashEntry *entry;
    unsigned hashValue;
    int found;

    if ((hash == NULL) || (hash->size == 0) || (key == NULL))
        return(NULL);
    hashValue = xmlHashValue(hash->randomSeed, key, key2, key3, NULL);
    entry = xmlHashFindEntry(hash, key, key2, key3, hashValue, &found);
    if (found)
        return(entry->payload);
    return(NULL);
}

/**
 * xmlHashQLookup3:
 * @hash: hash table
 * @prefix: first prefix
 * @name: first local name
 * @prefix2: second prefix
 * @name2: second local name
 * @prefix3: third prefix
 * @name3: third local name
 *
 * Find the payload specified by the QNames tuple.
 *
 * Returns a pointer to the payload or NULL if no entry was found.
 */
ATTRIBUTE_NO_SANITIZE_INTEGER
void *
xmlHashQLookup3(xmlHashTablePtr hash,
                const xmlChar *prefix, const xmlChar *name,
                const xmlChar *prefix2, const xmlChar *name2,
                const xmlChar *prefix3, const xmlChar *name3) {
    const xmlHashEntry *entry;
    unsigned hashValue, mask, pos, displ;

    if ((hash == NULL) || (hash->size == 0) || (name == NULL))
        return(NULL);

    hashValue = xmlHashQNameValue(hash->randomSeed, prefix, name, prefix2,
                                  name2, prefix3, name3);
    mask = hash->size - 1;
    pos = hashValue & mask;
    entry = &hash->table[pos];

    if (entry->hashValue != 0) {
        displ = 0;
        hashValue |= MAX_HASH_SIZE;

        do {
            if ((hashValue == entry->hashValue) &&
                (xmlStrQEqual(prefix, name, entry->key)) &&
                (xmlStrQEqual(prefix2, name2, entry->key2)) &&
                (xmlStrQEqual(prefix3, name3, entry->key3)))
                return(entry->payload);

            displ++;
            pos++;
            entry++;
            if ((pos & mask) == 0)
                entry = hash->table;
        } while ((entry->hashValue != 0) &&
                 (((pos - entry->hashValue) & mask) >= displ));
    }

    return(NULL);
}

typedef struct {
    xmlHashScanner scan;
    void *data;
} stubData;

static void
stubHashScannerFull(void *payload, void *data, const xmlChar *key,
                    const xmlChar *key2 ATTRIBUTE_UNUSED,
                    const xmlChar *key3 ATTRIBUTE_UNUSED) {
    stubData *sdata = (stubData *) data;
    sdata->scan(payload, sdata->data, key);
}

/**
 * xmlHashScan:
 * @hash: hash table
 * @scan: scanner function for items in the hash
 * @data: extra data passed to @scan
 *
 * Scan the hash @table and apply @scan to each value.
 */
void
xmlHashScan(xmlHashTablePtr hash, xmlHashScanner scan, void *data) {
    stubData sdata;
    sdata.data = data;
    sdata.scan = scan;
    xmlHashScanFull(hash, stubHashScannerFull, &sdata);
}

/**
 * xmlHashScanFull:
 * @hash: hash table
 * @scan: scanner function for items in the hash
 * @data: extra data passed to @scan
 *
 * Scan the hash @table and apply @scan to each value.
 */
void
xmlHashScanFull(xmlHashTablePtr hash, xmlHashScannerFull scan, void *data) {
    const xmlHashEntry *entry, *end;
    xmlHashEntry old;
    unsigned i;

    if ((hash == NULL) || (hash->size == 0) || (scan == NULL))
        return;

    /*
     * We must handle the case that a scanned entry is removed when executing
     * the callback (xmlCleanSpecialAttr and possibly other places).
     *
     * Find the start of a probe sequence to avoid scanning entries twice if
     * a deletion happens.
     */
    entry = hash->table;
    end = &hash->table[hash->size];
    while (entry->hashValue != 0) {
        if (++entry >= end)
            entry = hash->table;
    }

    for (i = 0; i < hash->size; i++) {
        if ((entry->hashValue != 0) && (entry->payload != NULL)) {
            /*
             * Make sure to rescan after a possible deletion.
             */
            do {
                old = *entry;
                scan(entry->payload, data, entry->key, entry->key2, entry->key3);
            } while ((entry->hashValue != 0) &&
                     (entry->payload != NULL) &&
                     ((entry->key != old.key) ||
                      (entry->key2 != old.key2) ||
                      (entry->key3 != old.key3)));
        }
        if (++entry >= end)
            entry = hash->table;
    }
}

/**
 * xmlHashScan3:
 * @hash: hash table
 * @key: first string key or NULL
 * @key2: second string key or NULL
 * @key3: third string key or NULL
 * @scan: scanner function for items in the hash
 * @data: extra data passed to @scan
 *
 * Scan the hash @table and apply @scan to each value matching
 * (@key, @key2, @key3) tuple. If one of the keys is null,
 * the comparison is considered to match.
 */
void
xmlHashScan3(xmlHashTablePtr hash, const xmlChar *key,
             const xmlChar *key2, const xmlChar *key3,
             xmlHashScanner scan, void *data) {
    stubData sdata;
    sdata.data = data;
    sdata.scan = scan;
    xmlHashScanFull3(hash, key, key2, key3, stubHashScannerFull, &sdata);
}

/**
 * xmlHashScanFull3:
 * @hash: hash table
 * @key: first string key or NULL
 * @key2: second string key or NULL
 * @key3: third string key or NULL
 * @scan: scanner function for items in the hash
 * @data: extra data passed to @scan
 *
 * Scan the hash @table and apply @scan to each value matching
 * (@key, @key2, @key3) tuple. If one of the keys is null,
 * the comparison is considered to match.
 */
void
xmlHashScanFull3(xmlHashTablePtr hash, const xmlChar *key,
                 const xmlChar *key2, const xmlChar *key3,
                 xmlHashScannerFull scan, void *data) {
    const xmlHashEntry *entry, *end;
    xmlHashEntry old;
    unsigned i;

    if ((hash == NULL) || (hash->size == 0) || (scan == NULL))
        return;

    /*
     * We must handle the case that a scanned entry is removed when executing
     * the callback (xmlCleanSpecialAttr and possibly other places).
     *
     * Find the start of a probe sequence to avoid scanning entries twice if
     * a deletion happens.
     */
    entry = hash->table;
    end = &hash->table[hash->size];
    while (entry->hashValue != 0) {
        if (++entry >= end)
            entry = hash->table;
    }

    for (i = 0; i < hash->size; i++) {
        if ((entry->hashValue != 0) && (entry->payload != NULL)) {
            /*
             * Make sure to rescan after a possible deletion.
             */
            do {
                if (((key != NULL) && (strcmp((const char *) key,
                                              (const char *) entry->key) != 0)) ||
                    ((key2 != NULL) && (!xmlFastStrEqual(key2, entry->key2))) ||
                    ((key3 != NULL) && (!xmlFastStrEqual(key3, entry->key3))))
                    break;
                old = *entry;
                scan(entry->payload, data, entry->key, entry->key2, entry->key3);
            } while ((entry->hashValue != 0) &&
                     (entry->payload != NULL) &&
                     ((entry->key != old.key) ||
                      (entry->key2 != old.key2) ||
                      (entry->key3 != old.key3)));
        }
        if (++entry >= end)
            entry = hash->table;
    }
}

/*
 * xmlHashCopySafe:
 * @hash: hash table
 * @copyFunc: copier function for items in the hash
 * @deallocFunc: deallocation function in case of errors
 *
 * Copy the hash table using @copyFunc to copy payloads.
 *
 * Available since 2.13.0.
 *
 * Returns the new table or NULL if a memory allocation failed.
 */
xmlHashTablePtr
xmlHashCopySafe(xmlHashTablePtr hash, xmlHashCopier copyFunc,
                xmlHashDeallocator deallocFunc) {
    const xmlHashEntry *entry, *end;
    xmlHashTablePtr ret;

    if ((hash == NULL) || (copyFunc == NULL))
        return(NULL);

    ret = xmlHashCreate(hash->size);
    if (ret == NULL)
        return(NULL);

    if (hash->size == 0)
        return(ret);

    end = &hash->table[hash->size];

    for (entry = hash->table; entry < end; entry++) {
        if (entry->hashValue != 0) {
            void *copy;

            copy = copyFunc(entry->payload, entry->key);
            if (copy == NULL)
                goto error;
            if (xmlHashAdd3(ret, entry->key, entry->key2, entry->key3,
                            copy) <= 0) {
                if (deallocFunc != NULL)
                    deallocFunc(copy, entry->key);
                goto error;
            }
        }
    }

    return(ret);

error:
    xmlHashFree(ret, deallocFunc);
    return(NULL);
}

/*
 * xmlHashCopy:
 * @hash: hash table
 * @copy: copier function for items in the hash
 *
 * DEPRECATED: Leaks memory in error case.
 *
 * Copy the hash table using @copy to copy payloads.
 *
 * Returns the new table or NULL if a memory allocation failed.
 */
xmlHashTablePtr
xmlHashCopy(xmlHashTablePtr hash, xmlHashCopier copy) {
    return(xmlHashCopySafe(hash, copy, NULL));
}

/**
 * xmlHashSize:
 * @hash: hash table
 *
 * Query the number of elements in the hash table.
 *
 * Returns the number of elements in the hash table or
 * -1 in case of error.
 */
int
xmlHashSize(xmlHashTablePtr hash) {
    if (hash == NULL)
        return(-1);
    return(hash->nbElems);
}

/**
 * xmlHashRemoveEntry:
 * @hash: hash table
 * @key: string key
 * @dealloc: deallocator function for removed item or NULL
 *
 * Find the entry specified by the @key and remove it from the hash table.
 * Payload will be freed with @dealloc.
 *
 * Returns 0 on success and -1 if no entry was found.
 */
int xmlHashRemoveEntry(xmlHashTablePtr hash, const xmlChar *key,
                       xmlHashDeallocator dealloc) {
    return(xmlHashRemoveEntry3(hash, key, NULL, NULL, dealloc));
}

/**
 * xmlHashRemoveEntry2:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 * @dealloc: deallocator function for removed item or NULL
 *
 * Remove an entry with two strings as key.
 *
 * See xmlHashRemoveEntry.
 *
 * Returns 0 on success and -1 in case of error.
 */
int
xmlHashRemoveEntry2(xmlHashTablePtr hash, const xmlChar *key,
                    const xmlChar *key2, xmlHashDeallocator dealloc) {
    return(xmlHashRemoveEntry3(hash, key, key2, NULL, dealloc));
}

/**
 * xmlHashRemoveEntry3:
 * @hash: hash table
 * @key: first string key
 * @key2: second string key
 * @key3: third string key
 * @dealloc: deallocator function for removed item or NULL
 *
 * Remove an entry with three strings as key.
 *
 * See xmlHashRemoveEntry.
 *
 * Returns 0 on success and -1 in case of error.
 */
ATTRIBUTE_NO_SANITIZE_INTEGER
int
xmlHashRemoveEntry3(xmlHashTablePtr hash, const xmlChar *key,
                    const xmlChar *key2, const xmlChar *key3,
                    xmlHashDeallocator dealloc) {
    xmlHashEntry *entry, *cur, *next;
    unsigned hashValue, mask, pos, nextpos;
    int found;

    if ((hash == NULL) || (hash->size == 0) || (key == NULL))
        return(-1);

    hashValue = xmlHashValue(hash->randomSeed, key, key2, key3, NULL);
    entry = xmlHashFindEntry(hash, key, key2, key3, hashValue, &found);
    if (!found)
        return(-1);

    if ((dealloc != NULL) && (entry->payload != NULL))
        dealloc(entry->payload, entry->key);
    if (hash->dict == NULL) {
        if (entry->key)
            xmlFree(entry->key);
        if (entry->key2)
            xmlFree(entry->key2);
        if (entry->key3)
            xmlFree(entry->key3);
    }

    /*
     * Find end of probe sequence. Entries at their initial probe
     * position start a new sequence.
     */
    mask = hash->size - 1;
    pos = entry - hash->table;
    cur = entry;

    while (1) {
        nextpos = pos + 1;
        next = cur + 1;
        if ((nextpos & mask) == 0)
            next = hash->table;

        if ((next->hashValue == 0) ||
            (((next->hashValue - nextpos) & mask) == 0))
            break;

        cur = next;
        pos = nextpos;
    }

    /*
     * Backward shift
     */
    next = entry + 1;

    if (cur < entry) {
        xmlHashEntry *end = &hash->table[hash->size];

        memmove(entry, next, (char *) end - (char *) next);
        entry = hash->table;
        end[-1] = *entry;
        next = entry + 1;
    }

    memmove(entry, next, (char *) cur - (char *) entry);

    /*
     * Update entry
     */
    cur->hashValue = 0;

    hash->nbElems--;

    return(0);
}

