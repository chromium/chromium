/*
 * transform.c: Implementation of the XSL Transformation 1.0 engine
 *              transform part, i.e. applying a Stylesheet to a document
 *
 * References:
 *   http://www.w3.org/TR/1999/REC-xslt-19991116
 *
 *   Michael Kay "XSLT Programmer's Reference" pp 637-643
 *   Writing Multiple Output Files
 *
 *   XSLT-1.1 Working Draft
 *   http://www.w3.org/TR/xslt11#multiple-output
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXSLT
#include "libxslt.h"

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/hash.h>
#include <libxml/encoding.h>
#include <libxml/xmlerror.h>
#include <libxml/xpath.h>
#include <libxml/parserInternals.h>
#include <libxml/xpathInternals.h>
#include <libxml/HTMLtree.h>
#include <libxml/debugXML.h>
#include <libxml/uri.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "pattern.h"
#include "transform.h"
#include "variables.h"
#include "numbersInternals.h"
#include "namespaces.h"
#include "attributes.h"
#include "templates.h"
#include "imports.h"
#include "keys.h"
#include "documents.h"
#include "extensions.h"
#include "extra.h"
#include "preproc.h"
#include "security.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_EXTRA
#define WITH_XSLT_DEBUG_PROCESS
#define WITH_XSLT_DEBUG_VARIABLE
#endif

#define XSLT_GENERATE_HTML_DOCTYPE
#ifdef XSLT_GENERATE_HTML_DOCTYPE
static int xsltGetHTMLIDs(const xmlChar *version, const xmlChar **publicID,
			  const xmlChar **systemID);
#endif

int xsltMaxDepth = 3000;
int xsltMaxVars = 15000;

/*
 * Useful macros
 */

#ifndef FALSE
# define FALSE (0 == 1)
# define TRUE (!FALSE)
#endif

#define IS_BLANK_NODE(n)						\
    (((n)->type == XML_TEXT_NODE) && (xsltIsBlank((n)->content)))


/*
* Forward declarations
*/

static xmlNsPtr
xsltCopyNamespaceListInternal(xmlNodePtr node, xmlNsPtr cur);

static xmlNodePtr
xsltCopyTree(xsltTransformContextPtr ctxt, xmlNodePtr invocNode,
	     xmlNodePtr node, xmlNodePtr insert, int isLRE,
	     int topElemVisited);

static void
xsltApplySequenceConstructor(xsltTransformContextPtr ctxt,
			     xmlNodePtr contextNode, xmlNodePtr list,
			     xsltTemplatePtr templ);

static void
xsltApplyXSLTTemplate(xsltTransformContextPtr ctxt,
		      xmlNodePtr contextNode,
		      xmlNodePtr list,
		      xsltTemplatePtr templ,
		      xsltStackElemPtr withParams);

/**
 * templPush:
 * @ctxt: the transformation context
 * @value:  the template to push on the stack
 *
 * Push a template on the stack
 *
 * Returns the new index in the stack or 0 in case of error
 */
static int
templPush(xsltTransformContextPtr ctxt, xsltTemplatePtr value)
{
    if (ctxt->templMax == 0) {
        ctxt->templMax = 4;
        ctxt->templTab =
            (xsltTemplatePtr *) xmlMalloc(ctxt->templMax *
                                          sizeof(ctxt->templTab[0]));
        if (ctxt->templTab == NULL) {
            xmlGenericError(xmlGenericErrorContext, "malloc failed !\n");
            return (0);
        }
    }
    else if (ctxt->templNr >= ctxt->templMax) {
        ctxt->templMax *= 2;
        ctxt->templTab =
            (xsltTemplatePtr *) xmlRealloc(ctxt->templTab,
                                           ctxt->templMax *
                                           sizeof(ctxt->templTab[0]));
        if (ctxt->templTab == NULL) {
            xmlGenericError(xmlGenericErrorContext, "realloc failed !\n");
            return (0);
        }
    }
    ctxt->templTab[ctxt->templNr] = value;
    ctxt->templ = value;
    return (ctxt->templNr++);
}
/**
 * templPop:
 * @ctxt: the transformation context
 *
 * Pop a template value from the stack
 *
 * Returns the stored template value
 */
static xsltTemplatePtr
templPop(xsltTransformContextPtr ctxt)
{
    xsltTemplatePtr ret;

    if (ctxt->templNr <= 0)
        return (0);
    ctxt->templNr--;
    if (ctxt->templNr > 0)
        ctxt->templ = ctxt->templTab[ctxt->templNr - 1];
    else
        ctxt->templ = (xsltTemplatePtr) 0;
    ret = ctxt->templTab[ctxt->templNr];
    ctxt->templTab[ctxt->templNr] = 0;
    return (ret);
}

/**
 * xsltLocalVariablePop:
 * @ctxt: the transformation context
 * @limitNr: number of variables which should remain
 * @level: the depth in the xsl:template's tree
 *
 * Pops all variable values at the given @depth from the stack.
 *
 * Returns the stored variable value
 * **NOTE:**
 * This is an internal routine and should not be called by users!
 */
void
xsltLocalVariablePop(xsltTransformContextPtr ctxt, int limitNr, int level)
{
    xsltStackElemPtr variable;

    if (ctxt->varsNr <= 0)
        return;

    do {
	if (ctxt->varsNr <= limitNr)
	    break;
	variable = ctxt->varsTab[ctxt->varsNr - 1];
	if (variable->level <= level)
	    break;
	if (variable->level >= 0)
	    xsltFreeStackElemList(variable);
	ctxt->varsNr--;
    } while (ctxt->varsNr != 0);
    if (ctxt->varsNr > 0)
        ctxt->vars = ctxt->varsTab[ctxt->varsNr - 1];
    else
        ctxt->vars = NULL;
}

/**
 * xsltTemplateParamsCleanup:
 *
 * Removes xsl:param and xsl:with-param items from the
 * variable-stack. Only xsl:with-param items are not freed.
 */
static void
xsltTemplateParamsCleanup(xsltTransformContextPtr ctxt)
{
    xsltStackElemPtr param;

    for (; ctxt->varsNr > ctxt->varsBase; ctxt->varsNr--) {
	param = ctxt->varsTab[ctxt->varsNr -1];
	/*
	* Free xsl:param items.
	* xsl:with-param items will have a level of -1 or -2.
	*/
	if (param->level >= 0) {
	    xsltFreeStackElemList(param);
	}
    }
    if (ctxt->varsNr > 0)
        ctxt->vars = ctxt->varsTab[ctxt->varsNr - 1];
    else
        ctxt->vars = NULL;
}

#ifdef WITH_PROFILER

/**
 * profPush:
 * @ctxt: the transformation context
 * @value:  the profiling value to push on the stack
 *
 * Push a profiling value on the stack
 *
 * Returns the new index in the stack or 0 in case of error
 */
static int
profPush(xsltTransformContextPtr ctxt, long value)
{
    if (ctxt->profMax == 0) {
        ctxt->profMax = 4;
        ctxt->profTab =
            (long *) xmlMalloc(ctxt->profMax * sizeof(ctxt->profTab[0]));
        if (ctxt->profTab == NULL) {
            xmlGenericError(xmlGenericErrorContext, "malloc failed !\n");
            return (0);
        }
    }
    else if (ctxt->profNr >= ctxt->profMax) {
        ctxt->profMax *= 2;
        ctxt->profTab =
            (long *) xmlRealloc(ctxt->profTab,
                                ctxt->profMax * sizeof(ctxt->profTab[0]));
        if (ctxt->profTab == NULL) {
            xmlGenericError(xmlGenericErrorContext, "realloc failed !\n");
            return (0);
        }
    }
    ctxt->profTab[ctxt->profNr] = value;
    ctxt->prof = value;
    return (ctxt->profNr++);
}
/**
 * profPop:
 * @ctxt: the transformation context
 *
 * Pop a profiling value from the stack
 *
 * Returns the stored profiling value
 */
static long
profPop(xsltTransformContextPtr ctxt)
{
    long ret;

    if (ctxt->profNr <= 0)
        return (0);
    ctxt->profNr--;
    if (ctxt->profNr > 0)
        ctxt->prof = ctxt->profTab[ctxt->profNr - 1];
    else
        ctxt->prof = (long) 0;
    ret = ctxt->profTab[ctxt->profNr];
    ctxt->profTab[ctxt->profNr] = 0;
    return (ret);
}

static void
profCallgraphAdd(xsltTemplatePtr templ, xsltTemplatePtr parent)
{
    int i;

    if (templ->templMax == 0) {
        templ->templMax = 4;
        templ->templCalledTab =
            (xsltTemplatePtr *) xmlMalloc(templ->templMax *
                                          sizeof(templ->templCalledTab[0]));
        templ->templCountTab =
            (int *) xmlMalloc(templ->templMax *
                                          sizeof(templ->templCountTab[0]));
        if (templ->templCalledTab == NULL || templ->templCountTab == NULL) {
            xmlGenericError(xmlGenericErrorContext, "malloc failed !\n");
            return;
        }
    }
    else if (templ->templNr >= templ->templMax) {
        templ->templMax *= 2;
        templ->templCalledTab =
            (xsltTemplatePtr *) xmlRealloc(templ->templCalledTab,
                                           templ->templMax *
                                           sizeof(templ->templCalledTab[0]));
        templ->templCountTab =
            (int *) xmlRealloc(templ->templCountTab,
                                           templ->templMax *
                                           sizeof(templ->templCountTab[0]));
        if (templ->templCalledTab == NULL || templ->templCountTab == NULL) {
            xmlGenericError(xmlGenericErrorContext, "realloc failed !\n");
            return;
        }
    }

    for (i = 0; i < templ->templNr; i++) {
        if (templ->templCalledTab[i] == parent) {
            templ->templCountTab[i]++;
            break;
        }
    }
    if (i == templ->templNr) {
        /* not found, add new one */
        templ->templCalledTab[templ->templNr] = parent;
        templ->templCountTab[templ->templNr] = 1;
        templ->templNr++;
    }
}

#endif /* WITH_PROFILER */

/**
 * xsltPreCompEval:
 * @ctxt: transform context
 * @node: context node
 * @comp: precompiled expression
 *
 * Evaluate a precompiled XPath expression.
 */
static xmlXPathObjectPtr
xsltPreCompEval(xsltTransformContextPtr ctxt, xmlNodePtr node,
                xsltStylePreCompPtr comp) {
    xmlXPathObjectPtr res;
    xmlXPathContextPtr xpctxt;
    xmlNodePtr oldXPContextNode;
    xmlNsPtr *oldXPNamespaces;
    int oldXPProximityPosition, oldXPContextSize, oldXPNsNr;

    xpctxt = ctxt->xpathCtxt;
    oldXPContextNode = xpctxt->node;
    oldXPProximityPosition = xpctxt->proximityPosition;
    oldXPContextSize = xpctxt->contextSize;
    oldXPNsNr = xpctxt->nsNr;
    oldXPNamespaces = xpctxt->namespaces;

    xpctxt->node = node;
#ifdef XSLT_REFACTORED
    if (comp->inScopeNs != NULL) {
        xpctxt->namespaces = comp->inScopeNs->list;
        xpctxt->nsNr = comp->inScopeNs->xpathNumber;
    } else {
        xpctxt->namespaces = NULL;
        xpctxt->nsNr = 0;
    }
#else
    xpctxt->namespaces = comp->nsList;
    xpctxt->nsNr = comp->nsNr;
#endif

    res = xmlXPathCompiledEval(comp->comp, xpctxt);

    xpctxt->node = oldXPContextNode;
    xpctxt->proximityPosition = oldXPProximityPosition;
    xpctxt->contextSize = oldXPContextSize;
    xpctxt->nsNr = oldXPNsNr;
    xpctxt->namespaces = oldXPNamespaces;

    return(res);
}

/**
 * xsltPreCompEvalToBoolean:
 * @ctxt: transform context
 * @node: context node
 * @comp: precompiled expression
 *
 * Evaluate a precompiled XPath expression as boolean.
 */
static int
xsltPreCompEvalToBoolean(xsltTransformContextPtr ctxt, xmlNodePtr node,
                         xsltStylePreCompPtr comp) {
    int res;
    xmlXPathContextPtr xpctxt;
    xmlNodePtr oldXPContextNode;
    xmlNsPtr *oldXPNamespaces;
    int oldXPProximityPosition, oldXPContextSize, oldXPNsNr;

    xpctxt = ctxt->xpathCtxt;
    oldXPContextNode = xpctxt->node;
    oldXPProximityPosition = xpctxt->proximityPosition;
    oldXPContextSize = xpctxt->contextSize;
    oldXPNsNr = xpctxt->nsNr;
    oldXPNamespaces = xpctxt->namespaces;

    xpctxt->node = node;
#ifdef XSLT_REFACTORED
    if (comp->inScopeNs != NULL) {
        xpctxt->namespaces = comp->inScopeNs->list;
        xpctxt->nsNr = comp->inScopeNs->xpathNumber;
    } else {
        xpctxt->namespaces = NULL;
        xpctxt->nsNr = 0;
    }
#else
    xpctxt->namespaces = comp->nsList;
    xpctxt->nsNr = comp->nsNr;
#endif

    res = xmlXPathCompiledEvalToBoolean(comp->comp, xpctxt);

    xpctxt->node = oldXPContextNode;
    xpctxt->proximityPosition = oldXPProximityPosition;
    xpctxt->contextSize = oldXPContextSize;
    xpctxt->nsNr = oldXPNsNr;
    xpctxt->namespaces = oldXPNamespaces;

    return(res);
}

/************************************************************************
 *									*
 *			XInclude default settings			*
 *									*
 ************************************************************************/

static int xsltDoXIncludeDefault = 0;

/**
 * xsltSetXIncludeDefault:
 * @xinclude: whether to do XInclude processing
 *
 * Set whether XInclude should be processed on document being loaded by default
 */
void
xsltSetXIncludeDefault(int xinclude) {
    xsltDoXIncludeDefault = (xinclude != 0);
}

/**
 * xsltGetXIncludeDefault:
 *
 * Provides the default state for XInclude processing
 *
 * Returns 0 if there is no processing 1 otherwise
 */
int
xsltGetXIncludeDefault(void) {
    return(xsltDoXIncludeDefault);
}

static unsigned long xsltDefaultTrace = (unsigned long) XSLT_TRACE_ALL;

/**
 * xsltDebugSetDefaultTrace:
 * @val: tracing level mask
 *
 * Set the default debug tracing level mask
 */
void xsltDebugSetDefaultTrace(xsltDebugTraceCodes val) {
	xsltDefaultTrace = val;
}

/**
 * xsltDebugGetDefaultTrace:
 *
 * Get the current default debug tracing level mask
 *
 * Returns the current default debug tracing level mask
 */
xsltDebugTraceCodes xsltDebugGetDefaultTrace() {
	return xsltDefaultTrace;
}

/************************************************************************
 *									*
 *			Handling of Transformation Contexts		*
 *									*
 ************************************************************************/

static xsltTransformCachePtr
xsltTransformCacheCreate(void)
{
    xsltTransformCachePtr ret;

    ret = (xsltTransformCachePtr) xmlMalloc(sizeof(xsltTransformCache));
    if (ret == NULL) {
	xsltTransformError(NULL, NULL, NULL,
	    "xsltTransformCacheCreate : malloc failed\n");
	return(NULL);
    }
    memset(ret, 0, sizeof(xsltTransformCache));
    return(ret);
}

static void
xsltTransformCacheFree(xsltTransformCachePtr cache)
{
    if (cache == NULL)
	return;
    /*
    * Free tree fragments.
    */
    if (cache->RVT) {
	xmlDocPtr tmp, cur = cache->RVT;
	while (cur) {
	    tmp = cur;
	    cur = (xmlDocPtr) cur->next;
	    if (tmp->_private != NULL) {
		/*
		* Tree the document info.
		*/
		xsltFreeDocumentKeys((xsltDocumentPtr) tmp->_private);
		xmlFree(tmp->_private);
	    }
	    xmlFreeDoc(tmp);
	}
    }
    /*
    * Free vars/params.
    */
    if (cache->stackItems) {
	xsltStackElemPtr tmp, cur = cache->stackItems;
	while (cur) {
	    tmp = cur;
	    cur = cur->next;
	    /*
	    * REVISIT TODO: Should be call a destruction-function
	    * instead?
	    */
	    xmlFree(tmp);
	}
    }
    xmlFree(cache);
}

/**
 * xsltNewTransformContext:
 * @style:  a parsed XSLT stylesheet
 * @doc:  the input document
 *
 * Create a new XSLT TransformContext
 *
 * Returns the newly allocated xsltTransformContextPtr or NULL in case of error
 */
xsltTransformContextPtr
xsltNewTransformContext(xsltStylesheetPtr style, xmlDocPtr doc) {
    xsltTransformContextPtr cur;
    xsltDocumentPtr docu;
    int i;

    xsltInitGlobals();

    cur = (xsltTransformContextPtr) xmlMalloc(sizeof(xsltTransformContext));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, (xmlNodePtr)doc,
		"xsltNewTransformContext : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltTransformContext));

    cur->cache = xsltTransformCacheCreate();
    if (cur->cache == NULL)
	goto internal_err;
    /*
     * setup of the dictionary must be done early as some of the
     * processing later like key handling may need it.
     */
    cur->dict = xmlDictCreateSub(style->dict);
    cur->internalized = ((style->internalized) && (cur->dict != NULL));
#ifdef WITH_XSLT_DEBUG
    xsltGenericDebug(xsltGenericDebugContext,
	     "Creating sub-dictionary from stylesheet for transformation\n");
#endif

    /*
     * initialize the template stack
     */
    cur->templTab = (xsltTemplatePtr *)
	        xmlMalloc(10 * sizeof(xsltTemplatePtr));
    if (cur->templTab == NULL) {
	xsltTransformError(NULL, NULL, (xmlNodePtr) doc,
		"xsltNewTransformContext: out of memory\n");
	goto internal_err;
    }
    cur->templNr = 0;
    cur->templMax = 5;
    cur->templ = NULL;
    cur->maxTemplateDepth = xsltMaxDepth;

    /*
     * initialize the variables stack
     */
    cur->varsTab = (xsltStackElemPtr *)
	        xmlMalloc(10 * sizeof(xsltStackElemPtr));
    if (cur->varsTab == NULL) {
        xmlGenericError(xmlGenericErrorContext,
		"xsltNewTransformContext: out of memory\n");
	goto internal_err;
    }
    cur->varsNr = 0;
    cur->varsMax = 10;
    cur->vars = NULL;
    cur->varsBase = 0;
    cur->maxTemplateVars = xsltMaxVars;

    /*
     * the profiling stack is not initialized by default
     */
    cur->profTab = NULL;
    cur->profNr = 0;
    cur->profMax = 0;
    cur->prof = 0;

    cur->style = style;
    xmlXPathInit();
    cur->xpathCtxt = xmlXPathNewContext(doc);
    if (cur->xpathCtxt == NULL) {
	xsltTransformError(NULL, NULL, (xmlNodePtr) doc,
		"xsltNewTransformContext : xmlXPathNewContext failed\n");
	goto internal_err;
    }
    /*
    * Create an XPath cache.
    */
    if (xmlXPathContextSetCache(cur->xpathCtxt, 1, -1, 0) == -1)
	goto internal_err;
    /*
     * Initialize the extras array
     */
    if (style->extrasNr != 0) {
	cur->extrasMax = style->extrasNr + 20;
	cur->extras = (xsltRuntimeExtraPtr)
	    xmlMalloc(cur->extrasMax * sizeof(xsltRuntimeExtra));
	if (cur->extras == NULL) {
	    xmlGenericError(xmlGenericErrorContext,
		    "xsltNewTransformContext: out of memory\n");
	    goto internal_err;
	}
	cur->extrasNr = style->extrasNr;
	for (i = 0;i < cur->extrasMax;i++) {
	    cur->extras[i].info = NULL;
	    cur->extras[i].deallocate = NULL;
	    cur->extras[i].val.ptr = NULL;
	}
    } else {
	cur->extras = NULL;
	cur->extrasNr = 0;
	cur->extrasMax = 0;
    }

    XSLT_REGISTER_VARIABLE_LOOKUP(cur);
    XSLT_REGISTER_FUNCTION_LOOKUP(cur);
    cur->xpathCtxt->nsHash = style->nsHash;
    /*
     * Initialize the registered external modules
     */
    xsltInitCtxtExts(cur);
    /*
     * Setup document element ordering for later efficiencies
     * (bug 133289)
     */
    if (xslDebugStatus == XSLT_DEBUG_NONE)
        xmlXPathOrderDocElems(doc);
    /*
     * Must set parserOptions before calling xsltNewDocument
     * (bug 164530)
     */
    cur->parserOptions = XSLT_PARSE_OPTIONS;
    docu = xsltNewDocument(cur, doc);
    if (docu == NULL) {
	xsltTransformError(cur, NULL, (xmlNodePtr)doc,
		"xsltNewTransformContext : xsltNewDocument failed\n");
	goto internal_err;
    }
    docu->main = 1;
    cur->document = docu;
    cur->inst = NULL;
    cur->outputFile = NULL;
    cur->sec = xsltGetDefaultSecurityPrefs();
    cur->debugStatus = xslDebugStatus;
    cur->traceCode = (unsigned long*) &xsltDefaultTrace;
    cur->xinclude = xsltGetXIncludeDefault();
    cur->keyInitLevel = 0;

    return(cur);

internal_err:
    if (cur != NULL)
	xsltFreeTransformContext(cur);
    return(NULL);
}

/**
 * xsltFreeTransformContext:
 * @ctxt:  an XSLT parser context
 *
 * Free up the memory allocated by @ctxt
 */
void
xsltFreeTransformContext(xsltTransformContextPtr ctxt) {
    if (ctxt == NULL)
	return;

    /*
     * Shutdown the extension modules associated to the stylesheet
     * used if needed.
     */
    xsltShutdownCtxtExts(ctxt);

    if (ctxt->xpathCtxt != NULL) {
	ctxt->xpathCtxt->nsHash = NULL;
	xmlXPathFreeContext(ctxt->xpathCtxt);
    }
    if (ctxt->templTab != NULL)
	xmlFree(ctxt->templTab);
    if (ctxt->varsTab != NULL)
	xmlFree(ctxt->varsTab);
    if (ctxt->profTab != NULL)
	xmlFree(ctxt->profTab);
    if ((ctxt->extrasNr > 0) && (ctxt->extras != NULL)) {
	int i;

	for (i = 0;i < ctxt->extrasNr;i++) {
	    if ((ctxt->extras[i].deallocate != NULL) &&
		(ctxt->extras[i].info != NULL))
		ctxt->extras[i].deallocate(ctxt->extras[i].info);
	}
	xmlFree(ctxt->extras);
    }
    xsltFreeGlobalVariables(ctxt);
    xsltFreeDocuments(ctxt);
    xsltFreeCtxtExts(ctxt);
    xsltFreeRVTs(ctxt);
    xsltTransformCacheFree(ctxt->cache);
    xmlDictFree(ctxt->dict);
#ifdef WITH_XSLT_DEBUG
    xsltGenericDebug(xsltGenericDebugContext,
                     "freeing transformation dictionary\n");
#endif
    memset(ctxt, -1, sizeof(xsltTransformContext));
    xmlFree(ctxt);
}

/************************************************************************
 *									*
 *			Copy of Nodes in an XSLT fashion		*
 *									*
 ************************************************************************/

/**
 * xsltAddChild:
 * @parent:  the parent node
 * @cur:  the child node
 *
 * Wrapper version of xmlAddChild with a more consistent behaviour on
 * error. One expect the use to be child = xsltAddChild(parent, child);
 * and the routine will take care of not leaking on errors or node merge
 *
 * Returns the child is successfully attached or NULL if merged or freed
 */
static xmlNodePtr
xsltAddChild(xmlNodePtr parent, xmlNodePtr cur) {
   xmlNodePtr ret;

   if (cur == NULL)
       return(NULL);
   if (parent == NULL) {
       xmlFreeNode(cur);
       return(NULL);
   }
   ret = xmlAddChild(parent, cur);

   return(ret);
}

/**
 * xsltAddTextString:
 * @ctxt:  a XSLT process context
 * @target:  the text node where the text will be attached
 * @string:  the text string
 * @len:  the string length in byte
 *
 * Extend the current text node with the new string, it handles coalescing
 *
 * Returns: the text node
 */
static xmlNodePtr
xsltAddTextString(xsltTransformContextPtr ctxt, xmlNodePtr target,
		  const xmlChar *string, int len) {
    /*
     * optimization
     */
    if ((len <= 0) || (string == NULL) || (target == NULL))
        return(target);

    if (ctxt->lasttext == target->content) {
        int minSize;

        /* Check for integer overflow accounting for NUL terminator. */
        if (len >= INT_MAX - ctxt->lasttuse) {
            xsltTransformError(ctxt, NULL, target,
                "xsltCopyText: text allocation failed\n");
            return(NULL);
        }
        minSize = ctxt->lasttuse + len + 1;

        if (ctxt->lasttsize < minSize) {
	    xmlChar *newbuf;
	    int size;
            int extra;

            /* Double buffer size but increase by at least 100 bytes. */
            extra = minSize < 100 ? 100 : minSize;

            /* Check for integer overflow. */
            if (extra > INT_MAX - ctxt->lasttsize) {
                size = INT_MAX;
            }
            else {
                size = ctxt->lasttsize + extra;
            }

	    newbuf = (xmlChar *) xmlRealloc(target->content,size);
	    if (newbuf == NULL) {
		xsltTransformError(ctxt, NULL, target,
		 "xsltCopyText: text allocation failed\n");
		return(NULL);
	    }
	    ctxt->lasttsize = size;
	    ctxt->lasttext = newbuf;
	    target->content = newbuf;
	}
	memcpy(&(target->content[ctxt->lasttuse]), string, len);
	ctxt->lasttuse += len;
	target->content[ctxt->lasttuse] = 0;
    } else {
	xmlNodeAddContent(target, string);
	ctxt->lasttext = target->content;
	len = xmlStrlen(target->content);
	ctxt->lasttsize = len;
	ctxt->lasttuse = len;
    }
    return(target);
}

