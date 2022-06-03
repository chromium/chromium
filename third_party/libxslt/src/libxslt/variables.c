/*
 * variables.c: Implementation of the variable storage and lookup
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

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/hash.h>
#include <libxml/xmlerror.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/parserInternals.h>
#include <libxml/dict.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "variables.h"
#include "transform.h"
#include "imports.h"
#include "preproc.h"
#include "keys.h"

#ifdef WITH_XSLT_DEBUG
 #define WITH_XSLT_DEBUG_VARIABLE
#endif

#ifdef XSLT_REFACTORED
const xmlChar *xsltDocFragFake = (const xmlChar *) " fake node libxslt";
#endif

static const xmlChar *xsltComputingGlobalVarMarker =
 (const xmlChar *) " var/param being computed";

#define XSLT_VAR_GLOBAL (1<<0)
#define XSLT_VAR_IN_SELECT (1<<1)
#define XSLT_TCTXT_VARIABLE(c) ((xsltStackElemPtr) (c)->contextVariable)

/************************************************************************
 *									*
 *  Result Value Tree (Result Tree Fragment) interfaces			*
 *									*
 ************************************************************************/
/**
 * xsltCreateRVT:
 * @ctxt:  an XSLT transformation context
 *
 * Creates a Result Value Tree
 * (the XSLT 1.0 term for this is "Result Tree Fragment")
 *
 * Returns the result value tree or NULL in case of API or internal errors.
 */
xmlDocPtr
xsltCreateRVT(xsltTransformContextPtr ctxt)
{
    xmlDocPtr container;

    /*
    * Question: Why is this function public?
    * Answer: It is called by the EXSLT module.
    */
    if (ctxt == NULL)
	return(NULL);

    /*
    * Reuse a RTF from the cache if available.
    */
    if (ctxt->cache->RVT) {
	container = ctxt->cache->RVT;
	ctxt->cache->RVT = (xmlDocPtr) container->next;
	/* clear the internal pointers */
	container->next = NULL;
	container->prev = NULL;
	if (ctxt->cache->nbRVT > 0)
	    ctxt->cache->nbRVT--;
#ifdef XSLT_DEBUG_PROFILE_CACHE
	ctxt->cache->dbgReusedRVTs++;
#endif
	return(container);
    }

    container = xmlNewDoc(NULL);
    if (container == NULL)
	return(NULL);
    container->dict = ctxt->dict;
    xmlDictReference(container->dict);
    XSLT_MARK_RES_TREE_FRAG(container);
    container->doc = container;
    container->parent = NULL;
    return(container);
}

/**
 * xsltRegisterTmpRVT:
 * @ctxt:  an XSLT transformation context
 * @RVT:  a result value tree (Result Tree Fragment)
 *
 * Registers the result value tree (XSLT 1.0 term: Result Tree Fragment)
 * in the garbage collector.
 * The fragment will be freed at the exit of the currently
 * instantiated xsl:template.
 * Obsolete; this function might produce massive memory overhead,
 * since the fragment is only freed when the current xsl:template
 * exits. Use xsltRegisterLocalRVT() instead.
 *
 * Returns 0 in case of success and -1 in case of API or internal errors.
 */
int
xsltRegisterTmpRVT(xsltTransformContextPtr ctxt, xmlDocPtr RVT)
{
    if ((ctxt == NULL) || (RVT == NULL))
	return(-1);

    RVT->prev = NULL;
    RVT->psvi = XSLT_RVT_LOCAL;

    /*
    * We'll restrict the lifetime of user-created fragments
    * insinde an xsl:variable and xsl:param to the lifetime of the
    * var/param itself.
    */
    if (ctxt->contextVariable != NULL) {
	RVT->next = (xmlNodePtr) XSLT_TCTXT_VARIABLE(ctxt)->fragment;
	XSLT_TCTXT_VARIABLE(ctxt)->fragment = RVT;
	return(0);
    }

    RVT->next = (xmlNodePtr) ctxt->tmpRVT;
    if (ctxt->tmpRVT != NULL)
	ctxt->tmpRVT->prev = (xmlNodePtr) RVT;
    ctxt->tmpRVT = RVT;
    return(0);
}

/**
 * xsltRegisterLocalRVT:
 * @ctxt:  an XSLT transformation context
 * @RVT:  a result value tree (Result Tree Fragment; xmlDocPtr)
 *
 * Registers a result value tree (XSLT 1.0 term: Result Tree Fragment)
 * in the RVT garbage collector.
 * The fragment will be freed when the instruction which created the
 * fragment exits.
 *
 * Returns 0 in case of success and -1 in case of API or internal errors.
 */
int
xsltRegisterLocalRVT(xsltTransformContextPtr ctxt,
		     xmlDocPtr RVT)
{
    if ((ctxt == NULL) || (RVT == NULL))
	return(-1);

    RVT->prev = NULL;
    RVT->psvi = XSLT_RVT_LOCAL;

    /*
    * When evaluating "select" expressions of xsl:variable
    * and xsl:param, we need to bind newly created tree fragments
    * to the variable itself; otherwise the fragment will be
    * freed before we leave the scope of a var.
    */
    if ((ctxt->contextVariable != NULL) &&
	(XSLT_TCTXT_VARIABLE(ctxt)->flags & XSLT_VAR_IN_SELECT))
    {
	RVT->next = (xmlNodePtr) XSLT_TCTXT_VARIABLE(ctxt)->fragment;
	XSLT_TCTXT_VARIABLE(ctxt)->fragment = RVT;
	return(0);
    }
    /*
    * Store the fragment in the scope of the current instruction.
    * If not reference by a returning instruction (like EXSLT's function),
    * then this fragment will be freed, when the instruction exits.
    */
    RVT->next = (xmlNodePtr) ctxt->localRVT;
    if (ctxt->localRVT != NULL)
	ctxt->localRVT->prev = (xmlNodePtr) RVT;
    ctxt->localRVT = RVT;
    return(0);
}

/**
 * xsltExtensionInstructionResultFinalize:
 * @ctxt:  an XSLT transformation context
 *
 * Finalizes the data (e.g. result tree fragments) created
 * within a value-returning process (e.g. EXSLT's function).
 * Tree fragments marked as being returned by a function are
 * set to normal state, which means that the fragment garbage
 * collector will free them after the function-calling process exits.
 *
 * Returns 0 in case of success and -1 in case of API or internal errors.
 *
 * This function is unsupported in newer releases of libxslt.
 */
int
xsltExtensionInstructionResultFinalize(
        xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED)
{
    xmlGenericError(xmlGenericErrorContext,
            "xsltExtensionInstructionResultFinalize is unsupported "
            "in this release of libxslt.\n");
    return(-1);
}

/**
 * xsltExtensionInstructionResultRegister:
 * @ctxt: an XSLT transformation context
 * @obj: an XPath object to be inspected for result tree fragments
 *
 * Marks the result of a value-returning extension instruction
 * in order to avoid it being garbage collected before the
 * extension instruction exits.
 * Note that one still has to additionally register any newly created
 * tree fragments (via xsltCreateRVT()) with xsltRegisterLocalRVT().
 *
 * Returns 0 in case of success and -1 in case of error.
 *
 * It isn't necessary to call this function in newer releases of
 * libxslt.
 */
int
xsltExtensionInstructionResultRegister(
        xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED,
	xmlXPathObjectPtr obj ATTRIBUTE_UNUSED)
{
    return(0);
}

/**
 * xsltFlagRVTs:
 * @ctxt: an XSLT transformation context
 * @obj: an XPath object to be inspected for result tree fragments
 * @val: the flag value
 *
 * Updates ownership information of RVTs in @obj according to @val.
 *
 * @val = XSLT_RVT_FUNC_RESULT for the result of an extension function, so its
 *        RVTs won't be destroyed after leaving the returning scope.
 * @val = XSLT_RVT_LOCAL for the result of an extension function to reset
 *        the state of its RVTs after it was returned to a new scope.
 * @val = XSLT_RVT_GLOBAL for parts of global variables.
 *
 * Returns 0 in case of success and -1 in case of error.
 */
