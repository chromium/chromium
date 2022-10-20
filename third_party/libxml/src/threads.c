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
#include <libxml/globals.h>

#ifdef LIBXML_THREAD_ENABLED
  #ifdef HAVE_PTHREAD_H
    #include <pthread.h>
    #define HAVE_POSIX_THREADS
  #elif defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #ifndef HAVE_COMPILER_TLS
      #include <process.h>
    #endif
    #define HAVE_WIN32_THREADS
  #endif
#endif

#if defined(SOLARIS)
#include <note.h>
#endif

#include "private/dict.h"
#include "private/threads.h"

/* #define DEBUG_THREADS */

#ifdef HAVE_POSIX_THREADS

#if defined(__GNUC__) && defined(__linux__)

static int libxml_is_threaded = -1;

#define XML_PTHREAD_WEAK

#pragma weak pthread_once
#pragma weak pthread_getspecific
#pragma weak pthread_setspecific
#pragma weak pthread_key_create
#pragma weak pthread_key_delete
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

#else /* __GNUC__, __GLIBC__, __linux__ */

static int libxml_is_threaded = 1;

#endif /* __GNUC__, __GLIBC__, __linux__ */

#endif /* HAVE_POSIX_THREADS */

/*
 * TODO: this module still uses malloc/free and not xmlMalloc/xmlFree
 *       to avoid some craziness since xmlMalloc/xmlFree may actually
 *       be hosted on allocated blocks needing them for the allocation ...
 */

/*
 * xmlMutex are a simple mutual exception locks
 */
struct _xmlMutex {
#ifdef HAVE_POSIX_THREADS
    pthread_mutex_t lock;
#elif defined HAVE_WIN32_THREADS
    CRITICAL_SECTION cs;
#else
    int empty;
#endif
};

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

/*
 * This module still has some internal static data.
 *   - xmlLibraryLock a global lock
 *   - globalkey used for per-thread data
 */

#ifdef HAVE_POSIX_THREADS
static pthread_key_t globalkey;
static pthread_t mainthread;
static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static pthread_once_t once_control_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t global_init_lock = PTHREAD_MUTEX_INITIALIZER;
#elif defined HAVE_WIN32_THREADS
#if defined(HAVE_COMPILER_TLS)
static __declspec(thread) xmlGlobalState tlstate;
static __declspec(thread) int tlstate_inited = 0;
#else /* HAVE_COMPILER_TLS */
static DWORD globalkey = TLS_OUT_OF_INDEXES;
#endif /* HAVE_COMPILER_TLS */
static DWORD mainthread;
static struct {
    DWORD done;
    LONG control;
} run_once = { 0, 0};
static volatile LPCRITICAL_SECTION global_init_lock = NULL;
#endif

static xmlRMutexPtr xmlLibraryLock = NULL;

#ifdef LIBXML_THREAD_ENABLED
static void xmlOnceInit(void);
#endif

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
#ifdef HAVE_POSIX_THREADS
    if (libxml_is_threaded != 0)
        pthread_mutex_init(&tok->lock, NULL);
#elif defined HAVE_WIN32_THREADS
    InitializeCriticalSection(&tok->cs);
#endif
    return (tok);
}

/**
 * xmlFreeMutex:
 * @tok:  the simple mutex
 *
 * xmlFreeMutex() is used to reclaim resources associated with a libxml2 token
 * struct.
 */