/**
 * xsltCopyTextString:
 * @ctxt:  a XSLT process context
 * @target:  the element where the text will be attached
 * @string:  the text string
 * @noescape:  should disable-escaping be activated for this text node.
 *
 * Adds @string to a newly created or an existent text node child of
 * @target.
 *
 * Returns: the text node, where the text content of @cur is copied to.
 *          NULL in case of API or internal errors.
 */
xmlNodePtr
xsltCopyTextString(xsltTransformContextPtr ctxt, xmlNodePtr target,
	           const xmlChar *string, int noescape)
{
    xmlNodePtr copy;
    int len;

    if (string == NULL)
	return(NULL);

#ifdef WITH_XSLT_DEBUG_PROCESS
    XSLT_TRACE(ctxt,XSLT_TRACE_COPY_TEXT,xsltGenericDebug(xsltGenericDebugContext,
		     "xsltCopyTextString: copy text %s\n",
		     string));
#endif

    /*
    * Play safe and reset the merging mechanism for every new
    * target node.
    */
    if ((target == NULL) || (target->children == NULL)) {
	ctxt->lasttext = NULL;
    }

    /* handle coalescing of text nodes here */
    len = xmlStrlen(string);
    if ((ctxt->type == XSLT_OUTPUT_XML) &&
	(ctxt->style->cdataSection != NULL) &&
	(target != NULL) &&
	(target->type == XML_ELEMENT_NODE) &&
	(((target->ns == NULL) &&
	  (xmlHashLookup2(ctxt->style->cdataSection,
		          target->name, NULL) != NULL)) ||
	 ((target->ns != NULL) &&
	  (xmlHashLookup2(ctxt->style->cdataSection,
	                  target->name, target->ns->href) != NULL))))
    {
	/*
	* Process "cdata-section-elements".
	*/
	if ((target->last != NULL) &&
	    (target->last->type == XML_CDATA_SECTION_NODE))
	{
	    return(xsltAddTextString(ctxt, target->last, string, len));
	}
	copy = xmlNewCDataBlock(ctxt->output, string, len);
    } else if (noescape) {
	/*
	* Process "disable-output-escaping".
	*/
	if ((target != NULL) && (target->last != NULL) &&
	    (target->last->type == XML_TEXT_NODE) &&
	    (target->last->name == xmlStringTextNoenc))
	{
	    return(xsltAddTextString(ctxt, target->last, string, len));
	}
	copy = xmlNewTextLen(string, len);
	if (copy != NULL)
	    copy->name = xmlStringTextNoenc;
    } else {
	/*
	* Default processing.
	*/
	if ((target != NULL) && (target->last != NULL) &&
	    (target->last->type == XML_TEXT_NODE) &&
	    (target->last->name == xmlStringText)) {
	    return(xsltAddTextString(ctxt, target->last, string, len));
	}
	copy = xmlNewTextLen(string, len);
    }
    if (copy != NULL && target != NULL)
	copy = xsltAddChild(target, copy);
    if (copy != NULL) {
	ctxt->lasttext = copy->content;
	ctxt->lasttsize = len;
	ctxt->lasttuse = len;
    } else {
	xsltTransformError(ctxt, NULL, target,
			 "xsltCopyTextString: text copy failed\n");
	ctxt->lasttext = NULL;
    }
    return(copy);
}

/**
 * xsltCopyText:
 * @ctxt:  a XSLT process context
 * @target:  the element where the text will be attached
 * @cur:  the text or CDATA node
 * @interned:  the string is in the target doc dictionary
 *
 * Copy the text content of @cur and append it to @target's children.
 *
 * Returns: the text node, where the text content of @cur is copied to.
 *          NULL in case of API or internal errors.
 */
static xmlNodePtr
xsltCopyText(xsltTransformContextPtr ctxt, xmlNodePtr target,
	     xmlNodePtr cur, int interned)
{
    xmlNodePtr copy;

    if ((cur->type != XML_TEXT_NODE) &&
	(cur->type != XML_CDATA_SECTION_NODE))
	return(NULL);
    if (cur->content == NULL)
	return(NULL);

#ifdef WITH_XSLT_DEBUG_PROCESS
    if (cur->type == XML_CDATA_SECTION_NODE) {
	XSLT_TRACE(ctxt,XSLT_TRACE_COPY_TEXT,xsltGenericDebug(xsltGenericDebugContext,
			 "xsltCopyText: copy CDATA text %s\n",
			 cur->content));
    } else if (cur->name == xmlStringTextNoenc) {
	XSLT_TRACE(ctxt,XSLT_TRACE_COPY_TEXT,xsltGenericDebug(xsltGenericDebugContext,
		     "xsltCopyText: copy unescaped text %s\n",
			 cur->content));
    } else {
	XSLT_TRACE(ctxt,XSLT_TRACE_COPY_TEXT,xsltGenericDebug(xsltGenericDebugContext,
			 "xsltCopyText: copy text %s\n",
			 cur->content));
    }
#endif

    /*
    * Play save and reset the merging mechanism for every new
    * target node.
    */
    if ((target == NULL) || (target->children == NULL)) {
	ctxt->lasttext = NULL;
    }

    if ((ctxt->style->cdataSection != NULL) &&
	(ctxt->type == XSLT_OUTPUT_XML) &&
	(target != NULL) &&
	(target->type == XML_ELEMENT_NODE) &&
	(((target->ns == NULL) &&
	  (xmlHashLookup2(ctxt->style->cdataSection,
		          target->name, NULL) != NULL)) ||
	 ((target->ns != NULL) &&
	  (xmlHashLookup2(ctxt->style->cdataSection,
	                  target->name, target->ns->href) != NULL))))
    {
	/*
	* Process "cdata-section-elements".
	*/
	/*
	* OPTIMIZE TODO: xsltCopyText() is also used for attribute content.
	*/
	/*
	* TODO: Since this doesn't merge adjacent CDATA-section nodes,
	* we'll get: <![CDATA[x]]><!CDATA[y]]>.
	* TODO: Reported in #321505.
	*/
	if ((target->last != NULL) &&
	     (target->last->type == XML_CDATA_SECTION_NODE))
	{
	    /*
	    * Append to existing CDATA-section node.
	    */
	    copy = xsltAddTextString(ctxt, target->last, cur->content,
		xmlStrlen(cur->content));
	    goto exit;
	} else {
	    unsigned int len;

	    len = xmlStrlen(cur->content);
	    copy = xmlNewCDataBlock(ctxt->output, cur->content, len);
	    if (copy == NULL)
		goto exit;
	    ctxt->lasttext = copy->content;
	    ctxt->lasttsize = len;
	    ctxt->lasttuse = len;
	}
    } else if ((target != NULL) &&
	(target->last != NULL) &&
	/* both escaped or both non-escaped text-nodes */
	(((target->last->type == XML_TEXT_NODE) &&
	(target->last->name == cur->name)) ||
        /* non-escaped text nodes and CDATA-section nodes */
	(((target->last->type == XML_CDATA_SECTION_NODE) &&
	(cur->name == xmlStringTextNoenc)))))
    {
	/*
	 * we are appending to an existing text node
	 */
	copy = xsltAddTextString(ctxt, target->last, cur->content,
	    xmlStrlen(cur->content));
	goto exit;
    } else if ((interned) && (target != NULL) &&
	(target->doc != NULL) &&
	(target->doc->dict == ctxt->dict))
    {
	/*
	* TODO: DO we want to use this also for "text" output?
	*/
        copy = xmlNewTextLen(NULL, 0);
	if (copy == NULL)
	    goto exit;
	if (cur->name == xmlStringTextNoenc)
	    copy->name = xmlStringTextNoenc;

	/*
	 * Must confirm that content is in dict (bug 302821)
	 * TODO: This check should be not needed for text coming
	 * from the stylesheets
	 */
	if (xmlDictOwns(ctxt->dict, cur->content))
	    copy->content = cur->content;
	else {
	    if ((copy->content = xmlStrdup(cur->content)) == NULL)
		return NULL;
	}

	ctxt->lasttext = NULL;
    } else {
        /*
	 * normal processing. keep counters to extend the text node
	 * in xsltAddTextString if needed.
	 */
        unsigned int len;

	len = xmlStrlen(cur->content);
	copy = xmlNewTextLen(cur->content, len);
	if (copy == NULL)
	    goto exit;
	if (cur->name == xmlStringTextNoenc)
	    copy->name = xmlStringTextNoenc;
	ctxt->lasttext = copy->content;
	ctxt->lasttsize = len;
	ctxt->lasttuse = len;
    }
    if (copy != NULL) {
	if (target != NULL) {
	    copy->doc = target->doc;
	    /*
	    * MAYBE TODO: Maybe we should reset the ctxt->lasttext here
	    *  to ensure that the optimized text-merging mechanism
	    *  won't interfere with normal node-merging in any case.
	    */
	    copy = xsltAddChild(target, copy);
	}
    } else {
	xsltTransformError(ctxt, NULL, target,
			 "xsltCopyText: text copy failed\n");
    }

exit:
    if ((copy == NULL) || (copy->content == NULL)) {
	xsltTransformError(ctxt, NULL, target,
	    "Internal error in xsltCopyText(): "
	    "Failed to copy the string.\n");
	ctxt->state = XSLT_STATE_STOPPED;
    }
    return(copy);
}

/**
 * xsltShallowCopyAttr:
 * @ctxt:  a XSLT process context
 * @invocNode: responsible node in the stylesheet; used for error reports
 * @target:  the element where the attribute will be grafted
 * @attr: the attribute to be copied
 *
 * Do a copy of an attribute.
 * Called by:
 *  - xsltCopyTree()
 *  - xsltCopyOf()
 *  - xsltCopy()
 *
 * Returns: a new xmlAttrPtr, or NULL in case of error.
 */
static xmlAttrPtr
xsltShallowCopyAttr(xsltTransformContextPtr ctxt, xmlNodePtr invocNode,
	     xmlNodePtr target, xmlAttrPtr attr)
{
    xmlAttrPtr copy;
    xmlChar *value;

    if (attr == NULL)
	return(NULL);

    if (target->type != XML_ELEMENT_NODE) {
	xsltTransformError(ctxt, NULL, invocNode,
	    "Cannot add an attribute node to a non-element node.\n");
	return(NULL);
    }

    if (target->children != NULL) {
	xsltTransformError(ctxt, NULL, invocNode,
	    "Attribute nodes must be added before "
	    "any child nodes to an element.\n");
	return(NULL);
    }

    value = xmlNodeListGetString(attr->doc, attr->children, 1);
    if (attr->ns != NULL) {
	xmlNsPtr ns;

	ns = xsltGetSpecialNamespace(ctxt, invocNode,
	    attr->ns->href, attr->ns->prefix, target);
	if (ns == NULL) {
	    xsltTransformError(ctxt, NULL, invocNode,
		"Namespace fixup error: Failed to acquire an in-scope "
		"namespace binding of the copied attribute '{%s}%s'.\n",
		attr->ns->href, attr->name);
	    /*
	    * TODO: Should we just stop here?
	    */
	}
	/*
	* Note that xmlSetNsProp() will take care of duplicates
	* and assigns the new namespace even to a duplicate.
	*/
	copy = xmlSetNsProp(target, ns, attr->name, value);
    } else {
	copy = xmlSetNsProp(target, NULL, attr->name, value);
    }
    if (value != NULL)
	xmlFree(value);

    if (copy == NULL)
	return(NULL);

#if 0
    /*
    * NOTE: This was optimized according to bug #342695.
    * TODO: Can this further be optimized, if source and target
    *  share the same dict and attr->children is just 1 text node
    *  which is in the dict? How probable is such a case?
    */
    /*
    * TODO: Do we need to create an empty text node if the value
    *  is the empty string?
    */
    value = xmlNodeListGetString(attr->doc, attr->children, 1);
    if (value != NULL) {
	txtNode = xmlNewDocText(target->doc, NULL);
	if (txtNode == NULL)
	    return(NULL);
	if ((target->doc != NULL) &&
	    (target->doc->dict != NULL))
	{
	    txtNode->content =
		(xmlChar *) xmlDictLookup(target->doc->dict,
		    BAD_CAST value, -1);
	    xmlFree(value);
	} else
	    txtNode->content = value;
	copy->children = txtNode;
    }
#endif

    return(copy);
}

/**
 * xsltCopyAttrListNoOverwrite:
 * @ctxt:  a XSLT process context
 * @invocNode: responsible node in the stylesheet; used for error reports
 * @target:  the element where the new attributes will be grafted
 * @attr:  the first attribute in the list to be copied
 *
 * Copies a list of attribute nodes, starting with @attr, over to the
 * @target element node.
 *
 * Called by:
 *  - xsltCopyTree()
 *
 * Returns 0 on success and -1 on errors and internal errors.
 */
static int
xsltCopyAttrListNoOverwrite(xsltTransformContextPtr ctxt,
			    xmlNodePtr invocNode,
			    xmlNodePtr target, xmlAttrPtr attr)
{
    xmlAttrPtr copy;
    xmlNsPtr origNs = NULL, copyNs = NULL;
    xmlChar *value;

    /*
    * Don't use xmlCopyProp() here, since it will try to
    * reconciliate namespaces.
    */
    while (attr != NULL) {
	/*
	* Find a namespace node in the tree of @target.
	* Avoid searching for the same ns.
	*/
	if (attr->ns != origNs) {
	    origNs = attr->ns;
	    if (attr->ns != NULL) {
		copyNs = xsltGetSpecialNamespace(ctxt, invocNode,
		    attr->ns->href, attr->ns->prefix, target);
		if (copyNs == NULL)
		    return(-1);
	    } else
		copyNs = NULL;
	}
	/*
	 * If attribute has a value, we need to copy it (watching out
	 * for possible entities)
	 */
	if ((attr->children) && (attr->children->type == XML_TEXT_NODE) &&
            (attr->children->next == NULL)) {
            copy = xmlNewNsProp(target, copyNs, attr->name,
                                attr->children->content);
        } else if (attr->children != NULL) {
	    value = xmlNodeListGetString(attr->doc, attr->children, 1);
            copy = xmlNewNsProp(target, copyNs, attr->name, BAD_CAST value);
	    xmlFree(value);
        } else {
            copy = xmlNewNsProp(target, copyNs, attr->name, NULL);
        }

	if (copy == NULL)
	    return(-1);

	attr = attr->next;
    }
    return(0);
}

/**
 * xsltShallowCopyElem:
 * @ctxt:  the XSLT process context
 * @node:  the element node in the source tree
 *         or the Literal Result Element
 * @insert:  the parent in the result tree
 * @isLRE: if @node is a Literal Result Element
 *
 * Make a copy of the element node @node
 * and insert it as last child of @insert.
 *
 * URGENT TODO: The problem with this one (for the non-refactored code)
 * is that it is used for both, Literal Result Elements *and*
 * copying input nodes.
 *
 * BIG NOTE: This is only called for XML_ELEMENT_NODEs.
 *
 * Called from:
 *   xsltApplySequenceConstructor()
 *    (for Literal Result Elements - which is a problem)
 *   xsltCopy() (for shallow-copying elements via xsl:copy)
 *
 * Returns a pointer to the new node, or NULL in case of error
 */
static xmlNodePtr
xsltShallowCopyElem(xsltTransformContextPtr ctxt, xmlNodePtr node,
		    xmlNodePtr insert, int isLRE)
{
    xmlNodePtr copy;

    if ((node->type == XML_DTD_NODE) || (insert == NULL))
	return(NULL);
    if ((node->type == XML_TEXT_NODE) ||
	(node->type == XML_CDATA_SECTION_NODE))
	return(xsltCopyText(ctxt, insert, node, 0));

    copy = xmlDocCopyNode(node, insert->doc, 0);
    if (copy != NULL) {
	copy->doc = ctxt->output;
	copy = xsltAddChild(insert, copy);
        if (copy == NULL) {
             xsltTransformError(ctxt, NULL, node,
                "xsltShallowCopyElem: copy failed\n");
             return (copy);
        }

	if (node->type == XML_ELEMENT_NODE) {
	    /*
	     * Add namespaces as they are needed
	     */
	    if (node->nsDef != NULL) {
		/*
		* TODO: Remove the LRE case in the refactored code
		* gets enabled.
		*/
		if (isLRE)
		    xsltCopyNamespaceList(ctxt, copy, node->nsDef);
		else
		    xsltCopyNamespaceListInternal(copy, node->nsDef);
	    }

	    /*
	    * URGENT TODO: The problem with this is that it does not
	    *  copy over all namespace nodes in scope.
	    *  The damn thing about this is, that we would need to
	    *  use the xmlGetNsList(), for every single node; this is
	    *  also done in xsltCopyTree(), but only for the top node.
	    */
	    if (node->ns != NULL) {
		if (isLRE) {
		    /*
		    * REVISIT TODO: Since the non-refactored code still does
		    *  ns-aliasing, we need to call xsltGetNamespace() here.
		    *  Remove this when ready.
		    */
		    copy->ns = xsltGetNamespace(ctxt, node, node->ns, copy);
		} else {
		    copy->ns = xsltGetSpecialNamespace(ctxt,
			node, node->ns->href, node->ns->prefix, copy);

		}
	    } else if ((insert->type == XML_ELEMENT_NODE) &&
		       (insert->ns != NULL))
	    {
		/*
		* "Undeclare" the default namespace.
		*/
		xsltGetSpecialNamespace(ctxt, node, NULL, NULL, copy);
	    }
	}
    } else {
	xsltTransformError(ctxt, NULL, node,
		"xsltShallowCopyElem: copy %s failed\n", node->name);
    }
    return(copy);
}

/**
 * xsltCopyTreeList:
 * @ctxt:  a XSLT process context
 * @invocNode: responsible node in the stylesheet; used for error reports
 * @list:  the list of element nodes in the source tree.
 * @insert:  the parent in the result tree.
 * @isLRE:  is this a literal result element list
 * @topElemVisited: indicates if a top-most element was already processed
 *
 * Make a copy of the full list of tree @list
 * and insert it as last children of @insert
 *
 * NOTE: Not to be used for Literal Result Elements.
 *
 * Used by:
 *  - xsltCopyOf()
 *
 * Returns a pointer to the new list, or NULL in case of error
 */
static xmlNodePtr
xsltCopyTreeList(xsltTransformContextPtr ctxt, xmlNodePtr invocNode,
		 xmlNodePtr list,
		 xmlNodePtr insert, int isLRE, int topElemVisited)
{
    xmlNodePtr copy, ret = NULL;

    while (list != NULL) {
	copy = xsltCopyTree(ctxt, invocNode,
	    list, insert, isLRE, topElemVisited);
	if (copy != NULL) {
	    if (ret == NULL) {
		ret = copy;
	    }
	}
	list = list->next;
    }
    return(ret);
}

/**
 * xsltCopyNamespaceListInternal:
 * @node:  the target node
 * @cur:  the first namespace
 *
 * Do a copy of a namespace list. If @node is non-NULL the
 * new namespaces are added automatically.
 * Called by:
 *   xsltCopyTree()
 *
 * QUESTION: What is the exact difference between this function
 *  and xsltCopyNamespaceList() in "namespaces.c"?
 * ANSWER: xsltCopyNamespaceList() tries to apply ns-aliases.
 *
 * Returns: a new xmlNsPtr, or NULL in case of error.
 */
static xmlNsPtr
xsltCopyNamespaceListInternal(xmlNodePtr elem, xmlNsPtr ns) {
    xmlNsPtr ret = NULL;
    xmlNsPtr p = NULL, q, luNs;

    if (ns == NULL)
	return(NULL);
    /*
     * One can add namespaces only on element nodes
     */
    if ((elem != NULL) && (elem->type != XML_ELEMENT_NODE))
	elem = NULL;

    do {
	if (ns->type != XML_NAMESPACE_DECL)
	    break;
	/*
	 * Avoid duplicating namespace declarations on the tree.
	 */
	if (elem != NULL) {
	    if ((elem->ns != NULL) &&
		xmlStrEqual(elem->ns->prefix, ns->prefix) &&
		xmlStrEqual(elem->ns->href, ns->href))
	    {
		ns = ns->next;
		continue;
	    }
	    luNs = xmlSearchNs(elem->doc, elem, ns->prefix);
	    if ((luNs != NULL) && (xmlStrEqual(luNs->href, ns->href)))
	    {
		ns = ns->next;
		continue;
	    }
	}
	q = xmlNewNs(elem, ns->href, ns->prefix);
	if (p == NULL) {
	    ret = p = q;
	} else if (q != NULL) {
	    p->next = q;
	    p = q;
	}
	ns = ns->next;
    } while (ns != NULL);
    return(ret);
}

/**
 * xsltShallowCopyNsNode:
 * @ctxt:  the XSLT transformation context
 * @invocNode: responsible node in the stylesheet; used for error reports
 * @insert:  the target element node in the result tree
 * @ns: the namespace node
 *
 * This is used for copying ns-nodes with xsl:copy-of and xsl:copy.
 *
 * Returns a new/existing ns-node, or NULL.
 */
static xmlNsPtr
xsltShallowCopyNsNode(xsltTransformContextPtr ctxt,
		      xmlNodePtr invocNode,
		      xmlNodePtr insert,
		      xmlNsPtr ns)
{
    /*
     * TODO: Contrary to header comments, this is declared as int.
     * be modified to return a node pointer, or NULL if any error
     */
    xmlNsPtr tmpns;

    if ((insert == NULL) || (insert->type != XML_ELEMENT_NODE))
	return(NULL);

    if (insert->children != NULL) {
	xsltTransformError(ctxt, NULL, invocNode,
	    "Namespace nodes must be added before "
	    "any child nodes are added to an element.\n");
	return(NULL);
    }
    /*
     * BIG NOTE: Xalan-J simply overwrites any ns-decls with
     * an equal prefix. We definitively won't do that.
     *
     * MSXML 4.0 and the .NET ignores ns-decls for which an
     * equal prefix is already in use.
     *
     * Saxon raises an error like:
     * "net.sf.saxon.xpath.DynamicError: Cannot create two namespace
     * nodes with the same name".
     *
     * NOTE: We'll currently follow MSXML here.
     * REVISIT TODO: Check if it's better to follow Saxon here.
     */
    if (ns->prefix == NULL) {
	/*
	* If we are adding ns-nodes to an element using e.g.
	* <xsl:copy-of select="/foo/namespace::*">, then we need
	* to ensure that we don't incorrectly declare a default
	* namespace on an element in no namespace, which otherwise
	* would move the element incorrectly into a namespace, if
	* the node tree is serialized.
	*/
	if (insert->ns == NULL)
	    goto occupied;
    } else if ((ns->prefix[0] == 'x') &&
	xmlStrEqual(ns->prefix, BAD_CAST "xml"))
    {
	/*
	* The XML namespace is built in.
	*/
	return(NULL);
    }

    if (insert->nsDef != NULL) {
	tmpns = insert->nsDef;
	do {
	    if ((tmpns->prefix == NULL) == (ns->prefix == NULL)) {
		if ((tmpns->prefix == ns->prefix) ||
		    xmlStrEqual(tmpns->prefix, ns->prefix))
		{
		    /*
		    * Same prefix.
		    */
		    if (xmlStrEqual(tmpns->href, ns->href))
			return(NULL);
		    goto occupied;
		}
	    }
	    tmpns = tmpns->next;
	} while (tmpns != NULL);
    }
    tmpns = xmlSearchNs(insert->doc, insert, ns->prefix);
    if ((tmpns != NULL) && xmlStrEqual(tmpns->href, ns->href))
	return(NULL);
    /*
    * Declare a new namespace.
    * TODO: The problem (wrt efficiency) with this xmlNewNs() is
    * that it will again search the already declared namespaces
    * for a duplicate :-/
    */
    return(xmlNewNs(insert, ns->href, ns->prefix));

occupied:
    /*
    * TODO: We could as well raise an error here (like Saxon does),
    * or at least generate a warning.
    */
    return(NULL);
}

/**
 * xsltCopyTree:
 * @ctxt:  the XSLT transformation context
 * @invocNode: responsible node in the stylesheet; used for error reports
 * @node:  the element node in the source tree
 * @insert:  the parent in the result tree
 * @isLRE:  indicates if @node is a Literal Result Element
 * @topElemVisited: indicates if a top-most element was already processed
 *
 * Make a copy of the full tree under the element node @node
 * and insert it as last child of @insert
 *
 * NOTE: Not to be used for Literal Result Elements.
 *
 * Used by:
 *  - xsltCopyOf()
 *
 * Returns a pointer to the new tree, or NULL in case of error
 */
