/*
 * extensions.c: Implemetation of the extensions support
 *
 * Reference:
 *   http://www.w3.org/TR/1999/REC-xslt-19991116
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXSLT
#include "libxslt.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/hash.h>
#include <libxml/xmlerror.h>
#include <libxml/parserInternals.h>
#include <libxml/xpathInternals.h>
#ifdef WITH_MODULES
#include <libxml/xmlmodule.h>
#endif
#include <libxml/list.h>
#include <libxml/xmlIO.h>
#include <libxml/threads.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltlocale.h"
#include "xsltutils.h"
#include "imports.h"
#include "extensions.h"

#ifdef _WIN32
#include <stdlib.h>             /* for _MAX_PATH */
#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif
#endif

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_EXTENSIONS
#endif

/************************************************************************
 *									*
 *			Private Types and Globals			*
 *									*
 ************************************************************************/

typedef struct _xsltExtDef xsltExtDef;
typedef xsltExtDef *xsltExtDefPtr;
struct _xsltExtDef {
    struct _xsltExtDef *next;
    xmlChar *prefix;
    xmlChar *URI;
    void *data;
};

typedef struct _xsltExtModule xsltExtModule;
typedef xsltExtModule *xsltExtModulePtr;
struct _xsltExtModule {
    xsltExtInitFunction initFunc;
    xsltExtShutdownFunction shutdownFunc;
    xsltStyleExtInitFunction styleInitFunc;
    xsltStyleExtShutdownFunction styleShutdownFunc;
};

typedef struct _xsltExtData xsltExtData;
typedef xsltExtData *xsltExtDataPtr;
struct _xsltExtData {
    xsltExtModulePtr extModule;
    void *extData;
};

typedef struct _xsltExtElement xsltExtElement;
typedef xsltExtElement *xsltExtElementPtr;
struct _xsltExtElement {
    xsltPreComputeFunction precomp;
    xsltTransformFunction transform;
};

static xmlHashTablePtr xsltExtensionsHash = NULL;
static xmlHashTablePtr xsltFunctionsHash = NULL;
static xmlHashTablePtr xsltElementsHash = NULL;
static xmlHashTablePtr xsltTopLevelsHash = NULL;
static xmlHashTablePtr xsltModuleHash = NULL;
static xmlMutexPtr xsltExtMutex = NULL;

/************************************************************************
 *									*
 *			Type functions					*
 *									*
 ************************************************************************/

/**
 * xsltNewExtDef:
 * @prefix:  the extension prefix
 * @URI:  the namespace URI
 *
 * Create a new XSLT ExtDef
 *
 * Returns the newly allocated xsltExtDefPtr or NULL in case of error
 */
static xsltExtDefPtr
xsltNewExtDef(const xmlChar * prefix, const xmlChar * URI)
{
    xsltExtDefPtr cur;

    cur = (xsltExtDefPtr) xmlMalloc(sizeof(xsltExtDef));
    if (cur == NULL) {
        xsltTransformError(NULL, NULL, NULL,
                           "xsltNewExtDef : malloc failed\n");
        return (NULL);
    }
    memset(cur, 0, sizeof(xsltExtDef));
    if (prefix != NULL)
        cur->prefix = xmlStrdup(prefix);
    if (URI != NULL)
        cur->URI = xmlStrdup(URI);
    return (cur);
}

/**
 * xsltFreeExtDef:
 * @extensiond:  an XSLT extension definition
 *
 * Free up the memory allocated by @extensiond
 */
static void
xsltFreeExtDef(xsltExtDefPtr extensiond)
{
    if (extensiond == NULL)
        return;
    if (extensiond->prefix != NULL)
        xmlFree(extensiond->prefix);
    if (extensiond->URI != NULL)
        xmlFree(extensiond->URI);
    xmlFree(extensiond);
}

/**
 * xsltFreeExtDefList:
 * @extensiond:  an XSLT extension definition list
 *
 * Free up the memory allocated by all the elements of @extensiond
 */
static void
xsltFreeExtDefList(xsltExtDefPtr extensiond)
{
    xsltExtDefPtr cur;

    while (extensiond != NULL) {
        cur = extensiond;
        extensiond = extensiond->next;
        xsltFreeExtDef(cur);
    }
}

/**
 * xsltNewExtModule:
 * @initFunc:  the module initialization function
 * @shutdownFunc:  the module shutdown function
 * @styleInitFunc:  the stylesheet module data allocator function
 * @styleShutdownFunc:  the stylesheet module data free function
 *
 * Create a new XSLT extension module
 *
 * Returns the newly allocated xsltExtModulePtr or NULL in case of error
 */
static xsltExtModulePtr
xsltNewExtModule(xsltExtInitFunction initFunc,
                 xsltExtShutdownFunction shutdownFunc,
                 xsltStyleExtInitFunction styleInitFunc,
                 xsltStyleExtShutdownFunction styleShutdownFunc)
{
    xsltExtModulePtr cur;

    cur = (xsltExtModulePtr) xmlMalloc(sizeof(xsltExtModule));
    if (cur == NULL) {
        xsltTransformError(NULL, NULL, NULL,
                           "xsltNewExtModule : malloc failed\n");
        return (NULL);
    }
    cur->initFunc = initFunc;
    cur->shutdownFunc = shutdownFunc;
    cur->styleInitFunc = styleInitFunc;
    cur->styleShutdownFunc = styleShutdownFunc;
    return (cur);
}

/**
 * xsltFreeExtModule:
 * @ext:  an XSLT extension module
 *
 * Free up the memory allocated by @ext
 */
static void
xsltFreeExtModule(xsltExtModulePtr ext)
{
    if (ext == NULL)
        return;
    xmlFree(ext);
}

static void
xsltFreeExtModuleEntry(void *payload, const xmlChar *name ATTRIBUTE_UNUSED) {
    xsltFreeExtModule((xsltExtModulePtr) payload);
}

/**
 * xsltNewExtData:
 * @extModule:  the module
 * @extData:  the associated data
 *
 * Create a new XSLT extension module data wrapper
 *
 * Returns the newly allocated xsltExtDataPtr or NULL in case of error
 */
static xsltExtDataPtr
xsltNewExtData(xsltExtModulePtr extModule, void *extData)
{
    xsltExtDataPtr cur;

    if (extModule == NULL)
        return (NULL);
    cur = (xsltExtDataPtr) xmlMalloc(sizeof(xsltExtData));
    if (cur == NULL) {
        xsltTransformError(NULL, NULL, NULL,
                           "xsltNewExtData : malloc failed\n");
        return (NULL);
    }
    cur->extModule = extModule;
    cur->extData = extData;
    return (cur);
}

/**
 * xsltFreeExtData:
 * @ext:  an XSLT extension module data wrapper
 *
 * Free up the memory allocated by @ext
 */
static void
xsltFreeExtData(xsltExtDataPtr ext)
{
    if (ext == NULL)
        return;
    xmlFree(ext);
}

static void
xsltFreeExtDataEntry(void *payload, const xmlChar *name ATTRIBUTE_UNUSED) {
    xsltFreeExtData((xsltExtDataPtr) payload);
}

/**
 * xsltNewExtElement:
 * @precomp:  the pre-computation function
 * @transform:  the transformation function
 *
 * Create a new XSLT extension element
 *
 * Returns the newly allocated xsltExtElementPtr or NULL in case of
 * error
 */
static xsltExtElementPtr
xsltNewExtElement(xsltPreComputeFunction precomp,
                  xsltTransformFunction transform)
{
    xsltExtElementPtr cur;

    if (transform == NULL)
        return (NULL);

    cur = (xsltExtElementPtr) xmlMalloc(sizeof(xsltExtElement));
    if (cur == NULL) {
        xsltTransformError(NULL, NULL, NULL,
                           "xsltNewExtElement : malloc failed\n");
        return (NULL);
    }
    cur->precomp = precomp;
    cur->transform = transform;
    return (cur);
}

/**
 * xsltFreeExtElement:
 * @ext: an XSLT extension element
 *
 * Frees up the memory allocated by @ext
 */
static void
xsltFreeExtElement(xsltExtElementPtr ext)
{
    if (ext == NULL)
        return;
    xmlFree(ext);
}

static void
xsltFreeExtElementEntry(void *payload, const xmlChar *name ATTRIBUTE_UNUSED) {
    xsltFreeExtElement((xsltExtElementPtr) payload);
}


