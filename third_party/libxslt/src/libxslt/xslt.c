/*
 * xslt.c: Implemetation of an XSL Transformation 1.0 engine
 *
 * Reference:
 *   XSLT specification
 *   http://www.w3.org/TR/1999/REC-xslt-19991116
 *
 *   Associating Style Sheets with XML documents
 *   http://www.w3.org/1999/06/REC-xml-stylesheet-19990629
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXSLT
#include "libxslt.h"

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/hash.h>
#include <libxml/uri.h>
#include <libxml/xmlerror.h>
#include <libxml/parserInternals.h>
#include <libxml/xpathInternals.h>
#include <libxml/xpath.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "pattern.h"
#include "variables.h"
#include "namespaces.h"
#include "attributes.h"
#include "xsltutils.h"
#include "imports.h"
#include "keys.h"
#include "documents.h"
#include "extensions.h"
#include "preproc.h"
#include "extra.h"
#include "security.h"
#include "xsltlocale.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_PARSING
/* #define WITH_XSLT_DEBUG_BLANKS */
#endif

const char *xsltEngineVersion = LIBXSLT_VERSION_STRING LIBXSLT_VERSION_EXTRA;
const int xsltLibxsltVersion = LIBXSLT_VERSION;
const int xsltLibxmlVersion = LIBXML_VERSION;

#ifdef XSLT_REFACTORED

const xmlChar *xsltConstNamespaceNameXSLT = (const xmlChar *) XSLT_NAMESPACE;

#define XSLT_ELEMENT_CATEGORY_XSLT 0
#define XSLT_ELEMENT_CATEGORY_EXTENSION 1
#define XSLT_ELEMENT_CATEGORY_LRE 2

/*
* xsltLiteralResultMarker:
* Marker for Literal result elements, in order to avoid multiple attempts
* to recognize such elements in the stylesheet's tree.
* This marker is set on node->psvi during the initial traversal
* of a stylesheet's node tree.
*
const xmlChar *xsltLiteralResultMarker =
    (const xmlChar *) "Literal Result Element";
*/

/*
* xsltXSLTTextMarker:
* Marker for xsl:text elements. Used to recognize xsl:text elements
* for post-processing of the stylesheet's tree, where those
* elements are removed from the tree.
*/
const xmlChar *xsltXSLTTextMarker = (const xmlChar *) "XSLT Text Element";

/*
* xsltXSLTAttrMarker:
* Marker for XSLT attribute on Literal Result Elements.
*/
const xmlChar *xsltXSLTAttrMarker = (const xmlChar *) "LRE XSLT Attr";

#endif

#ifdef XSLT_LOCALE_WINAPI
extern xmlRMutexPtr xsltLocaleMutex;
#endif

/*
 * Useful macros
 */

#ifdef  IS_BLANK
#undef	IS_BLANK
#endif
#define IS_BLANK(c) (((c) == 0x20) || ((c) == 0x09) || ((c) == 0xA) ||	\
                     ((c) == 0x0D))

#ifdef	IS_BLANK_NODE
#undef	IS_BLANK_NODE
#endif
#define IS_BLANK_NODE(n)						\
    (((n)->type == XML_TEXT_NODE) && (xsltIsBlank((n)->content)))

/**
 * xsltParseContentError:
 *
 * @style: the stylesheet
 * @node: the node where the error occured
 *
 * Compile-time error function.
 */
static void
xsltParseContentError(xsltStylesheetPtr style,
		       xmlNodePtr node)
{
    if ((style == NULL) || (node == NULL))
	return;

    if (IS_XSLT_ELEM(node))
	xsltTransformError(NULL, style, node,
	    "The XSLT-element '%s' is not allowed at this position.\n",
	    node->name);
    else
	xsltTransformError(NULL, style, node,
	    "The element '%s' is not allowed at this position.\n",
	    node->name);
    style->errors++;
}

#ifdef XSLT_REFACTORED
#else
/**
 * exclPrefixPush:
 * @style: the transformation stylesheet
 * @value:  the excluded namespace name to push on the stack
 *
 * Push an excluded namespace name on the stack
 *
 * Returns the new index in the stack or -1 if already present or
 * in case of error
 */
static int
exclPrefixPush(xsltStylesheetPtr style, xmlChar * value)
{
    int i;

    /* do not push duplicates */
    for (i = 0;i < style->exclPrefixNr;i++) {
        if (xmlStrEqual(style->exclPrefixTab[i], value))
	    return(-1);
    }
    if (style->exclPrefixNr >= style->exclPrefixMax) {
        xmlChar **tmp;
        size_t max = style->exclPrefixMax ? style->exclPrefixMax * 2 : 4;

        tmp = xmlRealloc(style->exclPrefixTab,
                         max * sizeof(style->exclPrefixTab[0]));
        if (tmp == NULL) {
            xmlGenericError(xmlGenericErrorContext, "realloc failed !\n");
            return (-1);
        }
        style->exclPrefixTab = tmp;
        style->exclPrefixMax = max;
    }
    style->exclPrefixTab[style->exclPrefixNr] = value;
    style->exclPrefix = value;
    return (style->exclPrefixNr++);
}
/**
 * exclPrefixPop:
 * @style: the transformation stylesheet
 *
 * Pop an excluded prefix value from the stack
 *
 * Returns the stored excluded prefix value
 */
static xmlChar *
exclPrefixPop(xsltStylesheetPtr style)
{
    xmlChar *ret;

    if (style->exclPrefixNr <= 0)
        return (0);
    style->exclPrefixNr--;
    if (style->exclPrefixNr > 0)
        style->exclPrefix = style->exclPrefixTab[style->exclPrefixNr - 1];
    else
        style->exclPrefix = NULL;
    ret = style->exclPrefixTab[style->exclPrefixNr];
    style->exclPrefixTab[style->exclPrefixNr] = 0;
    return (ret);
}
#endif

/************************************************************************
 *									*
 *			Helper functions				*
 *									*
 ************************************************************************/

static int initialized = 0;
/**
 * xsltInit:
 *
 * Initializes the processor (e.g. registers built-in extensions,
 * etc.)
 */
void
xsltInit (void) {
    if (initialized == 0) {
	initialized = 1;
#ifdef XSLT_LOCALE_WINAPI
	xsltLocaleMutex = xmlNewRMutex();
#endif
        xsltRegisterAllExtras();
    }
}

/**
 * xsltUninit:
 *
 * Uninitializes the processor.
 */
void
xsltUninit (void) {
#ifdef XSLT_LOCALE_WINAPI
    xmlFreeRMutex(xsltLocaleMutex);
    xsltLocaleMutex = NULL;
#endif
    initialized = 0;
}

/**
 * xsltIsBlank:
 * @str:  a string
 *
 * Check if a string is ignorable
 *
 * Returns 1 if the string is NULL or made of blanks chars, 0 otherwise
 */
int
xsltIsBlank(xmlChar *str) {
    if (str == NULL)
	return(1);
    while (*str != 0) {
	if (!(IS_BLANK(*str))) return(0);
	str++;
    }
    return(1);
}

/************************************************************************
 *									*
 *		Routines to handle XSLT data structures			*
 *									*
 ************************************************************************/
static xsltDecimalFormatPtr
xsltNewDecimalFormat(const xmlChar *nsUri, xmlChar *name)
{
    xsltDecimalFormatPtr self;
    /* UTF-8 for 0x2030 */
    static const xmlChar permille[4] = {0xe2, 0x80, 0xb0, 0};

    self = xmlMalloc(sizeof(xsltDecimalFormat));
    if (self != NULL) {
	self->next = NULL;
        self->nsUri = nsUri;
	self->name = name;

	/* Default values */
	self->digit = xmlStrdup(BAD_CAST("#"));
	self->patternSeparator = xmlStrdup(BAD_CAST(";"));
	self->decimalPoint = xmlStrdup(BAD_CAST("."));
	self->grouping = xmlStrdup(BAD_CAST(","));
	self->percent = xmlStrdup(BAD_CAST("%"));
	self->permille = xmlStrdup(BAD_CAST(permille));
	self->zeroDigit = xmlStrdup(BAD_CAST("0"));
	self->minusSign = xmlStrdup(BAD_CAST("-"));
	self->infinity = xmlStrdup(BAD_CAST("Infinity"));
	self->noNumber = xmlStrdup(BAD_CAST("NaN"));
    }
    return self;
}

static void
xsltFreeDecimalFormat(xsltDecimalFormatPtr self)
{
    if (self != NULL) {
	if (self->digit)
	    xmlFree(self->digit);
	if (self->patternSeparator)
	    xmlFree(self->patternSeparator);
	if (self->decimalPoint)
	    xmlFree(self->decimalPoint);
	if (self->grouping)
	    xmlFree(self->grouping);
	if (self->percent)
	    xmlFree(self->percent);
	if (self->permille)
	    xmlFree(self->permille);
	if (self->zeroDigit)
	    xmlFree(self->zeroDigit);
	if (self->minusSign)
	    xmlFree(self->minusSign);
	if (self->infinity)
	    xmlFree(self->infinity);
	if (self->noNumber)
	    xmlFree(self->noNumber);
	if (self->name)
	    xmlFree(self->name);
	xmlFree(self);
    }
}

static void
xsltFreeDecimalFormatList(xsltStylesheetPtr self)
{
    xsltDecimalFormatPtr iter;
    xsltDecimalFormatPtr tmp;

    if (self == NULL)
	return;

    iter = self->decimalFormat;
    while (iter != NULL) {
	tmp = iter->next;
	xsltFreeDecimalFormat(iter);
	iter = tmp;
    }
}

/**
 * xsltDecimalFormatGetByName:
 * @style: the XSLT stylesheet
 * @name: the decimal-format name to find
 *
 * Find decimal-format by name
 *
 * Returns the xsltDecimalFormatPtr
 */
xsltDecimalFormatPtr
xsltDecimalFormatGetByName(xsltStylesheetPtr style, xmlChar *name)
{
    xsltDecimalFormatPtr result = NULL;

    if (name == NULL)
	return style->decimalFormat;

    while (style != NULL) {
	for (result = style->decimalFormat->next;
	     result != NULL;
	     result = result->next) {
	    if ((result->nsUri == NULL) && xmlStrEqual(name, result->name))
		return result;
	}
	style = xsltNextImport(style);
    }
    return result;
}

/**
 * xsltDecimalFormatGetByQName:
 * @style: the XSLT stylesheet
 * @nsUri: the namespace URI of the QName
 * @name: the local part of the QName
 *
 * Find decimal-format by QName
 *
 * Returns the xsltDecimalFormatPtr
 */
xsltDecimalFormatPtr
xsltDecimalFormatGetByQName(xsltStylesheetPtr style, const xmlChar *nsUri,
                            const xmlChar *name)
{
    xsltDecimalFormatPtr result = NULL;

    if (name == NULL)
	return style->decimalFormat;

    while (style != NULL) {
	for (result = style->decimalFormat->next;
	     result != NULL;
	     result = result->next) {
	    if (xmlStrEqual(nsUri, result->nsUri) &&
                xmlStrEqual(name, result->name))
		return result;
	}
	style = xsltNextImport(style);
    }
    return result;
}


/**
 * xsltNewTemplate:
 *
 * Create a new XSLT Template
 *
 * Returns the newly allocated xsltTemplatePtr or NULL in case of error
 */
static xsltTemplatePtr
xsltNewTemplate(void) {
    xsltTemplatePtr cur;

    cur = (xsltTemplatePtr) xmlMalloc(sizeof(xsltTemplate));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltNewTemplate : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltTemplate));
    cur->priority = XSLT_PAT_NO_PRIORITY;
    return(cur);
}

/**
 * xsltFreeTemplate:
 * @template:  an XSLT template
 *
 * Free up the memory allocated by @template
 */
static void
xsltFreeTemplate(xsltTemplatePtr template) {
    if (template == NULL)
	return;
    if (template->match) xmlFree(template->match);
/*
*   NOTE: @name and @nameURI are put into the string dict now.
*   if (template->name) xmlFree(template->name);
*   if (template->nameURI) xmlFree(template->nameURI);
*/
/*
    if (template->mode) xmlFree(template->mode);
    if (template->modeURI) xmlFree(template->modeURI);
 */
    if (template->inheritedNs) xmlFree(template->inheritedNs);

    /* free profiling data */
    if (template->templCalledTab) xmlFree(template->templCalledTab);
    if (template->templCountTab) xmlFree(template->templCountTab);

    memset(template, -1, sizeof(xsltTemplate));
    xmlFree(template);
}

/**
 * xsltFreeTemplateList:
 * @template:  an XSLT template list
 *
 * Free up the memory allocated by all the elements of @template
 */
static void
xsltFreeTemplateList(xsltTemplatePtr template) {
    xsltTemplatePtr cur;

    while (template != NULL) {
	cur = template;
	template = template->next;
	xsltFreeTemplate(cur);
    }
}

#ifdef XSLT_REFACTORED

static void
xsltFreeNsAliasList(xsltNsAliasPtr item)
{
    xsltNsAliasPtr tmp;

    while (item) {
	tmp = item;
	item = item->next;
	xmlFree(tmp);
    }
    return;
}

#ifdef XSLT_REFACTORED_XSLT_NSCOMP
static void
xsltFreeNamespaceMap(xsltNsMapPtr item)
{
    xsltNsMapPtr tmp;

    while (item) {
	tmp = item;
	item = item->next;
	xmlFree(tmp);
    }
    return;
}

static xsltNsMapPtr
xsltNewNamespaceMapItem(xsltCompilerCtxtPtr cctxt,
			xmlDocPtr doc,
			xmlNsPtr ns,
			xmlNodePtr elem)
{
    xsltNsMapPtr ret;

    if ((cctxt == NULL) || (doc == NULL) || (ns == NULL))
	return(NULL);

    ret = (xsltNsMapPtr) xmlMalloc(sizeof(xsltNsMap));
    if (ret == NULL) {
	xsltTransformError(NULL, cctxt->style, elem,
	    "Internal error: (xsltNewNamespaceMapItem) "
	    "memory allocation failed.\n");
	return(NULL);
    }
    memset(ret, 0, sizeof(xsltNsMap));
    ret->doc = doc;
    ret->ns = ns;
    ret->origNsName = ns->href;
    /*
    * Store the item at current stylesheet-level.
    */
    if (cctxt->psData->nsMap != NULL)
	ret->next = cctxt->psData->nsMap;
    cctxt->psData->nsMap = ret;

    return(ret);
}
#endif /* XSLT_REFACTORED_XSLT_NSCOMP */

/**
 * xsltCompilerVarInfoFree:
 * @cctxt: the compilation context
 *
 * Frees the list of information for vars/params.
 */
static void
xsltCompilerVarInfoFree(xsltCompilerCtxtPtr cctxt)
{
    xsltVarInfoPtr ivar = cctxt->ivars, ivartmp;

    while (ivar) {
	ivartmp = ivar;
	ivar = ivar->next;
	xmlFree(ivartmp);
    }
}

/**
 * xsltCompilerCtxtFree:
 *
 * Free an XSLT compiler context.
 */
static void
xsltCompilationCtxtFree(xsltCompilerCtxtPtr cctxt)
{
    if (cctxt == NULL)
	return;
#ifdef WITH_XSLT_DEBUG_PARSING
    xsltGenericDebug(xsltGenericDebugContext,
	"Freeing compilation context\n");
    xsltGenericDebug(xsltGenericDebugContext,
	"### Max inodes: %d\n", cctxt->maxNodeInfos);
    xsltGenericDebug(xsltGenericDebugContext,
	"### Max LREs  : %d\n", cctxt->maxLREs);
#endif
    /*
    * Free node-infos.
    */
    if (cctxt->inodeList != NULL) {
	xsltCompilerNodeInfoPtr tmp, cur = cctxt->inodeList;
	while (cur != NULL) {
	    tmp = cur;
	    cur = cur->next;
	    xmlFree(tmp);
	}
    }
    if (cctxt->tmpList != NULL)
	xsltPointerListFree(cctxt->tmpList);
    if (cctxt->nsAliases != NULL)
	xsltFreeNsAliasList(cctxt->nsAliases);

    if (cctxt->ivars)
	xsltCompilerVarInfoFree(cctxt);

    xmlFree(cctxt);
}

/**
 * xsltCompilerCreate:
 *
 * Creates an XSLT compiler context.
 *
 * Returns the pointer to the created xsltCompilerCtxt or
 *         NULL in case of an internal error.
 */
static xsltCompilerCtxtPtr
xsltCompilationCtxtCreate(xsltStylesheetPtr style) {
    xsltCompilerCtxtPtr ret;

    ret = (xsltCompilerCtxtPtr) xmlMalloc(sizeof(xsltCompilerCtxt));
    if (ret == NULL) {
	xsltTransformError(NULL, style, NULL,
	    "xsltCompilerCreate: allocation of compiler "
	    "context failed.\n");
	return(NULL);
    }
    memset(ret, 0, sizeof(xsltCompilerCtxt));

    ret->errSeverity = XSLT_ERROR_SEVERITY_ERROR;
    ret->tmpList = xsltPointerListCreate(20);
    if (ret->tmpList == NULL) {
	goto internal_err;
    }

    return(ret);

internal_err:
    xsltCompilationCtxtFree(ret);
    return(NULL);
}

static void
xsltLREEffectiveNsNodesFree(xsltEffectiveNsPtr first)
{
    xsltEffectiveNsPtr tmp;

    while (first != NULL) {
	tmp = first;
	first = first->nextInStore;
	xmlFree(tmp);
    }
}

static void
xsltFreePrincipalStylesheetData(xsltPrincipalStylesheetDataPtr data)
{
    if (data == NULL)
	return;

    if (data->inScopeNamespaces != NULL) {
	int i;
	xsltNsListContainerPtr nsi;
	xsltPointerListPtr list =
	    (xsltPointerListPtr) data->inScopeNamespaces;

	for (i = 0; i < list->number; i++) {
	    /*
	    * REVISIT TODO: Free info of in-scope namespaces.
	    */
	    nsi = (xsltNsListContainerPtr) list->items[i];
	    if (nsi->list != NULL)
		xmlFree(nsi->list);
	    xmlFree(nsi);
	}
	xsltPointerListFree(list);
	data->inScopeNamespaces = NULL;
    }

    if (data->exclResultNamespaces != NULL) {
	int i;
	xsltPointerListPtr list = (xsltPointerListPtr)
	    data->exclResultNamespaces;

	for (i = 0; i < list->number; i++)
	    xsltPointerListFree((xsltPointerListPtr) list->items[i]);

	xsltPointerListFree(list);
	data->exclResultNamespaces = NULL;
    }

    if (data->extElemNamespaces != NULL) {
	xsltPointerListPtr list = (xsltPointerListPtr)
	    data->extElemNamespaces;
	int i;

	for (i = 0; i < list->number; i++)
	    xsltPointerListFree((xsltPointerListPtr) list->items[i]);

	xsltPointerListFree(list);
	data->extElemNamespaces = NULL;
    }
    if (data->effectiveNs) {
	xsltLREEffectiveNsNodesFree(data->effectiveNs);
	data->effectiveNs = NULL;
    }
#ifdef XSLT_REFACTORED_XSLT_NSCOMP
    xsltFreeNamespaceMap(data->nsMap);
#endif
    xmlFree(data);
}

static xsltPrincipalStylesheetDataPtr
xsltNewPrincipalStylesheetData(void)
{
    xsltPrincipalStylesheetDataPtr ret;

    ret = (xsltPrincipalStylesheetDataPtr)
	xmlMalloc(sizeof(xsltPrincipalStylesheetData));
    if (ret == NULL) {
	xsltTransformError(NULL, NULL, NULL,
	    "xsltNewPrincipalStylesheetData: memory allocation failed.\n");
	return(NULL);
    }
    memset(ret, 0, sizeof(xsltPrincipalStylesheetData));

    /*
    * Global list of in-scope namespaces.
    */
    ret->inScopeNamespaces = xsltPointerListCreate(-1);
    if (ret->inScopeNamespaces == NULL)
	goto internal_err;
    /*
    * Global list of excluded result ns-decls.
    */
    ret->exclResultNamespaces = xsltPointerListCreate(-1);
    if (ret->exclResultNamespaces == NULL)
	goto internal_err;
    /*
    * Global list of extension instruction namespace names.
    */
    ret->extElemNamespaces = xsltPointerListCreate(-1);
    if (ret->extElemNamespaces == NULL)
	goto internal_err;

    return(ret);

internal_err:

    return(NULL);
}

#endif

/**
 * xsltNewStylesheetInternal:
 * @parent:  the parent stylesheet or NULL
 *
 * Create a new XSLT Stylesheet
 *
 * Returns the newly allocated xsltStylesheetPtr or NULL in case of error
 */
static xsltStylesheetPtr
xsltNewStylesheetInternal(xsltStylesheetPtr parent) {
    xsltStylesheetPtr ret = NULL;

    ret = (xsltStylesheetPtr) xmlMalloc(sizeof(xsltStylesheet));
    if (ret == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltNewStylesheet : malloc failed\n");
	goto internal_err;
    }
    memset(ret, 0, sizeof(xsltStylesheet));

    ret->parent = parent;
    ret->omitXmlDeclaration = -1;
    ret->standalone = -1;
    ret->decimalFormat = xsltNewDecimalFormat(NULL, NULL);
    ret->indent = -1;
    ret->errors = 0;
    ret->warnings = 0;
    ret->exclPrefixNr = 0;
    ret->exclPrefixMax = 0;
    ret->exclPrefixTab = NULL;
    ret->extInfos = NULL;
    ret->extrasNr = 0;
    ret->internalized = 1;
    ret->literal_result = 0;
    ret->forwards_compatible = 0;
    ret->dict = xmlDictCreate();
#ifdef WITH_XSLT_DEBUG
    xsltGenericDebug(xsltGenericDebugContext,
	"creating dictionary for stylesheet\n");
#endif

    if (parent == NULL) {
        ret->principal = ret;

        ret->xpathCtxt = xmlXPathNewContext(NULL);
        if (ret->xpathCtxt == NULL) {
            xsltTransformError(NULL, NULL, NULL,
                    "xsltNewStylesheet: xmlXPathNewContext failed\n");
            goto internal_err;
        }
        if (xmlXPathContextSetCache(ret->xpathCtxt, 1, -1, 0) == -1)
            goto internal_err;
    } else {
        ret->principal = parent->principal;
    }

    xsltInit();

    return(ret);

internal_err:
    if (ret != NULL)
	xsltFreeStylesheet(ret);
    return(NULL);
}

/**
 * xsltNewStylesheet:
 *
 * Create a new XSLT Stylesheet
 *
 * Returns the newly allocated xsltStylesheetPtr or NULL in case of error
 */
xsltStylesheetPtr
xsltNewStylesheet(void) {
    return xsltNewStylesheetInternal(NULL);
}

/**
 * xsltAllocateExtra:
 * @style:  an XSLT stylesheet
 *
 * Allocate an extra runtime information slot statically while compiling
 * the stylesheet and return its number
 *
 * Returns the number of the slot
 */
int
xsltAllocateExtra(xsltStylesheetPtr style)
{
    return(style->extrasNr++);
}

/**
 * xsltAllocateExtraCtxt:
 * @ctxt:  an XSLT transformation context
 *
 * Allocate an extra runtime information slot at run-time
 * and return its number
 * This make sure there is a slot ready in the transformation context
 *
 * Returns the number of the slot
 */
int
xsltAllocateExtraCtxt(xsltTransformContextPtr ctxt)
{
    if (ctxt->extrasNr >= ctxt->extrasMax) {
	int i;
	if (ctxt->extrasNr == 0) {
	    ctxt->extrasMax = 20;
	    ctxt->extras = (xsltRuntimeExtraPtr)
		xmlMalloc(ctxt->extrasMax * sizeof(xsltRuntimeExtra));
	    if (ctxt->extras == NULL) {
		xsltTransformError(ctxt, NULL, NULL,
			"xsltAllocateExtraCtxt: out of memory\n");
		return(0);
	    }
	    for (i = 0;i < ctxt->extrasMax;i++) {
		ctxt->extras[i].info = NULL;
		ctxt->extras[i].deallocate = NULL;
		ctxt->extras[i].val.ptr = NULL;
	    }

	} else {
	    xsltRuntimeExtraPtr tmp;

	    ctxt->extrasMax += 100;
	    tmp = (xsltRuntimeExtraPtr) xmlRealloc(ctxt->extras,
		            ctxt->extrasMax * sizeof(xsltRuntimeExtra));
	    if (tmp == NULL) {
		xsltTransformError(ctxt, NULL, NULL,
			"xsltAllocateExtraCtxt: out of memory\n");
		return(0);
	    }
	    ctxt->extras = tmp;
	    for (i = ctxt->extrasNr;i < ctxt->extrasMax;i++) {
		ctxt->extras[i].info = NULL;
		ctxt->extras[i].deallocate = NULL;
		ctxt->extras[i].val.ptr = NULL;
	    }
	}
    }
    return(ctxt->extrasNr++);
}

/**
 * xsltFreeStylesheetList:
 * @style:  an XSLT stylesheet list
 *
 * Free up the memory allocated by the list @style
 */
static void
xsltFreeStylesheetList(xsltStylesheetPtr style) {
    xsltStylesheetPtr next;

    while (style != NULL) {
	next = style->next;
	xsltFreeStylesheet(style);
	style = next;
    }
}

/**
 * xsltCleanupStylesheetTree:
 *
 * @doc: the document-node
 * @node: the element where the stylesheet is rooted at
 *
 * Actually @node need not be the document-element, but
 * currently Libxslt does not support embedded stylesheets.
 *
 * Returns 0 if OK, -1 on API or internal errors.
 */
static int
xsltCleanupStylesheetTree(xmlDocPtr doc ATTRIBUTE_UNUSED,
			  xmlNodePtr rootElem ATTRIBUTE_UNUSED)
{
#if 0 /* TODO: Currently disabled, since probably not needed. */
    xmlNodePtr cur;

    if ((doc == NULL) || (rootElem == NULL) ||
	(rootElem->type != XML_ELEMENT_NODE) ||
	(doc != rootElem->doc))
	return(-1);

    /*
    * Cleanup was suggested by Aleksey Sanin:
    * Clear the PSVI field to avoid problems if the
    * node-tree of the stylesheet is intended to be used for
    * further processing by the user (e.g. for compiling it
    * once again - although not recommended).
    */

    cur = rootElem;
    while (cur != NULL) {
	if (cur->type == XML_ELEMENT_NODE) {
	    /*
	    * Clear the PSVI field.
	    */
	    cur->psvi = NULL;
	    if (cur->children) {
		cur = cur->children;
		continue;
	    }
	}

leave_node:
	if (cur == rootElem)
	    break;
	if (cur->next != NULL)
	    cur = cur->next;
	else {
	    cur = cur->parent;
	    if (cur == NULL)
		break;
	    goto leave_node;
	}
    }
#endif /* #if 0 */
    return(0);
}

/**
 * xsltFreeStylesheet:
 * @style:  an XSLT stylesheet
 *
 * Free up the memory allocated by @style
 */
