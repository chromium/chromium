/*
 * fuzz.c: Common functions for fuzzing.
 *
 * See Copyright for the status of this software.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libxml/hash.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/tree.h>
#include <libxml/xmlIO.h>
#include "fuzz.h"

typedef struct {
    const char *data;
    size_t size;
} xmlFuzzEntityInfo;

/* Single static instance for now */
static struct {
    /* Original data */
    const char *data;
    size_t size;

    /* Remaining data */
    const char *ptr;
    size_t remaining;

    /* Buffer for unescaped strings */
    char *outBuf;
    char *outPtr; /* Free space at end of buffer */

    xmlHashTablePtr entities; /* Maps URLs to xmlFuzzEntityInfos */

    /* The first entity is the main entity. */
    const char *mainUrl;
    xmlFuzzEntityInfo *mainEntity;
} fuzzData;

size_t fuzzNumAllocs;
size_t fuzzMaxAllocs;

/**
 * xmlFuzzErrorFunc:
 *
 * An error function that simply discards all errors.
 */
void
xmlFuzzErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg ATTRIBUTE_UNUSED,
                 ...) {
}

/*
 * Malloc failure injection.
 *
 * Quick tip to debug complicated issues: Increase MALLOC_OFFSET until
 * the crash disappears (or a different issue is triggered). Then set
 * the offset to the highest value that produces a crash and set
 * MALLOC_ABORT to 1 to see which failed memory allocation causes the
 * issue.
 */

#define XML_FUZZ_MALLOC_OFFSET  0
#define XML_FUZZ_MALLOC_ABORT   0

static void *
xmlFuzzMalloc(size_t size) {
    if (fuzzMaxAllocs > 0) {
        if (fuzzNumAllocs >= fuzzMaxAllocs - 1)
#if XML_FUZZ_MALLOC_ABORT
            abort();
#else
            return(NULL);
#endif
        fuzzNumAllocs += 1;
    }
    return malloc(size);
}

static void *
xmlFuzzRealloc(void *ptr, size_t size) {
    if (fuzzMaxAllocs > 0) {
        if (fuzzNumAllocs >= fuzzMaxAllocs - 1)
#if XML_FUZZ_MALLOC_ABORT
            abort();
#else
            return(NULL);
#endif
        fuzzNumAllocs += 1;
    }
    return realloc(ptr, size);
}

void
xmlFuzzMemSetup(void) {
    xmlMemSetup(free, xmlFuzzMalloc, xmlFuzzRealloc, xmlMemStrdup);
}

void
xmlFuzzMemSetLimit(size_t limit) {
    fuzzNumAllocs = 0;
    fuzzMaxAllocs = limit ? limit + XML_FUZZ_MALLOC_OFFSET : 0;
}

/**
 * xmlFuzzDataInit:
 *
 * Initialize fuzz data provider.
 */
void
xmlFuzzDataInit(const char *data, size_t size) {
    fuzzData.data = data;
    fuzzData.size = size;
    fuzzData.ptr = data;
    fuzzData.remaining = size;

    fuzzData.outBuf = xmlMalloc(size + 1);
    fuzzData.outPtr = fuzzData.outBuf;

    fuzzData.entities = xmlHashCreate(8);
    fuzzData.mainUrl = NULL;
    fuzzData.mainEntity = NULL;
}

/**
 * xmlFuzzDataFree:
 *
 * Cleanup fuzz data provider.
 */
void
xmlFuzzDataCleanup(void) {
    xmlFree(fuzzData.outBuf);
    xmlHashFree(fuzzData.entities, xmlHashDefaultDeallocator);
}

/**
 * xmlFuzzWriteInt:
 * @out:  output file
 * @v:  integer to write
 * @size:  size of integer in bytes
 *
 * Write an integer to the fuzz data.
 */
void
xmlFuzzWriteInt(FILE *out, size_t v, int size) {
    int shift;

    while (size > (int) sizeof(size_t)) {
        putc(0, out);
        size--;
    }

    shift = size * 8;
    while (shift > 0) {
        shift -= 8;
        putc((v >> shift) & 255, out);
    }
}

/**
 * xmlFuzzReadInt:
 * @size:  size of integer in bytes
 *
 * Read an integer from the fuzz data.
 */
size_t
xmlFuzzReadInt(int size) {
    size_t ret = 0;

    while ((size > 0) && (fuzzData.remaining > 0)) {
        unsigned char c = (unsigned char) *fuzzData.ptr++;
        fuzzData.remaining--;
        ret = (ret << 8) | c;
        size--;
    }

    return ret;
}

/**
 * xmlFuzzReadRemaining:
 * @size:  size of string in bytes
 *
 * Read remaining bytes from fuzz data.
 */
const char *
xmlFuzzReadRemaining(size_t *size) {
    const char *ret = fuzzData.ptr;

    *size = fuzzData.remaining;
    fuzzData.ptr += fuzzData.remaining;
    fuzzData.remaining = 0;

    return(ret);
}

/*
 * xmlFuzzWriteString:
 * @out:  output file
 * @str:  string to write
 *
 * Write a random-length string to file in a format similar to
 * FuzzedDataProvider. Backslash followed by newline marks the end of the
 * string. Two backslashes are used to escape a backslash.
 */