static xmlNodePtr
xsltCopyTree(xsltTransformContextPtr ctxt, xmlNodePtr invocNode,
	     xmlNodePtr node, xmlNodePtr insert, int isLRE,
	     int topElemVisited)
{
    xmlNodePtr copy;

    if (node == NULL)
	return(NULL);
    switch (node->type) {
        case XML_ELEMENT_NODE:
        case XML_ENTITY_REF_NODE:
        case XML_ENTITY_NODE:
        case XML_PI_NODE:
        case XML_COMMENT_NODE:
        case XML_DOCUMENT_NODE:
        case XML_HTML_DOCUMENT_NODE:
#ifdef LIBXML_DOCB_ENABLED
        case XML_DOCB_DOCUMENT_NODE:
#endif
	    break;
        case XML_TEXT_NODE: {
	    int noenc = (node->name == xmlStringTextNoenc);
	    return(xsltCopyTextString(ctxt, insert, node->content, noenc));
	    }
        case XML_CDATA_SECTION_NODE:
	    return(xsltCopyTextString(ctxt, insert, node->content, 0));
        case XML_ATTRIBUTE_NODE:
	    return((xmlNodePtr)
		xsltShallowCopyAttr(ctxt, invocNode, insert, (xmlAttrPtr) node));
        case XML_NAMESPACE_DECL:
	    return((xmlNodePtr) xsltShallowCopyNsNode(ctxt, invocNode,
		insert, (xmlNsPtr) node));

        case XML_DOCUMENT_TYPE_NODE:
        case XML_DOCUMENT_FRAG_NODE:
        case XML_NOTATION_NODE:
        case XML_DTD_NODE:
        case XML_ELEMENT_DECL:
        case XML_ATTRIBUTE_DECL:
        case XML_ENTITY_DECL:
        case XML_XINCLUDE_START:
        case XML_XINCLUDE_END:
            return(NULL);
    }
    if (XSLT_IS_RES_TREE_FRAG(node)) {
	if (node->children != NULL)
	    copy = xsltCopyTreeList(ctxt, invocNode,
		node->children, insert, 0, 0);
	else
	    copy = NULL;
	return(copy);
    }
    copy = xmlDocCopyNode(node, insert->doc, 0);
    if (copy != NULL) {
	copy->doc = ctxt->output;
	copy = xsltAddChild(insert, copy);
        if (copy == NULL) {
            xsltTransformError(ctxt, NULL, invocNode,
            "xsltCopyTree: Copying of '%s' failed.\n", node->name);
            return (copy);
        }
	/*
	 * The node may have been coalesced into another text node.
	 */
	if (insert->last != copy)
	    return(insert->last);
	copy->next = NULL;

	if (node->type == XML_ELEMENT_NODE) {
	    /*
	    * Copy in-scope namespace nodes.
	    *
	    * REVISIT: Since we try to reuse existing in-scope ns-decls by
	    *  using xmlSearchNsByHref(), this will eventually change
	    *  the prefix of an original ns-binding; thus it might
	    *  break QNames in element/attribute content.
	    * OPTIMIZE TODO: If we had a xmlNsPtr * on the transformation
	    *  context, plus a ns-lookup function, which writes directly
	    *  to a given list, then we wouldn't need to create/free the
	    *  nsList every time.
	    */
	    if ((topElemVisited == 0) &&
		(node->parent != NULL) &&
		(node->parent->type != XML_DOCUMENT_NODE) &&
		(node->parent->type != XML_HTML_DOCUMENT_NODE))
	    {
		xmlNsPtr *nsList, *curns, ns;

		/*
		* If this is a top-most element in a tree to be
		* copied, then we need to ensure that all in-scope
		* namespaces are copied over. For nodes deeper in the
		* tree, it is sufficient to reconcile only the ns-decls
		* (node->nsDef entries).
		*/

		nsList = xmlGetNsList(node->doc, node);
		if (nsList != NULL) {
		    curns = nsList;
		    do {
			/*
			* Search by prefix first in order to break as less
			* QNames in element/attribute content as possible.
			*/
			ns = xmlSearchNs(insert->doc, insert,
			    (*curns)->prefix);

			if ((ns == NULL) ||
			    (! xmlStrEqual(ns->href, (*curns)->href)))
			{
			    ns = NULL;
			    /*
			    * Search by namespace name.
			    * REVISIT TODO: Currently disabled.
			    */
#if 0
			    ns = xmlSearchNsByHref(insert->doc,
				insert, (*curns)->href);
#endif
			}
			if (ns == NULL) {
			    /*
			    * Declare a new namespace on the copied element.
			    */
			    ns = xmlNewNs(copy, (*curns)->href,
				(*curns)->prefix);
			    /* TODO: Handle errors */
			}
			if (node->ns == *curns) {
			    /*
			    * If this was the original's namespace then set
			    * the generated counterpart on the copy.
			    */
			    copy->ns = ns;
			}
			curns++;
		    } while (*curns != NULL);
		    xmlFree(nsList);
		}
	    } else if (node->nsDef != NULL) {
		/*
		* Copy over all namespace declaration attributes.
		*/
		if (node->nsDef != NULL) {
		    if (isLRE)
			xsltCopyNamespaceList(ctxt, copy, node->nsDef);
		    else
			xsltCopyNamespaceListInternal(copy, node->nsDef);
		}
	    }
	    /*
	    * Set the namespace.
	    */
	    if (node->ns != NULL) {
		if (copy->ns == NULL) {
		    /*
		    * This will map copy->ns to one of the newly created
		    * in-scope ns-decls, OR create a new ns-decl on @copy.
		    */
		    copy->ns = xsltGetSpecialNamespace(ctxt, invocNode,
			node->ns->href, node->ns->prefix, copy);
		}
	    } else if ((insert->type == XML_ELEMENT_NODE) &&
		(insert->ns != NULL))
	    {
		/*
		* "Undeclare" the default namespace on @copy with xmlns="".
		*/
		xsltGetSpecialNamespace(ctxt, invocNode, NULL, NULL, copy);
	    }
	    /*
	    * Copy attribute nodes.
	    */
	    if (node->properties != NULL) {
		xsltCopyAttrListNoOverwrite(ctxt, invocNode,
		    copy, node->properties);
	    }
	    if (topElemVisited == 0)
		topElemVisited = 1;
	}
	/*
	* Copy the subtree.
	*/
	if (node->children != NULL) {
	    xsltCopyTreeList(ctxt, invocNode,
		node->children, copy, isLRE, topElemVisited);
	}
    } else {
	xsltTransformError(ctxt, NULL, invocNode,
	    "xsltCopyTree: Copying of '%s' failed.\n", node->name);
    }
    return(copy);
}

/************************************************************************
 *									*
 *		Error/fallback processing				*
 *									*
 ************************************************************************/

/**
 * xsltApplyFallbacks:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @inst:  the node generating the error
 *
 * Process possible xsl:fallback nodes present under @inst
 *
 * Returns the number of xsl:fallback element found and processed
 */
static int
xsltApplyFallbacks(xsltTransformContextPtr ctxt, xmlNodePtr node,
	           xmlNodePtr inst) {

    xmlNodePtr child;
    int ret = 0;

    if ((ctxt == NULL) || (node == NULL) || (inst == NULL) ||
	(inst->children == NULL))
	return(0);

    child = inst->children;
    while (child != NULL) {
        if ((IS_XSLT_ELEM(child)) &&
            (xmlStrEqual(child->name, BAD_CAST "fallback"))) {
#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
			     "applying xsl:fallback\n");
#endif
	    ret++;
	    xsltApplySequenceConstructor(ctxt, node, child->children,
		NULL);
	}
	child = child->next;
    }
    return(ret);
}

/************************************************************************
 *									*
 *			Default processing				*
 *									*
 ************************************************************************/

/**
 * xsltDefaultProcessOneNode:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @params: extra parameters passed to the template if any
 *
 * Process the source node with the default built-in template rule:
 * <xsl:template match="*|/">
 *   <xsl:apply-templates/>
 * </xsl:template>
 *
 * and
 *
 * <xsl:template match="text()|@*">
 *   <xsl:value-of select="."/>
 * </xsl:template>
 *
 * Note also that namespace declarations are copied directly:
 *
 * the built-in template rule is the only template rule that is applied
 * for namespace nodes.
 */
static void
xsltDefaultProcessOneNode(xsltTransformContextPtr ctxt, xmlNodePtr node,
			  xsltStackElemPtr params) {
    xmlNodePtr copy;
    xmlNodePtr cur;
    int nbchild = 0, oldSize;
    int childno = 0, oldPos;
    xsltTemplatePtr template;

    CHECK_STOPPED;
    /*
     * Handling of leaves
     */
    switch (node->type) {
	case XML_DOCUMENT_NODE:
	case XML_HTML_DOCUMENT_NODE:
	case XML_ELEMENT_NODE:
	    break;
	case XML_CDATA_SECTION_NODE:
#ifdef WITH_XSLT_DEBUG_PROCESS
	    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltDefaultProcessOneNode: copy CDATA %s\n",
		node->content));
#endif
	    copy = xsltCopyText(ctxt, ctxt->insert, node, 0);
	    if (copy == NULL) {
		xsltTransformError(ctxt, NULL, node,
		 "xsltDefaultProcessOneNode: cdata copy failed\n");
	    }
	    return;
	case XML_TEXT_NODE:
#ifdef WITH_XSLT_DEBUG_PROCESS
	    if (node->content == NULL) {
		XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
		 "xsltDefaultProcessOneNode: copy empty text\n"));
		return;
	    } else {
		XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
		 "xsltDefaultProcessOneNode: copy text %s\n",
			node->content));
            }
#endif
	    copy = xsltCopyText(ctxt, ctxt->insert, node, 0);
	    if (copy == NULL) {
		xsltTransformError(ctxt, NULL, node,
		 "xsltDefaultProcessOneNode: text copy failed\n");
	    }
	    return;
	case XML_ATTRIBUTE_NODE:
	    cur = node->children;
	    while ((cur != NULL) && (cur->type != XML_TEXT_NODE))
		cur = cur->next;
	    if (cur == NULL) {
		xsltTransformError(ctxt, NULL, node,
		 "xsltDefaultProcessOneNode: no text for attribute\n");
	    } else {
#ifdef WITH_XSLT_DEBUG_PROCESS
		if (cur->content == NULL) {
		    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
		     "xsltDefaultProcessOneNode: copy empty text\n"));
		} else {
		    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
		     "xsltDefaultProcessOneNode: copy text %s\n",
			cur->content));
                }
#endif
		copy = xsltCopyText(ctxt, ctxt->insert, cur, 0);
		if (copy == NULL) {
		    xsltTransformError(ctxt, NULL, node,
		     "xsltDefaultProcessOneNode: text copy failed\n");
		}
	    }
	    return;
	default:
	    return;
    }
    /*
     * Handling of Elements: first pass, counting
     */
    cur = node->children;
    while (cur != NULL) {
	if (IS_XSLT_REAL_NODE(cur))
	    nbchild++;
	cur = cur->next;
    }

    /*
     * Handling of Elements: second pass, actual processing
     *
     * Note that params are passed to the next template. This matches
     * XSLT 2.0 behavior but doesn't conform to XSLT 1.0.
     */
    oldSize = ctxt->xpathCtxt->contextSize;
    oldPos = ctxt->xpathCtxt->proximityPosition;
    cur = node->children;
    while (cur != NULL) {
	childno++;
	switch (cur->type) {
	    case XML_DOCUMENT_NODE:
	    case XML_HTML_DOCUMENT_NODE:
	    case XML_ELEMENT_NODE:
		ctxt->xpathCtxt->contextSize = nbchild;
		ctxt->xpathCtxt->proximityPosition = childno;
		xsltProcessOneNode(ctxt, cur, params);
		break;
	    case XML_CDATA_SECTION_NODE:
		template = xsltGetTemplate(ctxt, cur, NULL);
		if (template) {
#ifdef WITH_XSLT_DEBUG_PROCESS
		    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
		 "xsltDefaultProcessOneNode: applying template for CDATA %s\n",
				     cur->content));
#endif
		    /*
		    * Instantiate the xsl:template.
		    */
		    xsltApplyXSLTTemplate(ctxt, cur, template->content,
			template, params);
		} else /* if (ctxt->mode == NULL) */ {
#ifdef WITH_XSLT_DEBUG_PROCESS
		    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
		     "xsltDefaultProcessOneNode: copy CDATA %s\n",
				     cur->content));
#endif
		    copy = xsltCopyText(ctxt, ctxt->insert, cur, 0);
		    if (copy == NULL) {
			xsltTransformError(ctxt, NULL, cur,
			    "xsltDefaultProcessOneNode: cdata copy failed\n");
		    }
		}
		break;
	    case XML_TEXT_NODE:
		template = xsltGetTemplate(ctxt, cur, NULL);
		if (template) {
#ifdef WITH_XSLT_DEBUG_PROCESS
		    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltDefaultProcessOneNode: applying template for text %s\n",
				     cur->content));
#endif
		    ctxt->xpathCtxt->contextSize = nbchild;
		    ctxt->xpathCtxt->proximityPosition = childno;
		    /*
		    * Instantiate the xsl:template.
		    */
		    xsltApplyXSLTTemplate(ctxt, cur, template->content,
			template, params);
		} else /* if (ctxt->mode == NULL) */ {
#ifdef WITH_XSLT_DEBUG_PROCESS
		    if (cur->content == NULL) {
			XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
			 "xsltDefaultProcessOneNode: copy empty text\n"));
		    } else {
			XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
		     "xsltDefaultProcessOneNode: copy text %s\n",
					 cur->content));
                    }
#endif
		    copy = xsltCopyText(ctxt, ctxt->insert, cur, 0);
		    if (copy == NULL) {
			xsltTransformError(ctxt, NULL, cur,
			    "xsltDefaultProcessOneNode: text copy failed\n");
		    }
		}
		break;
	    case XML_PI_NODE:
	    case XML_COMMENT_NODE:
		template = xsltGetTemplate(ctxt, cur, NULL);
		if (template) {
#ifdef WITH_XSLT_DEBUG_PROCESS
		    if (cur->type == XML_PI_NODE) {
			XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
		     "xsltDefaultProcessOneNode: template found for PI %s\n",
			                 cur->name));
		    } else if (cur->type == XML_COMMENT_NODE) {
			XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
		     "xsltDefaultProcessOneNode: template found for comment\n"));
                    }
#endif
		    ctxt->xpathCtxt->contextSize = nbchild;
		    ctxt->xpathCtxt->proximityPosition = childno;
		    /*
		    * Instantiate the xsl:template.
		    */
		    xsltApplyXSLTTemplate(ctxt, cur, template->content,
			template, params);
		}
		break;
	    default:
		break;
	}
	cur = cur->next;
    }
    ctxt->xpathCtxt->contextSize = oldSize;
    ctxt->xpathCtxt->proximityPosition = oldPos;
}

/**
 * xsltProcessOneNode:
 * @ctxt:  a XSLT process context
 * @contextNode:  the "current node" in the source tree
 * @withParams:  extra parameters (e.g. xsl:with-param) passed to the
 *               template if any
 *
 * Process the source node.
 */
void
xsltProcessOneNode(xsltTransformContextPtr ctxt, xmlNodePtr contextNode,
	           xsltStackElemPtr withParams)
{
    xsltTemplatePtr templ;
    xmlNodePtr oldNode;

    templ = xsltGetTemplate(ctxt, contextNode, NULL);
    /*
     * If no template is found, apply the default rule.
     */
    if (templ == NULL) {
#ifdef WITH_XSLT_DEBUG_PROCESS
	if (contextNode->type == XML_DOCUMENT_NODE) {
	    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltProcessOneNode: no template found for /\n"));
	} else if (contextNode->type == XML_CDATA_SECTION_NODE) {
	    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltProcessOneNode: no template found for CDATA\n"));
	} else if (contextNode->type == XML_ATTRIBUTE_NODE) {
	    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltProcessOneNode: no template found for attribute %s\n",
	                     ((xmlAttrPtr) contextNode)->name));
	} else  {
	    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltProcessOneNode: no template found for %s\n", contextNode->name));
        }
#endif
	oldNode = ctxt->node;
	ctxt->node = contextNode;
	xsltDefaultProcessOneNode(ctxt, contextNode, withParams);
	ctxt->node = oldNode;
	return;
    }

    if (contextNode->type == XML_ATTRIBUTE_NODE) {
	xsltTemplatePtr oldCurTempRule = ctxt->currentTemplateRule;
	/*
	* Set the "current template rule".
	*/
	ctxt->currentTemplateRule = templ;

#ifdef WITH_XSLT_DEBUG_PROCESS
	XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltProcessOneNode: applying template '%s' for attribute %s\n",
	                 templ->match, contextNode->name));
#endif
	xsltApplyXSLTTemplate(ctxt, contextNode, templ->content, templ, withParams);

	ctxt->currentTemplateRule = oldCurTempRule;
    } else {
	xsltTemplatePtr oldCurTempRule = ctxt->currentTemplateRule;
	/*
	* Set the "current template rule".
	*/
	ctxt->currentTemplateRule = templ;

#ifdef WITH_XSLT_DEBUG_PROCESS
	if (contextNode->type == XML_DOCUMENT_NODE) {
	    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltProcessOneNode: applying template '%s' for /\n",
	                     templ->match));
	} else {
	    XSLT_TRACE(ctxt,XSLT_TRACE_PROCESS_NODE,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltProcessOneNode: applying template '%s' for %s\n",
	                     templ->match, contextNode->name));
        }
#endif
	xsltApplyXSLTTemplate(ctxt, contextNode, templ->content, templ, withParams);

	ctxt->currentTemplateRule = oldCurTempRule;
    }
}

#ifdef WITH_DEBUGGER
static xmlNodePtr
xsltDebuggerStartSequenceConstructor(xsltTransformContextPtr ctxt,
				     xmlNodePtr contextNode,
				     xmlNodePtr list,
				     xsltTemplatePtr templ,
				     int *addCallResult)
{
    xmlNodePtr debugedNode = NULL;

    if (ctxt->debugStatus != XSLT_DEBUG_NONE) {
        if (templ) {
            *addCallResult = xslAddCall(templ, templ->elem);
        } else {
            *addCallResult = xslAddCall(NULL, list);
        }
        switch (ctxt->debugStatus) {
            case XSLT_DEBUG_RUN_RESTART:
            case XSLT_DEBUG_QUIT:
                if (*addCallResult)
                    xslDropCall();
                return(NULL);
        }
        if (templ) {
            xslHandleDebugger(templ->elem, contextNode, templ, ctxt);
            debugedNode = templ->elem;
        } else if (list) {
            xslHandleDebugger(list, contextNode, templ, ctxt);
            debugedNode = list;
        } else if (ctxt->inst) {
            xslHandleDebugger(ctxt->inst, contextNode, templ, ctxt);
            debugedNode = ctxt->inst;
        }
    }
    return(debugedNode);
}
#endif /* WITH_DEBUGGER */

/**
 * xsltLocalVariablePush:
 * @ctxt: the transformation context
 * @variable: variable to be pushed to the variable stack
 * @level: new value for variable's level
 *
 * Places the variable onto the local variable stack
 *
 * Returns: 0 for success, -1 for any error
 * **NOTE:**
 * This is an internal routine and should not be called by users!
 */
int
xsltLocalVariablePush(xsltTransformContextPtr ctxt,
		      xsltStackElemPtr variable,
		      int level)
{
    if (ctxt->varsMax == 0) {
	ctxt->varsMax = 10;
	ctxt->varsTab =
	    (xsltStackElemPtr *) xmlMalloc(ctxt->varsMax *
	    sizeof(ctxt->varsTab[0]));
	if (ctxt->varsTab == NULL) {
	    xmlGenericError(xmlGenericErrorContext, "malloc failed !\n");
	    return (-1);
	}
    }
    if (ctxt->varsNr >= ctxt->varsMax) {
	ctxt->varsMax *= 2;
	ctxt->varsTab =
	    (xsltStackElemPtr *) xmlRealloc(ctxt->varsTab,
	    ctxt->varsMax *
	    sizeof(ctxt->varsTab[0]));
	if (ctxt->varsTab == NULL) {
	    xmlGenericError(xmlGenericErrorContext, "realloc failed !\n");
	    return (-1);
	}
    }
    ctxt->varsTab[ctxt->varsNr++] = variable;
    ctxt->vars = variable;
    variable->level = level;
    return(0);
}

/**
 * xsltReleaseLocalRVTs:
 *
 * Fragments which are results of extension instructions
 * are preserved; all other fragments are freed/cached.
 */
static void
xsltReleaseLocalRVTs(xsltTransformContextPtr ctxt, xmlDocPtr base)
{
    xmlDocPtr cur = ctxt->localRVT, tmp;

    if (cur == base)
        return;
    if (cur->prev != NULL)
        xsltTransformError(ctxt, NULL, NULL, "localRVT not head of list\n");

    /* Reset localRVT early because some RVTs might be registered again. */
    ctxt->localRVT = base;
    if (base != NULL)
        base->prev = NULL;

    do {
        tmp = cur;
        cur = (xmlDocPtr) cur->next;
        if (tmp->psvi == XSLT_RVT_LOCAL) {
            xsltReleaseRVT(ctxt, tmp);
        } else if (tmp->psvi == XSLT_RVT_GLOBAL) {
            xsltRegisterPersistRVT(ctxt, tmp);
        } else if (tmp->psvi == XSLT_RVT_FUNC_RESULT) {
            /*
             * This will either register the RVT again or move it to the
             * context variable.
             */
            xsltRegisterLocalRVT(ctxt, tmp);
            tmp->psvi = XSLT_RVT_FUNC_RESULT;
        } else {
            xmlGenericError(xmlGenericErrorContext,
                    "xsltReleaseLocalRVTs: Unexpected RVT flag %p\n",
                    tmp->psvi);
        }
    } while (cur != base);
}

/**
 * xsltApplySequenceConstructor:
 * @ctxt:  a XSLT process context
 * @contextNode:  the "current node" in the source tree
 * @list:  the nodes of a sequence constructor;
 *         (plus leading xsl:param elements)
 * @templ: the compiled xsl:template (optional)
 *
 * Processes a sequence constructor.
 *
 * NOTE: ctxt->currentTemplateRule was introduced to reflect the
 * semantics of "current template rule". I.e. the field ctxt->templ
 * is not intended to reflect this, thus always pushed onto the
 * template stack.
 */
static void
xsltApplySequenceConstructor(xsltTransformContextPtr ctxt,
			     xmlNodePtr contextNode, xmlNodePtr list,
			     xsltTemplatePtr templ)
{
    xmlNodePtr oldInsert, oldInst, oldCurInst, oldContextNode;
    xmlNodePtr cur, insert, copy = NULL;
    int level = 0, oldVarsNr;
    xmlDocPtr oldLocalFragmentTop;

#ifdef XSLT_REFACTORED
    xsltStylePreCompPtr info;
#endif

#ifdef WITH_DEBUGGER
    int addCallResult = 0;
    xmlNodePtr debuggedNode = NULL;
#endif

    if (ctxt == NULL)
	return;

#ifdef WITH_DEBUGGER
    if (ctxt->debugStatus != XSLT_DEBUG_NONE) {
	debuggedNode =
	    xsltDebuggerStartSequenceConstructor(ctxt, contextNode,
		list, templ, &addCallResult);
	if (debuggedNode == NULL)
	    return;
    }
#endif

    if (list == NULL)
        return;
    CHECK_STOPPED;

    /*
    * Check for infinite recursion: stop if the maximum of nested templates
    * is excceeded. Adjust xsltMaxDepth if you need more.
    */
    if (ctxt->depth >= ctxt->maxTemplateDepth) {
        xsltTransformError(ctxt, NULL, list,
	    "xsltApplySequenceConstructor: A potential infinite template "
            "recursion was detected.\n"
	    "You can adjust xsltMaxDepth (--maxdepth) in order to "
	    "raise the maximum number of nested template calls and "
	    "variables/params (currently set to %d).\n",
	    ctxt->maxTemplateDepth);
        xsltDebug(ctxt, contextNode, list, NULL);
	ctxt->state = XSLT_STATE_STOPPED;
        return;
    }
    ctxt->depth++;

    oldLocalFragmentTop = ctxt->localRVT;
    oldInsert = insert = ctxt->insert;
    oldInst = oldCurInst = ctxt->inst;
    oldContextNode = ctxt->node;
    /*
    * Save current number of variables on the stack; new vars are popped when
    * exiting.
    */
    oldVarsNr = ctxt->varsNr;
    /*
    * Process the sequence constructor.
    */
    cur = list;
    while (cur != NULL) {
        if (ctxt->opLimit != 0) {
            if (ctxt->opCount >= ctxt->opLimit) {
		xsltTransformError(ctxt, NULL, cur,
		    "xsltApplySequenceConstructor: "
                    "Operation limit exceeded\n");
	        ctxt->state = XSLT_STATE_STOPPED;
                goto error;
            }
            ctxt->opCount += 1;
        }

        ctxt->inst = cur;

#ifdef WITH_DEBUGGER
        switch (ctxt->debugStatus) {
            case XSLT_DEBUG_RUN_RESTART:
            case XSLT_DEBUG_QUIT:
                break;

        }
#endif
        /*
         * Test; we must have a valid insertion point.
         */
        if (insert == NULL) {

#ifdef WITH_XSLT_DEBUG_PROCESS
            XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
		"xsltApplySequenceConstructor: insert == NULL !\n"));
#endif
            goto error;
        }

#ifdef WITH_DEBUGGER
        if ((ctxt->debugStatus != XSLT_DEBUG_NONE) && (debuggedNode != cur))
            xslHandleDebugger(cur, contextNode, templ, ctxt);
#endif