void
xsltFreeStylesheet(xsltStylesheetPtr style)
{
    if (style == NULL)
        return;

#ifdef XSLT_REFACTORED
    /*
    * Start with a cleanup of the main stylesheet's doc.
    */
    if ((style->principal == style) && (style->doc))
	xsltCleanupStylesheetTree(style->doc,
	    xmlDocGetRootElement(style->doc));
#ifdef XSLT_REFACTORED_XSLT_NSCOMP
    /*
    * Restore changed ns-decls before freeing the document.
    */
    if ((style->doc != NULL) &&
	XSLT_HAS_INTERNAL_NSMAP(style))
    {
	xsltRestoreDocumentNamespaces(XSLT_GET_INTERNAL_NSMAP(style),
	    style->doc);
    }
#endif /* XSLT_REFACTORED_XSLT_NSCOMP */
#else
    /*
    * Start with a cleanup of the main stylesheet's doc.
    */
    if ((style->parent == NULL) && (style->doc))
	xsltCleanupStylesheetTree(style->doc,
	    xmlDocGetRootElement(style->doc));
#endif /* XSLT_REFACTORED */

    xsltFreeKeys(style);
    xsltFreeExts(style);
    xsltFreeTemplateHashes(style);
    xsltFreeDecimalFormatList(style);
    xsltFreeTemplateList(style->templates);
    xsltFreeAttributeSetsHashes(style);
    xsltFreeNamespaceAliasHashes(style);
    xsltFreeStylePreComps(style);
    /*
    * Free documents of all included stylsheet modules of this
    * stylesheet level.
    */
    xsltFreeStyleDocuments(style);
    /*
    * TODO: Best time to shutdown extension stuff?
    */
    xsltShutdownExts(style);

    if (style->variables != NULL)
        xsltFreeStackElemList(style->variables);
    if (style->cdataSection != NULL)
        xmlHashFree(style->cdataSection, NULL);
    if (style->stripSpaces != NULL)
        xmlHashFree(style->stripSpaces, NULL);
    if (style->nsHash != NULL)
        xmlHashFree(style->nsHash, NULL);
    if (style->exclPrefixTab != NULL)
        xmlFree(style->exclPrefixTab);
    if (style->method != NULL)
        xmlFree(style->method);
    if (style->methodURI != NULL)
        xmlFree(style->methodURI);
    if (style->version != NULL)
        xmlFree(style->version);
    if (style->encoding != NULL)
        xmlFree(style->encoding);
    if (style->doctypePublic != NULL)
        xmlFree(style->doctypePublic);
    if (style->doctypeSystem != NULL)
        xmlFree(style->doctypeSystem);
    if (style->mediaType != NULL)
        xmlFree(style->mediaType);
    if (style->attVTs)
        xsltFreeAVTList(style->attVTs);
    if (style->imports != NULL)
        xsltFreeStylesheetList(style->imports);

#ifdef XSLT_REFACTORED
    /*
    * If this is the principal stylesheet, then
    * free its internal data.
    */
    if (style->principal == style) {
	if (style->principalData) {
	    xsltFreePrincipalStylesheetData(style->principalData);
	    style->principalData = NULL;
	}
    }
#endif
    /*
    * Better to free the main document of this stylesheet level
    * at the end - so here.
    */
    if (style->doc != NULL) {
        xmlFreeDoc(style->doc);
    }

#ifdef WITH_XSLT_DEBUG
    xsltGenericDebug(xsltGenericDebugContext,
                     "freeing dictionary from stylesheet\n");
#endif
    xmlDictFree(style->dict);

    if (style->xpathCtxt != NULL)
	xmlXPathFreeContext(style->xpathCtxt);

    memset(style, -1, sizeof(xsltStylesheet));
    xmlFree(style);
}

/************************************************************************
 *									*
 *		Parsing of an XSLT Stylesheet				*
 *									*
 ************************************************************************/

#ifdef XSLT_REFACTORED
    /*
    * This is now performed in an optimized way in xsltParseXSLTTemplate.
    */
#else
/**
 * xsltGetInheritedNsList:
 * @style:  the stylesheet
 * @template: the template
 * @node:  the current node
 *
 * Search all the namespace applying to a given element except the ones
 * from excluded output prefixes currently in scope. Initialize the
 * template inheritedNs list with it.
 *
 * Returns the number of entries found
 */
static int
xsltGetInheritedNsList(xsltStylesheetPtr style,
	               xsltTemplatePtr template,
	               xmlNodePtr node)
{
    xmlNsPtr cur;
    xmlNsPtr *ret = NULL, *tmp;
    int nbns = 0;
    int maxns = 0;
    int i;

    if ((style == NULL) || (template == NULL) || (node == NULL) ||
	(template->inheritedNsNr != 0) || (template->inheritedNs != NULL))
	return(0);
    while (node != NULL) {
        if (node->type == XML_ELEMENT_NODE) {
            cur = node->nsDef;
            while (cur != NULL) {
		if (xmlStrEqual(cur->href, XSLT_NAMESPACE))
		    goto skip_ns;

		if ((cur->prefix != NULL) &&
		    (xsltCheckExtPrefix(style, cur->prefix)))
		    goto skip_ns;
		/*
		* Check if this namespace was excluded.
		* Note that at this point only the exclusions defined
		* on the topmost stylesheet element are in the exclusion-list.
		*/
		for (i = 0;i < style->exclPrefixNr;i++) {
		    if (xmlStrEqual(cur->href, style->exclPrefixTab[i]))
			goto skip_ns;
		}
		/*
		* Skip shadowed namespace bindings.
		*/
                for (i = 0; i < nbns; i++) {
                    if ((cur->prefix == ret[i]->prefix) ||
                        (xmlStrEqual(cur->prefix, ret[i]->prefix)))
                        break;
                }
                if (i >= nbns) {
                    if (nbns >= maxns) {
                        maxns = (maxns == 0) ? 10 : 2 * maxns;
                        tmp = (xmlNsPtr *) xmlRealloc(ret,
                                (maxns + 1) * sizeof(xmlNsPtr));
                        if (tmp == NULL) {
                            xmlGenericError(xmlGenericErrorContext,
                                            "xsltGetInheritedNsList : realloc failed!\n");
                            xmlFree(ret);
                            return(0);
                        }
                        ret = tmp;
                    }
                    ret[nbns++] = cur;
                    ret[nbns] = NULL;
                }
skip_ns:
                cur = cur->next;
            }
        }
        node = node->parent;
    }
    if (nbns != 0) {
#ifdef WITH_XSLT_DEBUG_PARSING
        xsltGenericDebug(xsltGenericDebugContext,
                         "template has %d inherited namespaces\n", nbns);
#endif
	template->inheritedNsNr = nbns;
	template->inheritedNs = ret;
    }
    return (nbns);
}
#endif /* else of XSLT_REFACTORED */

/**
 * xsltParseStylesheetOutput:
 * @style:  the XSLT stylesheet
 * @cur:  the "output" element
 *
 * parse an XSLT stylesheet output element and record
 * information related to the stylesheet output
 */

void
xsltParseStylesheetOutput(xsltStylesheetPtr style, xmlNodePtr cur)
{
    xmlChar *elements,
     *prop;
    xmlChar *element,
     *end;

    if ((cur == NULL) || (style == NULL) || (cur->type != XML_ELEMENT_NODE))
        return;

    prop = xmlGetNsProp(cur, (const xmlChar *) "version", NULL);
    if (prop != NULL) {
        if (style->version != NULL)
            xmlFree(style->version);
        style->version = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *) "encoding", NULL);
    if (prop != NULL) {
        if (style->encoding != NULL)
            xmlFree(style->encoding);
        style->encoding = prop;
    }

    /* relaxed to support xt:document
    * TODO KB: What does "relaxed to support xt:document" mean?
    */
    prop = xmlGetNsProp(cur, (const xmlChar *) "method", NULL);
    if (prop != NULL) {
        const xmlChar *URI;

        if (style->method != NULL)
            xmlFree(style->method);
        style->method = NULL;
        if (style->methodURI != NULL)
            xmlFree(style->methodURI);
        style->methodURI = NULL;

	/*
	* TODO: Don't use xsltGetQNameURI().
	*/
	URI = xsltGetQNameURI(cur, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	} else if (URI == NULL) {
            if ((xmlStrEqual(prop, (const xmlChar *) "xml")) ||
                (xmlStrEqual(prop, (const xmlChar *) "html")) ||
                (xmlStrEqual(prop, (const xmlChar *) "text"))) {
                style->method = prop;
            } else {
		xsltTransformError(NULL, style, cur,
                                 "invalid value for method: %s\n", prop);
                if (style != NULL) style->warnings++;
                xmlFree(prop);
            }
	} else {
	    style->method = prop;
	    style->methodURI = xmlStrdup(URI);
	}
    }

    prop = xmlGetNsProp(cur, (const xmlChar *) "doctype-system", NULL);
    if (prop != NULL) {
        if (style->doctypeSystem != NULL)
            xmlFree(style->doctypeSystem);
        style->doctypeSystem = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *) "doctype-public", NULL);
    if (prop != NULL) {
        if (style->doctypePublic != NULL)
            xmlFree(style->doctypePublic);
        style->doctypePublic = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *) "standalone", NULL);
    if (prop != NULL) {
        if (xmlStrEqual(prop, (const xmlChar *) "yes")) {
            style->standalone = 1;
        } else if (xmlStrEqual(prop, (const xmlChar *) "no")) {
            style->standalone = 0;
        } else {
	    xsltTransformError(NULL, style, cur,
                             "invalid value for standalone: %s\n", prop);
            style->errors++;
        }
        xmlFree(prop);
    }

    prop = xmlGetNsProp(cur, (const xmlChar *) "indent", NULL);
    if (prop != NULL) {
        if (xmlStrEqual(prop, (const xmlChar *) "yes")) {
            style->indent = 1;
        } else if (xmlStrEqual(prop, (const xmlChar *) "no")) {
            style->indent = 0;
        } else {
	    xsltTransformError(NULL, style, cur,
                             "invalid value for indent: %s\n", prop);
            style->errors++;
        }
        xmlFree(prop);
    }

    prop = xmlGetNsProp(cur, (const xmlChar *) "omit-xml-declaration", NULL);
    if (prop != NULL) {
        if (xmlStrEqual(prop, (const xmlChar *) "yes")) {
            style->omitXmlDeclaration = 1;
        } else if (xmlStrEqual(prop, (const xmlChar *) "no")) {
            style->omitXmlDeclaration = 0;
        } else {
	    xsltTransformError(NULL, style, cur,
                             "invalid value for omit-xml-declaration: %s\n",
                             prop);
            style->errors++;
        }
        xmlFree(prop);
    }

    elements = xmlGetNsProp(cur, (const xmlChar *) "cdata-section-elements",
	NULL);
    if (elements != NULL) {
        if (style->cdataSection == NULL)
            style->cdataSection = xmlHashCreate(10);
        if (style->cdataSection == NULL) {
            xmlFree(elements);
            return;
        }

        element = elements;
        while (*element != 0) {
            while (IS_BLANK(*element))
                element++;
            if (*element == 0)
                break;
            end = element;
            while ((*end != 0) && (!IS_BLANK(*end)))
                end++;
            element = xmlStrndup(element, end - element);
            if (element) {
#ifdef WITH_XSLT_DEBUG_PARSING
                xsltGenericDebug(xsltGenericDebugContext,
                                 "add cdata section output element %s\n",
                                 element);
#endif
		if (xmlValidateQName(BAD_CAST element, 0) != 0) {
		    xsltTransformError(NULL, style, cur,
			"Attribute 'cdata-section-elements': The value "
			"'%s' is not a valid QName.\n", element);
		    xmlFree(element);
		    style->errors++;
		} else {
		    const xmlChar *URI;

		    /*
		    * TODO: Don't use xsltGetQNameURI().
		    */
		    URI = xsltGetQNameURI(cur, &element);
		    if (element == NULL) {
			/*
			* TODO: We'll report additionally an error
			*  via the stylesheet's error handling.
			*/
			xsltTransformError(NULL, style, cur,
			    "Attribute 'cdata-section-elements': "
			    "Not a valid QName.\n");
			style->errors++;
		    } else {
			xmlNsPtr ns;

			/*
			* XSLT-1.0 "Each QName is expanded into an
			*  expanded-name using the namespace declarations in
			*  effect on the xsl:output element in which the QName
			*  occurs; if there is a default namespace, it is used
			*  for QNames that do not have a prefix"
			* NOTE: Fix of bug #339570.
			*/
			if (URI == NULL) {
			    ns = xmlSearchNs(style->doc, cur, NULL);
			    if (ns != NULL)
				URI = ns->href;
			}
			xmlHashAddEntry2(style->cdataSection, element, URI,
			    (void *) "cdata");
			xmlFree(element);
		    }
		}
            }
            element = end;
        }
        xmlFree(elements);
    }

    prop = xmlGetNsProp(cur, (const xmlChar *) "media-type", NULL);
    if (prop != NULL) {
	if (style->mediaType)
	    xmlFree(style->mediaType);
	style->mediaType = prop;
    }
    if (cur->children != NULL) {
	xsltParseContentError(style, cur->children);
    }
}

/**
 * xsltParseStylesheetDecimalFormat:
 * @style:  the XSLT stylesheet
 * @cur:  the "decimal-format" element
 *
 * <!-- Category: top-level-element -->
 * <xsl:decimal-format
 *   name = qname, decimal-separator = char, grouping-separator = char,
 *   infinity = string, minus-sign = char, NaN = string, percent = char
 *   per-mille = char, zero-digit = char, digit = char,
 * pattern-separator = char />
 *
 * parse an XSLT stylesheet decimal-format element and
 * and record the formatting characteristics
 */
static void
xsltParseStylesheetDecimalFormat(xsltStylesheetPtr style, xmlNodePtr cur)
{
    xmlChar *prop;
    xsltDecimalFormatPtr format;
    xsltDecimalFormatPtr iter;

    if ((cur == NULL) || (style == NULL) || (cur->type != XML_ELEMENT_NODE))
	return;

    format = style->decimalFormat;

    prop = xmlGetNsProp(cur, BAD_CAST("name"), NULL);
    if (prop != NULL) {
        const xmlChar *nsUri;

        if (xmlValidateQName(prop, 0) != 0) {
            xsltTransformError(NULL, style, cur,
                "xsl:decimal-format: Invalid QName '%s'.\n", prop);
	    style->warnings++;
            xmlFree(prop);
            return;
        }
        /*
        * TODO: Don't use xsltGetQNameURI().
        */
        nsUri = xsltGetQNameURI(cur, &prop);
        if (prop == NULL) {
	    style->warnings++;
            return;
        }
	format = xsltDecimalFormatGetByQName(style, nsUri, prop);
	if (format != NULL) {
	    xsltTransformError(NULL, style, cur,
	 "xsltParseStylestyleDecimalFormat: %s already exists\n", prop);
	    style->warnings++;
            xmlFree(prop);
	    return;
	}
	format = xsltNewDecimalFormat(nsUri, prop);
	if (format == NULL) {
	    xsltTransformError(NULL, style, cur,
     "xsltParseStylestyleDecimalFormat: failed creating new decimal-format\n");
	    style->errors++;
            xmlFree(prop);
	    return;
	}
	/* Append new decimal-format structure */
	for (iter = style->decimalFormat; iter->next; iter = iter->next)
	    ;
	if (iter)
	    iter->next = format;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *)"decimal-separator", NULL);
    if (prop != NULL) {
	if (format->decimalPoint != NULL) xmlFree(format->decimalPoint);
	format->decimalPoint  = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *)"grouping-separator", NULL);
    if (prop != NULL) {
	if (format->grouping != NULL) xmlFree(format->grouping);
	format->grouping  = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *)"infinity", NULL);
    if (prop != NULL) {
	if (format->infinity != NULL) xmlFree(format->infinity);
	format->infinity  = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *)"minus-sign", NULL);
    if (prop != NULL) {
	if (format->minusSign != NULL) xmlFree(format->minusSign);
	format->minusSign  = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *)"NaN", NULL);
    if (prop != NULL) {
	if (format->noNumber != NULL) xmlFree(format->noNumber);
	format->noNumber  = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *)"percent", NULL);
    if (prop != NULL) {
	if (format->percent != NULL) xmlFree(format->percent);
	format->percent  = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *)"per-mille", NULL);
    if (prop != NULL) {
	if (format->permille != NULL) xmlFree(format->permille);
	format->permille  = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *)"zero-digit", NULL);
    if (prop != NULL) {
	if (format->zeroDigit != NULL) xmlFree(format->zeroDigit);
	format->zeroDigit  = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *)"digit", NULL);
    if (prop != NULL) {
	if (format->digit != NULL) xmlFree(format->digit);
	format->digit  = prop;
    }

    prop = xmlGetNsProp(cur, (const xmlChar *)"pattern-separator", NULL);
    if (prop != NULL) {
	if (format->patternSeparator != NULL) xmlFree(format->patternSeparator);
	format->patternSeparator  = prop;
    }
    if (cur->children != NULL) {
	xsltParseContentError(style, cur->children);
    }
}

/**
 * xsltParseStylesheetPreserveSpace:
 * @style:  the XSLT stylesheet
 * @cur:  the "preserve-space" element
 *
 * parse an XSLT stylesheet preserve-space element and record
 * elements needing preserving
 */

static void
xsltParseStylesheetPreserveSpace(xsltStylesheetPtr style, xmlNodePtr cur) {
    xmlChar *elements;
    xmlChar *element, *end;

    if ((cur == NULL) || (style == NULL) || (cur->type != XML_ELEMENT_NODE))
	return;

    elements = xmlGetNsProp(cur, (const xmlChar *)"elements", NULL);
    if (elements == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsltParseStylesheetPreserveSpace: missing elements attribute\n");
	if (style != NULL) style->warnings++;
	return;
    }

    if (style->stripSpaces == NULL)
	style->stripSpaces = xmlHashCreate(10);
    if (style->stripSpaces == NULL) {
        xmlFree(elements);
	return;
    }

    element = elements;
    while (*element != 0) {
	while (IS_BLANK(*element)) element++;
	if (*element == 0)
	    break;
        end = element;
	while ((*end != 0) && (!IS_BLANK(*end))) end++;
	element = xmlStrndup(element, end - element);
	if (element) {
#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
		"add preserved space element %s\n", element);
#endif
	    if (xmlStrEqual(element, (const xmlChar *)"*")) {
		style->stripAll = -1;
	    } else {
		const xmlChar *URI;

		/*
		* TODO: Don't use xsltGetQNameURI().
		*/
                URI = xsltGetQNameURI(cur, &element);

		xmlHashAddEntry2(style->stripSpaces, element, URI,
				(xmlChar *) "preserve");
	    }
	    xmlFree(element);
	}
	element = end;
    }
    xmlFree(elements);
    if (cur->children != NULL) {
	xsltParseContentError(style, cur->children);
    }
}

#ifdef XSLT_REFACTORED
#else
/**
 * xsltParseStylesheetExtPrefix:
 * @style:  the XSLT stylesheet
 * @template:  the "extension-element-prefixes" prefix
 *
 * parse an XSLT stylesheet's "extension-element-prefix" attribute value
 * and register the namespaces of extension instruction.
 * SPEC "A namespace is designated as an extension namespace by using
 *   an extension-element-prefixes attribute on:
 *   1) an xsl:stylesheet element
 *   2) an xsl:extension-element-prefixes attribute on a
 *      literal result element
 *   3) an extension instruction."
 */
static void
xsltParseStylesheetExtPrefix(xsltStylesheetPtr style, xmlNodePtr cur,
			     int isXsltElem) {
    xmlChar *prefixes;
    xmlChar *prefix, *end;

    if ((cur == NULL) || (style == NULL) || (cur->type != XML_ELEMENT_NODE))
	return;

    if (isXsltElem) {
	/* For xsl:stylesheet/xsl:transform. */
	prefixes = xmlGetNsProp(cur,
	    (const xmlChar *)"extension-element-prefixes", NULL);
    } else {
	/* For literal result elements and extension instructions. */
	prefixes = xmlGetNsProp(cur,
	    (const xmlChar *)"extension-element-prefixes", XSLT_NAMESPACE);
    }
    if (prefixes == NULL) {
	return;
    }

    prefix = prefixes;
    while (*prefix != 0) {
	while (IS_BLANK(*prefix)) prefix++;
	if (*prefix == 0)
	    break;
        end = prefix;
	while ((*end != 0) && (!IS_BLANK(*end))) end++;
	prefix = xmlStrndup(prefix, end - prefix);
	if (prefix) {
	    xmlNsPtr ns;

	    if (xmlStrEqual(prefix, (const xmlChar *)"#default"))
		ns = xmlSearchNs(style->doc, cur, NULL);
	    else
		ns = xmlSearchNs(style->doc, cur, prefix);
	    if (ns == NULL) {
		xsltTransformError(NULL, style, cur,
	    "xsl:extension-element-prefix : undefined namespace %s\n",
	                         prefix);
		if (style != NULL) style->warnings++;
	    } else {
#ifdef WITH_XSLT_DEBUG_PARSING
		xsltGenericDebug(xsltGenericDebugContext,
		    "add extension prefix %s\n", prefix);
#endif
		xsltRegisterExtPrefix(style, prefix, ns->href);
	    }
	    xmlFree(prefix);
	}
	prefix = end;
    }
    xmlFree(prefixes);
}
#endif /* else of XSLT_REFACTORED */

/**
 * xsltParseStylesheetStripSpace:
 * @style:  the XSLT stylesheet
 * @cur:  the "strip-space" element
 *
 * parse an XSLT stylesheet's strip-space element and record
 * the elements needing stripping
 */

static void
xsltParseStylesheetStripSpace(xsltStylesheetPtr style, xmlNodePtr cur) {
    xmlChar *elements;
    xmlChar *element, *end;

    if ((cur == NULL) || (style == NULL) || (cur->type != XML_ELEMENT_NODE))
	return;

    if (style->stripSpaces == NULL)
	style->stripSpaces = xmlHashCreate(10);
    if (style->stripSpaces == NULL)
	return;

    elements = xmlGetNsProp(cur, (const xmlChar *)"elements", NULL);
    if (elements == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsltParseStylesheetStripSpace: missing elements attribute\n");
	if (style != NULL) style->warnings++;
	return;
    }

    element = elements;
    while (*element != 0) {
	while (IS_BLANK(*element)) element++;
	if (*element == 0)
	    break;
        end = element;
	while ((*end != 0) && (!IS_BLANK(*end))) end++;
	element = xmlStrndup(element, end - element);
	if (element) {
#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
		"add stripped space element %s\n", element);
#endif
	    if (xmlStrEqual(element, (const xmlChar *)"*")) {
		style->stripAll = 1;
	    } else {
		const xmlChar *URI;

		/*
		* TODO: Don't use xsltGetQNameURI().
		*/
                URI = xsltGetQNameURI(cur, &element);

		xmlHashAddEntry2(style->stripSpaces, element, URI,
			        (xmlChar *) "strip");
	    }
	    xmlFree(element);
	}
	element = end;
    }
    xmlFree(elements);
    if (cur->children != NULL) {
	xsltParseContentError(style, cur->children);
    }
}

#ifdef XSLT_REFACTORED
#else
/**
 * xsltParseStylesheetExcludePrefix:
 * @style:  the XSLT stylesheet
 * @cur:  the current point in the stylesheet
 *
 * parse an XSLT stylesheet exclude prefix and record
 * namespaces needing stripping
 *
 * Returns the number of Excluded prefixes added at that level
 */

static int
xsltParseStylesheetExcludePrefix(xsltStylesheetPtr style, xmlNodePtr cur,
				 int isXsltElem)
{
    int nb = 0;
    xmlChar *prefixes;
    xmlChar *prefix, *end;

    if ((cur == NULL) || (style == NULL) || (cur->type != XML_ELEMENT_NODE))
	return(0);

    if (isXsltElem)
	prefixes = xmlGetNsProp(cur,
	    (const xmlChar *)"exclude-result-prefixes", NULL);
    else
	prefixes = xmlGetNsProp(cur,
	    (const xmlChar *)"exclude-result-prefixes", XSLT_NAMESPACE);

    if (prefixes == NULL) {
	return(0);
    }

    prefix = prefixes;
    while (*prefix != 0) {
	while (IS_BLANK(*prefix)) prefix++;
	if (*prefix == 0)
	    break;
        end = prefix;
	while ((*end != 0) && (!IS_BLANK(*end))) end++;
	prefix = xmlStrndup(prefix, end - prefix);
	if (prefix) {
	    xmlNsPtr ns;

	    if (xmlStrEqual(prefix, (const xmlChar *)"#default"))
		ns = xmlSearchNs(style->doc, cur, NULL);
	    else
		ns = xmlSearchNs(style->doc, cur, prefix);
	    if (ns == NULL) {
		xsltTransformError(NULL, style, cur,
	    "xsl:exclude-result-prefixes : undefined namespace %s\n",
	                         prefix);
		if (style != NULL) style->warnings++;
	    } else {
		if (exclPrefixPush(style, (xmlChar *) ns->href) >= 0) {
#ifdef WITH_XSLT_DEBUG_PARSING
		    xsltGenericDebug(xsltGenericDebugContext,
			"exclude result prefix %s\n", prefix);
#endif
		    nb++;
		}
	    }
	    xmlFree(prefix);
	}
	prefix = end;
    }
    xmlFree(prefixes);
    return(nb);
}
#endif /* else of XSLT_REFACTORED */

#ifdef XSLT_REFACTORED

/*
* xsltTreeEnsureXMLDecl:
* @doc: the doc
*
* BIG NOTE:
*  This was copy&pasted from Libxml2's xmlTreeEnsureXMLDecl() in "tree.c".
* Ensures that there is an XML namespace declaration on the doc.
*
* Returns the XML ns-struct or NULL on API and internal errors.
*/
static xmlNsPtr
xsltTreeEnsureXMLDecl(xmlDocPtr doc)
{
    if (doc == NULL)
	return (NULL);
    if (doc->oldNs != NULL)
	return (doc->oldNs);
    {
	xmlNsPtr ns;
	ns = (xmlNsPtr) xmlMalloc(sizeof(xmlNs));
	if (ns == NULL) {
	    xmlGenericError(xmlGenericErrorContext,
		"xsltTreeEnsureXMLDecl: Failed to allocate "
		"the XML namespace.\n");
	    return (NULL);
	}
	memset(ns, 0, sizeof(xmlNs));
	ns->type = XML_LOCAL_NAMESPACE;
	/*
	* URGENT TODO: revisit this.
	*/
#ifdef LIBXML_NAMESPACE_DICT
	if (doc->dict)
	    ns->href = xmlDictLookup(doc->dict, XML_XML_NAMESPACE, -1);
	else
	    ns->href = xmlStrdup(XML_XML_NAMESPACE);
#else
	ns->href = xmlStrdup(XML_XML_NAMESPACE);
#endif
	ns->prefix = xmlStrdup((const xmlChar *)"xml");
	doc->oldNs = ns;
	return (ns);
    }
}