int
xsltFlagRVTs(xsltTransformContextPtr ctxt, xmlXPathObjectPtr obj, void *val) {
    int i;
    xmlNodePtr cur;
    xmlDocPtr doc;

    if ((ctxt == NULL) || (obj == NULL))
	return(-1);

    /*
    * OPTIMIZE TODO: If no local variables/params and no local tree
    * fragments were created, then we don't need to analyse the XPath
    * objects for tree fragments.
    */

    if ((obj->type != XPATH_NODESET) && (obj->type != XPATH_XSLT_TREE))
	return(0);
    if ((obj->nodesetval == NULL) || (obj->nodesetval->nodeNr == 0))
	return(0);

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	cur = obj->nodesetval->nodeTab[i];
	if (cur->type == XML_NAMESPACE_DECL) {
	    /*
	    * The XPath module sets the owner element of a ns-node on
	    * the ns->next field.
	    */
	    if ((((xmlNsPtr) cur)->next != NULL) &&
		(((xmlNsPtr) cur)->next->type == XML_ELEMENT_NODE))
	    {
		cur = (xmlNodePtr) ((xmlNsPtr) cur)->next;
		doc = cur->doc;
	    } else {
		xsltTransformError(ctxt, NULL, ctxt->inst,
		    "Internal error in xsltFlagRVTs(): "
		    "Cannot retrieve the doc of a namespace node.\n");
		return(-1);
	    }
	} else {
	    doc = cur->doc;
	}
	if (doc == NULL) {
	    xsltTransformError(ctxt, NULL, ctxt->inst,
		"Internal error in xsltFlagRVTs(): "
		"Cannot retrieve the doc of a node.\n");
	    return(-1);
	}
	if (doc->name && (doc->name[0] == ' ') &&
            doc->psvi != XSLT_RVT_GLOBAL) {
	    /*
	    * This is a result tree fragment.
	    * We store ownership information in the @psvi field.
	    * TODO: How do we know if this is a doc acquired via the
	    *  document() function?
	    */
#ifdef WITH_XSLT_DEBUG_VARIABLE
            XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
                "Flagging RVT %p: %p -> %p\n", doc, doc->psvi, val));
#endif

            if (val == XSLT_RVT_LOCAL) {
                if (doc->psvi == XSLT_RVT_FUNC_RESULT)
                    doc->psvi = XSLT_RVT_LOCAL;
            } else if (val == XSLT_RVT_GLOBAL) {
                if (doc->psvi != XSLT_RVT_LOCAL) {
		    xmlGenericError(xmlGenericErrorContext,
                            "xsltFlagRVTs: Invalid transition %p => GLOBAL\n",
                            doc->psvi);
                    doc->psvi = XSLT_RVT_GLOBAL;
                    return(-1);
                }

                /* Will be registered as persistant in xsltReleaseLocalRVTs. */
                doc->psvi = XSLT_RVT_GLOBAL;
            } else if (val == XSLT_RVT_FUNC_RESULT) {
	        doc->psvi = val;
            }
	}
    }

    return(0);
}

/**
 * xsltReleaseRVT:
 * @ctxt:  an XSLT transformation context
 * @RVT:  a result value tree (Result Tree Fragment)
 *
 * Either frees the RVT (which is an xmlDoc) or stores
 * it in the context's cache for later reuse.
 */
void
xsltReleaseRVT(xsltTransformContextPtr ctxt, xmlDocPtr RVT)
{
    if (RVT == NULL)
	return;

    if (ctxt && (ctxt->cache->nbRVT < 40)) {
	/*
	* Store the Result Tree Fragment.
	* Free the document info.
	*/
	if (RVT->_private != NULL) {
	    xsltFreeDocumentKeys((xsltDocumentPtr) RVT->_private);
	    xmlFree(RVT->_private);
	    RVT->_private = NULL;
	}
	/*
	* Clear the document tree.
	* REVISIT TODO: Do we expect ID/IDREF tables to be existent?
	*/
	if (RVT->children != NULL) {
	    xmlFreeNodeList(RVT->children);
	    RVT->children = NULL;
	    RVT->last = NULL;
	}
	if (RVT->ids != NULL) {
	    xmlFreeIDTable((xmlIDTablePtr) RVT->ids);
	    RVT->ids = NULL;
	}
	if (RVT->refs != NULL) {
	    xmlFreeRefTable((xmlRefTablePtr) RVT->refs);
	    RVT->refs = NULL;
	}

	/*
	* Reset the ownership information.
	*/
	RVT->psvi = NULL;

	RVT->next = (xmlNodePtr) ctxt->cache->RVT;
	ctxt->cache->RVT = RVT;

	ctxt->cache->nbRVT++;

#ifdef XSLT_DEBUG_PROFILE_CACHE
	ctxt->cache->dbgCachedRVTs++;
#endif
	return;
    }
    /*
    * Free it.
    */
    if (RVT->_private != NULL) {
	xsltFreeDocumentKeys((xsltDocumentPtr) RVT->_private);
	xmlFree(RVT->_private);
    }
    xmlFreeDoc(RVT);
}

/**
 * xsltRegisterPersistRVT:
 * @ctxt:  an XSLT transformation context
 * @RVT:  a result value tree (Result Tree Fragment)
 *
 * Register the result value tree (XSLT 1.0 term: Result Tree Fragment)
 * in the fragment garbage collector.
 * The fragment will be freed when the transformation context is
 * freed.
 *
 * Returns 0 in case of success and -1 in case of error.
 */
int
xsltRegisterPersistRVT(xsltTransformContextPtr ctxt, xmlDocPtr RVT)
{
    if ((ctxt == NULL) || (RVT == NULL)) return(-1);

    RVT->psvi = XSLT_RVT_GLOBAL;
    RVT->prev = NULL;
    RVT->next = (xmlNodePtr) ctxt->persistRVT;
    if (ctxt->persistRVT != NULL)
	ctxt->persistRVT->prev = (xmlNodePtr) RVT;
    ctxt->persistRVT = RVT;
    return(0);
}

/**
 * xsltFreeRVTs:
 * @ctxt:  an XSLT transformation context
 *
 * Frees all registered result value trees (Result Tree Fragments)
 * of the transformation. Internal function; should not be called
 * by user-code.
 */
void
xsltFreeRVTs(xsltTransformContextPtr ctxt)
{
    xmlDocPtr cur, next;

    if (ctxt == NULL)
	return;
    /*
    * Local fragments.
    */
    cur = ctxt->localRVT;
    while (cur != NULL) {
        next = (xmlDocPtr) cur->next;
	if (cur->_private != NULL) {
	    xsltFreeDocumentKeys(cur->_private);
	    xmlFree(cur->_private);
	}
	xmlFreeDoc(cur);
	cur = next;
    }
    ctxt->localRVT = NULL;
    /*
    * User-created per-template fragments.
    */
    cur = ctxt->tmpRVT;
    while (cur != NULL) {
        next = (xmlDocPtr) cur->next;
	if (cur->_private != NULL) {
	    xsltFreeDocumentKeys(cur->_private);
	    xmlFree(cur->_private);
	}
	xmlFreeDoc(cur);
	cur = next;
    }
    ctxt->tmpRVT = NULL;
    /*
    * Global fragments.
    */
    cur = ctxt->persistRVT;
    while (cur != NULL) {
        next = (xmlDocPtr) cur->next;
	if (cur->_private != NULL) {
	    xsltFreeDocumentKeys(cur->_private);
	    xmlFree(cur->_private);
	}
	xmlFreeDoc(cur);
	cur = next;
    }
    ctxt->persistRVT = NULL;
}

/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/

/**
 * xsltNewStackElem:
 *
 * Create a new XSLT ParserContext
 *
 * Returns the newly allocated xsltParserStackElem or NULL in case of error
 */
static xsltStackElemPtr
xsltNewStackElem(xsltTransformContextPtr ctxt)
{
    xsltStackElemPtr ret;
    /*
    * Reuse a stack item from the cache if available.
    */
    if (ctxt && ctxt->cache->stackItems) {
	ret = ctxt->cache->stackItems;
	ctxt->cache->stackItems = ret->next;
	ret->next = NULL;
	ctxt->cache->nbStackItems--;
#ifdef XSLT_DEBUG_PROFILE_CACHE
	ctxt->cache->dbgReusedVars++;
#endif
	return(ret);
    }
    ret = (xsltStackElemPtr) xmlMalloc(sizeof(xsltStackElem));
    if (ret == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltNewStackElem : malloc failed\n");
	return(NULL);
    }
    memset(ret, 0, sizeof(xsltStackElem));
    ret->context = ctxt;
    return(ret);
}

/**
 * xsltCopyStackElem:
 * @elem:  an XSLT stack element
 *
 * Makes a copy of the stack element
 *
 * Returns the copy of NULL
 */
static xsltStackElemPtr
xsltCopyStackElem(xsltStackElemPtr elem) {
    xsltStackElemPtr cur;

    cur = (xsltStackElemPtr) xmlMalloc(sizeof(xsltStackElem));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltCopyStackElem : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltStackElem));
    cur->context = elem->context;
    cur->name = elem->name;
    cur->nameURI = elem->nameURI;
    cur->select = elem->select;
    cur->tree = elem->tree;
    cur->comp = elem->comp;
    return(cur);
}

/**
 * xsltFreeStackElem:
 * @elem:  an XSLT stack element
 *
 * Free up the memory allocated by @elem
 */
