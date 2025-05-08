/*
 * globals.c: definition and handling of the set of global variables
 *            of the library
 *
 * See Copyright for the status of this software.
 *
 * Gary Pennington <Gary.Pennington@uk.sun.com>
 * daniel@veillard.com
 */

#define IN_LIBXML
#include "libxml.h"

#include <stdlib.h>
#include <string.h>

#define XML_GLOBALS_NO_REDEFINITION
#include <libxml/xmlerror.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/threads.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <libxml/SAX2.h>

#include "private/dict.h"
#include "private/error.h"
#include "private/globals.h"
#include "private/threads.h"
#include "private/tree.h"

/*
 * Mutex to protect "ForNewThreads" variables
 */
static xmlMutex xmlThrDefMutex;

/*
 * Thread-local storage emulation.
 *
 * This works by replacing a global variable
 *
 *     extern xmlError xmlLastError;
 *
 * with a macro that calls a function returning a pointer to the global in
 * thread-local storage:
 *
 *     xmlError *__xmlLastError(void);
 *     #define xmlError (*__xmlLastError());
 *
 * The code can operate in a multitude of ways depending on the environment.
 * First we support POSIX and Windows threads. Then we support both
 * thread-local storage provided by the compiler and older methods like
 * thread-specific data (pthreads) or TlsAlloc (Windows).
 *
 * To clean up thread-local storage, we use thread-specific data on POSIX.
 * On Windows, we either use DllMain when compiling a DLL or a registered
 * wait function for static builds.
 *
 * Compiler TLS isn't really useful for now. It can make allocation more
 * robust on some platforms but it also increases the memory consumption
 * of each thread by ~250 bytes whether it uses libxml2 or not. The main
 * problem is that we have to deallocate strings in xmlLastError and C
 * offers no simple way to deallocate dynamic data in _Thread_local
 * variables. In C++, one could simply use a thread_local variable with a
 * destructor.
 *
 * At some point, many of the deprecated globals can be removed,
 * although things like global error handlers will take a while.
 * Ultimately, the only crucial things seem to be xmlLastError and
 * RNG state. xmlLastError already involves dynamic allocation, so it
 * could be allocated dynamically as well, only storing a global
 * pointer.
 */

#ifdef LIBXML_THREAD_ENABLED

#ifdef HAVE_WIN32_THREADS
  #if defined(LIBXML_STATIC) && !defined(LIBXML_STATIC_FOR_DLL)
    #define USE_WAIT_DTOR
  #else
    #define USE_DLL_MAIN
  #endif
#endif

/*
 * On Darwin, thread-local storage destructors seem to be run before
 * pthread thread-specific data destructors. This causes ASan to
 * report a use-after-free.
 *
 * On Windows, we can't use TLS in static builds. The RegisterWait
 * callback would run after TLS was deallocated.
 */
#if defined(XML_THREAD_LOCAL) && \
    !defined(__APPLE__) && \
    !defined(USE_WAIT_DTOR)
#define USE_TLS
#endif

#ifdef HAVE_POSIX_THREADS

/*
 * On POSIX, we need thread-specific data even with thread-local storage
 * to destroy indirect references from global state (xmlLastError) at
 * thread exit.
 */
static pthread_key_t globalkey;

#elif defined HAVE_WIN32_THREADS

#ifndef USE_TLS
static DWORD globalkey = TLS_OUT_OF_INDEXES;
#endif

#endif /* HAVE_WIN32_THREADS */

static void
xmlFreeGlobalState(void *state);

#endif /* LIBXML_THREAD_ENABLED */

struct _xmlGlobalState {
#ifdef USE_TLS
    int initialized;
#endif

#ifdef USE_WAIT_DTOR
    void *threadHandle;
    void *waitHandle;
#endif

    unsigned localRngState[2];

    xmlError lastError;

#ifdef LIBXML_THREAD_ALLOC_ENABLED
    xmlMallocFunc malloc;
    xmlMallocFunc mallocAtomic;
    xmlReallocFunc realloc;
    xmlFreeFunc free;
    xmlStrdupFunc memStrdup;
#endif