/*
* xsltTreeAcquireStoredNs:
* @doc: the doc
* @nsName: the namespace name
* @prefix: the prefix
*
* BIG NOTE:
*  This was copy&pasted from Libxml2's xmlDOMWrapStoreNs() in "tree.c".
* Creates or reuses an xmlNs struct on doc->oldNs with
* the given prefix and namespace name.
*
* Returns the aquired ns struct or NULL in case of an API
*         or internal error.
*/
static xmlNsPtr
xsltTreeAcquireStoredNs(xmlDocPtr doc,
			const xmlChar *nsName,
			const xmlChar *prefix)
{
    xmlNsPtr ns;

    if (doc == NULL)
	return (NULL);
    if (doc->oldNs != NULL)
	ns = doc->oldNs;
    else
	ns = xsltTreeEnsureXMLDecl(doc);
    if (ns == NULL)
	return (NULL);
    if (ns->next != NULL) {
	/* Reuse. */
	ns = ns->next;
	while (ns != NULL) {
	    if ((ns->prefix == NULL) != (prefix == NULL)) {
		/* NOP */
	    } else if (prefix == NULL) {
		if (xmlStrEqual(ns->href, nsName))
		    return (ns);
	    } else {
		if ((ns->prefix[0] == prefix[0]) &&
		     xmlStrEqual(ns->prefix, prefix) &&
		     xmlStrEqual(ns->href, nsName))
		    return (ns);

	    }
	    if (ns->next == NULL)
		break;
	    ns = ns->next;
	}
    }
    /* Create. */
    ns->next = xmlNewNs(NULL, nsName, prefix);
    return (ns->next);
}

/**
 * xsltLREBuildEffectiveNs:
 *
 * Apply ns-aliasing on the namespace of the given @elem and
 * its attributes.
 */
static int
xsltLREBuildEffectiveNs(xsltCompilerCtxtPtr cctxt,
			xmlNodePtr elem)
{
    xmlNsPtr ns;
    xsltNsAliasPtr alias;

    if ((cctxt == NULL) || (elem == NULL))
	return(-1);
    if ((cctxt->nsAliases == NULL) || (! cctxt->hasNsAliases))
	return(0);

    alias = cctxt->nsAliases;
    while (alias != NULL) {
	if ( /* If both namespaces are NULL... */
	    ( (elem->ns == NULL) &&
	    ((alias->literalNs == NULL) ||
	    (alias->literalNs->href == NULL)) ) ||
	    /* ... or both namespace are equal */
	    ( (elem->ns != NULL) &&
	    (alias->literalNs != NULL) &&
	    xmlStrEqual(elem->ns->href, alias->literalNs->href) ) )
	{
	    if ((alias->targetNs != NULL) &&
		(alias->targetNs->href != NULL))
	    {
		/*
		* Convert namespace.
		*/
		if (elem->doc == alias->docOfTargetNs) {
		    /*
		    * This is the nice case: same docs.
		    * This will eventually assign a ns-decl which
		    * is shadowed, but this has no negative effect on
		    * the generation of the result tree.
		    */
		    elem->ns = alias->targetNs;
		} else {
		    /*
		    * This target xmlNs originates from a different
		    * stylesheet tree. Try to locate it in the
		    * in-scope namespaces.
		    * OPTIMIZE TODO: Use the compiler-node-info inScopeNs.
		    */
		    ns = xmlSearchNs(elem->doc, elem,
			alias->targetNs->prefix);
		    /*
		    * If no matching ns-decl found, then assign a
		    * ns-decl stored in xmlDoc.
		    */
		    if ((ns == NULL) ||
			(! xmlStrEqual(ns->href, alias->targetNs->href)))
		    {
			/*
			* BIG NOTE: The use of xsltTreeAcquireStoredNs()
			*  is not very efficient, but currently I don't
			*  see an other way of *safely* changing a node's
			*  namespace, since the xmlNs struct in
			*  alias->targetNs might come from an other
			*  stylesheet tree. So we need to anchor it in the
			*  current document, without adding it to the tree,
			*  which would otherwise change the in-scope-ns
			*  semantic of the tree.
			*/
			ns = xsltTreeAcquireStoredNs(elem->doc,
			    alias->targetNs->href,
			    alias->targetNs->prefix);

			if (ns == NULL) {
			    xsltTransformError(NULL, cctxt->style, elem,
				"Internal error in "
				"xsltLREBuildEffectiveNs(): "
				"failed to acquire a stored "
				"ns-declaration.\n");
			    cctxt->style->errors++;
			    return(-1);

			}
		    }
		    elem->ns = ns;
		}
	    } else {
		/*
		* Move into or leave in the NULL namespace.
		*/
		elem->ns = NULL;
	    }
	    break;
	}
	alias = alias->next;
    }
    /*
    * Same with attributes of literal result elements.
    */
    if (elem->properties != NULL) {
	xmlAttrPtr attr = elem->properties;

	while (attr != NULL) {
	    if (attr->ns == NULL) {
		attr = attr->next;
		continue;
	    }
	    alias = cctxt->nsAliases;
	    while (alias != NULL) {
		if ( /* If both namespaces are NULL... */
		    ( (elem->ns == NULL) &&
		    ((alias->literalNs == NULL) ||
		    (alias->literalNs->href == NULL)) ) ||
		    /* ... or both namespace are equal */
		    ( (elem->ns != NULL) &&
		    (alias->literalNs != NULL) &&
		    xmlStrEqual(elem->ns->href, alias->literalNs->href) ) )
		{
		    if ((alias->targetNs != NULL) &&
			(alias->targetNs->href != NULL))
		    {
			if (elem->doc == alias->docOfTargetNs) {
			    elem->ns = alias->targetNs;
			} else {
			    ns = xmlSearchNs(elem->doc, elem,
				alias->targetNs->prefix);
			    if ((ns == NULL) ||
				(! xmlStrEqual(ns->href, alias->targetNs->href)))
			    {
				ns = xsltTreeAcquireStoredNs(elem->doc,
				    alias->targetNs->href,
				    alias->targetNs->prefix);

				if (ns == NULL) {
				    xsltTransformError(NULL, cctxt->style, elem,
					"Internal error in "
					"xsltLREBuildEffectiveNs(): "
					"failed to acquire a stored "
					"ns-declaration.\n");
				    cctxt->style->errors++;
				    return(-1);

				}
			    }
			    elem->ns = ns;
			}
		    } else {
		    /*
		    * Move into or leave in the NULL namespace.
			*/
			elem->ns = NULL;
		    }
		    break;
		}
		alias = alias->next;
	    }

	    attr = attr->next;
	}
    }
    return(0);
}

/**
 * xsltLREBuildEffectiveNsNodes:
 *
 * Computes the effective namespaces nodes for a literal result
 * element.
 * @effectiveNs is the set of effective ns-nodes
 *  on the literal result element, which will be added to the result
 *  element if not already existing in the result tree.
 *  This means that excluded namespaces (via exclude-result-prefixes,
 *  extension-element-prefixes and the XSLT namespace) not added
 *  to the set.
 *  Namespace-aliasing was applied on the @effectiveNs.
 */
static int
xsltLREBuildEffectiveNsNodes(xsltCompilerCtxtPtr cctxt,
			     xsltStyleItemLRElementInfoPtr item,
			     xmlNodePtr elem,
			     int isLRE)
{
    xmlNsPtr ns, tmpns;
    xsltEffectiveNsPtr effNs, lastEffNs = NULL;
    int i, j, holdByElem;
    xsltPointerListPtr extElemNs = cctxt->inode->extElemNs;
    xsltPointerListPtr exclResultNs = cctxt->inode->exclResultNs;

    if ((cctxt == NULL) || (cctxt->inode == NULL) || (elem == NULL) ||
	(item == NULL) || (item->effectiveNs != NULL))
	return(-1);

    if (item->inScopeNs == NULL)
	return(0);

    extElemNs = cctxt->inode->extElemNs;
    exclResultNs = cctxt->inode->exclResultNs;

    for (i = 0; i < item->inScopeNs->totalNumber; i++) {
	ns = item->inScopeNs->list[i];
	/*
	* Skip namespaces designated as excluded namespaces
	* -------------------------------------------------
	*
	* XSLT-20 TODO: In XSLT 2.0 we need to keep namespaces
	*  which are target namespaces of namespace-aliases
	*  regardless if designated as excluded.
	*
	* Exclude the XSLT namespace.
	*/
	if (xmlStrEqual(ns->href, XSLT_NAMESPACE))
	    goto skip_ns;

	/*
	* Apply namespace aliasing
	* ------------------------
	*
	* SPEC XSLT 2.0
	*  "- A namespace node whose string value is a literal namespace
	*     URI is not copied to the result tree.
	*   - A namespace node whose string value is a target namespace URI
	*     is copied to the result tree, whether or not the URI
	*     identifies an excluded namespace."
	*
	* NOTE: The ns-aliasing machanism is non-cascading.
	*  (checked with Saxon, Xalan and MSXML .NET).
	* URGENT TODO: is style->nsAliases the effective list of
	*  ns-aliases, or do we need to lookup the whole
	*  import-tree?
	* TODO: Get rid of import-tree lookup.
	*/
	if (cctxt->hasNsAliases) {
	    xsltNsAliasPtr alias;
	    /*
	    * First check for being a target namespace.
	    */
	    alias = cctxt->nsAliases;
	    do {
		/*
		* TODO: Is xmlns="" handled already?
		*/
		if ((alias->targetNs != NULL) &&
		    (xmlStrEqual(alias->targetNs->href, ns->href)))
		{
		    /*
		    * Recognized as a target namespace; use it regardless
		    * if excluded otherwise.
		    */
		    goto add_effective_ns;
		}
		alias = alias->next;
	    } while (alias != NULL);

	    alias = cctxt->nsAliases;
	    do {
		/*
		* TODO: Is xmlns="" handled already?
		*/
		if ((alias->literalNs != NULL) &&
		    (xmlStrEqual(alias->literalNs->href, ns->href)))
		{
		    /*
		    * Recognized as an namespace alias; do not use it.
		    */
		    goto skip_ns;
		}
		alias = alias->next;
	    } while (alias != NULL);
	}

	/*
	* Exclude excluded result namespaces.
	*/
	if (exclResultNs) {
	    for (j = 0; j < exclResultNs->number; j++)
		if (xmlStrEqual(ns->href, BAD_CAST exclResultNs->items[j]))
		    goto skip_ns;
	}
	/*
	* Exclude extension-element namespaces.
	*/
	if (extElemNs) {
	    for (j = 0; j < extElemNs->number; j++)
		if (xmlStrEqual(ns->href, BAD_CAST extElemNs->items[j]))
		    goto skip_ns;
	}

add_effective_ns:
	/*
	* OPTIMIZE TODO: This information may not be needed.
	*/
	if (isLRE && (elem->nsDef != NULL)) {
	    holdByElem = 0;
	    tmpns = elem->nsDef;
	    do {
		if (tmpns == ns) {
		    holdByElem = 1;
		    break;
		}
		tmpns = tmpns->next;
	    } while (tmpns != NULL);
	} else
	    holdByElem = 0;


	/*
	* Add the effective namespace declaration.
	*/
	effNs = (xsltEffectiveNsPtr) xmlMalloc(sizeof(xsltEffectiveNs));
	if (effNs == NULL) {
	    xsltTransformError(NULL, cctxt->style, elem,
		"Internal error in xsltLREBuildEffectiveNs(): "
		"failed to allocate memory.\n");
	    cctxt->style->errors++;
	    return(-1);
	}
	if (cctxt->psData->effectiveNs == NULL) {
	    cctxt->psData->effectiveNs = effNs;
	    effNs->nextInStore = NULL;
	} else {
	    effNs->nextInStore = cctxt->psData->effectiveNs;
	    cctxt->psData->effectiveNs = effNs;
	}

	effNs->next = NULL;
	effNs->prefix = ns->prefix;
	effNs->nsName = ns->href;
	effNs->holdByElem = holdByElem;

	if (lastEffNs == NULL)
	    item->effectiveNs = effNs;
	else
	    lastEffNs->next = effNs;
	lastEffNs = effNs;

skip_ns:
	{}
    }
    return(0);
}


/**
 * xsltLREInfoCreate:
 *
 * @isLRE: indicates if the given @elem is a literal result element
 *
 * Creates a new info for a literal result element.
 */
static int
xsltLREInfoCreate(xsltCompilerCtxtPtr cctxt,
		  xmlNodePtr elem,
		  int isLRE)
{
    xsltStyleItemLRElementInfoPtr item;

    if ((cctxt == NULL) || (cctxt->inode == NULL))
	return(-1);

    item = (xsltStyleItemLRElementInfoPtr)
	xmlMalloc(sizeof(xsltStyleItemLRElementInfo));
    if (item == NULL) {
	xsltTransformError(NULL, cctxt->style, NULL,
	    "Internal error in xsltLREInfoCreate(): "
	    "memory allocation failed.\n");
	cctxt->style->errors++;
	return(-1);
    }
    memset(item, 0, sizeof(xsltStyleItemLRElementInfo));
    item->type = XSLT_FUNC_LITERAL_RESULT_ELEMENT;
    /*
    * Store it in the stylesheet.
    */
    item->next = cctxt->style->preComps;
    cctxt->style->preComps = (xsltElemPreCompPtr) item;
    /*
    * @inScopeNs are used for execution of XPath expressions
    *  in AVTs.
    */
    item->inScopeNs = cctxt->inode->inScopeNs;

    if (elem)
	xsltLREBuildEffectiveNsNodes(cctxt, item, elem, isLRE);

    cctxt->inode->litResElemInfo = item;
    cctxt->inode->nsChanged = 0;
    cctxt->maxLREs++;
    return(0);
}

/**
 * xsltCompilerVarInfoPush:
 * @cctxt: the compilation context
 *
 * Pushes a new var/param info onto the stack.
 *
 * Returns the acquired variable info.
 */
static xsltVarInfoPtr
xsltCompilerVarInfoPush(xsltCompilerCtxtPtr cctxt,
				  xmlNodePtr inst,
				  const xmlChar *name,
				  const xmlChar *nsName)
{
    xsltVarInfoPtr ivar;

    if ((cctxt->ivar != NULL) && (cctxt->ivar->next != NULL)) {
	ivar = cctxt->ivar->next;
    } else if ((cctxt->ivar == NULL) && (cctxt->ivars != NULL)) {
	ivar = cctxt->ivars;
    } else {
	ivar = (xsltVarInfoPtr) xmlMalloc(sizeof(xsltVarInfo));
	if (ivar == NULL) {
	    xsltTransformError(NULL, cctxt->style, inst,
		"xsltParseInScopeVarPush: xmlMalloc() failed!\n");
	    cctxt->style->errors++;
	    return(NULL);
	}
	/* memset(retVar, 0, sizeof(xsltInScopeVar)); */
	if (cctxt->ivars == NULL) {
	    cctxt->ivars = ivar;
	    ivar->prev = NULL;
	} else {
	    cctxt->ivar->next = ivar;
	    ivar->prev = cctxt->ivar;
	}
	cctxt->ivar = ivar;
	ivar->next = NULL;
    }
    ivar->depth = cctxt->depth;
    ivar->name = name;
    ivar->nsName = nsName;
    return(ivar);
}

/**
 * xsltCompilerVarInfoPop:
 * @cctxt: the compilation context
 *
 * Pops all var/param infos from the stack, which
 * have the current depth.
 */
static void
xsltCompilerVarInfoPop(xsltCompilerCtxtPtr cctxt)
{

    while ((cctxt->ivar != NULL) &&
	(cctxt->ivar->depth > cctxt->depth))
    {
	cctxt->ivar = cctxt->ivar->prev;
    }
}

/*
* xsltCompilerNodePush:
*
* @cctxt: the compilation context
* @node: the node to be pushed (this can also be the doc-node)
*
*
*
* Returns the current node info structure or
*         NULL in case of an internal error.
*/
static xsltCompilerNodeInfoPtr
xsltCompilerNodePush(xsltCompilerCtxtPtr cctxt, xmlNodePtr node)
{
    xsltCompilerNodeInfoPtr inode, iprev;

    if ((cctxt->inode != NULL) && (cctxt->inode->next != NULL)) {
	inode = cctxt->inode->next;
    } else if ((cctxt->inode == NULL) && (cctxt->inodeList != NULL)) {
	inode = cctxt->inodeList;
    } else {
	/*
	* Create a new node-info.
	*/
	inode = (xsltCompilerNodeInfoPtr)
	    xmlMalloc(sizeof(xsltCompilerNodeInfo));
	if (inode == NULL) {
	    xsltTransformError(NULL, cctxt->style, NULL,
		"xsltCompilerNodePush: malloc failed.\n");
	    return(NULL);
	}
	memset(inode, 0, sizeof(xsltCompilerNodeInfo));
	if (cctxt->inodeList == NULL)
	    cctxt->inodeList = inode;
	else {
	    cctxt->inodeLast->next = inode;
	    inode->prev = cctxt->inodeLast;
	}
	cctxt->inodeLast = inode;
	cctxt->maxNodeInfos++;
	if (cctxt->inode == NULL) {
	    cctxt->inode = inode;
	    /*
	    * Create an initial literal result element info for
	    * the root of the stylesheet.
	    */
	    xsltLREInfoCreate(cctxt, NULL, 0);
	}
    }
    cctxt->depth++;
    cctxt->inode = inode;
    /*
    * REVISIT TODO: Keep the reset always complete.
    * NOTE: Be carefull with the @node, since it might be
    *  a doc-node.
    */
    inode->node = node;
    inode->depth = cctxt->depth;
    inode->templ = NULL;
    inode->category = XSLT_ELEMENT_CATEGORY_XSLT;
    inode->type = 0;
    inode->item = NULL;
    inode->curChildType = 0;
    inode->extContentHandled = 0;
    inode->isRoot = 0;

    if (inode->prev != NULL) {
	iprev = inode->prev;
	/*
	* Inherit the following information:
	* ---------------------------------
	*
	* In-scope namespaces
	*/
	inode->inScopeNs = iprev->inScopeNs;
	/*
	* Info for literal result elements
	*/
	inode->litResElemInfo = iprev->litResElemInfo;
	inode->nsChanged = iprev->nsChanged;
	/*
	* Excluded result namespaces
	*/
	inode->exclResultNs = iprev->exclResultNs;
	/*
	* Extension instruction namespaces
	*/
	inode->extElemNs = iprev->extElemNs;
	/*
	* Whitespace preservation
	*/
	inode->preserveWhitespace = iprev->preserveWhitespace;
	/*
	* Forwards-compatible mode
	*/
	inode->forwardsCompat = iprev->forwardsCompat;
    } else {
	inode->inScopeNs = NULL;
	inode->exclResultNs = NULL;
	inode->extElemNs = NULL;
	inode->preserveWhitespace = 0;
	inode->forwardsCompat = 0;
    }

    return(inode);
}

/*
* xsltCompilerNodePop:
*
* @cctxt: the compilation context
* @node: the node to be pushed (this can also be the doc-node)
*
* Pops the current node info.
*/
static void
xsltCompilerNodePop(xsltCompilerCtxtPtr cctxt, xmlNodePtr node)
{
    if (cctxt->inode == NULL) {
	xmlGenericError(xmlGenericErrorContext,
	    "xsltCompilerNodePop: Top-node mismatch.\n");
	return;
    }
    /*
    * NOTE: Be carefull with the @node, since it might be
    *  a doc-node.
    */
    if (cctxt->inode->node != node) {
	xmlGenericError(xmlGenericErrorContext,
	"xsltCompilerNodePop: Node mismatch.\n");
	goto mismatch;
    }
    if (cctxt->inode->depth != cctxt->depth) {
	xmlGenericError(xmlGenericErrorContext,
	"xsltCompilerNodePop: Depth mismatch.\n");
	goto mismatch;
    }
    cctxt->depth--;
    /*
    * Pop information of variables.
    */
    if ((cctxt->ivar) && (cctxt->ivar->depth > cctxt->depth))
	xsltCompilerVarInfoPop(cctxt);

    cctxt->inode = cctxt->inode->prev;
    if (cctxt->inode != NULL)
	cctxt->inode->curChildType = 0;
    return;

mismatch:
    {
	const xmlChar *nsName = NULL, *name = NULL;
	const xmlChar *infnsName = NULL, *infname = NULL;

	if (node) {
	    if (node->type == XML_ELEMENT_NODE) {
		name = node->name;
		if (node->ns != NULL)
		    nsName = node->ns->href;
		else
		    nsName = BAD_CAST "";
	    } else {
		name = BAD_CAST "#document";
		nsName = BAD_CAST "";
	    }
	} else
	    name = BAD_CAST "Not given";

	if (cctxt->inode->node) {
	    if (node->type == XML_ELEMENT_NODE) {
		infname = cctxt->inode->node->name;
		if (cctxt->inode->node->ns != NULL)
		    infnsName = cctxt->inode->node->ns->href;
		else
		    infnsName = BAD_CAST "";
	    } else {
		infname = BAD_CAST "#document";
		infnsName = BAD_CAST "";
	    }
	} else
	    infname = BAD_CAST "Not given";


	xmlGenericError(xmlGenericErrorContext,
	    "xsltCompilerNodePop: Given   : '%s' URI '%s'\n",
	    name, nsName);
	xmlGenericError(xmlGenericErrorContext,
	    "xsltCompilerNodePop: Expected: '%s' URI '%s'\n",
	    infname, infnsName);
    }
}

/*
* xsltCompilerBuildInScopeNsList:
*
* Create and store the list of in-scope namespaces for the given
* node in the stylesheet. If there are no changes in the in-scope
* namespaces then the last ns-info of the ancestor axis will be returned.
* Compilation-time only.
*
* Returns the ns-info or NULL if there are no namespaces in scope.
*/
static xsltNsListContainerPtr
xsltCompilerBuildInScopeNsList(xsltCompilerCtxtPtr cctxt, xmlNodePtr node)
{
    xsltNsListContainerPtr nsi = NULL;
    xmlNsPtr *list = NULL, ns;
    int i, maxns = 5;
    /*
    * Create a new ns-list for this position in the node-tree.
    * xmlGetNsList() will return NULL, if there are no ns-decls in the
    * tree. Note that the ns-decl for the XML namespace is not added
    * to the resulting list; the XPath module handles the XML namespace
    * internally.
    */
    while (node != NULL) {
        if (node->type == XML_ELEMENT_NODE) {
            ns = node->nsDef;
            while (ns != NULL) {
                if (nsi == NULL) {
		    nsi = (xsltNsListContainerPtr)
			xmlMalloc(sizeof(xsltNsListContainer));
		    if (nsi == NULL) {
			xsltTransformError(NULL, cctxt->style, NULL,
			    "xsltCompilerBuildInScopeNsList: "
			    "malloc failed!\n");
			goto internal_err;
		    }
		    memset(nsi, 0, sizeof(xsltNsListContainer));
                    nsi->list =
                        (xmlNsPtr *) xmlMalloc(maxns * sizeof(xmlNsPtr));
                    if (nsi->list == NULL) {
			xsltTransformError(NULL, cctxt->style, NULL,
			    "xsltCompilerBuildInScopeNsList: "
			    "malloc failed!\n");
			goto internal_err;
                    }
                    nsi->list[0] = NULL;
                }
		/*
		* Skip shadowed namespace bindings.
		*/
                for (i = 0; i < nsi->totalNumber; i++) {
                    if ((ns->prefix == nsi->list[i]->prefix) ||
                        (xmlStrEqual(ns->prefix, nsi->list[i]->prefix)))
		    break;
                }
                if (i >= nsi->totalNumber) {
                    if (nsi->totalNumber +1 >= maxns) {
                        maxns *= 2;
			nsi->list =
			    (xmlNsPtr *) xmlRealloc(nsi->list,
				maxns * sizeof(xmlNsPtr));
                        if (nsi->list == NULL) {
                            xsltTransformError(NULL, cctxt->style, NULL,
				"xsltCompilerBuildInScopeNsList: "
				"realloc failed!\n");
				goto internal_err;
                        }
                    }
                    nsi->list[nsi->totalNumber++] = ns;
                    nsi->list[nsi->totalNumber] = NULL;
                }

                ns = ns->next;
            }
        }
        node = node->parent;
    }
    if (nsi == NULL)
	return(NULL);
    /*
    * Move the default namespace to last position.
    */
    nsi->xpathNumber = nsi->totalNumber;
    for (i = 0; i < nsi->totalNumber; i++) {
	if (nsi->list[i]->prefix == NULL) {
	    ns = nsi->list[i];
	    nsi->list[i] = nsi->list[nsi->totalNumber-1];
	    nsi->list[nsi->totalNumber-1] = ns;
	    nsi->xpathNumber--;
	    break;
	}
    }
    /*
    * Store the ns-list in the stylesheet.
    */
    if (xsltPointerListAddSize(
	(xsltPointerListPtr)cctxt->psData->inScopeNamespaces,
	(void *) nsi, 5) == -1)
    {
	xmlFree(nsi);
	nsi = NULL;
	xsltTransformError(NULL, cctxt->style, NULL,
	    "xsltCompilerBuildInScopeNsList: failed to add ns-info.\n");
	goto internal_err;
    }
    /*
    * Notify of change in status wrt namespaces.
    */
    if (cctxt->inode != NULL)
	cctxt->inode->nsChanged = 1;

    return(nsi);

internal_err:
    if (list != NULL)
	xmlFree(list);
    cctxt->style->errors++;
    return(NULL);
}

static int
xsltParseNsPrefixList(xsltCompilerCtxtPtr cctxt,
		      xsltPointerListPtr list,
		      xmlNodePtr node,
		      const xmlChar *value)
{
    xmlChar *cur, *end;
    xmlNsPtr ns;

    if ((cctxt == NULL) || (value == NULL) || (list == NULL))
	return(-1);

    list->number = 0;

    cur = (xmlChar *) value;
    while (*cur != 0) {
	while (IS_BLANK(*cur)) cur++;
	if (*cur == 0)
	    break;
	end = cur;
	while ((*end != 0) && (!IS_BLANK(*end))) end++;
	cur = xmlStrndup(cur, end - cur);
	if (cur == NULL) {
	    cur = end;
	    continue;
	}
	/*
	* TODO: Export and use xmlSearchNsByPrefixStrict()
	*   in Libxml2, tree.c, since xmlSearchNs() is in most
	*   cases not efficient and in some cases not correct.
	*
	* XSLT-2 TODO: XSLT 2.0 allows an additional "#all" value.
	*/
	if ((cur[0] == '#') &&
	    xmlStrEqual(cur, (const xmlChar *)"#default"))
	    ns = xmlSearchNs(cctxt->style->doc, node, NULL);
	else
	    ns = xmlSearchNs(cctxt->style->doc, node, cur);

	if (ns == NULL) {
	    /*
	    * TODO: Better to report the attr-node, otherwise
	    *  the user won't know which attribute was invalid.
	    */
	    xsltTransformError(NULL, cctxt->style, node,
		"No namespace binding in scope for prefix '%s'.\n", cur);
	    /*
	    * XSLT-1.0: "It is an error if there is no namespace
	    *  bound to the prefix on the element bearing the
	    *  exclude-result-prefixes or xsl:exclude-result-prefixes
	    *  attribute."
	    */
	    cctxt->style->errors++;
	} else {
#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
		"resolved prefix '%s'\n", cur);
#endif
	    /*
	    * Note that we put the namespace name into the dict.
	    */
	    if (xsltPointerListAddSize(list,
		(void *) xmlDictLookup(cctxt->style->dict,
		ns->href, -1), 5) == -1)
	    {
		xmlFree(cur);
		goto internal_err;
	    }
	}
	xmlFree(cur);

	cur = end;
    }
    return(0);