static void
xsltFreeStackElem(xsltStackElemPtr elem) {
    if (elem == NULL)
	return;
    if (elem->value != NULL)
	xmlXPathFreeObject(elem->value);
    /*
    * Release the list of temporary Result Tree Fragments.
    */
    if (elem->context) {
	xmlDocPtr cur;

	while (elem->fragment != NULL) {
	    cur = elem->fragment;
	    elem->fragment = (xmlDocPtr) cur->next;

            if (cur->psvi == XSLT_RVT_LOCAL) {
		xsltReleaseRVT(elem->context, cur);
            } else if (cur->psvi == XSLT_RVT_FUNC_RESULT) {
                xsltRegisterLocalRVT(elem->context, cur);
                cur->psvi = XSLT_RVT_FUNC_RESULT;
            } else {
                xmlGenericError(xmlGenericErrorContext,
                        "xsltFreeStackElem: Unexpected RVT flag %p\n",
                        cur->psvi);
            }
	}
    }
    /*
    * Cache or free the variable structure.
    */
    if (elem->context && (elem->context->cache->nbStackItems < 50)) {
	/*
	* Store the item in the cache.
	*/
	xsltTransformContextPtr ctxt = elem->context;
	memset(elem, 0, sizeof(xsltStackElem));
	elem->context = ctxt;
	elem->next = ctxt->cache->stackItems;
	ctxt->cache->stackItems = elem;
	ctxt->cache->nbStackItems++;
#ifdef XSLT_DEBUG_PROFILE_CACHE
	ctxt->cache->dbgCachedVars++;
#endif
	return;
    }
    xmlFree(elem);
}

static void
xsltFreeStackElemEntry(void *payload, const xmlChar *name ATTRIBUTE_UNUSED) {
    xsltFreeStackElem((xsltStackElemPtr) payload);
}


/**
 * xsltFreeStackElemList:
 * @elem:  an XSLT stack element
 *
 * Free up the memory allocated by @elem
 */
void
xsltFreeStackElemList(xsltStackElemPtr elem) {
    xsltStackElemPtr next;

    while (elem != NULL) {
	next = elem->next;
	xsltFreeStackElem(elem);
	elem = next;
    }
}

/**
 * xsltStackLookup:
 * @ctxt:  an XSLT transformation context
 * @name:  the local part of the name
 * @nameURI:  the URI part of the name
 *
 * Locate an element in the stack based on its name.
 */
#if 0 /* TODO: Those seem to have been used for debugging. */
static int stack_addr = 0;
static int stack_cmp = 0;
#endif

static xsltStackElemPtr
xsltStackLookup(xsltTransformContextPtr ctxt, const xmlChar *name,
	        const xmlChar *nameURI) {
    int i;
    xsltStackElemPtr cur;

    if ((ctxt == NULL) || (name == NULL) || (ctxt->varsNr == 0))
	return(NULL);

    /*
     * Do the lookup from the top of the stack, but
     * don't use params being computed in a call-param
     * First lookup expects the variable name and URI to
     * come from the disctionnary and hence pointer comparison.
     */
    for (i = ctxt->varsNr; i > ctxt->varsBase; i--) {
	cur = ctxt->varsTab[i-1];
	while (cur != NULL) {
	    if ((cur->name == name) && (cur->nameURI == nameURI)) {
#if 0
		stack_addr++;
#endif
		return(cur);
	    }
	    cur = cur->next;
	}
    }

    /*
     * Redo the lookup with interned string compares
     * to avoid string compares.
     */
    name = xmlDictLookup(ctxt->dict, name, -1);
    if (nameURI != NULL)
        nameURI = xmlDictLookup(ctxt->dict, nameURI, -1);

    for (i = ctxt->varsNr; i > ctxt->varsBase; i--) {
	cur = ctxt->varsTab[i-1];
	while (cur != NULL) {
	    if ((cur->name == name) && (cur->nameURI == nameURI)) {
#if 0
		stack_cmp++;
#endif
		return(cur);
	    }
	    cur = cur->next;
	}
    }

    return(NULL);
}

#ifdef XSLT_REFACTORED
#else

/**
 * xsltCheckStackElem:
 * @ctxt:  xn XSLT transformation context
 * @name:  the variable name
 * @nameURI:  the variable namespace URI
 *
 * Checks whether a variable or param is already defined.
 *
 * URGENT TODO: Checks for redefinition of vars/params should be
 *  done only at compilation time.
 *
 * Returns 1 if variable is present, 2 if param is present, 3 if this
 *         is an inherited param, 0 if not found, -1 in case of failure.
 */
static int
xsltCheckStackElem(xsltTransformContextPtr ctxt, const xmlChar *name,
	           const xmlChar *nameURI) {
    xsltStackElemPtr cur;

    if ((ctxt == NULL) || (name == NULL))
	return(-1);

    cur = xsltStackLookup(ctxt, name, nameURI);
    if (cur == NULL)
        return(0);
    if (cur->comp != NULL) {
        if (cur->comp->type == XSLT_FUNC_WITHPARAM)
	    return(3);
	else if (cur->comp->type == XSLT_FUNC_PARAM)
	    return(2);
    }

    return(1);
}

#endif /* XSLT_REFACTORED */

/**
 * xsltAddStackElem:
 * @ctxt:  xn XSLT transformation context
 * @elem:  a stack element
 *
 * Push an element (or list) onto the stack.
 * In case of a list, each member will be pushed into
 * a seperate slot; i.e. there's always 1 stack entry for
 * 1 stack element.
 *
 * Returns 0 in case of success, -1 in case of failure.
 */
static int
xsltAddStackElem(xsltTransformContextPtr ctxt, xsltStackElemPtr elem)
{
    if ((ctxt == NULL) || (elem == NULL))
	return(-1);

    do {
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
	ctxt->varsTab[ctxt->varsNr++] = elem;
	ctxt->vars = elem;

	elem = elem->next;
    } while (elem != NULL);

    return(0);
}

/**
 * xsltAddStackElemList:
 * @ctxt:  xn XSLT transformation context
 * @elems:  a stack element list
 *
 * Push an element list onto the stack.
 *
 * Returns 0 in case of success, -1 in case of failure.
 */
int
xsltAddStackElemList(xsltTransformContextPtr ctxt, xsltStackElemPtr elems)
{
    return(xsltAddStackElem(ctxt, elems));
}

/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/

/**
 * xsltEvalVariable:
 * @ctxt:  the XSLT transformation context
 * @variable:  the variable or parameter item
 * @comp: the compiled XSLT instruction
 *
 * Evaluate a variable value.
 *
 * Returns the XPath Object value or NULL in case of error
 */
static xmlXPathObjectPtr
xsltEvalVariable(xsltTransformContextPtr ctxt, xsltStackElemPtr variable,
	         xsltStylePreCompPtr castedComp)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemVariablePtr comp =
	(xsltStyleItemVariablePtr) castedComp;
#else
    xsltStylePreCompPtr comp = castedComp;
#endif
    xmlXPathObjectPtr result = NULL;
    xmlNodePtr oldInst;

    if ((ctxt == NULL) || (variable == NULL))
	return(NULL);

    /*
    * A variable or parameter are evaluated on demand; thus the
    * context (of XSLT and XPath) need to be temporarily adjusted and
    * restored on exit.
    */
    oldInst = ctxt->inst;

#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
	"Evaluating variable '%s'\n", variable->name));
