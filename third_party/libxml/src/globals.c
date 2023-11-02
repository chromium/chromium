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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XML_GLOBALS_NO_REDEFINITION
#include <libxml/globals.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlIO.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/threads.h>
#include <libxml/tree.h>
#include <libxml/SAX.h>
#include <libxml/SAX2.h>

#include "private/error.h"
#include "private/globals.h"
#include "private/threads.h"
#include "private/tree.h"

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
 * First we support POSIX and Windows threads. Then we support both thread-local
 * storage provided by the compiler and older methods like thread-specific data
 * (pthreads) or TlsAlloc (Windows).
 *
 * To clean up thread-local storage, we use thread-specific data on POSIX.
 * On Windows, we either use DllMain when compiling a DLL or a registered wait
 * function for static builds.
 */

/*
 * Helpful Macro
 */
#ifdef LIBXML_THREAD_ENABLED
#define IS_MAIN_THREAD (xmlIsMainThreadInternal())
#else
#define IS_MAIN_THREAD 1
#endif

#define XML_DECLARE_MEMBER(name, type, attrs) \
  type gs_##name;

struct _xmlGlobalState {
    int initialized;

#if defined(HAVE_WIN32_THREADS) && \
    defined(LIBXML_STATIC) && !defined(LIBXML_STATIC_FOR_DLL)
    void *threadHandle;
    void *waitHandle;
#endif

#define XML_OP XML_DECLARE_MEMBER
XML_GLOBALS_ALLOC
XML_GLOBALS_ERROR
XML_GLOBALS_HTML
XML_GLOBALS_IO
XML_GLOBALS_PARSER
XML_GLOBALS_SAVE
XML_GLOBALS_TREE
#undef XML_OP
};

static int parserInitialized;

/*
 * Mutex to protect "ForNewThreads" variables
 */
static xmlMutex xmlThrDefMutex;

#ifdef LIBXML_THREAD_ENABLED

/*
 * On Darwin, thread-local storage destructors seem to be run before
 * pthread thread-specific data destructors. This causes ASan to
 * report a use-after-free.
 */
#if defined(XML_THREAD_LOCAL) && !defined(__APPLE__)
#define USE_TLS
#endif

#ifdef USE_TLS
static XML_THREAD_LOCAL xmlGlobalState globalState;
#endif

#ifdef HAVE_POSIX_THREADS

/*
 * Weak symbol hack, see threads.c
 */
#if defined(__GNUC__) && \
    defined(__GLIBC__) && \
    __GLIBC__ * 100 + __GLIBC_MINOR__ < 234

#define XML_PTHREAD_WEAK

static int libxml_is_threaded = -1;

#endif

/*
 * On POSIX, we need thread-specific data even with thread-local storage
 * to destroy indirect references from global state (xmlLastError) at
 * thread exit.
 */
static pthread_key_t globalkey;
static pthread_t mainthread;

#elif defined HAVE_WIN32_THREADS

#ifndef USE_TLS
static DWORD globalkey = TLS_OUT_OF_INDEXES;
#endif
static DWORD mainthread;

#endif /* HAVE_WIN32_THREADS */

static void
xmlFreeGlobalState(void *state);

#endif /* LIBXML_THREAD_ENABLED */

/************************************************************************
 *									*
 *	All the user accessible global variables of the library		*
 *									*
 ************************************************************************/

/*
 * Memory allocation routines
 */

#if defined(DEBUG_MEMORY_LOCATION)
xmlFreeFunc xmlFree = (xmlFreeFunc) xmlMemFree;
xmlMallocFunc xmlMalloc = (xmlMallocFunc) xmlMemMalloc;
xmlMallocFunc xmlMallocAtomic = (xmlMallocFunc) xmlMemMalloc;
xmlReallocFunc xmlRealloc = (xmlReallocFunc) xmlMemRealloc;
xmlStrdupFunc xmlMemStrdup = (xmlStrdupFunc) xmlMemoryStrdup;
#else
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
#endif /* DEBUG_MEMORY_LOCATION */

/**
 * xmlBufferAllocScheme:
 *
 * DEPRECATED: Don't use.
 *
 * Global setting, default allocation policy for buffers, default is
 * XML_BUFFER_ALLOC_EXACT
 */
xmlBufferAllocationScheme xmlBufferAllocScheme = XML_BUFFER_ALLOC_EXACT;
static xmlBufferAllocationScheme xmlBufferAllocSchemeThrDef = XML_BUFFER_ALLOC_EXACT;
/**
 * xmlDefaultBufferSize:
 *
 * DEPRECATED: Don't use.
 *
 * Global setting, default buffer size. Default value is BASE_BUFFER_SIZE
 */