    int doValidityCheckingDefaultValue;
    int getWarningsDefaultValue;
    int keepBlanksDefaultValue;
    int lineNumbersDefaultValue;
    int loadExtDtdDefaultValue;
    int pedanticParserDefaultValue;
    int substituteEntitiesDefaultValue;

#ifdef LIBXML_OUTPUT_ENABLED
    int indentTreeOutput;
    const char *treeIndentString;
    int saveNoEmptyTags;
#endif

    xmlGenericErrorFunc genericError;
    void *genericErrorContext;
    xmlStructuredErrorFunc structuredError;
    void *structuredErrorContext;

    xmlRegisterNodeFunc registerNodeDefaultValue;
    xmlDeregisterNodeFunc deregisterNodeDefaultValue;

    xmlParserInputBufferCreateFilenameFunc parserInputBufferCreateFilenameValue;
    xmlOutputBufferCreateFilenameFunc outputBufferCreateFilenameValue;
};

typedef struct _xmlGlobalState xmlGlobalState;
typedef xmlGlobalState *xmlGlobalStatePtr;

#ifdef LIBXML_THREAD_ENABLED

#ifdef USE_TLS
static XML_THREAD_LOCAL xmlGlobalState globalState;
#endif

#else /* LIBXML_THREAD_ENABLED */

static xmlGlobalState globalState;

#endif /* LIBXML_THREAD_ENABLED */

/************************************************************************
 *									*
 *	All the user accessible global variables of the library		*
 *									*
 ************************************************************************/

/*
 * Memory allocation routines
 */

/**
 * xmlFree:
 * @mem: an already allocated block of memory
 *
 * The variable holding the libxml free() implementation
 */
xmlFreeFunc xmlFree = free;
/**
 * xmlMalloc:
 * @size:  the size requested in bytes
 *
 * The variable holding the libxml malloc() implementation
 *
 * Returns a pointer to the newly allocated block or NULL in case of error
 */
xmlMallocFunc xmlMalloc = malloc;
/**
 * xmlMallocAtomic:
 * @size:  the size requested in bytes
 *
 * The variable holding the libxml malloc() implementation for atomic
 * data (i.e. blocks not containing pointers), useful when using a
 * garbage collecting allocator.
 *
 * Returns a pointer to the newly allocated block or NULL in case of error
 */
xmlMallocFunc xmlMallocAtomic = malloc;
/**
 * xmlRealloc:
 * @mem: an already allocated block of memory
 * @size:  the new size requested in bytes
 *
 * The variable holding the libxml realloc() implementation
 *
 * Returns a pointer to the newly reallocated block or NULL in case of error
 */
xmlReallocFunc xmlRealloc = realloc;
/**
 * xmlPosixStrdup
 * @cur:  the input char *
 *
 * a strdup implementation with a type signature matching POSIX
 *
 * Returns a new xmlChar * or NULL
 */
static char *
xmlPosixStrdup(const char *cur) {
    return((char*) xmlCharStrdup(cur));
}
/**
 * xmlMemStrdup:
 * @str: a zero terminated string
 *
 * The variable holding the libxml strdup() implementation
 *
 * Returns the copy of the string or NULL in case of error
 */
xmlStrdupFunc xmlMemStrdup = xmlPosixStrdup;

/*
 * Parser defaults
 */

static int xmlDoValidityCheckingDefaultValueThrDef = 0;
static int xmlGetWarningsDefaultValueThrDef = 1;
static int xmlLoadExtDtdDefaultValueThrDef = 0;
static int xmlPedanticParserDefaultValueThrDef = 0;
static int xmlLineNumbersDefaultValueThrDef = 0;
static int xmlKeepBlanksDefaultValueThrDef = 1;
static int xmlSubstituteEntitiesDefaultValueThrDef = 0;

static xmlRegisterNodeFunc xmlRegisterNodeDefaultValueThrDef = NULL;
static xmlDeregisterNodeFunc xmlDeregisterNodeDefaultValueThrDef = NULL;