#ifdef WITH_MODULES
typedef void (*exsltRegisterFunction) (void);

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/**
 * xsltExtModuleRegisterDynamic:
 * @URI:  the function or element namespace URI
 *
 * Dynamically loads an extension plugin when available.
 *
 * The plugin name is derived from the URI by removing the
 * initial protocol designation, e.g. "http://", then converting
 * the characters ".", "-", "/", and "\" into "_", the removing
 * any trailing "/", then concatenating LIBXML_MODULE_EXTENSION.
 *
 * Plugins are loaded from the directory specified by the
 * environment variable LIBXSLT_PLUGINS_PATH, or if NULL,
 * by LIBXSLT_DEFAULT_PLUGINS_PATH() which is determined at
 * compile time.
 *
 * Returns 0 if successful, -1 in case of error.
 */

static int
xsltExtModuleRegisterDynamic(const xmlChar * URI)
{

    xmlModulePtr m;
    exsltRegisterFunction regfunc;
    xmlChar *ext_name;
    char module_filename[PATH_MAX];
    const xmlChar *ext_directory = NULL;
    const xmlChar *protocol = NULL;
    xmlChar *i, *regfunc_name;
    void *vregfunc;
    int rc;

    /* check for bad inputs */
    if (URI == NULL)
        return (-1);

    if (NULL == xsltModuleHash) {
        xsltModuleHash = xmlHashCreate(5);
        if (xsltModuleHash == NULL)
            return (-1);
    }

    xmlMutexLock(xsltExtMutex);

    /* have we attempted to register this module already? */
    if (xmlHashLookup(xsltModuleHash, URI) != NULL) {
        xmlMutexUnlock(xsltExtMutex);
        return (-1);
    }
    xmlMutexUnlock(xsltExtMutex);

    /* transform extension namespace into a module name */
    protocol = xmlStrstr(URI, BAD_CAST "://");
    if (protocol == NULL) {
        ext_name = xmlStrdup(URI);
    } else {
        ext_name = xmlStrdup(protocol + 3);
    }
    if (ext_name == NULL) {
        return (-1);
    }

    i = ext_name;
    while ('\0' != *i) {
        if (('/' == *i) || ('\\' == *i) || ('.' == *i) || ('-' == *i))
            *i = '_';
        i++;
    }

    /* Strip underscores from end of string. */
    while (i > ext_name && *(i - 1) == '_') {
        i--;
        *i = '\0';
    }

    /* determine module directory */
    ext_directory = (xmlChar *) getenv("LIBXSLT_PLUGINS_PATH");

    if (NULL == ext_directory) {
        ext_directory = BAD_CAST LIBXSLT_DEFAULT_PLUGINS_PATH();
	if (NULL == ext_directory)
	  return (-1);
    }
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    else
      xsltGenericDebug(xsltGenericDebugContext,
		       "LIBXSLT_PLUGINS_PATH is %s\n", ext_directory);
#endif

    /* build the module filename, and confirm the module exists */
    xmlStrPrintf((xmlChar *) module_filename, sizeof(module_filename),
                 "%s/%s%s", ext_directory, ext_name, LIBXML_MODULE_EXTENSION);

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext,
                     "Attempting to load plugin: %s for URI: %s\n",
                     module_filename, URI);
#endif

#if LIBXML_VERSION < 21300
    if (1 != xmlCheckFilename(module_filename)) {

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	xsltGenericDebug(xsltGenericDebugContext,
                     "xmlCheckFilename failed for plugin: %s\n", module_filename);
#endif

        xmlFree(ext_name);
        return (-1);
    }
#endif

    /* attempt to open the module */
    m = xmlModuleOpen(module_filename, 0);
    if (NULL == m) {

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	xsltGenericDebug(xsltGenericDebugContext,
                     "xmlModuleOpen failed for plugin: %s\n", module_filename);
#endif

        xmlFree(ext_name);
        return (-1);
    }

    /* construct initialization func name */
    regfunc_name = xmlStrdup(ext_name);
    regfunc_name = xmlStrcat(regfunc_name, BAD_CAST "_init");

    vregfunc = NULL;
    rc = xmlModuleSymbol(m, (const char *) regfunc_name, &vregfunc);
    regfunc = vregfunc;
    if (0 == rc) {
        /*
	 * Call the module's init function.  Note that this function
	 * calls xsltRegisterExtModuleFull which will add the module
	 * to xsltExtensionsHash (together with it's entry points).
	 */
        (*regfunc) ();

        /* register this module in our hash */
        xmlMutexLock(xsltExtMutex);
        xmlHashAddEntry(xsltModuleHash, URI, (void *) m);
        xmlMutexUnlock(xsltExtMutex);
    } else {

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	xsltGenericDebug(xsltGenericDebugContext,
                     "xmlModuleSymbol failed for plugin: %s, regfunc: %s\n",
                     module_filename, regfunc_name);
#endif

        /* if regfunc not found unload the module immediately */
        xmlModuleClose(m);
    }

    xmlFree(ext_name);
    xmlFree(regfunc_name);
    return (NULL == regfunc) ? -1 : 0;
}
#else
static int
xsltExtModuleRegisterDynamic(const xmlChar * URI ATTRIBUTE_UNUSED)
{
  return -1;
}
#endif

/************************************************************************
 *									*
 *		The stylesheet extension prefixes handling		*
 *									*
 ************************************************************************/


/**
 * xsltFreeExts:
 * @style: an XSLT stylesheet
 *
 * Free up the memory used by XSLT extensions in a stylesheet
 */
void
xsltFreeExts(xsltStylesheetPtr style)
{
    if (style->nsDefs != NULL)
        xsltFreeExtDefList((xsltExtDefPtr) style->nsDefs);
}

/**
 * xsltRegisterExtPrefix:
 * @style: an XSLT stylesheet
 * @prefix: the prefix used (optional)
 * @URI: the URI associated to the extension
 *
 * Registers an extension namespace
 * This is called from xslt.c during compile-time.
 * The given prefix is not needed.
 * Called by:
 *   xsltParseExtElemPrefixes() (new function)
 *   xsltRegisterExtPrefix() (old function)
 *
 * Returns 0 in case of success, 1 if the @URI was already
 *         registered as an extension namespace and
 *         -1 in case of failure
 */
int
xsltRegisterExtPrefix(xsltStylesheetPtr style,
                      const xmlChar * prefix, const xmlChar * URI)
{
    xsltExtDefPtr def, ret;

    if ((style == NULL) || (URI == NULL))
        return (-1);

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext,
	"Registering extension namespace '%s'.\n", URI);
#endif
    def = (xsltExtDefPtr) style->nsDefs;
#ifdef XSLT_REFACTORED
    /*
    * The extension is associated with a namespace name.
    */
    while (def != NULL) {
        if (xmlStrEqual(URI, def->URI))
            return (1);
        def = def->next;
    }
#else
    while (def != NULL) {
        if (xmlStrEqual(prefix, def->prefix))
            return (-1);
        def = def->next;
    }
#endif
    ret = xsltNewExtDef(prefix, URI);
    if (ret == NULL)
        return (-1);
    ret->next = (xsltExtDefPtr) style->nsDefs;
    style->nsDefs = ret;

    /*
     * check whether there is an extension module with a stylesheet
     * initialization function.
     */
#ifdef XSLT_REFACTORED
    /*
    * Don't initialize modules based on specified namespaces via
    * the attribute "[xsl:]extension-element-prefixes".
    */
#else
    if (xsltExtensionsHash != NULL) {
        xsltExtModulePtr module;

        xmlMutexLock(xsltExtMutex);
        module = xmlHashLookup(xsltExtensionsHash, URI);
        xmlMutexUnlock(xsltExtMutex);
        if (NULL == module) {
            if (!xsltExtModuleRegisterDynamic(URI)) {
                xmlMutexLock(xsltExtMutex);
                module = xmlHashLookup(xsltExtensionsHash, URI);
                xmlMutexUnlock(xsltExtMutex);
            }
        }
        if (module != NULL) {
            xsltStyleGetExtData(style, URI);
        }
    }
#endif
    return (0);
}

/************************************************************************
 *									*
 *		The extensions modules interfaces			*
 *									*
 ************************************************************************/

/**
 * xsltRegisterExtFunction:
 * @ctxt: an XSLT transformation context
 * @name: the name of the element
 * @URI: the URI associated to the element
 * @function: the actual implementation which should be called
 *
 * Registers an extension function
 *
 * Returns 0 in case of success, -1 in case of failure
 */
int
xsltRegisterExtFunction(xsltTransformContextPtr ctxt, const xmlChar * name,
                        const xmlChar * URI, xmlXPathFunction function)
{
    int ret;

    if ((ctxt == NULL) || (name == NULL) ||
        (URI == NULL) || (function == NULL))
        return (-1);
    if (ctxt->xpathCtxt != NULL) {
        xmlXPathRegisterFuncNS(ctxt->xpathCtxt, name, URI, function);
    }
    if (ctxt->extFunctions == NULL)
        ctxt->extFunctions = xmlHashCreate(10);
    if (ctxt->extFunctions == NULL)
        return (-1);

    ret = xmlHashAddEntry2(ctxt->extFunctions, name, URI,
                           XML_CAST_FPTR(function));

    return(ret);
}