#endif
    if (variable->select != NULL) {
	xmlXPathCompExprPtr xpExpr = NULL;
	xmlDocPtr oldXPDoc;
	xmlNodePtr oldXPContextNode;
	int oldXPProximityPosition, oldXPContextSize, oldXPNsNr;
	xmlNsPtr *oldXPNamespaces;
	xmlXPathContextPtr xpctxt = ctxt->xpathCtxt;
	xsltStackElemPtr oldVar = ctxt->contextVariable;

	if ((comp != NULL) && (comp->comp != NULL)) {
	    xpExpr = comp->comp;
	} else {
	    xpExpr = xmlXPathCtxtCompile(ctxt->xpathCtxt, variable->select);
	}
	if (xpExpr == NULL)
	    return(NULL);
	/*
	* Save context states.
	*/
	oldXPDoc = xpctxt->doc;
	oldXPContextNode = xpctxt->node;
	oldXPProximityPosition = xpctxt->proximityPosition;
	oldXPContextSize = xpctxt->contextSize;
	oldXPNamespaces = xpctxt->namespaces;
	oldXPNsNr = xpctxt->nsNr;

	xpctxt->node = ctxt->node;
	/*
	* OPTIMIZE TODO: Lame try to set the context doc.
	*   Get rid of this somehow in xpath.c.
	*/
	if ((ctxt->node->type != XML_NAMESPACE_DECL) &&
	    ctxt->node->doc)
	    xpctxt->doc = ctxt->node->doc;
	/*
	* BUG TODO: The proximity position and the context size will
	*  potentially be wrong.
	*  Example:
	*  <xsl:template select="foo">
	*    <xsl:variable name="pos" select="position()"/>
	*    <xsl:for-each select="bar">
	*      <xsl:value-of select="$pos"/>
	*    </xsl:for-each>
	*  </xsl:template>
	*  Here the proximity position and context size are changed
	*  to the context of <xsl:for-each select="bar">, but
	*  the variable needs to be evaluated in the context of
	*  <xsl:template select="foo">.
	*/
	if (comp != NULL) {

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
	} else {
	    xpctxt->namespaces = NULL;
	    xpctxt->nsNr = 0;
	}

	/*
	* We need to mark that we are "selecting" a var's value;
	* if any tree fragments are created inside the expression,
	* then those need to be stored inside the variable; otherwise
	* we'll eventually free still referenced fragments, before
	* we leave the scope of the variable.
	*/
	ctxt->contextVariable = variable;
	variable->flags |= XSLT_VAR_IN_SELECT;

	result = xmlXPathCompiledEval(xpExpr, xpctxt);

	variable->flags ^= XSLT_VAR_IN_SELECT;
	/*
	* Restore Context states.
	*/
	ctxt->contextVariable = oldVar;

	xpctxt->doc = oldXPDoc;
	xpctxt->node = oldXPContextNode;
	xpctxt->contextSize = oldXPContextSize;
	xpctxt->proximityPosition = oldXPProximityPosition;
	xpctxt->namespaces = oldXPNamespaces;
	xpctxt->nsNr = oldXPNsNr;

	if ((comp == NULL) || (comp->comp == NULL))
	    xmlXPathFreeCompExpr(xpExpr);
	if (result == NULL) {
	    xsltTransformError(ctxt, NULL,
		(comp != NULL) ? comp->inst : NULL,
		"Failed to evaluate the expression of variable '%s'.\n",
		variable->name);
	    ctxt->state = XSLT_STATE_STOPPED;

#ifdef WITH_XSLT_DEBUG_VARIABLE
#ifdef LIBXML_DEBUG_ENABLED
	} else {
	    if ((xsltGenericDebugContext == stdout) ||
		(xsltGenericDebugContext == stderr))
		xmlXPathDebugDumpObject((FILE *)xsltGenericDebugContext,
					result, 0);
#endif
#endif
	}
    } else {
	if (variable->tree == NULL) {
	    result = xmlXPathNewCString("");
	} else {
	    if (variable->tree) {
		xmlDocPtr container;
		xmlNodePtr oldInsert;
		xmlDocPtr  oldOutput;
		xsltStackElemPtr oldVar = ctxt->contextVariable;

		/*
		* Generate a result tree fragment.
		*/
		container = xsltCreateRVT(ctxt);
		if (container == NULL)
		    goto error;
		/*
		* NOTE: Local Result Tree Fragments of params/variables
		* are not registered globally anymore; the life-time
		* is not directly dependant of the param/variable itself.
		*
		* OLD: xsltRegisterTmpRVT(ctxt, container);
		*/
		/*
		* Attach the Result Tree Fragment to the variable;
		* when the variable is freed, it will also free
		* the Result Tree Fragment.
		*/
		variable->fragment = container;
                container->psvi = XSLT_RVT_LOCAL;

		oldOutput = ctxt->output;
		oldInsert = ctxt->insert;

		ctxt->output = container;
		ctxt->insert = (xmlNodePtr) container;
		ctxt->contextVariable = variable;
		/*
		* Process the sequence constructor (variable->tree).
		* The resulting tree will be held by @container.
		*/
		xsltApplyOneTemplate(ctxt, ctxt->node, variable->tree,
		    NULL, NULL);

		ctxt->contextVariable = oldVar;
		ctxt->insert = oldInsert;
		ctxt->output = oldOutput;

		result = xmlXPathNewValueTree((xmlNodePtr) container);
	    }
	    if (result == NULL) {
		result = xmlXPathNewCString("");
	    } else {
		/*
		* Freeing is not handled there anymore.
		* QUESTION TODO: What does the above comment mean?
		*/
	        result->boolval = 0;
	    }
#ifdef WITH_XSLT_DEBUG_VARIABLE
#ifdef LIBXML_DEBUG_ENABLED

	    if ((xsltGenericDebugContext == stdout) ||
		(xsltGenericDebugContext == stderr))
		xmlXPathDebugDumpObject((FILE *)xsltGenericDebugContext,
					result, 0);
#endif
#endif
	}
    }

error:
    ctxt->inst = oldInst;
    return(result);
}

/**
 * xsltEvalGlobalVariable:
 * @elem:  the variable or parameter
 * @ctxt:  the XSLT transformation context
 *
 * Evaluates a the value of a global xsl:variable or
 * xsl:param declaration.
 *
 * Returns the XPath Object value or NULL in case of error
 */
static xmlXPathObjectPtr
xsltEvalGlobalVariable(xsltStackElemPtr elem, xsltTransformContextPtr ctxt)
{
    xmlXPathObjectPtr result = NULL;
    xmlNodePtr oldInst;
    const xmlChar* oldVarName;

#ifdef XSLT_REFACTORED
    xsltStyleBasicItemVariablePtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((ctxt == NULL) || (elem == NULL))
	return(NULL);
    if (elem->computed)
	return(elem->value);


#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
	"Evaluating global variable %s\n", elem->name));
#endif

#ifdef WITH_DEBUGGER
    if ((ctxt->debugStatus != XSLT_DEBUG_NONE) &&
        elem->comp && elem->comp->inst)
        xslHandleDebugger(elem->comp->inst, NULL, NULL, ctxt);
#endif

    oldInst = ctxt->inst;
#ifdef XSLT_REFACTORED
    comp = (xsltStyleBasicItemVariablePtr) elem->comp;
#else
    comp = elem->comp;
#endif
    oldVarName = elem->name;
    elem->name = xsltComputingGlobalVarMarker;
    /*
    * OPTIMIZE TODO: We should consider instantiating global vars/params
    *  on-demand. The vars/params don't need to be evaluated if never
    *  called; and in the case of global params, if values for such params
    *  are provided by the user.
    */
    if (elem->select != NULL) {
	xmlXPathCompExprPtr xpExpr = NULL;
	xmlDocPtr oldXPDoc;
	xmlNodePtr oldXPContextNode;
	int oldXPProximityPosition, oldXPContextSize, oldXPNsNr;
	xmlNsPtr *oldXPNamespaces;
	xmlXPathContextPtr xpctxt = ctxt->xpathCtxt;

	if ((comp != NULL) && (comp->comp != NULL)) {
	    xpExpr = comp->comp;
	} else {
	    xpExpr = xmlXPathCtxtCompile(ctxt->xpathCtxt, elem->select);
	}
	if (xpExpr == NULL)
	    goto error;


	if (comp != NULL)
	    ctxt->inst = comp->inst;
	else
	    ctxt->inst = NULL;
	/*
	* SPEC XSLT 1.0:
	* "At top-level, the expression or template specifying the
	*  variable value is evaluated with the same context as that used
	*  to process the root node of the source document: the current
	*  node is the root node of the source document and the current
	*  node list is a list containing just the root node of the source
	*  document."
	*/
	/*
	* Save context states.
	*/
	oldXPDoc = xpctxt->doc;
	oldXPContextNode = xpctxt->node;
	oldXPProximityPosition = xpctxt->proximityPosition;
	oldXPContextSize = xpctxt->contextSize;
	oldXPNamespaces = xpctxt->namespaces;
	oldXPNsNr = xpctxt->nsNr;

	xpctxt->node = ctxt->initialContextNode;
	xpctxt->doc = ctxt->initialContextDoc;
	xpctxt->contextSize = 1;
	xpctxt->proximityPosition = 1;

	if (comp != NULL) {

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
	} else {
	    xpctxt->namespaces = NULL;
	    xpctxt->nsNr = 0;
	}

	result = xmlXPathCompiledEval(xpExpr, xpctxt);

	/*
	* Restore Context states.
	*/
	xpctxt->doc = oldXPDoc;
	xpctxt->node = oldXPContextNode;
	xpctxt->contextSize = oldXPContextSize;
	xpctxt->proximityPosition = oldXPProximityPosition;
	xpctxt->namespaces = oldXPNamespaces;
	xpctxt->nsNr = oldXPNsNr;

	if ((comp == NULL) || (comp->comp == NULL))
	    xmlXPathFreeCompExpr(xpExpr);
	if (result == NULL) {
	    if (comp == NULL)
		xsltTransformError(ctxt, NULL, NULL,
		    "Evaluating global variable %s failed\n", elem->name);
	    else
		xsltTransformError(ctxt, NULL, comp->inst,
		    "Evaluating global variable %s failed\n", elem->name);
	    ctxt->state = XSLT_STATE_STOPPED;
            goto error;
        }

        /*
         * Mark all RVTs that are referenced from result as part
         * of this variable so they won't be freed too early.
         */
        xsltFlagRVTs(ctxt, result, XSLT_RVT_GLOBAL);

#ifdef WITH_XSLT_DEBUG_VARIABLE
#ifdef LIBXML_DEBUG_ENABLED
	if ((xsltGenericDebugContext == stdout) ||
	    (xsltGenericDebugContext == stderr))
	    xmlXPathDebugDumpObject((FILE *)xsltGenericDebugContext,
				    result, 0);
#endif
#endif
    } else {
	if (elem->tree == NULL) {
	    result = xmlXPathNewCString("");
	} else {
	    xmlDocPtr container;
	    xmlNodePtr oldInsert;
	    xmlDocPtr  oldOutput, oldXPDoc;
	    /*
	    * Generate a result tree fragment.
	    */
	    container = xsltCreateRVT(ctxt);
	    if (container == NULL)
		goto error;
	    /*
	    * Let the lifetime of the tree fragment be handled by
	    * the Libxslt's garbage collector.
	    */
	    xsltRegisterPersistRVT(ctxt, container);

	    oldOutput = ctxt->output;
	    oldInsert = ctxt->insert;

	    oldXPDoc = ctxt->xpathCtxt->doc;

	    ctxt->output = container;
	    ctxt->insert = (xmlNodePtr) container;

	    ctxt->xpathCtxt->doc = ctxt->initialContextDoc;
	    /*
	    * Process the sequence constructor.
	    */
	    xsltApplyOneTemplate(ctxt, ctxt->node, elem->tree, NULL, NULL);

	    ctxt->xpathCtxt->doc = oldXPDoc;

	    ctxt->insert = oldInsert;
	    ctxt->output = oldOutput;

	    result = xmlXPathNewValueTree((xmlNodePtr) container);
	    if (result == NULL) {
		result = xmlXPathNewCString("");
	    } else {
	        result->boolval = 0; /* Freeing is not handled there anymore */
	    }
#ifdef WITH_XSLT_DEBUG_VARIABLE
#ifdef LIBXML_DEBUG_ENABLED
	    if ((xsltGenericDebugContext == stdout) ||
		(xsltGenericDebugContext == stderr))
		xmlXPathDebugDumpObject((FILE *)xsltGenericDebugContext,
					result, 0);
#endif
#endif
	}
    }