#ifdef XSLT_REFACTORED
	if (cur->type == XML_ELEMENT_NODE) {
	    info = (xsltStylePreCompPtr) cur->psvi;
	    /*
	    * We expect a compiled representation on:
	    * 1) XSLT instructions of this XSLT version (1.0)
	    *    (with a few exceptions)
	    * 2) Literal result elements
	    * 3) Extension instructions
	    * 4) XSLT instructions of future XSLT versions
	    *    (forwards-compatible mode).
	    */
	    if (info == NULL) {
		/*
		* Handle the rare cases where we don't expect a compiled
		* representation on an XSLT element.
		*/
		if (IS_XSLT_ELEM_FAST(cur) && IS_XSLT_NAME(cur, "message")) {
		    xsltMessage(ctxt, contextNode, cur);
		    goto skip_children;
		}
		/*
		* Something really went wrong:
		*/
		xsltTransformError(ctxt, NULL, cur,
		    "Internal error in xsltApplySequenceConstructor(): "
		    "The element '%s' in the stylesheet has no compiled "
		    "representation.\n",
		    cur->name);
                goto skip_children;
            }

	    if (info->type == XSLT_FUNC_LITERAL_RESULT_ELEMENT) {
		xsltStyleItemLRElementInfoPtr lrInfo =
		    (xsltStyleItemLRElementInfoPtr) info;
		/*
		* Literal result elements
		* --------------------------------------------------------
		*/
#ifdef WITH_XSLT_DEBUG_PROCESS
		XSLT_TRACE(ctxt, XSLT_TRACE_APPLY_TEMPLATE,
		    xsltGenericDebug(xsltGenericDebugContext,
		    "xsltApplySequenceConstructor: copy literal result "
		    "element '%s'\n", cur->name));
#endif
		/*
		* Copy the raw element-node.
		* OLD: if ((copy = xsltShallowCopyElem(ctxt, cur, insert))
		*     == NULL)
		*   goto error;
		*/
		copy = xmlDocCopyNode(cur, insert->doc, 0);
		if (copy == NULL) {
		    xsltTransformError(ctxt, NULL, cur,
			"Internal error in xsltApplySequenceConstructor(): "
			"Failed to copy literal result element '%s'.\n",
			cur->name);
		    goto error;
		} else {
		    /*
		    * Add the element-node to the result tree.
		    */
		    copy->doc = ctxt->output;
		    copy = xsltAddChild(insert, copy);
		    /*
		    * Create effective namespaces declarations.
		    * OLD: xsltCopyNamespaceList(ctxt, copy, cur->nsDef);
		    */
		    if (lrInfo->effectiveNs != NULL) {
			xsltEffectiveNsPtr effNs = lrInfo->effectiveNs;
			xmlNsPtr ns, lastns = NULL;

			while (effNs != NULL) {
			    /*
			    * Avoid generating redundant namespace
			    * declarations; thus lookup if there is already
			    * such a ns-decl in the result.
			    */
			    ns = xmlSearchNs(copy->doc, copy, effNs->prefix);
			    if ((ns != NULL) &&
				(xmlStrEqual(ns->href, effNs->nsName)))
			    {
				effNs = effNs->next;
				continue;
			    }
			    ns = xmlNewNs(copy, effNs->nsName, effNs->prefix);
			    if (ns == NULL) {
				xsltTransformError(ctxt, NULL, cur,
				    "Internal error in "
				    "xsltApplySequenceConstructor(): "
				    "Failed to copy a namespace "
				    "declaration.\n");
				goto error;
			    }

			    if (lastns == NULL)
				copy->nsDef = ns;
			    else
				lastns->next =ns;
			    lastns = ns;

			    effNs = effNs->next;
			}

		    }
		    /*
		    * NOTE that we don't need to apply ns-alising: this was
		    *  already done at compile-time.
		    */
		    if (cur->ns != NULL) {
			/*
			* If there's no such ns-decl in the result tree,
			* then xsltGetSpecialNamespace() will
			* create a ns-decl on the copied node.
			*/
			copy->ns = xsltGetSpecialNamespace(ctxt, cur,
			    cur->ns->href, cur->ns->prefix, copy);
		    } else {
			/*
			* Undeclare the default namespace if needed.
			* This can be skipped, if the result element has
			*  no ns-decls, in which case the result element
			*  obviously does not declare a default namespace;
			*  AND there's either no parent, or the parent
			*  element is in no namespace; this means there's no
			*  default namespace is scope to care about.
			*
			* REVISIT: This might result in massive
			*  generation of ns-decls if nodes in a default
			*  namespaces are mixed with nodes in no namespace.
			*
			*/
			if (copy->nsDef ||
			    ((insert != NULL) &&
			     (insert->type == XML_ELEMENT_NODE) &&
			     (insert->ns != NULL)))
			{
			    xsltGetSpecialNamespace(ctxt, cur,
				NULL, NULL, copy);
			}
		    }
		}
		/*
		* SPEC XSLT 2.0 "Each attribute of the literal result
		*  element, other than an attribute in the XSLT namespace,
		*  is processed to produce an attribute for the element in
		*  the result tree."
		* NOTE: See bug #341325.
		*/
		if (cur->properties != NULL) {
		    xsltAttrListTemplateProcess(ctxt, copy, cur->properties);
		}
	    } else if (IS_XSLT_ELEM_FAST(cur)) {
		/*
		* XSLT instructions
		* --------------------------------------------------------
		*/
		if (info->type == XSLT_FUNC_UNKOWN_FORWARDS_COMPAT) {
		    /*
		    * We hit an unknown XSLT element.
		    * Try to apply one of the fallback cases.
		    */
		    ctxt->insert = insert;
		    if (!xsltApplyFallbacks(ctxt, contextNode, cur)) {
			xsltTransformError(ctxt, NULL, cur,
			    "The is no fallback behaviour defined for "
			    "the unknown XSLT element '%s'.\n",
			    cur->name);
		    }
		    ctxt->insert = oldInsert;
		} else if (info->func != NULL) {
		    /*
		    * Execute the XSLT instruction.
		    */
		    ctxt->insert = insert;

		    info->func(ctxt, contextNode, cur,
			(xsltElemPreCompPtr) info);

		    /*
		    * Cleanup temporary tree fragments.
		    */
		    if (oldLocalFragmentTop != ctxt->localRVT)
			xsltReleaseLocalRVTs(ctxt, oldLocalFragmentTop);

		    ctxt->insert = oldInsert;
		} else if (info->type == XSLT_FUNC_VARIABLE) {
		    xsltStackElemPtr tmpvar = ctxt->vars;

		    xsltParseStylesheetVariable(ctxt, cur);

		    if (tmpvar != ctxt->vars) {
			/*
			* TODO: Using a @tmpvar is an annoying workaround, but
			*  the current mechanisms do not provide any other way
			*  of knowing if the var was really pushed onto the
			*  stack.
			*/
			ctxt->vars->level = level;
		    }
		} else if (info->type == XSLT_FUNC_MESSAGE) {
		    /*
		    * TODO: Won't be hit, since we don't compile xsl:message.
		    */
		    xsltMessage(ctxt, contextNode, cur);
		} else {
		    xsltTransformError(ctxt, NULL, cur,
			"Unexpected XSLT element '%s'.\n", cur->name);
		}
		goto skip_children;

	    } else {
		xsltTransformFunction func;
		/*
		* Extension intructions (elements)
		* --------------------------------------------------------
		*/
		if (cur->psvi == xsltExtMarker) {
		    /*
		    * The xsltExtMarker was set during the compilation
		    * of extension instructions if there was no registered
		    * handler for this specific extension function at
		    * compile-time.
		    * Libxslt will now lookup if a handler is
		    * registered in the context of this transformation.
		    */
		    func = xsltExtElementLookup(ctxt, cur->name,
                                                cur->ns->href);
		} else
		    func = ((xsltElemPreCompPtr) cur->psvi)->func;

		if (func == NULL) {
		    /*
		    * No handler available.
		    * Try to execute fallback behaviour via xsl:fallback.
		    */
#ifdef WITH_XSLT_DEBUG_PROCESS
		    XSLT_TRACE(ctxt, XSLT_TRACE_APPLY_TEMPLATE,
			xsltGenericDebug(xsltGenericDebugContext,
			    "xsltApplySequenceConstructor: unknown extension %s\n",
			    cur->name));
#endif
		    ctxt->insert = insert;
		    if (!xsltApplyFallbacks(ctxt, contextNode, cur)) {
			xsltTransformError(ctxt, NULL, cur,
			    "Unknown extension instruction '{%s}%s'.\n",
			    cur->ns->href, cur->name);
		    }
		    ctxt->insert = oldInsert;
		} else {
		    /*
		    * Execute the handler-callback.
		    */
#ifdef WITH_XSLT_DEBUG_PROCESS
		    XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
			"xsltApplySequenceConstructor: extension construct %s\n",
			cur->name));
#endif
                    /*
                     * Disable the xsltCopyTextString optimization for
                     * extension elements. Extensions could append text using
                     * xmlAddChild which will free the buffer pointed to by
                     * 'lasttext'. This buffer could later be reallocated with
                     * a different size than recorded in 'lasttsize'. See bug
                     * #777432.
                     */
                    if (cur->psvi == xsltExtMarker) {
                        ctxt->lasttext = NULL;
                    }

		    ctxt->insert = insert;

		    func(ctxt, contextNode, cur, cur->psvi);

		    /*
		    * Cleanup temporary tree fragments.
		    */
		    if (oldLocalFragmentTop != ctxt->localRVT)
			xsltReleaseLocalRVTs(ctxt, oldLocalFragmentTop);

		    ctxt->insert = oldInsert;
		}
		goto skip_children;
	    }

	} else if (XSLT_IS_TEXT_NODE(cur)) {
	    /*
	    * Text
	    * ------------------------------------------------------------
	    */
#ifdef WITH_XSLT_DEBUG_PROCESS
            if (cur->name == xmlStringTextNoenc) {
                XSLT_TRACE(ctxt, XSLT_TRACE_APPLY_TEMPLATE,
		    xsltGenericDebug(xsltGenericDebugContext,
		    "xsltApplySequenceConstructor: copy unescaped text '%s'\n",
		    cur->content));
            } else {
                XSLT_TRACE(ctxt, XSLT_TRACE_APPLY_TEMPLATE,
		    xsltGenericDebug(xsltGenericDebugContext,
		    "xsltApplySequenceConstructor: copy text '%s'\n",
		    cur->content));
            }
#endif
            if (xsltCopyText(ctxt, insert, cur, ctxt->internalized) == NULL)
		goto error;
	}

#else /* XSLT_REFACTORED */

        if (IS_XSLT_ELEM(cur)) {
            /*
             * This is an XSLT node
             */
            xsltStylePreCompPtr info = (xsltStylePreCompPtr) cur->psvi;

            if (info == NULL) {
                if (IS_XSLT_NAME(cur, "message")) {
                    xsltMessage(ctxt, contextNode, cur);
                } else {
                    /*
                     * That's an error try to apply one of the fallback cases
                     */
                    ctxt->insert = insert;
                    if (!xsltApplyFallbacks(ctxt, contextNode, cur)) {
                        xsltGenericError(xsltGenericErrorContext,
			    "xsltApplySequenceConstructor: %s was not compiled\n",
			    cur->name);
                    }
                    ctxt->insert = oldInsert;
                }
                goto skip_children;
            }

            if (info->func != NULL) {
		oldCurInst = ctxt->inst;
		ctxt->inst = cur;
                ctxt->insert = insert;

                info->func(ctxt, contextNode, cur, (xsltElemPreCompPtr) info);

		/*
		* Cleanup temporary tree fragments.
		*/
		if (oldLocalFragmentTop != ctxt->localRVT)
		    xsltReleaseLocalRVTs(ctxt, oldLocalFragmentTop);

                ctxt->insert = oldInsert;
		ctxt->inst = oldCurInst;
                goto skip_children;
            }

            if (IS_XSLT_NAME(cur, "variable")) {
		xsltStackElemPtr tmpvar = ctxt->vars;

		oldCurInst = ctxt->inst;
		ctxt->inst = cur;

		xsltParseStylesheetVariable(ctxt, cur);

		ctxt->inst = oldCurInst;

		if (tmpvar != ctxt->vars) {
		    /*
		    * TODO: Using a @tmpvar is an annoying workaround, but
		    *  the current mechanisms do not provide any other way
		    *  of knowing if the var was really pushed onto the
		    *  stack.
		    */
		    ctxt->vars->level = level;
		}
            } else if (IS_XSLT_NAME(cur, "message")) {
                xsltMessage(ctxt, contextNode, cur);
            } else {
		xsltTransformError(ctxt, NULL, cur,
		    "Unexpected XSLT element '%s'.\n", cur->name);
            }
            goto skip_children;
        } else if ((cur->type == XML_TEXT_NODE) ||
                   (cur->type == XML_CDATA_SECTION_NODE)) {

            /*
             * This text comes from the stylesheet
             * For stylesheets, the set of whitespace-preserving
             * element names consists of just xsl:text.
             */
#ifdef WITH_XSLT_DEBUG_PROCESS
            if (cur->type == XML_CDATA_SECTION_NODE) {
                XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
                                 "xsltApplySequenceConstructor: copy CDATA text %s\n",
                                 cur->content));
            } else if (cur->name == xmlStringTextNoenc) {
                XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
                                 "xsltApplySequenceConstructor: copy unescaped text %s\n",
                                 cur->content));
            } else {
                XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
                                 "xsltApplySequenceConstructor: copy text %s\n",
                                 cur->content));
            }
#endif
            if (xsltCopyText(ctxt, insert, cur, ctxt->internalized) == NULL)
		goto error;
        } else if ((cur->type == XML_ELEMENT_NODE) &&
                   (cur->ns != NULL) && (cur->psvi != NULL)) {
            xsltTransformFunction function;

	    oldCurInst = ctxt->inst;
	    ctxt->inst = cur;
            /*
             * Flagged as an extension element
             */
            if (cur->psvi == xsltExtMarker)
                function = xsltExtElementLookup(ctxt, cur->name,
                                                cur->ns->href);
            else
                function = ((xsltElemPreCompPtr) cur->psvi)->func;

            if (function == NULL) {
                xmlNodePtr child;
                int found = 0;

#ifdef WITH_XSLT_DEBUG_PROCESS
                XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
		    "xsltApplySequenceConstructor: unknown extension %s\n",
                    cur->name));
#endif
                /*
                 * Search if there are fallbacks
                 */
                ctxt->insert = insert;
                child = cur->children;
                while (child != NULL) {
                    if ((IS_XSLT_ELEM(child)) &&
                        (IS_XSLT_NAME(child, "fallback")))
		    {
                        found = 1;
                        xsltApplySequenceConstructor(ctxt, contextNode,
			    child->children, NULL);
                    }
                    child = child->next;
                }
                ctxt->insert = oldInsert;

                if (!found) {
                    xsltTransformError(ctxt, NULL, cur,
			"xsltApplySequenceConstructor: failed to find extension %s\n",
			cur->name);
                }
            } else {
#ifdef WITH_XSLT_DEBUG_PROCESS
                XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
		    "xsltApplySequenceConstructor: extension construct %s\n",
                    cur->name));
#endif

                /*
                 * Disable the xsltCopyTextString optimization for
                 * extension elements. Extensions could append text using
                 * xmlAddChild which will free the buffer pointed to by
                 * 'lasttext'. This buffer could later be reallocated with
                 * a different size than recorded in 'lasttsize'. See bug
                 * #777432.
                 */
                if (cur->psvi == xsltExtMarker) {
	            ctxt->lasttext = NULL;
                }

                ctxt->insert = insert;

                function(ctxt, contextNode, cur, cur->psvi);
		/*
		* Cleanup temporary tree fragments.
		*/
		if (oldLocalFragmentTop != ctxt->localRVT)
		    xsltReleaseLocalRVTs(ctxt, oldLocalFragmentTop);

                ctxt->insert = oldInsert;

            }
	    ctxt->inst = oldCurInst;
            goto skip_children;
        } else if (cur->type == XML_ELEMENT_NODE) {
#ifdef WITH_XSLT_DEBUG_PROCESS
            XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
		"xsltApplySequenceConstructor: copy node %s\n",
                cur->name));
#endif
	    oldCurInst = ctxt->inst;
	    ctxt->inst = cur;

            if ((copy = xsltShallowCopyElem(ctxt, cur, insert, 1)) == NULL)
		goto error;
            /*
             * Add extra namespaces inherited from the current template
             * if we are in the first level children and this is a
	     * "real" template.
             */
            if ((templ != NULL) && (oldInsert == insert) &&
                (ctxt->templ != NULL) && (ctxt->templ->inheritedNs != NULL)) {
                int i;
                xmlNsPtr ns, ret;

                for (i = 0; i < ctxt->templ->inheritedNsNr; i++) {
		    const xmlChar *URI = NULL;
		    xsltStylesheetPtr style;
                    ns = ctxt->templ->inheritedNs[i];

		    /* Note that the XSLT namespace was already excluded
		    * in xsltGetInheritedNsList().
		    */
#if 0
		    if (xmlStrEqual(ns->href, XSLT_NAMESPACE))
			continue;
#endif
		    style = ctxt->style;
		    while (style != NULL) {
			if (style->nsAliases != NULL)
			    URI = (const xmlChar *)
				xmlHashLookup(style->nsAliases, ns->href);
			if (URI != NULL)
			    break;

			style = xsltNextImport(style);
		    }
		    if (URI == UNDEFINED_DEFAULT_NS)
			continue;
		    if (URI == NULL)
			URI = ns->href;
		    /*
		    * TODO: The following will still be buggy for the
		    * non-refactored code.
		    */
		    ret = xmlSearchNs(copy->doc, copy, ns->prefix);
		    if ((ret == NULL) || (!xmlStrEqual(ret->href, URI)))
		    {
			xmlNewNs(copy, URI, ns->prefix);
		    }
                }
		if (copy->ns != NULL) {
		    /*
		     * Fix the node namespace if needed
		     */
		    copy->ns = xsltGetNamespace(ctxt, cur, copy->ns, copy);
		}
            }
	    /*
             * all the attributes are directly inherited
             */
            if (cur->properties != NULL) {
                xsltAttrListTemplateProcess(ctxt, copy, cur->properties);
            }
	    ctxt->inst = oldCurInst;
        }
#endif /* else of XSLT_REFACTORED */

        /*
         * Descend into content in document order.
         */
        if (cur->children != NULL) {
            if (cur->children->type != XML_ENTITY_DECL) {
                cur = cur->children;
		level++;
                if (copy != NULL)
                    insert = copy;
                continue;
            }
        }

skip_children:
	/*
	* If xslt:message was just processed, we might have hit a
	* terminate='yes'; if so, then break the loop and clean up.
	* TODO: Do we need to check this also before trying to descend
	*  into the content?
	*/
	if (ctxt->state == XSLT_STATE_STOPPED)
	    break;
        if (cur->next != NULL) {
            cur = cur->next;
            continue;
        }

        do {
            cur = cur->parent;
	    level--;
	    /*
	    * Pop variables/params (xsl:variable and xsl:param).
	    */
	    if ((ctxt->varsNr > oldVarsNr) && (ctxt->vars->level > level)) {
		xsltLocalVariablePop(ctxt, oldVarsNr, level);
	    }

            insert = insert->parent;
            if (cur == NULL)
                break;
            if (cur == list->parent) {
                cur = NULL;
                break;
            }
            if (cur->next != NULL) {
                cur = cur->next;
                break;
            }
        } while (cur != NULL);
    }

error:
    /*
    * In case of errors: pop remaining variables.
    */
    if (ctxt->varsNr > oldVarsNr)
	xsltLocalVariablePop(ctxt, oldVarsNr, -1);

    ctxt->node = oldContextNode;
    ctxt->inst = oldInst;
    ctxt->insert = oldInsert;

    ctxt->depth--;

#ifdef WITH_DEBUGGER
    if ((ctxt->debugStatus != XSLT_DEBUG_NONE) && (addCallResult)) {
        xslDropCall();
    }
#endif
}

/*
* xsltApplyXSLTTemplate:
* @ctxt:  a XSLT transformation context
* @contextNode:  the node in the source tree.
* @list:  the nodes of a sequence constructor;
*         (plus leading xsl:param elements)
* @templ: the compiled xsl:template declaration;
*         NULL if a sequence constructor
* @withParams:  a set of caller-parameters (xsl:with-param) or NULL
*
* Called by:
* - xsltApplyImports()
* - xsltCallTemplate()
* - xsltDefaultProcessOneNode()
* - xsltProcessOneNode()
*/
static void
xsltApplyXSLTTemplate(xsltTransformContextPtr ctxt,
		      xmlNodePtr contextNode,
		      xmlNodePtr list,
		      xsltTemplatePtr templ,
		      xsltStackElemPtr withParams)
{
    int oldVarsBase = 0;
    xmlNodePtr cur;
    xsltStackElemPtr tmpParam = NULL;
    xmlDocPtr oldUserFragmentTop;
#ifdef WITH_PROFILER
    long start = 0;
#endif

#ifdef XSLT_REFACTORED
    xsltStyleItemParamPtr iparam;
#else
    xsltStylePreCompPtr iparam;
#endif

#ifdef WITH_DEBUGGER
    int addCallResult = 0;
#endif

    if (ctxt == NULL)
	return;
    if (templ == NULL) {
	xsltTransformError(ctxt, NULL, list,
	    "xsltApplyXSLTTemplate: Bad arguments; @templ is mandatory.\n");
	return;
    }

#ifdef WITH_DEBUGGER
    if (ctxt->debugStatus != XSLT_DEBUG_NONE) {
	if (xsltDebuggerStartSequenceConstructor(ctxt, contextNode,
		list, templ, &addCallResult) == NULL)
	    return;
    }
#endif

    if (list == NULL)
        return;
    CHECK_STOPPED;

    if (ctxt->varsNr >= ctxt->maxTemplateVars)
	{
        xsltTransformError(ctxt, NULL, list,
	    "xsltApplyXSLTTemplate: A potential infinite template recursion "
	    "was detected.\n"
	    "You can adjust maxTemplateVars (--maxvars) in order to "
	    "raise the maximum number of variables/params (currently set to %d).\n",
	    ctxt->maxTemplateVars);
        xsltDebug(ctxt, contextNode, list, NULL);
	ctxt->state = XSLT_STATE_STOPPED;
        return;
	}

    oldUserFragmentTop = ctxt->tmpRVT;
    ctxt->tmpRVT = NULL;

    /*
    * Initiate a distinct scope of local params/variables.
    */
    oldVarsBase = ctxt->varsBase;
    ctxt->varsBase = ctxt->varsNr;

    ctxt->node = contextNode;

#ifdef WITH_PROFILER
    if (ctxt->profile) {
	templ->nbCalls++;
	start = xsltTimestamp();
	profPush(ctxt, 0);
	profCallgraphAdd(templ, ctxt->templ);
    }
#endif

    /*
    * Push the xsl:template declaration onto the stack.
    */
    templPush(ctxt, templ);

#ifdef WITH_XSLT_DEBUG_PROCESS
    if (templ->name != NULL)
	XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
	"applying xsl:template '%s'\n", templ->name));
#endif
    /*
    * Process xsl:param instructions and skip those elements for
    * further processing.
    */
    cur = list;
    do {
	if (cur->type == XML_TEXT_NODE) {
	    cur = cur->next;
	    continue;
	}
	if ((cur->type != XML_ELEMENT_NODE) ||
	    (cur->name[0] != 'p') ||
	    (cur->psvi == NULL) ||
	    (! xmlStrEqual(cur->name, BAD_CAST "param")) ||
	    (! IS_XSLT_ELEM(cur)))
	{
	    break;
	}

	list = cur->next;

#ifdef XSLT_REFACTORED
	iparam = (xsltStyleItemParamPtr) cur->psvi;
#else
	iparam = (xsltStylePreCompPtr) cur->psvi;
#endif

	/*
	* Substitute xsl:param for a given xsl:with-param.
	* Since the XPath expression will reference the params/vars
	* by index, we need to slot the xsl:with-params in the
	* order of encountered xsl:params to keep the sequence of
	* params/variables in the stack exactly as it was at
	* compile time,
	*/
	tmpParam = NULL;
	if (withParams) {
	    tmpParam = withParams;
	    do {
		if ((tmpParam->name == (iparam->name)) &&
		    (tmpParam->nameURI == (iparam->ns)))
		{
		    /*
		    * Push the caller-parameter.
		    */
		    xsltLocalVariablePush(ctxt, tmpParam, -1);
		    break;
		}
		tmpParam = tmpParam->next;
	    } while (tmpParam != NULL);
	}
	/*
	* Push the xsl:param.
	*/
	if (tmpParam == NULL) {
	    /*
	    * Note that we must assume that the added parameter
	    * has a @depth of 0.
	    */
	    xsltParseStylesheetParam(ctxt, cur);
	}
	cur = cur->next;
    } while (cur != NULL);
    /*
    * Process the sequence constructor.
    */
    xsltApplySequenceConstructor(ctxt, contextNode, list, templ);

    /*
    * Remove remaining xsl:param and xsl:with-param items from
    * the stack. Don't free xsl:with-param items.
    */
    if (ctxt->varsNr > ctxt->varsBase)
	xsltTemplateParamsCleanup(ctxt);
    ctxt->varsBase = oldVarsBase;

    /*
    * Release user-created fragments stored in the scope
    * of xsl:template. Note that this mechanism is deprecated:
    * user code should now use xsltRegisterLocalRVT() instead
    * of the obsolete xsltRegisterTmpRVT().
    */
    if (ctxt->tmpRVT) {
	xmlDocPtr curdoc = ctxt->tmpRVT, tmp;

	while (curdoc != NULL) {
	    tmp = curdoc;
	    curdoc = (xmlDocPtr) curdoc->next;
	    xsltReleaseRVT(ctxt, tmp);
	}
    }
    ctxt->tmpRVT = oldUserFragmentTop;

    /*
    * Pop the xsl:template declaration from the stack.
    */
    templPop(ctxt);

#ifdef WITH_PROFILER
    if (ctxt->profile) {
	long spent, child, total, end;

	end = xsltTimestamp();
	child = profPop(ctxt);
	total = end - start;
	spent = total - child;
	if (spent <= 0) {
	    /*
	    * Not possible unless the original calibration failed
	    * we can try to correct it on the fly.
	    */
	    xsltCalibrateAdjust(spent);
	    spent = 0;
	}

	templ->time += spent;
	if (ctxt->profNr > 0)
	    ctxt->profTab[ctxt->profNr - 1] += total;
    }
#endif

#ifdef WITH_DEBUGGER
    if ((ctxt->debugStatus != XSLT_DEBUG_NONE) && (addCallResult)) {
        xslDropCall();
    }
#endif
}


/**
 * xsltApplyOneTemplate:
 * @ctxt:  a XSLT process context
 * @contextNode:  the node in the source tree.
 * @list:  the nodes of a sequence constructor
 * @templ: not used
 * @params:  a set of parameters (xsl:param) or NULL
 *
 * Processes a sequence constructor on the current node in the source tree.
 *
 * @params are the already computed variable stack items; this function
 * pushes them on the variable stack, and pops them before exiting; it's
 * left to the caller to free or reuse @params afterwards. The initial
 * states of the variable stack will always be restored before this
 * function exits.
 * NOTE that this does *not* initiate a new distinct variable scope; i.e.
 * variables already on the stack are visible to the process. The caller's
 * side needs to start a new variable scope if needed (e.g. in exsl:function).
 *
 * @templ is obsolete and not used anymore (e.g. <exslt:function> does not
 * provide a @templ); a non-NULL @templ might raise an error in the future.
 *
 * BIG NOTE: This function is not intended to process the content of an
 * xsl:template; it does not expect xsl:param instructions in @list and
 * will report errors if found.
 *
 * Called by:
 *  - xsltEvalVariable() (variables.c)
 *  - exsltFuncFunctionFunction() (libexsl/functions.c)
 */