/**
 * xsltRegisterExtElement:
 * @ctxt: an XSLT transformation context
 * @name: the name of the element
 * @URI: the URI associated to the element
 * @function: the actual implementation which should be called
 *
 * Registers an extension element
 *
 * Returns 0 in case of success, -1 in case of failure
 */
int
xsltRegisterExtElement(xsltTransformContextPtr ctxt, const xmlChar * name,
                       const xmlChar * URI, xsltTransformFunction function)
{
    if ((ctxt == NULL) || (name == NULL) ||
        (URI == NULL) || (function == NULL))
        return (-1);
    if (ctxt->extElements == NULL)
        ctxt->extElements = xmlHashCreate(10);
    if (ctxt->extElements == NULL)
        return (-1);
    return (xmlHashAddEntry2
            (ctxt->extElements, name, URI, XML_CAST_FPTR(function)));
}

/**
 * xsltFreeCtxtExts:
 * @ctxt: an XSLT transformation context
 *
 * Free the XSLT extension data
 */
void
xsltFreeCtxtExts(xsltTransformContextPtr ctxt)
{
    if (ctxt->extElements != NULL)
        xmlHashFree(ctxt->extElements, NULL);
    if (ctxt->extFunctions != NULL)
        xmlHashFree(ctxt->extFunctions, NULL);
}

/**
 * xsltStyleGetStylesheetExtData:
 * @style: an XSLT stylesheet
 * @URI:  the URI associated to the exension module
 *
 * Fires the compile-time initialization callback
 * of an extension module and returns a container
 * holding the user-data (retrieved via the callback).
 *
 * Returns the create module-data container
 *         or NULL if such a module was not registered.
 */
static xsltExtDataPtr
xsltStyleInitializeStylesheetModule(xsltStylesheetPtr style,
				     const xmlChar * URI)
{
    xsltExtDataPtr dataContainer;
    void *userData = NULL;
    xsltExtModulePtr module;

    if ((style == NULL) || (URI == NULL))
	return(NULL);

    if (xsltExtensionsHash == NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	xsltGenericDebug(xsltGenericDebugContext,
	    "Not registered extension module: %s\n", URI);
#endif
	return(NULL);
    }

    xmlMutexLock(xsltExtMutex);

    module = xmlHashLookup(xsltExtensionsHash, URI);

    xmlMutexUnlock(xsltExtMutex);

    if (module == NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	xsltGenericDebug(xsltGenericDebugContext,
	    "Not registered extension module: %s\n", URI);
#endif
	return (NULL);
    }
    /*
    * The specified module was registered so initialize it.
    */
    if (style->extInfos == NULL) {
	style->extInfos = xmlHashCreate(10);
	if (style->extInfos == NULL)
	    return (NULL);
    }
    /*
    * Fire the initialization callback if available.
    */
    if (module->styleInitFunc == NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	xsltGenericDebug(xsltGenericDebugContext,
	    "Initializing module with *no* callback: %s\n", URI);
#endif
    } else {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	xsltGenericDebug(xsltGenericDebugContext,
	    "Initializing module with callback: %s\n", URI);
#endif
	/*
	* Fire the initialization callback.
	*/
	userData = module->styleInitFunc(style, URI);
    }
    /*
    * Store the user-data in the context of the given stylesheet.
    */
    dataContainer = xsltNewExtData(module, userData);
    if (dataContainer == NULL) {
	if (module->styleShutdownFunc)
	    module->styleShutdownFunc(style, URI, userData);
	return (NULL);
    }

    if (xmlHashAddEntry(style->extInfos, URI,
	(void *) dataContainer) < 0)
    {
	xsltTransformError(NULL, style, NULL,
	    "Failed to register module '%s'.\n", URI);
	style->errors++;
	if (module->styleShutdownFunc)
	    module->styleShutdownFunc(style, URI, userData);
	xsltFreeExtData(dataContainer);
	return (NULL);
    }

    return(dataContainer);
}

/**
 * xsltStyleGetExtData:
 * @style: an XSLT stylesheet
 * @URI:  the URI associated to the exension module
 *
 * Retrieve the data associated to the extension module
 * in this given stylesheet.
 * Called by:
 *   xsltRegisterExtPrefix(),
 *   ( xsltExtElementPreCompTest(), xsltExtInitTest )
 *
 * Returns the pointer or NULL if not present
 */
void *
xsltStyleGetExtData(xsltStylesheetPtr style, const xmlChar * URI)
{
    xsltExtDataPtr dataContainer = NULL;
    xsltStylesheetPtr tmpStyle;

    if ((style == NULL) || (URI == NULL) ||
	(xsltExtensionsHash == NULL))
	return (NULL);


#ifdef XSLT_REFACTORED
    /*
    * This is intended for global storage, so only the main
    * stylesheet will hold the data.
    */
    tmpStyle = style;
    while (tmpStyle->parent != NULL)
	tmpStyle = tmpStyle->parent;
    if (tmpStyle->extInfos != NULL) {
	dataContainer =
	    (xsltExtDataPtr) xmlHashLookup(tmpStyle->extInfos, URI);
	if (dataContainer != NULL) {
	    /*
	    * The module was already initialized in the context
	    * of this stylesheet; just return the user-data that
	    * comes with it.
	    */
	    return(dataContainer->extData);
	}
    }
#else
    /*
    * Old behaviour.
    */
    tmpStyle = style;
    if (tmpStyle->extInfos != NULL) {
        dataContainer =
            (xsltExtDataPtr) xmlHashLookup(tmpStyle->extInfos, URI);
        if (dataContainer != NULL) {
            return(dataContainer->extData);
        }
    }
#endif

    dataContainer =
        xsltStyleInitializeStylesheetModule(tmpStyle, URI);
    if (dataContainer != NULL)
	return (dataContainer->extData);
    return(NULL);
}

#ifdef XSLT_REFACTORED
/**
 * xsltStyleStylesheetLevelGetExtData:
 * @style: an XSLT stylesheet
 * @URI:  the URI associated to the exension module
 *
 * Retrieve the data associated to the extension module in this given
 * stylesheet.
 *
 * Returns the pointer or NULL if not present
 */
void *
xsltStyleStylesheetLevelGetExtData(xsltStylesheetPtr style,
				   const xmlChar * URI)
{
    xsltExtDataPtr dataContainer = NULL;

    if ((style == NULL) || (URI == NULL) ||
	(xsltExtensionsHash == NULL))
	return (NULL);

    if (style->extInfos != NULL) {
	dataContainer = (xsltExtDataPtr) xmlHashLookup(style->extInfos, URI);
	/*
	* The module was already initialized in the context
	* of this stylesheet; just return the user-data that
	* comes with it.
	*/
	if (dataContainer)
	    return(dataContainer->extData);
    }

    dataContainer =
        xsltStyleInitializeStylesheetModule(style, URI);
    if (dataContainer != NULL)
	return (dataContainer->extData);
    return(NULL);
}
#endif

/**
 * xsltGetExtData:
 * @ctxt: an XSLT transformation context
 * @URI:  the URI associated to the exension module
 *
 * Retrieve the data associated to the extension module in this given
 * transformation.
 *
 * Returns the pointer or NULL if not present
 */
void *
xsltGetExtData(xsltTransformContextPtr ctxt, const xmlChar * URI)
{
    xsltExtDataPtr data;

    if ((ctxt == NULL) || (URI == NULL))
        return (NULL);
    if (ctxt->extInfos == NULL) {
        ctxt->extInfos = xmlHashCreate(10);
        if (ctxt->extInfos == NULL)
            return (NULL);
        data = NULL;
    } else {
        data = (xsltExtDataPtr) xmlHashLookup(ctxt->extInfos, URI);
    }
    if (data == NULL) {
        void *extData;
        xsltExtModulePtr module;

        xmlMutexLock(xsltExtMutex);

        module = xmlHashLookup(xsltExtensionsHash, URI);

        xmlMutexUnlock(xsltExtMutex);

        if (module == NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
            xsltGenericDebug(xsltGenericDebugContext,
                             "Not registered extension module: %s\n", URI);
#endif
            return (NULL);
        } else {
            if (module->initFunc == NULL)
                return (NULL);

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
            xsltGenericDebug(xsltGenericDebugContext,
                             "Initializing module: %s\n", URI);
#endif

            extData = module->initFunc(ctxt, URI);
            if (extData == NULL)
                return (NULL);

            data = xsltNewExtData(module, extData);
            if ((data == NULL) ||
                (xmlHashAddEntry(ctxt->extInfos, URI, (void *) data) < 0)) {
                xsltTransformError(ctxt, NULL, NULL,
                                   "Failed to register module data: %s\n",
                                   URI);
                if (module->shutdownFunc)
                    module->shutdownFunc(ctxt, URI, extData);
                xsltFreeExtData(data);
                return (NULL);
            }
        }
    }
    return (data->extData);
}

