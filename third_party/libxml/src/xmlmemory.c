/*
 * xmlmemory.c:  libxml memory allocator wrapper.
 *
 * daniel@veillard.com
 */

#define IN_LIBXML
#include "libxml.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include <libxml/xmlmemory.h>
#include <libxml/xmlerror.h>
#include <libxml/parser.h>
#include <libxml/threads.h>

#include "private/error.h"
#include "private/memory.h"
#include "private/threads.h"

static unsigned long  debugMemSize = 0;
static unsigned long  debugMemBlocks = 0;
static xmlMutex xmlMemMutex;

/************************************************************************
 *									*
 *		Macros, variables and associated types			*
 *									*
 ************************************************************************/

/*
 * Each of the blocks allocated begin with a header containing information
 */

#define MEMTAG 0x5aa5U

typedef struct memnod {
    unsigned int   mh_tag;
    size_t         mh_size;
} MEMHDR;

#ifdef SUN4
#define ALIGN_SIZE  16
#else
#define ALIGN_SIZE  sizeof(double)
#endif
#define RESERVE_SIZE (((sizeof(MEMHDR) + ALIGN_SIZE - 1) \
		      / ALIGN_SIZE ) * ALIGN_SIZE)

#define MAX_SIZE_T ((size_t)-1)

#define CLIENT_2_HDR(a) ((void *) (((char *) (a)) - RESERVE_SIZE))
#define HDR_2_CLIENT(a)    ((void *) (((char *) (a)) + RESERVE_SIZE))

/**
 * xmlMallocLoc:
 * @size:  an int specifying the size in byte to allocate.
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * DEPRECATED: don't use
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */
void *
xmlMallocLoc(size_t size, const char *file ATTRIBUTE_UNUSED,
             int line ATTRIBUTE_UNUSED)
{
    return(xmlMemMalloc(size));
}

/**
 * xmlMallocAtomicLoc:
 * @size:  an unsigned int specifying the size in byte to allocate.
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * DEPRECATED: don't use
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */
void *
xmlMallocAtomicLoc(size_t size, const char *file ATTRIBUTE_UNUSED,
                   int line ATTRIBUTE_UNUSED)
{
    return(xmlMemMalloc(size));
}

/**
 * xmlMemMalloc:
 * @size:  an int specifying the size in byte to allocate.
 *
 * a malloc() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */
void *
xmlMemMalloc(size_t size)
{
    MEMHDR *p;

    xmlInitParser();

    if (size > (MAX_SIZE_T - RESERVE_SIZE))
        return(NULL);

    p = (MEMHDR *) malloc(RESERVE_SIZE + size);
    if (!p)
        return(NULL);
    p->mh_tag = MEMTAG;
    p->mh_size = size;

    xmlMutexLock(&xmlMemMutex);
    debugMemSize += size;
    debugMemBlocks++;
    xmlMutexUnlock(&xmlMemMutex);

    return(HDR_2_CLIENT(p));
}

/**
 * xmlReallocLoc:
 * @ptr:  the initial memory block pointer
 * @size:  an int specifying the size in byte to allocate.
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * DEPRECATED: don't use
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */
void *
xmlReallocLoc(void *ptr, size_t size, const char *file ATTRIBUTE_UNUSED,
              int line ATTRIBUTE_UNUSED)
{
    return(xmlMemRealloc(ptr, size));
}

/**
 * xmlMemRealloc:
 * @ptr:  the initial memory block pointer
 * @size:  an int specifying the size in byte to allocate.
 *
 * a realloc() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */
void *
xmlMemRealloc(void *ptr, size_t size) {
    MEMHDR *p, *tmp;
    size_t oldSize;

    if (ptr == NULL)
        return(xmlMemMalloc(size));

    xmlInitParser();

    if (size > (MAX_SIZE_T - RESERVE_SIZE))
        return(NULL);

    p = CLIENT_2_HDR(ptr);
    if (p->mh_tag != MEMTAG) {
        xmlPrintErrorMessage("xmlMemRealloc: Tag error\n");
        return(NULL);
    }
    oldSize = p->mh_size;
    p->mh_tag = ~MEMTAG;

    tmp = (MEMHDR *) realloc(p, RESERVE_SIZE + size);
    if (!tmp) {
        p->mh_tag = MEMTAG;
        return(NULL);
    }
    p = tmp;
    p->mh_tag = MEMTAG;
    p->mh_size = size;

    xmlMutexLock(&xmlMemMutex);
    debugMemSize -= oldSize;
    debugMemSize += size;
    xmlMutexUnlock(&xmlMemMutex);

    return(HDR_2_CLIENT(p));
}

/**
 * xmlMemFree:
 * @ptr:  the memory block pointer
 *
 * a free() equivalent, with error checking.
 */