static xmlParserInputBufferCreateFilenameFunc
xmlParserInputBufferCreateFilenameValueThrDef = NULL;
static xmlOutputBufferCreateFilenameFunc
xmlOutputBufferCreateFilenameValueThrDef = NULL;

static xmlGenericErrorFunc xmlGenericErrorThrDef = xmlGenericErrorDefaultFunc;
static xmlStructuredErrorFunc xmlStructuredErrorThrDef = NULL;
static void *xmlGenericErrorContextThrDef = NULL;
static void *xmlStructuredErrorContextThrDef = NULL;

#ifdef LIBXML_OUTPUT_ENABLED
static int xmlIndentTreeOutputThrDef = 1;
static const char *xmlTreeIndentStringThrDef = "  ";
static int xmlSaveNoEmptyTagsThrDef = 0;
#endif /* LIBXML_OUTPUT_ENABLED */

#ifdef LIBXML_SAX1_ENABLED
/**
 * xmlDefaultSAXHandler:
 *
 * DEPRECATED: This handler is unused and will be removed from future
 * versions.
 *
 * Default SAX version1 handler for XML, builds the DOM tree
 */
const xmlSAXHandlerV1 xmlDefaultSAXHandler = {
    xmlSAX2InternalSubset,
    xmlSAX2IsStandalone,
    xmlSAX2HasInternalSubset,
    xmlSAX2HasExternalSubset,
    xmlSAX2ResolveEntity,
    xmlSAX2GetEntity,
    xmlSAX2EntityDecl,
    xmlSAX2NotationDecl,
    xmlSAX2AttributeDecl,
    xmlSAX2ElementDecl,
    xmlSAX2UnparsedEntityDecl,
    xmlSAX2SetDocumentLocator,
    xmlSAX2StartDocument,
    xmlSAX2EndDocument,
    xmlSAX2StartElement,
    xmlSAX2EndElement,
    xmlSAX2Reference,
    xmlSAX2Characters,
    xmlSAX2Characters,
    xmlSAX2ProcessingInstruction,
    xmlSAX2Comment,
    xmlParserWarning,
    xmlParserError,
    xmlParserError,
    xmlSAX2GetParameterEntity,
    xmlSAX2CDataBlock,
    xmlSAX2ExternalSubset,
    1,
};
#endif /* LIBXML_SAX1_ENABLED */

/**
 * xmlDefaultSAXLocator:
 *
 * DEPRECATED: Don't use
 *
 * The default SAX Locator
 * { getPublicId, getSystemId, getLineNumber, getColumnNumber}
 */
const xmlSAXLocator xmlDefaultSAXLocator = {
    xmlSAX2GetPublicId,
    xmlSAX2GetSystemId,
    xmlSAX2GetLineNumber,
    xmlSAX2GetColumnNumber
};

#if defined(LIBXML_HTML_ENABLED) && defined(LIBXML_SAX1_ENABLED)
/**
 * htmlDefaultSAXHandler:
 *
 * DEPRECATED: This handler is unused and will be removed from future
 * versions.
 *
 * Default old SAX v1 handler for HTML, builds the DOM tree
 */
const xmlSAXHandlerV1 htmlDefaultSAXHandler = {
    xmlSAX2InternalSubset,
    NULL,
    NULL,
    NULL,
    NULL,
    xmlSAX2GetEntity,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    xmlSAX2SetDocumentLocator,
    xmlSAX2StartDocument,
    xmlSAX2EndDocument,
    xmlSAX2StartElement,
    xmlSAX2EndElement,
    NULL,
    xmlSAX2Characters,
    xmlSAX2IgnorableWhitespace,
    xmlSAX2ProcessingInstruction,
    xmlSAX2Comment,
    xmlParserWarning,
    xmlParserError,
    xmlParserError,
    NULL,
    xmlSAX2CDataBlock,
    NULL,
    1,
};
#endif /* LIBXML_HTML_ENABLED */

