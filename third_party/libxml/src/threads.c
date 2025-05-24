/**
 * threads.c: set of generic threading related routines
 *
 * See Copyright for the status of this software.
 *
 * Gary Pennington <Gary.Pennington@uk.sun.com>
 * daniel@veillard.com
 */

#define IN_LIBXML
#include "libxml.h"

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <libxml/threads.h>
#include <libxml/parser.h>
#ifdef LIBXML_CATALOG_ENABLED
#include <libxml/catalog.h>
#endif
#ifdef LIBXML_RELAXNG_ENABLED
#include <libxml/relaxng.h>
#endif
#ifdef LIBXML_SCHEMAS_ENABLED
#include <libxml/xmlschemastypes.h>
#endif

#if defined(SOLARIS)
#include <note.h>
#endif

#include "private/cata.h"
#include "private/dict.h"
#include "private/enc.h"
#include "private/error.h"
#include "private/globals.h"
#include "private/io.h"
#include "private/memory.h"
#include "private/threads.h"
#include "private/xpath.h"

/*
 * TODO: this module still uses malloc/free and not xmlMalloc/xmlFree
 *       to avoid some craziness since xmlMalloc/xmlFree may actually
 *       be hosted on allocated blocks needing them for the allocation ...
 */

static xmlRMutex xmlLibraryLock;

/**
 * xmlInitMutex:
 * @mutex:  the mutex
 *
 * Initialize a mutex.
 */
void
xmlInitMutex(xmlMutexPtr mutex)
{
#ifdef HAVE_POSIX_THREADS
    pthread_mutex_init(&mutex->lock, NULL);
#elif defined HAVE_WIN32_THREADS
    InitializeCriticalSection(&mutex->cs);
#else
    (void) mutex;
#endif
}

/**
 * xmlNewMutex:
 *
 * xmlNewMutex() is used to allocate a libxml2 token struct for use in
 * synchronizing access to data.
 *
 * Returns a new simple mutex pointer or NULL in case of error
 */
xmlMutexPtr
xmlNewMutex(void)
{
    xmlMutexPtr tok;

    tok = malloc(sizeof(xmlMutex));
    if (tok == NULL)
        return (NULL);
    xmlInitMutex(tok);
    return (tok);
}

/**
 * xmlCleanupMutex:
 * @mutex:  the simple mutex
 *
 * Reclaim resources associated with a mutex.
 */
void
xmlCleanupMutex(xmlMutexPtr mutex)
{
#ifdef HAVE_POSIX_THREADS
    pthread_mutex_destroy(&mutex->lock);
#elif defined HAVE_WIN32_THREADS
    DeleteCriticalSection(&mutex->cs);
#else
    (void) mutex;
#endif
}

/**
 * xmlFreeMutex:
 * @tok:  the simple mutex
 *
 * Free a mutex.
 */
void
xmlFreeMutex(xmlMutexPtr tok)
{
    if (tok == NULL)
        return;

    xmlCleanupMutex(tok);
    free(tok);
}

/**
 * xmlMutexLock:
 * @tok:  the simple mutex
 *
 * xmlMutexLock() is used to lock a libxml2 token.
 */
void
xmlMutexLock(xmlMutexPtr tok)
{
    if (tok == NULL)
        return;
#ifdef HAVE_POSIX_THREADS
    /*
     * This assumes that __libc_single_threaded won't change while the
     * lock is held.
     */
    pthread_mutex_lock(&tok->lock);
#elif defined HAVE_WIN32_THREADS
    EnterCriticalSection(&tok->cs);
#endif

}

/**
 * xmlMutexUnlock:
 * @tok:  the simple mutex
 *
 * xmlMutexUnlock() is used to unlock a libxml2 token.
 */
void
xmlMutexUnlock(xmlMutexPtr tok)
{
    if (tok == NULL)
        return;
#ifdef HAVE_POSIX_THREADS
    pthread_mutex_unlock(&tok->lock);
#elif defined HAVE_WIN32_THREADS
    LeaveCriticalSection(&tok->cs);
#endif
}

/**
 * xmlInitRMutex:
 * @tok:  mutex
 *
 * Initialize the mutex.
 */
void
xmlInitRMutex(xmlRMutexPtr tok) {
    (void) tok;

#ifdef HAVE_POSIX_THREADS
    pthread_mutex_init(&tok->lock, NULL);
    tok->held = 0;
    tok->waiters = 0;
    pthread_cond_init(&tok->cv, NULL);
#elif defined HAVE_WIN32_THREADS
    InitializeCriticalSection(&tok->cs);
#endif
}