int xmlDefaultBufferSize = BASE_BUFFER_SIZE;
static int xmlDefaultBufferSizeThrDef = BASE_BUFFER_SIZE;

/*
 * Parser defaults
 */

/**
 * oldXMLWDcompatibility:
 *
 * Global setting, DEPRECATED.
 */
int oldXMLWDcompatibility = 0; /* DEPRECATED */
/**
 * xmlParserDebugEntities:
 *
 * DEPRECATED: Don't use
 *
 * Global setting, asking the parser to print out debugging information.
 * while handling entities.
 * Disabled by default
 */
int xmlParserDebugEntities = 0;
static int xmlParserDebugEntitiesThrDef = 0;
/**
 * xmlDoValidityCheckingDefaultValue:
 *
 * DEPRECATED: Use the modern options API with XML_PARSE_DTDVALID.
 *
 * Global setting, indicate that the parser should work in validating mode.
 * Disabled by default.
 */
int xmlDoValidityCheckingDefaultValue = 0;
static int xmlDoValidityCheckingDefaultValueThrDef = 0;
/**
 * xmlGetWarningsDefaultValue:
 *
 * DEPRECATED: Don't use
 *
 * Global setting, indicate that the DTD validation should provide warnings.
 * Activated by default.
 */
int xmlGetWarningsDefaultValue = 1;
static int xmlGetWarningsDefaultValueThrDef = 1;
/**
 * xmlLoadExtDtdDefaultValue:
 *
 * DEPRECATED: Use the modern options API with XML_PARSE_DTDLOAD.
 *
 * Global setting, indicate that the parser should load DTD while not
 * validating.
 * Disabled by default.
 */
int xmlLoadExtDtdDefaultValue = 0;
static int xmlLoadExtDtdDefaultValueThrDef = 0;
/**
 * xmlPedanticParserDefaultValue:
 *
 * DEPRECATED: Use the modern options API with XML_PARSE_PEDANTIC.
 *
 * Global setting, indicate that the parser be pedantic
 * Disabled by default.
 */
int xmlPedanticParserDefaultValue = 0;
static int xmlPedanticParserDefaultValueThrDef = 0;
/**
 * xmlLineNumbersDefaultValue:
 *
 * DEPRECATED: The modern options API always enables line numbers.
 *
 * Global setting, indicate that the parser should store the line number
 * in the content field of elements in the DOM tree.
 * Disabled by default since this may not be safe for old classes of
 * application.
 */
int xmlLineNumbersDefaultValue = 0;
static int xmlLineNumbersDefaultValueThrDef = 0;
/**
 * xmlKeepBlanksDefaultValue:
 *
 * DEPRECATED: Use the modern options API with XML_PARSE_NOBLANKS.
 *
 * Global setting, indicate that the parser should keep all blanks
 * nodes found in the content
 * Activated by default, this is actually needed to have the parser
 * conformant to the XML Recommendation, however the option is kept
 * for some applications since this was libxml1 default behaviour.
 */
int xmlKeepBlanksDefaultValue = 1;
static int xmlKeepBlanksDefaultValueThrDef = 1;
/**
 * xmlSubstituteEntitiesDefaultValue:
 *
 * DEPRECATED: Use the modern options API with XML_PARSE_NOENT.
 *
 * Global setting, indicate that the parser should not generate entity
 * references but replace them with the actual content of the entity
 * Disabled by default, this should be activated when using XPath since
 * the XPath data model requires entities replacement and the XPath
 * engine does not handle entities references transparently.
 */
int xmlSubstituteEntitiesDefaultValue = 0;
static int xmlSubstituteEntitiesDefaultValueThrDef = 0;

/**
 * xmlRegisterNodeDefaultValue:
 *
 * DEPRECATED: Don't use
 */
xmlRegisterNodeFunc xmlRegisterNodeDefaultValue = NULL;
static xmlRegisterNodeFunc xmlRegisterNodeDefaultValueThrDef = NULL;

/**
 * xmlDeregisterNodeDefaultValue:
 *
 * DEPRECATED: Don't use
 */
xmlDeregisterNodeFunc xmlDeregisterNodeDefaultValue = NULL;
static xmlDeregisterNodeFunc xmlDeregisterNodeDefaultValueThrDef = NULL;

/**
 * xmlParserInputBufferCreateFilenameValue:
 *
 * DEPRECATED: Don't use
 */
xmlParserInputBufferCreateFilenameFunc xmlParserInputBufferCreateFilenameValue = NULL;
static xmlParserInputBufferCreateFilenameFunc xmlParserInputBufferCreateFilenameValueThrDef = NULL;