static void
xmlInitGlobalState(xmlGlobalStatePtr gs);

/************************************************************************
 *									*
 *			Per thread global state handling		*
 *									*
 ************************************************************************/

/**
 * xmlInitGlobals:
 *
 * DEPRECATED: Alias for xmlInitParser.
 */
void xmlInitGlobals(void) {
    xmlInitParser();
}

/**
 * xmlInitGlobalsInternal:
 *
 * Additional initialisation for multi-threading
 */
void xmlInitGlobalsInternal(void) {
    xmlInitMutex(&xmlThrDefMutex);

#ifdef HAVE_POSIX_THREADS
    pthread_key_create(&globalkey, xmlFreeGlobalState);
#elif defined(HAVE_WIN32_THREADS)
#ifndef USE_TLS
    if (globalkey == TLS_OUT_OF_INDEXES)
        globalkey = TlsAlloc();
#endif
#else /* no thread support */
    xmlInitGlobalState(&globalState);
#endif
}

/**
 * xmlCleanupGlobals:
 *
 * DEPRECATED: This function is a no-op. Call xmlCleanupParser
 * to free global state but see the warnings there. xmlCleanupParser
 * should be only called once at program exit. In most cases, you don't
 * have call cleanup functions at all.
 */
void xmlCleanupGlobals(void) {
}

/**
 * xmlCleanupGlobalsInternal:
 *
 * Additional cleanup for multi-threading
 */
void xmlCleanupGlobalsInternal(void) {
    /*
     * We assume that all other threads using the library have
     * terminated and the last remaining thread calls
     * xmlCleanupParser.
     */

#ifdef HAVE_POSIX_THREADS
    /*
     * Free thread-specific data of last thread before calling
     * pthread_key_delete.
     */
    xmlGlobalState *gs = pthread_getspecific(globalkey);
    if (gs != NULL)
        xmlFreeGlobalState(gs);
    pthread_key_delete(globalkey);
#elif defined(HAVE_WIN32_THREADS)
#if defined(USE_WAIT_DTOR) && !defined(USE_TLS)
    if (globalkey != TLS_OUT_OF_INDEXES) {
        TlsFree(globalkey);
        globalkey = TLS_OUT_OF_INDEXES;
    }
#endif
#else /* no thread support */
    xmlResetError(&globalState.lastError);
#endif

    xmlCleanupMutex(&xmlThrDefMutex);
}

#ifdef LIBXML_THREAD_ENABLED

static void
xmlFreeGlobalState(void *state)
{
    xmlGlobalState *gs = (xmlGlobalState *) state;

    /*
     * Free any memory allocated in the thread's error struct. If it
     * weren't for this indirect allocation, we wouldn't need
     * a destructor with thread-local storage at all!
     */
    xmlResetError(&gs->lastError);
#ifndef USE_TLS
    free(state);
#endif
}

#if defined(USE_WAIT_DTOR)
static void WINAPI
xmlGlobalStateDtor(void *ctxt, unsigned char timedOut ATTRIBUTE_UNUSED) {
    xmlGlobalStatePtr gs = ctxt;

    UnregisterWait(gs->waitHandle);
    CloseHandle(gs->threadHandle);
    xmlFreeGlobalState(gs);
}

static int
xmlRegisterGlobalStateDtor(xmlGlobalState *gs) {
    void *processHandle = GetCurrentProcess();
    void *threadHandle;
    void *waitHandle;

    if (DuplicateHandle(processHandle, GetCurrentThread(), processHandle,
                &threadHandle, 0, FALSE, DUPLICATE_SAME_ACCESS) == 0) {
        return(-1);
    }

    if (RegisterWaitForSingleObject(&waitHandle, threadHandle,
                xmlGlobalStateDtor, gs, INFINITE, WT_EXECUTEONLYONCE) == 0) {
        CloseHandle(threadHandle);
        return(-1);
    }

    gs->threadHandle = threadHandle;
    gs->waitHandle = waitHandle;
    return(0);
}
#endif /* USE_WAIT_DTOR */