/**
 * xmlNewRMutex:
 *
 * xmlRNewMutex() is used to allocate a reentrant mutex for use in
 * synchronizing access to data. token_r is a re-entrant lock and thus useful
 * for synchronizing access to data structures that may be manipulated in a
 * recursive fashion.
 *
 * Returns the new reentrant mutex pointer or NULL in case of error
 */
xmlRMutexPtr
xmlNewRMutex(void)
{
    xmlRMutexPtr tok;

    tok = malloc(sizeof(xmlRMutex));
    if (tok == NULL)
        return (NULL);
    xmlInitRMutex(tok);
    return (tok);
}

/**
 * xmlCleanupRMutex:
 * @tok:  mutex
 *
 * Cleanup the mutex.
 */
void
xmlCleanupRMutex(xmlRMutexPtr tok) {
    (void) tok;

#ifdef HAVE_POSIX_THREADS
    pthread_mutex_destroy(&tok->lock);
    pthread_cond_destroy(&tok->cv);
#elif defined HAVE_WIN32_THREADS
    DeleteCriticalSection(&tok->cs);
#endif
}

/**
 * xmlFreeRMutex:
 * @tok:  the reentrant mutex
 *
 * xmlRFreeMutex() is used to reclaim resources associated with a
 * reentrant mutex.
 */
void
xmlFreeRMutex(xmlRMutexPtr tok)
{
    if (tok == NULL)
        return;
    xmlCleanupRMutex(tok);
    free(tok);
}

/**
 * xmlRMutexLock:
 * @tok:  the reentrant mutex
 *
 * xmlRMutexLock() is used to lock a libxml2 token_r.
 */
void
xmlRMutexLock(xmlRMutexPtr tok)
{
    if (tok == NULL)
        return;
#ifdef HAVE_POSIX_THREADS
    pthread_mutex_lock(&tok->lock);
    if (tok->held) {
        if (pthread_equal(tok->tid, pthread_self())) {
            tok->held++;
            pthread_mutex_unlock(&tok->lock);
            return;
        } else {
            tok->waiters++;
            while (tok->held)
                pthread_cond_wait(&tok->cv, &tok->lock);
            tok->waiters--;
        }
    }
    tok->tid = pthread_self();
    tok->held = 1;
    pthread_mutex_unlock(&tok->lock);
#elif defined HAVE_WIN32_THREADS
    EnterCriticalSection(&tok->cs);
#endif
}

/**
 * xmlRMutexUnlock:
 * @tok:  the reentrant mutex
 *
 * xmlRMutexUnlock() is used to unlock a libxml2 token_r.
 */
void
xmlRMutexUnlock(xmlRMutexPtr tok ATTRIBUTE_UNUSED)
{
    if (tok == NULL)
        return;
#ifdef HAVE_POSIX_THREADS
    pthread_mutex_lock(&tok->lock);
    tok->held--;
    if (tok->held == 0) {
        if (tok->waiters)
            pthread_cond_signal(&tok->cv);
        memset(&tok->tid, 0, sizeof(tok->tid));
    }
    pthread_mutex_unlock(&tok->lock);
#elif defined HAVE_WIN32_THREADS
    LeaveCriticalSection(&tok->cs);
#endif
}

/************************************************************************
 *									*
 *			Library wide thread interfaces			*
 *									*
 ************************************************************************/

/**
 * xmlLockLibrary:
 *
 * xmlLockLibrary() is used to take out a re-entrant lock on the libxml2
 * library.
 */
void
xmlLockLibrary(void)
{
    xmlRMutexLock(&xmlLibraryLock);
}

/**
 * xmlUnlockLibrary:
 *
 * xmlUnlockLibrary() is used to release a re-entrant lock on the libxml2
 * library.
 */
void
xmlUnlockLibrary(void)
{
    xmlRMutexUnlock(&xmlLibraryLock);
}

/**
 * xmlInitThreads:
 *
 * DEPRECATED: Alias for xmlInitParser.
 */
void
xmlInitThreads(void)
{
    xmlInitParser();
}

/**
 * xmlCleanupThreads:
 *
 * DEPRECATED: This function is a no-op. Call xmlCleanupParser
 * to free global state but see the warnings there. xmlCleanupParser
 * should be only called once at program exit. In most cases, you don't
 * have call cleanup functions at all.
 */
void
xmlCleanupThreads(void)
{
}

static void
xmlInitThreadsInternal(void) {
    xmlInitRMutex(&xmlLibraryLock);
}

static void
xmlCleanupThreadsInternal(void) {
    xmlCleanupRMutex(&xmlLibraryLock);
}

/************************************************************************
 *									*
 *			Library wide initialization			*
 *									*
 ************************************************************************/

static int xmlParserInitialized = 0;