internal_err:
    cctxt->style->errors++;
    return(-1);
}

/**
 * xsltCompilerUtilsCreateMergedList:
 * @dest: the destination list (optional)
 * @first: the first list
 * @second: the second list (optional)
 *
 * Appends the content of @second to @first into @destination.
 * If @destination is NULL a new list will be created.
 *
 * Returns the merged list of items or NULL if there's nothing to merge.
 */
static xsltPointerListPtr
xsltCompilerUtilsCreateMergedList(xsltPointerListPtr first,
			    xsltPointerListPtr second)
{
    xsltPointerListPtr ret;
    size_t num;

    if (first)
	num = first->number;
    else
	num = 0;
    if (second)
	num += second->number;
    if (num == 0)
	return(NULL);
    ret = xsltPointerListCreate(num);
    if (ret == NULL)
	return(NULL);
    /*
    * Copy contents.
    */
    if ((first != NULL) &&  (first->number != 0)) {
	memcpy(ret->items, first->items,
	    first->number * sizeof(void *));
	if ((second != NULL) && (second->number != 0))
	    memcpy(ret->items + first->number, second->items,
		second->number * sizeof(void *));
    } else if ((second != NULL) && (second->number != 0))
	memcpy(ret->items, (void *) second->items,
	    second->number * sizeof(void *));
    ret->number = num;
    return(ret);
}

/*
* xsltParseExclResultPrefixes:
*
* Create and store the list of in-scope namespaces for the given
* node in the stylesheet. If there are no changes in the in-scope
* namespaces then the last ns-info of the ancestor axis will be returned.
* Compilation-time only.
*
* Returns the ns-info or NULL if there are no namespaces in scope.
*/
static xsltPointerListPtr
xsltParseExclResultPrefixes(xsltCompilerCtxtPtr cctxt, xmlNodePtr node,
			    xsltPointerListPtr def,
			    int instrCategory)
{
    xsltPointerListPtr list = NULL;
    xmlChar *value;
    xmlAttrPtr attr;

    if ((cctxt == NULL) || (node == NULL))
	return(NULL);

    if (instrCategory == XSLT_ELEMENT_CATEGORY_XSLT)
	attr = xmlHasNsProp(node, BAD_CAST "exclude-result-prefixes", NULL);
    else
	attr = xmlHasNsProp(node, BAD_CAST "exclude-result-prefixes",
	    XSLT_NAMESPACE);
    if (attr == NULL)
	return(def);

    if (attr && (instrCategory == XSLT_ELEMENT_CATEGORY_LRE)) {
	/*
	* Mark the XSLT attr.
	*/
	attr->psvi = (void *) xsltXSLTAttrMarker;
    }

    if ((attr->children != NULL) &&
	(attr->children->content != NULL))
	value = attr->children->content;
    else {
	xsltTransformError(NULL, cctxt->style, node,
	    "Attribute 'exclude-result-prefixes': Invalid value.\n");
	cctxt->style->errors++;
	return(def);
    }

    if (xsltParseNsPrefixList(cctxt, cctxt->tmpList, node,
	BAD_CAST value) != 0)
	goto exit;
    if (cctxt->tmpList->number == 0)
	goto exit;
    /*
    * Merge the list with the inherited list.
    */
    list = xsltCompilerUtilsCreateMergedList(def, cctxt->tmpList);
    if (list == NULL)
	goto exit;
    /*
    * Store the list in the stylesheet/compiler context.
    */
    if (xsltPointerListAddSize(
	cctxt->psData->exclResultNamespaces, list, 5) == -1)
    {
	xsltPointerListFree(list);
	list = NULL;
	goto exit;
    }
    /*
    * Notify of change in status wrt namespaces.
    */
    if (cctxt->inode != NULL)
	cctxt->inode->nsChanged = 1;

exit:
    if (list != NULL)
	return(list);
    else
	return(def);
}

/*
* xsltParseExtElemPrefixes:
*
* Create and store the list of in-scope namespaces for the given
* node in the stylesheet. If there are no changes in the in-scope
* namespaces then the last ns-info of the ancestor axis will be returned.
* Compilation-time only.
*
* Returns the ns-info or NULL if there are no namespaces in scope.
*/
static xsltPointerListPtr
xsltParseExtElemPrefixes(xsltCompilerCtxtPtr cctxt, xmlNodePtr node,
			 xsltPointerListPtr def,
			 int instrCategory)
{
    xsltPointerListPtr list = NULL;
    xmlAttrPtr attr;
    xmlChar *value;
    int i;

    if ((cctxt == NULL) || (node == NULL))
	return(NULL);

    if (instrCategory == XSLT_ELEMENT_CATEGORY_XSLT)
	attr = xmlHasNsProp(node, BAD_CAST "extension-element-prefixes", NULL);
    else
	attr = xmlHasNsProp(node, BAD_CAST "extension-element-prefixes",
	    XSLT_NAMESPACE);
    if (attr == NULL)
	return(def);

    if (attr && (instrCategory == XSLT_ELEMENT_CATEGORY_LRE)) {
	/*
	* Mark the XSLT attr.
	*/
	attr->psvi = (void *) xsltXSLTAttrMarker;
    }

    if ((attr->children != NULL) &&
	(attr->children->content != NULL))
	value = attr->children->content;
    else {
	xsltTransformError(NULL, cctxt->style, node,
	    "Attribute 'extension-element-prefixes': Invalid value.\n");
	cctxt->style->errors++;
	return(def);
    }


    if (xsltParseNsPrefixList(cctxt, cctxt->tmpList, node,
	BAD_CAST value) != 0)
	goto exit;

    if (cctxt->tmpList->number == 0)
	goto exit;
    /*
    * REVISIT: Register the extension namespaces.
    */
    for (i = 0; i < cctxt->tmpList->number; i++)
	xsltRegisterExtPrefix(cctxt->style, NULL,
	BAD_CAST cctxt->tmpList->items[i]);
    /*
    * Merge the list with the inherited list.
    */
    list = xsltCompilerUtilsCreateMergedList(def, cctxt->tmpList);
    if (list == NULL)
	goto exit;
    /*
    * Store the list in the stylesheet.
    */
    if (xsltPointerListAddSize(
	cctxt->psData->extElemNamespaces, list, 5) == -1)
    {
	xsltPointerListFree(list);
	list = NULL;
	goto exit;
    }
    /*
    * Notify of change in status wrt namespaces.
    */
    if (cctxt->inode != NULL)
	cctxt->inode->nsChanged = 1;

exit:
    if (list != NULL)
	return(list);
    else
	return(def);
}

/*
* xsltParseAttrXSLTVersion:
*
* @cctxt: the compilation context
* @node: the element-node
* @isXsltElem: whether this is an XSLT element
*
* Parses the attribute xsl:version.
*
* Returns 1 if there was such an attribute, 0 if not and
*         -1 if an internal or API error occured.
*/
static int
xsltParseAttrXSLTVersion(xsltCompilerCtxtPtr cctxt, xmlNodePtr node,
			 int instrCategory)
{
    xmlChar *value;
    xmlAttrPtr attr;

    if ((cctxt == NULL) || (node == NULL))
	return(-1);

    if (instrCategory == XSLT_ELEMENT_CATEGORY_XSLT)
	attr = xmlHasNsProp(node, BAD_CAST "version", NULL);
    else
	attr = xmlHasNsProp(node, BAD_CAST "version", XSLT_NAMESPACE);

    if (attr == NULL)
	return(0);

    attr->psvi = (void *) xsltXSLTAttrMarker;

    if ((attr->children != NULL) &&
	(attr->children->content != NULL))
	value = attr->children->content;
    else {
	xsltTransformError(NULL, cctxt->style, node,
	    "Attribute 'version': Invalid value.\n");
	cctxt->style->errors++;
	return(1);
    }

    if (! xmlStrEqual(value, (const xmlChar *)"1.0")) {
	cctxt->inode->forwardsCompat = 1;
	/*
	* TODO: To what extent do we support the
	*  forwards-compatible mode?
	*/
	/*
	* Report this only once per compilation episode.
	*/
	if (! cctxt->hasForwardsCompat) {
	    cctxt->hasForwardsCompat = 1;
	    cctxt->errSeverity = XSLT_ERROR_SEVERITY_WARNING;
	    xsltTransformError(NULL, cctxt->style, node,
		"Warning: the attribute xsl:version specifies a value "
		"different from '1.0'. Switching to forwards-compatible "
		"mode. Only features of XSLT 1.0 are supported by this "
		"processor.\n");
	    cctxt->style->warnings++;
	    cctxt->errSeverity = XSLT_ERROR_SEVERITY_ERROR;
	}
    } else {
	cctxt->inode->forwardsCompat = 0;
    }

    if (attr && (instrCategory == XSLT_ELEMENT_CATEGORY_LRE)) {
	/*
	* Set a marker on XSLT attributes.
	*/
	attr->psvi = (void *) xsltXSLTAttrMarker;
    }
    return(1);
}

static int
xsltParsePreprocessStylesheetTree(xsltCompilerCtxtPtr cctxt, xmlNodePtr node)
{
    xmlNodePtr deleteNode, cur, txt, textNode = NULL;
    xmlDocPtr doc;
    xsltStylesheetPtr style;
    int internalize = 0, findSpaceAttr;
    int xsltStylesheetElemDepth;
    xmlAttrPtr attr;
    xmlChar *value;
    const xmlChar *name, *nsNameXSLT = NULL;
    int strictWhitespace, inXSLText = 0;
#ifdef XSLT_REFACTORED_XSLT_NSCOMP
    xsltNsMapPtr nsMapItem;
#endif

    if ((cctxt == NULL) || (cctxt->style == NULL) ||
	(node == NULL) || (node->type != XML_ELEMENT_NODE))
        return(-1);

    doc = node->doc;
    if (doc == NULL)
	goto internal_err;

    style = cctxt->style;
    if ((style->dict != NULL) && (doc->dict == style->dict))
	internalize = 1;
    else
        style->internalized = 0;

    /*
    * Init value of xml:space. Since this might be an embedded
    * stylesheet, this is needed to be performed on the element
    * where the stylesheet is rooted at, taking xml:space of
    * ancestors into account.
    */
    if (! cctxt->simplified)
	xsltStylesheetElemDepth = cctxt->depth +1;
    else
	xsltStylesheetElemDepth = 0;

    if (xmlNodeGetSpacePreserve(node) != 1)
	cctxt->inode->preserveWhitespace = 0;
    else
	cctxt->inode->preserveWhitespace = 1;

    /*
    * Eval if we should keep the old incorrect behaviour.
    */
    strictWhitespace = (cctxt->strict != 0) ? 1 : 0;

    nsNameXSLT = xsltConstNamespaceNameXSLT;

    deleteNode = NULL;
    cur = node;
    while (cur != NULL) {
	if (deleteNode != NULL)	{

#ifdef WITH_XSLT_DEBUG_BLANKS
	    xsltGenericDebug(xsltGenericDebugContext,
	     "xsltParsePreprocessStylesheetTree: removing node\n");
#endif
	    xmlUnlinkNode(deleteNode);
	    xmlFreeNode(deleteNode);
	    deleteNode = NULL;
	}
	if (cur->type == XML_ELEMENT_NODE) {

	    /*
	    * Clear the PSVI field.
	    */
	    cur->psvi = NULL;

	    xsltCompilerNodePush(cctxt, cur);

	    inXSLText = 0;
	    textNode = NULL;
	    findSpaceAttr = 1;
	    cctxt->inode->stripWhitespace = 0;
	    /*
	    * TODO: I'd love to use a string pointer comparison here :-/
	    */
	    if (IS_XSLT_ELEM(cur)) {
#ifdef XSLT_REFACTORED_XSLT_NSCOMP
		if (cur->ns->href != nsNameXSLT) {
		    nsMapItem = xsltNewNamespaceMapItem(cctxt,
			doc, cur->ns, cur);
		    if (nsMapItem == NULL)
			goto internal_err;
		    cur->ns->href = nsNameXSLT;
		}
#endif

		if (cur->name == NULL)
		    goto process_attributes;
		/*
		* Mark the XSLT element for later recognition.
		* TODO: Using the marker is still too dangerous, since if
		*   the parsing mechanism leaves out an XSLT element, then
		*   this might hit the transformation-mechanism, which
		*   will break if it doesn't expect such a marker.
		*/
		/* cur->psvi = (void *) xsltXSLTElemMarker; */

		/*
		* XSLT 2.0: "Any whitespace text node whose parent is
		* one of the following elements is removed from the "
		* tree, regardless of any xml:space attributes:..."
		* xsl:apply-imports,
		* xsl:apply-templates,
		* xsl:attribute-set,
		* xsl:call-template,
		* xsl:choose,
		* xsl:stylesheet, xsl:transform.
		* XSLT 2.0: xsl:analyze-string,
		*           xsl:character-map,
		*           xsl:next-match
		*
		* TODO: I'd love to use a string pointer comparison here :-/
		*/
		name = cur->name;
		switch (*name) {
		    case 't':
			if ((name[0] == 't') && (name[1] == 'e') &&
			    (name[2] == 'x') && (name[3] == 't') &&
			    (name[4] == 0))
			{
			    /*
			    * Process the xsl:text element.
			    * ----------------------------
			    * Mark it for later recognition.
			    */
			    cur->psvi = (void *) xsltXSLTTextMarker;
			    /*
			    * For stylesheets, the set of
			    * whitespace-preserving element names
			    * consists of just xsl:text.
			    */
			    findSpaceAttr = 0;
			    cctxt->inode->preserveWhitespace = 1;
			    inXSLText = 1;
			}
			break;
		    case 'c':
			if (xmlStrEqual(name, BAD_CAST "choose") ||
			    xmlStrEqual(name, BAD_CAST "call-template"))
			    cctxt->inode->stripWhitespace = 1;
			break;
		    case 'a':
			if (xmlStrEqual(name, BAD_CAST "apply-templates") ||
			    xmlStrEqual(name, BAD_CAST "apply-imports") ||
			    xmlStrEqual(name, BAD_CAST "attribute-set"))

			    cctxt->inode->stripWhitespace = 1;
			break;
		    default:
			if (xsltStylesheetElemDepth == cctxt->depth) {
			    /*
			    * This is a xsl:stylesheet/xsl:transform.
			    */
			    cctxt->inode->stripWhitespace = 1;
			    break;
			}

			if ((cur->prev != NULL) &&
			    (cur->prev->type == XML_TEXT_NODE))
			{
			    /*
			    * XSLT 2.0 : "Any whitespace text node whose
			    *  following-sibling node is an xsl:param or
			    *  xsl:sort element is removed from the tree,
			    *  regardless of any xml:space attributes."
			    */
			    if (((*name == 'p') || (*name == 's')) &&
				(xmlStrEqual(name, BAD_CAST "param") ||
				 xmlStrEqual(name, BAD_CAST "sort")))
			    {
				do {
				    if (IS_BLANK_NODE(cur->prev)) {
					txt = cur->prev;
					xmlUnlinkNode(txt);
					xmlFreeNode(txt);
				    } else {
					/*
					* This will result in a content
					* error, when hitting the parsing
					* functions.
					*/
					break;
				    }
				} while (cur->prev);
			    }
			}
			break;
		}
	    }

process_attributes:
	    /*
	    * Process attributes.
	    * ------------------
	    */
	    if (cur->properties != NULL) {
		if (cur->children == NULL)
		    findSpaceAttr = 0;
		attr = cur->properties;
		do {
#ifdef XSLT_REFACTORED_XSLT_NSCOMP
		    if ((attr->ns) && (attr->ns->href != nsNameXSLT) &&
			xmlStrEqual(attr->ns->href, nsNameXSLT))
		    {
			nsMapItem = xsltNewNamespaceMapItem(cctxt,
			    doc, attr->ns, cur);
			if (nsMapItem == NULL)
			    goto internal_err;
			attr->ns->href = nsNameXSLT;
		    }
#endif
		    if (internalize) {
			/*
			* Internalize the attribute's value; the goal is to
			* speed up operations and minimize used space by
			* compiled stylesheets.
			*/
			txt = attr->children;
			/*
			* NOTE that this assumes only one
			*  text-node in the attribute's content.
			*/
			if ((txt != NULL) && (txt->content != NULL) &&
			    (!xmlDictOwns(style->dict, txt->content)))
			{
			    value = (xmlChar *) xmlDictLookup(style->dict,
				txt->content, -1);
			    xmlNodeSetContent(txt, NULL);
			    txt->content = value;
			}
		    }
		    /*
		    * Process xml:space attributes.
		    * ----------------------------
		    */
		    if ((findSpaceAttr != 0) &&
			(attr->ns != NULL) &&
			(attr->name != NULL) &&
			(attr->name[0] == 's') &&
			(attr->ns->prefix != NULL) &&
			(attr->ns->prefix[0] == 'x') &&
			(attr->ns->prefix[1] == 'm') &&
			(attr->ns->prefix[2] == 'l') &&
			(attr->ns->prefix[3] == 0))
		    {
			value = xmlGetNsProp(cur, BAD_CAST "space",
			    XML_XML_NAMESPACE);
			if (value != NULL) {
			    if (xmlStrEqual(value, BAD_CAST "preserve")) {
				cctxt->inode->preserveWhitespace = 1;
			    } else if (xmlStrEqual(value, BAD_CAST "default")) {
				cctxt->inode->preserveWhitespace = 0;
			    } else {
				/* Invalid value for xml:space. */
				xsltTransformError(NULL, style, cur,
				    "Attribute xml:space: Invalid value.\n");
				cctxt->style->warnings++;
			    }
			    findSpaceAttr = 0;
			    xmlFree(value);
			}

		    }
		    attr = attr->next;
		} while (attr != NULL);
	    }
	    /*
	    * We'll descend into the children of element nodes only.
	    */
	    if (cur->children != NULL) {
		cur = cur->children;
		continue;
	    }
	} else if ((cur->type == XML_TEXT_NODE) ||
		(cur->type == XML_CDATA_SECTION_NODE))
	{
	    /*
	    * Merge adjacent text/CDATA-section-nodes
	    * ---------------------------------------
	    * In order to avoid breaking of existing stylesheets,
	    * if the old behaviour is wanted (strictWhitespace == 0),
	    * then we *won't* merge adjacent text-nodes
	    * (except in xsl:text); this will ensure that whitespace-only
	    * text nodes are (incorrectly) not stripped in some cases.
	    *
	    * Example:               : <foo>  <!-- bar -->zoo</foo>
	    * Corrent (strict) result: <foo>  zoo</foo>
	    * Incorrect (old) result : <foo>zoo</foo>
	    *
	    * NOTE that we *will* merge adjacent text-nodes if
	    * they are in xsl:text.
	    * Example, the following:
	    * <xsl:text>  <!-- bar -->zoo<xsl:text>
	    * will result in both cases in:
	    * <xsl:text>  zoo<xsl:text>
	    */
	    cur->type = XML_TEXT_NODE;
	    if ((strictWhitespace != 0) || (inXSLText != 0)) {
		/*
		* New behaviour; merge nodes.
		*/
		if (textNode == NULL)
		    textNode = cur;
		else {
		    if (cur->content != NULL)
			xmlNodeAddContent(textNode, cur->content);
		    deleteNode = cur;
		}
		if ((cur->next == NULL) ||
		    (cur->next->type == XML_ELEMENT_NODE))
		    goto end_of_text;
		else
		    goto next_sibling;
	    } else {
		/*
		* Old behaviour.
		*/
		if (textNode == NULL)
		    textNode = cur;
		goto end_of_text;
	    }
	} else if ((cur->type == XML_COMMENT_NODE) ||
	    (cur->type == XML_PI_NODE))
	{
	    /*
	    * Remove processing instructions and comments.
	    */
	    deleteNode = cur;
	    if ((cur->next == NULL) ||
		(cur->next->type == XML_ELEMENT_NODE))
		goto end_of_text;
	    else
		goto next_sibling;
	} else {
	    textNode = NULL;
	    /*
	    * Invalid node-type for this data-model.
	    */
	    xsltTransformError(NULL, style, cur,
		"Invalid type of node for the XSLT data model.\n");
	    cctxt->style->errors++;
	    goto next_sibling;
	}

end_of_text:
	if (textNode) {
	    value = textNode->content;
	    /*
	    * At this point all adjacent text/CDATA-section nodes
	    * have been merged.
	    *
	    * Strip whitespace-only text-nodes.
	    * (cctxt->inode->stripWhitespace)
	    */
	    if ((value == NULL) || (*value == 0) ||
		(((cctxt->inode->stripWhitespace) ||
		  (! cctxt->inode->preserveWhitespace)) &&
		 IS_BLANK(*value) &&
		 xsltIsBlank(value)))
	    {
		if (textNode != cur) {
		    xmlUnlinkNode(textNode);
		    xmlFreeNode(textNode);
		} else
		    deleteNode = textNode;
		textNode = NULL;
		goto next_sibling;
	    }
	    /*
	    * Convert CDATA-section nodes to text-nodes.
	    * TODO: Can this produce problems?
	    */
	    if (textNode->type != XML_TEXT_NODE) {
		textNode->type = XML_TEXT_NODE;
		textNode->name = xmlStringText;
	    }
	    if (internalize &&
		(textNode->content != NULL) &&
		(!xmlDictOwns(style->dict, textNode->content)))
	    {
		/*
		* Internalize the string.
		*/
		value = (xmlChar *) xmlDictLookup(style->dict,
		    textNode->content, -1);
		xmlNodeSetContent(textNode, NULL);
		textNode->content = value;
	    }
	    textNode = NULL;
	    /*
	    * Note that "disable-output-escaping" of the xsl:text
	    * element will be applied at a later level, when
	    * XSLT elements are processed.
	    */
	}

next_sibling:
	if (cur->type == XML_ELEMENT_NODE) {
	    xsltCompilerNodePop(cctxt, cur);
	}
	if (cur == node)
	    break;
	if (cur->next != NULL) {
	    cur = cur->next;
	} else {
	    cur = cur->parent;
	    inXSLText = 0;
	    goto next_sibling;
	};
    }
    if (deleteNode != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
	 "xsltParsePreprocessStylesheetTree: removing node\n");
#endif
	xmlUnlinkNode(deleteNode);
	xmlFreeNode(deleteNode);
    }
    return(0);

internal_err:
    return(-1);
}

#endif /* XSLT_REFACTORED */

#ifdef XSLT_REFACTORED
#else
static void
xsltPreprocessStylesheet(xsltStylesheetPtr style, xmlNodePtr cur)
{
    xmlNodePtr deleteNode, styleelem;
    int internalize = 0;

    if ((style == NULL) || (cur == NULL))
        return;

    if ((cur->doc != NULL) && (style->dict != NULL) &&
        (cur->doc->dict == style->dict))
	internalize = 1;
    else
        style->internalized = 0;

    if ((cur != NULL) && (IS_XSLT_ELEM(cur)) &&
        (IS_XSLT_NAME(cur, "stylesheet"))) {
	styleelem = cur;
    } else {
        styleelem = NULL;
    }

    /*
     * This content comes from the stylesheet
     * For stylesheets, the set of whitespace-preserving
     * element names consists of just xsl:text.
     */
    deleteNode = NULL;
    while (cur != NULL) {
	if (deleteNode != NULL) {
#ifdef WITH_XSLT_DEBUG_BLANKS
	    xsltGenericDebug(xsltGenericDebugContext,
	     "xsltPreprocessStylesheet: removing ignorable blank node\n");
#endif
	    xmlUnlinkNode(deleteNode);
	    xmlFreeNode(deleteNode);
	    deleteNode = NULL;
	}
	if (cur->type == XML_ELEMENT_NODE) {
	    int exclPrefixes;
	    /*
	     * Internalize attributes values.
	     */
	    if ((internalize) && (cur->properties != NULL)) {
	        xmlAttrPtr attr = cur->properties;
		xmlNodePtr txt;

		while (attr != NULL) {
		    txt = attr->children;
		    if ((txt != NULL) && (txt->type == XML_TEXT_NODE) &&
		        (txt->content != NULL) &&
			(!xmlDictOwns(style->dict, txt->content)))
		    {
			xmlChar *tmp;

			/*
			 * internalize the text string, goal is to speed
			 * up operations and minimize used space by compiled
			 * stylesheets.
			 */
			tmp = (xmlChar *) xmlDictLookup(style->dict,
			                                txt->content, -1);
			if (tmp != txt->content) {
			    xmlNodeSetContent(txt, NULL);
			    txt->content = tmp;
			}
		    }
		    attr = attr->next;
		}
	    }
	    if (IS_XSLT_ELEM(cur)) {
		exclPrefixes = 0;
		if (IS_XSLT_NAME(cur, "text")) {
		    for (;exclPrefixes > 0;exclPrefixes--)
			exclPrefixPop(style);
		    goto skip_children;
		}
	    } else {
		exclPrefixes = xsltParseStylesheetExcludePrefix(style, cur, 0);
	    }

	    if ((cur->nsDef != NULL) && (style->exclPrefixNr > 0)) {
		xmlNsPtr ns = cur->nsDef, prev = NULL, next;
		xmlNodePtr root = NULL;
		int i, moved;

		root = xmlDocGetRootElement(cur->doc);
		if ((root != NULL) && (root != cur)) {
		    while (ns != NULL) {
			moved = 0;
			next = ns->next;
			for (i = 0;i < style->exclPrefixNr;i++) {
			    if ((ns->prefix != NULL) &&
			        (xmlStrEqual(ns->href,
					     style->exclPrefixTab[i]))) {
				/*
				 * Move the namespace definition on the root
				 * element to avoid duplicating it without
				 * loosing it.
				 */
				if (prev == NULL) {
				    cur->nsDef = ns->next;
				} else {
				    prev->next = ns->next;
				}
				ns->next = root->nsDef;
				root->nsDef = ns;
				moved = 1;
				break;
			    }
			}
			if (moved == 0)
			    prev = ns;
			ns = next;
		    }
		}
	    }
	    /*
	     * If we have prefixes locally, recurse and pop them up when
	     * going back
	     */
	    if (exclPrefixes > 0) {
		xsltPreprocessStylesheet(style, cur->children);
		for (;exclPrefixes > 0;exclPrefixes--)
		    exclPrefixPop(style);
		goto skip_children;
	    }
	} else if (cur->type == XML_TEXT_NODE) {
	    if (IS_BLANK_NODE(cur)) {
		if (xmlNodeGetSpacePreserve(cur->parent) != 1) {
		    deleteNode = cur;
		}
	    } else if ((cur->content != NULL) && (internalize) &&
	               (!xmlDictOwns(style->dict, cur->content))) {
		xmlChar *tmp;

		/*
		 * internalize the text string, goal is to speed
		 * up operations and minimize used space by compiled
		 * stylesheets.
		 */
		tmp = (xmlChar *) xmlDictLookup(style->dict, cur->content, -1);
		xmlNodeSetContent(cur, NULL);
		cur->content = tmp;
	    }
	} else if ((cur->type != XML_ELEMENT_NODE) &&
		   (cur->type != XML_CDATA_SECTION_NODE)) {
	    deleteNode = cur;
	    goto skip_children;
	}

	/*
	 * Skip to next node. In case of a namespaced element children of
	 * the stylesheet and not in the XSLT namespace and not an extension
	 * element, ignore its content.
	 */
	if ((cur->type == XML_ELEMENT_NODE) && (cur->ns != NULL) &&
	    (styleelem != NULL) && (cur->parent == styleelem) &&
	    (!xmlStrEqual(cur->ns->href, XSLT_NAMESPACE)) &&
	    (!xsltCheckExtURI(style, cur->ns->href))) {
	    goto skip_children;
	} else if (cur->children != NULL) {
	    cur = cur->children;
	    continue;
	}

skip_children:
	if (cur->next != NULL) {
	    cur = cur->next;
	    continue;
	}
	do {

	    cur = cur->parent;
	    if (cur == NULL)
		break;
	    if (cur == (xmlNodePtr) style->doc) {
		cur = NULL;
		break;
	    }
	    if (cur->next != NULL) {
		cur = cur->next;
		break;
	    }
	} while (cur != NULL);
    }
    if (deleteNode != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
	 "xsltPreprocessStylesheet: removing ignorable blank node\n");
#endif
	xmlUnlinkNode(deleteNode);
	xmlFreeNode(deleteNode);
    }
}
#endif /* end of else XSLT_REFACTORED */