/**
 * xmlOutputBufferCreateFilenameValue:
 *
 * DEPRECATED: Don't use
 */
xmlOutputBufferCreateFilenameFunc xmlOutputBufferCreateFilenameValue = NULL;
static xmlOutputBufferCreateFilenameFunc xmlOutputBufferCreateFilenameValueThrDef = NULL;

/**
 * xmlGenericError:
 *
 * Global setting: function used for generic error callbacks
 */
xmlGenericErrorFunc xmlGenericError = xmlGenericErrorDefaultFunc;
static xmlGenericErrorFunc xmlGenericErrorThrDef = xmlGenericErrorDefaultFunc;
/**
 * xmlStructuredError:
 *
 * Global setting: function used for structured error callbacks
 */
xmlStructuredErrorFunc xmlStructuredError = NULL;
static xmlStructuredErrorFunc xmlStructuredErrorThrDef = NULL;
/**
 * xmlGenericErrorContext:
 *
 * Global setting passed to generic error callbacks
 */
void *xmlGenericErrorContext = NULL;
static void *xmlGenericErrorContextThrDef = NULL;
/**
 * xmlStructuredErrorContext:
 *
 * Global setting passed to structured error callbacks
 */
void *xmlStructuredErrorContext = NULL;
static void *xmlStructuredErrorContextThrDef = NULL;
xmlError xmlLastError;

#ifdef LIBXML_OUTPUT_ENABLED
/*
 * output defaults
 */
/**
 * xmlIndentTreeOutput:
 *
 * Global setting, asking the serializer to indent the output tree by default
 * Enabled by default
 */
int xmlIndentTreeOutput = 1;
static int xmlIndentTreeOutputThrDef = 1;

/**
 * xmlTreeIndentString:
 *
 * The string used to do one-level indent. By default is equal to "  " (two spaces)
 */
const char *xmlTreeIndentString = "  ";
static const char *xmlTreeIndentStringThrDef = "  ";

/**
 * xmlSaveNoEmptyTags:
 *
 * Global setting, asking the serializer to not output empty tags
 * as <empty/> but <empty></empty>. those two forms are indistinguishable
 * once parsed.
 * Disabled by default
 */