typedef struct _xsltInitExtCtxt xsltInitExtCtxt;
struct _xsltInitExtCtxt {
    xsltTransformContextPtr ctxt;
    int ret;
};

/**
 * xsltInitCtxtExt:
 * @styleData:  the registered stylesheet data for the module
 * @ctxt:  the XSLT transformation context + the return value
 * @URI:  the extension URI
 *
 * Initializes an extension module
 */
static void
xsltInitCtxtExt(void *payload, void *data, const xmlChar * URI)
{
    xsltExtDataPtr styleData = (xsltExtDataPtr) payload;
    xsltInitExtCtxt *ctxt = (xsltInitExtCtxt *) data;
    xsltExtModulePtr module;
    xsltExtDataPtr ctxtData;
    void *extData;

    if ((styleData == NULL) || (ctxt == NULL) || (URI == NULL) ||
        (ctxt->ret == -1)) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
        xsltGenericDebug(xsltGenericDebugContext,
                         "xsltInitCtxtExt: NULL param or error\n");
#endif
        return;
    }
    module = styleData->extModule;
    if ((module == NULL) || (module->initFunc == NULL)) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
        xsltGenericDebug(xsltGenericDebugContext,
                         "xsltInitCtxtExt: no module or no initFunc\n");
#endif
        return;
    }

    ctxtData = (xsltExtDataPtr) xmlHashLookup(ctxt->ctxt->extInfos, URI);
    if (ctxtData != NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
        xsltGenericDebug(xsltGenericDebugContext,
                         "xsltInitCtxtExt: already initialized\n");
#endif
        return;
    }

    extData = module->initFunc(ctxt->ctxt, URI);
    if (extData == NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
        xsltGenericDebug(xsltGenericDebugContext,
                         "xsltInitCtxtExt: no extData\n");
#endif
    }
    ctxtData = xsltNewExtData(module, extData);
    if (ctxtData == NULL) {
        if (module->shutdownFunc)
            module->shutdownFunc(ctxt->ctxt, URI, extData);
        ctxt->ret = -1;
        return;
    }

    if (ctxt->ctxt->extInfos == NULL)
        ctxt->ctxt->extInfos = xmlHashCreate(10);
    if (ctxt->ctxt->extInfos == NULL) {
        if (module->shutdownFunc)
            module->shutdownFunc(ctxt->ctxt, URI, extData);
        xsltFreeExtData(ctxtData);
        ctxt->ret = -1;
        return;
    }

    if (xmlHashAddEntry(ctxt->ctxt->extInfos, URI, ctxtData) < 0) {
        xsltGenericError(xsltGenericErrorContext,
                         "Failed to register module data: %s\n", URI);
        if (module->shutdownFunc)
            module->shutdownFunc(ctxt->ctxt, URI, extData);
        xsltFreeExtData(ctxtData);
        ctxt->ret = -1;
        return;
    }
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext, "Registered module %s\n",
                     URI);
#endif
    ctxt->ret++;
}

/**
 * xsltInitCtxtExts:
 * @ctxt: an XSLT transformation context
 *
 * Initialize the set of modules with registered stylesheet data
 *
 * Returns the number of modules initialized or -1 in case of error
 */
int
xsltInitCtxtExts(xsltTransformContextPtr ctxt)
{
    xsltStylesheetPtr style;
    xsltInitExtCtxt ctx;

    if (ctxt == NULL)
        return (-1);

    style = ctxt->style;
    if (style == NULL)
        return (-1);

    ctx.ctxt = ctxt;
    ctx.ret = 0;

    while (style != NULL) {
        if (style->extInfos != NULL) {
            xmlHashScan(style->extInfos, xsltInitCtxtExt, &ctx);
            if (ctx.ret == -1)
                return (-1);
        }
        style = xsltNextImport(style);
    }
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext, "Registered %d modules\n",
                     ctx.ret);
#endif
    return (ctx.ret);
}

/**
 * xsltShutdownCtxtExt:
 * @data:  the registered data for the module
 * @ctxt:  the XSLT transformation context
 * @URI:  the extension URI
 *
 * Shutdown an extension module loaded
 */
static void
xsltShutdownCtxtExt(void *payload, void *vctxt, const xmlChar * URI)
{
    xsltExtDataPtr data = (xsltExtDataPtr) payload;
    xsltTransformContextPtr ctxt = (xsltTransformContextPtr) vctxt;
    xsltExtModulePtr module;

    if ((data == NULL) || (ctxt == NULL) || (URI == NULL))
        return;
    module = data->extModule;
    if ((module == NULL) || (module->shutdownFunc == NULL))
        return;

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext,
                     "Shutting down module : %s\n", URI);
#endif
    module->shutdownFunc(ctxt, URI, data->extData);
}

/**
 * xsltShutdownCtxtExts:
 * @ctxt: an XSLT transformation context
 *
 * Shutdown the set of modules loaded
 */
void
xsltShutdownCtxtExts(xsltTransformContextPtr ctxt)
{
    if (ctxt == NULL)
        return;
    if (ctxt->extInfos == NULL)
        return;
    xmlHashScan(ctxt->extInfos, xsltShutdownCtxtExt, ctxt);
    xmlHashFree(ctxt->extInfos, xsltFreeExtDataEntry);
    ctxt->extInfos = NULL;
}

/**
 * xsltShutdownExt:
 * @data:  the registered data for the module
 * @ctxt:  the XSLT stylesheet
 * @URI:  the extension URI
 *
 * Shutdown an extension module loaded
 */
static void
xsltShutdownExt(void *payload, void *vctxt, const xmlChar * URI)
{
    xsltExtDataPtr data = (xsltExtDataPtr) payload;
    xsltStylesheetPtr style = (xsltStylesheetPtr) vctxt;
    xsltExtModulePtr module;

    if ((data == NULL) || (style == NULL) || (URI == NULL))
        return;
    module = data->extModule;
    if ((module == NULL) || (module->styleShutdownFunc == NULL))
        return;

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext,
                     "Shutting down module : %s\n", URI);
#endif
    module->styleShutdownFunc(style, URI, data->extData);
    /*
    * Don't remove the entry from the hash table here, since
    * this will produce segfaults - this fixes bug #340624.
    *
    * xmlHashRemoveEntry(style->extInfos, URI, xsltFreeExtDataEntry);
    */
}

/**
 * xsltShutdownExts:
 * @style: an XSLT stylesheet
 *
 * Shutdown the set of modules loaded
 */
void
xsltShutdownExts(xsltStylesheetPtr style)
{
    if (style == NULL)
        return;
    if (style->extInfos == NULL)
        return;
    xmlHashScan(style->extInfos, xsltShutdownExt, style);
    xmlHashFree(style->extInfos, xsltFreeExtDataEntry);
    style->extInfos = NULL;
}

/**
 * xsltCheckExtPrefix:
 * @style: the stylesheet
 * @URI: the namespace prefix (possibly NULL)
 *
 * Check if the given prefix is one of the declared extensions.
 * This is intended to be called only at compile-time.
 * Called by:
 *  xsltGetInheritedNsList() (xslt.c)
 *  xsltParseTemplateContent (xslt.c)
 *
 * Returns 1 if this is an extension, 0 otherwise
 */
int
xsltCheckExtPrefix(xsltStylesheetPtr style, const xmlChar * URI)
{
#ifdef XSLT_REFACTORED
    if ((style == NULL) || (style->compCtxt == NULL) ||
	(XSLT_CCTXT(style)->inode == NULL) ||
	(XSLT_CCTXT(style)->inode->extElemNs == NULL))
        return (0);
    /*
    * Lookup the extension namespaces registered
    * at the current node in the stylesheet's tree.
    */
    if (XSLT_CCTXT(style)->inode->extElemNs != NULL) {
	int i;
	xsltPointerListPtr list = XSLT_CCTXT(style)->inode->extElemNs;

	for (i = 0; i < list->number; i++) {
	    if (xmlStrEqual((const xmlChar *) list->items[i],
		URI))
	    {
		return(1);
	    }
	}
    }
#else
    xsltExtDefPtr cur;

    if ((style == NULL) || (style->nsDefs == NULL))
        return (0);
    if (URI == NULL)
        URI = BAD_CAST "#default";
    cur = (xsltExtDefPtr) style->nsDefs;
    while (cur != NULL) {
	/*
	* NOTE: This was change to work on namespace names rather
	* than namespace prefixes. This fixes bug #339583.
	* TODO: Consider renaming the field "prefix" of xsltExtDef
	*  to "href".
	*/
        if (xmlStrEqual(URI, cur->prefix))
            return (1);
        cur = cur->next;
    }
#endif
    return (0);
}

