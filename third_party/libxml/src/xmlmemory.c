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

/**
 * MEM_LIST:
 *
 * keep track of all allocated blocks for error reporting
 * Always build the memory list !
 */
#ifdef DEBUG_MEMORY_LOCATION
#ifndef MEM_LIST
#define MEM_LIST /* keep a list of all the allocated memory blocks */
#endif
#endif

#include <libxml/xmlmemory.h>
#include <libxml/xmlerror.h>
#include <libxml/parser.h>
#include <libxml/threads.h>

#include "private/memory.h"
#include "private/threads.h"

static unsigned long  debugMemSize = 0;
static unsigned long  debugMemBlocks = 0;
static unsigned long  debugMaxMemSize = 0;
static xmlMutex xmlMemMutex;

void xmlMallocBreakpoint(void);

/************************************************************************
 *									*
 *		Macros, variables and associated types			*
 *									*
 ************************************************************************/

#if !defined(LIBXML_THREAD_ENABLED) && !defined(LIBXML_THREAD_ALLOC_ENABLED)
#ifdef xmlMalloc
#undef xmlMalloc
#endif
#ifdef xmlRealloc
#undef xmlRealloc
#endif
#ifdef xmlMemStrdup
#undef xmlMemStrdup
#endif
#endif

/*
 * Each of the blocks allocated begin with a header containing information
 */

#define MEMTAG 0x5aa5U

#define MALLOC_TYPE 1
#define REALLOC_TYPE 2
#define STRDUP_TYPE 3
#define MALLOC_ATOMIC_TYPE 4
#define REALLOC_ATOMIC_TYPE 5

typedef struct memnod {
    unsigned int   mh_tag;
    unsigned int   mh_type;
    unsigned long  mh_number;
    size_t         mh_size;
#ifdef MEM_LIST
   struct memnod *mh_next;
   struct memnod *mh_prev;
#endif
   const char    *mh_file;
   unsigned int   mh_line;
}  MEMHDR;


#ifdef SUN4
#define ALIGN_SIZE  16
#else
#define ALIGN_SIZE  sizeof(double)
#endif
#define HDR_SIZE    sizeof(MEMHDR)
#define RESERVE_SIZE (((HDR_SIZE + (ALIGN_SIZE-1)) \
		      / ALIGN_SIZE ) * ALIGN_SIZE)

#define MAX_SIZE_T ((size_t)-1)

#define CLIENT_2_HDR(a) ((void *) (((char *) (a)) - RESERVE_SIZE))
#define HDR_2_CLIENT(a)    ((void *) (((char *) (a)) + RESERVE_SIZE))


static unsigned int block=0;
static unsigned int xmlMemStopAtBlock = 0;
static void *xmlMemTraceBlockAt = NULL;
#ifdef MEM_LIST
static MEMHDR *memlist = NULL;
#endif

static void debugmem_tag_error(void *addr);
#ifdef MEM_LIST
static void  debugmem_list_add(MEMHDR *);
static void debugmem_list_delete(MEMHDR *);
#endif
#define Mem_Tag_Err(a) debugmem_tag_error(a);

#ifndef TEST_POINT
#define TEST_POINT
#endif

/**
 * xmlMallocBreakpoint:
 *
 * Breakpoint to use in conjunction with xmlMemStopAtBlock. When the block
 * number reaches the specified value this function is called. One need to add a breakpoint
 * to it to get the context in which the given block is allocated.
 */

void
xmlMallocBreakpoint(void) {
    xmlGenericError(xmlGenericErrorContext,
	    "xmlMallocBreakpoint reached on block %d\n", xmlMemStopAtBlock);
}

/**
 * xmlMallocLoc:
 * @size:  an int specifying the size in byte to allocate.
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * a malloc() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */

void *
xmlMallocLoc(size_t size, const char * file, int line)
{
    MEMHDR *p;
    void *ret;

    xmlInitParser();

    TEST_POINT

    if (size > (MAX_SIZE_T - RESERVE_SIZE)) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlMallocLoc : Unsigned overflow\n");
	return(NULL);
    }

    p = (MEMHDR *) malloc(RESERVE_SIZE+size);

    if (!p) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlMallocLoc : Out of free space\n");
	return(NULL);
    }
    p->mh_tag = MEMTAG;
    p->mh_size = size;
    p->mh_type = MALLOC_TYPE;
    p->mh_file = file;
    p->mh_line = line;
    xmlMutexLock(&xmlMemMutex);
    p->mh_number = ++block;
    debugMemSize += size;
    debugMemBlocks++;
    if (debugMemSize > debugMaxMemSize) debugMaxMemSize = debugMemSize;