error:
    elem->name = oldVarName;
    ctxt->inst = oldInst;
    if (result != NULL) {
	elem->value = result;
	elem->computed = 1;
    }
    return(result);
}

static void
xsltEvalGlobalVariableWrapper(void *payload, void *data,
                              const xmlChar *name ATTRIBUTE_UNUSED) {
    xsltEvalGlobalVariable((xsltStackElemPtr) payload,
                           (xsltTransformContextPtr) data);
}

/**
 * xsltEvalGlobalVariables:
 * @ctxt:  the XSLT transformation context
 *
 * Evaluates all global variables and parameters of a stylesheet.
 * For internal use only. This is called at start of a transformation.
 *
 * Returns 0 in case of success, -1 in case of error
 */
int
xsltEvalGlobalVariables(xsltTransformContextPtr ctxt) {
    xsltStackElemPtr elem;
    xsltStylesheetPtr style;

    if ((ctxt == NULL) || (ctxt->document == NULL))
	return(-1);

#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
	"Registering global variables\n"));
#endif
    /*
     * Walk the list from the stylesheets and populate the hash table
     */
    style = ctxt->style;
    while (style != NULL) {
	elem = style->variables;

#ifdef WITH_XSLT_DEBUG_VARIABLE
	if ((style->doc != NULL) && (style->doc->URL != NULL)) {
	    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
			     "Registering global variables from %s\n",
		             style->doc->URL));
	}
#endif

	while (elem != NULL) {
	    xsltStackElemPtr def;

	    /*
	     * Global variables are stored in the variables pool.
	     */
	    def = (xsltStackElemPtr)
		    xmlHashLookup2(ctxt->globalVars,
		                 elem->name, elem->nameURI);
	    if (def == NULL) {

		def = xsltCopyStackElem(elem);
		xmlHashAddEntry2(ctxt->globalVars,
				 elem->name, elem->nameURI, def);
	    } else if ((elem->comp != NULL) &&
		       (elem->comp->type == XSLT_FUNC_VARIABLE)) {
		/*
		 * Redefinition of variables from a different stylesheet
		 * should not generate a message.
		 */
		if ((elem->comp->inst != NULL) &&
		    (def->comp != NULL) && (def->comp->inst != NULL) &&
		    (elem->comp->inst->doc == def->comp->inst->doc))
		{
		    xsltTransformError(ctxt, style, elem->comp->inst,
			"Global variable %s already defined\n", elem->name);
		    if (style != NULL) style->errors++;
		}
	    }
	    elem = elem->next;
	}

	style = xsltNextImport(style);
    }

    /*
     * This part does the actual evaluation
     */
    xmlHashScan(ctxt->globalVars, xsltEvalGlobalVariableWrapper, ctxt);

    return(0);
}

/**
 * xsltRegisterGlobalVariable:
 * @style:  the XSLT transformation context
 * @name:  the variable name
 * @ns_uri:  the variable namespace URI
 * @sel:  the expression which need to be evaluated to generate a value
 * @tree:  the subtree if sel is NULL
 * @comp:  the precompiled value
 * @value:  the string value if available
 *
 * Register a new variable value. If @value is NULL it unregisters
 * the variable
 *
 * Returns 0 in case of success, -1 in case of error
 */
static int
xsltRegisterGlobalVariable(xsltStylesheetPtr style, const xmlChar *name,
		     const xmlChar *ns_uri, const xmlChar *sel,
		     xmlNodePtr tree, xsltStylePreCompPtr comp,
		     const xmlChar *value) {
    xsltStackElemPtr elem, tmp;
    if (style == NULL)
	return(-1);
    if (name == NULL)
	return(-1);
    if (comp == NULL)
	return(-1);

#ifdef WITH_XSLT_DEBUG_VARIABLE
    if (comp->type == XSLT_FUNC_PARAM)
	xsltGenericDebug(xsltGenericDebugContext,
			 "Defining global param %s\n", name);
    else
	xsltGenericDebug(xsltGenericDebugContext,
			 "Defining global variable %s\n", name);
#endif

    elem = xsltNewStackElem(NULL);
    if (elem == NULL)
	return(-1);
    elem->comp = comp;
    elem->name = xmlDictLookup(style->dict, name, -1);
    elem->select = xmlDictLookup(style->dict, sel, -1);
    if (ns_uri)
	elem->nameURI = xmlDictLookup(style->dict, ns_uri, -1);
    elem->tree = tree;
    tmp = style->variables;
    if (tmp == NULL) {
	elem->next = NULL;
	style->variables = elem;
    } else {
	while (tmp != NULL) {
	    if ((elem->comp->type == XSLT_FUNC_VARIABLE) &&
		(tmp->comp->type == XSLT_FUNC_VARIABLE) &&
		(xmlStrEqual(elem->name, tmp->name)) &&
		((elem->nameURI == tmp->nameURI) ||
		 (xmlStrEqual(elem->nameURI, tmp->nameURI))))
	    {
		xsltTransformError(NULL, style, comp->inst,
		"redefinition of global variable %s\n", elem->name);
		style->errors++;
	    }
	    if (tmp->next == NULL)
	        break;
	    tmp = tmp->next;
	}
	elem->next = NULL;
	tmp->next = elem;
    }
    if (value != NULL) {
	elem->computed = 1;
	elem->value = xmlXPathNewString(value);
    }
    return(0);
}

/**
 * xsltProcessUserParamInternal
 *
 * @ctxt:  the XSLT transformation context
 * @name:  a null terminated parameter name
 * @value: a null terminated value (may be an XPath expression)
 * @eval:  0 to treat the value literally, else evaluate as XPath expression
 *
 * If @eval is 0 then @value is treated literally and is stored in the global
 * parameter/variable table without any change.
 *
 * Uf @eval is 1 then @value is treated as an XPath expression and is
 * evaluated.  In this case, if you want to pass a string which will be
 * interpreted literally then it must be enclosed in single or double quotes.
 * If the string contains single quotes (double quotes) then it cannot be
 * enclosed single quotes (double quotes).  If the string which you want to
 * be treated literally contains both single and double quotes (e.g. Meet
 * at Joe's for "Twelfth Night" at 7 o'clock) then there is no suitable
 * quoting character.  You cannot use &apos; or &quot; inside the string
 * because the replacement of character entities with their equivalents is
 * done at a different stage of processing.  The solution is to call
 * xsltQuoteUserParams or xsltQuoteOneUserParam.
 *
 * This needs to be done on parsed stylesheets before starting to apply
 * transformations.  Normally this will be called (directly or indirectly)
 * only from xsltEvalUserParams, xsltEvalOneUserParam, xsltQuoteUserParams,
 * or xsltQuoteOneUserParam.
 *
 * Returns 0 in case of success, -1 in case of error
 */

static
int
xsltProcessUserParamInternal(xsltTransformContextPtr ctxt,
		             const xmlChar * name,
			     const xmlChar * value,
			     int eval) {

    xsltStylesheetPtr style;
    const xmlChar *prefix;
    const xmlChar *href;
    xmlXPathCompExprPtr xpExpr;
    xmlXPathObjectPtr result;

    xsltStackElemPtr elem;
    int res;
    void *res_ptr;

    if (ctxt == NULL)
	return(-1);
    if (name == NULL)
	return(0);
    if (value == NULL)
	return(0);

    style = ctxt->style;

#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
	    "Evaluating user parameter %s=%s\n", name, value));