/**
 * xsltGatherNamespaces:
 * @style:  the XSLT stylesheet
 *
 * Browse the stylesheet and build the namspace hash table which
 * will be used for XPath interpretation. If needed do a bit of normalization
 */

static void
xsltGatherNamespaces(xsltStylesheetPtr style) {
    xmlNodePtr cur;
    const xmlChar *URI;

    if (style == NULL)
        return;
    /*
     * TODO: basically if the stylesheet uses the same prefix for different
     *       patterns, well they may be in problem, hopefully they will get
     *       a warning first.
     */
    /*
    * TODO: Eliminate the use of the hash for XPath expressions.
    *   An expression should be evaluated in the context of the in-scope
    *   namespaces; eliminate the restriction of an XML document to contain
    *   no duplicate prefixes for different namespace names.
    *
    */
    cur = xmlDocGetRootElement(style->doc);
    while (cur != NULL) {
	if (cur->type == XML_ELEMENT_NODE) {
	    xmlNsPtr ns = cur->nsDef;
	    while (ns != NULL) {
		if (ns->prefix != NULL) {
		    if (style->nsHash == NULL) {
			style->nsHash = xmlHashCreate(10);
			if (style->nsHash == NULL) {
			    xsltTransformError(NULL, style, cur,
		 "xsltGatherNamespaces: failed to create hash table\n");
			    style->errors++;
			    return;
			}
		    }
		    URI = xmlHashLookup(style->nsHash, ns->prefix);
		    if ((URI != NULL) && (!xmlStrEqual(URI, ns->href))) {
			xsltTransformError(NULL, style, cur,
	     "Namespaces prefix %s used for multiple namespaces\n",ns->prefix);
			style->warnings++;
		    } else if (URI == NULL) {
			xmlHashUpdateEntry(style->nsHash, ns->prefix,
			    (void *) ns->href, NULL);

#ifdef WITH_XSLT_DEBUG_PARSING
			xsltGenericDebug(xsltGenericDebugContext,
		 "Added namespace: %s mapped to %s\n", ns->prefix, ns->href);
#endif
		    }
		}
		ns = ns->next;
	    }
	}

	/*
	 * Skip to next node
	 */
	if (cur->children != NULL) {
	    if (cur->children->type != XML_ENTITY_DECL) {
		cur = cur->children;
		continue;
	    }
	}
	if (cur->next != NULL) {
	    cur = cur->next;
	    continue;
	}

	do {
	    cur = cur->parent;
	    if (cur == NULL)
		break;
	    if (cur == (xmlNodePtr) style->doc) {
		cur = NULL;
		break;
	    }
	    if (cur->next != NULL) {
		cur = cur->next;
		break;
	    }
	} while (cur != NULL);
    }
}

#ifdef XSLT_REFACTORED

static xsltStyleType
xsltGetXSLTElementTypeByNode(xsltCompilerCtxtPtr cctxt,
			     xmlNodePtr node)
{
    if ((node == NULL) || (node->type != XML_ELEMENT_NODE) ||
	(node->name == NULL))
	return(0);

    if (node->name[0] == 'a') {
	if (IS_XSLT_NAME(node, "apply-templates"))
	    return(XSLT_FUNC_APPLYTEMPLATES);
	else if (IS_XSLT_NAME(node, "attribute"))
	    return(XSLT_FUNC_ATTRIBUTE);
	else if (IS_XSLT_NAME(node, "apply-imports"))
	    return(XSLT_FUNC_APPLYIMPORTS);
	else if (IS_XSLT_NAME(node, "attribute-set"))
	    return(0);

    } else if (node->name[0] == 'c') {
	if (IS_XSLT_NAME(node, "choose"))
	    return(XSLT_FUNC_CHOOSE);
	else if (IS_XSLT_NAME(node, "copy"))
	    return(XSLT_FUNC_COPY);
	else if (IS_XSLT_NAME(node, "copy-of"))
	    return(XSLT_FUNC_COPYOF);
	else if (IS_XSLT_NAME(node, "call-template"))
	    return(XSLT_FUNC_CALLTEMPLATE);
	else if (IS_XSLT_NAME(node, "comment"))
	    return(XSLT_FUNC_COMMENT);

    } else if (node->name[0] == 'd') {
	if (IS_XSLT_NAME(node, "document"))
	    return(XSLT_FUNC_DOCUMENT);
	else if (IS_XSLT_NAME(node, "decimal-format"))
	    return(0);

    } else if (node->name[0] == 'e') {
	if (IS_XSLT_NAME(node, "element"))
	    return(XSLT_FUNC_ELEMENT);

    } else if (node->name[0] == 'f') {
	if (IS_XSLT_NAME(node, "for-each"))
	    return(XSLT_FUNC_FOREACH);
	else if (IS_XSLT_NAME(node, "fallback"))
	    return(XSLT_FUNC_FALLBACK);

    } else if (*(node->name) == 'i') {
	if (IS_XSLT_NAME(node, "if"))
	    return(XSLT_FUNC_IF);
	else if (IS_XSLT_NAME(node, "include"))
	    return(0);
	else if (IS_XSLT_NAME(node, "import"))
	    return(0);

    } else if (*(node->name) == 'k') {
	if (IS_XSLT_NAME(node, "key"))
	    return(0);

    } else if (*(node->name) == 'm') {
	if (IS_XSLT_NAME(node, "message"))
	    return(XSLT_FUNC_MESSAGE);

    } else if (*(node->name) == 'n') {
	if (IS_XSLT_NAME(node, "number"))
	    return(XSLT_FUNC_NUMBER);
	else if (IS_XSLT_NAME(node, "namespace-alias"))
	    return(0);

    } else if (*(node->name) == 'o') {
	if (IS_XSLT_NAME(node, "otherwise"))
	    return(XSLT_FUNC_OTHERWISE);
	else if (IS_XSLT_NAME(node, "output"))
	    return(0);

    } else if (*(node->name) == 'p') {
	if (IS_XSLT_NAME(node, "param"))
	    return(XSLT_FUNC_PARAM);
	else if (IS_XSLT_NAME(node, "processing-instruction"))
	    return(XSLT_FUNC_PI);
	else if (IS_XSLT_NAME(node, "preserve-space"))
	    return(0);

    } else if (*(node->name) == 's') {
	if (IS_XSLT_NAME(node, "sort"))
	    return(XSLT_FUNC_SORT);
	else if (IS_XSLT_NAME(node, "strip-space"))
	    return(0);
	else if (IS_XSLT_NAME(node, "stylesheet"))
	    return(0);

    } else if (node->name[0] == 't') {
	if (IS_XSLT_NAME(node, "text"))
	    return(XSLT_FUNC_TEXT);
	else if (IS_XSLT_NAME(node, "template"))
	    return(0);
	else if (IS_XSLT_NAME(node, "transform"))
	    return(0);

    } else if (*(node->name) == 'v') {
	if (IS_XSLT_NAME(node, "value-of"))
	    return(XSLT_FUNC_VALUEOF);
	else if (IS_XSLT_NAME(node, "variable"))
	    return(XSLT_FUNC_VARIABLE);

    } else if (*(node->name) == 'w') {
	if (IS_XSLT_NAME(node, "when"))
	    return(XSLT_FUNC_WHEN);
	if (IS_XSLT_NAME(node, "with-param"))
	    return(XSLT_FUNC_WITHPARAM);
    }
    return(0);
}

/**
 * xsltParseAnyXSLTElem:
 *
 * @cctxt: the compilation context
 * @elem: the element node of the XSLT instruction
 *
 * Parses, validates the content models and compiles XSLT instructions.
 *
 * Returns 0 if everything's fine;
 *         -1 on API or internal errors.
 */
int
xsltParseAnyXSLTElem(xsltCompilerCtxtPtr cctxt, xmlNodePtr elem)
{
    if ((cctxt == NULL) || (elem == NULL) ||
	(elem->type != XML_ELEMENT_NODE))
	return(-1);

    elem->psvi = NULL;

    if (! (IS_XSLT_ELEM_FAST(elem)))
	return(-1);
    /*
    * Detection of handled content of extension instructions.
    */
    if (cctxt->inode->category == XSLT_ELEMENT_CATEGORY_EXTENSION) {
	cctxt->inode->extContentHandled = 1;
    }

    xsltCompilerNodePush(cctxt, elem);
    /*
    * URGENT TODO: Find a way to speed up this annoying redundant
    *  textual node-name and namespace comparison.
    */
    if (cctxt->inode->prev->curChildType != 0)
	cctxt->inode->type = cctxt->inode->prev->curChildType;
    else
	cctxt->inode->type = xsltGetXSLTElementTypeByNode(cctxt, elem);
    /*
    * Update the in-scope namespaces if needed.
    */
    if (elem->nsDef != NULL)
	cctxt->inode->inScopeNs =
	    xsltCompilerBuildInScopeNsList(cctxt, elem);
    /*
    * xsltStylePreCompute():
    *  This will compile the information found on the current
    *  element's attributes. NOTE that this won't process the
    *  children of the instruction.
    */
    xsltStylePreCompute(cctxt->style, elem);
    /*
    * TODO: How to react on errors in xsltStylePreCompute() ?
    */

    /*
    * Validate the content model of the XSLT-element.
    */
    switch (cctxt->inode->type) {
	case XSLT_FUNC_APPLYIMPORTS:
	    /* EMPTY */
	    goto empty_content;
	case XSLT_FUNC_APPLYTEMPLATES:
	    /* <!-- Content: (xsl:sort | xsl:with-param)* --> */
	    goto apply_templates;
	case XSLT_FUNC_ATTRIBUTE:
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_CALLTEMPLATE:
	    /* <!-- Content: xsl:with-param* --> */
	    goto call_template;
	case XSLT_FUNC_CHOOSE:
	    /* <!-- Content: (xsl:when+, xsl:otherwise?) --> */
	    goto choose;
	case XSLT_FUNC_COMMENT:
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_COPY:
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_COPYOF:
	    /* EMPTY */
	    goto empty_content;
	case XSLT_FUNC_DOCUMENT: /* Extra one */
	    /* ?? template ?? */
	    goto sequence_constructor;
	case XSLT_FUNC_ELEMENT:
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_FALLBACK:
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_FOREACH:
	    /* <!-- Content: (xsl:sort*, template) --> */
	    goto for_each;
	case XSLT_FUNC_IF:
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_OTHERWISE:
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_MESSAGE:
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_NUMBER:
	    /* EMPTY */
	    goto empty_content;
	case XSLT_FUNC_PARAM:
	    /*
	    * Check for redefinition.
	    */
	    if ((elem->psvi != NULL) && (cctxt->ivar != NULL)) {
		xsltVarInfoPtr ivar = cctxt->ivar;

		do {
		    if ((ivar->name ==
			 ((xsltStyleItemParamPtr) elem->psvi)->name) &&
			(ivar->nsName ==
			 ((xsltStyleItemParamPtr) elem->psvi)->ns))
		    {
			elem->psvi = NULL;
			xsltTransformError(NULL, cctxt->style, elem,
			    "Redefinition of variable or parameter '%s'.\n",
			    ivar->name);
			cctxt->style->errors++;
			goto error;
		    }
		    ivar = ivar->prev;
		} while (ivar != NULL);
	    }
	    /*  <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_PI:
	    /*  <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_SORT:
	    /* EMPTY */
	    goto empty_content;
	case XSLT_FUNC_TEXT:
	    /* <!-- Content: #PCDATA --> */
	    goto text;
	case XSLT_FUNC_VALUEOF:
	    /* EMPTY */
	    goto empty_content;
	case XSLT_FUNC_VARIABLE:
	    /*
	    * Check for redefinition.
	    */
	    if ((elem->psvi != NULL) && (cctxt->ivar != NULL)) {
		xsltVarInfoPtr ivar = cctxt->ivar;

		do {
		    if ((ivar->name ==
			 ((xsltStyleItemVariablePtr) elem->psvi)->name) &&
			(ivar->nsName ==
			 ((xsltStyleItemVariablePtr) elem->psvi)->ns))
		    {
			elem->psvi = NULL;
			xsltTransformError(NULL, cctxt->style, elem,
			    "Redefinition of variable or parameter '%s'.\n",
			    ivar->name);
			cctxt->style->errors++;
			goto error;
		    }
		    ivar = ivar->prev;
		} while (ivar != NULL);
	    }
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_WHEN:
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	case XSLT_FUNC_WITHPARAM:
	    /* <!-- Content: template --> */
	    goto sequence_constructor;
	default:
#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
		"xsltParseXSLTNode: Unhandled XSLT element '%s'.\n",
		elem->name);
#endif
	    xsltTransformError(NULL, cctxt->style, elem,
		"xsltParseXSLTNode: Internal error; "
		"unhandled XSLT element '%s'.\n", elem->name);
	    cctxt->style->errors++;
	    goto internal_err;
    }

apply_templates:
    /* <!-- Content: (xsl:sort | xsl:with-param)* --> */
    if (elem->children != NULL) {
	xmlNodePtr child = elem->children;
	do {
	    if (child->type == XML_ELEMENT_NODE) {
		if (IS_XSLT_ELEM_FAST(child)) {
		    if (xmlStrEqual(child->name, BAD_CAST "with-param")) {
			cctxt->inode->curChildType = XSLT_FUNC_WITHPARAM;
			xsltParseAnyXSLTElem(cctxt, child);
		    } else if (xmlStrEqual(child->name, BAD_CAST "sort")) {
			cctxt->inode->curChildType = XSLT_FUNC_SORT;
			xsltParseAnyXSLTElem(cctxt, child);
		    } else
			xsltParseContentError(cctxt->style, child);
		} else
		    xsltParseContentError(cctxt->style, child);
	    }
	    child = child->next;
	} while (child != NULL);
    }
    goto exit;

call_template:
    /* <!-- Content: xsl:with-param* --> */
    if (elem->children != NULL) {
	xmlNodePtr child = elem->children;
	do {
	    if (child->type == XML_ELEMENT_NODE) {
		if (IS_XSLT_ELEM_FAST(child)) {
		    xsltStyleType type;

		    type = xsltGetXSLTElementTypeByNode(cctxt, child);
		    if (type == XSLT_FUNC_WITHPARAM) {
			cctxt->inode->curChildType = XSLT_FUNC_WITHPARAM;
			xsltParseAnyXSLTElem(cctxt, child);
		    } else {
			xsltParseContentError(cctxt->style, child);
		    }
		} else
		    xsltParseContentError(cctxt->style, child);
	    }
	    child = child->next;
	} while (child != NULL);
    }
    goto exit;

text:
    if (elem->children != NULL) {
	xmlNodePtr child = elem->children;
	do {
	    if ((child->type != XML_TEXT_NODE) &&
		(child->type != XML_CDATA_SECTION_NODE))
	    {
		xsltTransformError(NULL, cctxt->style, elem,
		    "The XSLT 'text' element must have only character "
		    "data as content.\n");
	    }
	    child = child->next;
	} while (child != NULL);
    }
    goto exit;

empty_content:
    if (elem->children != NULL) {
	xmlNodePtr child = elem->children;
	/*
	* Relaxed behaviour: we will allow whitespace-only text-nodes.
	*/
	do {
	    if (((child->type != XML_TEXT_NODE) &&
		 (child->type != XML_CDATA_SECTION_NODE)) ||
		(! IS_BLANK_NODE(child)))
	    {
		xsltTransformError(NULL, cctxt->style, elem,
		    "This XSLT element must have no content.\n");
		cctxt->style->errors++;
		break;
	    }
	    child = child->next;
	} while (child != NULL);
    }
    goto exit;

choose:
    /* <!-- Content: (xsl:when+, xsl:otherwise?) --> */
    /*
    * TODO: text-nodes in between are *not* allowed in XSLT 1.0.
    *   The old behaviour did not check this.
    * NOTE: In XSLT 2.0 they are stripped beforehand
    *  if whitespace-only (regardless of xml:space).
    */
    if (elem->children != NULL) {
	xmlNodePtr child = elem->children;
	int nbWhen = 0, nbOtherwise = 0, err = 0;
	do {
	    if (child->type == XML_ELEMENT_NODE) {
		if (IS_XSLT_ELEM_FAST(child)) {
		    xsltStyleType type;

		    type = xsltGetXSLTElementTypeByNode(cctxt, child);
		    if (type == XSLT_FUNC_WHEN) {
			nbWhen++;
			if (nbOtherwise) {
			    xsltParseContentError(cctxt->style, child);
			    err = 1;
			    break;
			}
			cctxt->inode->curChildType = XSLT_FUNC_WHEN;
			xsltParseAnyXSLTElem(cctxt, child);
		    } else if (type == XSLT_FUNC_OTHERWISE) {
			if (! nbWhen) {
			    xsltParseContentError(cctxt->style, child);
			    err = 1;
			    break;
			}
			if (nbOtherwise) {
			    xsltTransformError(NULL, cctxt->style, elem,
				"The XSLT 'choose' element must not contain "
				"more than one XSLT 'otherwise' element.\n");
			    cctxt->style->errors++;
			    err = 1;
			    break;
			}
			nbOtherwise++;
			cctxt->inode->curChildType = XSLT_FUNC_OTHERWISE;
			xsltParseAnyXSLTElem(cctxt, child);
		    } else
			xsltParseContentError(cctxt->style, child);
		} else
		    xsltParseContentError(cctxt->style, child);
	    }
	    /*
		else
		    xsltParseContentError(cctxt, child);
	    */
	    child = child->next;
	} while (child != NULL);
	if ((! err) && (! nbWhen)) {
	    xsltTransformError(NULL, cctxt->style, elem,
		"The XSLT element 'choose' must contain at least one "
		"XSLT element 'when'.\n");
		cctxt->style->errors++;
	}
    }
    goto exit;

for_each:
    /* <!-- Content: (xsl:sort*, template) --> */
    /*
    * NOTE: Text-nodes before xsl:sort are *not* allowed in XSLT 1.0.
    *   The old behaviour did not allow this, but it catched this
    *   only at transformation-time.
    *   In XSLT 2.0 they are stripped beforehand if whitespace-only
    *   (regardless of xml:space).
    */
    if (elem->children != NULL) {
	xmlNodePtr child = elem->children;
	/*
	* Parse xsl:sort first.
	*/
	do {
	    if ((child->type == XML_ELEMENT_NODE) &&
		IS_XSLT_ELEM_FAST(child))
	    {
		if (xsltGetXSLTElementTypeByNode(cctxt, child) ==
		    XSLT_FUNC_SORT)
		{
		    cctxt->inode->curChildType = XSLT_FUNC_SORT;
		    xsltParseAnyXSLTElem(cctxt, child);
		} else
		    break;
	    } else
		break;
	    child = child->next;
	} while (child != NULL);
	/*
	* Parse the sequece constructor.
	*/
	if (child != NULL)
	    xsltParseSequenceConstructor(cctxt, child);
    }
    goto exit;

sequence_constructor:
    /*
    * Parse the sequence constructor.
    */
    if (elem->children != NULL)
	xsltParseSequenceConstructor(cctxt, elem->children);

    /*
    * Register information for vars/params. Only needed if there
    * are any following siblings.
    */
    if ((elem->next != NULL) &&
	((cctxt->inode->type == XSLT_FUNC_VARIABLE) ||
	 (cctxt->inode->type == XSLT_FUNC_PARAM)))
    {
	if ((elem->psvi != NULL) &&
	    (((xsltStyleBasicItemVariablePtr) elem->psvi)->name))
	{
	    xsltCompilerVarInfoPush(cctxt, elem,
		((xsltStyleBasicItemVariablePtr) elem->psvi)->name,
		((xsltStyleBasicItemVariablePtr) elem->psvi)->ns);
	}
    }

error:
exit:
    xsltCompilerNodePop(cctxt, elem);
    return(0);

internal_err:
    xsltCompilerNodePop(cctxt, elem);
    return(-1);
}

/**
 * xsltForwardsCompatUnkownItemCreate:
 *
 * @cctxt: the compilation context
 *
 * Creates a compiled representation of the unknown
 * XSLT instruction.
 *
 * Returns the compiled representation.
 */
static xsltStyleItemUknownPtr
xsltForwardsCompatUnkownItemCreate(xsltCompilerCtxtPtr cctxt)
{
    xsltStyleItemUknownPtr item;

    item = (xsltStyleItemUknownPtr) xmlMalloc(sizeof(xsltStyleItemUknown));
    if (item == NULL) {
	xsltTransformError(NULL, cctxt->style, NULL,
	    "Internal error in xsltForwardsCompatUnkownItemCreate(): "
	    "Failed to allocate memory.\n");
	cctxt->style->errors++;
	return(NULL);
    }
    memset(item, 0, sizeof(xsltStyleItemUknown));
    item->type = XSLT_FUNC_UNKOWN_FORWARDS_COMPAT;
    /*
    * Store it in the stylesheet.
    */
    item->next = cctxt->style->preComps;
    cctxt->style->preComps = (xsltElemPreCompPtr) item;
    return(item);
}

/**
 * xsltParseUnknownXSLTElem:
 *
 * @cctxt: the compilation context
 * @node: the element of the unknown XSLT instruction
 *
 * Parses an unknown XSLT element.
 * If forwards compatible mode is enabled this will allow
 * such an unknown XSLT and; otherwise it is rejected.
 *
 * Returns 1 in the unknown XSLT instruction is rejected,
 *         0 if everything's fine and
 *         -1 on API or internal errors.
 */
static int
xsltParseUnknownXSLTElem(xsltCompilerCtxtPtr cctxt,
			    xmlNodePtr node)
{
    if ((cctxt == NULL) || (node == NULL) || (node->type != XML_ELEMENT_NODE))
	return(-1);

    /*
    * Detection of handled content of extension instructions.
    */
    if (cctxt->inode->category == XSLT_ELEMENT_CATEGORY_EXTENSION) {
	cctxt->inode->extContentHandled = 1;
    }
    if (cctxt->inode->forwardsCompat == 0) {
	/*
	* We are not in forwards-compatible mode, so raise an error.
	*/
	xsltTransformError(NULL, cctxt->style, node,
	    "Unknown XSLT element '%s'.\n", node->name);
	cctxt->style->errors++;
	return(1);
    }
    /*
    * Forwards-compatible mode.
    * ------------------------
    *
    * Parse/compile xsl:fallback elements.
    *
    * QUESTION: Do we have to raise an error if there's no xsl:fallback?
    * ANSWER: No, since in the stylesheet the fallback behaviour might
    *  also be provided by using the XSLT function "element-available".
    */
    if (cctxt->unknownItem == NULL) {
	/*
	* Create a singleton for all unknown XSLT instructions.
	*/
	cctxt->unknownItem = xsltForwardsCompatUnkownItemCreate(cctxt);
	if (cctxt->unknownItem == NULL) {
	    node->psvi = NULL;
	    return(-1);
	}
    }
    node->psvi = cctxt->unknownItem;
    if (node->children == NULL)
	return(0);
    else {
	xmlNodePtr child = node->children;

	xsltCompilerNodePush(cctxt, node);
	/*
	* Update the in-scope namespaces if needed.
	*/
	if (node->nsDef != NULL)
	    cctxt->inode->inScopeNs =
		xsltCompilerBuildInScopeNsList(cctxt, node);
	/*
	* Parse all xsl:fallback children.
	*/
	do {
	    if ((child->type == XML_ELEMENT_NODE) &&
		IS_XSLT_ELEM_FAST(child) &&
		IS_XSLT_NAME(child, "fallback"))
	    {
		cctxt->inode->curChildType = XSLT_FUNC_FALLBACK;
		xsltParseAnyXSLTElem(cctxt, child);
	    }
	    child = child->next;
	} while (child != NULL);

	xsltCompilerNodePop(cctxt, node);
    }
    return(0);
}

/**
 * xsltParseSequenceConstructor:
 *
 * @cctxt: the compilation context
 * @cur: the start-node of the content to be parsed
 *
 * Parses a "template" content (or "sequence constructor" in XSLT 2.0 terms).
 * This will additionally remove xsl:text elements from the tree.
 */