/**
 * xsltCheckExtURI:
 * @style: the stylesheet
 * @URI: the namespace URI (possibly NULL)
 *
 * Check if the given prefix is one of the declared extensions.
 * This is intended to be called only at compile-time.
 * Called by:
 *  xsltPrecomputeStylesheet() (xslt.c)
 *  xsltParseTemplateContent (xslt.c)
 *
 * Returns 1 if this is an extension, 0 otherwise
 */
int
xsltCheckExtURI(xsltStylesheetPtr style, const xmlChar * URI)
{
    xsltExtDefPtr cur;

    if ((style == NULL) || (style->nsDefs == NULL))
        return (0);
    if (URI == NULL)
        return (0);
    cur = (xsltExtDefPtr) style->nsDefs;
    while (cur != NULL) {
        if (xmlStrEqual(URI, cur->URI))
            return (1);
        cur = cur->next;
    }
    return (0);
}

/**
 * xsltRegisterExtModuleFull:
 * @URI:  URI associated to this module
 * @initFunc:  the module initialization function
 * @shutdownFunc:  the module shutdown function
 * @styleInitFunc:  the module initialization function
 * @styleShutdownFunc:  the module shutdown function
 *
 * Register an XSLT extension module to the library.
 *
 * Returns 0 if sucessful, -1 in case of error
 */
int
xsltRegisterExtModuleFull(const xmlChar * URI,
                          xsltExtInitFunction initFunc,
                          xsltExtShutdownFunction shutdownFunc,
                          xsltStyleExtInitFunction styleInitFunc,
                          xsltStyleExtShutdownFunction styleShutdownFunc)
{
    int ret;
    xsltExtModulePtr module;

    if ((URI == NULL) || (initFunc == NULL))
        return (-1);
    if (xsltExtensionsHash == NULL)
        xsltExtensionsHash = xmlHashCreate(10);

    if (xsltExtensionsHash == NULL)
        return (-1);

    xmlMutexLock(xsltExtMutex);

    module = xmlHashLookup(xsltExtensionsHash, URI);
    if (module != NULL) {
        if ((module->initFunc == initFunc) &&
            (module->shutdownFunc == shutdownFunc))
            ret = 0;
        else
            ret = -1;
        goto done;
    }
    module = xsltNewExtModule(initFunc, shutdownFunc,
                              styleInitFunc, styleShutdownFunc);
    if (module == NULL) {
        ret = -1;
        goto done;
    }
    ret = xmlHashAddEntry(xsltExtensionsHash, URI, (void *) module);

done:
    xmlMutexUnlock(xsltExtMutex);
    return (ret);
}

/**
 * xsltRegisterExtModule:
 * @URI:  URI associated to this module
 * @initFunc:  the module initialization function
 * @shutdownFunc:  the module shutdown function
 *
 * Register an XSLT extension module to the library.
 *
 * Returns 0 if sucessful, -1 in case of error
 */
int
xsltRegisterExtModule(const xmlChar * URI,
                      xsltExtInitFunction initFunc,
                      xsltExtShutdownFunction shutdownFunc)
{
    return xsltRegisterExtModuleFull(URI, initFunc, shutdownFunc,
                                     NULL, NULL);
}

/**
 * xsltUnregisterExtModule:
 * @URI:  URI associated to this module
 *
 * Unregister an XSLT extension module from the library.
 *
 * Returns 0 if sucessful, -1 in case of error
 */
int
xsltUnregisterExtModule(const xmlChar * URI)
{
    int ret;

    if (URI == NULL)
        return (-1);
    if (xsltExtensionsHash == NULL)
        return (-1);

    xmlMutexLock(xsltExtMutex);

    ret = xmlHashRemoveEntry(xsltExtensionsHash, URI, xsltFreeExtModuleEntry);

    xmlMutexUnlock(xsltExtMutex);

    return (ret);
}

/**
 * xsltUnregisterAllExtModules:
 *
 * Unregister all the XSLT extension module from the library.
 */
static void
xsltUnregisterAllExtModules(void)
{
    if (xsltExtensionsHash == NULL)
        return;

    xmlMutexLock(xsltExtMutex);

    xmlHashFree(xsltExtensionsHash, xsltFreeExtModuleEntry);
    xsltExtensionsHash = NULL;

    xmlMutexUnlock(xsltExtMutex);
}

/**
 * xsltXPathGetTransformContext:
 * @ctxt:  an XPath transformation context
 *
 * Provides the XSLT transformation context from the XPath transformation
 * context. This is useful when an XPath function in the extension module
 * is called by the XPath interpreter and that the XSLT context is needed
 * for example to retrieve the associated data pertaining to this XSLT
 * transformation.
 *
 * Returns the XSLT transformation context or NULL in case of error.
 */
xsltTransformContextPtr
xsltXPathGetTransformContext(xmlXPathParserContextPtr ctxt)
{
    if ((ctxt == NULL) || (ctxt->context == NULL))
        return (NULL);
    return (ctxt->context->extra);
}

/**
 * xsltRegisterExtModuleFunction:
 * @name:  the function name
 * @URI:  the function namespace URI
 * @function:  the function callback
 *
 * Registers an extension module function.
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltRegisterExtModuleFunction(const xmlChar * name, const xmlChar * URI,
                              xmlXPathFunction function)
{
    if ((name == NULL) || (URI == NULL) || (function == NULL))
        return (-1);

    if (xsltFunctionsHash == NULL)
        xsltFunctionsHash = xmlHashCreate(10);
    if (xsltFunctionsHash == NULL)
        return (-1);

    xmlMutexLock(xsltExtMutex);

    xmlHashUpdateEntry2(xsltFunctionsHash, name, URI,
                        XML_CAST_FPTR(function), NULL);

    xmlMutexUnlock(xsltExtMutex);

    return (0);
}

/**
 * xsltExtModuleFunctionLookup:
 * @name:  the function name
 * @URI:  the function namespace URI
 *
 * Looks up an extension module function
 *
 * Returns the function if found, NULL otherwise.
 */
xmlXPathFunction
xsltExtModuleFunctionLookup(const xmlChar * name, const xmlChar * URI)
{
    xmlXPathFunction ret;

    if ((xsltFunctionsHash == NULL) || (name == NULL) || (URI == NULL))
        return (NULL);

    xmlMutexLock(xsltExtMutex);

    XML_CAST_FPTR(ret) = xmlHashLookup2(xsltFunctionsHash, name, URI);

    xmlMutexUnlock(xsltExtMutex);

    /* if lookup fails, attempt a dynamic load on supported platforms */
    if (NULL == ret) {
        if (!xsltExtModuleRegisterDynamic(URI)) {
            xmlMutexLock(xsltExtMutex);

            XML_CAST_FPTR(ret) =
                xmlHashLookup2(xsltFunctionsHash, name, URI);

            xmlMutexUnlock(xsltExtMutex);
        }
    }

    return ret;
}

/**
 * xsltUnregisterExtModuleFunction:
 * @name:  the function name
 * @URI:  the function namespace URI
 *
 * Unregisters an extension module function
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltUnregisterExtModuleFunction(const xmlChar * name, const xmlChar * URI)
{
    int ret;

    if ((xsltFunctionsHash == NULL) || (name == NULL) || (URI == NULL))
        return (-1);

    xmlMutexLock(xsltExtMutex);

    ret = xmlHashRemoveEntry2(xsltFunctionsHash, name, URI, NULL);

    xmlMutexUnlock(xsltExtMutex);

    return(ret);
}

/**
 * xsltUnregisterAllExtModuleFunction:
 *
 * Unregisters all extension module function
 */
static void
xsltUnregisterAllExtModuleFunction(void)
{
    xmlMutexLock(xsltExtMutex);

    xmlHashFree(xsltFunctionsHash, NULL);
    xsltFunctionsHash = NULL;

    xmlMutexUnlock(xsltExtMutex);
}


static void
xsltFreeElemPreComp(xsltElemPreCompPtr comp) {
    xmlFree(comp);
}

