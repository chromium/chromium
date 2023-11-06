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
#include <stdlib.h>

#include <libxml/threads.h>
#include <libxml/parser.h>
#ifdef LIBXML_CATALOG_ENABLED
#include <libxml/catalog.h>
#endif
#ifdef LIBXML_SCHEMAS_ENABLED
#include <libxml/xmlschemastypes.h>
#include <libxml/relaxng.h>
#endif

#if defined(SOLARIS)
#include <note.h>
#endif

#include "private/dict.h"
#include "private/enc.h"
#include "private/globals.h"
#include "private/memory.h"
#include "private/threads.h"
#include "private/xpath.h"

#if defined(HAVE_POSIX_THREADS) && \
    defined(__GLIBC__) && \
    __GLIBC__ * 100 + __GLIBC_MINOR__ >= 234

/*
 * The modern way available since glibc 2.32.
 *
 * The check above is for glibc 2.34 which merged the pthread symbols into
 * libc. Since we still allow linking without pthread symbols (see below),
 * this only works if pthread symbols are guaranteed to be available.
 */

#include <sys/single_threaded.h>

#define XML_IS_THREADED() (!__libc_single_threaded)
#define XML_IS_NEVER_THREADED() 0

#elif defined(HAVE_POSIX_THREADS) && \
      defined(__GLIBC__) && \
      defined(__GNUC__)

/*
 * The traditional way to check for single-threaded applications with
 * glibc was to check whether the separate libpthread library is
 * linked in. This works by not linking libxml2 with libpthread (see
 * BASE_THREAD_LIBS in configure.ac and Makefile.am) and declaring
 * pthread functions as weak symbols.
 *
 * In glibc 2.34, the pthread symbols were moved from libpthread to libc,
 * so this doesn't work anymore.
 *
 * At some point, this legacy code and the BASE_THREAD_LIBS hack in
 * configure.ac can probably be removed.
 */

#pragma weak pthread_mutex_init
#pragma weak pthread_mutex_destroy
#pragma weak pthread_mutex_lock
#pragma weak pthread_mutex_unlock
#pragma weak pthread_cond_init
#pragma weak pthread_cond_destroy
#pragma weak pthread_cond_wait
#pragma weak pthread_equal
#pragma weak pthread_self
#pragma weak pthread_key_create
#pragma weak pthread_key_delete
#pragma weak pthread_cond_signal

#define XML_PTHREAD_WEAK
#define XML_IS_THREADED() libxml_is_threaded
#define XML_IS_NEVER_THREADED() (!libxml_is_threaded)

static int libxml_is_threaded = -1;

#else /* other POSIX platforms */

#define XML_IS_THREADED() 1
#define XML_IS_NEVER_THREADED() 0

#endif

/*
 * TODO: this module still uses malloc/free and not xmlMalloc/xmlFree
 *       to avoid some craziness since xmlMalloc/xmlFree may actually
 *       be hosted on allocated blocks needing them for the allocation ...
 */

/*
 * xmlRMutex are reentrant mutual exception locks
 */
struct _xmlRMutex {
#ifdef HAVE_POSIX_THREADS
    pthread_mutex_t lock;
    unsigned int held;
    unsigned int waiters;
    pthread_t tid;
    pthread_cond_t cv;
#elif defined HAVE_WIN32_THREADS
    CRITICAL_SECTION cs;
#else
    int empty;
#endif
};

static xmlRMutexPtr xmlLibraryLock = NULL;

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
    if (XML_IS_NEVER_THREADED() == 0)
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

    if ((tok = malloc(sizeof(xmlMutex))) == NULL)
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
    if (XML_IS_NEVER_THREADED() == 0)
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
    if (XML_IS_THREADED() != 0)
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
    if (XML_IS_THREADED() != 0)
        pthread_mutex_unlock(&tok->lock);
#elif defined HAVE_WIN32_THREADS
    LeaveCriticalSection(&tok->cs);
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

    if ((tok = malloc(sizeof(xmlRMutex))) == NULL)
        return (NULL);
#ifdef HAVE_POSIX_THREADS
    if (XML_IS_NEVER_THREADED() == 0) {
        pthread_mutex_init(&tok->lock, NULL);
        tok->held = 0;
        tok->waiters = 0;
        pthread_cond_init(&tok->cv, NULL);
    }
#elif defined HAVE_WIN32_THREADS
    InitializeCriticalSection(&tok->cs);
#endif
    return (tok);
}

/**
 * xmlFreeRMutex:
 * @tok:  the reentrant mutex
 *
 * xmlRFreeMutex() is used to reclaim resources associated with a
 * reentrant mutex.
 */