void
xsltParseSequenceConstructor(xsltCompilerCtxtPtr cctxt, xmlNodePtr cur)
{
    xsltStyleType type;
    xmlNodePtr deleteNode = NULL;

    if (cctxt == NULL) {
	xmlGenericError(xmlGenericErrorContext,
	    "xsltParseSequenceConstructor: Bad arguments\n");
	cctxt->style->errors++;
	return;
    }
    /*
    * Detection of handled content of extension instructions.
    */
    if (cctxt->inode->category == XSLT_ELEMENT_CATEGORY_EXTENSION) {
	cctxt->inode->extContentHandled = 1;
    }
    if ((cur == NULL) || (cur->type == XML_NAMESPACE_DECL))
	return;
    /*
    * This is the content reffered to as a "template".
    * E.g. an xsl:element has such content model:
    * <xsl:element
    *   name = { qname }
    *   namespace = { uri-reference }
    *   use-attribute-sets = qnames>
    * <!-- Content: template -->
    *
    * NOTE that in XSLT-2 the term "template" was abandoned due to
    *  confusion with xsl:template and the term "sequence constructor"
    *  was introduced instead.
    *
    * The following XSLT-instructions are allowed to appear:
    *  xsl:apply-templates, xsl:call-template, xsl:apply-imports,
    *  xsl:for-each, xsl:value-of, xsl:copy-of, xsl:number,
    *  xsl:choose, xsl:if, xsl:text, xsl:copy, xsl:variable,
    *  xsl:message, xsl:fallback,
    *  xsl:processing-instruction, xsl:comment, xsl:element
    *  xsl:attribute.
    * Additional allowed content:
    * 1) extension instructions
    * 2) literal result elements
    * 3) PCDATA
    *
    * NOTE that this content model does *not* allow xsl:param.
    */
    while (cur != NULL) {
        cctxt->style->principal->opCount += 1;

	if (deleteNode != NULL)	{
#ifdef WITH_XSLT_DEBUG_BLANKS
	    xsltGenericDebug(xsltGenericDebugContext,
	     "xsltParseSequenceConstructor: removing xsl:text element\n");
#endif
	    xmlUnlinkNode(deleteNode);
	    xmlFreeNode(deleteNode);
	    deleteNode = NULL;
	}
	if (cur->type == XML_ELEMENT_NODE) {

	    if (cur->psvi == xsltXSLTTextMarker) {
		/*
		* xsl:text elements
		* --------------------------------------------------------
		*/
		xmlNodePtr tmp;

		cur->psvi = NULL;
		/*
		* Mark the xsl:text element for later deletion.
		*/
		deleteNode = cur;
		/*
		* Validate content.
		*/
		tmp = cur->children;
		if (tmp) {
		    /*
		    * We don't expect more than one text-node in the
		    * content, since we already merged adjacent
		    * text/CDATA-nodes and eliminated PI/comment-nodes.
		    */
		    if ((tmp->type == XML_TEXT_NODE) ||
			(tmp->next == NULL))
		    {
			/*
			* Leave the contained text-node in the tree.
			*/
			xmlUnlinkNode(tmp);
			if (xmlAddPrevSibling(cur, tmp) == NULL) {
                            xsltTransformError(ctxt, NULL, NULL,
                                    "out of memory\n");
                            xmlFreeNode(tmp);
                        }
		    } else {
			tmp = NULL;
			xsltTransformError(NULL, cctxt->style, cur,
			    "Element 'xsl:text': Invalid type "
			    "of node found in content.\n");
			cctxt->style->errors++;
		    }
		}
		if (cur->properties) {
		    xmlAttrPtr attr;
		    /*
		    * TODO: We need to report errors for
		    *  invalid attrs.
		    */
		    attr = cur->properties;
		    do {
			if ((attr->ns == NULL) &&
			    (attr->name != NULL) &&
			    (attr->name[0] == 'd') &&
			    xmlStrEqual(attr->name,
			    BAD_CAST "disable-output-escaping"))
			{
			    /*
			    * Attr "disable-output-escaping".
			    * XSLT-2: This attribute is deprecated.
			    */
			    if ((attr->children != NULL) &&
				xmlStrEqual(attr->children->content,
				BAD_CAST "yes"))
			    {
				/*
				* Disable output escaping for this
				* text node.
				*/
				if (tmp)
				    tmp->name = xmlStringTextNoenc;
			    } else if ((attr->children == NULL) ||
				(attr->children->content == NULL) ||
				(!xmlStrEqual(attr->children->content,
				BAD_CAST "no")))
			    {
				xsltTransformError(NULL, cctxt->style,
				    cur,
				    "Attribute 'disable-output-escaping': "
				    "Invalid value. Expected is "
				    "'yes' or 'no'.\n");
				cctxt->style->errors++;
			    }
			    break;
			}
			attr = attr->next;
		    } while (attr != NULL);
		}
	    } else if (IS_XSLT_ELEM_FAST(cur)) {
		/*
		* TODO: Using the XSLT-marker is still not stable yet.
		*/
		/* if (cur->psvi == xsltXSLTElemMarker) { */
		/*
		* XSLT instructions
		* --------------------------------------------------------
		*/
		cur->psvi = NULL;
		type = xsltGetXSLTElementTypeByNode(cctxt, cur);
		switch (type) {
		    case XSLT_FUNC_APPLYIMPORTS:
		    case XSLT_FUNC_APPLYTEMPLATES:
		    case XSLT_FUNC_ATTRIBUTE:
		    case XSLT_FUNC_CALLTEMPLATE:
		    case XSLT_FUNC_CHOOSE:
		    case XSLT_FUNC_COMMENT:
		    case XSLT_FUNC_COPY:
		    case XSLT_FUNC_COPYOF:
		    case XSLT_FUNC_DOCUMENT: /* Extra one */
		    case XSLT_FUNC_ELEMENT:
		    case XSLT_FUNC_FALLBACK:
		    case XSLT_FUNC_FOREACH:
		    case XSLT_FUNC_IF:
		    case XSLT_FUNC_MESSAGE:
		    case XSLT_FUNC_NUMBER:
		    case XSLT_FUNC_PI:
		    case XSLT_FUNC_TEXT:
		    case XSLT_FUNC_VALUEOF:
		    case XSLT_FUNC_VARIABLE:
			/*
			* Parse the XSLT element.
			*/
			cctxt->inode->curChildType = type;
			xsltParseAnyXSLTElem(cctxt, cur);
			break;
		    default:
			xsltParseUnknownXSLTElem(cctxt, cur);
			cur = cur->next;
			continue;
		}
	    } else {
		/*
		* Non-XSLT elements
		* -----------------
		*/
		xsltCompilerNodePush(cctxt, cur);
		/*
		* Update the in-scope namespaces if needed.
		*/
		if (cur->nsDef != NULL)
		    cctxt->inode->inScopeNs =
			xsltCompilerBuildInScopeNsList(cctxt, cur);
		/*
		* The current element is either a literal result element
		* or an extension instruction.
		*
		* Process attr "xsl:extension-element-prefixes".
		* FUTURE TODO: IIRC in XSLT 2.0 this attribute must be
		* processed by the implementor of the extension function;
		* i.e., it won't be handled by the XSLT processor.
		*/
		/* SPEC 1.0:
		*   "exclude-result-prefixes" is only allowed on literal
		*   result elements and "xsl:exclude-result-prefixes"
		*   on xsl:stylesheet/xsl:transform.
		* SPEC 2.0:
		*   "There are a number of standard attributes
		*   that may appear on any XSLT element: specifically
		*   version, exclude-result-prefixes,
		*   extension-element-prefixes, xpath-default-namespace,
		*   default-collation, and use-when."
		*
		* SPEC 2.0:
		*   For literal result elements:
		*   "xsl:version, xsl:exclude-result-prefixes,
		*    xsl:extension-element-prefixes,
		*    xsl:xpath-default-namespace,
		*    xsl:default-collation, or xsl:use-when."
		*/
		if (cur->properties)
		    cctxt->inode->extElemNs =
			xsltParseExtElemPrefixes(cctxt,
			    cur, cctxt->inode->extElemNs,
			    XSLT_ELEMENT_CATEGORY_LRE);
		/*
		* Eval if we have an extension instruction here.
		*/
		if ((cur->ns != NULL) &&
		    (cctxt->inode->extElemNs != NULL) &&
		    (xsltCheckExtPrefix(cctxt->style, cur->ns->href) == 1))
		{
		    /*
		    * Extension instructions
		    * ----------------------------------------------------
		    * Mark the node information.
		    */
		    cctxt->inode->category = XSLT_ELEMENT_CATEGORY_EXTENSION;
		    cctxt->inode->extContentHandled = 0;
		    if (cur->psvi != NULL) {
			cur->psvi = NULL;
			/*
			* TODO: Temporary sanity check.
			*/
			xsltTransformError(NULL, cctxt->style, cur,
			    "Internal error in xsltParseSequenceConstructor(): "
			    "Occupied PSVI field.\n");
			cctxt->style->errors++;
			cur = cur->next;
			continue;
		    }
		    cur->psvi = (void *)
			xsltPreComputeExtModuleElement(cctxt->style, cur);

		    if (cur->psvi == NULL) {
			/*
			* OLD COMMENT: "Unknown element, maybe registered
			*  at the context level. Mark it for later
			*  recognition."
			* QUESTION: What does the xsltExtMarker mean?
			*  ANSWER: It is used in
			*   xsltApplySequenceConstructor() at
			*   transformation-time to look out for extension
			*   registered in the transformation context.
			*/
			cur->psvi = (void *) xsltExtMarker;
		    }
		    /*
		    * BIG NOTE: Now the ugly part. In previous versions
		    *  of Libxslt (until 1.1.16), all the content of an
		    *  extension instruction was processed and compiled without
		    *  the need of the extension-author to explicitely call
		    *  such a processing;.We now need to mimic this old
		    *  behaviour in order to avoid breaking old code
		    *  on the extension-author's side.
		    * The mechanism:
		    *  1) If the author does *not* set the
		    *    compile-time-flag @extContentHandled, then we'll
		    *    parse the content assuming that it's a "template"
		    *    (or "sequence constructor in XSLT 2.0 terms).
		    *    NOTE: If the extension is registered at
		    *    transformation-time only, then there's no way of
		    *    knowing that content shall be valid, and we'll
		    *    process the content the same way.
		    *  2) If the author *does* set the flag, then we'll assume
		    *   that the author has handled the parsing him/herself
		    *   (e.g. called xsltParseSequenceConstructor(), etc.
		    *   explicitely in his/her code).
		    */
		    if ((cur->children != NULL) &&
			(cctxt->inode->extContentHandled == 0))
		    {
			/*
			* Default parsing of the content using the
			* sequence-constructor model.
			*/
			xsltParseSequenceConstructor(cctxt, cur->children);
		    }
		} else {
		    /*
		    * Literal result element
		    * ----------------------------------------------------
		    * Allowed XSLT attributes:
		    *  xsl:extension-element-prefixes CDATA #IMPLIED
		    *  xsl:exclude-result-prefixes CDATA #IMPLIED
		    *  TODO: xsl:use-attribute-sets %qnames; #IMPLIED
		    *  xsl:version NMTOKEN #IMPLIED
		    */
		    cur->psvi = NULL;
		    cctxt->inode->category = XSLT_ELEMENT_CATEGORY_LRE;
		    if (cur->properties != NULL) {
			xmlAttrPtr attr = cur->properties;
			/*
			* Attribute "xsl:exclude-result-prefixes".
			*/
			cctxt->inode->exclResultNs =
			    xsltParseExclResultPrefixes(cctxt, cur,
				cctxt->inode->exclResultNs,
				XSLT_ELEMENT_CATEGORY_LRE);
			/*
			* Attribute "xsl:version".
			*/
			xsltParseAttrXSLTVersion(cctxt, cur,
			    XSLT_ELEMENT_CATEGORY_LRE);
			/*
			* Report invalid XSLT attributes.
			* For XSLT 1.0 only xsl:use-attribute-sets is allowed
			* next to xsl:version, xsl:exclude-result-prefixes and
			* xsl:extension-element-prefixes.
			*
			* Mark all XSLT attributes, in order to skip such
			* attributes when instantiating the LRE.
			*/
			do {
			    if ((attr->psvi != xsltXSLTAttrMarker) &&
				IS_XSLT_ATTR_FAST(attr))
			    {
				if (! xmlStrEqual(attr->name,
				    BAD_CAST "use-attribute-sets"))
				{
				    xsltTransformError(NULL, cctxt->style,
					cur,
					"Unknown XSLT attribute '%s'.\n",
					attr->name);
				    cctxt->style->errors++;
				} else {
				    /*
				    * XSLT attr marker.
				    */
				    attr->psvi = (void *) xsltXSLTAttrMarker;
				}
			    }
			    attr = attr->next;
			} while (attr != NULL);
		    }
		    /*
		    * Create/reuse info for the literal result element.
		    */
		    if (cctxt->inode->nsChanged)
			xsltLREInfoCreate(cctxt, cur, 1);
		    cur->psvi = cctxt->inode->litResElemInfo;
		    /*
		    * Apply ns-aliasing on the element and on its attributes.
		    */
		    if (cctxt->hasNsAliases)
			xsltLREBuildEffectiveNs(cctxt, cur);
		    /*
		    * Compile attribute value templates (AVT).
		    */
		    if (cur->properties) {
			xmlAttrPtr attr = cur->properties;

			while (attr != NULL) {
			    xsltCompileAttr(cctxt->style, attr);
			    attr = attr->next;
			}
		    }
		    /*
		    * Parse the content, which is defined to be a "template"
		    * (or "sequence constructor" in XSLT 2.0 terms).
		    */
		    if (cur->children != NULL) {
			xsltParseSequenceConstructor(cctxt, cur->children);
		    }
		}
		/*
		* Leave the non-XSLT element.
		*/
		xsltCompilerNodePop(cctxt, cur);
	    }
	}
	cur = cur->next;
    }
    if (deleteNode != NULL) {
#ifdef WITH_XSLT_DEBUG_BLANKS
	xsltGenericDebug(xsltGenericDebugContext,
	    "xsltParseSequenceConstructor: removing xsl:text element\n");
#endif
	xmlUnlinkNode(deleteNode);
	xmlFreeNode(deleteNode);
	deleteNode = NULL;
    }
}

/**
 * xsltParseTemplateContent:
 * @style:  the XSLT stylesheet
 * @templ:  the node containing the content to be parsed
 *
 * Parses and compiles the content-model of an xsl:template element.
 * Note that this is *not* the "template" content model (or "sequence
 *  constructor" in XSLT 2.0); it it allows addional xsl:param
 *  elements as immediate children of @templ.
 *
 * Called by:
 *   exsltFuncFunctionComp() (EXSLT, functions.c)
 *   So this is intended to be called from extension functions.
 */
void
xsltParseTemplateContent(xsltStylesheetPtr style, xmlNodePtr templ) {
    if ((style == NULL) || (templ == NULL) ||
        (templ->type == XML_NAMESPACE_DECL))
	return;

    /*
    * Detection of handled content of extension instructions.
    */
    if (XSLT_CCTXT(style)->inode->category == XSLT_ELEMENT_CATEGORY_EXTENSION) {
	XSLT_CCTXT(style)->inode->extContentHandled = 1;
    }

    if (templ->children != NULL) {
	xmlNodePtr child = templ->children;
	/*
	* Process xsl:param elements, which can only occur as the
	* immediate children of xsl:template (well, and of any
	* user-defined extension instruction if needed).
	*/
	do {
            style->principal->opCount += 1;

	    if ((child->type == XML_ELEMENT_NODE) &&
		IS_XSLT_ELEM_FAST(child) &&
		IS_XSLT_NAME(child, "param"))
	    {
		XSLT_CCTXT(style)->inode->curChildType = XSLT_FUNC_PARAM;
		xsltParseAnyXSLTElem(XSLT_CCTXT(style), child);
	    } else
		break;
	    child = child->next;
	} while (child != NULL);
	/*
	* Parse the content and register the pattern.
	*/
	xsltParseSequenceConstructor(XSLT_CCTXT(style), child);
    }
}

#else /* XSLT_REFACTORED */

/**
 * xsltParseTemplateContent:
 * @style:  the XSLT stylesheet
 * @templ:  the container node (can be a document for literal results)
 *
 * parse a template content-model
 * Clean-up the template content from unwanted ignorable blank nodes
 * and process xslt:text
 */
void
xsltParseTemplateContent(xsltStylesheetPtr style, xmlNodePtr templ) {
    xmlNodePtr cur, delete;

    if ((style == NULL) || (templ == NULL) ||
        (templ->type == XML_NAMESPACE_DECL)) return;

    /*
     * This content comes from the stylesheet
     * For stylesheets, the set of whitespace-preserving
     * element names consists of just xsl:text.
     */
    cur = templ->children;
    delete = NULL;
    while (cur != NULL) {
        style->principal->opCount += 1;

	if (delete != NULL) {
#ifdef WITH_XSLT_DEBUG_BLANKS
	    xsltGenericDebug(xsltGenericDebugContext,
	     "xsltParseTemplateContent: removing text\n");
#endif
	    xmlUnlinkNode(delete);
	    xmlFreeNode(delete);
	    delete = NULL;
	}
	if (IS_XSLT_ELEM(cur)) {
            xsltStylePreCompute(style, cur);

	    if (IS_XSLT_NAME(cur, "text")) {
		/*
		* TODO: Processing of xsl:text should be moved to
		*   xsltPreprocessStylesheet(), since otherwise this
		*   will be performed for every multiply included
		*   stylesheet; i.e. this here is not skipped with
		*   the use of the style->nopreproc flag.
		*/
		if (cur->children != NULL) {
		    xmlChar *prop;
		    xmlNodePtr text = cur->children, next;
		    int noesc = 0;

		    prop = xmlGetNsProp(cur,
			(const xmlChar *)"disable-output-escaping",
			NULL);
		    if (prop != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
			xsltGenericDebug(xsltGenericDebugContext,
			     "Disable escaping: %s\n", text->content);
#endif
			if (xmlStrEqual(prop, (const xmlChar *)"yes")) {
			    noesc = 1;
			} else if (!xmlStrEqual(prop,
						(const xmlChar *)"no")){
			    xsltTransformError(NULL, style, cur,
	     "xsl:text: disable-output-escaping allows only yes or no\n");
			    style->warnings++;

			}
			xmlFree(prop);
		    }

		    while (text != NULL) {
			if (text->type == XML_COMMENT_NODE) {
			    text = text->next;
			    continue;
			}
			if ((text->type != XML_TEXT_NODE) &&
			     (text->type != XML_CDATA_SECTION_NODE)) {
			    xsltTransformError(NULL, style, cur,
		 "xsltParseTemplateContent: xslt:text content problem\n");
			    style->errors++;
			    break;
			}
			if ((noesc) && (text->type != XML_CDATA_SECTION_NODE))
			    text->name = xmlStringTextNoenc;
			text = text->next;
		    }

		    /*
		     * replace xsl:text by the list of childs
		     */
		    if (text == NULL) {
			text = cur->children;
			while (text != NULL) {
			    if ((style->internalized) &&
			        (text->content != NULL) &&
			        (!xmlDictOwns(style->dict, text->content))) {

				/*
				 * internalize the text string
				 */
				if (text->doc->dict != NULL) {
				    const xmlChar *tmp;

				    tmp = xmlDictLookup(text->doc->dict,
				                        text->content, -1);
				    if (tmp != text->content) {
				        xmlNodeSetContent(text, NULL);
					text->content = (xmlChar *) tmp;
				    }
				}
			    }

			    next = text->next;
			    xmlUnlinkNode(text);
                            if (xmlAddPrevSibling(cur, text) == NULL) {
                                xsltTransformError(NULL, style, NULL,
                                        "out of memory\n");
                                xmlFreeNode(text);
                            }
			    text = next;
			}
		    }
		}
		delete = cur;
		goto skip_children;
	    }
	}
	else if ((cur->ns != NULL) && (style->nsDefs != NULL) &&
	    (xsltCheckExtPrefix(style, cur->ns->prefix)))
	{
	    /*
	     * okay this is an extension element compile it too
	     */
	    xsltStylePreCompute(style, cur);
	}
	else if (cur->type == XML_ELEMENT_NODE)
	{
	    /*
	     * This is an element which will be output as part of the
	     * template exectution, precompile AVT if found.
	     */
	    if ((cur->ns == NULL) && (style->defaultAlias != NULL)) {
		cur->ns = xmlSearchNsByHref(cur->doc, cur,
			style->defaultAlias);
	    }
	    if (cur->properties != NULL) {
	        xmlAttrPtr attr = cur->properties;

		while (attr != NULL) {
		    xsltCompileAttr(style, attr);
		    attr = attr->next;
		}
	    }
	}
	/*
	 * Skip to next node
	 */
	if (cur->children != NULL) {
	    if (cur->children->type != XML_ENTITY_DECL) {
		cur = cur->children;
		continue;
	    }
	}
skip_children:
	if (cur->next != NULL) {
	    cur = cur->next;
	    continue;
	}

	do {
	    cur = cur->parent;
	    if (cur == NULL)
		break;
	    if (cur == templ) {
		cur = NULL;
		break;
	    }
	    if (cur->next != NULL) {
		cur = cur->next;
		break;
	    }
	} while (cur != NULL);
    }
    if (delete != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
	 "xsltParseTemplateContent: removing text\n");
#endif
	xmlUnlinkNode(delete);
	xmlFreeNode(delete);
	delete = NULL;
    }

    /*
     * Skip the first params
     */
    cur = templ->children;
    while (cur != NULL) {
	if ((IS_XSLT_ELEM(cur)) && (!(IS_XSLT_NAME(cur, "param"))))
	    break;
	cur = cur->next;
    }

    /*
     * Browse the remainder of the template
     */
    while (cur != NULL) {
	if ((IS_XSLT_ELEM(cur)) && (IS_XSLT_NAME(cur, "param"))) {
	    xmlNodePtr param = cur;

	    xsltTransformError(NULL, style, cur,
		"xsltParseTemplateContent: ignoring misplaced param element\n");
	    if (style != NULL) style->warnings++;
            cur = cur->next;
	    xmlUnlinkNode(param);
	    xmlFreeNode(param);
	} else
	    break;
    }
}

#endif /* else XSLT_REFACTORED */

/**
 * xsltParseStylesheetKey:
 * @style:  the XSLT stylesheet
 * @key:  the "key" element
 *
 * <!-- Category: top-level-element -->
 * <xsl:key name = qname, match = pattern, use = expression />
 *
 * parse an XSLT stylesheet key definition and register it
 */

static void
xsltParseStylesheetKey(xsltStylesheetPtr style, xmlNodePtr key) {
    xmlChar *prop = NULL;
    xmlChar *use = NULL;
    xmlChar *match = NULL;
    xmlChar *name = NULL;
    xmlChar *nameURI = NULL;

    if ((style == NULL) || (key == NULL) || (key->type != XML_ELEMENT_NODE))
	return;

    /*
     * Get arguments
     */
    prop = xmlGetNsProp(key, (const xmlChar *)"name", NULL);
    if (prop != NULL) {
        const xmlChar *URI;

	/*
	* TODO: Don't use xsltGetQNameURI().
	*/
	URI = xsltGetQNameURI(key, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	    goto error;
	} else {
	    name = prop;
	    if (URI != NULL)
		nameURI = xmlStrdup(URI);
	}
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
	     "xsltParseStylesheetKey: name %s\n", name);
#endif
    } else {
	xsltTransformError(NULL, style, key,
	    "xsl:key : error missing name\n");
	if (style != NULL) style->errors++;
	goto error;
    }

    match = xmlGetNsProp(key, (const xmlChar *)"match", NULL);
    if (match == NULL) {
	xsltTransformError(NULL, style, key,
	    "xsl:key : error missing match\n");
	if (style != NULL) style->errors++;
	goto error;
    }

    use = xmlGetNsProp(key, (const xmlChar *)"use", NULL);
    if (use == NULL) {
	xsltTransformError(NULL, style, key,
	    "xsl:key : error missing use\n");
	if (style != NULL) style->errors++;
	goto error;
    }

    /*
     * register the keys
     */
    xsltAddKey(style, name, nameURI, match, use, key);


error:
    if (use != NULL)
	xmlFree(use);
    if (match != NULL)
	xmlFree(match);
    if (name != NULL)
	xmlFree(name);
    if (nameURI != NULL)
	xmlFree(nameURI);

    if (key->children != NULL) {
	xsltParseContentError(style, key->children);
    }
}

#ifdef XSLT_REFACTORED
/**
 * xsltParseXSLTTemplate:
 * @style:  the XSLT stylesheet
 * @template:  the "template" element
 *
 * parse an XSLT stylesheet template building the associated structures
 * TODO: Is @style ever expected to be NULL?
 *
 * Called from:
 *   xsltParseXSLTStylesheet()
 *   xsltParseStylesheetTop()
 */

static void
xsltParseXSLTTemplate(xsltCompilerCtxtPtr cctxt, xmlNodePtr templNode) {
    xsltTemplatePtr templ;
    xmlChar *prop;
    double  priority;

    if ((cctxt == NULL) || (templNode == NULL) ||
        (templNode->type != XML_ELEMENT_NODE))
	return;

    /*
     * Create and link the structure
     */
    templ = xsltNewTemplate();
    if (templ == NULL)
	return;

    xsltCompilerNodePush(cctxt, templNode);
    if (templNode->nsDef != NULL)
	cctxt->inode->inScopeNs =
	    xsltCompilerBuildInScopeNsList(cctxt, templNode);

    templ->next = cctxt->style->templates;
    cctxt->style->templates = templ;
    templ->style = cctxt->style;

    /*
    * Attribute "mode".
    */
    prop = xmlGetNsProp(templNode, (const xmlChar *)"mode", NULL);
    if (prop != NULL) {
        const xmlChar *modeURI;

	/*
	* TODO: We need a standardized function for extraction
	*  of namespace names and local names from QNames.
	*  Don't use xsltGetQNameURI() as it cannot channe�
	*  reports through the context.
	*/
	modeURI = xsltGetQNameURI(templNode, &prop);
	if (prop == NULL) {
	    cctxt->style->errors++;
	    goto error;
	}
	templ->mode = xmlDictLookup(cctxt->style->dict, prop, -1);
	xmlFree(prop);
	prop = NULL;
	if (xmlValidateNCName(templ->mode, 0)) {
	    xsltTransformError(NULL, cctxt->style, templNode,
		"xsl:template: Attribute 'mode': The local part '%s' "
		"of the value is not a valid NCName.\n", templ->name);
	    cctxt->style->errors++;
	    goto error;
	}
	if (modeURI != NULL)
	    templ->modeURI = xmlDictLookup(cctxt->style->dict, modeURI, -1);
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
	     "xsltParseXSLTTemplate: mode %s\n", templ->mode);
#endif
    }
    /*
    * Attribute "match".
    */
    prop = xmlGetNsProp(templNode, (const xmlChar *)"match", NULL);
    if (prop != NULL) {
	templ->match  = prop;
	prop = NULL;
    }
    /*
    * Attribute "priority".
    */
    prop = xmlGetNsProp(templNode, (const xmlChar *)"priority", NULL);
    if (prop != NULL) {
	priority = xmlXPathStringEvalNumber(prop);
	templ->priority = (float) priority;
	xmlFree(prop);
	prop = NULL;
    }
    /*
    * Attribute "name".
    */
    prop = xmlGetNsProp(templNode, (const xmlChar *)"name", NULL);
    if (prop != NULL) {
        const xmlChar *nameURI;
	xsltTemplatePtr curTempl;

	/*
	* TODO: Don't use xsltGetQNameURI().
	*/
	nameURI = xsltGetQNameURI(templNode, &prop);
	if (prop == NULL) {
	    cctxt->style->errors++;
	    goto error;
	}
	templ->name = xmlDictLookup(cctxt->style->dict, prop, -1);
	xmlFree(prop);
	prop = NULL;
	if (xmlValidateNCName(templ->name, 0)) {
	    xsltTransformError(NULL, cctxt->style, templNode,
		"xsl:template: Attribute 'name': The local part '%s' of "
		"the value is not a valid NCName.\n", templ->name);
	    cctxt->style->errors++;
	    goto error;
	}
	if (nameURI != NULL)
	    templ->nameURI = xmlDictLookup(cctxt->style->dict, nameURI, -1);
	curTempl = templ->next;
	while (curTempl != NULL) {
	    if ((nameURI != NULL && xmlStrEqual(curTempl->name, templ->name) &&
		xmlStrEqual(curTempl->nameURI, nameURI) ) ||
		(nameURI == NULL && curTempl->nameURI == NULL &&
		xmlStrEqual(curTempl->name, templ->name)))
	    {
		xsltTransformError(NULL, cctxt->style, templNode,
		    "xsl:template: error duplicate name '%s'\n", templ->name);
		cctxt->style->errors++;
		goto error;
	    }
	    curTempl = curTempl->next;
	}
    }
    if (templNode->children != NULL) {
	xsltParseTemplateContent(cctxt->style, templNode);
	/*
	* MAYBE TODO: Custom behaviour: In order to stay compatible with
	* Xalan and MSXML(.NET), we could allow whitespace
	* to appear before an xml:param element; this whitespace
	* will additionally become part of the "template".
	* NOTE that this is totally deviates from the spec, but
	* is the de facto behaviour of Xalan and MSXML(.NET).
	* Personally I wouldn't allow this, since if we have:
	* <xsl:template ...xml:space="preserve">
	*   <xsl:param name="foo"/>
	*   <xsl:param name="bar"/>
	*   <xsl:param name="zoo"/>
	* ... the whitespace between every xsl:param would be
	* added to the result tree.
	*/
    }

    templ->elem = templNode;
    templ->content = templNode->children;
    xsltAddTemplate(cctxt->style, templ, templ->mode, templ->modeURI);