void
xmlFuzzWriteString(FILE *out, const char *str) {
    for (; *str; str++) {
        int c = (unsigned char) *str;
        putc(c, out);
        if (c == '\\')
            putc(c, out);
    }
    putc('\\', out);
    putc('\n', out);
}

/**
 * xmlFuzzReadString:
 * @size:  size of string in bytes
 *
 * Read a random-length string from the fuzz data.
 *
 * The format is similar to libFuzzer's FuzzedDataProvider but treats
 * backslash followed by newline as end of string. This makes the fuzz data
 * more readable. A backslash character is escaped with another backslash.
 *
 * Returns a zero-terminated string or NULL if the fuzz data is exhausted.
 */
const char *
xmlFuzzReadString(size_t *size) {
    const char *out = fuzzData.outPtr;

    while (fuzzData.remaining > 0) {
        int c = *fuzzData.ptr++;
        fuzzData.remaining--;

        if ((c == '\\') && (fuzzData.remaining > 0)) {
            int c2 = *fuzzData.ptr;

            if (c2 == '\n') {
                fuzzData.ptr++;
                fuzzData.remaining--;
                if (size != NULL)
                    *size = fuzzData.outPtr - out;
                *fuzzData.outPtr++ = '\0';
                return(out);
            }
            if (c2 == '\\') {
                fuzzData.ptr++;
                fuzzData.remaining--;
            }
        }

        *fuzzData.outPtr++ = c;
    }

    if (fuzzData.outPtr > out) {
        if (size != NULL)
            *size = fuzzData.outPtr - out;
        *fuzzData.outPtr++ = '\0';
        return(out);
    }

    if (size != NULL)
        *size = 0;
    return(NULL);
}

/**
 * xmlFuzzReadEntities:
 *
 * Read entities like the main XML file, external DTDs, external parsed
 * entities from fuzz data.
 */
void
xmlFuzzReadEntities(void) {
    size_t num = 0;

    while (1) {
        const char *url, *entity;
        size_t entitySize;
        xmlFuzzEntityInfo *entityInfo;

        url = xmlFuzzReadString(NULL);
        if (url == NULL) break;

        entity = xmlFuzzReadString(&entitySize);
        if (entity == NULL) break;

        if (xmlHashLookup(fuzzData.entities, (xmlChar *)url) == NULL) {
            entityInfo = xmlMalloc(sizeof(xmlFuzzEntityInfo));
            if (entityInfo == NULL)
                break;
            entityInfo->data = entity;
            entityInfo->size = entitySize;

            xmlHashAddEntry(fuzzData.entities, (xmlChar *)url, entityInfo);

            if (num == 0) {
                fuzzData.mainUrl = url;
                fuzzData.mainEntity = entityInfo;
            }

            num++;
        }
    }
}

/**
 * xmlFuzzMainUrl:
 *
 * Returns the main URL.
 */
const char *
xmlFuzzMainUrl(void) {
    return(fuzzData.mainUrl);
}

/**
 * xmlFuzzMainEntity:
 * @size:  size of the main entity in bytes
 *
 * Returns the main entity.
 */
const char *
xmlFuzzMainEntity(size_t *size) {
    if (fuzzData.mainEntity == NULL)
        return(NULL);
    *size = fuzzData.mainEntity->size;
    return(fuzzData.mainEntity->data);
}

/**
 * xmlFuzzEntityLoader:
 *
 * The entity loader for fuzz data.
 */
xmlParserInputPtr
xmlFuzzEntityLoader(const char *URL, const char *ID ATTRIBUTE_UNUSED,
                    xmlParserCtxtPtr ctxt) {
    xmlParserInputPtr input;
    xmlFuzzEntityInfo *entity;

    if (URL == NULL)
        return(NULL);
    entity = xmlHashLookup(fuzzData.entities, (xmlChar *) URL);
    if (entity == NULL)
        return(NULL);

    input = xmlNewInputStream(ctxt);
    if (input == NULL)
        return(NULL);
    input->filename = (char *) xmlCharStrdup(URL);
    input->buf = xmlParserInputBufferCreateMem(entity->data, entity->size,
                                               XML_CHAR_ENCODING_NONE);
    if (input->buf == NULL) {
        xmlFreeInputStream(input);
        return(NULL);
    }
    input->base = input->cur = xmlBufContent(input->buf->buffer);
    input->end = input->base + entity->size;

    return input;
}

char *
xmlSlurpFile(const char *path, size_t *sizeRet) {
    FILE *file;
    struct stat statbuf;
    char *data;
    size_t size;

    if ((stat(path, &statbuf) != 0) || (!S_ISREG(statbuf.st_mode)))
        return(NULL);
    size = statbuf.st_size;
    file = fopen(path, "rb");
    if (file == NULL)
        return(NULL);
    data = xmlMalloc(size + 1);
    if (data != NULL) {
        if (fread(data, 1, size, file) != size) {
            xmlFree(data);
            data = NULL;
        } else {
            data[size] = 0;
            if (sizeRet != NULL)
                *sizeRet = size;
        }
    }
    fclose(file);

    return(data);
}