void
xmlFreeRMutex(xmlRMutexPtr tok ATTRIBUTE_UNUSED)
{
    if (tok == NULL)
        return;
#ifdef HAVE_POSIX_THREADS
    if (XML_IS_NEVER_THREADED() == 0) {
        pthread_mutex_destroy(&tok->lock);
        pthread_cond_destroy(&tok->cv);
    }
#elif defined HAVE_WIN32_THREADS
    DeleteCriticalSection(&tok->cs);
#endif
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
    if (XML_IS_THREADED() == 0)
        return;

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
    if (XML_IS_THREADED() == 0)
        return;

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
 * xmlGetThreadId:
 *
 * DEPRECATED: Internal function, do not use.
 *
 * xmlGetThreadId() find the current thread ID number
 * Note that this is likely to be broken on some platforms using pthreads
 * as the specification doesn't mandate pthread_t to be an integer type
 *
 * Returns the current thread ID number
 */
int
xmlGetThreadId(void)
{
#ifdef HAVE_POSIX_THREADS
    pthread_t id;
    int ret;

    if (XML_IS_THREADED() == 0)
        return (0);
    id = pthread_self();
    /* horrible but preserves compat, see warning above */
    memcpy(&ret, &id, sizeof(ret));
    return (ret);
#elif defined HAVE_WIN32_THREADS
    return GetCurrentThreadId();
#else
    return ((int) 0);
#endif
}

/**
 * xmlLockLibrary:
 *
 * xmlLockLibrary() is used to take out a re-entrant lock on the libxml2
 * library.
 */
void
xmlLockLibrary(void)
{
    xmlRMutexLock(xmlLibraryLock);
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
    xmlRMutexUnlock(xmlLibraryLock);
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

/************************************************************************
 *									*
 *			Library wide initialization			*
 *									*
 ************************************************************************/

static int xmlParserInitialized = 0;
static int xmlParserInnerInitialized = 0;


#ifdef HAVE_POSIX_THREADS
static pthread_mutex_t global_init_lock = PTHREAD_MUTEX_INITIALIZER;
#elif defined HAVE_WIN32_THREADS
static volatile LPCRITICAL_SECTION global_init_lock = NULL;
#endif

/**
 * xmlGlobalInitMutexLock
 *
 * Makes sure that the global initialization mutex is initialized and
 * locks it.
 */
static void
xmlGlobalInitMutexLock(void) {
#ifdef HAVE_POSIX_THREADS

#ifdef XML_PTHREAD_WEAK
    /*
     * This is somewhat unreliable since libpthread could be loaded
     * later with dlopen() and threads could be created. But it's
     * long-standing behavior and hard to work around.
     */
    if (libxml_is_threaded == -1)
        libxml_is_threaded =
            (pthread_mutex_init != NULL) &&
            (pthread_mutex_destroy != NULL) &&
            (pthread_mutex_lock != NULL) &&
            (pthread_mutex_unlock != NULL) &&
            (pthread_cond_init != NULL) &&
            (pthread_cond_destroy != NULL) &&
            (pthread_cond_wait != NULL) &&
            /*
             * pthread_equal can be inline, resuting in -Waddress warnings.
             * Let's assume it's available if all the other functions are.
             */
            /* (pthread_equal != NULL) && */
            (pthread_self != NULL) &&
            (pthread_cond_signal != NULL);
#endif

    /* The mutex is statically initialized, so we just lock it. */
    if (XML_IS_THREADED() != 0)
        pthread_mutex_lock(&global_init_lock);

#elif defined HAVE_WIN32_THREADS

    LPCRITICAL_SECTION cs;

    /* Create a new critical section */
    if (global_init_lock == NULL) {
        cs = malloc(sizeof(CRITICAL_SECTION));
        if (cs == NULL) {
            xmlGenericError(xmlGenericErrorContext,
                            "xmlGlobalInitMutexLock: out of memory\n");
            return;
        }
        InitializeCriticalSection(cs);

        /* Swap it into the global_init_lock */
#ifdef InterlockedCompareExchangePointer
        InterlockedCompareExchangePointer((void **) &global_init_lock,
                                          cs, NULL);
#else /* Use older void* version */
        InterlockedCompareExchange((void **) &global_init_lock,
                                   (void *) cs, NULL);
#endif /* InterlockedCompareExchangePointer */

        /* If another thread successfully recorded its critical
         * section in the global_init_lock then discard the one
         * allocated by this thread. */
        if (global_init_lock != cs) {
            DeleteCriticalSection(cs);
            free(cs);
        }
    }

    /* Lock the chosen critical section */
    EnterCriticalSection(global_init_lock);

#endif
}

static void
xmlGlobalInitMutexUnlock(void) {
#ifdef HAVE_POSIX_THREADS
    if (XML_IS_THREADED() != 0)
        pthread_mutex_unlock(&global_init_lock);
#elif defined HAVE_WIN32_THREADS
    if (global_init_lock != NULL)
	LeaveCriticalSection(global_init_lock);
#endif
}

/**
 * xmlGlobalInitMutexDestroy
 *
 * Makes sure that the global initialization mutex is destroyed before
 * application termination.
 */
static void
xmlGlobalInitMutexDestroy(void) {
#ifdef HAVE_POSIX_THREADS
#elif defined HAVE_WIN32_THREADS
    if (global_init_lock != NULL) {
        DeleteCriticalSection(global_init_lock);
        free(global_init_lock);
        global_init_lock = NULL;
    }
#endif
}

/**
 * xmlInitParser:
 *
 * Initialization function for the XML parser.
 *
 * Call once from the main thread before using the library in
 * multithreaded programs.
 */
void
xmlInitParser(void) {
    /*
     * Note that the initialization code must not make memory allocations.
     */
    if (xmlParserInitialized != 0)
        return;

    xmlGlobalInitMutexLock();

    if (xmlParserInnerInitialized == 0) {
#if defined(_WIN32) && \
    !defined(LIBXML_THREAD_ALLOC_ENABLED) && \
    (!defined(LIBXML_STATIC) || defined(LIBXML_STATIC_FOR_DLL))
        if (xmlFree == free)
            atexit(xmlCleanupParser);
#endif

        xmlInitMemoryInternal(); /* Should come second */
        xmlInitGlobalsInternal();
        xmlInitRandom();
        xmlInitDictInternal();
        xmlInitEncodingInternal();
#if defined(LIBXML_XPATH_ENABLED) || defined(LIBXML_SCHEMAS_ENABLED)
        xmlInitXPathInternal();
#endif

        xmlRegisterDefaultInputCallbacks();
#ifdef LIBXML_OUTPUT_ENABLED
        xmlRegisterDefaultOutputCallbacks();
#endif /* LIBXML_OUTPUT_ENABLED */

        xmlParserInnerInitialized = 1;
    }

    xmlGlobalInitMutexUnlock();

    xmlParserInitialized = 1;
}

/**
 * xmlCleanupParser:
 *
 * This function name is somewhat misleading. It does not clean up
 * parser state, it cleans up memory allocated by the library itself.
 * It is a cleanup function for the XML library. It tries to reclaim all
 * related global memory allocated for the library processing.
 * It doesn't deallocate any document related memory. One should
 * call xmlCleanupParser() only when the process has finished using
 * the library and all XML/HTML documents built with it.
 * See also xmlInitParser() which has the opposite function of preparing
 * the library for operations.
 *
 * WARNING: if your application is multithreaded or has plugin support
 *          calling this may crash the application if another thread or
 *          a plugin is still using libxml2. It's sometimes very hard to
 *          guess if libxml2 is in use in the application, some libraries
 *          or plugins may use it without notice. In case of doubt abstain
 *          from calling this function or do it just before calling exit()
 *          to avoid leak reports from valgrind !
 */
void
xmlCleanupParser(void) {
    if (!xmlParserInitialized)
        return;

    /* These functions can call xmlFree. */

    xmlCleanupCharEncodingHandlers();
#ifdef LIBXML_CATALOG_ENABLED
    xmlCatalogCleanup();
#endif
#ifdef LIBXML_SCHEMAS_ENABLED
    xmlSchemaCleanupTypes();
    xmlRelaxNGCleanupTypes();
#endif

    /* These functions should never call xmlFree. */

    xmlCleanupInputCallbacks();
#ifdef LIBXML_OUTPUT_ENABLED
    xmlCleanupOutputCallbacks();
#endif

    xmlCleanupDictInternal();
    xmlCleanupRandom();
    xmlCleanupGlobalsInternal();
    /*
     * Must come last. On Windows, xmlCleanupGlobalsInternal can call
     * xmlFree which uses xmlMemMutex in debug mode.
     */
    xmlCleanupMemoryInternal();

    xmlGlobalInitMutexDestroy();

    xmlParserInitialized = 0;
    xmlParserInnerInitialized = 0;
}

#if defined(HAVE_ATTRIBUTE_DESTRUCTOR) && \
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