#ifndef USE_TLS
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
xmlNewGlobalState(int allowFailure)
{
    xmlGlobalState *gs;

    /*
     * We use malloc/free to allow accessing globals before setting
     * custom memory allocators.
     */
    gs = malloc(sizeof(xmlGlobalState));
    if (gs == NULL) {
        if (allowFailure)
            return(NULL);

        /*
         * If an application didn't call xmlCheckThreadLocalStorage to make
         * sure that global state could be allocated, it's too late to
         * handle the error.
         */
        xmlAbort("libxml2: Failed to allocate globals for thread\n"
                 "libxml2: See xmlCheckThreadLocalStorage\n");
    }

    memset(gs, 0, sizeof(xmlGlobalState));
    xmlInitGlobalState(gs);
    return (gs);
}
#endif

static xmlGlobalStatePtr
xmlGetThreadLocalStorage(int allowFailure) {
    xmlGlobalState *gs;

    (void) allowFailure;

    xmlInitParser();

#ifdef USE_TLS
    gs = &globalState;
    if (gs->initialized == 0)
        xmlInitGlobalState(gs);
#elif defined(HAVE_POSIX_THREADS)
    gs = (xmlGlobalState *) pthread_getspecific(globalkey);
    if (gs == NULL)
        gs = xmlNewGlobalState(allowFailure);
#elif defined(HAVE_WIN32_THREADS)
    gs = (xmlGlobalState *) TlsGetValue(globalkey);
    if (gs == NULL)
        gs = xmlNewGlobalState(allowFailure);
#else
    gs = NULL;
#endif

    return(gs);
}

#else /* LIBXML_THREAD_ENABLED */

static xmlGlobalStatePtr
xmlGetThreadLocalStorage(int allowFailure ATTRIBUTE_UNUSED) {
    return(&globalState);
}

#endif /* LIBXML_THREAD_ENABLED */

static void
xmlInitGlobalState(xmlGlobalStatePtr gs) {
    gs->localRngState[0] = xmlGlobalRandom();
    gs->localRngState[1] = xmlGlobalRandom();

    memset(&gs->lastError, 0, sizeof(xmlError));

#ifdef LIBXML_THREAD_ALLOC_ENABLED
    /* XML_GLOBALS_ALLOC */
    gs->free = free;
    gs->malloc = malloc;
    gs->mallocAtomic = malloc;
    gs->realloc = realloc;
    gs->memStrdup = xmlPosixStrdup;
#endif

    xmlMutexLock(&xmlThrDefMutex);

    /* XML_GLOBALS_PARSER */
    gs->doValidityCheckingDefaultValue =
         xmlDoValidityCheckingDefaultValueThrDef;
    gs->getWarningsDefaultValue = xmlGetWarningsDefaultValueThrDef;
    gs->keepBlanksDefaultValue = xmlKeepBlanksDefaultValueThrDef;
    gs->lineNumbersDefaultValue = xmlLineNumbersDefaultValueThrDef;
    gs->loadExtDtdDefaultValue = xmlLoadExtDtdDefaultValueThrDef;
    gs->pedanticParserDefaultValue = xmlPedanticParserDefaultValueThrDef;
    gs->substituteEntitiesDefaultValue =
        xmlSubstituteEntitiesDefaultValueThrDef;
#ifdef LIBXML_OUTPUT_ENABLED
    gs->indentTreeOutput = xmlIndentTreeOutputThrDef;
    gs->treeIndentString = xmlTreeIndentStringThrDef;
    gs->saveNoEmptyTags = xmlSaveNoEmptyTagsThrDef;
#endif

    /* XML_GLOBALS_ERROR */
    gs->genericError = xmlGenericErrorThrDef;
    gs->structuredError = xmlStructuredErrorThrDef;
    gs->genericErrorContext = xmlGenericErrorContextThrDef;
    gs->structuredErrorContext = xmlStructuredErrorContextThrDef;

    /* XML_GLOBALS_TREE */
    gs->registerNodeDefaultValue = xmlRegisterNodeDefaultValueThrDef;
    gs->deregisterNodeDefaultValue = xmlDeregisterNodeDefaultValueThrDef;

    /* XML_GLOBALS_IO */
    gs->parserInputBufferCreateFilenameValue =
        xmlParserInputBufferCreateFilenameValueThrDef;
    gs->outputBufferCreateFilenameValue =
        xmlOutputBufferCreateFilenameValueThrDef;

    xmlMutexUnlock(&xmlThrDefMutex);

#ifdef USE_TLS
    gs->initialized = 1;
#endif

#ifdef HAVE_POSIX_THREADS
    pthread_setspecific(globalkey, gs);
#elif defined HAVE_WIN32_THREADS
#ifndef USE_TLS
    TlsSetValue(globalkey, gs);
#endif
#ifdef USE_WAIT_DTOR
    xmlRegisterGlobalStateDtor(gs);
#endif
#endif
}