#ifdef MEM_LIST
    debugmem_list_add(p);
#endif
    xmlMutexUnlock(&xmlMemMutex);

    if (xmlMemStopAtBlock == p->mh_number) xmlMallocBreakpoint();

    ret = HDR_2_CLIENT(p);

    if (xmlMemTraceBlockAt == ret) {
	xmlGenericError(xmlGenericErrorContext,
			"%p : Malloc(%lu) Ok\n", xmlMemTraceBlockAt,
			(long unsigned)size);
	xmlMallocBreakpoint();
    }

    TEST_POINT

    return(ret);
}

/**
 * xmlMallocAtomicLoc:
 * @size:  an unsigned int specifying the size in byte to allocate.
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * a malloc() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */

void *
xmlMallocAtomicLoc(size_t size, const char * file, int line)
{
    MEMHDR *p;
    void *ret;

    xmlInitParser();

    TEST_POINT

    if (size > (MAX_SIZE_T - RESERVE_SIZE)) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlMallocAtomicLoc : Unsigned overflow\n");
	return(NULL);
    }

    p = (MEMHDR *) malloc(RESERVE_SIZE+size);

    if (!p) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlMallocAtomicLoc : Out of free space\n");
	return(NULL);
    }
    p->mh_tag = MEMTAG;
    p->mh_size = size;
    p->mh_type = MALLOC_ATOMIC_TYPE;
    p->mh_file = file;
    p->mh_line = line;
    xmlMutexLock(&xmlMemMutex);
    p->mh_number = ++block;
    debugMemSize += size;
    debugMemBlocks++;
    if (debugMemSize > debugMaxMemSize) debugMaxMemSize = debugMemSize;
#ifdef MEM_LIST
    debugmem_list_add(p);
#endif
    xmlMutexUnlock(&xmlMemMutex);

    if (xmlMemStopAtBlock == p->mh_number) xmlMallocBreakpoint();

    ret = HDR_2_CLIENT(p);

    if (xmlMemTraceBlockAt == ret) {
	xmlGenericError(xmlGenericErrorContext,
			"%p : Malloc(%lu) Ok\n", xmlMemTraceBlockAt,
			(long unsigned)size);
	xmlMallocBreakpoint();
    }

    TEST_POINT

    return(ret);
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
    return(xmlMallocLoc(size, "none", 0));
}

/**
 * xmlReallocLoc:
 * @ptr:  the initial memory block pointer
 * @size:  an int specifying the size in byte to allocate.
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * a realloc() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */

void *
xmlReallocLoc(void *ptr,size_t size, const char * file, int line)
{
    MEMHDR *p, *tmp;
    unsigned long number;

    if (ptr == NULL)
        return(xmlMallocLoc(size, file, line));

    xmlInitParser();
    TEST_POINT

    p = CLIENT_2_HDR(ptr);
    number = p->mh_number;
    if (xmlMemStopAtBlock == number) xmlMallocBreakpoint();
    if (p->mh_tag != MEMTAG) {
       Mem_Tag_Err(p);
	 goto error;
    }
    p->mh_tag = ~MEMTAG;
    xmlMutexLock(&xmlMemMutex);
    debugMemSize -= p->mh_size;
    debugMemBlocks--;
#ifdef MEM_LIST
    debugmem_list_delete(p);
#endif
    xmlMutexUnlock(&xmlMemMutex);

    if (size > (MAX_SIZE_T - RESERVE_SIZE)) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlReallocLoc : Unsigned overflow\n");
	return(NULL);
    }

    tmp = (MEMHDR *) realloc(p,RESERVE_SIZE+size);
    if (!tmp) {
	 free(p);
	 goto error;
    }
    p = tmp;
    if (xmlMemTraceBlockAt == ptr) {
	xmlGenericError(xmlGenericErrorContext,
			"%p : Realloced(%lu -> %lu) Ok\n",
			xmlMemTraceBlockAt, (long unsigned)p->mh_size,
			(long unsigned)size);
	xmlMallocBreakpoint();
    }
    p->mh_tag = MEMTAG;
    p->mh_number = number;
    p->mh_type = REALLOC_TYPE;
    p->mh_size = size;
    p->mh_file = file;
    p->mh_line = line;
    xmlMutexLock(&xmlMemMutex);
    debugMemSize += size;
    debugMemBlocks++;
    if (debugMemSize > debugMaxMemSize) debugMaxMemSize = debugMemSize;