/**
 * xsltNewElemPreComp:
 * @style:  the XSLT stylesheet
 * @inst:  the element node
 * @function: the transform function
 *
 * Creates and initializes an #xsltElemPreComp
 *
 * Returns the new and initialized #xsltElemPreComp
 */
xsltElemPreCompPtr
xsltNewElemPreComp(xsltStylesheetPtr style, xmlNodePtr inst,
                   xsltTransformFunction function)
{
    xsltElemPreCompPtr cur;

    cur = (xsltElemPreCompPtr) xmlMalloc(sizeof(xsltElemPreComp));
    if (cur == NULL) {
        xsltTransformError(NULL, style, NULL,
                           "xsltNewExtElement : malloc failed\n");
        return (NULL);
    }
    memset(cur, 0, sizeof(xsltElemPreComp));

    xsltInitElemPreComp(cur, style, inst, function, xsltFreeElemPreComp);

    return (cur);
}

/**
 * xsltInitElemPreComp:
 * @comp:  an #xsltElemPreComp (or generally a derived structure)
 * @style:  the XSLT stylesheet
 * @inst:  the element node
 * @function:  the transform function
 * @freeFunc:  the @comp deallocator
 *
 * Initializes an existing #xsltElemPreComp structure. This is usefull
 * when extending an #xsltElemPreComp to store precomputed data.
 * This function MUST be called on any extension element precomputed
 * data struct.
 */
void
xsltInitElemPreComp(xsltElemPreCompPtr comp, xsltStylesheetPtr style,
                    xmlNodePtr inst, xsltTransformFunction function,
                    xsltElemPreCompDeallocator freeFunc)
{
    comp->type = XSLT_FUNC_EXTENSION;
    comp->func = function;
    comp->inst = inst;
    comp->free = freeFunc;

    comp->next = style->preComps;
    style->preComps = comp;
}

/**
 * xsltPreComputeExtModuleElement:
 * @style:  the stylesheet
 * @inst:  the element node
 *
 * Precomputes an extension module element
 *
 * Returns the precomputed data
 */
xsltElemPreCompPtr
xsltPreComputeExtModuleElement(xsltStylesheetPtr style, xmlNodePtr inst)
{
    xsltExtElementPtr ext;
    xsltElemPreCompPtr comp = NULL;

    if ((style == NULL) || (inst == NULL) ||
        (inst->type != XML_ELEMENT_NODE) || (inst->ns == NULL))
        return (NULL);

    xmlMutexLock(xsltExtMutex);

    ext = (xsltExtElementPtr)
        xmlHashLookup2(xsltElementsHash, inst->name, inst->ns->href);

    xmlMutexUnlock(xsltExtMutex);

    /*
    * EXT TODO: Now what?
    */
    if (ext == NULL)
        return (NULL);

    if (ext->precomp != NULL) {
	/*
	* REVISIT TODO: Check if the text below is correct.
	* This will return a xsltElemPreComp structure or NULL.
	* 1) If the the author of the extension needs a
	*  custom structure to hold the specific values of
	*  this extension, he will derive a structure based on
	*  xsltElemPreComp; thus we obviously *cannot* refactor
	*  the xsltElemPreComp structure, since all already derived
	*  user-defined strucures will break.
	*  Example: For the extension xsl:document,
	*   in xsltDocumentComp() (preproc.c), the structure
	*   xsltStyleItemDocument is allocated, filled with
	*   specific values and returned.
	* 2) If the author needs no values to be stored in
	*  this structure, then he'll return NULL;
	*/
        comp = ext->precomp(style, inst, ext->transform);
    }
    if (comp == NULL) {
	/*
	* Default creation of a xsltElemPreComp structure, if
	* the author of this extension did not create a custom
	* structure.
	*/
        comp = xsltNewElemPreComp(style, inst, ext->transform);
    }

    return (comp);
}

/**
 * xsltRegisterExtModuleElement:
 * @name:  the element name
 * @URI:  the element namespace URI
 * @precomp:  the pre-computation callback
 * @transform:  the transformation callback
 *
 * Registers an extension module element.
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltRegisterExtModuleElement(const xmlChar * name, const xmlChar * URI,
                             xsltPreComputeFunction precomp,
                             xsltTransformFunction transform)
{
    int ret = 0;

    xsltExtElementPtr ext;

    if ((name == NULL) || (URI == NULL) || (transform == NULL))
        return (-1);

    if (xsltElementsHash == NULL)
        xsltElementsHash = xmlHashCreate(10);
    if (xsltElementsHash == NULL)
        return (-1);

    xmlMutexLock(xsltExtMutex);

    ext = xsltNewExtElement(precomp, transform);
    if (ext == NULL) {
        ret = -1;
        goto done;
    }

    xmlHashUpdateEntry2(xsltElementsHash, name, URI, (void *) ext,
                        xsltFreeExtElementEntry);

done:
    xmlMutexUnlock(xsltExtMutex);

    return (ret);
}

/**
 * xsltExtElementLookup:
 * @ctxt:  an XSLT process context
 * @name:  the element name
 * @URI:  the element namespace URI
 *
 * Looks up an extension element. @ctxt can be NULL to search only in
 * module elements.
 *
 * Returns the element callback or NULL if not found
 */
xsltTransformFunction
xsltExtElementLookup(xsltTransformContextPtr ctxt,
                     const xmlChar * name, const xmlChar * URI)
{
    xsltTransformFunction ret;

    if ((name == NULL) || (URI == NULL))
        return (NULL);

    if ((ctxt != NULL) && (ctxt->extElements != NULL)) {
        XML_CAST_FPTR(ret) = xmlHashLookup2(ctxt->extElements, name, URI);
        if (ret != NULL) {
            return(ret);
        }
    }

    ret = xsltExtModuleElementLookup(name, URI);

    return (ret);
}

/**
 * xsltExtModuleElementLookup:
 * @name:  the element name
 * @URI:  the element namespace URI
 *
 * Looks up an extension module element
 *
 * Returns the callback function if found, NULL otherwise.
 */
xsltTransformFunction
xsltExtModuleElementLookup(const xmlChar * name, const xmlChar * URI)
{
    xsltExtElementPtr ext;

    if ((xsltElementsHash == NULL) || (name == NULL) || (URI == NULL))
        return (NULL);

    xmlMutexLock(xsltExtMutex);

    ext = (xsltExtElementPtr) xmlHashLookup2(xsltElementsHash, name, URI);

    xmlMutexUnlock(xsltExtMutex);

    /*
     * if function lookup fails, attempt a dynamic load on
     * supported platforms
     */
    if (NULL == ext) {
        if (!xsltExtModuleRegisterDynamic(URI)) {
            xmlMutexLock(xsltExtMutex);

            ext = (xsltExtElementPtr)
	          xmlHashLookup2(xsltElementsHash, name, URI);

            xmlMutexUnlock(xsltExtMutex);
        }
    }

    if (ext == NULL)
        return (NULL);
    return (ext->transform);
}

/**
 * xsltExtModuleElementPreComputeLookup:
 * @name:  the element name
 * @URI:  the element namespace URI
 *
 * Looks up an extension module element pre-computation function
 *
 * Returns the callback function if found, NULL otherwise.
 */
xsltPreComputeFunction
xsltExtModuleElementPreComputeLookup(const xmlChar * name,
                                     const xmlChar * URI)
{
    xsltExtElementPtr ext;

    if ((xsltElementsHash == NULL) || (name == NULL) || (URI == NULL))
        return (NULL);

    xmlMutexLock(xsltExtMutex);

    ext = (xsltExtElementPtr) xmlHashLookup2(xsltElementsHash, name, URI);

    xmlMutexUnlock(xsltExtMutex);

    if (ext == NULL) {
        if (!xsltExtModuleRegisterDynamic(URI)) {
            xmlMutexLock(xsltExtMutex);

            ext = (xsltExtElementPtr)
	          xmlHashLookup2(xsltElementsHash, name, URI);

            xmlMutexUnlock(xsltExtMutex);
        }
    }

    if (ext == NULL)
        return (NULL);
    return (ext->precomp);
}

/**
 * xsltUnregisterExtModuleElement:
 * @name:  the element name
 * @URI:  the element namespace URI
 *
 * Unregisters an extension module element
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltUnregisterExtModuleElement(const xmlChar * name, const xmlChar * URI)
{
    int ret;

    if ((xsltElementsHash == NULL) || (name == NULL) || (URI == NULL))
        return (-1);

    xmlMutexLock(xsltExtMutex);

    ret = xmlHashRemoveEntry2(xsltElementsHash, name, URI,
                              xsltFreeExtElementEntry);

    xmlMutexUnlock(xsltExtMutex);

    return(ret);
}

/**
 * xsltUnregisterAllExtModuleElement:
 *
 * Unregisters all extension module element
 */