const xmlError *
__xmlLastError(void) {
    return(&xmlGetThreadLocalStorage(0)->lastError);
}

int *
__xmlDoValidityCheckingDefaultValue(void) {
    return(&xmlGetThreadLocalStorage(0)->doValidityCheckingDefaultValue);
}

int *
__xmlGetWarningsDefaultValue(void) {
    return(&xmlGetThreadLocalStorage(0)->getWarningsDefaultValue);
}

int *
__xmlKeepBlanksDefaultValue(void) {
    return(&xmlGetThreadLocalStorage(0)->keepBlanksDefaultValue);
}

int *
__xmlLineNumbersDefaultValue(void) {
    return(&xmlGetThreadLocalStorage(0)->lineNumbersDefaultValue);
}

int *
__xmlLoadExtDtdDefaultValue(void) {
    return(&xmlGetThreadLocalStorage(0)->loadExtDtdDefaultValue);
}

int *
__xmlPedanticParserDefaultValue(void) {
    return(&xmlGetThreadLocalStorage(0)->pedanticParserDefaultValue);
}

int *
__xmlSubstituteEntitiesDefaultValue(void) {
    return(&xmlGetThreadLocalStorage(0)->substituteEntitiesDefaultValue);
}

#ifdef LIBXML_OUTPUT_ENABLED
int *
__xmlIndentTreeOutput(void) {
    return(&xmlGetThreadLocalStorage(0)->indentTreeOutput);
}

const char **
__xmlTreeIndentString(void) {
    return(&xmlGetThreadLocalStorage(0)->treeIndentString);
}

int *
__xmlSaveNoEmptyTags(void) {
    return(&xmlGetThreadLocalStorage(0)->saveNoEmptyTags);
}
#endif

xmlGenericErrorFunc *
__xmlGenericError(void) {
    return(&xmlGetThreadLocalStorage(0)->genericError);
}

void **
__xmlGenericErrorContext(void) {
    return(&xmlGetThreadLocalStorage(0)->genericErrorContext);
}

xmlStructuredErrorFunc *
__xmlStructuredError(void) {
    return(&xmlGetThreadLocalStorage(0)->structuredError);
}

void **
__xmlStructuredErrorContext(void) {
    return(&xmlGetThreadLocalStorage(0)->structuredErrorContext);
}

xmlRegisterNodeFunc *
__xmlRegisterNodeDefaultValue(void) {
    return(&xmlGetThreadLocalStorage(0)->registerNodeDefaultValue);
}

xmlDeregisterNodeFunc *
__xmlDeregisterNodeDefaultValue(void) {
    return(&xmlGetThreadLocalStorage(0)->deregisterNodeDefaultValue);
}

xmlParserInputBufferCreateFilenameFunc *
__xmlParserInputBufferCreateFilenameValue(void) {
    return(&xmlGetThreadLocalStorage(0)->parserInputBufferCreateFilenameValue);
}