error:
    xsltCompilerNodePop(cctxt, templNode);
    return;
}

#else /* XSLT_REFACTORED */

/**
 * xsltParseStylesheetTemplate:
 * @style:  the XSLT stylesheet
 * @template:  the "template" element
 *
 * parse an XSLT stylesheet template building the associated structures
 */

static void
xsltParseStylesheetTemplate(xsltStylesheetPtr style, xmlNodePtr template) {
    xsltTemplatePtr ret;
    xmlChar *prop;
    xmlChar *mode = NULL;
    xmlChar *modeURI = NULL;
    double  priority;

    if ((style == NULL) || (template == NULL) ||
        (template->type != XML_ELEMENT_NODE))
	return;

    if (style->principal->opLimit > 0) {
        if (style->principal->opCount > style->principal->opLimit) {
            xsltTransformError(NULL, style, NULL,
                "XSLT parser operation limit exceeded\n");
	    style->errors++;
            return;
        }
    }

    /*
     * Create and link the structure
     */
    ret = xsltNewTemplate();
    if (ret == NULL)
	return;
    ret->next = style->templates;
    style->templates = ret;
    ret->style = style;

    /*
     * Get inherited namespaces
     */
    /*
    * TODO: Apply the optimized in-scope-namespace mechanism
    *   as for the other XSLT instructions.
    */
    xsltGetInheritedNsList(style, ret, template);

    /*
     * Get arguments
     */
    prop = xmlGetNsProp(template, (const xmlChar *)"mode", NULL);
    if (prop != NULL) {
        const xmlChar *URI;

	/*
	* TODO: Don't use xsltGetQNameURI().
	*/
	URI = xsltGetQNameURI(template, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	    goto error;
	} else {
	    mode = prop;
	    if (URI != NULL)
		modeURI = xmlStrdup(URI);
	}
	ret->mode = xmlDictLookup(style->dict, mode, -1);
	ret->modeURI = xmlDictLookup(style->dict, modeURI, -1);
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
	     "xsltParseStylesheetTemplate: mode %s\n", mode);
#endif
        if (mode != NULL) xmlFree(mode);
	if (modeURI != NULL) xmlFree(modeURI);
    }
    prop = xmlGetNsProp(template, (const xmlChar *)"match", NULL);
    if (prop != NULL) {
	if (ret->match != NULL) xmlFree(ret->match);
	ret->match  = prop;
    }

    prop = xmlGetNsProp(template, (const xmlChar *)"priority", NULL);
    if (prop != NULL) {
	priority = xmlXPathStringEvalNumber(prop);
	ret->priority = (float) priority;
	xmlFree(prop);
    }

    prop = xmlGetNsProp(template, (const xmlChar *)"name", NULL);
    if (prop != NULL) {
        const xmlChar *URI;

	/*
	* TODO: Don't use xsltGetQNameURI().
	*/
	URI = xsltGetQNameURI(template, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	    goto error;
	} else {
	    if (xmlValidateNCName(prop,0)) {
	        xsltTransformError(NULL, style, template,
	            "xsl:template : error invalid name '%s'\n", prop);
		if (style != NULL) style->errors++;
                xmlFree(prop);
		goto error;
	    }
	    ret->name = xmlDictLookup(style->dict, BAD_CAST prop, -1);
	    xmlFree(prop);
	    prop = NULL;
	    if (URI != NULL)
		ret->nameURI = xmlDictLookup(style->dict, BAD_CAST URI, -1);
	    else
		ret->nameURI = NULL;
	}
    }

    /*
     * parse the content and register the pattern
     */
    xsltParseTemplateContent(style, template);
    ret->elem = template;
    ret->content = template->children;
    xsltAddTemplate(style, ret, ret->mode, ret->modeURI);

error:
    return;
}

#endif /* else XSLT_REFACTORED */

#ifdef XSLT_REFACTORED

/**
 * xsltIncludeComp:
 * @cctxt: the compilation context
 * @node:  the xsl:include node
 *
 * Process the xslt include node on the source node
 */
static xsltStyleItemIncludePtr
xsltCompileXSLTIncludeElem(xsltCompilerCtxtPtr cctxt, xmlNodePtr node) {
    xsltStyleItemIncludePtr item;

    if ((cctxt == NULL) || (node == NULL) || (node->type != XML_ELEMENT_NODE))
	return(NULL);

    node->psvi = NULL;
    item = (xsltStyleItemIncludePtr) xmlMalloc(sizeof(xsltStyleItemInclude));
    if (item == NULL) {
	xsltTransformError(NULL, cctxt->style, node,
		"xsltIncludeComp : malloc failed\n");
	cctxt->style->errors++;
	return(NULL);
    }
    memset(item, 0, sizeof(xsltStyleItemInclude));

    node->psvi = item;
    item->inst = node;
    item->type = XSLT_FUNC_INCLUDE;

    item->next = cctxt->style->preComps;
    cctxt->style->preComps = (xsltElemPreCompPtr) item;

    return(item);
}

static int
xsltParseFindTopLevelElem(xsltCompilerCtxtPtr cctxt,
			      xmlNodePtr cur,
			      const xmlChar *name,
			      const xmlChar *namespaceURI,
			      int breakOnOtherElem,
			      xmlNodePtr *resultNode)
{
    if (name == NULL)
	return(-1);

    *resultNode = NULL;
    while (cur != NULL) {
	if (cur->type == XML_ELEMENT_NODE) {
	    if ((cur->ns != NULL) && (cur->name != NULL)) {
		if ((*(cur->name) == *name) &&
		    xmlStrEqual(cur->name, name) &&
		    xmlStrEqual(cur->ns->href, namespaceURI))
		{
		    *resultNode = cur;
		    return(1);
		}
	    }
	    if (breakOnOtherElem)
		break;
	}
	cur = cur->next;
    }
    *resultNode = cur;
    return(0);
}

static int
xsltParseTopLevelXSLTElem(xsltCompilerCtxtPtr cctxt,
			  xmlNodePtr node,
			  xsltStyleType type)
{
    int ret = 0;

    /*
    * TODO: The reason why this function exists:
    *  due to historical reasons some of the
    *  top-level declarations are processed by functions
    *  in other files. Since we need still to set
    *  up the node-info and generate information like
    *  in-scope namespaces, this is a wrapper around
    *  those old parsing functions.
    */
    xsltCompilerNodePush(cctxt, node);
    if (node->nsDef != NULL)
	cctxt->inode->inScopeNs =
	    xsltCompilerBuildInScopeNsList(cctxt, node);
    cctxt->inode->type = type;

    switch (type) {
	case XSLT_FUNC_INCLUDE:
	    {
		int oldIsInclude;

		if (xsltCompileXSLTIncludeElem(cctxt, node) == NULL)
		    goto exit;
		/*
		* Mark this stylesheet tree as being currently included.
		*/
		oldIsInclude = cctxt->isInclude;
		cctxt->isInclude = 1;

		if (xsltParseStylesheetInclude(cctxt->style, node) != 0) {
		    cctxt->style->errors++;
		}
		cctxt->isInclude = oldIsInclude;
	    }
	    break;
	case XSLT_FUNC_PARAM:
	    xsltStylePreCompute(cctxt->style, node);
	    xsltParseGlobalParam(cctxt->style, node);
	    break;
	case XSLT_FUNC_VARIABLE:
	    xsltStylePreCompute(cctxt->style, node);
	    xsltParseGlobalVariable(cctxt->style, node);
	    break;
	case XSLT_FUNC_ATTRSET:
	    xsltParseStylesheetAttributeSet(cctxt->style, node);
	    break;
	default:
	    xsltTransformError(NULL, cctxt->style, node,
		"Internal error: (xsltParseTopLevelXSLTElem) "
		"Cannot handle this top-level declaration.\n");
	    cctxt->style->errors++;
	    ret = -1;
    }

exit:
    xsltCompilerNodePop(cctxt, node);

    return(ret);
}

#if 0
static int
xsltParseRemoveWhitespace(xmlNodePtr node)
{
    if ((node == NULL) || (node->children == NULL))
	return(0);
    else {
	xmlNodePtr delNode = NULL, child = node->children;

	do {
	    if (delNode) {
		xmlUnlinkNode(delNode);
		xmlFreeNode(delNode);
		delNode = NULL;
	    }
	    if (((child->type == XML_TEXT_NODE) ||
		 (child->type == XML_CDATA_SECTION_NODE)) &&
		(IS_BLANK_NODE(child)))
		delNode = child;
	    child = child->next;
	} while (child != NULL);
	if (delNode) {
	    xmlUnlinkNode(delNode);
	    xmlFreeNode(delNode);
	    delNode = NULL;
	}
    }
    return(0);
}
#endif

static int
xsltParseXSLTStylesheetElemCore(xsltCompilerCtxtPtr cctxt, xmlNodePtr node)
{
#ifdef WITH_XSLT_DEBUG_PARSING
    int templates = 0;
#endif
    xmlNodePtr cur, start = NULL;
    xsltStylesheetPtr style;

    if ((cctxt == NULL) || (node == NULL) ||
	(node->type != XML_ELEMENT_NODE))
	return(-1);

    style = cctxt->style;
    /*
    * At this stage all import declarations of all stylesheet modules
    * with the same stylesheet level have been processed.
    * Now we can safely parse the rest of the declarations.
    */
    if (IS_XSLT_ELEM_FAST(node) && IS_XSLT_NAME(node, "include"))
    {
	xsltDocumentPtr include;
	/*
	* URGENT TODO: Make this work with simplified stylesheets!
	*   I.e., when we won't find an xsl:stylesheet element.
	*/
	/*
	* This is as include declaration.
	*/
	include = ((xsltStyleItemIncludePtr) node->psvi)->include;
	if (include == NULL) {
	    /* TODO: raise error? */
	    return(-1);
	}
	/*
	* TODO: Actually an xsl:include should locate an embedded
	*  stylesheet as well; so the document-element won't always
	*  be the element where the actual stylesheet is rooted at.
	*  But such embedded stylesheets are not supported by Libxslt yet.
	*/
	node = xmlDocGetRootElement(include->doc);
	if (node == NULL) {
	    return(-1);
	}
    }

    if (node->children == NULL)
	return(0);
    /*
    * Push the xsl:stylesheet/xsl:transform element.
    */
    xsltCompilerNodePush(cctxt, node);
    cctxt->inode->isRoot = 1;
    cctxt->inode->nsChanged = 0;
    /*
    * Start with the naked dummy info for literal result elements.
    */
    cctxt->inode->litResElemInfo = cctxt->inodeList->litResElemInfo;

    /*
    * In every case, we need to have
    * the in-scope namespaces of the element, where the
    * stylesheet is rooted at, regardless if it's an XSLT
    * instruction or a literal result instruction (or if
    * this is an embedded stylesheet).
    */
    cctxt->inode->inScopeNs =
	xsltCompilerBuildInScopeNsList(cctxt, node);

    /*
    * Process attributes of xsl:stylesheet/xsl:transform.
    * --------------------------------------------------
    * Allowed are:
    *  id = id
    *  extension-element-prefixes = tokens
    *  exclude-result-prefixes = tokens
    *  version = number (mandatory)
    */
    if (xsltParseAttrXSLTVersion(cctxt, node,
	XSLT_ELEMENT_CATEGORY_XSLT) == 0)
    {
	/*
	* Attribute "version".
	* XSLT 1.0: "An xsl:stylesheet element *must* have a version
	*  attribute, indicating the version of XSLT that the
	*  stylesheet requires".
	* The root element of a simplified stylesheet must also have
	* this attribute.
	*/
#ifdef XSLT_REFACTORED_MANDATORY_VERSION
	if (isXsltElem)
	    xsltTransformError(NULL, cctxt->style, node,
		"The attribute 'version' is missing.\n");
	cctxt->style->errors++;
#else
	/* OLD behaviour. */
	xsltTransformError(NULL, cctxt->style, node,
	    "xsl:version is missing: document may not be a stylesheet\n");
	cctxt->style->warnings++;
#endif
    }
    /*
    * The namespaces declared by the attributes
    *  "extension-element-prefixes" and
    *  "exclude-result-prefixes" are local to *this*
    *  stylesheet tree; i.e., they are *not* visible to
    *  other stylesheet-modules, whether imported or included.
    *
    * Attribute "extension-element-prefixes".
    */
    cctxt->inode->extElemNs =
	xsltParseExtElemPrefixes(cctxt, node, NULL,
	    XSLT_ELEMENT_CATEGORY_XSLT);
    /*
    * Attribute "exclude-result-prefixes".
    */
    cctxt->inode->exclResultNs =
	xsltParseExclResultPrefixes(cctxt, node, NULL,
	    XSLT_ELEMENT_CATEGORY_XSLT);
    /*
    * Create/reuse info for the literal result element.
    */
    if (cctxt->inode->nsChanged)
	xsltLREInfoCreate(cctxt, node, 0);
    /*
    * Processed top-level elements:
    * ----------------------------
    *  xsl:variable, xsl:param (QName, in-scope ns,
    *    expression (vars allowed))
    *  xsl:attribute-set (QName, in-scope ns)
    *  xsl:strip-space, xsl:preserve-space (XPath NameTests,
    *    in-scope ns)
    *    I *think* global scope, merge with includes
    *  xsl:output (QName, in-scope ns)
    *  xsl:key (QName, in-scope ns, pattern,
    *    expression (vars *not* allowed))
    *  xsl:decimal-format (QName, needs in-scope ns)
    *  xsl:namespace-alias (in-scope ns)
    *    global scope, merge with includes
    *  xsl:template (last, QName, pattern)
    *
    * (whitespace-only text-nodes have *not* been removed
    *  yet; this will be done in xsltParseSequenceConstructor)
    *
    * Report misplaced child-nodes first.
    */
    cur = node->children;
    while (cur != NULL) {
	if (cur->type == XML_TEXT_NODE) {
	    xsltTransformError(NULL, style, cur,
		"Misplaced text node (content: '%s').\n",
		(cur->content != NULL) ? cur->content : BAD_CAST "");
	    style->errors++;
	} else if (cur->type != XML_ELEMENT_NODE) {
	    xsltTransformError(NULL, style, cur, "Misplaced node.\n");
	    style->errors++;
	}
	cur = cur->next;
    }
    /*
    * Skip xsl:import elements; they have been processed
    * already.
    */
    cur = node->children;
    while ((cur != NULL) && xsltParseFindTopLevelElem(cctxt, cur,
	    BAD_CAST "import", XSLT_NAMESPACE, 1, &cur) == 1)
	cur = cur->next;
    if (cur == NULL)
	goto exit;

    start = cur;
    /*
    * Process all top-level xsl:param elements.
    */
    while ((cur != NULL) &&
	xsltParseFindTopLevelElem(cctxt, cur,
	BAD_CAST "param", XSLT_NAMESPACE, 0, &cur) == 1)
    {
	xsltParseTopLevelXSLTElem(cctxt, cur, XSLT_FUNC_PARAM);
	cur = cur->next;
    }
    /*
    * Process all top-level xsl:variable elements.
    */
    cur = start;
    while ((cur != NULL) &&
	xsltParseFindTopLevelElem(cctxt, cur,
	BAD_CAST "variable", XSLT_NAMESPACE, 0, &cur) == 1)
    {
	xsltParseTopLevelXSLTElem(cctxt, cur, XSLT_FUNC_VARIABLE);
	cur = cur->next;
    }
    /*
    * Process all the rest of top-level elements.
    */
    cur = start;
    while (cur != NULL) {
	/*
	* Process element nodes.
	*/
	if (cur->type == XML_ELEMENT_NODE) {
	    if (cur->ns == NULL) {
		xsltTransformError(NULL, style, cur,
		    "Unexpected top-level element in no namespace.\n");
		style->errors++;
		cur = cur->next;
		continue;
	    }
	    /*
	    * Process all XSLT elements.
	    */
	    if (IS_XSLT_ELEM_FAST(cur)) {
		/*
		* xsl:import is only allowed at the beginning.
		*/
		if (IS_XSLT_NAME(cur, "import")) {
		    xsltTransformError(NULL, style, cur,
			"Misplaced xsl:import element.\n");
		    style->errors++;
		    cur = cur->next;
		    continue;
		}
		/*
		* TODO: Change the return type of the parsing functions
		*  to int.
		*/
		if (IS_XSLT_NAME(cur, "template")) {
#ifdef WITH_XSLT_DEBUG_PARSING
		    templates++;
#endif
		    /*
		    * TODO: Is the position of xsl:template in the
		    *  tree significant? If not it would be easier to
		    *  parse them at a later stage.
		    */
		    xsltParseXSLTTemplate(cctxt, cur);
		} else if (IS_XSLT_NAME(cur, "variable")) {
		    /* NOP; done already */
		} else if (IS_XSLT_NAME(cur, "param")) {
		    /* NOP; done already */
		} else if (IS_XSLT_NAME(cur, "include")) {
		    if (cur->psvi != NULL)
			xsltParseXSLTStylesheetElemCore(cctxt, cur);
		    else {
			xsltTransformError(NULL, style, cur,
			    "Internal error: "
			    "(xsltParseXSLTStylesheetElemCore) "
			    "The xsl:include element was not compiled.\n");
			style->errors++;
		    }
		} else if (IS_XSLT_NAME(cur, "strip-space")) {
		    /* No node info needed. */
		    xsltParseStylesheetStripSpace(style, cur);
		} else if (IS_XSLT_NAME(cur, "preserve-space")) {
		    /* No node info needed. */
		    xsltParseStylesheetPreserveSpace(style, cur);
		} else if (IS_XSLT_NAME(cur, "output")) {
		    /* No node-info needed. */
		    xsltParseStylesheetOutput(style, cur);
		} else if (IS_XSLT_NAME(cur, "key")) {
		    /* TODO: node-info needed for expressions ? */
		    xsltParseStylesheetKey(style, cur);
		} else if (IS_XSLT_NAME(cur, "decimal-format")) {
		    /* No node-info needed. */
		    xsltParseStylesheetDecimalFormat(style, cur);
		} else if (IS_XSLT_NAME(cur, "attribute-set")) {
		    xsltParseTopLevelXSLTElem(cctxt, cur,
			XSLT_FUNC_ATTRSET);
		} else if (IS_XSLT_NAME(cur, "namespace-alias")) {
		    /* NOP; done already */
		} else {
		    if (cctxt->inode->forwardsCompat) {
			/*
			* Forwards-compatible mode:
			*
			* XSLT-1: "if it is a top-level element and
			*  XSLT 1.0 does not allow such elements as top-level
			*  elements, then the element must be ignored along
			*  with its content;"
			*/
			/*
			* TODO: I don't think we should generate a warning.
			*/
			xsltTransformError(NULL, style, cur,
			    "Forwards-compatible mode: Ignoring unknown XSLT "
			    "element '%s'.\n", cur->name);
			style->warnings++;
		    } else {
			xsltTransformError(NULL, style, cur,
			    "Unknown XSLT element '%s'.\n", cur->name);
			style->errors++;
		    }
		}
	    } else {
		xsltTopLevelFunction function;

		/*
		* Process non-XSLT elements, which are in a
		*  non-NULL namespace.
		*/
		/*
		* QUESTION: What does xsltExtModuleTopLevelLookup()
		*  do exactly?
		*/
		function = xsltExtModuleTopLevelLookup(cur->name,
		    cur->ns->href);
		if (function != NULL)
		    function(style, cur);
#ifdef WITH_XSLT_DEBUG_PARSING
		xsltGenericDebug(xsltGenericDebugContext,
		    "xsltParseXSLTStylesheetElemCore : User-defined "
		    "data element '%s'.\n", cur->name);
#endif
	    }
	}
	cur = cur->next;
    }

exit:

#ifdef WITH_XSLT_DEBUG_PARSING
    xsltGenericDebug(xsltGenericDebugContext,
	"### END of parsing top-level elements of doc '%s'.\n",
	node->doc->URL);
    xsltGenericDebug(xsltGenericDebugContext,
	"### Templates: %d\n", templates);
#ifdef XSLT_REFACTORED
    xsltGenericDebug(xsltGenericDebugContext,
	"### Max inodes: %d\n", cctxt->maxNodeInfos);
    xsltGenericDebug(xsltGenericDebugContext,
	"### Max LREs  : %d\n", cctxt->maxLREs);
#endif /* XSLT_REFACTORED */
#endif /* WITH_XSLT_DEBUG_PARSING */

    xsltCompilerNodePop(cctxt, node);
    return(0);
}

/**
 * xsltParseXSLTStylesheet:
 * @cctxt: the compiler context
 * @node: the xsl:stylesheet/xsl:transform element-node
 *
 * Parses the xsl:stylesheet and xsl:transform element.
 *
 * <xsl:stylesheet
 *  id = id
 *  extension-element-prefixes = tokens
 *  exclude-result-prefixes = tokens
 *  version = number>
 *  <!-- Content: (xsl:import*, top-level-elements) -->
 * </xsl:stylesheet>
 *
 * BIG TODO: The xsl:include stuff.
 *
 * Called by xsltParseStylesheetTree()
 *
 * Returns 0 on success, a positive result on errors and
 *         -1 on API or internal errors.
 */
static int
xsltParseXSLTStylesheetElem(xsltCompilerCtxtPtr cctxt, xmlNodePtr node)
{
    xmlNodePtr cur, start;

    if ((cctxt == NULL) || (node == NULL) || (node->type != XML_ELEMENT_NODE))
	return(-1);

    if (node->children == NULL)
	goto exit;

    /*
    * Process top-level elements:
    *  xsl:import (must be first)
    *  xsl:include (this is just a pre-processing)
    */
    cur = node->children;
    /*
    * Process xsl:import elements.
    * XSLT 1.0: "The xsl:import element children must precede all
    *  other element children of an xsl:stylesheet element,
    *  including any xsl:include element children."
    */
    while ((cur != NULL) &&
	xsltParseFindTopLevelElem(cctxt, cur,
	    BAD_CAST "import", XSLT_NAMESPACE, 1, &cur) == 1)
    {
	if (xsltParseStylesheetImport(cctxt->style, cur) != 0) {
	    cctxt->style->errors++;
	}
	cur = cur->next;
    }
    if (cur == NULL)
	goto exit;
    start = cur;
    /*
    * Pre-process all xsl:include elements.
    */
    cur = start;
    while ((cur != NULL) &&
	xsltParseFindTopLevelElem(cctxt, cur,
	    BAD_CAST "include", XSLT_NAMESPACE, 0, &cur) == 1)
    {
	xsltParseTopLevelXSLTElem(cctxt, cur, XSLT_FUNC_INCLUDE);
	cur = cur->next;
    }
    /*
    * Pre-process all xsl:namespace-alias elements.
    * URGENT TODO: This won't work correctly: the order of included
    *  aliases and aliases defined here is significant.
    */
    cur = start;
    while ((cur != NULL) &&
	xsltParseFindTopLevelElem(cctxt, cur,
	    BAD_CAST "namespace-alias", XSLT_NAMESPACE, 0, &cur) == 1)
    {
	xsltNamespaceAlias(cctxt->style, cur);
	cur = cur->next;
    }

    if (cctxt->isInclude) {
	/*
	* If this stylesheet is intended for inclusion, then
	* we will process only imports and includes.
	*/
	goto exit;
    }
    /*
    * Now parse the rest of the top-level elements.
    */
    xsltParseXSLTStylesheetElemCore(cctxt, node);
exit:

    return(0);
}

#else /* XSLT_REFACTORED */

/**
 * xsltParseStylesheetTop:
 * @style:  the XSLT stylesheet
 * @top:  the top level "stylesheet" or "transform" element
 *
 * scan the top level elements of an XSL stylesheet
 */