#endif

    /*
     * Name lookup
     */
    href = NULL;

    if (name[0] == '{') {
        int len = 0;

        while ((name[len] != 0) && (name[len] != '}')) len++;
        if (name[len] == 0) {
           xsltTransformError(ctxt, style, NULL,
           "user param : malformed parameter name : %s\n", name);
        } else {
           href = xmlDictLookup(ctxt->dict, &name[1], len-1);
           name = xmlDictLookup(ctxt->dict, &name[len + 1], -1);
       }
    }
    else {
        name = xsltSplitQName(ctxt->dict, name, &prefix);
        if (prefix != NULL) {
            xmlNsPtr ns;

            ns = xmlSearchNs(style->doc, xmlDocGetRootElement(style->doc),
                             prefix);
            if (ns == NULL) {
                xsltTransformError(ctxt, style, NULL,
                "user param : no namespace bound to prefix %s\n", prefix);
                href = NULL;
            } else {
                href = ns->href;
            }
        }
    }

    if (name == NULL)
	return (-1);

    res_ptr = xmlHashLookup2(ctxt->globalVars, name, href);
    if (res_ptr != 0) {
	xsltTransformError(ctxt, style, NULL,
	    "Global parameter %s already defined\n", name);
    }
    if (ctxt->globalVars == NULL)
	ctxt->globalVars = xmlHashCreate(20);

    /*
     * do not overwrite variables with parameters from the command line
     */
    while (style != NULL) {
        elem = ctxt->style->variables;
	while (elem != NULL) {
	    if ((elem->comp != NULL) &&
	        (elem->comp->type == XSLT_FUNC_VARIABLE) &&
		(xmlStrEqual(elem->name, name)) &&
		(xmlStrEqual(elem->nameURI, href))) {
		return(0);
	    }
            elem = elem->next;
	}
        style = xsltNextImport(style);
    }
    style = ctxt->style;
    elem = NULL;

    /*
     * Do the evaluation if @eval is non-zero.
     */

    result = NULL;
    if (eval != 0) {
        xpExpr = xmlXPathCtxtCompile(ctxt->xpathCtxt, value);
	if (xpExpr != NULL) {
	    xmlDocPtr oldXPDoc;
	    xmlNodePtr oldXPContextNode;
	    int oldXPProximityPosition, oldXPContextSize, oldXPNsNr;
	    xmlNsPtr *oldXPNamespaces;
	    xmlXPathContextPtr xpctxt = ctxt->xpathCtxt;

	    /*
	    * Save context states.
	    */
	    oldXPDoc = xpctxt->doc;
	    oldXPContextNode = xpctxt->node;
	    oldXPProximityPosition = xpctxt->proximityPosition;
	    oldXPContextSize = xpctxt->contextSize;
	    oldXPNamespaces = xpctxt->namespaces;
	    oldXPNsNr = xpctxt->nsNr;

	    /*
	    * SPEC XSLT 1.0:
	    * "At top-level, the expression or template specifying the
	    *  variable value is evaluated with the same context as that used
	    *  to process the root node of the source document: the current
	    *  node is the root node of the source document and the current
	    *  node list is a list containing just the root node of the source
	    *  document."
	    */
	    xpctxt->doc = ctxt->initialContextDoc;
	    xpctxt->node = ctxt->initialContextNode;
	    xpctxt->contextSize = 1;
	    xpctxt->proximityPosition = 1;
	    /*
	    * There is really no in scope namespace for parameters on the
	    * command line.
	    */
	    xpctxt->namespaces = NULL;
	    xpctxt->nsNr = 0;

	    result = xmlXPathCompiledEval(xpExpr, xpctxt);

	    /*
	    * Restore Context states.
	    */
	    xpctxt->doc = oldXPDoc;
	    xpctxt->node = oldXPContextNode;
	    xpctxt->contextSize = oldXPContextSize;
	    xpctxt->proximityPosition = oldXPProximityPosition;
	    xpctxt->namespaces = oldXPNamespaces;
	    xpctxt->nsNr = oldXPNsNr;

	    xmlXPathFreeCompExpr(xpExpr);
	}
	if (result == NULL) {
	    xsltTransformError(ctxt, style, NULL,
		"Evaluating user parameter %s failed\n", name);
	    ctxt->state = XSLT_STATE_STOPPED;
	    return(-1);
	}
    }

    /*
     * If @eval is 0 then @value is to be taken literally and result is NULL
     *
     * If @eval is not 0, then @value is an XPath expression and has been
     * successfully evaluated and result contains the resulting value and
     * is not NULL.
     *
     * Now create an xsltStackElemPtr for insertion into the context's
     * global variable/parameter hash table.
     */

#ifdef WITH_XSLT_DEBUG_VARIABLE
#ifdef LIBXML_DEBUG_ENABLED
    if ((xsltGenericDebugContext == stdout) ||
        (xsltGenericDebugContext == stderr))
	    xmlXPathDebugDumpObject((FILE *)xsltGenericDebugContext,
				    result, 0);
#endif
#endif

    elem = xsltNewStackElem(NULL);
    if (elem != NULL) {
	elem->name = name;
	elem->select = xmlDictLookup(ctxt->dict, value, -1);
	if (href != NULL)
	    elem->nameURI = xmlDictLookup(ctxt->dict, href, -1);
	elem->tree = NULL;
	elem->computed = 1;
	if (eval == 0) {
	    elem->value = xmlXPathNewString(value);
	}
	else {
	    elem->value = result;
	}
    }

    /*
     * Global parameters are stored in the XPath context variables pool.
     */

    res = xmlHashAddEntry2(ctxt->globalVars, name, href, elem);
    if (res != 0) {
	xsltFreeStackElem(elem);
	xsltTransformError(ctxt, style, NULL,
	    "Global parameter %s already defined\n", name);
    }
    return(0);
}

/**
 * xsltEvalUserParams:
 *
 * @ctxt:  the XSLT transformation context
 * @params:  a NULL terminated array of parameters name/value tuples
 *
 * Evaluate the global variables of a stylesheet. This needs to be
 * done on parsed stylesheets before starting to apply transformations.
 * Each of the parameters is evaluated as an XPath expression and stored
 * in the global variables/parameter hash table.  If you want your
 * parameter used literally, use xsltQuoteUserParams.
 *
 * Returns 0 in case of success, -1 in case of error
 */

int
xsltEvalUserParams(xsltTransformContextPtr ctxt, const char **params) {
    int indx = 0;
    const xmlChar *name;
    const xmlChar *value;

    if (params == NULL)
	return(0);
    while (params[indx] != NULL) {
	name = (const xmlChar *) params[indx++];
	value = (const xmlChar *) params[indx++];
	if (xsltEvalOneUserParam(ctxt, name, value) != 0)
	    return(-1);
    }
    return 0;
}

/**
 * xsltQuoteUserParams:
 *
 * @ctxt:  the XSLT transformation context
 * @params:  a NULL terminated arry of parameters names/values tuples
 *
 * Similar to xsltEvalUserParams, but the values are treated literally and
 * are * *not* evaluated as XPath expressions. This should be done on parsed
 * stylesheets before starting to apply transformations.
 *
 * Returns 0 in case of success, -1 in case of error.
 */

int
xsltQuoteUserParams(xsltTransformContextPtr ctxt, const char **params) {
    int indx = 0;
    const xmlChar *name;
    const xmlChar *value;

    if (params == NULL)
	return(0);
    while (params[indx] != NULL) {
	name = (const xmlChar *) params[indx++];
	value = (const xmlChar *) params[indx++];
	if (xsltQuoteOneUserParam(ctxt, name, value) != 0)
	    return(-1);
    }
    return 0;
}

/**
 * xsltEvalOneUserParam:
 * @ctxt:  the XSLT transformation context
 * @name:  a null terminated string giving the name of the parameter
 * @value:  a null terminated string giving the XPath expression to be evaluated
 *
 * This is normally called from xsltEvalUserParams to process a single
 * parameter from a list of parameters.  The @value is evaluated as an
 * XPath expression and the result is stored in the context's global
 * variable/parameter hash table.
 *
 * To have a parameter treated literally (not as an XPath expression)
 * use xsltQuoteUserParams (or xsltQuoteOneUserParam).  For more
 * details see description of xsltProcessOneUserParamInternal.
 *
 * Returns 0 in case of success, -1 in case of error.
 */

int
xsltEvalOneUserParam(xsltTransformContextPtr ctxt,
		     const xmlChar * name,
		     const xmlChar * value) {
    return xsltProcessUserParamInternal(ctxt, name, value,
		                        1 /* xpath eval ? */);
}

/**
 * xsltQuoteOneUserParam:
 * @ctxt:  the XSLT transformation context
 * @name:  a null terminated string giving the name of the parameter
 * @value:  a null terminated string giving the parameter value
 *
 * This is normally called from xsltQuoteUserParams to process a single
 * parameter from a list of parameters.  The @value is stored in the
 * context's global variable/parameter hash table.
 *
 * Returns 0 in case of success, -1 in case of error.
 */