xmlOutputBufferCreateFilenameFunc *
__xmlOutputBufferCreateFilenameValue(void) {
    return(&xmlGetThreadLocalStorage(0)->outputBufferCreateFilenameValue);
}

#ifdef LIBXML_THREAD_ALLOC_ENABLED
xmlMallocFunc *
__xmlMalloc(void) {
    return(&xmlGetThreadLocalStorage(0)->malloc);
}

xmlMallocFunc *
__xmlMallocAtomic(void) {
    return(&xmlGetThreadLocalStorage(0)->mallocAtomic);
}

xmlReallocFunc *
__xmlRealloc(void) {
    return(&xmlGetThreadLocalStorage(0)->realloc);
}

xmlFreeFunc *
__xmlFree(void) {
    return(&xmlGetThreadLocalStorage(0)->free);
}

xmlStrdupFunc *
__xmlMemStrdup(void) {
    return(&xmlGetThreadLocalStorage(0)->memStrdup);
}
#endif /* LIBXML_THREAD_ALLOC_ENABLED */

/**
 * xmlGetLocalRngState:
 *
 * Returns the local RNG state.
 */
unsigned *
xmlGetLocalRngState(void) {
    return(xmlGetThreadLocalStorage(0)->localRngState);
}

/**
 * xmlCheckThreadLocalStorage:
 *
 * Check whether thread-local storage could be allocated.
 *
 * In cross-platform code running in multithreaded environments, this
 * function should be called once in each thread before calling other
 * library functions to make sure that thread-local storage was
 * allocated properly.
 *
 * Returns 0 on success or -1 if a memory allocation failed. A failed
 * allocation signals a typically fatal and irrecoverable out-of-memory
 * situation. Don't call any library functions in this case.
 *
 * Available since 2.12.0.
 */
int
xmlCheckThreadLocalStorage(void) {
#if defined(LIBXML_THREAD_ENABLED) && !defined(USE_TLS)
    if (xmlGetThreadLocalStorage(1) == NULL)
        return(-1);
#endif
    return(0);
}

/**
 * xmlGetLastErrorInternal:
 *
 * Returns a pointer to the global error struct.
 */
xmlError *
xmlGetLastErrorInternal(void) {
    return(&xmlGetThreadLocalStorage(0)->lastError);
}

/** DOC_DISABLE */

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
#ifdef USE_DLL_MAIN
#if defined(LIBXML_STATIC_FOR_DLL)
int
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
#ifdef USE_TLS
            xmlFreeGlobalState(&globalState);
#else
            if (globalkey != TLS_OUT_OF_INDEXES) {
                xmlGlobalState *globalval;

                globalval = (xmlGlobalState *) TlsGetValue(globalkey);
                if (globalval) {
                    xmlFreeGlobalState(globalval);
                    TlsSetValue(globalkey, NULL);
                }
            }
#endif
            break;

#ifndef LIBXML_THREAD_ALLOC_ENABLED
        case DLL_PROCESS_DETACH:
            if (xmlFree == free)
                xmlCleanupParser();
            if (globalkey != TLS_OUT_OF_INDEXES) {
                TlsFree(globalkey);
                globalkey = TLS_OUT_OF_INDEXES;
            }
            break;
#endif
    }
    return TRUE;
}
#endif /* USE_DLL_MAIN */

void
xmlThrDefSetGenericErrorFunc(void *ctx, xmlGenericErrorFunc handler) {
    xmlMutexLock(&xmlThrDefMutex);
    xmlGenericErrorContextThrDef = ctx;
    if (handler != NULL)
	xmlGenericErrorThrDef = handler;
    else
	xmlGenericErrorThrDef = xmlGenericErrorDefaultFunc;
    xmlMutexUnlock(&xmlThrDefMutex);
}

void
xmlThrDefSetStructuredErrorFunc(void *ctx, xmlStructuredErrorFunc handler) {
    xmlMutexLock(&xmlThrDefMutex);
    xmlStructuredErrorContextThrDef = ctx;
    xmlStructuredErrorThrDef = handler;
    xmlMutexUnlock(&xmlThrDefMutex);
}