void
xmlMemFree(void *ptr)
{
    MEMHDR *p;

    if (ptr == NULL)
        return;

    if (ptr == (void *) -1) {
        xmlPrintErrorMessage("xmlMemFree: Pointer from freed area\n");
        return;
    }

    p = CLIENT_2_HDR(ptr);
    if (p->mh_tag != MEMTAG) {
        xmlPrintErrorMessage("xmlMemFree: Tag error\n");
        return;
    }
    p->mh_tag = ~MEMTAG;
    memset(ptr, -1, p->mh_size);

    xmlMutexLock(&xmlMemMutex);
    debugMemSize -= p->mh_size;
    debugMemBlocks--;
    xmlMutexUnlock(&xmlMemMutex);

    free(p);
}

/**
 * xmlMemStrdupLoc:
 * @str:  the initial string pointer
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * DEPRECATED: don't use
 *
 * Returns a pointer to the new string or NULL if allocation error occurred.
 */
char *
xmlMemStrdupLoc(const char *str, const char *file ATTRIBUTE_UNUSED,
                int line ATTRIBUTE_UNUSED)
{
    return(xmlMemoryStrdup(str));
}

/**
 * xmlMemoryStrdup:
 * @str:  the initial string pointer
 *
 * a strdup() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the new string or NULL if allocation error occurred.
 */
char *
xmlMemoryStrdup(const char *str) {
    char *s;
    size_t size = strlen(str) + 1;
    MEMHDR *p;

    xmlInitParser();

    if (size > (MAX_SIZE_T - RESERVE_SIZE))
        return(NULL);

    p = (MEMHDR *) malloc(RESERVE_SIZE + size);
    if (!p)
        return(NULL);
    p->mh_tag = MEMTAG;
    p->mh_size = size;

    xmlMutexLock(&xmlMemMutex);
    debugMemSize += size;
    debugMemBlocks++;
    xmlMutexUnlock(&xmlMemMutex);

    s = (char *) HDR_2_CLIENT(p);

    memcpy(s, str, size);

    return(s);
}

/**
 * xmlMemSize:
 * @ptr:  pointer to the memory allocation
 *
 * Returns the size of a memory allocation.
 */

size_t
xmlMemSize(void *ptr) {
    MEMHDR *p;

    if (ptr == NULL)
	return(0);

    p = CLIENT_2_HDR(ptr);
    if (p->mh_tag != MEMTAG)
        return(0);

    return(p->mh_size);
}

/**
 * xmlMemUsed:
 *
 * Provides the amount of memory currently allocated
 *
 * Returns an int representing the amount of memory allocated.
 */

int
xmlMemUsed(void) {
    return(debugMemSize);
}

/**
 * xmlMemBlocks:
 *
 * Provides the number of memory areas currently allocated
 *
 * Returns an int representing the number of blocks
 */

int
xmlMemBlocks(void) {
    int res;

    xmlMutexLock(&xmlMemMutex);
    res = debugMemBlocks;
    xmlMutexUnlock(&xmlMemMutex);
    return(res);
}

/**
 * xmlMemDisplayLast:
 * @fp:  a FILE descriptor
 * @nbBytes: the amount of memory to dump
 *
 * DEPRECATED: This feature was removed.
 */
void
xmlMemDisplayLast(FILE *fp ATTRIBUTE_UNUSED, long nbBytes ATTRIBUTE_UNUSED)
{
}

/**
 * xmlMemDisplay:
 * @fp:  a FILE descriptor
 *
 * DEPRECATED: This feature was removed.
 */
void
xmlMemDisplay(FILE *fp ATTRIBUTE_UNUSED)
{
}

/**
 * xmlMemShow:
 * @fp:  a FILE descriptor
 * @nr:  number of entries to dump
 *
 * DEPRECATED: This feature was removed.
 */
void
xmlMemShow(FILE *fp ATTRIBUTE_UNUSED, int nr ATTRIBUTE_UNUSED)
{
}

/**
 * xmlMemoryDump:
 *
 * DEPRECATED: This feature was removed.
 */
void
xmlMemoryDump(void)
{
}


/****************************************************************
 *								*
 *		Initialization Routines				*
 *								*
 ****************************************************************/

/**
 * xmlInitMemory:
 *
 * DEPRECATED: Alias for xmlInitParser.
 *
 * Returns 0.
 */
int
xmlInitMemory(void) {
    xmlInitParser();
    return(0);
}

/**
 * xmlInitMemoryInternal:
 *
 * Initialize the memory layer.
 */
void
xmlInitMemoryInternal(void) {
    xmlInitMutex(&xmlMemMutex);
}

/**
 * xmlCleanupMemory:
 *
 * DEPRECATED: This function is a no-op. Call xmlCleanupParser
 * to free global state but see the warnings there. xmlCleanupParser
 * should be only called once at program exit. In most cases, you don't
 * have call cleanup functions at all.
 */
void
xmlCleanupMemory(void) {
}

/**
 * xmlCleanupMemoryInternal:
 *
 * Free up all the memory allocated by the library for its own
 * use. This should not be called by user level code.
 */