int
xsltQuoteOneUserParam(xsltTransformContextPtr ctxt,
			 const xmlChar * name,
			 const xmlChar * value) {
    return xsltProcessUserParamInternal(ctxt, name, value,
					0 /* xpath eval ? */);
}

/**
 * xsltBuildVariable:
 * @ctxt:  the XSLT transformation context
 * @comp:  the precompiled form
 * @tree:  the tree if select is NULL
 *
 * Computes a new variable value.
 *
 * Returns the xsltStackElemPtr or NULL in case of error
 */
static xsltStackElemPtr
xsltBuildVariable(xsltTransformContextPtr ctxt,
		  xsltStylePreCompPtr castedComp,
		  xmlNodePtr tree)
{
#ifdef XSLT_REFACTORED
    xsltStyleBasicItemVariablePtr comp =
	(xsltStyleBasicItemVariablePtr) castedComp;
#else
    xsltStylePreCompPtr comp = castedComp;
#endif
    xsltStackElemPtr elem;

#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
		     "Building variable %s", comp->name));
    if (comp->select != NULL)
	XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
			 " select %s", comp->select));
    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext, "\n"));
#endif

    elem = xsltNewStackElem(ctxt);
    if (elem == NULL)
	return(NULL);
    elem->comp = (xsltStylePreCompPtr) comp;
    elem->name = comp->name;
    elem->select = comp->select;
    elem->nameURI = comp->ns;
    elem->tree = tree;
    elem->value = xsltEvalVariable(ctxt, elem,
	(xsltStylePreCompPtr) comp);
    elem->computed = 1;
    return(elem);
}

/**
 * xsltRegisterVariable:
 * @ctxt:  the XSLT transformation context
 * @comp: the compiled XSLT-variable (or param) instruction
 * @tree:  the tree if select is NULL
 * @isParam:  indicates if this is a parameter
 *
 * Computes and registers a new variable.
 *
 * Returns 0 in case of success, -1 in case of error
 */
static int
xsltRegisterVariable(xsltTransformContextPtr ctxt,
		     xsltStylePreCompPtr castedComp,
		     xmlNodePtr tree, int isParam)
{
#ifdef XSLT_REFACTORED
    xsltStyleBasicItemVariablePtr comp =
	(xsltStyleBasicItemVariablePtr) castedComp;
#else
    xsltStylePreCompPtr comp = castedComp;
    int present;
#endif
    xsltStackElemPtr variable;

#ifdef XSLT_REFACTORED
    /*
    * REFACTORED NOTE: Redefinitions of vars/params are checked
    *  at compilation time in the refactored code.
    * xsl:with-param parameters are checked in xsltApplyXSLTTemplate().
    */
#else
    present = xsltCheckStackElem(ctxt, comp->name, comp->ns);
    if (isParam == 0) {
	if ((present != 0) && (present != 3)) {
	    /* TODO: report QName. */
	    xsltTransformError(ctxt, NULL, comp->inst,
		"XSLT-variable: Redefinition of variable '%s'.\n", comp->name);
	    return(0);
	}
    } else if (present != 0) {
	if ((present == 1) || (present == 2)) {
	    /* TODO: report QName. */
	    xsltTransformError(ctxt, NULL, comp->inst,
		"XSLT-param: Redefinition of parameter '%s'.\n", comp->name);
	    return(0);
	}
#ifdef WITH_XSLT_DEBUG_VARIABLE
	XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
		 "param %s defined by caller\n", comp->name));
#endif
	return(0);
    }
#endif /* else of XSLT_REFACTORED */

    variable = xsltBuildVariable(ctxt, (xsltStylePreCompPtr) comp, tree);
    xsltAddStackElem(ctxt, variable);
    return(0);
}

/**
 * xsltGlobalVariableLookup:
 * @ctxt:  the XSLT transformation context
 * @name:  the variable name
 * @ns_uri:  the variable namespace URI
 *
 * Search in the Variable array of the context for the given
 * variable value.
 *
 * Returns the value or NULL if not found
 */
static xmlXPathObjectPtr
xsltGlobalVariableLookup(xsltTransformContextPtr ctxt, const xmlChar *name,
		         const xmlChar *ns_uri) {
    xsltStackElemPtr elem;
    xmlXPathObjectPtr ret = NULL;

    /*
     * Lookup the global variables in XPath global variable hash table
     */
    if ((ctxt->xpathCtxt == NULL) || (ctxt->globalVars == NULL))
	return(NULL);
    elem = (xsltStackElemPtr)
	    xmlHashLookup2(ctxt->globalVars, name, ns_uri);
    if (elem == NULL) {
#ifdef WITH_XSLT_DEBUG_VARIABLE
	XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
			 "global variable not found %s\n", name));
#endif
	return(NULL);
    }
    /*
    * URGENT TODO: Move the detection of recursive definitions
    * to compile-time.
    */
    if (elem->computed == 0) {
	if (elem->name == xsltComputingGlobalVarMarker) {
	    xsltTransformError(ctxt, NULL, elem->comp->inst,
		"Recursive definition of %s\n", name);
	    return(NULL);
	}
	ret = xsltEvalGlobalVariable(elem, ctxt);
    } else
	ret = elem->value;
    return(xmlXPathObjectCopy(ret));
}

/**
 * xsltVariableLookup:
 * @ctxt:  the XSLT transformation context
 * @name:  the variable name
 * @ns_uri:  the variable namespace URI
 *
 * Search in the Variable array of the context for the given
 * variable value.
 *
 * Returns the value or NULL if not found
 */
xmlXPathObjectPtr
xsltVariableLookup(xsltTransformContextPtr ctxt, const xmlChar *name,
		   const xmlChar *ns_uri) {
    xsltStackElemPtr elem;

    if (ctxt == NULL)
	return(NULL);

    elem = xsltStackLookup(ctxt, name, ns_uri);
    if (elem == NULL) {
	return(xsltGlobalVariableLookup(ctxt, name, ns_uri));
    }
    if (elem->computed == 0) {
#ifdef WITH_XSLT_DEBUG_VARIABLE
	XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
		         "uncomputed variable %s\n", name));
#endif
        elem->value = xsltEvalVariable(ctxt, elem, NULL);
	elem->computed = 1;
    }
    if (elem->value != NULL)
	return(xmlXPathObjectCopy(elem->value));
#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
		     "variable not found %s\n", name));
#endif
    return(NULL);
}

/**
 * xsltParseStylesheetCallerParam:
 * @ctxt:  the XSLT transformation context
 * @inst:  the xsl:with-param instruction element
 *
 * Processes an xsl:with-param instruction at transformation time.
 * The value is computed, but not recorded.
 * NOTE that this is also called with an *xsl:param* element
 * from exsltFuncFunctionFunction().
 *
 * Returns the new xsltStackElemPtr or NULL
 */

xsltStackElemPtr
xsltParseStylesheetCallerParam(xsltTransformContextPtr ctxt, xmlNodePtr inst)
{
#ifdef XSLT_REFACTORED
    xsltStyleBasicItemVariablePtr comp;
#else
    xsltStylePreCompPtr comp;
#endif
    xmlNodePtr tree = NULL; /* The first child node of the instruction or
                               the instruction itself. */
    xsltStackElemPtr param = NULL;

    if ((ctxt == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return(NULL);

#ifdef XSLT_REFACTORED
    comp = (xsltStyleBasicItemVariablePtr) inst->psvi;
#else
    comp = (xsltStylePreCompPtr) inst->psvi;
#endif

    if (comp == NULL) {
        xsltTransformError(ctxt, NULL, inst,
	    "Internal error in xsltParseStylesheetCallerParam(): "
	    "The XSLT 'with-param' instruction was not compiled.\n");
        return(NULL);
    }
    if (comp->name == NULL) {
	xsltTransformError(ctxt, NULL, inst,
	    "Internal error in xsltParseStylesheetCallerParam(): "
	    "XSLT 'with-param': The attribute 'name' was not compiled.\n");
	return(NULL);
    }

#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
	    "Handling xsl:with-param %s\n", comp->name));
#endif

    if (comp->select == NULL) {
	tree = inst->children;
    } else {
#ifdef WITH_XSLT_DEBUG_VARIABLE
	XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
	    "        select %s\n", comp->select));
#endif
	tree = inst;
    }

    param = xsltBuildVariable(ctxt, (xsltStylePreCompPtr) comp, tree);

    return(param);
}

/**
 * xsltParseGlobalVariable:
 * @style:  the XSLT stylesheet
 * @cur:  the "variable" element
 *
 * Parses a global XSLT 'variable' declaration at compilation time
 * and registers it
 */