void
xmlFreeMutex(xmlMutexPtr tok)
{
    if (tok == NULL)
        return;

#ifdef HAVE_POSIX_THREADS
    if (libxml_is_threaded != 0)
        pthread_mutex_destroy(&tok->lock);
#elif defined HAVE_WIN32_THREADS
    DeleteCriticalSection(&tok->cs);
#endif
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
    if (libxml_is_threaded != 0)
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
    if (libxml_is_threaded != 0)
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
    if (libxml_is_threaded != 0) {
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
    if (libxml_is_threaded != 0) {
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
    if (libxml_is_threaded == 0)
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
    if (libxml_is_threaded == 0)
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

/**
 * xmlGlobalInitMutexLock
 *
 * Makes sure that the global initialization mutex is initialized and
 * locks it.
 */
void
__xmlGlobalInitMutexLock(void)
{
    /* Make sure the global init lock is initialized and then lock it. */
#ifdef HAVE_POSIX_THREADS
    /* The mutex is statically initialized, so we just lock it. */
#ifdef XML_PTHREAD_WEAK
    if (pthread_mutex_lock == NULL)
        return;
#endif /* XML_PTHREAD_WEAK */
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

void
__xmlGlobalInitMutexUnlock(void)
{
#ifdef HAVE_POSIX_THREADS
#ifdef XML_PTHREAD_WEAK
    if (pthread_mutex_unlock == NULL)
        return;
#endif /* XML_PTHREAD_WEAK */
    pthread_mutex_unlock(&global_init_lock);
#elif defined HAVE_WIN32_THREADS
    if (global_init_lock != NULL) {
	LeaveCriticalSection(global_init_lock);
    }
#endif
}

/**
 * xmlGlobalInitMutexDestroy
 *
 * Makes sure that the global initialization mutex is destroyed before
 * application termination.
 */
void
__xmlGlobalInitMutexDestroy(void)
{
#ifdef HAVE_POSIX_THREADS
#elif defined HAVE_WIN32_THREADS
    if (global_init_lock != NULL) {
        DeleteCriticalSection(global_init_lock);
        free(global_init_lock);
        global_init_lock = NULL;
    }
#endif
}

/************************************************************************
 *									*
 *			Per thread global state handling		*
 *									*
 ************************************************************************/

#ifdef LIBXML_THREAD_ENABLED
#ifdef xmlLastError
#undef xmlLastError
#endif

/**
 * xmlFreeGlobalState:
 * @state:  a thread global state
 *
 * xmlFreeGlobalState() is called when a thread terminates with a non-NULL
 * global state. It is is used here to reclaim memory resources.
 */
static void
xmlFreeGlobalState(void *state)
{
    xmlGlobalState *gs = (xmlGlobalState *) state;

    /* free any memory allocated in the thread's xmlLastError */
    xmlResetError(&(gs->xmlLastError));
    free(state);
}

/**
 * xmlNewGlobalState:
 *
 * xmlNewGlobalState() allocates a global state. This structure is used to
 * hold all data for use by a thread when supporting backwards compatibility
 * of libxml2 to pre-thread-safe behaviour.
 *
 * Returns the newly allocated xmlGlobalStatePtr or NULL in case of error
 */
static xmlGlobalStatePtr
xmlNewGlobalState(void)
{
    xmlGlobalState *gs;

    gs = malloc(sizeof(xmlGlobalState));
    if (gs == NULL) {
	xmlGenericError(xmlGenericErrorContext,
			"xmlGetGlobalState: out of memory\n");
        return (NULL);
    }

    memset(gs, 0, sizeof(xmlGlobalState));
    xmlInitializeGlobalState(gs);
    return (gs);
}
#endif /* LIBXML_THREAD_ENABLED */

#ifdef HAVE_POSIX_THREADS
#elif defined HAVE_WIN32_THREADS
#if !defined(HAVE_COMPILER_TLS)
#if defined(LIBXML_STATIC) && !defined(LIBXML_STATIC_FOR_DLL)
typedef struct _xmlGlobalStateCleanupHelperParams {
    HANDLE thread;
    void *memory;
} xmlGlobalStateCleanupHelperParams;

static void XMLCDECL
xmlGlobalStateCleanupHelper(void *p)
{
    xmlGlobalStateCleanupHelperParams *params =
        (xmlGlobalStateCleanupHelperParams *) p;
    WaitForSingleObject(params->thread, INFINITE);
    CloseHandle(params->thread);
    xmlFreeGlobalState(params->memory);
    free(params);
    _endthread();
}
#else /* LIBXML_STATIC && !LIBXML_STATIC_FOR_DLL */

typedef struct _xmlGlobalStateCleanupHelperParams {
    void *memory;
    struct _xmlGlobalStateCleanupHelperParams *prev;
    struct _xmlGlobalStateCleanupHelperParams *next;
} xmlGlobalStateCleanupHelperParams;

static xmlGlobalStateCleanupHelperParams *cleanup_helpers_head = NULL;
static CRITICAL_SECTION cleanup_helpers_cs;

#endif /* LIBXMLSTATIC && !LIBXML_STATIC_FOR_DLL */
#endif /* HAVE_COMPILER_TLS */
#endif /* HAVE_WIN32_THREADS */

/**
 * xmlGetGlobalState:
 *
 * xmlGetGlobalState() is called to retrieve the global state for a thread.
 *
 * Returns the thread global state or NULL in case of error
 */
xmlGlobalStatePtr
xmlGetGlobalState(void)
{
#ifdef HAVE_POSIX_THREADS
    xmlGlobalState *globalval;

    if (libxml_is_threaded == 0)
        return (NULL);

    pthread_once(&once_control, xmlOnceInit);

    if ((globalval = (xmlGlobalState *)
         pthread_getspecific(globalkey)) == NULL) {
        xmlGlobalState *tsd = xmlNewGlobalState();
	if (tsd == NULL)
	    return(NULL);

        pthread_setspecific(globalkey, tsd);
        return (tsd);
    }
    return (globalval);
#elif defined HAVE_WIN32_THREADS
#if defined(HAVE_COMPILER_TLS)
    if (!tlstate_inited) {
        tlstate_inited = 1;
        xmlInitializeGlobalState(&tlstate);
    }
    return &tlstate;
#else /* HAVE_COMPILER_TLS */
    xmlGlobalState *globalval;
    xmlGlobalStateCleanupHelperParams *p;

    xmlOnceInit();
#if defined(LIBXML_STATIC) && !defined(LIBXML_STATIC_FOR_DLL)
    globalval = (xmlGlobalState *) TlsGetValue(globalkey);
#else
    p = (xmlGlobalStateCleanupHelperParams *) TlsGetValue(globalkey);
    globalval = (xmlGlobalState *) (p ? p->memory : NULL);
#endif
    if (globalval == NULL) {
        xmlGlobalState *tsd = xmlNewGlobalState();

        if (tsd == NULL)
	    return(NULL);
        p = (xmlGlobalStateCleanupHelperParams *)
            malloc(sizeof(xmlGlobalStateCleanupHelperParams));
	if (p == NULL) {
            xmlGenericError(xmlGenericErrorContext,
                            "xmlGetGlobalState: out of memory\n");
            xmlFreeGlobalState(tsd);
	    return(NULL);
	}
        p->memory = tsd;
#if defined(LIBXML_STATIC) && !defined(LIBXML_STATIC_FOR_DLL)
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                        GetCurrentProcess(), &p->thread, 0, TRUE,
                        DUPLICATE_SAME_ACCESS);
        TlsSetValue(globalkey, tsd);
        _beginthread(xmlGlobalStateCleanupHelper, 0, p);
#else
        EnterCriticalSection(&cleanup_helpers_cs);
        if (cleanup_helpers_head != NULL) {
            cleanup_helpers_head->prev = p;
        }
        p->next = cleanup_helpers_head;
        p->prev = NULL;
        cleanup_helpers_head = p;
        TlsSetValue(globalkey, p);
        LeaveCriticalSection(&cleanup_helpers_cs);
#endif

        return (tsd);
    }
    return (globalval);
#endif /* HAVE_COMPILER_TLS */
#else
    return (NULL);
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

    if (libxml_is_threaded == 0)
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
 * xmlIsMainThread:
 *
 * xmlIsMainThread() check whether the current thread is the main thread.
 *
 * Returns 1 if the current thread is the main thread, 0 otherwise
 */
int
xmlIsMainThread(void)
{
#ifdef HAVE_POSIX_THREADS
    if (libxml_is_threaded == -1)
        xmlInitThreads();
    if (libxml_is_threaded == 0)
        return (1);
    pthread_once(&once_control, xmlOnceInit);
#elif defined HAVE_WIN32_THREADS
    xmlOnceInit();
#endif

#ifdef DEBUG_THREADS
    xmlGenericError(xmlGenericErrorContext, "xmlIsMainThread()\n");
#endif
#ifdef HAVE_POSIX_THREADS
    return (pthread_equal(mainthread,pthread_self()));
#elif defined HAVE_WIN32_THREADS
    return (mainthread == GetCurrentThreadId());
#else
    return (1);
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
#ifdef DEBUG_THREADS
    xmlGenericError(xmlGenericErrorContext, "xmlLockLibrary()\n");
#endif
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
#ifdef DEBUG_THREADS
    xmlGenericError(xmlGenericErrorContext, "xmlUnlockLibrary()\n");
#endif
    xmlRMutexUnlock(xmlLibraryLock);
}

/**
 * xmlInitThreads:
 *
 * DEPRECATED: This function will be made private. Call xmlInitParser to
 * initialize the library.
 *
 * xmlInitThreads() is used to to initialize all the thread related
 * data of the libxml2 library.
 */
void
xmlInitThreads(void)
{
#ifdef HAVE_POSIX_THREADS
#ifdef XML_PTHREAD_WEAK
    if (libxml_is_threaded == -1) {
        if ((pthread_once != NULL) &&
            (pthread_getspecific != NULL) &&
            (pthread_setspecific != NULL) &&
            (pthread_key_create != NULL) &&
            (pthread_key_delete != NULL) &&
            (pthread_mutex_init != NULL) &&
            (pthread_mutex_destroy != NULL) &&
            (pthread_mutex_lock != NULL) &&
            (pthread_mutex_unlock != NULL) &&
            (pthread_cond_init != NULL) &&
            (pthread_cond_destroy != NULL) &&
            (pthread_cond_wait != NULL) &&
            (pthread_equal != NULL) &&
            (pthread_self != NULL) &&
            (pthread_cond_signal != NULL)) {
            libxml_is_threaded = 1;

/* fprintf(stderr, "Running multithreaded\n"); */
        } else {

/* fprintf(stderr, "Running without multithread\n"); */
            libxml_is_threaded = 0;
        }
    }
#endif /* XML_PTHREAD_WEAK */
#endif
}

/**
 * xmlCleanupThreads:
 *
 * DEPRECATED: This function will be made private. Call xmlCleanupParser
 * to free global state but see the warnings there. xmlCleanupParser
 * should be only called once at program exit. In most cases, you don't
 * have call cleanup functions at all.
 *
 * xmlCleanupThreads() is used to to cleanup all the thread related
 * data of the libxml2 library once processing has ended.
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
xmlCleanupThreads(void)
{
#ifdef DEBUG_THREADS
    xmlGenericError(xmlGenericErrorContext, "xmlCleanupThreads()\n");
#endif
#ifdef HAVE_POSIX_THREADS
    if (libxml_is_threaded != 0)
        pthread_key_delete(globalkey);
    once_control = once_control_init;
#elif defined(HAVE_WIN32_THREADS)
#if !defined(HAVE_COMPILER_TLS)
    if (globalkey != TLS_OUT_OF_INDEXES) {
#if !defined(LIBXML_STATIC) || defined(LIBXML_STATIC_FOR_DLL)
        xmlGlobalStateCleanupHelperParams *p;

        EnterCriticalSection(&cleanup_helpers_cs);
        p = cleanup_helpers_head;
        while (p != NULL) {
            xmlGlobalStateCleanupHelperParams *temp = p;

            p = p->next;
            xmlFreeGlobalState(temp->memory);
            free(temp);
        }
        cleanup_helpers_head = 0;
        LeaveCriticalSection(&cleanup_helpers_cs);
#endif
        TlsFree(globalkey);
        globalkey = TLS_OUT_OF_INDEXES;
    }
#if !defined(LIBXML_STATIC) || defined(LIBXML_STATIC_FOR_DLL)
    DeleteCriticalSection(&cleanup_helpers_cs);
#endif
#endif
    run_once.done = 0;
    run_once.control = 0;
#endif
}

#ifdef LIBXML_THREAD_ENABLED

/**
 * xmlOnceInit
 *
 * xmlOnceInit() is used to initialize the value of mainthread for use
 * in other routines. This function should only be called using
 * pthread_once() in association with the once_control variable to ensure
 * that the function is only called once. See man pthread_once for more
 * details.
 */
static void
xmlOnceInit(void)
{
#ifdef HAVE_POSIX_THREADS
    (void) pthread_key_create(&globalkey, xmlFreeGlobalState);
    mainthread = pthread_self();
    __xmlInitializeDict();
#elif defined(HAVE_WIN32_THREADS)
    if (!run_once.done) {
        if (InterlockedIncrement(&run_once.control) == 1) {
#if !defined(HAVE_COMPILER_TLS)
#if !defined(LIBXML_STATIC) || defined(LIBXML_STATIC_FOR_DLL)
            InitializeCriticalSection(&cleanup_helpers_cs);
#endif
            globalkey = TlsAlloc();
#endif
            mainthread = GetCurrentThreadId();
	    __xmlInitializeDict();
            run_once.done = 1;
        } else {
            /* Another thread is working; give up our slice and
             * wait until they're done. */
            while (!run_once.done)
                Sleep(0);
        }
    }
#endif
}
#endif

/**
 * DllMain:
 * @hinstDLL: handle to DLL instance
 * @fdwReason: Reason code for entry
 * @lpvReserved: generic pointer (depends upon reason code)
 *
 * Entry point for Windows library. It is being used to free thread-specific
 * storage.
 *
 * Returns TRUE always
 */
#ifdef HAVE_POSIX_THREADS
#elif defined(HAVE_WIN32_THREADS) && !defined(HAVE_COMPILER_TLS) && (!defined(LIBXML_STATIC) || defined(LIBXML_STATIC_FOR_DLL))
#if defined(LIBXML_STATIC_FOR_DLL)
int XMLCALL
xmlDllMain(ATTRIBUTE_UNUSED void *hinstDLL, unsigned long fdwReason,
           ATTRIBUTE_UNUSED void *lpvReserved)
#else
/* declare to avoid "no previous prototype for 'DllMain'" warning */
/* Note that we do NOT want to include this function declaration in
   a public header because it's meant to be called by Windows itself,
   not a program that uses this library.  This also has to be exported. */

XMLPUBFUN BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD     fdwReason,
         LPVOID    lpvReserved);

BOOL WINAPI
DllMain(ATTRIBUTE_UNUSED HINSTANCE hinstDLL, DWORD fdwReason,
        ATTRIBUTE_UNUSED LPVOID lpvReserved)
#endif
{
    switch (fdwReason) {
        case DLL_THREAD_DETACH:
            if (globalkey != TLS_OUT_OF_INDEXES) {
                xmlGlobalState *globalval = NULL;
                xmlGlobalStateCleanupHelperParams *p =
                    (xmlGlobalStateCleanupHelperParams *)
                    TlsGetValue(globalkey);
                globalval = (xmlGlobalState *) (p ? p->memory : NULL);
                if (globalval) {
                    xmlFreeGlobalState(globalval);
                    TlsSetValue(globalkey, NULL);
                }
                if (p) {
                    EnterCriticalSection(&cleanup_helpers_cs);
                    if (p == cleanup_helpers_head)
                        cleanup_helpers_head = p->next;
                    else
                        p->prev->next = p->next;
                    if (p->next != NULL)
                        p->next->prev = p->prev;
                    LeaveCriticalSection(&cleanup_helpers_cs);
                    free(p);
                }
            }
            break;
    }
    return TRUE;
}
#endif