void
xsltApplyOneTemplate(xsltTransformContextPtr ctxt,
		     xmlNodePtr contextNode,
                     xmlNodePtr list,
		     xsltTemplatePtr templ ATTRIBUTE_UNUSED,
                     xsltStackElemPtr params)
{
    if ((ctxt == NULL) || (list == NULL))
	return;
    CHECK_STOPPED;

    if (params) {
	/*
	 * This code should be obsolete - was previously used
	 * by libexslt/functions.c, but due to bug 381319 the
	 * logic there was changed.
	 */
	int oldVarsNr = ctxt->varsNr;

	/*
	* Push the given xsl:param(s) onto the variable stack.
	*/
	while (params != NULL) {
	    xsltLocalVariablePush(ctxt, params, -1);
	    params = params->next;
	}
	xsltApplySequenceConstructor(ctxt, contextNode, list, templ);
	/*
	* Pop the given xsl:param(s) from the stack but don't free them.
	*/
	xsltLocalVariablePop(ctxt, oldVarsNr, -2);
    } else
	xsltApplySequenceConstructor(ctxt, contextNode, list, templ);
}

/************************************************************************
 *									*
 *		    XSLT-1.1 extensions					*
 *									*
 ************************************************************************/

/**
 * xsltDocumentElem:
 * @ctxt:  an XSLT processing context
 * @node:  The current node
 * @inst:  the instruction in the stylesheet
 * @castedComp:  precomputed information
 *
 * Process an EXSLT/XSLT-1.1 document element
 */
void
xsltDocumentElem(xsltTransformContextPtr ctxt, xmlNodePtr node,
                 xmlNodePtr inst, xsltElemPreCompPtr castedComp)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemDocumentPtr comp = (xsltStyleItemDocumentPtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    xsltStylesheetPtr style = NULL;
    int ret;
    xmlChar *filename = NULL, *prop, *elements;
    xmlChar *element, *end;
    xmlDocPtr res = NULL;
    xmlDocPtr oldOutput;
    xmlNodePtr oldInsert, root;
    const char *oldOutputFile;
    xsltOutputType oldType;
    xmlChar *URL = NULL;
    const xmlChar *method;
    const xmlChar *doctypePublic;
    const xmlChar *doctypeSystem;
    const xmlChar *version;
    const xmlChar *encoding;
    int redirect_write_append = 0;

    if ((ctxt == NULL) || (node == NULL) || (inst == NULL) || (comp == NULL))
        return;

    if (comp->filename == NULL) {

        if (xmlStrEqual(inst->name, (const xmlChar *) "output")) {
	    /*
	    * The element "output" is in the namespace XSLT_SAXON_NAMESPACE
	    *   (http://icl.com/saxon)
	    * The @file is in no namespace.
	    */
#ifdef WITH_XSLT_DEBUG_EXTRA
            xsltGenericDebug(xsltGenericDebugContext,
                             "Found saxon:output extension\n");
#endif
            URL = xsltEvalAttrValueTemplate(ctxt, inst,
                                                 (const xmlChar *) "file",
                                                 XSLT_SAXON_NAMESPACE);

	    if (URL == NULL)
		URL = xsltEvalAttrValueTemplate(ctxt, inst,
                                                 (const xmlChar *) "href",
                                                 XSLT_SAXON_NAMESPACE);
        } else if (xmlStrEqual(inst->name, (const xmlChar *) "write")) {
#ifdef WITH_XSLT_DEBUG_EXTRA
            xsltGenericDebug(xsltGenericDebugContext,
                             "Found xalan:write extension\n");
#endif
            URL = xsltEvalAttrValueTemplate(ctxt, inst,
                                                 (const xmlChar *)
                                                 "select",
                                                 XSLT_XALAN_NAMESPACE);
	    if (URL != NULL) {
		xmlXPathCompExprPtr cmp;
		xmlChar *val;

		/*
		 * Trying to handle bug #59212
		 * The value of the "select" attribute is an
		 * XPath expression.
		 * (see http://xml.apache.org/xalan-j/extensionslib.html#redirect)
		 */
		cmp = xmlXPathCtxtCompile(ctxt->xpathCtxt, URL);
                val = xsltEvalXPathString(ctxt, cmp);
		xmlXPathFreeCompExpr(cmp);
		xmlFree(URL);
		URL = val;
	    }
	    if (URL == NULL)
		URL = xsltEvalAttrValueTemplate(ctxt, inst,
						     (const xmlChar *)
						     "file",
						     XSLT_XALAN_NAMESPACE);
	    if (URL == NULL)
		URL = xsltEvalAttrValueTemplate(ctxt, inst,
						     (const xmlChar *)
						     "href",
						     XSLT_XALAN_NAMESPACE);
        } else if (xmlStrEqual(inst->name, (const xmlChar *) "document")) {
            URL = xsltEvalAttrValueTemplate(ctxt, inst,
                                                 (const xmlChar *) "href",
                                                 NULL);
        }

    } else {
        URL = xmlStrdup(comp->filename);
    }

    if (URL == NULL) {
	xsltTransformError(ctxt, NULL, inst,
		         "xsltDocumentElem: href/URI-Reference not found\n");
	return;
    }

    /*
     * If the computation failed, it's likely that the URL wasn't escaped
     */
    filename = xmlBuildURI(URL, (const xmlChar *) ctxt->outputFile);
    if (filename == NULL) {
	xmlChar *escURL;

	escURL=xmlURIEscapeStr(URL, BAD_CAST ":/.?,");
	if (escURL != NULL) {
	    filename = xmlBuildURI(escURL, (const xmlChar *) ctxt->outputFile);
	    xmlFree(escURL);
	}
    }

    if (filename == NULL) {
	xsltTransformError(ctxt, NULL, inst,
		         "xsltDocumentElem: URL computation failed for %s\n",
			 URL);
	xmlFree(URL);
	return;
    }

    /*
     * Security checking: can we write to this resource
     */
    if (ctxt->sec != NULL) {
	ret = xsltCheckWrite(ctxt->sec, ctxt, filename);
	if (ret <= 0) {
            if (ret == 0)
                xsltTransformError(ctxt, NULL, inst,
                     "xsltDocumentElem: write rights for %s denied\n",
                                 filename);
	    xmlFree(URL);
	    xmlFree(filename);
	    return;
	}
    }

    oldOutputFile = ctxt->outputFile;
    oldOutput = ctxt->output;
    oldInsert = ctxt->insert;
    oldType = ctxt->type;
    ctxt->outputFile = (const char *) filename;

    style = xsltNewStylesheet();
    if (style == NULL) {
	xsltTransformError(ctxt, NULL, inst,
                         "xsltDocumentElem: out of memory\n");
        goto error;
    }

    /*
     * Version described in 1.1 draft allows full parameterization
     * of the output.
     */
    prop = xsltEvalAttrValueTemplate(ctxt, inst,
				     (const xmlChar *) "version",
				     NULL);
    if (prop != NULL) {
	if (style->version != NULL)
	    xmlFree(style->version);
	style->version = prop;
    }
    prop = xsltEvalAttrValueTemplate(ctxt, inst,
				     (const xmlChar *) "encoding",
				     NULL);
    if (prop != NULL) {
	if (style->encoding != NULL)
	    xmlFree(style->encoding);
	style->encoding = prop;
    }
    prop = xsltEvalAttrValueTemplate(ctxt, inst,
				     (const xmlChar *) "method",
				     NULL);
    if (prop != NULL) {
	const xmlChar *URI;

	if (style->method != NULL)
	    xmlFree(style->method);
	style->method = NULL;
	if (style->methodURI != NULL)
	    xmlFree(style->methodURI);
	style->methodURI = NULL;

	URI = xsltGetQNameURI(inst, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	} else if (URI == NULL) {
	    if ((xmlStrEqual(prop, (const xmlChar *) "xml")) ||
		(xmlStrEqual(prop, (const xmlChar *) "html")) ||
		(xmlStrEqual(prop, (const xmlChar *) "text"))) {
		style->method = prop;
	    } else {
		xsltTransformError(ctxt, NULL, inst,
				 "invalid value for method: %s\n", prop);
		if (style != NULL) style->warnings++;
	    }
	} else {
	    style->method = prop;
	    style->methodURI = xmlStrdup(URI);
	}
    }
    prop = xsltEvalAttrValueTemplate(ctxt, inst,
				     (const xmlChar *)
				     "doctype-system", NULL);
    if (prop != NULL) {
	if (style->doctypeSystem != NULL)
	    xmlFree(style->doctypeSystem);
	style->doctypeSystem = prop;
    }
    prop = xsltEvalAttrValueTemplate(ctxt, inst,
				     (const xmlChar *)
				     "doctype-public", NULL);
    if (prop != NULL) {
	if (style->doctypePublic != NULL)
	    xmlFree(style->doctypePublic);
	style->doctypePublic = prop;
    }
    prop = xsltEvalAttrValueTemplate(ctxt, inst,
				     (const xmlChar *) "standalone",
				     NULL);
    if (prop != NULL) {
	if (xmlStrEqual(prop, (const xmlChar *) "yes")) {
	    style->standalone = 1;
	} else if (xmlStrEqual(prop, (const xmlChar *) "no")) {
	    style->standalone = 0;
	} else {
	    xsltTransformError(ctxt, NULL, inst,
			     "invalid value for standalone: %s\n",
			     prop);
	    if (style != NULL) style->warnings++;
	}
	xmlFree(prop);
    }

    prop = xsltEvalAttrValueTemplate(ctxt, inst,
				     (const xmlChar *) "indent",
				     NULL);
    if (prop != NULL) {
	if (xmlStrEqual(prop, (const xmlChar *) "yes")) {
	    style->indent = 1;
	} else if (xmlStrEqual(prop, (const xmlChar *) "no")) {
	    style->indent = 0;
	} else {
	    xsltTransformError(ctxt, NULL, inst,
			     "invalid value for indent: %s\n", prop);
	    if (style != NULL) style->warnings++;
	}
	xmlFree(prop);
    }

    prop = xsltEvalAttrValueTemplate(ctxt, inst,
				     (const xmlChar *)
				     "omit-xml-declaration",
				     NULL);
    if (prop != NULL) {
	if (xmlStrEqual(prop, (const xmlChar *) "yes")) {
	    style->omitXmlDeclaration = 1;
	} else if (xmlStrEqual(prop, (const xmlChar *) "no")) {
	    style->omitXmlDeclaration = 0;
	} else {
	    xsltTransformError(ctxt, NULL, inst,
			     "invalid value for omit-xml-declaration: %s\n",
			     prop);
	    if (style != NULL) style->warnings++;
	}
	xmlFree(prop);
    }

    elements = xsltEvalAttrValueTemplate(ctxt, inst,
					 (const xmlChar *)
					 "cdata-section-elements",
					 NULL);
    if (elements != NULL) {
	if (style->stripSpaces == NULL)
	    style->stripSpaces = xmlHashCreate(10);
	if (style->stripSpaces == NULL)
	    return;

	element = elements;
	while (*element != 0) {
	    while (IS_BLANK_CH(*element))
		element++;
	    if (*element == 0)
		break;
	    end = element;
	    while ((*end != 0) && (!IS_BLANK_CH(*end)))
		end++;
	    element = xmlStrndup(element, end - element);
	    if (element) {
		const xmlChar *URI;

#ifdef WITH_XSLT_DEBUG_PARSING
		xsltGenericDebug(xsltGenericDebugContext,
				 "add cdata section output element %s\n",
				 element);
#endif
                URI = xsltGetQNameURI(inst, &element);

		xmlHashAddEntry2(style->stripSpaces, element, URI,
			        (xmlChar *) "cdata");
		xmlFree(element);
	    }
	    element = end;
	}
	xmlFree(elements);
    }

    /*
     * Create a new document tree and process the element template
     */
    XSLT_GET_IMPORT_PTR(method, style, method)
    XSLT_GET_IMPORT_PTR(doctypePublic, style, doctypePublic)
    XSLT_GET_IMPORT_PTR(doctypeSystem, style, doctypeSystem)
    XSLT_GET_IMPORT_PTR(version, style, version)
    XSLT_GET_IMPORT_PTR(encoding, style, encoding)

    if ((method != NULL) &&
	(!xmlStrEqual(method, (const xmlChar *) "xml"))) {
	if (xmlStrEqual(method, (const xmlChar *) "html")) {
	    ctxt->type = XSLT_OUTPUT_HTML;
	    if (((doctypePublic != NULL) || (doctypeSystem != NULL)))
		res = htmlNewDoc(doctypeSystem, doctypePublic);
	    else {
		if (version != NULL) {
#ifdef XSLT_GENERATE_HTML_DOCTYPE
		    xsltGetHTMLIDs(version, &doctypePublic, &doctypeSystem);
#endif
                }
		res = htmlNewDocNoDtD(doctypeSystem, doctypePublic);
	    }
	    if (res == NULL)
		goto error;
	    res->dict = ctxt->dict;
	    xmlDictReference(res->dict);
	} else if (xmlStrEqual(method, (const xmlChar *) "xhtml")) {
	    xsltTransformError(ctxt, NULL, inst,
	     "xsltDocumentElem: unsupported method xhtml\n");
	    ctxt->type = XSLT_OUTPUT_HTML;
	    res = htmlNewDocNoDtD(doctypeSystem, doctypePublic);
	    if (res == NULL)
		goto error;
	    res->dict = ctxt->dict;
	    xmlDictReference(res->dict);
	} else if (xmlStrEqual(method, (const xmlChar *) "text")) {
	    ctxt->type = XSLT_OUTPUT_TEXT;
	    res = xmlNewDoc(style->version);
	    if (res == NULL)
		goto error;
	    res->dict = ctxt->dict;
	    xmlDictReference(res->dict);
#ifdef WITH_XSLT_DEBUG
	    xsltGenericDebug(xsltGenericDebugContext,
                     "reusing transformation dict for output\n");
#endif
	} else {
	    xsltTransformError(ctxt, NULL, inst,
			     "xsltDocumentElem: unsupported method (%s)\n",
		             method);
	    goto error;
	}
    } else {
	ctxt->type = XSLT_OUTPUT_XML;
	res = xmlNewDoc(style->version);
	if (res == NULL)
	    goto error;
	res->dict = ctxt->dict;
	xmlDictReference(res->dict);
#ifdef WITH_XSLT_DEBUG
	xsltGenericDebug(xsltGenericDebugContext,
                     "reusing transformation dict for output\n");
#endif
    }
    res->charset = XML_CHAR_ENCODING_UTF8;
    if (encoding != NULL)
	res->encoding = xmlStrdup(encoding);
    ctxt->output = res;
    ctxt->insert = (xmlNodePtr) res;
    xsltApplySequenceConstructor(ctxt, node, inst->children, NULL);

    /*
     * Do some post processing work depending on the generated output
     */
    root = xmlDocGetRootElement(res);
    if (root != NULL) {
        const xmlChar *doctype = NULL;

        if ((root->ns != NULL) && (root->ns->prefix != NULL))
	    doctype = xmlDictQLookup(ctxt->dict, root->ns->prefix, root->name);
	if (doctype == NULL)
	    doctype = root->name;

        /*
         * Apply the default selection of the method
         */
        if ((method == NULL) &&
            (root->ns == NULL) &&
            (!xmlStrcasecmp(root->name, (const xmlChar *) "html"))) {
            xmlNodePtr tmp;

            tmp = res->children;
            while ((tmp != NULL) && (tmp != root)) {
                if (tmp->type == XML_ELEMENT_NODE)
                    break;
                if ((tmp->type == XML_TEXT_NODE) && (!xmlIsBlankNode(tmp)))
                    break;
		tmp = tmp->next;
            }
            if (tmp == root) {
                ctxt->type = XSLT_OUTPUT_HTML;
                res->type = XML_HTML_DOCUMENT_NODE;
                if (((doctypePublic != NULL) || (doctypeSystem != NULL))) {
                    res->intSubset = xmlCreateIntSubset(res, doctype,
                                                        doctypePublic,
                                                        doctypeSystem);
#ifdef XSLT_GENERATE_HTML_DOCTYPE
		} else if (version != NULL) {
                    xsltGetHTMLIDs(version, &doctypePublic,
                                   &doctypeSystem);
                    if (((doctypePublic != NULL) || (doctypeSystem != NULL)))
                        res->intSubset =
                            xmlCreateIntSubset(res, doctype,
                                               doctypePublic,
                                               doctypeSystem);
#endif
                }
            }

        }
        if (ctxt->type == XSLT_OUTPUT_XML) {
            XSLT_GET_IMPORT_PTR(doctypePublic, style, doctypePublic)
                XSLT_GET_IMPORT_PTR(doctypeSystem, style, doctypeSystem)
                if (((doctypePublic != NULL) || (doctypeSystem != NULL)))
                res->intSubset = xmlCreateIntSubset(res, doctype,
                                                    doctypePublic,
                                                    doctypeSystem);
        }
    }

    /*
     * Calls to redirect:write also take an optional attribute append.
     * Attribute append="true|yes" which will attempt to simply append
     * to an existing file instead of always opening a new file. The
     * default behavior of always overwriting the file still happens
     * if we do not specify append.
     * Note that append use will forbid use of remote URI target.
     */
    prop = xsltEvalAttrValueTemplate(ctxt, inst, (const xmlChar *)"append",
				     NULL);
    if (prop != NULL) {
	if (xmlStrEqual(prop, (const xmlChar *) "true") ||
	    xmlStrEqual(prop, (const xmlChar *) "yes")) {
	    style->omitXmlDeclaration = 1;
	    redirect_write_append = 1;
	} else
	    style->omitXmlDeclaration = 0;
	xmlFree(prop);
    }

    if (redirect_write_append) {
        FILE *f;

	f = fopen((const char *) filename, "ab");
	if (f == NULL) {
	    ret = -1;
	} else {
	    ret = xsltSaveResultToFile(f, res, style);
	    fclose(f);
	}
    } else {
	ret = xsltSaveResultToFilename((const char *) filename, res, style, 0);
    }
    if (ret < 0) {
	xsltTransformError(ctxt, NULL, inst,
                         "xsltDocumentElem: unable to save to %s\n",
                         filename);
#ifdef WITH_XSLT_DEBUG_EXTRA
    } else {
        xsltGenericDebug(xsltGenericDebugContext,
                         "Wrote %d bytes to %s\n", ret, filename);
#endif
    }

  error:
    ctxt->output = oldOutput;
    ctxt->insert = oldInsert;
    ctxt->type = oldType;
    ctxt->outputFile = oldOutputFile;
    if (URL != NULL)
        xmlFree(URL);
    if (filename != NULL)
        xmlFree(filename);
    if (style != NULL)
        xsltFreeStylesheet(style);
    if (res != NULL)
        xmlFreeDoc(res);
}

/************************************************************************
 *									*
 *		Most of the XSLT-1.0 transformations			*
 *									*
 ************************************************************************/

/**
 * xsltSort:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @inst:  the xslt sort node
 * @comp:  precomputed information
 *
 * function attached to xsl:sort nodes, but this should not be
 * called directly
 */
void
xsltSort(xsltTransformContextPtr ctxt,
	xmlNodePtr node ATTRIBUTE_UNUSED, xmlNodePtr inst,
	xsltElemPreCompPtr comp) {
    if (comp == NULL) {
	xsltTransformError(ctxt, NULL, inst,
	     "xsl:sort : compilation failed\n");
	return;
    }
    xsltTransformError(ctxt, NULL, inst,
	 "xsl:sort : improper use this should not be reached\n");
}

/**
 * xsltCopy:
 * @ctxt:  an XSLT process context
 * @node:  the node in the source tree
 * @inst:  the element node of the XSLT-copy instruction
 * @castedComp:  computed information of the XSLT-copy instruction
 *
 * Execute the XSLT-copy instruction on the source node.
 */
void
xsltCopy(xsltTransformContextPtr ctxt, xmlNodePtr node,
	 xmlNodePtr inst, xsltElemPreCompPtr castedComp)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemCopyPtr comp = (xsltStyleItemCopyPtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    xmlNodePtr copy, oldInsert;

    oldInsert = ctxt->insert;
    if (ctxt->insert != NULL) {
	switch (node->type) {
	    case XML_TEXT_NODE:
	    case XML_CDATA_SECTION_NODE:
		/*
		 * This text comes from the stylesheet
		 * For stylesheets, the set of whitespace-preserving
		 * element names consists of just xsl:text.
		 */
#ifdef WITH_XSLT_DEBUG_PROCESS
		if (node->type == XML_CDATA_SECTION_NODE) {
		    XSLT_TRACE(ctxt,XSLT_TRACE_COPY,xsltGenericDebug(xsltGenericDebugContext,
			 "xsltCopy: CDATA text %s\n", node->content));
		} else {
		    XSLT_TRACE(ctxt,XSLT_TRACE_COPY,xsltGenericDebug(xsltGenericDebugContext,
			 "xsltCopy: text %s\n", node->content));
                }
#endif
		xsltCopyText(ctxt, ctxt->insert, node, 0);
		break;
	    case XML_DOCUMENT_NODE:
	    case XML_HTML_DOCUMENT_NODE:
		break;
	    case XML_ELEMENT_NODE:
		/*
		* REVISIT NOTE: The "fake" is a doc-node, not an element node.
		* REMOVED:
		*   if (xmlStrEqual(node->name, BAD_CAST " fake node libxslt"))
		*    return;
		*/

#ifdef WITH_XSLT_DEBUG_PROCESS
		XSLT_TRACE(ctxt,XSLT_TRACE_COPY,xsltGenericDebug(xsltGenericDebugContext,
				 "xsltCopy: node %s\n", node->name));
#endif
		copy = xsltShallowCopyElem(ctxt, node, ctxt->insert, 0);
		ctxt->insert = copy;
		if (comp->use != NULL) {
		    xsltApplyAttributeSet(ctxt, node, inst, comp->use);
		}
		break;
	    case XML_ATTRIBUTE_NODE: {
#ifdef WITH_XSLT_DEBUG_PROCESS
		XSLT_TRACE(ctxt,XSLT_TRACE_COPY,xsltGenericDebug(xsltGenericDebugContext,
				 "xsltCopy: attribute %s\n", node->name));
#endif
		/*
		* REVISIT: We could also raise an error if the parent is not
		* an element node.
		* OPTIMIZE TODO: Can we set the value/children of the
		* attribute without an intermediate copy of the string value?
		*/
		xsltShallowCopyAttr(ctxt, inst, ctxt->insert, (xmlAttrPtr) node);
		break;
	    }
	    case XML_PI_NODE:
#ifdef WITH_XSLT_DEBUG_PROCESS
		XSLT_TRACE(ctxt,XSLT_TRACE_COPY,xsltGenericDebug(xsltGenericDebugContext,
				 "xsltCopy: PI %s\n", node->name));
#endif
		copy = xmlNewDocPI(ctxt->insert->doc, node->name,
		                   node->content);
		copy = xsltAddChild(ctxt->insert, copy);
		break;
	    case XML_COMMENT_NODE:
#ifdef WITH_XSLT_DEBUG_PROCESS
		XSLT_TRACE(ctxt,XSLT_TRACE_COPY,xsltGenericDebug(xsltGenericDebugContext,
				 "xsltCopy: comment\n"));
#endif
		copy = xmlNewComment(node->content);
		copy = xsltAddChild(ctxt->insert, copy);
		break;
	    case XML_NAMESPACE_DECL:
#ifdef WITH_XSLT_DEBUG_PROCESS
		XSLT_TRACE(ctxt,XSLT_TRACE_COPY,xsltGenericDebug(xsltGenericDebugContext,
				 "xsltCopy: namespace declaration\n"));
#endif
		xsltShallowCopyNsNode(ctxt, inst, ctxt->insert, (xmlNsPtr)node);
		break;
	    default:
		break;

	}
    }

    switch (node->type) {
	case XML_DOCUMENT_NODE:
	case XML_HTML_DOCUMENT_NODE:
	case XML_ELEMENT_NODE:
	    xsltApplySequenceConstructor(ctxt, ctxt->node, inst->children,
		NULL);
	    break;
	default:
	    break;
    }
    ctxt->insert = oldInsert;
}

/**
 * xsltText:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @inst:  the xslt text node
 * @comp:  precomputed information
 *
 * Process the xslt text node on the source node
 */
void
xsltText(xsltTransformContextPtr ctxt, xmlNodePtr node ATTRIBUTE_UNUSED,
	    xmlNodePtr inst, xsltElemPreCompPtr comp ATTRIBUTE_UNUSED) {
    if ((inst->children != NULL) && (comp != NULL)) {
	xmlNodePtr text = inst->children;
	xmlNodePtr copy;

	while (text != NULL) {
	    if ((text->type != XML_TEXT_NODE) &&
	         (text->type != XML_CDATA_SECTION_NODE)) {
		xsltTransformError(ctxt, NULL, inst,
				 "xsl:text content problem\n");
		break;
	    }
	    copy = xmlNewDocText(ctxt->output, text->content);
	    if (text->type != XML_CDATA_SECTION_NODE) {
#ifdef WITH_XSLT_DEBUG_PARSING
		xsltGenericDebug(xsltGenericDebugContext,
		     "Disable escaping: %s\n", text->content);
#endif
		copy->name = xmlStringTextNoenc;
	    }
	    copy = xsltAddChild(ctxt->insert, copy);
	    text = text->next;
	}
    }
}

/**
 * xsltElement:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @inst:  the xslt element node
 * @castedComp:  precomputed information
 *
 * Process the xslt element node on the source node
 */