static void
xsltParseStylesheetTop(xsltStylesheetPtr style, xmlNodePtr top) {
    xmlNodePtr cur;
    xmlChar *prop;
#ifdef WITH_XSLT_DEBUG_PARSING
    int templates = 0;
#endif

    if ((top == NULL) || (top->type != XML_ELEMENT_NODE))
	return;

    if (style->principal->opLimit > 0) {
        if (style->principal->opCount > style->principal->opLimit) {
            xsltTransformError(NULL, style, NULL,
                "XSLT parser operation limit exceeded\n");
	    style->errors++;
            return;
        }
    }

    prop = xmlGetNsProp(top, (const xmlChar *)"version", NULL);
    if (prop == NULL) {
	xsltTransformError(NULL, style, top,
	    "xsl:version is missing: document may not be a stylesheet\n");
	if (style != NULL) style->warnings++;
    } else {
	if ((!xmlStrEqual(prop, (const xmlChar *)"1.0")) &&
            (!xmlStrEqual(prop, (const xmlChar *)"1.1"))) {
	    xsltTransformError(NULL, style, top,
		"xsl:version: only 1.1 features are supported\n");
	    if (style != NULL) {
                style->forwards_compatible = 1;
                style->warnings++;
            }
	}
	xmlFree(prop);
    }

    /*
     * process xsl:import elements
     */
    cur = top->children;
    while (cur != NULL) {
            style->principal->opCount += 1;

	    if (IS_BLANK_NODE(cur)) {
		    cur = cur->next;
		    continue;
	    }
	    if (IS_XSLT_ELEM(cur) && IS_XSLT_NAME(cur, "import")) {
		    if (xsltParseStylesheetImport(style, cur) != 0)
			    if (style != NULL) style->errors++;
	    } else
		    break;
	    cur = cur->next;
    }

    /*
     * process other top-level elements
     */
    while (cur != NULL) {
        style->principal->opCount += 1;

	if (IS_BLANK_NODE(cur)) {
	    cur = cur->next;
	    continue;
	}
	if (cur->type == XML_TEXT_NODE) {
	    if (cur->content != NULL) {
		xsltTransformError(NULL, style, cur,
		    "misplaced text node: '%s'\n", cur->content);
	    }
	    if (style != NULL) style->errors++;
            cur = cur->next;
	    continue;
	}
	if ((cur->type == XML_ELEMENT_NODE) && (cur->ns == NULL)) {
	    xsltGenericError(xsltGenericErrorContext,
		     "Found a top-level element %s with null namespace URI\n",
		     cur->name);
	    if (style != NULL) style->errors++;
	    cur = cur->next;
	    continue;
	}
	if ((cur->type == XML_ELEMENT_NODE) && (!(IS_XSLT_ELEM(cur)))) {
	    xsltTopLevelFunction function;

	    function = xsltExtModuleTopLevelLookup(cur->name,
						   cur->ns->href);
	    if (function != NULL)
		function(style, cur);

#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
		    "xsltParseStylesheetTop : found foreign element %s\n",
		    cur->name);
#endif
            cur = cur->next;
	    continue;
	}
	if (IS_XSLT_NAME(cur, "import")) {
	    xsltTransformError(NULL, style, cur,
			"xsltParseStylesheetTop: ignoring misplaced import element\n");
	    if (style != NULL) style->errors++;
        } else if (IS_XSLT_NAME(cur, "include")) {
	    if (xsltParseStylesheetInclude(style, cur) != 0)
		if (style != NULL) style->errors++;
        } else if (IS_XSLT_NAME(cur, "strip-space")) {
	    xsltParseStylesheetStripSpace(style, cur);
        } else if (IS_XSLT_NAME(cur, "preserve-space")) {
	    xsltParseStylesheetPreserveSpace(style, cur);
        } else if (IS_XSLT_NAME(cur, "output")) {
	    xsltParseStylesheetOutput(style, cur);
        } else if (IS_XSLT_NAME(cur, "key")) {
	    xsltParseStylesheetKey(style, cur);
        } else if (IS_XSLT_NAME(cur, "decimal-format")) {
	    xsltParseStylesheetDecimalFormat(style, cur);
        } else if (IS_XSLT_NAME(cur, "attribute-set")) {
	    xsltParseStylesheetAttributeSet(style, cur);
        } else if (IS_XSLT_NAME(cur, "variable")) {
	    xsltParseGlobalVariable(style, cur);
        } else if (IS_XSLT_NAME(cur, "param")) {
	    xsltParseGlobalParam(style, cur);
        } else if (IS_XSLT_NAME(cur, "template")) {
#ifdef WITH_XSLT_DEBUG_PARSING
	    templates++;
#endif
	    xsltParseStylesheetTemplate(style, cur);
        } else if (IS_XSLT_NAME(cur, "namespace-alias")) {
	    xsltNamespaceAlias(style, cur);
	} else {
            if ((style != NULL) && (style->forwards_compatible == 0)) {
	        xsltTransformError(NULL, style, cur,
			"xsltParseStylesheetTop: unknown %s element\n",
			cur->name);
	        if (style != NULL) style->errors++;
	    }
	}
	cur = cur->next;
    }
#ifdef WITH_XSLT_DEBUG_PARSING
    xsltGenericDebug(xsltGenericDebugContext,
		    "parsed %d templates\n", templates);
#endif
}

#endif /* else of XSLT_REFACTORED */

#ifdef XSLT_REFACTORED
/**
 * xsltParseSimplifiedStylesheetTree:
 *
 * @style: the stylesheet (TODO: Change this to the compiler context)
 * @doc: the document containing the stylesheet.
 * @node: the node where the stylesheet is rooted at
 *
 * Returns 0 in case of success, a positive result if an error occurred
 *         and -1 on API and internal errors.
 */
static int
xsltParseSimplifiedStylesheetTree(xsltCompilerCtxtPtr cctxt,
				  xmlDocPtr doc,
				  xmlNodePtr node)
{
    xsltTemplatePtr templ;

    if ((cctxt == NULL) || (node == NULL))
	return(-1);

    if (xsltParseAttrXSLTVersion(cctxt, node, 0) == XSLT_ELEMENT_CATEGORY_LRE)
    {
	/*
	* TODO: Adjust report, since this might be an
	* embedded stylesheet.
	*/
	xsltTransformError(NULL, cctxt->style, node,
	    "The attribute 'xsl:version' is missing; cannot identify "
	    "this document as an XSLT stylesheet document.\n");
	cctxt->style->errors++;
	return(1);
    }

#ifdef WITH_XSLT_DEBUG_PARSING
    xsltGenericDebug(xsltGenericDebugContext,
	"xsltParseSimplifiedStylesheetTree: document is stylesheet\n");
#endif

    /*
    * Create and link the template
    */
    templ = xsltNewTemplate();
    if (templ == NULL) {
	return(-1);
    }
    templ->next = cctxt->style->templates;
    cctxt->style->templates = templ;
    templ->match = xmlStrdup(BAD_CAST "/");

    /*
    * Note that we push the document-node in this special case.
    */
    xsltCompilerNodePush(cctxt, (xmlNodePtr) doc);
    /*
    * In every case, we need to have
    * the in-scope namespaces of the element, where the
    * stylesheet is rooted at, regardless if it's an XSLT
    * instruction or a literal result instruction (or if
    * this is an embedded stylesheet).
    */
    cctxt->inode->inScopeNs =
	xsltCompilerBuildInScopeNsList(cctxt, node);
    /*
    * Parse the content and register the match-pattern.
    */
    xsltParseSequenceConstructor(cctxt, node);
    xsltCompilerNodePop(cctxt, (xmlNodePtr) doc);

    templ->elem = (xmlNodePtr) doc;
    templ->content = node;
    xsltAddTemplate(cctxt->style, templ, NULL, NULL);
    cctxt->style->literal_result = 1;
    return(0);
}

#ifdef XSLT_REFACTORED_XSLT_NSCOMP
/**
 * xsltRestoreDocumentNamespaces:
 * @ns: map of namespaces
 * @doc: the document
 *
 * Restore the namespaces for the document
 *
 * Returns 0 in case of success, -1 in case of failure
 */
int
xsltRestoreDocumentNamespaces(xsltNsMapPtr ns, xmlDocPtr doc)
{
    if (doc == NULL)
	return(-1);
    /*
    * Revert the changes we have applied to the namespace-URIs of
    * ns-decls.
    */
    while (ns != NULL) {
	if ((ns->doc == doc) && (ns->ns != NULL)) {
	    ns->ns->href = ns->origNsName;
	    ns->origNsName = NULL;
	    ns->ns = NULL;
	}
	ns = ns->next;
    }
    return(0);
}
#endif /* XSLT_REFACTORED_XSLT_NSCOMP */

/**
 * xsltParseStylesheetProcess:
 * @style:  the XSLT stylesheet (the current stylesheet-level)
 * @doc:  and xmlDoc parsed XML
 *
 * Parses an XSLT stylesheet, adding the associated structures.
 * Called by:
 *  xsltParseStylesheetImportedDoc() (xslt.c)
 *  xsltParseStylesheetInclude() (imports.c)
 *
 * Returns the value of the @style parameter if everything
 * went right, NULL if something went amiss.
 */
xsltStylesheetPtr
xsltParseStylesheetProcess(xsltStylesheetPtr style, xmlDocPtr doc)
{
    xsltCompilerCtxtPtr cctxt;
    xmlNodePtr cur;
    int oldIsSimplifiedStylesheet;

    xsltInitGlobals();

    if ((style == NULL) || (doc == NULL))
	return(NULL);

    cctxt = XSLT_CCTXT(style);

    cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
	xsltTransformError(NULL, style, (xmlNodePtr) doc,
		"xsltParseStylesheetProcess : empty stylesheet\n");
	return(NULL);
    }
    oldIsSimplifiedStylesheet = cctxt->simplified;

    if ((IS_XSLT_ELEM(cur)) &&
	((IS_XSLT_NAME(cur, "stylesheet")) ||
	 (IS_XSLT_NAME(cur, "transform")))) {
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
		"xsltParseStylesheetProcess : found stylesheet\n");
#endif
	cctxt->simplified = 0;
	style->literal_result = 0;
    } else {
	cctxt->simplified = 1;
	style->literal_result = 1;
    }
    /*
    * Pre-process the stylesheet if not already done before.
    *  This will remove PIs and comments, merge adjacent
    *  text nodes, internalize strings, etc.
    */
    if (! style->nopreproc)
	xsltParsePreprocessStylesheetTree(cctxt, cur);
    /*
    * Parse and compile the stylesheet.
    */
    if (style->literal_result == 0) {
	if (xsltParseXSLTStylesheetElem(cctxt, cur) != 0)
	    return(NULL);
    } else {
	if (xsltParseSimplifiedStylesheetTree(cctxt, doc, cur) != 0)
	    return(NULL);
    }

    cctxt->simplified = oldIsSimplifiedStylesheet;

    return(style);
}

#else /* XSLT_REFACTORED */

/**
 * xsltParseStylesheetProcess:
 * @ret:  the XSLT stylesheet (the current stylesheet-level)
 * @doc:  and xmlDoc parsed XML
 *
 * Parses an XSLT stylesheet, adding the associated structures.
 * Called by:
 *  xsltParseStylesheetImportedDoc() (xslt.c)
 *  xsltParseStylesheetInclude() (imports.c)
 *
 * Returns the value of the @style parameter if everything
 * went right, NULL if something went amiss.
 */
xsltStylesheetPtr
xsltParseStylesheetProcess(xsltStylesheetPtr ret, xmlDocPtr doc) {
    xmlNodePtr cur;

    xsltInitGlobals();

    if (doc == NULL)
	return(NULL);
    if (ret == NULL)
	return(ret);

    /*
     * First steps, remove blank nodes,
     * locate the xsl:stylesheet element and the
     * namespace declaration.
     */
    cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
	xsltTransformError(NULL, ret, (xmlNodePtr) doc,
		"xsltParseStylesheetProcess : empty stylesheet\n");
	return(NULL);
    }

    if ((IS_XSLT_ELEM(cur)) &&
	((IS_XSLT_NAME(cur, "stylesheet")) ||
	 (IS_XSLT_NAME(cur, "transform")))) {
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
		"xsltParseStylesheetProcess : found stylesheet\n");
#endif
	ret->literal_result = 0;
	xsltParseStylesheetExcludePrefix(ret, cur, 1);
	xsltParseStylesheetExtPrefix(ret, cur, 1);
    } else {
	xsltParseStylesheetExcludePrefix(ret, cur, 0);
	xsltParseStylesheetExtPrefix(ret, cur, 0);
	ret->literal_result = 1;
    }
    if (!ret->nopreproc) {
	xsltPreprocessStylesheet(ret, cur);
    }
    if (ret->literal_result == 0) {
	xsltParseStylesheetTop(ret, cur);
    } else {
	xmlChar *prop;
	xsltTemplatePtr template;

	/*
	 * the document itself might be the template, check xsl:version
	 */
	prop = xmlGetNsProp(cur, (const xmlChar *)"version", XSLT_NAMESPACE);
	if (prop == NULL) {
	    xsltTransformError(NULL, ret, cur,
		"xsltParseStylesheetProcess : document is not a stylesheet\n");
	    return(NULL);
	}

#ifdef WITH_XSLT_DEBUG_PARSING
        xsltGenericDebug(xsltGenericDebugContext,
		"xsltParseStylesheetProcess : document is stylesheet\n");
#endif

	if ((!xmlStrEqual(prop, (const xmlChar *)"1.0")) &&
            (!xmlStrEqual(prop, (const xmlChar *)"1.1"))) {
	    xsltTransformError(NULL, ret, cur,
		"xsl:version: only 1.1 features are supported\n");
            ret->forwards_compatible = 1;
	    ret->warnings++;
	}
	xmlFree(prop);

	/*
	 * Create and link the template
	 */
	template = xsltNewTemplate();
	if (template == NULL) {
	    return(NULL);
	}
	template->next = ret->templates;
	ret->templates = template;
	template->match = xmlStrdup((const xmlChar *)"/");

	/*
	 * parse the content and register the pattern
	 */
	xsltParseTemplateContent(ret, (xmlNodePtr) doc);
	template->elem = (xmlNodePtr) doc;
	template->content = doc->children;
	xsltAddTemplate(ret, template, NULL, NULL);
	ret->literal_result = 1;
    }

    return(ret);
}

#endif /* else of XSLT_REFACTORED */

/**
 * xsltParseStylesheetImportedDoc:
 * @doc:  an xmlDoc parsed XML
 * @parentStyle: pointer to the parent stylesheet (if it exists)
 *
 * parse an XSLT stylesheet building the associated structures
 * except the processing not needed for imported documents.
 *
 * Returns a new XSLT stylesheet structure.
 */

xsltStylesheetPtr
xsltParseStylesheetImportedDoc(xmlDocPtr doc,
			       xsltStylesheetPtr parentStyle) {
    xsltStylesheetPtr retStyle;

    if (doc == NULL)
	return(NULL);

    retStyle = xsltNewStylesheetInternal(parentStyle);
    if (retStyle == NULL)
	return(NULL);

    if (xsltParseStylesheetUser(retStyle, doc) != 0) {
        xsltFreeStylesheet(retStyle);
        return(NULL);
    }

    return(retStyle);
}

/**
 * xsltParseStylesheetUser:
 * @style: pointer to the stylesheet
 * @doc:  an xmlDoc parsed XML
 *
 * Parse an XSLT stylesheet with a user-provided stylesheet struct.
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltParseStylesheetUser(xsltStylesheetPtr style, xmlDocPtr doc) {
    if ((style == NULL) || (doc == NULL))
	return(-1);

    /*
    * Adjust the string dict.
    */
    if (doc->dict != NULL) {
        xmlDictFree(style->dict);
	style->dict = doc->dict;
#ifdef WITH_XSLT_DEBUG
        xsltGenericDebug(xsltGenericDebugContext,
	    "reusing dictionary from %s for stylesheet\n",
	    doc->URL);
#endif
	xmlDictReference(style->dict);
    }

    /*
    * TODO: Eliminate xsltGatherNamespaces(); we must not restrict
    *  the stylesheet to containt distinct namespace prefixes.
    */
    xsltGatherNamespaces(style);

#ifdef XSLT_REFACTORED
    {
	xsltCompilerCtxtPtr cctxt;
	xsltStylesheetPtr oldCurSheet;

	if (style->parent == NULL) {
	    xsltPrincipalStylesheetDataPtr principalData;
	    /*
	    * Create extra data for the principal stylesheet.
	    */
	    principalData = xsltNewPrincipalStylesheetData();
	    if (principalData == NULL) {
		return(-1);
	    }
	    style->principalData = principalData;
	    /*
	    * Create the compilation context
	    * ------------------------------
	    * (only once; for the principal stylesheet).
	    * This is currently the only function where the
	    * compilation context is created.
	    */
	    cctxt = xsltCompilationCtxtCreate(style);
	    if (cctxt == NULL) {
		return(-1);
	    }
	    style->compCtxt = (void *) cctxt;
	    cctxt->style = style;
	    cctxt->dict = style->dict;
	    cctxt->psData = principalData;
	    /*
	    * Push initial dummy node info.
	    */
	    cctxt->depth = -1;
	    xsltCompilerNodePush(cctxt, (xmlNodePtr) doc);
	} else {
	    /*
	    * Imported stylesheet.
	    */
	    cctxt = style->parent->compCtxt;
	    style->compCtxt = cctxt;
	}
	/*
	* Save the old and set the current stylesheet structure in the
	* compilation context.
	*/
	oldCurSheet = cctxt->style;
	cctxt->style = style;

	style->doc = doc;
	xsltParseStylesheetProcess(style, doc);

	cctxt->style = oldCurSheet;
	if (style->parent == NULL) {
	    /*
	    * Pop the initial dummy node info.
	    */
	    xsltCompilerNodePop(cctxt, (xmlNodePtr) doc);
	} else {
	    /*
	    * Clear the compilation context of imported
	    * stylesheets.
	    * TODO: really?
	    */
	    /* style->compCtxt = NULL; */
	}

#ifdef XSLT_REFACTORED_XSLT_NSCOMP
        if (style->errors != 0) {
            /*
            * Restore all changes made to namespace URIs of ns-decls.
            */
            if (cctxt->psData->nsMap)
                xsltRestoreDocumentNamespaces(cctxt->psData->nsMap, doc);
        }
#endif

        if (style->parent == NULL) {
            xsltCompilationCtxtFree(style->compCtxt);
            style->compCtxt = NULL;
        }
    }

#else /* XSLT_REFACTORED */
    /*
    * Old behaviour.
    */
    style->doc = doc;
    if (xsltParseStylesheetProcess(style, doc) == NULL) {
        style->doc = NULL;
        return(-1);
    }
#endif /* else of XSLT_REFACTORED */

    if (style->parent == NULL)
        xsltResolveStylesheetAttributeSet(style);

    if (style->errors != 0) {
        /*
        * Detach the doc from the stylesheet; otherwise the doc
        * will be freed in xsltFreeStylesheet().
        */
        style->doc = NULL;
        /*
        * Cleanup the doc if its the main stylesheet.
        */
        if (style->parent == NULL)
            xsltCleanupStylesheetTree(doc, xmlDocGetRootElement(doc));
        return(-1);
    }

    return(0);
}

/**
 * xsltParseStylesheetDoc:
 * @doc:  an xmlDoc parsed XML
 *
 * parse an XSLT stylesheet, building the associated structures.  doc
 * is kept as a reference within the returned stylesheet, so changes
 * to doc after the parsing will be reflected when the stylesheet
 * is applied, and the doc is automatically freed when the
 * stylesheet is closed.
 *
 * Returns a new XSLT stylesheet structure.
 */

xsltStylesheetPtr
xsltParseStylesheetDoc(xmlDocPtr doc) {
    xsltInitGlobals();

    return(xsltParseStylesheetImportedDoc(doc, NULL));
}

/**
 * xsltParseStylesheetFile:
 * @filename:  the filename/URL to the stylesheet
 *
 * Load and parse an XSLT stylesheet
 *
 * Returns a new XSLT stylesheet structure.
 */

xsltStylesheetPtr
xsltParseStylesheetFile(const xmlChar* filename) {
    xsltSecurityPrefsPtr sec;
    xsltStylesheetPtr ret;
    xmlDocPtr doc;

    xsltInitGlobals();

    if (filename == NULL)
	return(NULL);

#ifdef WITH_XSLT_DEBUG_PARSING
    xsltGenericDebug(xsltGenericDebugContext,
	    "xsltParseStylesheetFile : parse %s\n", filename);
#endif

    /*
     * Security framework check
     */
    sec = xsltGetDefaultSecurityPrefs();
    if (sec != NULL) {
	int res;

	res = xsltCheckRead(sec, NULL, filename);
	if (res <= 0) {
            if (res == 0)
                xsltTransformError(NULL, NULL, NULL,
                     "xsltParseStylesheetFile: read rights for %s denied\n",
                                 filename);
	    return(NULL);
	}
    }

    doc = xsltDocDefaultLoader(filename, NULL, XSLT_PARSE_OPTIONS,
                               NULL, XSLT_LOAD_START);
    if (doc == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltParseStylesheetFile : cannot parse %s\n", filename);
	return(NULL);
    }
    ret = xsltParseStylesheetDoc(doc);
    if (ret == NULL) {
	xmlFreeDoc(doc);
	return(NULL);
    }

    return(ret);
}

/************************************************************************
 *									*
 *			Handling of Stylesheet PI			*
 *									*
 ************************************************************************/

#define CUR (*cur)
#define SKIP(val) cur += (val)
#define NXT(val) cur[(val)]
#define SKIP_BLANKS						\
    while (IS_BLANK(CUR)) NEXT
#define NEXT ((*cur) ?  cur++ : cur)

/**
 * xsltParseStylesheetPI:
 * @value: the value of the PI
 *
 * This function checks that the type is text/xml and extracts
 * the URI-Reference for the stylesheet
 *
 * Returns the URI-Reference for the stylesheet or NULL (it need to
 *         be freed by the caller)
 */
static xmlChar *
xsltParseStylesheetPI(const xmlChar *value) {
    const xmlChar *cur;
    const xmlChar *start;
    xmlChar *val;
    xmlChar tmp;
    xmlChar *href = NULL;
    int isXml = 0;

    if (value == NULL)
	return(NULL);

    cur = value;
    while (CUR != 0) {
	SKIP_BLANKS;
	if ((CUR == 't') && (NXT(1) == 'y') && (NXT(2) == 'p') &&
	    (NXT(3) == 'e')) {
	    SKIP(4);
	    SKIP_BLANKS;
	    if (CUR != '=')
		continue;
	    NEXT;
	    if ((CUR != '\'') && (CUR != '"'))
		continue;
	    tmp = CUR;
	    NEXT;
	    start = cur;
	    while ((CUR != 0) && (CUR != tmp))
		NEXT;
	    if (CUR != tmp)
		continue;
	    val = xmlStrndup(start, cur - start);
	    NEXT;
	    if (val == NULL)
		return(NULL);
	    if ((xmlStrcasecmp(val, BAD_CAST "text/xml")) &&
		(xmlStrcasecmp(val, BAD_CAST "text/xsl")) &&
		(xmlStrcasecmp(val, BAD_CAST "application/xslt+xml"))) {
                xmlFree(val);
		break;
	    }
	    isXml = 1;
	    xmlFree(val);
	} else if ((CUR == 'h') && (NXT(1) == 'r') && (NXT(2) == 'e') &&
	    (NXT(3) == 'f')) {
	    SKIP(4);
	    SKIP_BLANKS;
	    if (CUR != '=')
		continue;
	    NEXT;
	    if ((CUR != '\'') && (CUR != '"'))
		continue;
	    tmp = CUR;
	    NEXT;
	    start = cur;
	    while ((CUR != 0) && (CUR != tmp))
		NEXT;
	    if (CUR != tmp)
		continue;
	    if (href == NULL)
		href = xmlStrndup(start, cur - start);
	    NEXT;
	} else {
	    while ((CUR != 0) && (!IS_BLANK(CUR)))
		NEXT;
	}

    }

    if (!isXml) {
	if (href != NULL)
	    xmlFree(href);
	href = NULL;
    }
    return(href);
}

/**
 * xsltLoadStylesheetPI:
 * @doc:  a document to process
 *
 * This function tries to locate the stylesheet PI in the given document
 * If found, and if contained within the document, it will extract
 * that subtree to build the stylesheet to process @doc (doc itself will
 * be modified). If found but referencing an external document it will
 * attempt to load it and generate a stylesheet from it. In both cases,
 * the resulting stylesheet and the document need to be freed once the
 * transformation is done.
 *
 * Returns a new XSLT stylesheet structure or NULL if not found.
 */
xsltStylesheetPtr
xsltLoadStylesheetPI(xmlDocPtr doc) {
    xmlNodePtr child;
    xsltStylesheetPtr ret = NULL;
    xmlChar *href = NULL;
    xmlURIPtr URI;

    xsltInitGlobals();

    if (doc == NULL)
	return(NULL);

    /*
     * Find the text/xml stylesheet PI id any before the root
     */
    child = doc->children;
    while ((child != NULL) && (child->type != XML_ELEMENT_NODE)) {
	if ((child->type == XML_PI_NODE) &&
	    (xmlStrEqual(child->name, BAD_CAST "xml-stylesheet"))) {
	    href = xsltParseStylesheetPI(child->content);
	    if (href != NULL)
		break;
	}
	child = child->next;
    }

    /*
     * If found check the href to select processing
     */
    if (href != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
		"xsltLoadStylesheetPI : found PI href=%s\n", href);
#endif
	URI = xmlParseURI((const char *) href);
	if (URI == NULL) {
	    xsltTransformError(NULL, NULL, child,
		    "xml-stylesheet : href %s is not valid\n", href);
	    xmlFree(href);
	    return(NULL);
	}
	if ((URI->fragment != NULL) && (URI->scheme == NULL) &&
            (URI->opaque == NULL) && (URI->authority == NULL) &&
            (URI->server == NULL) && (URI->user == NULL) &&
            (URI->path == NULL) && (URI->query == NULL)) {
	    xmlAttrPtr ID;

#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
		    "xsltLoadStylesheetPI : Reference to ID %s\n", href);
#endif
	    if (URI->fragment[0] == '#')
		ID = xmlGetID(doc, (const xmlChar *) &(URI->fragment[1]));
	    else
		ID = xmlGetID(doc, (const xmlChar *) URI->fragment);
	    if (ID == NULL) {
		xsltTransformError(NULL, NULL, child,
		    "xml-stylesheet : no ID %s found\n", URI->fragment);
	    } else {
		xmlDocPtr fake;
		xmlNodePtr subtree, newtree;
		xmlNsPtr ns;

#ifdef WITH_XSLT_DEBUG
		xsltGenericDebug(xsltGenericDebugContext,
		    "creating new document from %s for embedded stylesheet\n",
		    doc->URL);
#endif
		/*
		 * move the subtree in a new document passed to
		 * the stylesheet analyzer
		 */
		subtree = ID->parent;
		fake = xmlNewDoc(NULL);
		if (fake != NULL) {
		    /*
		    * Should the dictionary still be shared even though
		    * the nodes are being copied rather than moved?
		    */
		    fake->dict = doc->dict;
		    xmlDictReference(doc->dict);
#ifdef WITH_XSLT_DEBUG
		    xsltGenericDebug(xsltGenericDebugContext,
			"reusing dictionary from %s for embedded stylesheet\n",
			doc->URL);
#endif

		    newtree = xmlDocCopyNode(subtree, fake, 1);

		    fake->URL = xmlNodeGetBase(doc, subtree->parent);
#ifdef WITH_XSLT_DEBUG
		    xsltGenericDebug(xsltGenericDebugContext,
			"set base URI for embedded stylesheet as %s\n",
			fake->URL);
#endif

		    /*
		    * Add all namespaces in scope of embedded stylesheet to
		    * root element of newly created stylesheet document
		    */
		    while ((subtree = subtree->parent) != (xmlNodePtr)doc) {
			for (ns = subtree->ns; ns; ns = ns->next) {
			    xmlNewNs(newtree,  ns->href, ns->prefix);
			}
		    }

		    xmlAddChild((xmlNodePtr)fake, newtree);
		    ret = xsltParseStylesheetDoc(fake);
		    if (ret == NULL)
			xmlFreeDoc(fake);
		}
	    }
	} else {
	    xmlChar *URL, *base;

	    /*
	     * Reference to an external stylesheet
	     */

	    base = xmlNodeGetBase(doc, (xmlNodePtr) doc);
	    URL = xmlBuildURI(href, base);
	    if (URL != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
		xsltGenericDebug(xsltGenericDebugContext,
			"xsltLoadStylesheetPI : fetching %s\n", URL);
#endif
		ret = xsltParseStylesheetFile(URL);
		xmlFree(URL);
	    } else {
#ifdef WITH_XSLT_DEBUG_PARSING
		xsltGenericDebug(xsltGenericDebugContext,
			"xsltLoadStylesheetPI : fetching %s\n", href);
#endif
		ret = xsltParseStylesheetFile(href);
	    }
	    if (base != NULL)
		xmlFree(base);
	}
	xmlFreeURI(URI);
	xmlFree(href);
    }
    return(ret);
}