#ifdef MEM_LIST
    debugmem_list_add(p);
#endif
    xmlMutexUnlock(&xmlMemMutex);

    TEST_POINT

    return(HDR_2_CLIENT(p));

error:
    return(NULL);
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
xmlMemRealloc(void *ptr,size_t size) {
    return(xmlReallocLoc(ptr, size, "none", 0));
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
    char *target;

    if (ptr == NULL)
	return;

    if (ptr == (void *) -1) {
	xmlGenericError(xmlGenericErrorContext,
	    "trying to free pointer from freed area\n");
        goto error;
    }

    if (xmlMemTraceBlockAt == ptr) {
	xmlGenericError(xmlGenericErrorContext,
			"%p : Freed()\n", xmlMemTraceBlockAt);
	xmlMallocBreakpoint();
    }

    TEST_POINT

    target = (char *) ptr;

    p = CLIENT_2_HDR(ptr);
    if (p->mh_tag != MEMTAG) {
        Mem_Tag_Err(p);
        goto error;
    }
    if (xmlMemStopAtBlock == p->mh_number) xmlMallocBreakpoint();
    p->mh_tag = ~MEMTAG;
    memset(target, -1, p->mh_size);
    xmlMutexLock(&xmlMemMutex);
    debugMemSize -= p->mh_size;
    debugMemBlocks--;
#ifdef MEM_LIST
    debugmem_list_delete(p);
#endif
    xmlMutexUnlock(&xmlMemMutex);

    free(p);

    TEST_POINT

    return;

error:
    xmlGenericError(xmlGenericErrorContext,
	    "xmlMemFree(%p) error\n", ptr);
    xmlMallocBreakpoint();
    return;
}

/**
 * xmlMemStrdupLoc:
 * @str:  the initial string pointer
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * a strdup() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the new string or NULL if allocation error occurred.
 */

char *
xmlMemStrdupLoc(const char *str, const char *file, int line)
{
    char *s;
    size_t size = strlen(str) + 1;
    MEMHDR *p;

    xmlInitParser();
    TEST_POINT

    if (size > (MAX_SIZE_T - RESERVE_SIZE)) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlMemStrdupLoc : Unsigned overflow\n");
	return(NULL);
    }

    p = (MEMHDR *) malloc(RESERVE_SIZE+size);
    if (!p) {
      goto error;
    }
    p->mh_tag = MEMTAG;
    p->mh_size = size;
    p->mh_type = STRDUP_TYPE;
    p->mh_file = file;
    p->mh_line = line;
    xmlMutexLock(&xmlMemMutex);
    p->mh_number = ++block;
    debugMemSize += size;
    debugMemBlocks++;
    if (debugMemSize > debugMaxMemSize) debugMaxMemSize = debugMemSize;
#ifdef MEM_LIST
    debugmem_list_add(p);
#endif
    xmlMutexUnlock(&xmlMemMutex);

    s = (char *) HDR_2_CLIENT(p);

    if (xmlMemStopAtBlock == p->mh_number) xmlMallocBreakpoint();

    strcpy(s,str);

    TEST_POINT

    if (xmlMemTraceBlockAt == s) {
	xmlGenericError(xmlGenericErrorContext,
			"%p : Strdup() Ok\n", xmlMemTraceBlockAt);
	xmlMallocBreakpoint();
    }

    return(s);

error:
    return(NULL);
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
    return(xmlMemStrdupLoc(str, "none", 0));
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
 * @fp:  a FILE descriptor used as the output file, if NULL, the result is
 *       written to the file .memorylist
 * @nbBytes: the amount of memory to dump
 *
 * the last nbBytes of memory allocated and not freed, useful for dumping
 * the memory left allocated between two places at runtime.
 */