static void
xsltUnregisterAllExtModuleElement(void)
{
    xmlMutexLock(xsltExtMutex);

    xmlHashFree(xsltElementsHash, xsltFreeExtElementEntry);
    xsltElementsHash = NULL;

    xmlMutexUnlock(xsltExtMutex);
}

/**
 * xsltRegisterExtModuleTopLevel:
 * @name:  the top-level element name
 * @URI:  the top-level element namespace URI
 * @function:  the top-level element callback
 *
 * Registers an extension module top-level element.
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltRegisterExtModuleTopLevel(const xmlChar * name, const xmlChar * URI,
                              xsltTopLevelFunction function)
{
    if ((name == NULL) || (URI == NULL) || (function == NULL))
        return (-1);

    if (xsltTopLevelsHash == NULL)
        xsltTopLevelsHash = xmlHashCreate(10);
    if (xsltTopLevelsHash == NULL)
        return (-1);

    xmlMutexLock(xsltExtMutex);

    xmlHashUpdateEntry2(xsltTopLevelsHash, name, URI,
                        XML_CAST_FPTR(function), NULL);

    xmlMutexUnlock(xsltExtMutex);

    return (0);
}

/**
 * xsltExtModuleTopLevelLookup:
 * @name:  the top-level element name
 * @URI:  the top-level element namespace URI
 *
 * Looks up an extension module top-level element
 *
 * Returns the callback function if found, NULL otherwise.
 */
xsltTopLevelFunction
xsltExtModuleTopLevelLookup(const xmlChar * name, const xmlChar * URI)
{
    xsltTopLevelFunction ret;

    if ((xsltTopLevelsHash == NULL) || (name == NULL) || (URI == NULL))
        return (NULL);

    xmlMutexLock(xsltExtMutex);

    XML_CAST_FPTR(ret) = xmlHashLookup2(xsltTopLevelsHash, name, URI);

    xmlMutexUnlock(xsltExtMutex);

    /* if lookup fails, attempt a dynamic load on supported platforms */
    if (NULL == ret) {
        if (!xsltExtModuleRegisterDynamic(URI)) {
            xmlMutexLock(xsltExtMutex);

            XML_CAST_FPTR(ret) = xmlHashLookup2(xsltTopLevelsHash, name, URI);

            xmlMutexUnlock(xsltExtMutex);
        }
    }

    return (ret);
}

/**
 * xsltUnregisterExtModuleTopLevel:
 * @name:  the top-level element name
 * @URI:  the top-level element namespace URI
 *
 * Unregisters an extension module top-level element
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltUnregisterExtModuleTopLevel(const xmlChar * name, const xmlChar * URI)
{
    int ret;

    if ((xsltTopLevelsHash == NULL) || (name == NULL) || (URI == NULL))
        return (-1);

    xmlMutexLock(xsltExtMutex);

    ret = xmlHashRemoveEntry2(xsltTopLevelsHash, name, URI, NULL);

    xmlMutexUnlock(xsltExtMutex);

    return(ret);
}

/**
 * xsltUnregisterAllExtModuleTopLevel:
 *
 * Unregisters all extension module function
 */
static void
xsltUnregisterAllExtModuleTopLevel(void)
{
    xmlMutexLock(xsltExtMutex);

    xmlHashFree(xsltTopLevelsHash, NULL);
    xsltTopLevelsHash = NULL;

    xmlMutexUnlock(xsltExtMutex);
}

/**
 * xsltGetExtInfo:
 * @style:  pointer to a stylesheet
 * @URI:    the namespace URI desired
 *
 * looks up URI in extInfos of the stylesheet
 *
 * returns a pointer to the hash table if found, else NULL
 */
xmlHashTablePtr
xsltGetExtInfo(xsltStylesheetPtr style, const xmlChar * URI)
{
    xsltExtDataPtr data;

    /*
    * TODO: Why do we have a return type of xmlHashTablePtr?
    *   Is the user-allocated data for extension modules expected
    *   to be a xmlHashTablePtr only? Or is this intended for
    *   the EXSLT module only?
    */

    if (style != NULL && style->extInfos != NULL) {
        data = xmlHashLookup(style->extInfos, URI);
        if (data != NULL && data->extData != NULL)
            return data->extData;
    }
    return NULL;
}

/************************************************************************
 *									*
 *		Test of the extension module API			*
 *									*
 ************************************************************************/

static xmlChar *testData = NULL;
static xmlChar *testStyleData = NULL;

/**
 * xsltExtFunctionTest:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * function libxslt:test() for testing the extensions support.
 */
static void
xsltExtFunctionTest(xmlXPathParserContextPtr ctxt,
                    int nargs ATTRIBUTE_UNUSED)
{
    xsltTransformContextPtr tctxt;
    void *data = NULL;

    tctxt = xsltXPathGetTransformContext(ctxt);

    if (testData == NULL) {
        xsltGenericDebug(xsltGenericDebugContext,
                         "xsltExtFunctionTest: not initialized,"
                         " calling xsltGetExtData\n");
        data = xsltGetExtData(tctxt, (const xmlChar *) XSLT_DEFAULT_URL);
        if (data == NULL) {
            xsltTransformError(tctxt, NULL, NULL,
                               "xsltExtElementTest: not initialized\n");
            return;
        }
    }
    if (tctxt == NULL) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                           "xsltExtFunctionTest: failed to get the transformation context\n");
        return;
    }
    if (data == NULL)
        data = xsltGetExtData(tctxt, (const xmlChar *) XSLT_DEFAULT_URL);
    if (data == NULL) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                           "xsltExtFunctionTest: failed to get module data\n");
        return;
    }
    if (data != testData) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                           "xsltExtFunctionTest: got wrong module data\n");
        return;
    }
#ifdef WITH_XSLT_DEBUG_FUNCTION
    xsltGenericDebug(xsltGenericDebugContext,
                     "libxslt:test() called with %d args\n", nargs);
#endif
}

/**
 * xsltExtElementPreCompTest:
 * @style:  the stylesheet
 * @inst:  the instruction in the stylesheet
 *
 * Process a libxslt:test node
 */
static xsltElemPreCompPtr
xsltExtElementPreCompTest(xsltStylesheetPtr style, xmlNodePtr inst,
                          xsltTransformFunction function)
{
    xsltElemPreCompPtr ret;

    if (style == NULL) {
        xsltTransformError(NULL, NULL, inst,
                           "xsltExtElementTest: no transformation context\n");
        return (NULL);
    }
    if (testStyleData == NULL) {
        xsltGenericDebug(xsltGenericDebugContext,
                         "xsltExtElementPreCompTest: not initialized,"
                         " calling xsltStyleGetExtData\n");
        xsltStyleGetExtData(style, (const xmlChar *) XSLT_DEFAULT_URL);
        if (testStyleData == NULL) {
            xsltTransformError(NULL, style, inst,
                               "xsltExtElementPreCompTest: not initialized\n");
            if (style != NULL)
                style->errors++;
            return (NULL);
        }
    }
    if (inst == NULL) {
        xsltTransformError(NULL, style, inst,
                           "xsltExtElementPreCompTest: no instruction\n");
        if (style != NULL)
            style->errors++;
        return (NULL);
    }
    ret = xsltNewElemPreComp(style, inst, function);
    return (ret);
}

/**
 * xsltExtElementTest:
 * @ctxt:  an XSLT processing context
 * @node:  The current node
 * @inst:  the instruction in the stylesheet
 * @comp:  precomputed information
 *
 * Process a libxslt:test node
 */
static void
xsltExtElementTest(xsltTransformContextPtr ctxt, xmlNodePtr node,
                   xmlNodePtr inst,
                   xsltElemPreCompPtr comp ATTRIBUTE_UNUSED)
{
    xmlNodePtr commentNode;

    if (testData == NULL) {
        xsltGenericDebug(xsltGenericDebugContext,
                         "xsltExtElementTest: not initialized,"
                         " calling xsltGetExtData\n");
        xsltGetExtData(ctxt, (const xmlChar *) XSLT_DEFAULT_URL);
        if (testData == NULL) {
            xsltTransformError(ctxt, NULL, inst,
                               "xsltExtElementTest: not initialized\n");
            return;
        }
    }
    if (ctxt == NULL) {
        xsltTransformError(ctxt, NULL, inst,
                           "xsltExtElementTest: no transformation context\n");
        return;
    }
    if (node == NULL) {
        xsltTransformError(ctxt, NULL, inst,
                           "xsltExtElementTest: no current node\n");
        return;
    }
    if (inst == NULL) {
        xsltTransformError(ctxt, NULL, inst,
                           "xsltExtElementTest: no instruction\n");
        return;
    }
    if (ctxt->insert == NULL) {
        xsltTransformError(ctxt, NULL, inst,
                           "xsltExtElementTest: no insertion point\n");
        return;
    }
    commentNode = xmlNewComment((const xmlChar *)
                                "libxslt:test element test worked");
    xmlAddChild(ctxt->insert, commentNode);
}

