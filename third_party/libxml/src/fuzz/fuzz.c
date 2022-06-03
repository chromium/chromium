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

/**
 * xmlFuzzErrorFunc:
 *
 * An error function that simply discards all errors.
 */
void
xmlFuzzErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg ATTRIBUTE_UNUSED,
                 ...) {
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
 * xmlFuzzReadInt:
 * @size:  size of string in bytes
 *
 * Read an integer from the fuzz data.
 */
int
xmlFuzzReadInt() {
    int ret;

    if (fuzzData.remaining < sizeof(int))
        return(0);
    memcpy(&ret, fuzzData.ptr, sizeof(int));
    fuzzData.ptr += sizeof(int);
    fuzzData.remaining -= sizeof(int);

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
        *size = fuzzData.outPtr - out;
        *fuzzData.outPtr++ = '\0';
        return(out);
    }

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
        size_t urlSize, entitySize;
        xmlFuzzEntityInfo *entityInfo;
        
        url = xmlFuzzReadString(&urlSize);
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
    input->filename = NULL;
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

/**
 * xmlFuzzExtractStrings:
 *
 * Extract C strings from input data. Use exact-size allocations to detect
 * potential memory errors.
 */
size_t
xmlFuzzExtractStrings(const char *data, size_t size, char **strings,
                      size_t numStrings) {
    const char *start = data;
    const char *end = data + size;
    size_t i = 0, ret;

    while (i < numStrings) {
        size_t strSize = end - start;
        const char *zero = memchr(start, 0, strSize);

        if (zero != NULL)
            strSize = zero - start;

        strings[i] = xmlMalloc(strSize + 1);
        memcpy(strings[i], start, strSize);
        strings[i][strSize] = '\0';

        i++;
        if (zero != NULL)
            start = zero + 1;
        else
            break;
    }

    ret = i;

    while (i < numStrings) {
        strings[i] = NULL;
        i++;
    }

    return(ret);
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

