/*
 * attributes.c: Implementation of the XSLT attributes handling
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
#include <libxml/hash.h>
#include <libxml/xmlerror.h>
#include <libxml/uri.h>
#include <libxml/parserInternals.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "attributes.h"
#include "namespaces.h"
#include "templates.h"
#include "imports.h"
#include "transform.h"
#include "preproc.h"

#define WITH_XSLT_DEBUG_ATTRIBUTES
#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_ATTRIBUTES
#endif

/*
 * Useful macros
 */
#ifdef IS_BLANK
#undef IS_BLANK
#endif

#define IS_BLANK(c) (((c) == 0x20) || ((c) == 0x09) || ((c) == 0xA) ||	\
                     ((c) == 0x0D))

#define IS_BLANK_NODE(n)						\
    (((n)->type == XML_TEXT_NODE) && (xsltIsBlank((n)->content)))

#define ATTRSET_UNRESOLVED 0
#define ATTRSET_RESOLVING  1
#define ATTRSET_RESOLVED   2


/*
 * The in-memory structure corresponding to an XSLT Attribute in
 * an attribute set
 */


typedef struct _xsltAttrElem xsltAttrElem;
typedef xsltAttrElem *xsltAttrElemPtr;
struct _xsltAttrElem {
    struct _xsltAttrElem *next;/* chained list */
    xmlNodePtr attr;	/* the xsl:attribute definition */
};

typedef struct _xsltUseAttrSet xsltUseAttrSet;
typedef xsltUseAttrSet *xsltUseAttrSetPtr;
struct _xsltUseAttrSet {
    struct _xsltUseAttrSet *next; /* chained list */
    const xmlChar *ncname;
    const xmlChar *ns;
};

typedef struct _xsltAttrSet xsltAttrSet;
typedef xsltAttrSet *xsltAttrSetPtr;
struct _xsltAttrSet {
    int state;
    xsltAttrElemPtr attrs; /* list head */
    xsltUseAttrSetPtr useAttrSets; /* list head */
};

typedef struct _xsltAttrSetContext xsltAttrSetContext;
typedef xsltAttrSetContext *xsltAttrSetContextPtr;
struct _xsltAttrSetContext {
    xsltStylesheetPtr topStyle;
    xsltStylesheetPtr style;
    int error;
};

static void
xsltResolveAttrSet(xsltAttrSetPtr set, xsltStylesheetPtr topStyle,
                   xsltStylesheetPtr style, const xmlChar *name,
                   const xmlChar *ns, int depth);

/************************************************************************
 *									*
 *			XSLT Attribute handling				*
 *									*
 ************************************************************************/

/**
 * xsltNewAttrElem:
 * @attr:  the new xsl:attribute node
 *
 * Create a new XSLT AttrElem
 *
 * Returns the newly allocated xsltAttrElemPtr or NULL in case of error
 */