#ifdef HAVE_POSIX_THREADS
static pthread_once_t onceControl = PTHREAD_ONCE_INIT;
#elif defined HAVE_WIN32_THREADS
static INIT_ONCE onceControl = INIT_ONCE_STATIC_INIT;
#else
static int onceControl = 0;
#endif

static void
xmlInitParserInternal(void) {
    /*
     * Note that the initialization code must not make memory allocations.
     */
    xmlInitRandom(); /* Required by xmlInitGlobalsInternal */
    xmlInitMemoryInternal();
    xmlInitThreadsInternal();
    xmlInitGlobalsInternal();
    xmlInitDictInternal();
    xmlInitEncodingInternal();
#if defined(LIBXML_XPATH_ENABLED)
    xmlInitXPathInternal();
#endif
    xmlInitIOCallbacks();
#ifdef LIBXML_CATALOG_ENABLED
    xmlInitCatalogInternal();
#endif

    xmlParserInitialized = 1;
}

#if defined(HAVE_WIN32_THREADS)
static BOOL WINAPI
xmlInitParserWinWrapper(INIT_ONCE *initOnce ATTRIBUTE_UNUSED,
                        void *parameter ATTRIBUTE_UNUSED,
                        void **context ATTRIBUTE_UNUSED) {
    xmlInitParserInternal();
    return(TRUE);
}
#endif

/**
 * xmlInitParser:
 *
 * Initialization function for the XML parser.
 *
 * For older versions, it's recommended to call this function once
 * from the main thread before using the library in multithreaded
 * programs.
 *
 * Since 2.14.0, there's no distinction between threads. It should
 * be unnecessary to call this function.
 */
void
xmlInitParser(void) {
#ifdef HAVE_POSIX_THREADS
    pthread_once(&onceControl, xmlInitParserInternal);
#elif defined(HAVE_WIN32_THREADS)
    InitOnceExecuteOnce(&onceControl, xmlInitParserWinWrapper, NULL, NULL);
#else
    if (onceControl == 0) {
        xmlInitParserInternal();
        onceControl = 1;
    }
#endif
}

/**
 * xmlCleanupParser:
 *
 * This function is named somewhat misleadingly. It does not clean up
 * parser state but global memory allocated by the library itself.
 *
 * Since 2.9.11, cleanup is performed automatically if a shared or
 * dynamic libxml2 library is unloaded. This function should only
 * be used to avoid false positives from memory leak checkers in
 * static builds.
 *
 * WARNING: xmlCleanupParser assumes that all other threads that called
 * libxml2 functions have terminated. No library calls must be made
 * after calling this function. In general, THIS FUNCTION SHOULD ONLY
 * BE CALLED RIGHT BEFORE THE WHOLE PROCESS EXITS.
 */
void
xmlCleanupParser(void) {
    /*
     * Unfortunately, some users call this function to fix memory
     * leaks on unload with versions before 2.9.11. This can result
     * in the library being reinitialized, so this use case must
     * be supported.
     */
    if (!xmlParserInitialized)
        return;

    xmlCleanupCharEncodingHandlers();
#ifdef LIBXML_CATALOG_ENABLED
    xmlCatalogCleanup();
    xmlCleanupCatalogInternal();
#endif
#ifdef LIBXML_SCHEMAS_ENABLED
    xmlSchemaCleanupTypes();
#endif
#ifdef LIBXML_RELAXNG_ENABLED
    xmlRelaxNGCleanupTypes();
#endif

    xmlCleanupDictInternal();
    xmlCleanupRandom();
    xmlCleanupGlobalsInternal();
    xmlCleanupThreadsInternal();

    /*
     * Must come after all cleanup functions that call xmlFree which
     * uses xmlMemMutex in debug mode.
     */
    xmlCleanupMemoryInternal();

    xmlParserInitialized = 0;

    /*
     * This is a bit sketchy but should make reinitialization work.
     */
#ifdef HAVE_POSIX_THREADS
    {
        pthread_once_t tmp = PTHREAD_ONCE_INIT;
        memcpy(&onceControl, &tmp, sizeof(tmp));
    }
#elif defined(HAVE_WIN32_THREADS)
    {
        INIT_ONCE tmp = INIT_ONCE_STATIC_INIT;
        memcpy(&onceControl, &tmp, sizeof(tmp));
    }
#else
    onceControl = 0;
#endif
}

#if defined(HAVE_FUNC_ATTRIBUTE_DESTRUCTOR) && \
    !defined(LIBXML_THREAD_ALLOC_ENABLED) && \
    !defined(LIBXML_STATIC) && \
    !defined(_WIN32)
static void
ATTRIBUTE_DESTRUCTOR
xmlDestructor(void) {
    /*
     * Calling custom deallocation functions in a destructor can cause
     * problems, for example with Nokogiri.
     */
    if (xmlFree == free)
        xmlCleanupParser();
}
#endif