int xmlThrDefDoValidityCheckingDefaultValue(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlDoValidityCheckingDefaultValueThrDef;
    xmlDoValidityCheckingDefaultValueThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}

int xmlThrDefGetWarningsDefaultValue(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlGetWarningsDefaultValueThrDef;
    xmlGetWarningsDefaultValueThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}

#ifdef LIBXML_OUTPUT_ENABLED
int xmlThrDefIndentTreeOutput(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlIndentTreeOutputThrDef;
    xmlIndentTreeOutputThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}

const char * xmlThrDefTreeIndentString(const char * v) {
    const char * ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlTreeIndentStringThrDef;
    xmlTreeIndentStringThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}

int xmlThrDefSaveNoEmptyTags(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlSaveNoEmptyTagsThrDef;
    xmlSaveNoEmptyTagsThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}
#endif

int xmlThrDefKeepBlanksDefaultValue(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlKeepBlanksDefaultValueThrDef;
    xmlKeepBlanksDefaultValueThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}

int xmlThrDefLineNumbersDefaultValue(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlLineNumbersDefaultValueThrDef;
    xmlLineNumbersDefaultValueThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}

int xmlThrDefLoadExtDtdDefaultValue(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlLoadExtDtdDefaultValueThrDef;
    xmlLoadExtDtdDefaultValueThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}

int xmlThrDefPedanticParserDefaultValue(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlPedanticParserDefaultValueThrDef;
    xmlPedanticParserDefaultValueThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}

int xmlThrDefSubstituteEntitiesDefaultValue(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlSubstituteEntitiesDefaultValueThrDef;
    xmlSubstituteEntitiesDefaultValueThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}

xmlRegisterNodeFunc
xmlThrDefRegisterNodeDefault(xmlRegisterNodeFunc func)
{
    xmlRegisterNodeFunc old;

    xmlMutexLock(&xmlThrDefMutex);
    old = xmlRegisterNodeDefaultValueThrDef;

    xmlRegisterCallbacks = 1;
    xmlRegisterNodeDefaultValueThrDef = func;
    xmlMutexUnlock(&xmlThrDefMutex);

    return(old);
}

xmlDeregisterNodeFunc
xmlThrDefDeregisterNodeDefault(xmlDeregisterNodeFunc func)
{
    xmlDeregisterNodeFunc old;

    xmlMutexLock(&xmlThrDefMutex);
    old = xmlDeregisterNodeDefaultValueThrDef;

    xmlRegisterCallbacks = 1;
    xmlDeregisterNodeDefaultValueThrDef = func;
    xmlMutexUnlock(&xmlThrDefMutex);

    return(old);
}

xmlParserInputBufferCreateFilenameFunc
xmlThrDefParserInputBufferCreateFilenameDefault(xmlParserInputBufferCreateFilenameFunc func)
{
    xmlParserInputBufferCreateFilenameFunc old;

    xmlMutexLock(&xmlThrDefMutex);
    old = xmlParserInputBufferCreateFilenameValueThrDef;
    if (old == NULL) {
		old = __xmlParserInputBufferCreateFilename;
	}

    xmlParserInputBufferCreateFilenameValueThrDef = func;
    xmlMutexUnlock(&xmlThrDefMutex);

    return(old);
}

xmlOutputBufferCreateFilenameFunc
xmlThrDefOutputBufferCreateFilenameDefault(xmlOutputBufferCreateFilenameFunc func)
{
    xmlOutputBufferCreateFilenameFunc old;

    xmlMutexLock(&xmlThrDefMutex);
    old = xmlOutputBufferCreateFilenameValueThrDef;
#ifdef LIBXML_OUTPUT_ENABLED
    if (old == NULL) {
		old = __xmlOutputBufferCreateFilename;
	}
#endif
    xmlOutputBufferCreateFilenameValueThrDef = func;
    xmlMutexUnlock(&xmlThrDefMutex);

    return(old);
}

/** DOC_ENABLE */