/**
 * xsltExtInitTest:
 * @ctxt:  an XSLT transformation context
 * @URI:  the namespace URI for the extension
 *
 * A function called at initialization time of an XSLT extension module
 *
 * Returns a pointer to the module specific data for this transformation
 */
static void *
xsltExtInitTest(xsltTransformContextPtr ctxt, const xmlChar * URI)
{
    if (testStyleData == NULL) {
        xsltGenericDebug(xsltGenericErrorContext,
                         "xsltExtInitTest: not initialized,"
                         " calling xsltStyleGetExtData\n");
        testStyleData = xsltStyleGetExtData(ctxt->style, URI);
        if (testStyleData == NULL) {
            xsltTransformError(ctxt, NULL, NULL,
                               "xsltExtInitTest: not initialized\n");
            return (NULL);
        }
    }
    if (testData != NULL) {
        xsltTransformError(ctxt, NULL, NULL,
                           "xsltExtInitTest: already initialized\n");
        return (NULL);
    }
    testData = (void *) "test data";
    xsltGenericDebug(xsltGenericDebugContext,
                     "Registered test module : %s\n", URI);
    return (testData);
}


/**
 * xsltExtShutdownTest:
 * @ctxt:  an XSLT transformation context
 * @URI:  the namespace URI for the extension
 * @data:  the data associated to this module
 *
 * A function called at shutdown time of an XSLT extension module
 */
static void
xsltExtShutdownTest(xsltTransformContextPtr ctxt,
                    const xmlChar * URI, void *data)
{
    if (testData == NULL) {
        xsltTransformError(ctxt, NULL, NULL,
                           "xsltExtShutdownTest: not initialized\n");
        return;
    }
    if (data != testData) {
        xsltTransformError(ctxt, NULL, NULL,
                           "xsltExtShutdownTest: wrong data\n");
    }
    testData = NULL;
    xsltGenericDebug(xsltGenericDebugContext,
                     "Unregistered test module : %s\n", URI);
}

/**
 * xsltExtStyleInitTest:
 * @style:  an XSLT stylesheet
 * @URI:  the namespace URI for the extension
 *
 * A function called at initialization time of an XSLT extension module
 *
 * Returns a pointer to the module specific data for this transformation
 */
static void *
xsltExtStyleInitTest(xsltStylesheetPtr style ATTRIBUTE_UNUSED,
                     const xmlChar * URI)
{
    if (testStyleData != NULL) {
        xsltTransformError(NULL, NULL, NULL,
                           "xsltExtInitTest: already initialized\n");
        return (NULL);
    }
    testStyleData = (void *) "test data";
    xsltGenericDebug(xsltGenericDebugContext,
                     "Registered test module : %s\n", URI);
    return (testStyleData);
}


/**
 * xsltExtStyleShutdownTest:
 * @style:  an XSLT stylesheet
 * @URI:  the namespace URI for the extension
 * @data:  the data associated to this module
 *
 * A function called at shutdown time of an XSLT extension module
 */
static void
xsltExtStyleShutdownTest(xsltStylesheetPtr style ATTRIBUTE_UNUSED,
                         const xmlChar * URI, void *data)
{
    if (testStyleData == NULL) {
        xsltGenericError(xsltGenericErrorContext,
                         "xsltExtShutdownTest: not initialized\n");
        return;
    }
    if (data != testStyleData) {
        xsltTransformError(NULL, NULL, NULL,
                           "xsltExtShutdownTest: wrong data\n");
    }
    testStyleData = NULL;
    xsltGenericDebug(xsltGenericDebugContext,
                     "Unregistered test module : %s\n", URI);
}

/**
 * xsltRegisterTestModule:
 *
 * Registers the test module
 */
void
xsltRegisterTestModule(void)
{
    xsltInitGlobals();
    xsltRegisterExtModuleFull((const xmlChar *) XSLT_DEFAULT_URL,
                              xsltExtInitTest, xsltExtShutdownTest,
                              xsltExtStyleInitTest,
                              xsltExtStyleShutdownTest);
    xsltRegisterExtModuleFunction((const xmlChar *) "test",
                                  (const xmlChar *) XSLT_DEFAULT_URL,
                                  xsltExtFunctionTest);
    xsltRegisterExtModuleElement((const xmlChar *) "test",
                                 (const xmlChar *) XSLT_DEFAULT_URL,
                                 xsltExtElementPreCompTest,
                                 xsltExtElementTest);
}

static void
xsltHashScannerModuleFree(void *payload ATTRIBUTE_UNUSED,
                          void *data ATTRIBUTE_UNUSED,
                          const xmlChar *name ATTRIBUTE_UNUSED)
{
#ifdef WITH_MODULES
    xmlModuleClose(payload);
#endif
}

/**
 * xsltInitGlobals:
 *
 * Initialize the global variables for extensions
 */
void
xsltInitGlobals(void)
{
    if (xsltExtMutex == NULL) {
        xsltExtMutex = xmlNewMutex();
    }
}

/**
 * xsltCleanupGlobals:
 *
 * Unregister all global variables set up by the XSLT library
 */
void
xsltCleanupGlobals(void)
{
    xsltUnregisterAllExtModules();
    xsltUnregisterAllExtModuleFunction();
    xsltUnregisterAllExtModuleElement();
    xsltUnregisterAllExtModuleTopLevel();

    xmlMutexLock(xsltExtMutex);
    /* cleanup dynamic module hash */
    if (NULL != xsltModuleHash) {
        xmlHashScan(xsltModuleHash, xsltHashScannerModuleFree, 0);
        xmlHashFree(xsltModuleHash, NULL);
        xsltModuleHash = NULL;
    }
    xmlMutexUnlock(xsltExtMutex);

    xmlFreeMutex(xsltExtMutex);
    xsltExtMutex = NULL;
    xsltFreeLocales();
    xsltUninit();
}

static void
xsltDebugDumpExtensionsCallback(void *function ATTRIBUTE_UNUSED,
                                void *data, const xmlChar * name,
                                const xmlChar * URI,
                                const xmlChar * not_used ATTRIBUTE_UNUSED)
{
    FILE *output = (FILE *) data;
    if (!name || !URI)
        return;
    fprintf(output, "{%s}%s\n", URI, name);
}

static void
xsltDebugDumpExtModulesCallback(void *function ATTRIBUTE_UNUSED,
                                void *data, const xmlChar * URI,
                                const xmlChar * not_used ATTRIBUTE_UNUSED,
                                const xmlChar * not_used2 ATTRIBUTE_UNUSED)
{
    FILE *output = (FILE *) data;
    if (!URI)
        return;
    fprintf(output, "%s\n", URI);
}

/**
 * xsltDebugDumpExtensions:
 * @output:  the FILE * for the output, if NULL stdout is used
 *
 * Dumps a list of the registered XSLT extension functions and elements
 */
void
xsltDebugDumpExtensions(FILE * output)
{
    if (output == NULL)
        output = stdout;
    fprintf(output,
            "Registered XSLT Extensions\n--------------------------\n");
    xmlMutexLock(xsltExtMutex);
    if (!xsltFunctionsHash) {
        fprintf(output, "No registered extension functions\n");
    } else {
        fprintf(output, "Registered extension functions:\n");
        xmlHashScanFull(xsltFunctionsHash, xsltDebugDumpExtensionsCallback,
                        output);
    }
    if (!xsltTopLevelsHash) {
        fprintf(output, "\nNo registered top-level extension elements\n");
    } else {
        fprintf(output, "\nRegistered top-level extension elements:\n");
        xmlHashScanFull(xsltTopLevelsHash, xsltDebugDumpExtensionsCallback,
                        output);
    }
    if (!xsltElementsHash) {
        fprintf(output, "\nNo registered instruction extension elements\n");
    } else {
        fprintf(output, "\nRegistered instruction extension elements:\n");
        xmlHashScanFull(xsltElementsHash, xsltDebugDumpExtensionsCallback,
                        output);
    }
    if (!xsltExtensionsHash) {
        fprintf(output, "\nNo registered extension modules\n");
    } else {
        fprintf(output, "\nRegistered extension modules:\n");
        xmlHashScanFull(xsltExtensionsHash, xsltDebugDumpExtModulesCallback,
                        output);
    }
    xmlMutexUnlock(xsltExtMutex);
}