int xmlSaveNoEmptyTags = 0;
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
xmlSAXHandlerV1 xmlDefaultSAXHandler = {
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
xmlSAXLocator xmlDefaultSAXLocator = {
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
xmlSAXHandlerV1 htmlDefaultSAXHandler = {
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
#ifdef XML_PTHREAD_WEAK
    if (libxml_is_threaded == -1)
        libxml_is_threaded =
            (pthread_getspecific != NULL) &&
            (pthread_setspecific != NULL) &&
            (pthread_key_create != NULL) &&
            (pthread_key_delete != NULL);
    if (libxml_is_threaded == 0)
        return;
#endif /* XML_PTHREAD_WEAK */
    pthread_key_create(&globalkey, xmlFreeGlobalState);
    mainthread = pthread_self();
#elif defined(HAVE_WIN32_THREADS)
#ifndef USE_TLS
    globalkey = TlsAlloc();
#endif
    mainthread = GetCurrentThreadId();
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
    xmlResetError(&xmlLastError);

    xmlCleanupMutex(&xmlThrDefMutex);

#ifdef HAVE_POSIX_THREADS
#ifdef XML_PTHREAD_WEAK
    if (libxml_is_threaded == 0)
        return;
#endif /* XML_PTHREAD_WEAK */
    pthread_key_delete(globalkey);
#elif defined(HAVE_WIN32_THREADS)
#ifndef USE_TLS
    if (globalkey != TLS_OUT_OF_INDEXES) {
        TlsFree(globalkey);
        globalkey = TLS_OUT_OF_INDEXES;
    }
#endif
#endif

    parserInitialized = 0;
}

/**
 * xmlInitializeGlobalState:
 * @gs: a pointer to a newly allocated global state
 *
 * DEPRECATED: No-op.
 */
void
xmlInitializeGlobalState(xmlGlobalStatePtr gs ATTRIBUTE_UNUSED)
{
}

/**
 * xmlGetGlobalState:
 *
 * DEPRECATED
 *
 * Returns NULL.
 */
xmlGlobalStatePtr
xmlGetGlobalState(void)
{
    return(NULL);
}

static int
xmlIsMainThreadInternal(void) {
    if (parserInitialized == 0) {
        xmlInitParser();
        parserInitialized = 1;
    }

#ifdef HAVE_POSIX_THREADS
#ifdef XML_PTHREAD_WEAK
    if (libxml_is_threaded == 0)
        return (1);
#endif
    return (pthread_equal(mainthread, pthread_self()));
#elif defined HAVE_WIN32_THREADS
    return (mainthread == GetCurrentThreadId());
#else
    return (1);
#endif
}

/**
 * xmlIsMainThread:
 *
 * DEPRECATED: Internal function, do not use.
 *
 * Check whether the current thread is the main thread.
 *
 * Returns 1 if the current thread is the main thread, 0 otherwise
 */
int
xmlIsMainThread(void) {
    return(xmlIsMainThreadInternal());
}

#ifdef LIBXML_THREAD_ENABLED

static void
xmlFreeGlobalState(void *state)
{
    xmlGlobalState *gs = (xmlGlobalState *) state;

    /*
     * Free any memory allocated in the thread's xmlLastError. If it
     * weren't for this indirect allocation, we wouldn't need
     * a destructor with thread-local storage at all!
     *
     * It would be nice if we could make xmlLastError a special error
     * type which uses statically allocated, fixed-size buffers.
     * But the xmlError struct is fully public and widely used,
     * so changes are dangerous.
     */
    xmlResetError(&(gs->gs_xmlLastError));
#ifndef USE_TLS
    free(state);
#endif
}

#if defined(HAVE_WIN32_THREADS) && \
    defined(LIBXML_STATIC) && !defined(LIBXML_STATIC_FOR_DLL)
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
#endif /* LIBXML_STATIC */

static void
xmlInitGlobalState(xmlGlobalStatePtr gs) {
    xmlMutexLock(&xmlThrDefMutex);

#if defined(LIBXML_HTML_ENABLED) && defined(LIBXML_LEGACY_ENABLED) && defined(LIBXML_SAX1_ENABLED)
    inithtmlDefaultSAXHandler(&gs->gs_htmlDefaultSAXHandler);
#endif

    gs->gs_oldXMLWDcompatibility = 0;
    gs->gs_xmlBufferAllocScheme = xmlBufferAllocSchemeThrDef;
    gs->gs_xmlDefaultBufferSize = xmlDefaultBufferSizeThrDef;
#if defined(LIBXML_SAX1_ENABLED) && defined(LIBXML_LEGACY_ENABLED)
    initxmlDefaultSAXHandler(&gs->gs_xmlDefaultSAXHandler, 1);
#endif /* LIBXML_SAX1_ENABLED */
    gs->gs_xmlDefaultSAXLocator.getPublicId = xmlSAX2GetPublicId;
    gs->gs_xmlDefaultSAXLocator.getSystemId = xmlSAX2GetSystemId;
    gs->gs_xmlDefaultSAXLocator.getLineNumber = xmlSAX2GetLineNumber;
    gs->gs_xmlDefaultSAXLocator.getColumnNumber = xmlSAX2GetColumnNumber;
    gs->gs_xmlDoValidityCheckingDefaultValue =
         xmlDoValidityCheckingDefaultValueThrDef;
#ifdef LIBXML_THREAD_ALLOC_ENABLED
#ifdef DEBUG_MEMORY_LOCATION
    gs->gs_xmlFree = xmlMemFree;
    gs->gs_xmlMalloc = xmlMemMalloc;
    gs->gs_xmlMallocAtomic = xmlMemMalloc;
    gs->gs_xmlRealloc = xmlMemRealloc;
    gs->gs_xmlMemStrdup = xmlMemoryStrdup;
#else
    gs->gs_xmlFree = free;
    gs->gs_xmlMalloc = malloc;
    gs->gs_xmlMallocAtomic = malloc;
    gs->gs_xmlRealloc = realloc;
    gs->gs_xmlMemStrdup = xmlPosixStrdup;
#endif
#endif
    gs->gs_xmlGetWarningsDefaultValue = xmlGetWarningsDefaultValueThrDef;
#ifdef LIBXML_OUTPUT_ENABLED
    gs->gs_xmlIndentTreeOutput = xmlIndentTreeOutputThrDef;
    gs->gs_xmlTreeIndentString = xmlTreeIndentStringThrDef;
    gs->gs_xmlSaveNoEmptyTags = xmlSaveNoEmptyTagsThrDef;
#endif
    gs->gs_xmlKeepBlanksDefaultValue = xmlKeepBlanksDefaultValueThrDef;
    gs->gs_xmlLineNumbersDefaultValue = xmlLineNumbersDefaultValueThrDef;
    gs->gs_xmlLoadExtDtdDefaultValue = xmlLoadExtDtdDefaultValueThrDef;
    gs->gs_xmlParserDebugEntities = xmlParserDebugEntitiesThrDef;
    gs->gs_xmlPedanticParserDefaultValue = xmlPedanticParserDefaultValueThrDef;
    gs->gs_xmlSubstituteEntitiesDefaultValue =
        xmlSubstituteEntitiesDefaultValueThrDef;

    gs->gs_xmlGenericError = xmlGenericErrorThrDef;
    gs->gs_xmlStructuredError = xmlStructuredErrorThrDef;
    gs->gs_xmlGenericErrorContext = xmlGenericErrorContextThrDef;
    gs->gs_xmlStructuredErrorContext = xmlStructuredErrorContextThrDef;
    gs->gs_xmlRegisterNodeDefaultValue = xmlRegisterNodeDefaultValueThrDef;
    gs->gs_xmlDeregisterNodeDefaultValue = xmlDeregisterNodeDefaultValueThrDef;

    gs->gs_xmlParserInputBufferCreateFilenameValue =
        xmlParserInputBufferCreateFilenameValueThrDef;
    gs->gs_xmlOutputBufferCreateFilenameValue =
        xmlOutputBufferCreateFilenameValueThrDef;
    memset(&gs->gs_xmlLastError, 0, sizeof(xmlError));

    xmlMutexUnlock(&xmlThrDefMutex);

#ifdef HAVE_POSIX_THREADS
    pthread_setspecific(globalkey, gs);
#elif defined HAVE_WIN32_THREADS
#ifndef USE_TLS
    TlsSetValue(globalkey, gs);
#endif
#if defined(LIBXML_STATIC) && !defined(LIBXML_STATIC_FOR_DLL)
    xmlRegisterGlobalStateDtor(gs);
#endif
#endif

    gs->initialized = 1;
}

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

    gs = malloc(sizeof(xmlGlobalState));
    if (gs == NULL) {
        if (allowFailure)
            return(NULL);

        /*
         * If an application didn't call xmlCheckThreadLocalStorage to make
         * sure that global state could be allocated, it's too late to
         * handle the error.
         */
        fprintf(stderr, "libxml2: Failed to allocate globals for thread\n"
                        "libxml2: See xmlCheckThreadLocalStorage\n");
        abort();
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

/* Define thread-local storage accessors with macro magic */

#define XML_DEFINE_GLOBAL_WRAPPER(name, type, attrs) \
    type *__##name(void) { \
        if (IS_MAIN_THREAD) \
            return (&name); \
        else \
            return (&xmlGetThreadLocalStorage(0)->gs_##name); \
    }

#define XML_OP XML_DEFINE_GLOBAL_WRAPPER
XML_GLOBALS_ALLOC
XML_GLOBALS_ERROR
XML_GLOBALS_HTML
XML_GLOBALS_IO
XML_GLOBALS_PARSER
XML_GLOBALS_SAVE
XML_GLOBALS_TREE
#undef XML_OP

/* For backward compatibility */

const char *const *
__xmlParserVersion(void) {
    return &xmlParserVersion;
}

#endif /* LIBXML_THREAD_ENABLED */

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
 * This function never fails if the library is compiled with support
 * for thread-local storage.
 *
 * This function never fails for the "main" thread which is the first
 * thread calling xmlInitParser.
 *
 * Available since v2.12.0.
 */
int
xmlCheckThreadLocalStorage(void) {
#if defined(LIBXML_THREAD_ENABLED) && !defined(USE_TLS)
    if ((!xmlIsMainThreadInternal()) && (xmlGetThreadLocalStorage(1) == NULL))
        return(-1);
#endif
    return(0);
}

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
#if defined(HAVE_WIN32_THREADS) && \
    (!defined(LIBXML_STATIC) || defined(LIBXML_STATIC_FOR_DLL))
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
    }
    return TRUE;
}
#endif

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

xmlBufferAllocationScheme xmlThrDefBufferAllocScheme(xmlBufferAllocationScheme v) {
    xmlBufferAllocationScheme ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlBufferAllocSchemeThrDef;
    xmlBufferAllocSchemeThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
}

int xmlThrDefDefaultBufferSize(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlDefaultBufferSizeThrDef;
    xmlDefaultBufferSizeThrDef = v;
    xmlMutexUnlock(&xmlThrDefMutex);
    return ret;
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

int xmlThrDefParserDebugEntities(int v) {
    int ret;
    xmlMutexLock(&xmlThrDefMutex);
    ret = xmlParserDebugEntitiesThrDef;
    xmlParserDebugEntitiesThrDef = v;
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

    __xmlRegisterCallbacks = 1;
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

    __xmlRegisterCallbacks = 1;
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