void
xsltElement(xsltTransformContextPtr ctxt, xmlNodePtr node,
	    xmlNodePtr inst, xsltElemPreCompPtr castedComp) {
#ifdef XSLT_REFACTORED
    xsltStyleItemElementPtr comp = (xsltStyleItemElementPtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    xmlChar *prop = NULL;
    const xmlChar *name, *prefix = NULL, *nsName = NULL;
    xmlNodePtr copy;
    xmlNodePtr oldInsert;

    if (ctxt->insert == NULL)
	return;

    /*
    * A comp->has_name == 0 indicates that we need to skip this instruction,
    * since it was evaluated to be invalid already during compilation.
    */
    if (!comp->has_name)
        return;

    /*
     * stack and saves
     */
    oldInsert = ctxt->insert;

    if (comp->name == NULL) {
	/* TODO: fix attr acquisition wrt to the XSLT namespace */
        prop = xsltEvalAttrValueTemplate(ctxt, inst,
	    (const xmlChar *) "name", XSLT_NAMESPACE);
        if (prop == NULL) {
            xsltTransformError(ctxt, NULL, inst,
		"xsl:element: The attribute 'name' is missing.\n");
            goto error;
        }
	if (xmlValidateQName(prop, 0)) {
	    xsltTransformError(ctxt, NULL, inst,
		"xsl:element: The effective name '%s' is not a "
		"valid QName.\n", prop);
	    /* we fall through to catch any further errors, if possible */
	}
	name = xsltSplitQName(ctxt->dict, prop, &prefix);
	xmlFree(prop);
    } else {
	/*
	* The "name" value was static.
	*/
#ifdef XSLT_REFACTORED
	prefix = comp->nsPrefix;
	name = comp->name;
#else
	name = xsltSplitQName(ctxt->dict, comp->name, &prefix);
#endif
    }

    /*
     * Create the new element
     */
    if (ctxt->output->dict == ctxt->dict) {
	copy = xmlNewDocNodeEatName(ctxt->output, NULL, (xmlChar *)name, NULL);
    } else {
	copy = xmlNewDocNode(ctxt->output, NULL, (xmlChar *)name, NULL);
    }
    if (copy == NULL) {
	xsltTransformError(ctxt, NULL, inst,
	    "xsl:element : creation of %s failed\n", name);
	return;
    }
    copy = xsltAddChild(ctxt->insert, copy);
    if (copy == NULL) {
        xsltTransformError(ctxt, NULL, inst,
            "xsl:element : xsltAddChild failed\n");
        return;
    }

    /*
    * Namespace
    * ---------
    */
    if (comp->has_ns) {
	if (comp->ns != NULL) {
	    /*
	    * No AVT; just plain text for the namespace name.
	    */
	    if (comp->ns[0] != 0)
		nsName = comp->ns;
	} else {
	    xmlChar *tmpNsName;
	    /*
	    * Eval the AVT.
	    */
	    /* TODO: check attr acquisition wrt to the XSLT namespace */
	    tmpNsName = xsltEvalAttrValueTemplate(ctxt, inst,
		(const xmlChar *) "namespace", XSLT_NAMESPACE);
	    /*
	    * SPEC XSLT 1.0:
	    *  "If the string is empty, then the expanded-name of the
	    *  attribute has a null namespace URI."
	    */
	    if ((tmpNsName != NULL) && (tmpNsName[0] != 0))
		nsName = xmlDictLookup(ctxt->dict, BAD_CAST tmpNsName, -1);
	    xmlFree(tmpNsName);
	}

        if (xmlStrEqual(nsName, BAD_CAST "http://www.w3.org/2000/xmlns/")) {
            xsltTransformError(ctxt, NULL, inst,
                "xsl:attribute: Namespace http://www.w3.org/2000/xmlns/ "
                "forbidden.\n");
            goto error;
        }
        if (xmlStrEqual(nsName, XML_XML_NAMESPACE)) {
            prefix = BAD_CAST "xml";
        } else if (xmlStrEqual(prefix, BAD_CAST "xml")) {
            prefix = NULL;
        }
    } else {
	xmlNsPtr ns;
	/*
	* SPEC XSLT 1.0:
	*  "If the namespace attribute is not present, then the QName is
	*  expanded into an expanded-name using the namespace declarations
	*  in effect for the xsl:element element, including any default
	*  namespace declaration.
	*/
	ns = xmlSearchNs(inst->doc, inst, prefix);
	if (ns == NULL) {
	    /*
	    * TODO: Check this in the compilation layer in case it's a
	    * static value.
	    */
            if (prefix != NULL) {
                xsltTransformError(ctxt, NULL, inst,
                    "xsl:element: The QName '%s:%s' has no "
                    "namespace binding in scope in the stylesheet; "
                    "this is an error, since the namespace was not "
                    "specified by the instruction itself.\n", prefix, name);
            }
	} else
	    nsName = ns->href;
    }
    /*
    * Find/create a matching ns-decl in the result tree.
    */
    if (nsName != NULL) {
	if (xmlStrEqual(prefix, BAD_CAST "xmlns")) {
            /* Don't use a prefix of "xmlns" */
	    xmlChar *pref = xmlStrdup(BAD_CAST "ns_1");

	    copy->ns = xsltGetSpecialNamespace(ctxt, inst, nsName, pref, copy);

	    xmlFree(pref);
	} else {
	    copy->ns = xsltGetSpecialNamespace(ctxt, inst, nsName, prefix,
		copy);
	}
    } else if ((copy->parent != NULL) &&
	(copy->parent->type == XML_ELEMENT_NODE) &&
	(copy->parent->ns != NULL))
    {
	/*
	* "Undeclare" the default namespace.
	*/
	xsltGetSpecialNamespace(ctxt, inst, NULL, NULL, copy);
    }

    ctxt->insert = copy;

    if (comp->has_use) {
	if (comp->use != NULL) {
	    xsltApplyAttributeSet(ctxt, node, inst, comp->use);
	} else {
	    xmlChar *attrSets = NULL;
	    /*
	    * BUG TODO: use-attribute-sets is not a value template.
	    *  use-attribute-sets = qnames
	    */
	    attrSets = xsltEvalAttrValueTemplate(ctxt, inst,
		(const xmlChar *)"use-attribute-sets", NULL);
	    if (attrSets != NULL) {
		xsltApplyAttributeSet(ctxt, node, inst, attrSets);
		xmlFree(attrSets);
	    }
	}
    }
    /*
    * Instantiate the sequence constructor.
    */
    if (inst->children != NULL)
	xsltApplySequenceConstructor(ctxt, ctxt->node, inst->children,
	    NULL);

error:
    ctxt->insert = oldInsert;
    return;
}


/**
 * xsltComment:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @inst:  the xslt comment node
 * @comp:  precomputed information
 *
 * Process the xslt comment node on the source node
 */
void
xsltComment(xsltTransformContextPtr ctxt, xmlNodePtr node,
	           xmlNodePtr inst, xsltElemPreCompPtr comp ATTRIBUTE_UNUSED) {
    xmlChar *value = NULL;
    xmlNodePtr commentNode;
    int len;

    value = xsltEvalTemplateString(ctxt, node, inst);
    /* TODO: use or generate the compiled form */
    len = xmlStrlen(value);
    if (len > 0) {
        if ((value[len-1] == '-') ||
	    (xmlStrstr(value, BAD_CAST "--"))) {
	    xsltTransformError(ctxt, NULL, inst,
		    "xsl:comment : '--' or ending '-' not allowed in comment\n");
	    /* fall through to try to catch further errors */
	}
    }
#ifdef WITH_XSLT_DEBUG_PROCESS
    if (value == NULL) {
	XSLT_TRACE(ctxt,XSLT_TRACE_COMMENT,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltComment: empty\n"));
    } else {
	XSLT_TRACE(ctxt,XSLT_TRACE_COMMENT,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltComment: content %s\n", value));
    }
#endif

    commentNode = xmlNewComment(value);
    commentNode = xsltAddChild(ctxt->insert, commentNode);

    if (value != NULL)
	xmlFree(value);
}

/**
 * xsltProcessingInstruction:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @inst:  the xslt processing-instruction node
 * @castedComp:  precomputed information
 *
 * Process the xslt processing-instruction node on the source node
 */
void
xsltProcessingInstruction(xsltTransformContextPtr ctxt, xmlNodePtr node,
	           xmlNodePtr inst, xsltElemPreCompPtr castedComp) {
#ifdef XSLT_REFACTORED
    xsltStyleItemPIPtr comp = (xsltStyleItemPIPtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    const xmlChar *name;
    xmlChar *value = NULL;
    xmlNodePtr pi;


    if (ctxt->insert == NULL)
	return;
    if (comp->has_name == 0)
	return;
    if (comp->name == NULL) {
	name = xsltEvalAttrValueTemplate(ctxt, inst,
			    (const xmlChar *)"name", NULL);
	if (name == NULL) {
	    xsltTransformError(ctxt, NULL, inst,
		 "xsl:processing-instruction : name is missing\n");
	    goto error;
	}
    } else {
	name = comp->name;
    }
    /* TODO: check that it's both an an NCName and a PITarget. */


    value = xsltEvalTemplateString(ctxt, node, inst);
    if (xmlStrstr(value, BAD_CAST "?>") != NULL) {
	xsltTransformError(ctxt, NULL, inst,
	     "xsl:processing-instruction: '?>' not allowed within PI content\n");
	goto error;
    }
#ifdef WITH_XSLT_DEBUG_PROCESS
    if (value == NULL) {
	XSLT_TRACE(ctxt,XSLT_TRACE_PI,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltProcessingInstruction: %s empty\n", name));
    } else {
	XSLT_TRACE(ctxt,XSLT_TRACE_PI,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltProcessingInstruction: %s content %s\n", name, value));
    }
#endif

    pi = xmlNewDocPI(ctxt->insert->doc, name, value);
    pi = xsltAddChild(ctxt->insert, pi);

error:
    if ((name != NULL) && (name != comp->name))
        xmlFree((xmlChar *) name);
    if (value != NULL)
	xmlFree(value);
}

/**
 * xsltCopyOf:
 * @ctxt:  an XSLT transformation context
 * @node:  the current node in the source tree
 * @inst:  the element node of the XSLT copy-of instruction
 * @castedComp:  precomputed information of the XSLT copy-of instruction
 *
 * Process the XSLT copy-of instruction.
 */
void
xsltCopyOf(xsltTransformContextPtr ctxt, xmlNodePtr node,
	           xmlNodePtr inst, xsltElemPreCompPtr castedComp) {
#ifdef XSLT_REFACTORED
    xsltStyleItemCopyOfPtr comp = (xsltStyleItemCopyOfPtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    xmlXPathObjectPtr res = NULL;
    xmlNodeSetPtr list = NULL;
    int i;

    if ((ctxt == NULL) || (node == NULL) || (inst == NULL))
	return;
    if ((comp == NULL) || (comp->select == NULL) || (comp->comp == NULL)) {
	xsltTransformError(ctxt, NULL, inst,
	     "xsl:copy-of : compilation failed\n");
	return;
    }

     /*
    * SPEC XSLT 1.0:
    *  "The xsl:copy-of element can be used to insert a result tree
    *  fragment into the result tree, without first converting it to
    *  a string as xsl:value-of does (see [7.6.1 Generating Text with
    *  xsl:value-of]). The required select attribute contains an
    *  expression. When the result of evaluating the expression is a
    *  result tree fragment, the complete fragment is copied into the
    *  result tree. When the result is a node-set, all the nodes in the
    *  set are copied in document order into the result tree; copying
    *  an element node copies the attribute nodes, namespace nodes and
    *  children of the element node as well as the element node itself;
    *  a root node is copied by copying its children. When the result
    *  is neither a node-set nor a result tree fragment, the result is
    *  converted to a string and then inserted into the result tree,
    *  as with xsl:value-of.
    */

#ifdef WITH_XSLT_DEBUG_PROCESS
    XSLT_TRACE(ctxt,XSLT_TRACE_COPY_OF,xsltGenericDebug(xsltGenericDebugContext,
	 "xsltCopyOf: select %s\n", comp->select));
#endif

    /*
    * Evaluate the "select" expression.
    */
    res = xsltPreCompEval(ctxt, node, comp);

    if (res != NULL) {
	if (res->type == XPATH_NODESET) {
	    /*
	    * Node-set
	    * --------
	    */
#ifdef WITH_XSLT_DEBUG_PROCESS
	    XSLT_TRACE(ctxt,XSLT_TRACE_COPY_OF,xsltGenericDebug(xsltGenericDebugContext,
		 "xsltCopyOf: result is a node set\n"));
#endif
	    list = res->nodesetval;
	    if (list != NULL) {
		xmlNodePtr cur;
		/*
		* The list is already sorted in document order by XPath.
		* Append everything in this order under ctxt->insert.
		*/
		for (i = 0;i < list->nodeNr;i++) {
		    cur = list->nodeTab[i];
		    if (cur == NULL)
			continue;
		    if ((cur->type == XML_DOCUMENT_NODE) ||
			(cur->type == XML_HTML_DOCUMENT_NODE))
		    {
			xsltCopyTreeList(ctxt, inst,
			    cur->children, ctxt->insert, 0, 0);
		    } else if (cur->type == XML_ATTRIBUTE_NODE) {
			xsltShallowCopyAttr(ctxt, inst,
			    ctxt->insert, (xmlAttrPtr) cur);
		    } else {
			xsltCopyTree(ctxt, inst, cur, ctxt->insert, 0, 0);
		    }
		}
	    }
	} else if (res->type == XPATH_XSLT_TREE) {
	    /*
	    * Result tree fragment
	    * --------------------
	    * E.g. via <xsl:variable ...><foo/></xsl:variable>
	    * Note that the root node of such trees is an xmlDocPtr in Libxslt.
	    */
#ifdef WITH_XSLT_DEBUG_PROCESS
	    XSLT_TRACE(ctxt,XSLT_TRACE_COPY_OF,xsltGenericDebug(xsltGenericDebugContext,
		 "xsltCopyOf: result is a result tree fragment\n"));
#endif
	    list = res->nodesetval;
	    if ((list != NULL) && (list->nodeTab != NULL) &&
		(list->nodeTab[0] != NULL) &&
		(IS_XSLT_REAL_NODE(list->nodeTab[0])))
	    {
		xsltCopyTreeList(ctxt, inst,
		    list->nodeTab[0]->children, ctxt->insert, 0, 0);
	    }
	} else {
	    xmlChar *value = NULL;
	    /*
	    * Convert to a string.
	    */
	    value = xmlXPathCastToString(res);
	    if (value == NULL) {
		xsltTransformError(ctxt, NULL, inst,
		    "Internal error in xsltCopyOf(): "
		    "failed to cast an XPath object to string.\n");
		ctxt->state = XSLT_STATE_STOPPED;
	    } else {
		if (value[0] != 0) {
		    /*
		    * Append content as text node.
		    */
		    xsltCopyTextString(ctxt, ctxt->insert, value, 0);
		}
		xmlFree(value);

#ifdef WITH_XSLT_DEBUG_PROCESS
		XSLT_TRACE(ctxt,XSLT_TRACE_COPY_OF,xsltGenericDebug(xsltGenericDebugContext,
		    "xsltCopyOf: result %s\n", res->stringval));
#endif
	    }
	}
    } else {
	ctxt->state = XSLT_STATE_STOPPED;
    }

    if (res != NULL)
	xmlXPathFreeObject(res);
}

/**
 * xsltValueOf:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @inst:  the xslt value-of node
 * @castedComp:  precomputed information
 *
 * Process the xslt value-of node on the source node
 */
void
xsltValueOf(xsltTransformContextPtr ctxt, xmlNodePtr node,
	           xmlNodePtr inst, xsltElemPreCompPtr castedComp)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemValueOfPtr comp = (xsltStyleItemValueOfPtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    xmlXPathObjectPtr res = NULL;
    xmlChar *value = NULL;

    if ((ctxt == NULL) || (node == NULL) || (inst == NULL))
	return;

    if ((comp == NULL) || (comp->select == NULL) || (comp->comp == NULL)) {
	xsltTransformError(ctxt, NULL, inst,
	    "Internal error in xsltValueOf(): "
	    "The XSLT 'value-of' instruction was not compiled.\n");
	return;
    }

#ifdef WITH_XSLT_DEBUG_PROCESS
    XSLT_TRACE(ctxt,XSLT_TRACE_VALUE_OF,xsltGenericDebug(xsltGenericDebugContext,
	 "xsltValueOf: select %s\n", comp->select));
#endif

    res = xsltPreCompEval(ctxt, node, comp);

    /*
    * Cast the XPath object to string.
    */
    if (res != NULL) {
	value = xmlXPathCastToString(res);
	if (value == NULL) {
	    xsltTransformError(ctxt, NULL, inst,
		"Internal error in xsltValueOf(): "
		"failed to cast an XPath object to string.\n");
	    ctxt->state = XSLT_STATE_STOPPED;
	    goto error;
	}
	if (value[0] != 0) {
	    xsltCopyTextString(ctxt, ctxt->insert, value, comp->noescape);
	}
    } else {
	xsltTransformError(ctxt, NULL, inst,
	    "XPath evaluation returned no result.\n");
	ctxt->state = XSLT_STATE_STOPPED;
	goto error;
    }

#ifdef WITH_XSLT_DEBUG_PROCESS
    if (value) {
	XSLT_TRACE(ctxt,XSLT_TRACE_VALUE_OF,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltValueOf: result '%s'\n", value));
    }
#endif

error:
    if (value != NULL)
	xmlFree(value);
    if (res != NULL)
	xmlXPathFreeObject(res);
}

/**
 * xsltNumber:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @inst:  the xslt number node
 * @castedComp:  precomputed information
 *
 * Process the xslt number node on the source node
 */
void
xsltNumber(xsltTransformContextPtr ctxt, xmlNodePtr node,
	   xmlNodePtr inst, xsltElemPreCompPtr castedComp)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemNumberPtr comp = (xsltStyleItemNumberPtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    xmlXPathContextPtr xpctxt;
    xmlNsPtr *oldXPNamespaces;
    int oldXPNsNr;

    if (comp == NULL) {
	xsltTransformError(ctxt, NULL, inst,
	     "xsl:number : compilation failed\n");
	return;
    }

    if ((ctxt == NULL) || (node == NULL) || (inst == NULL) || (comp == NULL))
	return;

    comp->numdata.doc = inst->doc;
    comp->numdata.node = inst;

    xpctxt = ctxt->xpathCtxt;
    oldXPNsNr = xpctxt->nsNr;
    oldXPNamespaces = xpctxt->namespaces;

#ifdef XSLT_REFACTORED
    if (comp->inScopeNs != NULL) {
        xpctxt->namespaces = comp->inScopeNs->list;
        xpctxt->nsNr = comp->inScopeNs->xpathNumber;
    } else {
        xpctxt->namespaces = NULL;
        xpctxt->nsNr = 0;
    }
#else
    xpctxt->namespaces = comp->nsList;
    xpctxt->nsNr = comp->nsNr;
#endif

    xsltNumberFormat(ctxt, &comp->numdata, node);

    xpctxt->nsNr = oldXPNsNr;
    xpctxt->namespaces = oldXPNamespaces;
}

/**
 * xsltApplyImports:
 * @ctxt:  an XSLT transformation context
 * @contextNode:  the current node in the source tree.
 * @inst:  the element node of the XSLT 'apply-imports' instruction
 * @comp:  the compiled instruction
 *
 * Process the XSLT apply-imports element.
 */
void
xsltApplyImports(xsltTransformContextPtr ctxt, xmlNodePtr contextNode,
	         xmlNodePtr inst,
		 xsltElemPreCompPtr comp ATTRIBUTE_UNUSED)
{
    xsltTemplatePtr templ;

    if ((ctxt == NULL) || (inst == NULL))
	return;

    if (comp == NULL) {
	xsltTransformError(ctxt, NULL, inst,
	    "Internal error in xsltApplyImports(): "
	    "The XSLT 'apply-imports' instruction was not compiled.\n");
	return;
    }
    /*
    * NOTE that ctxt->currentTemplateRule and ctxt->templ is not the
    * same; the former is the "Current Template Rule" as defined by the
    * XSLT spec, the latter is simply the template struct being
    * currently processed.
    */
    if (ctxt->currentTemplateRule == NULL) {
	/*
	* SPEC XSLT 2.0:
	* "[ERR XTDE0560] It is a non-recoverable dynamic error if
	*  xsl:apply-imports or xsl:next-match is evaluated when the
	*  current template rule is null."
	*/
	xsltTransformError(ctxt, NULL, inst,
	     "It is an error to call 'apply-imports' "
	     "when there's no current template rule.\n");
	return;
    }
    /*
    * TODO: Check if this is correct.
    */
    templ = xsltGetTemplate(ctxt, contextNode,
	ctxt->currentTemplateRule->style);

    if (templ != NULL) {
	xsltTemplatePtr oldCurTemplRule = ctxt->currentTemplateRule;
	/*
	* Set the current template rule.
	*/
	ctxt->currentTemplateRule = templ;
	/*
	* URGENT TODO: Need xsl:with-param be handled somehow here?
	*/
	xsltApplyXSLTTemplate(ctxt, contextNode, templ->content,
	    templ, NULL);

	ctxt->currentTemplateRule = oldCurTemplRule;
    }
    else {
        /* Use built-in templates. */
        xsltDefaultProcessOneNode(ctxt, contextNode, NULL);
    }
}

/**
 * xsltCallTemplate:
 * @ctxt:  a XSLT transformation context
 * @node:  the "current node" in the source tree
 * @inst:  the XSLT 'call-template' instruction
 * @castedComp:  the compiled information of the instruction
 *
 * Processes the XSLT call-template instruction on the source node.
 */
void
xsltCallTemplate(xsltTransformContextPtr ctxt, xmlNodePtr node,
	           xmlNodePtr inst, xsltElemPreCompPtr castedComp)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemCallTemplatePtr comp =
	(xsltStyleItemCallTemplatePtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    xsltStackElemPtr withParams = NULL;

    if (ctxt->insert == NULL)
	return;
    if (comp == NULL) {
	xsltTransformError(ctxt, NULL, inst,
	     "The XSLT 'call-template' instruction was not compiled.\n");
	return;
    }

    /*
     * The template must have been precomputed
     */
    if (comp->templ == NULL) {
	comp->templ = xsltFindTemplate(ctxt, comp->name, comp->ns);
	if (comp->templ == NULL) {
	    if (comp->ns != NULL) {
	        xsltTransformError(ctxt, NULL, inst,
			"The called template '{%s}%s' was not found.\n",
			comp->ns, comp->name);
	    } else {
	        xsltTransformError(ctxt, NULL, inst,
			"The called template '%s' was not found.\n",
			comp->name);
	    }
	    return;
	}
    }

#ifdef WITH_XSLT_DEBUG_PROCESS
    if ((comp != NULL) && (comp->name != NULL))
	XSLT_TRACE(ctxt,XSLT_TRACE_CALL_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
			 "call-template: name %s\n", comp->name));
#endif

    if (inst->children) {
	xmlNodePtr cur;
	xsltStackElemPtr param;

	cur = inst->children;
	while (cur != NULL) {
#ifdef WITH_DEBUGGER
	    if (ctxt->debugStatus != XSLT_DEBUG_NONE)
		xslHandleDebugger(cur, node, comp->templ, ctxt);
#endif
	    if (ctxt->state == XSLT_STATE_STOPPED) break;
	    /*
	    * TODO: The "with-param"s could be part of the "call-template"
	    *   structure. Avoid to "search" for params dynamically
	    *   in the XML tree every time.
	    */
	    if (IS_XSLT_ELEM(cur)) {
		if (IS_XSLT_NAME(cur, "with-param")) {
		    param = xsltParseStylesheetCallerParam(ctxt, cur);
		    if (param != NULL) {
			param->next = withParams;
			withParams = param;
		    }
		} else {
		    xsltGenericError(xsltGenericErrorContext,
			"xsl:call-template: misplaced xsl:%s\n", cur->name);
		}
	    } else {
		xsltGenericError(xsltGenericErrorContext,
		    "xsl:call-template: misplaced %s element\n", cur->name);
	    }
	    cur = cur->next;
	}
    }
    /*
     * Create a new frame using the params first
     */
    xsltApplyXSLTTemplate(ctxt, node, comp->templ->content, comp->templ,
	withParams);
    if (withParams != NULL)
	xsltFreeStackElemList(withParams);

#ifdef WITH_XSLT_DEBUG_PROCESS
    if ((comp != NULL) && (comp->name != NULL))
	XSLT_TRACE(ctxt,XSLT_TRACE_CALL_TEMPLATE,xsltGenericDebug(xsltGenericDebugContext,
			 "call-template returned: name %s\n", comp->name));
#endif
}

/**
 * xsltApplyTemplates:
 * @ctxt:  a XSLT transformation context
 * @node:  the 'current node' in the source tree
 * @inst:  the element node of an XSLT 'apply-templates' instruction
 * @castedComp:  the compiled instruction
 *
 * Processes the XSLT 'apply-templates' instruction on the current node.
 */
void
xsltApplyTemplates(xsltTransformContextPtr ctxt, xmlNodePtr node,
	           xmlNodePtr inst, xsltElemPreCompPtr castedComp)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemApplyTemplatesPtr comp =
	(xsltStyleItemApplyTemplatesPtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    int i;
    xmlNodePtr cur, oldContextNode;
    xmlNodeSetPtr list = NULL, oldList;
    xsltStackElemPtr withParams = NULL;
    int oldXPProximityPosition, oldXPContextSize;
    const xmlChar *oldMode, *oldModeURI;
    xmlDocPtr oldXPDoc;
    xsltDocumentPtr oldDocInfo;
    xmlXPathContextPtr xpctxt;

    if (comp == NULL) {
	xsltTransformError(ctxt, NULL, inst,
	     "xsl:apply-templates : compilation failed\n");
	return;
    }
    if ((ctxt == NULL) || (node == NULL) || (inst == NULL) || (comp == NULL))
	return;

#ifdef WITH_XSLT_DEBUG_PROCESS
    if ((node != NULL) && (node->name != NULL))
	XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATES,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltApplyTemplates: node: '%s'\n", node->name));
#endif

    xpctxt = ctxt->xpathCtxt;
    /*
    * Save context states.
    */
    oldContextNode = ctxt->node;
    oldMode = ctxt->mode;
    oldModeURI = ctxt->modeURI;
    oldDocInfo = ctxt->document;
    oldList = ctxt->nodeList;

    /*
     * The xpath context size and proximity position, as
     * well as the xpath and context documents, may be changed
     * so we save their initial state and will restore on exit
     */
    oldXPContextSize = xpctxt->contextSize;
    oldXPProximityPosition = xpctxt->proximityPosition;
    oldXPDoc = xpctxt->doc;

    /*
    * Set up contexts.
    */
    ctxt->mode = comp->mode;
    ctxt->modeURI = comp->modeURI;

    if (comp->select != NULL) {
	xmlXPathObjectPtr res = NULL;

	if (comp->comp == NULL) {
	    xsltTransformError(ctxt, NULL, inst,
		 "xsl:apply-templates : compilation failed\n");
	    goto error;
	}
#ifdef WITH_XSLT_DEBUG_PROCESS
	XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATES,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltApplyTemplates: select %s\n", comp->select));
#endif

	res = xsltPreCompEval(ctxt, node, comp);

	if (res != NULL) {
	    if (res->type == XPATH_NODESET) {
		list = res->nodesetval; /* consume the node set */
		res->nodesetval = NULL;
	    } else {
		xsltTransformError(ctxt, NULL, inst,
		    "The 'select' expression did not evaluate to a "
		    "node set.\n");
		ctxt->state = XSLT_STATE_STOPPED;
		xmlXPathFreeObject(res);
		goto error;
	    }
	    xmlXPathFreeObject(res);
	    /*
	    * Note: An xsl:apply-templates with a 'select' attribute,
	    * can change the current source doc.
	    */
	} else {
	    xsltTransformError(ctxt, NULL, inst,
		"Failed to evaluate the 'select' expression.\n");
	    ctxt->state = XSLT_STATE_STOPPED;
	    goto error;
	}
	if (list == NULL) {
#ifdef WITH_XSLT_DEBUG_PROCESS
	    XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATES,xsltGenericDebug(xsltGenericDebugContext,
		"xsltApplyTemplates: select didn't evaluate to a node list\n"));
#endif
	    goto exit;
	}
	/*
	*
	* NOTE: Previously a document info (xsltDocument) was
	* created and attached to the Result Tree Fragment.
	* But such a document info is created on demand in
	* xsltKeyFunction() (functions.c), so we need to create
	* it here beforehand.
	* In order to take care of potential keys we need to
	* do some extra work for the case when a Result Tree Fragment
	* is converted into a nodeset (e.g. exslt:node-set()) :
	* We attach a "pseudo-doc" (xsltDocument) to _private.
	* This xsltDocument, together with the keyset, will be freed
	* when the Result Tree Fragment is freed.
	*
	*/
#if 0
	if ((ctxt->nbKeys > 0) &&
	    (list->nodeNr != 0) &&
	    (list->nodeTab[0]->doc != NULL) &&
	    XSLT_IS_RES_TREE_FRAG(list->nodeTab[0]->doc))
	{
	    /*
	    * NOTE that it's also OK if @effectiveDocInfo will be
	    * set to NULL.
	    */
	    isRTF = 1;
	    effectiveDocInfo = list->nodeTab[0]->doc->_private;
	}
#endif
    } else {
	/*
	 * Build an XPath node set with the children
	 */
	list = xmlXPathNodeSetCreate(NULL);
	if (list == NULL)
	    goto error;
	if (node->type != XML_NAMESPACE_DECL)
	    cur = node->children;
	else
	    cur = NULL;
	while (cur != NULL) {
            if (IS_XSLT_REAL_NODE(cur))
		xmlXPathNodeSetAddUnique(list, cur);
	    cur = cur->next;
	}
    }

#ifdef WITH_XSLT_DEBUG_PROCESS
    if (list != NULL)
    XSLT_TRACE(ctxt,XSLT_TRACE_APPLY_TEMPLATES,xsltGenericDebug(xsltGenericDebugContext,
	"xsltApplyTemplates: list of %d nodes\n", list->nodeNr));
#endif

    if ((list == NULL) || (list->nodeNr == 0))
	goto exit;

    /*
    * Set the context's node set and size; this is also needed for
    * for xsltDoSortFunction().
    */
    ctxt->nodeList = list;
    /*
    * Process xsl:with-param and xsl:sort instructions.
    * (The code became so verbose just to avoid the
    *  xmlNodePtr sorts[XSLT_MAX_SORT] if there's no xsl:sort)
    * BUG TODO: We are not using namespaced potentially defined on the
    * xsl:sort or xsl:with-param elements; XPath expression might fail.
    */
    if (inst->children) {
	xsltStackElemPtr param;

	cur = inst->children;
	while (cur) {

#ifdef WITH_DEBUGGER
	    if (ctxt->debugStatus != XSLT_DEBUG_NONE)
		xslHandleDebugger(cur, node, NULL, ctxt);
#endif
	    if (ctxt->state == XSLT_STATE_STOPPED)
		break;
	    if (cur->type == XML_TEXT_NODE) {
		cur = cur->next;
		continue;
	    }
	    if (! IS_XSLT_ELEM(cur))
		break;
	    if (IS_XSLT_NAME(cur, "with-param")) {
		param = xsltParseStylesheetCallerParam(ctxt, cur);
		if (param != NULL) {
		    param->next = withParams;
		    withParams = param;
		}
	    }
	    if (IS_XSLT_NAME(cur, "sort")) {
		xsltTemplatePtr oldCurTempRule =
		    ctxt->currentTemplateRule;
		int nbsorts = 0;
		xmlNodePtr sorts[XSLT_MAX_SORT];

		sorts[nbsorts++] = cur;
		cur = cur->next;

		while (cur) {

#ifdef WITH_DEBUGGER
		    if (ctxt->debugStatus != XSLT_DEBUG_NONE)
			xslHandleDebugger(cur, node, NULL, ctxt);
#endif
		    if (ctxt->state == XSLT_STATE_STOPPED)
			break;

		    if (cur->type == XML_TEXT_NODE) {
			cur = cur->next;
			continue;
		    }

		    if (! IS_XSLT_ELEM(cur))
			break;
		    if (IS_XSLT_NAME(cur, "with-param")) {
			param = xsltParseStylesheetCallerParam(ctxt, cur);
			if (param != NULL) {
			    param->next = withParams;
			    withParams = param;
			}
		    }
		    if (IS_XSLT_NAME(cur, "sort")) {
			if (nbsorts >= XSLT_MAX_SORT) {
			    xsltTransformError(ctxt, NULL, cur,
				"The number (%d) of xsl:sort instructions exceeds the "
				"maximum allowed by this processor's settings.\n",
				nbsorts);
			    ctxt->state = XSLT_STATE_STOPPED;
			    break;
			} else {
			    sorts[nbsorts++] = cur;
			}
		    }
		    cur = cur->next;
		}
		/*
		* The "current template rule" is cleared for xsl:sort.
		*/
		ctxt->currentTemplateRule = NULL;
		/*
		* Sort.
		*/
		xsltDoSortFunction(ctxt, sorts, nbsorts);
		ctxt->currentTemplateRule = oldCurTempRule;
		break;
	    }
	    cur = cur->next;
	}
    }
    xpctxt->contextSize = list->nodeNr;
    /*
    * Apply templates for all selected source nodes.
    */
    for (i = 0; i < list->nodeNr; i++) {
	cur = list->nodeTab[i];
	/*
	* The node becomes the "current node".
	*/
	ctxt->node = cur;
	/*
	* An xsl:apply-templates can change the current context doc.
	* OPTIMIZE TODO: Get rid of the need to set the context doc.
	*/
	if ((cur->type != XML_NAMESPACE_DECL) && (cur->doc != NULL))
	    xpctxt->doc = cur->doc;

	xpctxt->proximityPosition = i + 1;
	/*
	* Find and apply a template for this node.
	*/
	xsltProcessOneNode(ctxt, cur, withParams);
    }

exit:
error:
    /*
    * Free the parameter list.
    */
    if (withParams != NULL)
	xsltFreeStackElemList(withParams);
    if (list != NULL)
	xmlXPathFreeNodeSet(list);
    /*
    * Restore context states.
    */
    xpctxt->doc = oldXPDoc;
    xpctxt->contextSize = oldXPContextSize;
    xpctxt->proximityPosition = oldXPProximityPosition;

    ctxt->document = oldDocInfo;
    ctxt->nodeList = oldList;
    ctxt->node = oldContextNode;
    ctxt->mode = oldMode;
    ctxt->modeURI = oldModeURI;
}


/**
 * xsltChoose:
 * @ctxt:  a XSLT process context
 * @contextNode:  the current node in the source tree
 * @inst:  the xsl:choose instruction
 * @comp:  compiled information of the instruction
 *
 * Processes the xsl:choose instruction on the source node.
 */
void
xsltChoose(xsltTransformContextPtr ctxt, xmlNodePtr contextNode,
	   xmlNodePtr inst, xsltElemPreCompPtr comp ATTRIBUTE_UNUSED)
{
    xmlNodePtr cur;

    if ((ctxt == NULL) || (contextNode == NULL) || (inst == NULL))
	return;

    /*
    * TODO: Content model checks should be done only at compilation
    * time.
    */
    cur = inst->children;
    if (cur == NULL) {
	xsltTransformError(ctxt, NULL, inst,
	    "xsl:choose: The instruction has no content.\n");
	return;
    }

#ifdef XSLT_REFACTORED
    /*
    * We don't check the content model during transformation.
    */
#else
    if ((! IS_XSLT_ELEM(cur)) || (! IS_XSLT_NAME(cur, "when"))) {
	xsltTransformError(ctxt, NULL, inst,
	     "xsl:choose: xsl:when expected first\n");
	return;
    }
#endif

    {
	int testRes = 0, res = 0;

#ifdef XSLT_REFACTORED
	xsltStyleItemWhenPtr wcomp = NULL;
#else
	xsltStylePreCompPtr wcomp = NULL;
#endif

	/*
	* Process xsl:when ---------------------------------------------------
	*/
	while (IS_XSLT_ELEM(cur) && IS_XSLT_NAME(cur, "when")) {
	    wcomp = cur->psvi;

	    if ((wcomp == NULL) || (wcomp->test == NULL) ||
		(wcomp->comp == NULL))
	    {
		xsltTransformError(ctxt, NULL, cur,
		    "Internal error in xsltChoose(): "
		    "The XSLT 'when' instruction was not compiled.\n");
		goto error;
	    }


#ifdef WITH_DEBUGGER
	    if (xslDebugStatus != XSLT_DEBUG_NONE) {
		/*
		* TODO: Isn't comp->templ always NULL for xsl:choose?
		*/
		xslHandleDebugger(cur, contextNode, NULL, ctxt);
	    }
#endif
#ifdef WITH_XSLT_DEBUG_PROCESS
	    XSLT_TRACE(ctxt,XSLT_TRACE_CHOOSE,xsltGenericDebug(xsltGenericDebugContext,
		"xsltChoose: test %s\n", wcomp->test));
#endif

#ifdef XSLT_FAST_IF
	    res = xsltPreCompEvalToBoolean(ctxt, contextNode, wcomp);

	    if (res == -1) {
		ctxt->state = XSLT_STATE_STOPPED;
		goto error;
	    }
	    testRes = (res == 1) ? 1 : 0;

#else /* XSLT_FAST_IF */

	    res = xsltPreCompEval(ctxt, cotextNode, wcomp);

	    if (res != NULL) {
		if (res->type != XPATH_BOOLEAN)
		    res = xmlXPathConvertBoolean(res);
		if (res->type == XPATH_BOOLEAN)
		    testRes = res->boolval;
		else {
#ifdef WITH_XSLT_DEBUG_PROCESS
		    XSLT_TRACE(ctxt,XSLT_TRACE_CHOOSE,xsltGenericDebug(xsltGenericDebugContext,
			"xsltChoose: test didn't evaluate to a boolean\n"));
#endif
		    goto error;
		}
		xmlXPathFreeObject(res);
		res = NULL;
	    } else {
		ctxt->state = XSLT_STATE_STOPPED;
		goto error;
	    }

#endif /* else of XSLT_FAST_IF */

#ifdef WITH_XSLT_DEBUG_PROCESS
	    XSLT_TRACE(ctxt,XSLT_TRACE_CHOOSE,xsltGenericDebug(xsltGenericDebugContext,
		"xsltChoose: test evaluate to %d\n", testRes));
#endif
	    if (testRes)
		goto test_is_true;

	    cur = cur->next;
	}

	/*
	* Process xsl:otherwise ----------------------------------------------
	*/
	if (IS_XSLT_ELEM(cur) && IS_XSLT_NAME(cur, "otherwise")) {

#ifdef WITH_DEBUGGER
	    if (xslDebugStatus != XSLT_DEBUG_NONE)
		xslHandleDebugger(cur, contextNode, NULL, ctxt);
#endif

#ifdef WITH_XSLT_DEBUG_PROCESS
	    XSLT_TRACE(ctxt,XSLT_TRACE_CHOOSE,xsltGenericDebug(xsltGenericDebugContext,
		"evaluating xsl:otherwise\n"));
#endif
	    goto test_is_true;
	}
	goto exit;

test_is_true:

	goto process_sequence;
    }

process_sequence:

    /*
    * Instantiate the sequence constructor.
    */
    xsltApplySequenceConstructor(ctxt, ctxt->node, cur->children,
	NULL);

exit:
error:
    return;
}

/**
 * xsltIf:
 * @ctxt:  a XSLT process context
 * @contextNode:  the current node in the source tree
 * @inst:  the xsl:if instruction
 * @castedComp:  compiled information of the instruction
 *
 * Processes the xsl:if instruction on the source node.
 */
void
xsltIf(xsltTransformContextPtr ctxt, xmlNodePtr contextNode,
	           xmlNodePtr inst, xsltElemPreCompPtr castedComp)
{
    int res = 0;

#ifdef XSLT_REFACTORED
    xsltStyleItemIfPtr comp = (xsltStyleItemIfPtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif

    if ((ctxt == NULL) || (contextNode == NULL) || (inst == NULL))
	return;
    if ((comp == NULL) || (comp->test == NULL) || (comp->comp == NULL)) {
	xsltTransformError(ctxt, NULL, inst,
	    "Internal error in xsltIf(): "
	    "The XSLT 'if' instruction was not compiled.\n");
	return;
    }

#ifdef WITH_XSLT_DEBUG_PROCESS
    XSLT_TRACE(ctxt,XSLT_TRACE_IF,xsltGenericDebug(xsltGenericDebugContext,
	 "xsltIf: test %s\n", comp->test));
#endif

#ifdef XSLT_FAST_IF
    {
	xmlDocPtr oldLocalFragmentTop = ctxt->localRVT;

	res = xsltPreCompEvalToBoolean(ctxt, contextNode, comp);

	/*
	* Cleanup fragments created during evaluation of the
	* "select" expression.
	*/
	if (oldLocalFragmentTop != ctxt->localRVT)
	    xsltReleaseLocalRVTs(ctxt, oldLocalFragmentTop);
    }

#ifdef WITH_XSLT_DEBUG_PROCESS
    XSLT_TRACE(ctxt,XSLT_TRACE_IF,xsltGenericDebug(xsltGenericDebugContext,
	"xsltIf: test evaluate to %d\n", res));
#endif

    if (res == -1) {
	ctxt->state = XSLT_STATE_STOPPED;
	goto error;
    }
    if (res == 1) {
	/*
	* Instantiate the sequence constructor of xsl:if.
	*/
	xsltApplySequenceConstructor(ctxt,
	    contextNode, inst->children, NULL);
    }

#else /* XSLT_FAST_IF */
    {
	/*
	* OLD CODE:
	*/
	xmlXPathObjectPtr xpobj = xsltPreCompEval(ctxt, contextNode, comp);
	if (xpobj != NULL) {
	    if (xpobj->type != XPATH_BOOLEAN)
		xpobj = xmlXPathConvertBoolean(xpobj);
	    if (xpobj->type == XPATH_BOOLEAN) {
		res = xpobj->boolval;

#ifdef WITH_XSLT_DEBUG_PROCESS
		XSLT_TRACE(ctxt,XSLT_TRACE_IF,xsltGenericDebug(xsltGenericDebugContext,
		    "xsltIf: test evaluate to %d\n", res));
#endif
		if (res) {
		    xsltApplySequenceConstructor(ctxt,
			contextNode, inst->children, NULL);
		}
	    } else {

#ifdef WITH_XSLT_DEBUG_PROCESS
		XSLT_TRACE(ctxt, XSLT_TRACE_IF,
		    xsltGenericDebug(xsltGenericDebugContext,
		    "xsltIf: test didn't evaluate to a boolean\n"));
#endif
		ctxt->state = XSLT_STATE_STOPPED;
	    }
	    xmlXPathFreeObject(xpobj);
	} else {
	    ctxt->state = XSLT_STATE_STOPPED;
	}
    }
#endif /* else of XSLT_FAST_IF */

error:
    return;
}

/**
 * xsltForEach:
 * @ctxt:  an XSLT transformation context
 * @contextNode:  the "current node" in the source tree
 * @inst:  the element node of the xsl:for-each instruction
 * @castedComp:  the compiled information of the instruction
 *
 * Process the xslt for-each node on the source node
 */
void
xsltForEach(xsltTransformContextPtr ctxt, xmlNodePtr contextNode,
	    xmlNodePtr inst, xsltElemPreCompPtr castedComp)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemForEachPtr comp = (xsltStyleItemForEachPtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    int i;
    xmlXPathObjectPtr res = NULL;
    xmlNodePtr cur, curInst;
    xmlNodeSetPtr list = NULL;
    xmlNodeSetPtr oldList;
    int oldXPProximityPosition, oldXPContextSize;
    xmlNodePtr oldContextNode;
    xsltTemplatePtr oldCurTemplRule;
    xmlDocPtr oldXPDoc;
    xsltDocumentPtr oldDocInfo;
    xmlXPathContextPtr xpctxt;

    if ((ctxt == NULL) || (contextNode == NULL) || (inst == NULL)) {
	xsltGenericError(xsltGenericErrorContext,
	    "xsltForEach(): Bad arguments.\n");
	return;
    }

    if (comp == NULL) {
        xsltTransformError(ctxt, NULL, inst,
	    "Internal error in xsltForEach(): "
	    "The XSLT 'for-each' instruction was not compiled.\n");
        return;
    }
    if ((comp->select == NULL) || (comp->comp == NULL)) {
	xsltTransformError(ctxt, NULL, inst,
	    "Internal error in xsltForEach(): "
	    "The selecting expression of the XSLT 'for-each' "
	    "instruction was not compiled correctly.\n");
	return;
    }
    xpctxt = ctxt->xpathCtxt;

#ifdef WITH_XSLT_DEBUG_PROCESS
    XSLT_TRACE(ctxt,XSLT_TRACE_FOR_EACH,xsltGenericDebug(xsltGenericDebugContext,
	 "xsltForEach: select %s\n", comp->select));
#endif

    /*
    * Save context states.
    */
    oldDocInfo = ctxt->document;
    oldList = ctxt->nodeList;
    oldContextNode = ctxt->node;
    /*
    * The "current template rule" is cleared for the instantiation of
    * xsl:for-each.
    */
    oldCurTemplRule = ctxt->currentTemplateRule;
    ctxt->currentTemplateRule = NULL;

    oldXPDoc = xpctxt->doc;
    oldXPProximityPosition = xpctxt->proximityPosition;
    oldXPContextSize = xpctxt->contextSize;

    /*
    * Evaluate the 'select' expression.
    */
    res = xsltPreCompEval(ctxt, contextNode, comp);

    if (res != NULL) {
	if (res->type == XPATH_NODESET)
	    list = res->nodesetval;
	else {
	    xsltTransformError(ctxt, NULL, inst,
		"The 'select' expression does not evaluate to a node set.\n");

#ifdef WITH_XSLT_DEBUG_PROCESS
	    XSLT_TRACE(ctxt,XSLT_TRACE_FOR_EACH,xsltGenericDebug(xsltGenericDebugContext,
		"xsltForEach: select didn't evaluate to a node list\n"));
#endif
	    goto error;
	}
    } else {
	xsltTransformError(ctxt, NULL, inst,
	    "Failed to evaluate the 'select' expression.\n");
	ctxt->state = XSLT_STATE_STOPPED;
	goto error;
    }

    if ((list == NULL) || (list->nodeNr <= 0))
	goto exit;

#ifdef WITH_XSLT_DEBUG_PROCESS
    XSLT_TRACE(ctxt,XSLT_TRACE_FOR_EACH,xsltGenericDebug(xsltGenericDebugContext,
	"xsltForEach: select evaluates to %d nodes\n", list->nodeNr));
#endif

    /*
    * Set the list; this has to be done already here for xsltDoSortFunction().
    */
    ctxt->nodeList = list;
    /*
    * Handle xsl:sort instructions and skip them for further processing.
    * BUG TODO: We are not using namespaced potentially defined on the
    * xsl:sort element; XPath expression might fail.
    */
    curInst = inst->children;
    if (IS_XSLT_ELEM(curInst) && IS_XSLT_NAME(curInst, "sort")) {
	int nbsorts = 0;
	xmlNodePtr sorts[XSLT_MAX_SORT];

	sorts[nbsorts++] = curInst;

#ifdef WITH_DEBUGGER
	if (xslDebugStatus != XSLT_DEBUG_NONE)
	    xslHandleDebugger(curInst, contextNode, NULL, ctxt);
#endif

	curInst = curInst->next;
	while (IS_XSLT_ELEM(curInst) && IS_XSLT_NAME(curInst, "sort")) {
	    if (nbsorts >= XSLT_MAX_SORT) {
		xsltTransformError(ctxt, NULL, curInst,
		    "The number of xsl:sort instructions exceeds the "
		    "maximum (%d) allowed by this processor.\n",
		    XSLT_MAX_SORT);
		goto error;
	    } else {
		sorts[nbsorts++] = curInst;
	    }

#ifdef WITH_DEBUGGER
	    if (xslDebugStatus != XSLT_DEBUG_NONE)
		xslHandleDebugger(curInst, contextNode, NULL, ctxt);
#endif
	    curInst = curInst->next;
	}
	xsltDoSortFunction(ctxt, sorts, nbsorts);
    }
    xpctxt->contextSize = list->nodeNr;
    /*
    * Instantiate the sequence constructor for each selected node.
    */
    for (i = 0; i < list->nodeNr; i++) {
	cur = list->nodeTab[i];
	/*
	* The selected node becomes the "current node".
	*/
	ctxt->node = cur;
	/*
	* An xsl:for-each can change the current context doc.
	* OPTIMIZE TODO: Get rid of the need to set the context doc.
	*/
	if ((cur->type != XML_NAMESPACE_DECL) && (cur->doc != NULL))
	    xpctxt->doc = cur->doc;

	xpctxt->proximityPosition = i + 1;

	xsltApplySequenceConstructor(ctxt, cur, curInst, NULL);
    }

exit:
error:
    if (res != NULL)
	xmlXPathFreeObject(res);
    /*
    * Restore old states.
    */
    ctxt->document = oldDocInfo;
    ctxt->nodeList = oldList;
    ctxt->node = oldContextNode;
    ctxt->currentTemplateRule = oldCurTemplRule;

    xpctxt->doc = oldXPDoc;
    xpctxt->contextSize = oldXPContextSize;
    xpctxt->proximityPosition = oldXPProximityPosition;
}

/************************************************************************
 *									*
 *			Generic interface				*
 *									*
 ************************************************************************/

#ifdef XSLT_GENERATE_HTML_DOCTYPE
typedef struct xsltHTMLVersion {
    const char *version;
    const char *public;
    const char *system;
} xsltHTMLVersion;

static xsltHTMLVersion xsltHTMLVersions[] = {
    { "5", NULL, "about:legacy-compat" },
    { "4.01frame", "-//W3C//DTD HTML 4.01 Frameset//EN",
      "http://www.w3.org/TR/1999/REC-html401-19991224/frameset.dtd"},
    { "4.01strict", "-//W3C//DTD HTML 4.01//EN",
      "http://www.w3.org/TR/1999/REC-html401-19991224/strict.dtd"},
    { "4.01trans", "-//W3C//DTD HTML 4.01 Transitional//EN",
      "http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd"},
    { "4.01", "-//W3C//DTD HTML 4.01 Transitional//EN",
      "http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd"},
    { "4.0strict", "-//W3C//DTD HTML 4.01//EN",
      "http://www.w3.org/TR/html4/strict.dtd"},
    { "4.0trans", "-//W3C//DTD HTML 4.01 Transitional//EN",
      "http://www.w3.org/TR/html4/loose.dtd"},
    { "4.0frame", "-//W3C//DTD HTML 4.01 Frameset//EN",
      "http://www.w3.org/TR/html4/frameset.dtd"},
    { "4.0", "-//W3C//DTD HTML 4.01 Transitional//EN",
      "http://www.w3.org/TR/html4/loose.dtd"},
    { "3.2", "-//W3C//DTD HTML 3.2//EN", NULL }
};

/**
 * xsltGetHTMLIDs:
 * @version:  the version string
 * @publicID:  used to return the public ID
 * @systemID:  used to return the system ID
 *
 * Returns -1 if not found, 0 otherwise and the system and public
 *         Identifier for this given verion of HTML
 */
static int
xsltGetHTMLIDs(const xmlChar *version, const xmlChar **publicID,
	            const xmlChar **systemID) {
    unsigned int i;
    if (version == NULL)
	return(-1);
    for (i = 0;i < (sizeof(xsltHTMLVersions)/sizeof(xsltHTMLVersions[1]));
	 i++) {
	if (!xmlStrcasecmp(version,
		           (const xmlChar *) xsltHTMLVersions[i].version)) {
	    if (publicID != NULL)
		*publicID = (const xmlChar *) xsltHTMLVersions[i].public;
	    if (systemID != NULL)
		*systemID = (const xmlChar *) xsltHTMLVersions[i].system;
	    return(0);
	}
    }
    return(-1);
}
#endif

/**
 * xsltApplyStripSpaces:
 * @ctxt:  a XSLT process context
 * @node:  the root of the XML tree
 *
 * Strip the unwanted ignorable spaces from the input tree
 */
void
xsltApplyStripSpaces(xsltTransformContextPtr ctxt, xmlNodePtr node) {
    xmlNodePtr current;
#ifdef WITH_XSLT_DEBUG_PROCESS
    int nb = 0;
#endif


    current = node;
    while (current != NULL) {
	/*
	 * Cleanup children empty nodes if asked for
	 */
	if ((IS_XSLT_REAL_NODE(current)) &&
	    (current->children != NULL) &&
	    (xsltFindElemSpaceHandling(ctxt, current))) {
	    xmlNodePtr delete = NULL, cur = current->children;

	    while (cur != NULL) {
		if (IS_BLANK_NODE(cur))
		    delete = cur;

		cur = cur->next;
		if (delete != NULL) {
		    xmlUnlinkNode(delete);
		    xmlFreeNode(delete);
		    delete = NULL;
#ifdef WITH_XSLT_DEBUG_PROCESS
		    nb++;
#endif
		}
	    }
	}

	/*
	 * Skip to next node in document order.
	 */
	if (node->type == XML_ENTITY_REF_NODE) {
	    /* process deep in entities */
	    xsltApplyStripSpaces(ctxt, node->children);
	}
	if ((current->children != NULL) &&
            (current->type != XML_ENTITY_REF_NODE)) {
	    current = current->children;
	} else if (current->next != NULL) {
	    current = current->next;
	} else {
	    do {
		current = current->parent;
		if (current == NULL)
		    break;
		if (current == node)
		    goto done;
		if (current->next != NULL) {
		    current = current->next;
		    break;
		}
	    } while (current != NULL);
	}
    }

done:
#ifdef WITH_XSLT_DEBUG_PROCESS
    XSLT_TRACE(ctxt,XSLT_TRACE_STRIP_SPACES,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltApplyStripSpaces: removed %d ignorable blank node\n", nb));
#endif
    return;
}

static int
xsltCountKeys(xsltTransformContextPtr ctxt)
{
    xsltStylesheetPtr style;
    xsltKeyDefPtr keyd;

    if (ctxt == NULL)
	return(-1);

    /*
    * Do we have those nastly templates with a key() in the match pattern?
    */
    ctxt->hasTemplKeyPatterns = 0;
    style = ctxt->style;
    while (style != NULL) {
	if (style->keyMatch != NULL) {
	    ctxt->hasTemplKeyPatterns = 1;
	    break;
	}
	style = xsltNextImport(style);
    }
    /*
    * Count number of key declarations.
    */
    ctxt->nbKeys = 0;
    style = ctxt->style;
    while (style != NULL) {
	keyd = style->keys;
	while (keyd) {
	    ctxt->nbKeys++;
	    keyd = keyd->next;
	}
	style = xsltNextImport(style);
    }
    return(ctxt->nbKeys);
}

/**
 * xsltApplyStylesheetInternal:
 * @style:  a parsed XSLT stylesheet
 * @doc:  a parsed XML document
 * @params:  a NULL terminated array of parameters names/values tuples
 * @output:  the targetted output
 * @profile:  profile FILE * output or NULL
 * @user:  user provided parameter
 *
 * Apply the stylesheet to the document
 * NOTE: This may lead to a non-wellformed output XML wise !
 *
 * Returns the result document or NULL in case of error
 */
static xmlDocPtr
xsltApplyStylesheetInternal(xsltStylesheetPtr style, xmlDocPtr doc,
                            const char **params, const char *output,
                            FILE * profile, xsltTransformContextPtr userCtxt)
{
    xmlDocPtr res = NULL;
    xsltTransformContextPtr ctxt = NULL;
    xmlNodePtr root, node;
    const xmlChar *method;
    const xmlChar *doctypePublic;
    const xmlChar *doctypeSystem;
    const xmlChar *version;
    const xmlChar *encoding;
    xsltStackElemPtr variables;
    xsltStackElemPtr vptr;

    xsltInitGlobals();

    if ((style == NULL) || (doc == NULL))
        return (NULL);

    if (style->internalized == 0) {
#ifdef WITH_XSLT_DEBUG
	xsltGenericDebug(xsltGenericDebugContext,
			 "Stylesheet was not fully internalized !\n");
#endif
    }
    if (doc->intSubset != NULL) {
	/*
	 * Avoid hitting the DTD when scanning nodes
	 * but keep it linked as doc->intSubset
	 */
	xmlNodePtr cur = (xmlNodePtr) doc->intSubset;
	if (cur->next != NULL)
	    cur->next->prev = cur->prev;
	if (cur->prev != NULL)
	    cur->prev->next = cur->next;
	if (doc->children == cur)
	    doc->children = cur->next;
	if (doc->last == cur)
	    doc->last = cur->prev;
	cur->prev = cur->next = NULL;
    }

    /*
     * Check for XPath document order availability
     */
    root = xmlDocGetRootElement(doc);
    if (root != NULL) {
	if (((ptrdiff_t) root->content >= 0) &&
            (xslDebugStatus == XSLT_DEBUG_NONE))
	    xmlXPathOrderDocElems(doc);
    }

    if (userCtxt != NULL)
	ctxt = userCtxt;
    else
	ctxt = xsltNewTransformContext(style, doc);

    if (ctxt == NULL)
        return (NULL);

    ctxt->initialContextDoc = doc;
    ctxt->initialContextNode = (xmlNodePtr) doc;

    if (profile != NULL) {
#ifdef WITH_PROFILER
        ctxt->profile = 1;
#else
        xsltTransformError(ctxt, NULL, (xmlNodePtr) doc,
                "xsltApplyStylesheetInternal: "
                "libxslt compiled without profiler\n");
        goto error;
#endif
    }

    if (output != NULL)
        ctxt->outputFile = output;
    else
        ctxt->outputFile = NULL;

    /*
     * internalize the modes if needed
     */
    if (ctxt->dict != NULL) {
        if (ctxt->mode != NULL)
	    ctxt->mode = xmlDictLookup(ctxt->dict, ctxt->mode, -1);
        if (ctxt->modeURI != NULL)
	    ctxt->modeURI = xmlDictLookup(ctxt->dict, ctxt->modeURI, -1);
    }

    XSLT_GET_IMPORT_PTR(method, style, method)
    XSLT_GET_IMPORT_PTR(doctypePublic, style, doctypePublic)
    XSLT_GET_IMPORT_PTR(doctypeSystem, style, doctypeSystem)
    XSLT_GET_IMPORT_PTR(version, style, version)
    XSLT_GET_IMPORT_PTR(encoding, style, encoding)

    if ((method != NULL) &&
	(!xmlStrEqual(method, (const xmlChar *) "xml")))
    {
        if (xmlStrEqual(method, (const xmlChar *) "html")) {
            ctxt->type = XSLT_OUTPUT_HTML;
            if (((doctypePublic != NULL) || (doctypeSystem != NULL))) {
                res = htmlNewDoc(doctypeSystem, doctypePublic);
	    } else {
                if (version == NULL) {
		    xmlDtdPtr dtd;

		    res = htmlNewDoc(NULL, NULL);
		    /*
		    * Make sure no DTD node is generated in this case
		    */
		    if (res != NULL) {
			dtd = xmlGetIntSubset(res);
			if (dtd != NULL) {
			    xmlUnlinkNode((xmlNodePtr) dtd);
			    xmlFreeDtd(dtd);
			}
			res->intSubset = NULL;
			res->extSubset = NULL;
		    }
		} else {

#ifdef XSLT_GENERATE_HTML_DOCTYPE
		    xsltGetHTMLIDs(version, &doctypePublic, &doctypeSystem);
#endif
		    res = htmlNewDoc(doctypeSystem, doctypePublic);
		}
            }
            if (res == NULL)
                goto error;
	    res->dict = ctxt->dict;
	    xmlDictReference(res->dict);

#ifdef WITH_XSLT_DEBUG
	    xsltGenericDebug(xsltGenericDebugContext,
		"reusing transformation dict for output\n");
#endif
        } else if (xmlStrEqual(method, (const xmlChar *) "xhtml")) {
	    xsltTransformError(ctxt, NULL, (xmlNodePtr) doc,
		"xsltApplyStylesheetInternal: unsupported method xhtml, using html\n");
            ctxt->type = XSLT_OUTPUT_HTML;
            res = htmlNewDoc(doctypeSystem, doctypePublic);
            if (res == NULL)
                goto error;
	    res->dict = ctxt->dict;
	    xmlDictReference(res->dict);

#ifdef WITH_XSLT_DEBUG
	    xsltGenericDebug(xsltGenericDebugContext,
		"reusing transformation dict for output\n");
#endif
        } else if (xmlStrEqual(method, (const xmlChar *) "text")) {
            ctxt->type = XSLT_OUTPUT_TEXT;
            res = xmlNewDoc(style->version);
            if (res == NULL)
                goto error;
	    res->dict = ctxt->dict;
	    xmlDictReference(res->dict);

#ifdef WITH_XSLT_DEBUG
	    xsltGenericDebug(xsltGenericDebugContext,
		"reusing transformation dict for output\n");
#endif
        } else {
	    xsltTransformError(ctxt, NULL, (xmlNodePtr) doc,
		"xsltApplyStylesheetInternal: unsupported method (%s)\n",
		method);
            goto error;
        }
    } else {
        ctxt->type = XSLT_OUTPUT_XML;
        res = xmlNewDoc(style->version);
        if (res == NULL)
            goto error;
	res->dict = ctxt->dict;
	xmlDictReference(ctxt->dict);
#ifdef WITH_XSLT_DEBUG
	xsltGenericDebug(xsltGenericDebugContext,
			 "reusing transformation dict for output\n");
#endif
    }
    res->charset = XML_CHAR_ENCODING_UTF8;
    if (encoding != NULL)
        res->encoding = xmlStrdup(encoding);
    variables = style->variables;

    ctxt->node = (xmlNodePtr) doc;
    ctxt->output = res;

    ctxt->xpathCtxt->contextSize = 1;
    ctxt->xpathCtxt->proximityPosition = 1;
    ctxt->xpathCtxt->node = NULL; /* TODO: Set the context node here? */

    /*
     * Start the evaluation, evaluate the params, the stylesheets globals
     * and start by processing the top node.
     */
    if (xsltNeedElemSpaceHandling(ctxt))
	xsltApplyStripSpaces(ctxt, xmlDocGetRootElement(doc));
    /*
    * Evaluate global params and user-provided params.
    */
    if (ctxt->globalVars == NULL)
	ctxt->globalVars = xmlHashCreate(20);
    if (params != NULL) {
        xsltEvalUserParams(ctxt, params);
    }

    /* need to be called before evaluating global variables */
    xsltCountKeys(ctxt);

    xsltEvalGlobalVariables(ctxt);

    /* Clean up any unused RVTs. */
    xsltReleaseLocalRVTs(ctxt, NULL);

    ctxt->insert = (xmlNodePtr) res;
    ctxt->varsBase = ctxt->varsNr - 1;

    /*
    * Start processing the source tree -----------------------------------
    */
    xsltProcessOneNode(ctxt, ctxt->node, NULL);
    /*
    * Remove all remaining vars from the stack.
    */
    xsltLocalVariablePop(ctxt, 0, -2);
    xsltShutdownCtxtExts(ctxt);

    xsltCleanupTemplates(style); /* TODO: <- style should be read only */

    /*
     * Now cleanup our variables so stylesheet can be re-used
     *
     * TODO: this is not needed anymore global variables are copied
     *       and not evaluated directly anymore, keep this as a check
     */
    if (style->variables != variables) {
        vptr = style->variables;
        while (vptr->next != variables)
            vptr = vptr->next;
        vptr->next = NULL;
        xsltFreeStackElemList(style->variables);
        style->variables = variables;
    }
    vptr = style->variables;
    while (vptr != NULL) {
        if (vptr->computed) {
            if (vptr->value != NULL) {
                xmlXPathFreeObject(vptr->value);
                vptr->value = NULL;
                vptr->computed = 0;
            }
        }
        vptr = vptr->next;
    }
#if 0
    /*
     * code disabled by wmb; awaiting kb's review
     * problem is that global variable(s) may contain xpath objects
     * from doc associated with RVT, so can't be freed at this point.
     * xsltFreeTransformContext includes a call to xsltFreeRVTs, so
     * I assume this shouldn't be required at this point.
     */
    /*
    * Free all remaining tree fragments.
    */
    xsltFreeRVTs(ctxt);
#endif
    /*
     * Do some post processing work depending on the generated output
     */
    root = xmlDocGetRootElement(res);
    if (root != NULL) {
        const xmlChar *doctype = NULL;

        if ((root->ns != NULL) && (root->ns->prefix != NULL))
	    doctype = xmlDictQLookup(ctxt->dict, root->ns->prefix, root->name);
	if (doctype == NULL)
	    doctype = root->name;

        /*
         * Apply the default selection of the method
         */
        if ((method == NULL) &&
            (root->ns == NULL) &&
            (!xmlStrcasecmp(root->name, (const xmlChar *) "html"))) {
            xmlNodePtr tmp;

            tmp = res->children;
            while ((tmp != NULL) && (tmp != root)) {
                if (tmp->type == XML_ELEMENT_NODE)
                    break;
                if ((tmp->type == XML_TEXT_NODE) && (!xmlIsBlankNode(tmp)))
                    break;
		tmp = tmp->next;
            }
            if (tmp == root) {
                ctxt->type = XSLT_OUTPUT_HTML;
		/*
		* REVISIT TODO: XML_HTML_DOCUMENT_NODE is set after the
		*  transformation on the doc, but functions like
		*/
                res->type = XML_HTML_DOCUMENT_NODE;
                if (((doctypePublic != NULL) || (doctypeSystem != NULL))) {
                    res->intSubset = xmlCreateIntSubset(res, doctype,
                                                        doctypePublic,
                                                        doctypeSystem);
#ifdef XSLT_GENERATE_HTML_DOCTYPE
		} else if (version != NULL) {
                    xsltGetHTMLIDs(version, &doctypePublic,
                                   &doctypeSystem);
                    if (((doctypePublic != NULL) || (doctypeSystem != NULL)))
                        res->intSubset =
                            xmlCreateIntSubset(res, doctype,
                                               doctypePublic,
                                               doctypeSystem);
#endif
                }
            }

        }
        if (ctxt->type == XSLT_OUTPUT_XML) {
            XSLT_GET_IMPORT_PTR(doctypePublic, style, doctypePublic)
            XSLT_GET_IMPORT_PTR(doctypeSystem, style, doctypeSystem)
            if (((doctypePublic != NULL) || (doctypeSystem != NULL))) {
	        xmlNodePtr last;
		/* Need a small "hack" here to assure DTD comes before
		   possible comment nodes */
		node = res->children;
		last = res->last;
		res->children = NULL;
		res->last = NULL;
                res->intSubset = xmlCreateIntSubset(res, doctype,
                                                    doctypePublic,
                                                    doctypeSystem);
		if (res->children != NULL) {
		    res->children->next = node;
		    node->prev = res->children;
		    res->last = last;
		} else {
		    res->children = node;
		    res->last = last;
		}
	    }
        }
    }
    xmlXPathFreeNodeSet(ctxt->nodeList);

#ifdef WITH_PROFILER
    if (profile != NULL) {
        xsltSaveProfiling(ctxt, profile);
    }
#endif

    /*
     * Be pedantic.
     */
    if ((ctxt != NULL) && (ctxt->state != XSLT_STATE_OK)) {
	xmlFreeDoc(res);
	res = NULL;
    }
    if ((res != NULL) && (ctxt != NULL) && (output != NULL)) {
	int ret;

	ret = xsltCheckWrite(ctxt->sec, ctxt, (const xmlChar *) output);
	if (ret == 0) {
	    xsltTransformError(ctxt, NULL, NULL,
		     "xsltApplyStylesheet: forbidden to save to %s\n",
			       output);
	} else if (ret < 0) {
	    xsltTransformError(ctxt, NULL, NULL,
		     "xsltApplyStylesheet: saving to %s may not be possible\n",
			       output);
	}
    }

#ifdef XSLT_DEBUG_PROFILE_CACHE
    printf("# Cache:\n");
    printf("# Reused tree fragments: %d\n", ctxt->cache->dbgReusedRVTs);
    printf("# Reused variables     : %d\n", ctxt->cache->dbgReusedVars);
#endif

    if ((ctxt != NULL) && (userCtxt == NULL))
	xsltFreeTransformContext(ctxt);

    return (res);

error:
    if (res != NULL)
        xmlFreeDoc(res);

#ifdef XSLT_DEBUG_PROFILE_CACHE
    printf("# Cache:\n");
    printf("# Reused tree fragments: %d\n", ctxt->cache->dbgReusedRVTs);
    printf("# Reused variables     : %d\n", ctxt->cache->dbgReusedVars);
#endif

    if ((ctxt != NULL) && (userCtxt == NULL))
        xsltFreeTransformContext(ctxt);
    return (NULL);
}

/**
 * xsltApplyStylesheet:
 * @style:  a parsed XSLT stylesheet
 * @doc:  a parsed XML document
 * @params:  a NULL terminated arry of parameters names/values tuples
 *
 * Apply the stylesheet to the document
 * NOTE: This may lead to a non-wellformed output XML wise !
 *
 * Returns the result document or NULL in case of error
 */
xmlDocPtr
xsltApplyStylesheet(xsltStylesheetPtr style, xmlDocPtr doc,
                    const char **params)
{
    return (xsltApplyStylesheetInternal(style, doc, params, NULL, NULL, NULL));
}

/**
 * xsltProfileStylesheet:
 * @style:  a parsed XSLT stylesheet
 * @doc:  a parsed XML document
 * @params:  a NULL terminated arry of parameters names/values tuples
 * @output:  a FILE * for the profiling output
 *
 * Apply the stylesheet to the document and dump the profiling to
 * the given output.
 *
 * Returns the result document or NULL in case of error
 */
xmlDocPtr
xsltProfileStylesheet(xsltStylesheetPtr style, xmlDocPtr doc,
                      const char **params, FILE * output)
{
    xmlDocPtr res;

    res = xsltApplyStylesheetInternal(style, doc, params, NULL, output, NULL);
    return (res);
}

/**
 * xsltApplyStylesheetUser:
 * @style:  a parsed XSLT stylesheet
 * @doc:  a parsed XML document
 * @params:  a NULL terminated array of parameters names/values tuples
 * @output:  the targetted output
 * @profile:  profile FILE * output or NULL
 * @userCtxt:  user provided transform context
 *
 * Apply the stylesheet to the document and allow the user to provide
 * its own transformation context.
 *
 * Returns the result document or NULL in case of error
 */
xmlDocPtr
xsltApplyStylesheetUser(xsltStylesheetPtr style, xmlDocPtr doc,
                            const char **params, const char *output,
                            FILE * profile, xsltTransformContextPtr userCtxt)
{
    xmlDocPtr res;

    res = xsltApplyStylesheetInternal(style, doc, params, output,
	                              profile, userCtxt);
    return (res);
}

/**
 * xsltRunStylesheetUser:
 * @style:  a parsed XSLT stylesheet
 * @doc:  a parsed XML document
 * @params:  a NULL terminated array of parameters names/values tuples
 * @output:  the URL/filename ot the generated resource if available
 * @SAX:  a SAX handler for progressive callback output (not implemented yet)
 * @IObuf:  an output buffer for progressive output (not implemented yet)
 * @profile:  profile FILE * output or NULL
 * @userCtxt:  user provided transform context
 *
 * Apply the stylesheet to the document and generate the output according
 * to @output @SAX and @IObuf. It's an error to specify both @SAX and @IObuf.
 *
 * NOTE: This may lead to a non-wellformed output XML wise !
 * NOTE: This may also result in multiple files being generated
 * NOTE: using IObuf, the result encoding used will be the one used for
 *       creating the output buffer, use the following macro to read it
 *       from the stylesheet
 *       XSLT_GET_IMPORT_PTR(encoding, style, encoding)
 * NOTE: using SAX, any encoding specified in the stylesheet will be lost
 *       since the interface uses only UTF8
 *
 * Returns the number of by written to the main resource or -1 in case of
 *         error.
 */
int
xsltRunStylesheetUser(xsltStylesheetPtr style, xmlDocPtr doc,
                  const char **params, const char *output,
                  xmlSAXHandlerPtr SAX, xmlOutputBufferPtr IObuf,
		  FILE * profile, xsltTransformContextPtr userCtxt)
{
    xmlDocPtr tmp;
    int ret;

    if ((output == NULL) && (SAX == NULL) && (IObuf == NULL))
        return (-1);
    if ((SAX != NULL) && (IObuf != NULL))
        return (-1);

    /* unsupported yet */
    if (SAX != NULL) {
        XSLT_TODO   /* xsltRunStylesheet xmlSAXHandlerPtr SAX */
	return (-1);
    }

    tmp = xsltApplyStylesheetInternal(style, doc, params, output, profile,
	                              userCtxt);
    if (tmp == NULL) {
	xsltTransformError(NULL, NULL, (xmlNodePtr) doc,
                         "xsltRunStylesheet : run failed\n");
        return (-1);
    }
    if (IObuf != NULL) {
        /* TODO: incomplete, IObuf output not progressive */
        ret = xsltSaveResultTo(IObuf, tmp, style);
    } else {
        ret = xsltSaveResultToFilename(output, tmp, style, 0);
    }
    xmlFreeDoc(tmp);
    return (ret);
}

/**
 * xsltRunStylesheet:
 * @style:  a parsed XSLT stylesheet
 * @doc:  a parsed XML document
 * @params:  a NULL terminated array of parameters names/values tuples
 * @output:  the URL/filename ot the generated resource if available
 * @SAX:  a SAX handler for progressive callback output (not implemented yet)
 * @IObuf:  an output buffer for progressive output (not implemented yet)
 *
 * Apply the stylesheet to the document and generate the output according
 * to @output @SAX and @IObuf. It's an error to specify both @SAX and @IObuf.
 *
 * NOTE: This may lead to a non-wellformed output XML wise !
 * NOTE: This may also result in multiple files being generated
 * NOTE: using IObuf, the result encoding used will be the one used for
 *       creating the output buffer, use the following macro to read it
 *       from the stylesheet
 *       XSLT_GET_IMPORT_PTR(encoding, style, encoding)
 * NOTE: using SAX, any encoding specified in the stylesheet will be lost
 *       since the interface uses only UTF8
 *
 * Returns the number of bytes written to the main resource or -1 in case of
 *         error.
 */
int
xsltRunStylesheet(xsltStylesheetPtr style, xmlDocPtr doc,
                  const char **params, const char *output,
                  xmlSAXHandlerPtr SAX, xmlOutputBufferPtr IObuf)
{
    return(xsltRunStylesheetUser(style, doc, params, output, SAX, IObuf,
		                 NULL, NULL));
}

static void
xsltMessageWrapper(xsltTransformContextPtr ctxt, xmlNodePtr node,
                   xmlNodePtr inst, xsltElemPreCompPtr comp ATTRIBUTE_UNUSED) {
    xsltMessage(ctxt, node, inst);
}

/**
 * xsltRegisterAllElement:
 * @ctxt:  the XPath context
 *
 * Registers all default XSLT elements in this context
 */
void
xsltRegisterAllElement(xsltTransformContextPtr ctxt)
{
    xsltRegisterExtElement(ctxt, (const xmlChar *) "apply-templates",
                           XSLT_NAMESPACE,
			   xsltApplyTemplates);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "apply-imports",
                           XSLT_NAMESPACE,
			   xsltApplyImports);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "call-template",
                           XSLT_NAMESPACE,
			   xsltCallTemplate);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "element",
                           XSLT_NAMESPACE,
			   xsltElement);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "attribute",
                           XSLT_NAMESPACE,
			   xsltAttribute);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "text",
                           XSLT_NAMESPACE,
			   xsltText);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "processing-instruction",
                           XSLT_NAMESPACE,
			   xsltProcessingInstruction);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "comment",
                           XSLT_NAMESPACE,
			   xsltComment);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "copy",
                           XSLT_NAMESPACE,
			   xsltCopy);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "value-of",
                           XSLT_NAMESPACE,
			   xsltValueOf);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "number",
                           XSLT_NAMESPACE,
			   xsltNumber);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "for-each",
                           XSLT_NAMESPACE,
			   xsltForEach);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "if",
                           XSLT_NAMESPACE,
			   xsltIf);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "choose",
                           XSLT_NAMESPACE,
			   xsltChoose);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "sort",
                           XSLT_NAMESPACE,
			   xsltSort);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "copy-of",
                           XSLT_NAMESPACE,
			   xsltCopyOf);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "message",
                           XSLT_NAMESPACE,
			   xsltMessageWrapper);

    /*
     * Those don't have callable entry points but are registered anyway
     */
    xsltRegisterExtElement(ctxt, (const xmlChar *) "variable",
                           XSLT_NAMESPACE,
			   xsltDebug);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "param",
                           XSLT_NAMESPACE,
			   xsltDebug);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "with-param",
                           XSLT_NAMESPACE,
			   xsltDebug);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "decimal-format",
                           XSLT_NAMESPACE,
			   xsltDebug);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "when",
                           XSLT_NAMESPACE,
			   xsltDebug);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "otherwise",
                           XSLT_NAMESPACE,
			   xsltDebug);
    xsltRegisterExtElement(ctxt, (const xmlChar *) "fallback",
                           XSLT_NAMESPACE,
			   xsltDebug);

}