void
xmlCleanupMemoryInternal(void) {
    /*
     * Don't clean up mutex on Windows. Global state destructors can call
     * malloc functions after xmlCleanupParser was called. If memory
     * debugging is enabled, xmlMemMutex can be used after cleanup.
     *
     * See python/tests/thread2.py
     */
#if !defined(LIBXML_THREAD_ENABLED) || !defined(_WIN32)
    xmlCleanupMutex(&xmlMemMutex);
#endif
}

/**
 * xmlMemSetup:
 * @freeFunc: the free() function to use
 * @mallocFunc: the malloc() function to use
 * @reallocFunc: the realloc() function to use
 * @strdupFunc: the strdup() function to use
 *
 * Override the default memory access functions with a new set
 * This has to be called before any other libxml routines !
 *
 * Should this be blocked if there was already some allocations
 * done ?
 *
 * Returns 0 on success
 */
int
xmlMemSetup(xmlFreeFunc freeFunc, xmlMallocFunc mallocFunc,
            xmlReallocFunc reallocFunc, xmlStrdupFunc strdupFunc) {
    if (freeFunc == NULL)
	return(-1);
    if (mallocFunc == NULL)
	return(-1);
    if (reallocFunc == NULL)
	return(-1);
    if (strdupFunc == NULL)
	return(-1);
    xmlFree = freeFunc;
    xmlMalloc = mallocFunc;
    xmlMallocAtomic = mallocFunc;
    xmlRealloc = reallocFunc;
    xmlMemStrdup = strdupFunc;
    return(0);
}

/**
 * xmlMemGet:
 * @freeFunc: place to save the free() function in use
 * @mallocFunc: place to save the malloc() function in use
 * @reallocFunc: place to save the realloc() function in use
 * @strdupFunc: place to save the strdup() function in use
 *
 * Provides the memory access functions set currently in use
 *
 * Returns 0 on success
 */
int
xmlMemGet(xmlFreeFunc *freeFunc, xmlMallocFunc *mallocFunc,
	  xmlReallocFunc *reallocFunc, xmlStrdupFunc *strdupFunc) {
    if (freeFunc != NULL) *freeFunc = xmlFree;
    if (mallocFunc != NULL) *mallocFunc = xmlMalloc;
    if (reallocFunc != NULL) *reallocFunc = xmlRealloc;
    if (strdupFunc != NULL) *strdupFunc = xmlMemStrdup;
    return(0);
}

/**
 * xmlGcMemSetup:
 * @freeFunc: the free() function to use
 * @mallocFunc: the malloc() function to use
 * @mallocAtomicFunc: the malloc() function to use for atomic allocations
 * @reallocFunc: the realloc() function to use
 * @strdupFunc: the strdup() function to use
 *
 * DEPRECATED: Use xmlMemSetup.
 *
 * Override the default memory access functions with a new set
 * This has to be called before any other libxml routines !
 * The mallocAtomicFunc is specialized for atomic block
 * allocations (i.e. of areas  useful for garbage collected memory allocators
 *
 * Should this be blocked if there was already some allocations
 * done ?
 *
 * Returns 0 on success
 */
int
xmlGcMemSetup(xmlFreeFunc freeFunc, xmlMallocFunc mallocFunc,
              xmlMallocFunc mallocAtomicFunc, xmlReallocFunc reallocFunc,
	      xmlStrdupFunc strdupFunc) {
    if (freeFunc == NULL)
	return(-1);
    if (mallocFunc == NULL)
	return(-1);
    if (mallocAtomicFunc == NULL)
	return(-1);
    if (reallocFunc == NULL)
	return(-1);
    if (strdupFunc == NULL)
	return(-1);
    xmlFree = freeFunc;
    xmlMalloc = mallocFunc;
    xmlMallocAtomic = mallocAtomicFunc;
    xmlRealloc = reallocFunc;
    xmlMemStrdup = strdupFunc;
    return(0);
}

/**
 * xmlGcMemGet:
 * @freeFunc: place to save the free() function in use
 * @mallocFunc: place to save the malloc() function in use
 * @mallocAtomicFunc: place to save the atomic malloc() function in use
 * @reallocFunc: place to save the realloc() function in use
 * @strdupFunc: place to save the strdup() function in use
 *
 * DEPRECATED: xmlMemGet.
 *
 * Provides the memory access functions set currently in use
 * The mallocAtomicFunc is specialized for atomic block
 * allocations (i.e. of areas  useful for garbage collected memory allocators
 *
 * Returns 0 on success
 */
int
xmlGcMemGet(xmlFreeFunc *freeFunc, xmlMallocFunc *mallocFunc,
            xmlMallocFunc *mallocAtomicFunc, xmlReallocFunc *reallocFunc,
	    xmlStrdupFunc *strdupFunc) {
    if (freeFunc != NULL) *freeFunc = xmlFree;
    if (mallocFunc != NULL) *mallocFunc = xmlMalloc;
    if (mallocAtomicFunc != NULL) *mallocAtomicFunc = xmlMallocAtomic;
    if (reallocFunc != NULL) *reallocFunc = xmlRealloc;
    if (strdupFunc != NULL) *strdupFunc = xmlMemStrdup;
    return(0);
}