void
xsltParseGlobalVariable(xsltStylesheetPtr style, xmlNodePtr cur)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemVariablePtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((cur == NULL) || (style == NULL) || (cur->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    /*
    * Note that xsltStylePreCompute() will be called from
    * xslt.c only.
    */
    comp = (xsltStyleItemVariablePtr) cur->psvi;
#else
    xsltStylePreCompute(style, cur);
    comp = (xsltStylePreCompPtr) cur->psvi;
#endif
    if (comp == NULL) {
	xsltTransformError(NULL, style, cur,
	     "xsl:variable : compilation failed\n");
	return;
    }

    if (comp->name == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsl:variable : missing name attribute\n");
	return;
    }

    /*
    * Parse the content (a sequence constructor) of xsl:variable.
    */
    if (cur->children != NULL) {
#ifdef XSLT_REFACTORED
        xsltParseSequenceConstructor(XSLT_CCTXT(style), cur->children);
#else
        xsltParseTemplateContent(style, cur);
#endif
    }
#ifdef WITH_XSLT_DEBUG_VARIABLE
    xsltGenericDebug(xsltGenericDebugContext,
	"Registering global variable %s\n", comp->name);
#endif

    xsltRegisterGlobalVariable(style, comp->name, comp->ns,
	comp->select, cur->children, (xsltStylePreCompPtr) comp,
	NULL);
}

/**
 * xsltParseGlobalParam:
 * @style:  the XSLT stylesheet
 * @cur:  the "param" element
 *
 * parse an XSLT transformation param declaration and record
 * its value.
 */

void
xsltParseGlobalParam(xsltStylesheetPtr style, xmlNodePtr cur) {
#ifdef XSLT_REFACTORED
    xsltStyleItemParamPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((cur == NULL) || (style == NULL) || (cur->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    /*
    * Note that xsltStylePreCompute() will be called from
    * xslt.c only.
    */
    comp = (xsltStyleItemParamPtr) cur->psvi;
#else
    xsltStylePreCompute(style, cur);
    comp = (xsltStylePreCompPtr) cur->psvi;
#endif
    if (comp == NULL) {
	xsltTransformError(NULL, style, cur,
	     "xsl:param : compilation failed\n");
	return;
    }

    if (comp->name == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsl:param : missing name attribute\n");
	return;
    }

    /*
    * Parse the content (a sequence constructor) of xsl:param.
    */
    if (cur->children != NULL) {
#ifdef XSLT_REFACTORED
        xsltParseSequenceConstructor(XSLT_CCTXT(style), cur->children);
#else
        xsltParseTemplateContent(style, cur);
#endif
    }

#ifdef WITH_XSLT_DEBUG_VARIABLE
    xsltGenericDebug(xsltGenericDebugContext,
	"Registering global param %s\n", comp->name);
#endif

    xsltRegisterGlobalVariable(style, comp->name, comp->ns,
	comp->select, cur->children, (xsltStylePreCompPtr) comp,
	NULL);
}

/**
 * xsltParseStylesheetVariable:
 * @ctxt:  the XSLT transformation context
 * @inst:  the xsl:variable instruction element
 *
 * Registers a local XSLT 'variable' instruction at transformation time
 * and evaluates its value.
 */
void
xsltParseStylesheetVariable(xsltTransformContextPtr ctxt, xmlNodePtr inst)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemVariablePtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((inst == NULL) || (ctxt == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

    comp = inst->psvi;
    if (comp == NULL) {
        xsltTransformError(ctxt, NULL, inst,
	    "Internal error in xsltParseStylesheetVariable(): "
	    "The XSLT 'variable' instruction was not compiled.\n");
        return;
    }
    if (comp->name == NULL) {
	xsltTransformError(ctxt, NULL, inst,
	    "Internal error in xsltParseStylesheetVariable(): "
	    "The attribute 'name' was not compiled.\n");
	return;
    }

#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
	"Registering variable '%s'\n", comp->name));
#endif

    xsltRegisterVariable(ctxt, (xsltStylePreCompPtr) comp, inst->children, 0);
}

/**
 * xsltParseStylesheetParam:
 * @ctxt:  the XSLT transformation context
 * @cur:  the XSLT 'param' element
 *
 * Registers a local XSLT 'param' declaration at transformation time and
 * evaluates its value.
 */
void
xsltParseStylesheetParam(xsltTransformContextPtr ctxt, xmlNodePtr cur)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemParamPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((cur == NULL) || (ctxt == NULL) || (cur->type != XML_ELEMENT_NODE))
	return;

    comp = cur->psvi;
    if ((comp == NULL) || (comp->name == NULL)) {
	xsltTransformError(ctxt, NULL, cur,
	    "Internal error in xsltParseStylesheetParam(): "
	    "The XSLT 'param' declaration was not compiled correctly.\n");
	return;
    }

#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(ctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
	"Registering param %s\n", comp->name));
#endif

    xsltRegisterVariable(ctxt, (xsltStylePreCompPtr) comp, cur->children, 1);
}

/**
 * xsltFreeGlobalVariables:
 * @ctxt:  the XSLT transformation context
 *
 * Free up the data associated to the global variables
 * its value.
 */

void
xsltFreeGlobalVariables(xsltTransformContextPtr ctxt) {
    xmlHashFree(ctxt->globalVars, xsltFreeStackElemEntry);
}

/**
 * xsltXPathVariableLookup:
 * @ctxt:  a void * but the the XSLT transformation context actually
 * @name:  the variable name
 * @ns_uri:  the variable namespace URI
 *
 * This is the entry point when a varibale is needed by the XPath
 * interpretor.
 *
 * Returns the value or NULL if not found
 */
xmlXPathObjectPtr
xsltXPathVariableLookup(void *ctxt, const xmlChar *name,
	                const xmlChar *ns_uri) {
    xsltTransformContextPtr tctxt;
    xmlXPathObjectPtr valueObj = NULL;

    if ((ctxt == NULL) || (name == NULL))
	return(NULL);

#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(((xsltTransformContextPtr)ctxt),XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
	    "Lookup variable '%s'\n", name));
#endif

    tctxt = (xsltTransformContextPtr) ctxt;
    /*
    * Local variables/params ---------------------------------------------
    *
    * Do the lookup from the top of the stack, but
    * don't use params being computed in a call-param
    * First lookup expects the variable name and URI to
    * come from the disctionnary and hence pointer comparison.
    */
    if (tctxt->varsNr != 0) {
	int i;
	xsltStackElemPtr variable = NULL, cur;

	for (i = tctxt->varsNr; i > tctxt->varsBase; i--) {
	    cur = tctxt->varsTab[i-1];
	    if ((cur->name == name) && (cur->nameURI == ns_uri)) {
#if 0
		stack_addr++;
#endif
		variable = cur;
		goto local_variable_found;
	    }
	    cur = cur->next;
	}
	/*
	* Redo the lookup with interned strings to avoid string comparison.
	*
	* OPTIMIZE TODO: The problem here is, that if we request a
	*  global variable, then this will be also executed.
	*/
	{
	    const xmlChar *tmpName = name, *tmpNsName = ns_uri;

	    name = xmlDictLookup(tctxt->dict, name, -1);
	    if (ns_uri)
		ns_uri = xmlDictLookup(tctxt->dict, ns_uri, -1);
	    if ((tmpName != name) || (tmpNsName != ns_uri)) {
		for (i = tctxt->varsNr; i > tctxt->varsBase; i--) {
		    cur = tctxt->varsTab[i-1];
		    if ((cur->name == name) && (cur->nameURI == ns_uri)) {
#if 0
			stack_cmp++;
#endif
			variable = cur;
			goto local_variable_found;
		    }
		}
	    }
	}

local_variable_found:

	if (variable) {
	    if (variable->computed == 0) {

#ifdef WITH_XSLT_DEBUG_VARIABLE
		XSLT_TRACE(tctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
		    "uncomputed variable '%s'\n", name));
#endif
		variable->value = xsltEvalVariable(tctxt, variable, NULL);
		variable->computed = 1;
	    }
	    if (variable->value != NULL) {
		valueObj = xmlXPathObjectCopy(variable->value);
	    }
	    return(valueObj);
	}
    }
    /*
    * Global variables/params --------------------------------------------
    */
    if (tctxt->globalVars) {
	valueObj = xsltGlobalVariableLookup(tctxt, name, ns_uri);
    }

    if (valueObj == NULL) {

#ifdef WITH_XSLT_DEBUG_VARIABLE
    XSLT_TRACE(tctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
		     "variable not found '%s'\n", name));
#endif

	if (ns_uri) {
	    xsltTransformError(tctxt, NULL, tctxt->inst,
		"Variable '{%s}%s' has not been declared.\n", ns_uri, name);
	} else {
	    xsltTransformError(tctxt, NULL, tctxt->inst,
		"Variable '%s' has not been declared.\n", name);
	}
    } else {

#ifdef WITH_XSLT_DEBUG_VARIABLE
	XSLT_TRACE(tctxt,XSLT_TRACE_VARIABLES,xsltGenericDebug(xsltGenericDebugContext,
	    "found variable '%s'\n", name));
#endif
    }

    return(valueObj);
}