void
xmlMemDisplayLast(FILE *fp, long nbBytes)
{
#ifdef MEM_LIST
    MEMHDR *p;
    unsigned idx;
    int     nb = 0;
#endif
    FILE *old_fp = fp;

    if (nbBytes <= 0)
        return;

    if (fp == NULL) {
	fp = fopen(".memorylist", "w");
	if (fp == NULL)
	    return;
    }

#ifdef MEM_LIST
    fprintf(fp,"   Last %li MEMORY ALLOCATED : %lu, MAX was %lu\n",
            nbBytes, debugMemSize, debugMaxMemSize);
    fprintf(fp,"BLOCK  NUMBER   SIZE  TYPE\n");
    idx = 0;
    xmlMutexLock(&xmlMemMutex);
    p = memlist;
    while ((p) && (nbBytes > 0)) {
	  fprintf(fp,"%-5u  %6lu %6lu ",idx++,p->mh_number,
		  (unsigned long)p->mh_size);
        switch (p->mh_type) {
           case STRDUP_TYPE:fprintf(fp,"strdup()  in ");break;
           case MALLOC_TYPE:fprintf(fp,"malloc()  in ");break;
           case REALLOC_TYPE:fprintf(fp,"realloc() in ");break;
           case MALLOC_ATOMIC_TYPE:fprintf(fp,"atomicmalloc()  in ");break;
           case REALLOC_ATOMIC_TYPE:fprintf(fp,"atomicrealloc() in ");break;
           default:
	        fprintf(fp,"Unknown memory block, may be corrupted");
		xmlMutexUnlock(&xmlMemMutex);
		if (old_fp == NULL)
		    fclose(fp);
		return;
        }
	if (p->mh_file != NULL) fprintf(fp,"%s(%u)", p->mh_file, p->mh_line);
        if (p->mh_tag != MEMTAG)
	      fprintf(fp,"  INVALID");
        nb++;

        fprintf(fp,"\n");
	nbBytes -= (unsigned long)p->mh_size;
        p = p->mh_next;
    }
    xmlMutexUnlock(&xmlMemMutex);
#else
    fprintf(fp,"Memory list not compiled (MEM_LIST not defined !)\n");
#endif
    if (old_fp == NULL)
	fclose(fp);
}

/**
 * xmlMemDisplay:
 * @fp:  a FILE descriptor used as the output file, if NULL, the result is
 *       written to the file .memorylist
 *
 * show in-extenso the memory blocks allocated
 */

void
xmlMemDisplay(FILE *fp)
{
#ifdef MEM_LIST
    MEMHDR *p;
    unsigned idx;
    int     nb = 0;
    time_t currentTime;
    char buf[500];
    struct tm * tstruct;
#endif
    FILE *old_fp = fp;

    if (fp == NULL) {
	fp = fopen(".memorylist", "w");
	if (fp == NULL)
	    return;
    }

#ifdef MEM_LIST
    currentTime = time(NULL);
    tstruct = localtime(&currentTime);
    strftime(buf, sizeof(buf) - 1, "%I:%M:%S %p", tstruct);
    fprintf(fp,"      %s\n\n", buf);


    fprintf(fp,"      MEMORY ALLOCATED : %lu, MAX was %lu\n",
            debugMemSize, debugMaxMemSize);
    fprintf(fp,"BLOCK  NUMBER   SIZE  TYPE\n");
    idx = 0;
    xmlMutexLock(&xmlMemMutex);
    p = memlist;
    while (p) {
	  fprintf(fp,"%-5u  %6lu %6lu ",idx++,p->mh_number,
		  (unsigned long)p->mh_size);
        switch (p->mh_type) {
           case STRDUP_TYPE:fprintf(fp,"strdup()  in ");break;
           case MALLOC_TYPE:fprintf(fp,"malloc()  in ");break;
           case REALLOC_TYPE:fprintf(fp,"realloc() in ");break;
           case MALLOC_ATOMIC_TYPE:fprintf(fp,"atomicmalloc()  in ");break;
           case REALLOC_ATOMIC_TYPE:fprintf(fp,"atomicrealloc() in ");break;
           default:
	        fprintf(fp,"Unknown memory block, may be corrupted");
		xmlMutexUnlock(&xmlMemMutex);
		if (old_fp == NULL)
		    fclose(fp);
		return;
        }
	if (p->mh_file != NULL) fprintf(fp,"%s(%u)", p->mh_file, p->mh_line);
        if (p->mh_tag != MEMTAG)
	      fprintf(fp,"  INVALID");
        nb++;

        fprintf(fp,"\n");
        p = p->mh_next;
    }
    xmlMutexUnlock(&xmlMemMutex);
#else
    fprintf(fp,"Memory list not compiled (MEM_LIST not defined !)\n");
#endif
    if (old_fp == NULL)
	fclose(fp);
}