static xsltAttrElemPtr
xsltNewAttrElem(xmlNodePtr attr) {
    xsltAttrElemPtr cur;

    cur = (xsltAttrElemPtr) xmlMalloc(sizeof(xsltAttrElem));
    if (cur == NULL) {
        xsltGenericError(xsltGenericErrorContext,
		"xsltNewAttrElem : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltAttrElem));
    cur->attr = attr;
    return(cur);
}

/**
 * xsltFreeAttrElem:
 * @attr:  an XSLT AttrElem
 *
 * Free up the memory allocated by @attr
 */
static void
xsltFreeAttrElem(xsltAttrElemPtr attr) {
    xmlFree(attr);
}

/**
 * xsltFreeAttrElemList:
 * @list:  an XSLT AttrElem list
 *
 * Free up the memory allocated by @list
 */
static void
xsltFreeAttrElemList(xsltAttrElemPtr list) {
    xsltAttrElemPtr next;

    while (list != NULL) {
	next = list->next;
	xsltFreeAttrElem(list);
	list = next;
    }
}

/**
 * xsltAddAttrElemList:
 * @list:  an XSLT AttrElem list
 * @attr:  the new xsl:attribute node
 *
 * Add the new attribute to the list.
 *
 * Returns the new list pointer
 */
static xsltAttrElemPtr
xsltAddAttrElemList(xsltAttrElemPtr list, xmlNodePtr attr) {
    xsltAttrElemPtr next, cur;

    if (attr == NULL)
	return(list);
    if (list == NULL)
	return(xsltNewAttrElem(attr));
    cur = list;
    while (cur != NULL) {
	next = cur->next;
	if (next == NULL) {
	    cur->next = xsltNewAttrElem(attr);
	    return(list);
	}
	cur = next;
    }
    return(list);
}

/**
 * xsltNewUseAttrSet:
 * @ncname:  local name
 * @ns:  namespace URI
 *
 * Create a new XSLT UseAttrSet
 *
 * Returns the newly allocated xsltUseAttrSetPtr or NULL in case of error.
 */
static xsltUseAttrSetPtr
xsltNewUseAttrSet(const xmlChar *ncname, const xmlChar *ns) {
    xsltUseAttrSetPtr cur;

    cur = (xsltUseAttrSetPtr) xmlMalloc(sizeof(xsltUseAttrSet));
    if (cur == NULL) {
        xsltGenericError(xsltGenericErrorContext,
		"xsltNewUseAttrSet : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltUseAttrSet));
    cur->ncname = ncname;
    cur->ns = ns;
    return(cur);
}

/**
 * xsltFreeUseAttrSet:
 * @use:  an XSLT UseAttrSet
 *
 * Free up the memory allocated by @use
 */
static void
xsltFreeUseAttrSet(xsltUseAttrSetPtr use) {
    xmlFree(use);
}

/**
 * xsltFreeUseAttrSetList:
 * @list:  an XSLT UseAttrSet list
 *
 * Free up the memory allocated by @list
 */
static void
xsltFreeUseAttrSetList(xsltUseAttrSetPtr list) {
    xsltUseAttrSetPtr next;

    while (list != NULL) {
	next = list->next;
	xsltFreeUseAttrSet(list);
	list = next;
    }
}

/**
 * xsltAddUseAttrSetList:
 * @list:  a xsltUseAttrSet list
 * @ncname:  local name
 * @ns:  namespace URI
 *
 * Add the use-attribute-set name to the list.
 *
 * Returns the new list pointer.
 */
static xsltUseAttrSetPtr
xsltAddUseAttrSetList(xsltUseAttrSetPtr list, const xmlChar *ncname,
                      const xmlChar *ns) {
    xsltUseAttrSetPtr next, cur;

    if (ncname == NULL)
        return(list);
    if (list == NULL)
	return(xsltNewUseAttrSet(ncname, ns));
    cur = list;
    while (cur != NULL) {
        if ((cur->ncname == ncname) && (cur->ns == ns))
            return(list);
	next = cur->next;
	if (next == NULL) {
	    cur->next = xsltNewUseAttrSet(ncname, ns);
	    return(list);
	}
	cur = next;
    }
    return(list);
}

/**
 * xsltNewAttrSet:
 *
 * Create a new attribute set.
 *
 * Returns the newly allocated xsltAttrSetPtr or NULL in case of error.
 */
static xsltAttrSetPtr
xsltNewAttrSet(void) {
    xsltAttrSetPtr cur;

    cur = (xsltAttrSetPtr) xmlMalloc(sizeof(xsltAttrSet));
    if (cur == NULL) {
        xsltGenericError(xsltGenericErrorContext,
		"xsltNewAttrSet : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltAttrSet));
    return(cur);
}

/**
 * xsltFreeAttrSet:
 * @set:  an attribute set
 *
 * Free memory allocated by @set
 */
static void
xsltFreeAttrSet(xsltAttrSetPtr set) {
    if (set == NULL)
        return;

    xsltFreeAttrElemList(set->attrs);
    xsltFreeUseAttrSetList(set->useAttrSets);
    xmlFree(set);
}

/**
 * xsltMergeAttrSets:
 * @set:  an attribute set
 * @other:  another attribute set
 *
 * Add all the attributes from @other to @set,
 * but drop redefinition of existing values.
 */
static void
xsltMergeAttrSets(xsltAttrSetPtr set, xsltAttrSetPtr other) {
    xsltAttrElemPtr cur;
    xsltAttrElemPtr old = other->attrs;
    int add;

    while (old != NULL) {
	/*
	 * Check that the attribute is not yet in the list
	 */
	cur = set->attrs;
	add = 1;
	while (cur != NULL) {
            xsltStylePreCompPtr curComp = cur->attr->psvi;
            xsltStylePreCompPtr oldComp = old->attr->psvi;

            if ((curComp->name == oldComp->name) &&
                (curComp->ns == oldComp->ns)) {
                add = 0;
                break;
            }
	    if (cur->next == NULL)
		break;
            cur = cur->next;
	}

	if (add == 1) {
	    if (cur == NULL) {
		set->attrs = xsltNewAttrElem(old->attr);
	    } else if (add) {
		cur->next = xsltNewAttrElem(old->attr);
	    }
	}

	old = old->next;
    }
}

/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/

/**
 * xsltParseStylesheetAttributeSet:
 * @style:  the XSLT stylesheet
 * @cur:  the "attribute-set" element
 *
 * parse an XSLT stylesheet attribute-set element
 */

void
xsltParseStylesheetAttributeSet(xsltStylesheetPtr style, xmlNodePtr cur) {
    const xmlChar *ncname;
    const xmlChar *prefix;
    const xmlChar *nsUri = NULL;
    xmlChar *value;
    xmlNodePtr child;
    xsltAttrSetPtr set;

    if ((cur == NULL) || (style == NULL) || (cur->type != XML_ELEMENT_NODE))
	return;

    value = xmlGetNsProp(cur, (const xmlChar *)"name", NULL);
    if ((value == NULL) || (*value == 0)) {
	xsltGenericError(xsltGenericErrorContext,
	     "xsl:attribute-set : name is missing\n");
        if (value)
	    xmlFree(value);
	return;
    }

    if (xmlValidateQName(value, 0)) {
        xsltTransformError(NULL, style, cur,
            "xsl:attribute-set : The name '%s' is not a valid QName.\n",
            value);
        style->errors++;
        xmlFree(value);
        return;
    }

    ncname = xsltSplitQName(style->dict, value, &prefix);
    xmlFree(value);
    value = NULL;
    if (prefix != NULL) {
        xmlNsPtr ns = xmlSearchNs(style->doc, cur, prefix);
        if (ns == NULL) {
            xsltTransformError(NULL, style, cur,
                "xsl:attribute-set : No namespace found for QName '%s:%s'\n",
                prefix, ncname);
            style->errors++;
            return;
        }
        nsUri = ns->href;
    }

    if (style->attributeSets == NULL) {
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
	xsltGenericDebug(xsltGenericDebugContext,
	    "creating attribute set table\n");
#endif
	style->attributeSets = xmlHashCreate(10);
    }
    if (style->attributeSets == NULL)
	return;

    set = xmlHashLookup2(style->attributeSets, ncname, nsUri);
    if (set == NULL) {
        set = xsltNewAttrSet();
        if ((set == NULL) ||
            (xmlHashAddEntry2(style->attributeSets, ncname, nsUri, set) < 0)) {
	    xsltGenericError(xsltGenericErrorContext, "memory error\n");
            xsltFreeAttrSet(set);
            return;
        }
    }

    /*
    * Parse the content. Only xsl:attribute elements are allowed.
    */
    child = cur->children;
    while (child != NULL) {
	/*
	* Report invalid nodes.
	*/
	if ((child->type != XML_ELEMENT_NODE) ||
	    (child->ns == NULL) ||
	    (! IS_XSLT_ELEM(child)))
	{
	    if (child->type == XML_ELEMENT_NODE)
		xsltTransformError(NULL, style, child,
			"xsl:attribute-set : unexpected child %s\n",
		                 child->name);
	    else
		xsltTransformError(NULL, style, child,
			"xsl:attribute-set : child of unexpected type\n");
	} else if (!IS_XSLT_NAME(child, "attribute")) {
	    xsltTransformError(NULL, style, child,
		"xsl:attribute-set : unexpected child xsl:%s\n",
		child->name);
	} else {
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
	    xsltGenericDebug(xsltGenericDebugContext,
		"add attribute to list %s\n", ncname);
#endif
            xsltStylePreCompute(style, child);
            if (child->children != NULL) {
#ifdef XSLT_REFACTORED
                xsltParseSequenceConstructor(XSLT_CCTXT(style),
                                             child->children);
#else
                xsltParseTemplateContent(style, child);
#endif
            }
            if (child->psvi == NULL) {
                xsltTransformError(NULL, style, child,
                    "xsl:attribute-set : internal error, attribute %s not "
                    "compiled\n", child->name);
            }
            else {
	        set->attrs = xsltAddAttrElemList(set->attrs, child);
            }
	}

	child = child->next;
    }

    /*
    * Process attribute "use-attribute-sets".
    */
    value = xmlGetNsProp(cur, BAD_CAST "use-attribute-sets", NULL);
    if (value != NULL) {
	const xmlChar *curval, *endval;
	curval = value;
	while (*curval != 0) {
	    while (IS_BLANK(*curval)) curval++;
	    if (*curval == 0)
		break;
	    endval = curval;
	    while ((*endval != 0) && (!IS_BLANK(*endval))) endval++;
	    curval = xmlDictLookup(style->dict, curval, endval - curval);
	    if (curval) {
		const xmlChar *ncname2 = NULL;
		const xmlChar *prefix2 = NULL;
                const xmlChar *nsUri2 = NULL;

#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
		xsltGenericDebug(xsltGenericDebugContext,
		    "xsl:attribute-set : %s adds use %s\n", ncname, curval);
#endif

                if (xmlValidateQName(curval, 0)) {
                    xsltTransformError(NULL, style, cur,
                        "xsl:attribute-set : The name '%s' in "
                        "use-attribute-sets is not a valid QName.\n", curval);
                    style->errors++;
                    xmlFree(value);
                    return;
                }

		ncname2 = xsltSplitQName(style->dict, curval, &prefix2);
                if (prefix2 != NULL) {
                    xmlNsPtr ns2 = xmlSearchNs(style->doc, cur, prefix2);
                    if (ns2 == NULL) {
                        xsltTransformError(NULL, style, cur,
                            "xsl:attribute-set : No namespace found for QName "
                            "'%s:%s' in use-attribute-sets\n",
                            prefix2, ncname2);
                        style->errors++;
                        xmlFree(value);
                        return;
                    }
                    nsUri2 = ns2->href;
                }
                set->useAttrSets = xsltAddUseAttrSetList(set->useAttrSets,
                                                         ncname2, nsUri2);
	    }
	    curval = endval;
	}
	xmlFree(value);
	value = NULL;
    }

#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
    xsltGenericDebug(xsltGenericDebugContext,
	"updated attribute list %s\n", ncname);
#endif
}

/**
 * xsltResolveUseAttrSets:
 * @set: the attribute set
 * @asctx:  the context for attribute set resolution
 * @depth: recursion depth
 *
 * Process "use-attribute-sets".
 */
static void
xsltResolveUseAttrSets(xsltAttrSetPtr set, xsltStylesheetPtr topStyle,
		       int depth) {
    xsltStylesheetPtr cur;
    xsltAttrSetPtr other;
    xsltUseAttrSetPtr use = set->useAttrSets;
    xsltUseAttrSetPtr next;

    while (use != NULL) {
        /*
         * Iterate top stylesheet and all imports.
         */
        cur = topStyle;
        while (cur != NULL) {
            if (cur->attributeSets) {
                other = xmlHashLookup2(cur->attributeSets, use->ncname,
                                       use->ns);
                if (other != NULL) {
                    xsltResolveAttrSet(other, topStyle, cur, use->ncname,
                                       use->ns, depth + 1);
                    xsltMergeAttrSets(set, other);
                    break;
                }
            }
            cur = xsltNextImport(cur);
        }

        next = use->next;
        /* Free useAttrSets early. */
        xsltFreeUseAttrSet(use);
        use = next;
    }

    set->useAttrSets = NULL;
}

/**
 * xsltResolveAttrSet:
 * @set: the attribute set
 * @asctx:  the context for attribute set resolution
 * @name: the local name of the attirbute set
 * @ns: the namespace of the attribute set
 * @depth: recursion depth
 *
 * resolve the references in an attribute set.
 */
static void
xsltResolveAttrSet(xsltAttrSetPtr set, xsltStylesheetPtr topStyle,
                   xsltStylesheetPtr style, const xmlChar *name,
                   const xmlChar *ns, int depth) {
    xsltStylesheetPtr cur;
    xsltAttrSetPtr other;

    if (set->state == ATTRSET_RESOLVED)
        return;
    if (set->state == ATTRSET_RESOLVING) {
	xsltTransformError(NULL, topStyle, NULL,
            "xsl:attribute-set : use-attribute-sets recursion detected"
            " on %s\n", name);
        topStyle->errors++;
        set->state = ATTRSET_RESOLVED;
        return;
    }
    if (depth > 100) {
	xsltTransformError(NULL, topStyle, NULL,
		"xsl:attribute-set : use-attribute-sets maximum recursion "
		"depth exceeded on %s\n", name);
        topStyle->errors++;
	return;
    }

    set->state = ATTRSET_RESOLVING;

    xsltResolveUseAttrSets(set, topStyle, depth);

    /* Merge imported sets. */
    cur = xsltNextImport(style);
    while (cur != NULL) {
        if (cur->attributeSets != NULL) {
            other = xmlHashLookup2(cur->attributeSets, name, ns);

            if (other != NULL) {
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
                xsltGenericDebug(xsltGenericDebugContext,
                    "xsl:attribute-set : merging import for %s\n", name);
#endif
                xsltResolveUseAttrSets(other, topStyle, depth);
                xsltMergeAttrSets(set, other);
                xmlHashRemoveEntry2(cur->attributeSets, name, ns, NULL);
                xsltFreeAttrSet(other);
            }
        }

        cur = xsltNextImport(cur);
    }

    set->state = ATTRSET_RESOLVED;
}

/**
 * xsltResolveSASCallback:
 * @set: the attribute set
 * @asctx:  the context for attribute set resolution
 * @name: the local name of the attirbute set
 * @ns: the namespace of the attribute set
 *
 * resolve the references in an attribute set.
 */
static void
xsltResolveSASCallback(void *payload, void *data,
	               const xmlChar *name, const xmlChar *ns,
		       ATTRIBUTE_UNUSED const xmlChar *ignored) {
    xsltAttrSetPtr set = (xsltAttrSetPtr) payload;
    xsltAttrSetContextPtr asctx = (xsltAttrSetContextPtr) data;
    xsltStylesheetPtr topStyle = asctx->topStyle;
    xsltStylesheetPtr style = asctx->style;

    if (asctx->error) {
        if (style != topStyle)
            xsltFreeAttrSet(set);
        return;
    }

    xsltResolveAttrSet(set, topStyle, style, name, ns, 1);

    /* Move attribute sets to top stylesheet. */
    if (style != topStyle) {
        /*
         * This imported stylesheet won't be visited anymore. Don't bother
         * removing the hash entry.
         */
        if (xmlHashAddEntry2(topStyle->attributeSets, name, ns, set) < 0) {
	    xsltGenericError(xsltGenericErrorContext,
                "xsl:attribute-set : internal error, can't move imported "
                " attribute set %s\n", name);
            asctx->error = 1;
            xsltFreeAttrSet(set);
        }
    }
}

/**
 * xsltResolveStylesheetAttributeSet:
 * @style:  the XSLT stylesheet
 *
 * resolve the references between attribute sets.
 */
void
xsltResolveStylesheetAttributeSet(xsltStylesheetPtr style) {
    xsltStylesheetPtr cur;
    xsltAttrSetContext asctx;

#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
    xsltGenericDebug(xsltGenericDebugContext,
	    "Resolving attribute sets references\n");
#endif
    asctx.topStyle = style;
    asctx.error = 0;
    cur = style;
    while (cur != NULL) {
	if (cur->attributeSets != NULL) {
	    if (style->attributeSets == NULL) {
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
		xsltGenericDebug(xsltGenericDebugContext,
		    "creating attribute set table\n");
#endif
		style->attributeSets = xmlHashCreate(10);
	    }
            asctx.style = cur;
	    xmlHashScanFull(cur->attributeSets, xsltResolveSASCallback,
                            &asctx);

            if (cur != style) {
                /*
                 * the attribute lists have either been migrated to style
                 * or freed directly in xsltResolveSASCallback()
                 */
                xmlHashFree(cur->attributeSets, NULL);
                cur->attributeSets = NULL;
            }
	}
	cur = xsltNextImport(cur);
    }
}

/**
 * xsltAttribute:
 * @ctxt:  a XSLT process context
 * @contextNode:  the current node in the source tree
 * @inst:  the xsl:attribute element
 * @castedComp:  precomputed information
 *
 * Process the xslt attribute node on the source node
 */
void
xsltAttribute(xsltTransformContextPtr ctxt,
	      xmlNodePtr contextNode,
              xmlNodePtr inst,
	      xsltElemPreCompPtr castedComp)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemAttributePtr comp =
	(xsltStyleItemAttributePtr) castedComp;
#else
    xsltStylePreCompPtr comp = (xsltStylePreCompPtr) castedComp;
#endif
    xmlNodePtr targetElem;
    xmlChar *prop = NULL;
    const xmlChar *name = NULL, *prefix = NULL, *nsName = NULL;
    xmlChar *value = NULL;
    xmlNsPtr ns = NULL;
    xmlAttrPtr attr;

    if ((ctxt == NULL) || (contextNode == NULL) || (inst == NULL) ||
        (inst->type != XML_ELEMENT_NODE) )
        return;

    /*
    * A comp->has_name == 0 indicates that we need to skip this instruction,
    * since it was evaluated to be invalid already during compilation.
    */
    if (!comp->has_name)
        return;
    /*
    * BIG NOTE: This previously used xsltGetSpecialNamespace() and
    *  xsltGetNamespace(), but since both are not appropriate, we
    *  will process namespace lookup here to avoid adding yet another
    *  ns-lookup function to namespaces.c.
    */
    /*
    * SPEC XSLT 1.0: Error cases:
    * - Creating nodes other than text nodes during the instantiation of
    *   the content of the xsl:attribute element; implementations may
    *   either signal the error or ignore the offending nodes."
    */

    if (comp == NULL) {
        xsltTransformError(ctxt, NULL, inst,
	    "Internal error in xsltAttribute(): "
	    "The XSLT 'attribute' instruction was not compiled.\n");
        return;
    }
    /*
    * TODO: Shouldn't ctxt->insert == NULL be treated as an internal error?
    *   So report an internal error?
    */
    if (ctxt->insert == NULL)
        return;
    /*
    * SPEC XSLT 1.0:
    *  "Adding an attribute to a node that is not an element;
    *  implementations may either signal the error or ignore the attribute."
    *
    * TODO: I think we should signal such errors in the future, and maybe
    *  provide an option to ignore such errors.
    */
    targetElem = ctxt->insert;
    if (targetElem->type != XML_ELEMENT_NODE)
	return;

    /*
    * SPEC XSLT 1.0:
    * "Adding an attribute to an element after children have been added
    *  to it; implementations may either signal the error or ignore the
    *  attribute."
    *
    * TODO: We should decide whether not to report such errors or
    *  to ignore them; note that we *ignore* if the parent is not an
    *  element, but here we report an error.
    */
    if (targetElem->children != NULL) {
	/*
	* NOTE: Ah! This seems to be intended to support streamed
	*  result generation!.
	*/
        xsltTransformError(ctxt, NULL, inst,
	    "xsl:attribute: Cannot add attributes to an "
	    "element if children have been already added "
	    "to the element.\n");
        return;
    }

    /*
    * Process the name
    * ----------------
    */

#ifdef WITH_DEBUGGER
    if (ctxt->debugStatus != XSLT_DEBUG_NONE)
        xslHandleDebugger(inst, contextNode, NULL, ctxt);
#endif

    if (comp->name == NULL) {
	/* TODO: fix attr acquisition wrt to the XSLT namespace */
        prop = xsltEvalAttrValueTemplate(ctxt, inst,
	    (const xmlChar *) "name", XSLT_NAMESPACE);
        if (prop == NULL) {
            xsltTransformError(ctxt, NULL, inst,
		"xsl:attribute: The attribute 'name' is missing.\n");
            goto error;
        }
	if (xmlValidateQName(prop, 0)) {
	    xsltTransformError(ctxt, NULL, inst,
		"xsl:attribute: The effective name '%s' is not a "
		"valid QName.\n", prop);
	    /* we fall through to catch any further errors, if possible */
	}

	/*
	* Reject a name of "xmlns".
	*/
	if (xmlStrEqual(prop, BAD_CAST "xmlns")) {
            xsltTransformError(ctxt, NULL, inst,
                "xsl:attribute: The effective name 'xmlns' is not allowed.\n");
	    xmlFree(prop);
	    goto error;
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
    * Process namespace semantics
    * ---------------------------
    *
    * Evaluate the namespace name.
    */
    if (comp->has_ns) {
	/*
	* The "namespace" attribute was existent.
	*/
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
	    * This fixes bug #302020: The AVT might also evaluate to the
	    * empty string; this means that the empty string also indicates
	    * "no namespace".
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
    } else if (prefix != NULL) {
	/*
	* SPEC XSLT 1.0:
	*  "If the namespace attribute is not present, then the QName is
	*  expanded into an expanded-name using the namespace declarations
	*  in effect for the xsl:attribute element, *not* including any
	*  default namespace declaration."
	*/
	ns = xmlSearchNs(inst->doc, inst, prefix);
	if (ns == NULL) {
	    /*
	    * Note that this is treated as an error now (checked with
	    *  Saxon, Xalan-J and MSXML).
	    */
	    xsltTransformError(ctxt, NULL, inst,
		"xsl:attribute: The QName '%s:%s' has no "
		"namespace binding in scope in the stylesheet; "
		"this is an error, since the namespace was not "
		"specified by the instruction itself.\n", prefix, name);
	} else
	    nsName = ns->href;
    }

    /*
    * Find/create a matching ns-decl in the result tree.
    */
    ns = NULL;

#if 0
    if (0) {
	/*
	* OPTIMIZE TODO: How do we know if we are adding to a
	*  fragment or to the result tree?
	*
	* If we are adding to a result tree fragment (i.e., not to the
	* actual result tree), we'll don't bother searching for the
	* ns-decl, but just store it in the dummy-doc of the result
	* tree fragment.
	*/
	if (nsName != NULL) {
	    /*
	    * TODO: Get the doc of @targetElem.
	    */
	    ns = xsltTreeAcquireStoredNs(some doc, nsName, prefix);
	}
    }
#endif

    if (nsName != NULL) {
	/*
	* Something about ns-prefixes:
	* SPEC XSLT 1.0:
	*  "XSLT processors may make use of the prefix of the QName specified
	*  in the name attribute when selecting the prefix used for outputting
	*  the created attribute as XML; however, they are not required to do
	*  so and, if the prefix is xmlns, they must not do so"
	*/
	/*
	* xsl:attribute can produce a scenario where the prefix is NULL,
	* so generate a prefix.
	*/
	if ((prefix == NULL) || xmlStrEqual(prefix, BAD_CAST "xmlns")) {
	    xmlChar *pref = xmlStrdup(BAD_CAST "ns_1");

	    ns = xsltGetSpecialNamespace(ctxt, inst, nsName, pref, targetElem);

	    xmlFree(pref);
	} else {
	    ns = xsltGetSpecialNamespace(ctxt, inst, nsName, prefix,
		targetElem);
	}
	if (ns == NULL) {
	    xsltTransformError(ctxt, NULL, inst,
		"Namespace fixup error: Failed to acquire an in-scope "
		"namespace binding for the generated attribute '{%s}%s'.\n",
		nsName, name);
	    goto error;
	}
    }
    /*
    * Construction of the value
    * -------------------------
    */
    if (inst->children == NULL) {
	/*
	* No content.
	* TODO: Do we need to put the empty string in ?
	*/
	attr = xmlSetNsProp(ctxt->insert, ns, name, (const xmlChar *) "");
    } else if ((inst->children->next == NULL) &&
	    ((inst->children->type == XML_TEXT_NODE) ||
	     (inst->children->type == XML_CDATA_SECTION_NODE)))
    {
	xmlNodePtr copyTxt;

	/*
	* xmlSetNsProp() will take care of duplicates.
	*/
	attr = xmlSetNsProp(ctxt->insert, ns, name, NULL);
	if (attr == NULL) /* TODO: report error ? */
	    goto error;
	/*
	* This was taken over from xsltCopyText() (transform.c).
	*/
	if (ctxt->internalized &&
	    (ctxt->insert->doc != NULL) &&
	    (ctxt->insert->doc->dict == ctxt->dict))
	{
	    copyTxt = xmlNewText(NULL);
	    if (copyTxt == NULL) /* TODO: report error */
		goto error;
	    /*
	    * This is a safe scenario where we don't need to lookup
	    * the dict.
	    */
	    copyTxt->content = inst->children->content;
	    /*
	    * Copy "disable-output-escaping" information.
	    * TODO: Does this have any effect for attribute values
	    *  anyway?
	    */
	    if (inst->children->name == xmlStringTextNoenc)
		copyTxt->name = xmlStringTextNoenc;
	} else {
	    /*
	    * Copy the value.
	    */
	    copyTxt = xmlNewText(inst->children->content);
	    if (copyTxt == NULL) /* TODO: report error */
		goto error;
	}
	attr->children = attr->last = copyTxt;
	copyTxt->parent = (xmlNodePtr) attr;
	copyTxt->doc = attr->doc;
	/*
	* Copy "disable-output-escaping" information.
	* TODO: Does this have any effect for attribute values
	*  anyway?
	*/
	if (inst->children->name == xmlStringTextNoenc)
	    copyTxt->name = xmlStringTextNoenc;

        /*
         * since we create the attribute without content IDness must be
         * asserted as a second step
         */
        if ((copyTxt->content != NULL) &&
            (xmlIsID(attr->doc, attr->parent, attr)))
            xmlAddID(NULL, attr->doc, copyTxt->content, attr);
    } else {
	/*
	* The sequence constructor might be complex, so instantiate it.
	*/
	value = xsltEvalTemplateString(ctxt, contextNode, inst);
	if (value != NULL) {
	    attr = xmlSetNsProp(ctxt->insert, ns, name, value);
	    xmlFree(value);
	} else {
	    /*
	    * TODO: Do we have to add the empty string to the attr?
	    * TODO: Does a  value of NULL indicate an
	    *  error in xsltEvalTemplateString() ?
	    */
	    attr = xmlSetNsProp(ctxt->insert, ns, name,
		(const xmlChar *) "");
	}
    }

error:
    return;
}

/**
 * xsltApplyAttributeSet:
 * @ctxt:  the XSLT stylesheet
 * @node:  the node in the source tree.
 * @inst:  the attribute node "xsl:use-attribute-sets"
 * @attrSets:  the list of QNames of the attribute-sets to be applied
 *
 * Apply the xsl:use-attribute-sets.
 * If @attrSets is NULL, then @inst will be used to exctract this
 * value.
 * If both, @attrSets and @inst, are NULL, then this will do nothing.
 */
void
xsltApplyAttributeSet(xsltTransformContextPtr ctxt, xmlNodePtr node,
                      xmlNodePtr inst,
                      const xmlChar *attrSets)
{
    const xmlChar *ncname = NULL;
    const xmlChar *prefix = NULL;
    const xmlChar *curstr, *endstr;
    xsltAttrSetPtr set;
    xsltStylesheetPtr style;

    if (attrSets == NULL) {
	if (inst == NULL)
	    return;
	else {
	    /*
	    * Extract the value from @inst.
	    */
	    if (inst->type == XML_ATTRIBUTE_NODE) {
		if ( ((xmlAttrPtr) inst)->children != NULL)
		    attrSets = ((xmlAttrPtr) inst)->children->content;

	    }
	    if (attrSets == NULL) {
		/*
		* TODO: Return an error?
		*/
		return;
	    }
	}
    }
    /*
    * Parse/apply the list of QNames.
    */
    curstr = attrSets;
    while (*curstr != 0) {
        while (IS_BLANK(*curstr))
            curstr++;
        if (*curstr == 0)
            break;
        endstr = curstr;
        while ((*endstr != 0) && (!IS_BLANK(*endstr)))
            endstr++;
        curstr = xmlDictLookup(ctxt->dict, curstr, endstr - curstr);
        if (curstr) {
            xmlNsPtr ns;
            const xmlChar *nsUri = NULL;

#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
            xsltGenericDebug(xsltGenericDebugContext,
                             "apply attribute set %s\n", curstr);
#endif

            if (xmlValidateQName(curstr, 0)) {
                xsltTransformError(ctxt, NULL, inst,
                    "The name '%s' in use-attribute-sets is not a valid "
                    "QName.\n", curstr);
                return;
            }

            ncname = xsltSplitQName(ctxt->dict, curstr, &prefix);
            if (prefix != NULL) {
	        ns = xmlSearchNs(inst->doc, inst, prefix);
                if (ns == NULL) {
                    xsltTransformError(ctxt, NULL, inst,
                        "use-attribute-set : No namespace found for QName "
                        "'%s:%s'\n", prefix, ncname);
                    return;
                }
                nsUri = ns->href;
            }

            style = ctxt->style;

#ifdef WITH_DEBUGGER
            if ((style != NULL) &&
		(style->attributeSets != NULL) &&
		(ctxt->debugStatus != XSLT_DEBUG_NONE))
	    {
                set = xmlHashLookup2(style->attributeSets, ncname, nsUri);
                if ((set != NULL) && (set->attrs != NULL) &&
                    (set->attrs->attr != NULL))
                    xslHandleDebugger(set->attrs->attr->parent, node, NULL,
			ctxt);
            }
#endif
	    /*
	    * Lookup the referenced attribute-set. All attribute sets were
            * moved to the top stylesheet so there's no need to iterate
            * imported stylesheets
	    */
            set = xmlHashLookup2(style->attributeSets, ncname, nsUri);
            if (set != NULL) {
                xsltAttrElemPtr cur = set->attrs;
                while (cur != NULL) {
                    if (cur->attr != NULL) {
                        xsltAttribute(ctxt, node, cur->attr,
                            cur->attr->psvi);
                    }
                    cur = cur->next;
                }
            }
        }
        curstr = endstr;
    }
}

static void
xsltFreeAttributeSetsEntry(void *payload,
                           const xmlChar *name ATTRIBUTE_UNUSED) {
    xsltFreeAttrSet((xsltAttrSetPtr) payload);
}

/**
 * xsltFreeAttributeSetsHashes:
 * @style: an XSLT stylesheet
 *
 * Free up the memory used by attribute sets
 */
void
xsltFreeAttributeSetsHashes(xsltStylesheetPtr style) {
    if (style->attributeSets != NULL)
	xmlHashFree((xmlHashTablePtr) style->attributeSets,
		    xsltFreeAttributeSetsEntry);
    style->attributeSets = NULL;
}