#ifdef MEM_LIST

static void debugmem_list_add(MEMHDR *p)
{
     p->mh_next = memlist;
     p->mh_prev = NULL;
     if (memlist) memlist->mh_prev = p;
     memlist = p;
}

static void debugmem_list_delete(MEMHDR *p)
{
     if (p->mh_next)
     p->mh_next->mh_prev = p->mh_prev;
     if (p->mh_prev)
     p->mh_prev->mh_next = p->mh_next;
     else memlist = p->mh_next;
}

#endif

/*
 * debugmem_tag_error:
 *
 * internal error function.
 */

static void debugmem_tag_error(void *p)
{
     xmlGenericError(xmlGenericErrorContext,
	     "Memory tag error occurs :%p \n\t bye\n", p);
#ifdef MEM_LIST
     if (stderr)
     xmlMemDisplay(stderr);
#endif
}

#ifdef MEM_LIST
static FILE *xmlMemoryDumpFile = NULL;
#endif

/**
 * xmlMemShow:
 * @fp:  a FILE descriptor used as the output file
 * @nr:  number of entries to dump
 *
 * show a show display of the memory allocated, and dump
 * the @nr last allocated areas which were not freed
 */

void
xmlMemShow(FILE *fp, int nr ATTRIBUTE_UNUSED)
{
#ifdef MEM_LIST
    MEMHDR *p;
#endif

    if (fp != NULL)
	fprintf(fp,"      MEMORY ALLOCATED : %lu, MAX was %lu\n",
		debugMemSize, debugMaxMemSize);
#ifdef MEM_LIST
    xmlMutexLock(&xmlMemMutex);
    if (nr > 0) {
	fprintf(fp,"NUMBER   SIZE  TYPE   WHERE\n");
	p = memlist;
	while ((p) && nr > 0) {
	      fprintf(fp,"%6lu %6lu ",p->mh_number,(unsigned long)p->mh_size);
	    switch (p->mh_type) {
	       case STRDUP_TYPE:fprintf(fp,"strdup()  in ");break;
	       case MALLOC_TYPE:fprintf(fp,"malloc()  in ");break;
	       case MALLOC_ATOMIC_TYPE:fprintf(fp,"atomicmalloc()  in ");break;
	      case REALLOC_TYPE:fprintf(fp,"realloc() in ");break;
	      case REALLOC_ATOMIC_TYPE:fprintf(fp,"atomicrealloc() in ");break;
		default:fprintf(fp,"   ???    in ");break;
	    }
	    if (p->mh_file != NULL)
	        fprintf(fp,"%s(%u)", p->mh_file, p->mh_line);
	    if (p->mh_tag != MEMTAG)
		fprintf(fp,"  INVALID");
	    fprintf(fp,"\n");
	    nr--;
	    p = p->mh_next;
	}
    }
    xmlMutexUnlock(&xmlMemMutex);
#endif /* MEM_LIST */
}

/**
 * xmlMemoryDump:
 *
 * Dump in-extenso the memory blocks allocated to the file .memorylist
 */

void
xmlMemoryDump(void)
{
#ifdef MEM_LIST
    FILE *dump;

    if (debugMaxMemSize == 0)
	return;
    dump = fopen(".memdump", "w");
    if (dump == NULL)
	xmlMemoryDumpFile = stderr;
    else xmlMemoryDumpFile = dump;

    xmlMemDisplay(xmlMemoryDumpFile);

    if (dump != NULL) fclose(dump);
#endif /* MEM_LIST */
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
 *
 * Returns 0 on success
 */
void
xmlInitMemoryInternal(void) {
     char *breakpoint;
     xmlInitMutex(&xmlMemMutex);

     breakpoint = getenv("XML_MEM_BREAKPOINT");
     if (breakpoint != NULL) {
         sscanf(breakpoint, "%ud", &xmlMemStopAtBlock);
     }
     breakpoint = getenv("XML_MEM_TRACE");
     if (breakpoint != NULL) {
         sscanf(breakpoint, "%p", &xmlMemTraceBlockAt);
     }

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

