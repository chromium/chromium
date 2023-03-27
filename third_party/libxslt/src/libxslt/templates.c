/*
 * templates.c: Implementation of the template processing
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
#include <libxml/globals.h>
#include <libxml/xmlerror.h>
#include <libxml/tree.h>
#include <libxml/dict.h>
#include <libxml/xpathInternals.h>
#include <libxml/parserInternals.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "variables.h"
#include "functions.h"
#include "templates.h"
#include "transform.h"
#include "namespaces.h"
#include "attributes.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_TEMPLATES
#endif

/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/

/**
 * xsltEvalXPathPredicate:
 * @ctxt:  the XSLT transformation context
 * @comp:  the XPath compiled expression
 * @nsList:  the namespaces in scope
 * @nsNr:  the number of namespaces in scope
 *
 * Process the expression using XPath and evaluate the result as
 * an XPath predicate
 *
 * Returns 1 is the predicate was true, 0 otherwise
 */
int
xsltEvalXPathPredicate(xsltTransformContextPtr ctxt, xmlXPathCompExprPtr comp,
		       xmlNsPtr *nsList, int nsNr) {
    int ret;
    xmlXPathObjectPtr res;
    int oldNsNr;
    xmlNsPtr *oldNamespaces;
    xmlNodePtr oldInst;
    int oldProximityPosition, oldContextSize;

    if ((ctxt == NULL) || (ctxt->inst == NULL)) {
        xsltTransformError(ctxt, NULL, NULL,
            "xsltEvalXPathPredicate: No context or instruction\n");
        return(0);
    }

    oldContextSize = ctxt->xpathCtxt->contextSize;
    oldProximityPosition = ctxt->xpathCtxt->proximityPosition;
    oldNsNr = ctxt->xpathCtxt->nsNr;
    oldNamespaces = ctxt->xpathCtxt->namespaces;
    oldInst = ctxt->inst;

    ctxt->xpathCtxt->node = ctxt->node;
    ctxt->xpathCtxt->namespaces = nsList;
    ctxt->xpathCtxt->nsNr = nsNr;

    res = xmlXPathCompiledEval(comp, ctxt->xpathCtxt);

    if (res != NULL) {
	ret = xmlXPathEvalPredicate(ctxt->xpathCtxt, res);
	xmlXPathFreeObject(res);
#ifdef WITH_XSLT_DEBUG_TEMPLATES
	XSLT_TRACE(ctxt,XSLT_TRACE_TEMPLATES,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltEvalXPathPredicate: returns %d\n", ret));
#endif
    } else {
#ifdef WITH_XSLT_DEBUG_TEMPLATES
	XSLT_TRACE(ctxt,XSLT_TRACE_TEMPLATES,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltEvalXPathPredicate: failed\n"));
#endif
	ctxt->state = XSLT_STATE_STOPPED;
	ret = 0;
    }
    ctxt->xpathCtxt->nsNr = oldNsNr;

    ctxt->xpathCtxt->namespaces = oldNamespaces;
    ctxt->inst = oldInst;
    ctxt->xpathCtxt->contextSize = oldContextSize;
    ctxt->xpathCtxt->proximityPosition = oldProximityPosition;

    return(ret);
}

/**
 * xsltEvalXPathStringNs:
 * @ctxt:  the XSLT transformation context
 * @comp:  the compiled XPath expression
 * @nsNr:  the number of namespaces in the list
 * @nsList:  the list of in-scope namespaces to use
 *
 * Process the expression using XPath, allowing to pass a namespace mapping
 * context and get a string
 *
 * Returns the computed string value or NULL, must be deallocated by the
 *    caller.
 */
xmlChar *
xsltEvalXPathStringNs(xsltTransformContextPtr ctxt, xmlXPathCompExprPtr comp,
	              int nsNr, xmlNsPtr *nsList) {
    xmlChar *ret = NULL;
    xmlXPathObjectPtr res;
    xmlNodePtr oldInst;
    xmlNodePtr oldNode;
    int	oldPos, oldSize;
    int oldNsNr;
    xmlNsPtr *oldNamespaces;

    if ((ctxt == NULL) || (ctxt->inst == NULL)) {
        xsltTransformError(ctxt, NULL, NULL,
            "xsltEvalXPathStringNs: No context or instruction\n");
        return(0);
    }

    oldInst = ctxt->inst;
    oldNode = ctxt->node;
    oldPos = ctxt->xpathCtxt->proximityPosition;
    oldSize = ctxt->xpathCtxt->contextSize;
    oldNsNr = ctxt->xpathCtxt->nsNr;
    oldNamespaces = ctxt->xpathCtxt->namespaces;

    ctxt->xpathCtxt->node = ctxt->node;
    /* TODO: do we need to propagate the namespaces here ? */
    ctxt->xpathCtxt->namespaces = nsList;
    ctxt->xpathCtxt->nsNr = nsNr;
    res = xmlXPathCompiledEval(comp, ctxt->xpathCtxt);
    if (res != NULL) {
	if (res->type != XPATH_STRING)
	    res = xmlXPathConvertString(res);
	if ((res != NULL) && (res->type == XPATH_STRING)) {
            ret = res->stringval;
	    res->stringval = NULL;
	} else {
	    xsltTransformError(ctxt, NULL, NULL,
		 "xpath : string() function didn't return a String\n");
	}
	xmlXPathFreeObject(res);
    } else {
	ctxt->state = XSLT_STATE_STOPPED;
    }
#ifdef WITH_XSLT_DEBUG_TEMPLATES
    XSLT_TRACE(ctxt,XSLT_TRACE_TEMPLATES,xsltGenericDebug(xsltGenericDebugContext,
	 "xsltEvalXPathString: returns %s\n", ret));
#endif
    ctxt->inst = oldInst;
    ctxt->node = oldNode;
    ctxt->xpathCtxt->contextSize = oldSize;
    ctxt->xpathCtxt->proximityPosition = oldPos;
    ctxt->xpathCtxt->nsNr = oldNsNr;
    ctxt->xpathCtxt->namespaces = oldNamespaces;
    return(ret);
}

/**
 * xsltEvalXPathString:
 * @ctxt:  the XSLT transformation context
 * @comp:  the compiled XPath expression
 *
 * Process the expression using XPath and get a string
 *
 * Returns the computed string value or NULL, must be deallocated by the
 *    caller.
 */
xmlChar *
xsltEvalXPathString(xsltTransformContextPtr ctxt, xmlXPathCompExprPtr comp) {
    return(xsltEvalXPathStringNs(ctxt, comp, 0, NULL));
}

/**
 * xsltEvalTemplateString:
 * @ctxt:  the XSLT transformation context
 * @contextNode:  the current node in the source tree
 * @inst:  the XSLT instruction (xsl:comment, xsl:processing-instruction)
 *
 * Processes the sequence constructor of the given instruction on
 * @contextNode and converts the resulting tree to a string.
 * This is needed by e.g. xsl:comment and xsl:processing-instruction.
 *
 * Returns the computed string value or NULL; it's up to the caller to
 *         free the result.
 */
xmlChar *
xsltEvalTemplateString(xsltTransformContextPtr ctxt,
		       xmlNodePtr contextNode,
	               xmlNodePtr inst)
{
    xmlNodePtr oldInsert, insert = NULL;
    xmlChar *ret;
    const xmlChar *oldLastText;
    int oldLastTextSize, oldLastTextUse;

    if ((ctxt == NULL) || (contextNode == NULL) || (inst == NULL) ||
        (inst->type != XML_ELEMENT_NODE))
	return(NULL);

    if (inst->children == NULL)
	return(NULL);

    /*
    * This creates a temporary element-node to add the resulting
    * text content to.
    * OPTIMIZE TODO: Keep such an element-node in the transformation
    *  context to avoid creating it every time.
    */
    insert = xmlNewDocNode(ctxt->output, NULL,
	                   (const xmlChar *)"fake", NULL);
    if (insert == NULL) {
	xsltTransformError(ctxt, NULL, inst,
		"Failed to create temporary node\n");
	return(NULL);
    }
    oldInsert = ctxt->insert;
    ctxt->insert = insert;
    oldLastText = ctxt->lasttext;
    oldLastTextSize = ctxt->lasttsize;
    oldLastTextUse = ctxt->lasttuse;
    /*
    * OPTIMIZE TODO: if inst->children consists only of text-nodes.
    */
    xsltApplyOneTemplate(ctxt, contextNode, inst->children, NULL, NULL);

    ctxt->insert = oldInsert;
    ctxt->lasttext = oldLastText;
    ctxt->lasttsize = oldLastTextSize;
    ctxt->lasttuse = oldLastTextUse;

    ret = xmlNodeGetContent(insert);
    if (insert != NULL)
	xmlFreeNode(insert);
    return(ret);
}

/**
 * xsltAttrTemplateValueProcessNode:
 * @ctxt:  the XSLT transformation context
 * @str:  the attribute template node value
 * @inst:  the instruction (or LRE) in the stylesheet holding the
 *         attribute with an AVT
 *
 * Process the given string, allowing to pass a namespace mapping
 * context and return the new string value.
 *
 * Called by:
 *  - xsltAttrTemplateValueProcess() (templates.c)
 *  - xsltEvalAttrValueTemplate() (templates.c)
 *
 * QUESTION: Why is this function public? It is not used outside
 *  of templates.c.
 *
 * Returns the computed string value or NULL, must be deallocated by the
 *    caller.
 */
xmlChar *
xsltAttrTemplateValueProcessNode(xsltTransformContextPtr ctxt,
	  const xmlChar *str, xmlNodePtr inst)
{
    xmlChar *ret = NULL;
    const xmlChar *cur;
    xmlChar *expr, *val;
    xmlNsPtr *nsList = NULL;
    int nsNr = 0;

    if (str == NULL) return(NULL);
    if (*str == 0)
	return(xmlStrndup((xmlChar *)"", 0));

    cur = str;
    while (*cur != 0) {
	if (*cur == '{') {
	    if (*(cur+1) == '{') {	/* escaped '{' */
	        cur++;
		ret = xmlStrncat(ret, str, cur - str);
		cur++;
		str = cur;
		continue;
	    }
	    ret = xmlStrncat(ret, str, cur - str);
	    str = cur;
	    cur++;
	    while ((*cur != 0) && (*cur != '}')) {
		/* Need to check for literal (bug539741) */
		if ((*cur == '\'') || (*cur == '"')) {
		    char delim = *(cur++);
		    while ((*cur != 0) && (*cur != delim))
			cur++;
		    if (*cur != 0)
			cur++;	/* skip the ending delimiter */
		} else
		    cur++;
            }
	    if (*cur == 0) {
	        xsltTransformError(ctxt, NULL, inst,
			"xsltAttrTemplateValueProcessNode: unmatched '{'\n");
		ret = xmlStrncat(ret, str, cur - str);
		goto exit;
	    }
	    str++;
	    expr = xmlStrndup(str, cur - str);
	    if (expr == NULL)
		goto exit;
	    else if (*expr == '{') {
		ret = xmlStrcat(ret, expr);
		xmlFree(expr);
	    } else {
		xmlXPathCompExprPtr comp;
		/*
		 * TODO: keep precompiled form around
		 */
		if ((nsList == NULL) && (inst != NULL)) {
		    int i = 0;

		    nsList = xmlGetNsList(inst->doc, inst);
		    if (nsList != NULL) {
			while (nsList[i] != NULL)
			    i++;
			nsNr = i;
		    }
		}
		comp = xmlXPathCtxtCompile(ctxt->xpathCtxt, expr);
                val = xsltEvalXPathStringNs(ctxt, comp, nsNr, nsList);
		xmlXPathFreeCompExpr(comp);
		xmlFree(expr);
		if (val != NULL) {
		    ret = xmlStrcat(ret, val);
		    xmlFree(val);
		}
	    }
	    cur++;
	    str = cur;
	} else if (*cur == '}') {
	    cur++;
	    if (*cur == '}') {	/* escaped '}' */
		ret = xmlStrncat(ret, str, cur - str);
		cur++;
		str = cur;
		continue;
	    } else {
	        xsltTransformError(ctxt, NULL, inst,
		     "xsltAttrTemplateValueProcessNode: unmatched '}'\n");
	    }
	} else
	    cur++;
    }
    if (cur != str) {
	ret = xmlStrncat(ret, str, cur - str);
    }

exit:
    if (nsList != NULL)
	xmlFree(nsList);

    return(ret);
}

/**
 * xsltAttrTemplateValueProcess:
 * @ctxt:  the XSLT transformation context
 * @str:  the attribute template node value
 *
 * Process the given node and return the new string value.
 *
 * Returns the computed string value or NULL, must be deallocated by the
 *    caller.
 */
xmlChar *
xsltAttrTemplateValueProcess(xsltTransformContextPtr ctxt, const xmlChar *str) {
    return(xsltAttrTemplateValueProcessNode(ctxt, str, NULL));
}

/**
 * xsltEvalAttrValueTemplate:
 * @ctxt:  the XSLT transformation context
 * @inst:  the instruction (or LRE) in the stylesheet holding the
 *         attribute with an AVT
 * @name:  the attribute QName
 * @ns:  the attribute namespace URI
 *
 * Evaluate a attribute value template, i.e. the attribute value can
 * contain expressions contained in curly braces ({}) and those are
 * substituted by they computed value.
 *
 * Returns the computed string value or NULL, must be deallocated by the
 *    caller.
 */
xmlChar *
xsltEvalAttrValueTemplate(xsltTransformContextPtr ctxt, xmlNodePtr inst,
	                  const xmlChar *name, const xmlChar *ns)
{
    xmlChar *ret;
    xmlChar *expr;

    if ((ctxt == NULL) || (inst == NULL) || (name == NULL) ||
        (inst->type != XML_ELEMENT_NODE))
	return(NULL);

    expr = xsltGetNsProp(inst, name, ns);
    if (expr == NULL)
	return(NULL);

    /*
     * TODO: though now {} is detected ahead, it would still be good to
     *       optimize both functions to keep the splitted value if the
     *       attribute content and the XPath precompiled expressions around
     */

    ret = xsltAttrTemplateValueProcessNode(ctxt, expr, inst);
#ifdef WITH_XSLT_DEBUG_TEMPLATES
    XSLT_TRACE(ctxt,XSLT_TRACE_TEMPLATES,xsltGenericDebug(xsltGenericDebugContext,
	 "xsltEvalAttrValueTemplate: %s returns %s\n", expr, ret));
#endif
    if (expr != NULL)
	xmlFree(expr);
    return(ret);
}

/**
 * xsltEvalStaticAttrValueTemplate:
 * @style:  the XSLT stylesheet
 * @inst:  the instruction (or LRE) in the stylesheet holding the
 *         attribute with an AVT
 * @name:  the attribute Name
 * @ns:  the attribute namespace URI
 * @found:  indicator whether the attribute is present
 *
 * Check if an attribute value template has a static value, i.e. the
 * attribute value does not contain expressions contained in curly braces ({})
 *
 * Returns the static string value or NULL, must be deallocated by the
 *    caller.
 */
const xmlChar *
xsltEvalStaticAttrValueTemplate(xsltStylesheetPtr style, xmlNodePtr inst,
			const xmlChar *name, const xmlChar *ns, int *found) {
    const xmlChar *ret;
    xmlChar *expr;

    if ((style == NULL) || (inst == NULL) || (name == NULL) ||
        (inst->type != XML_ELEMENT_NODE))
	return(NULL);

    expr = xsltGetNsProp(inst, name, ns);
    if (expr == NULL) {
	*found = 0;
	return(NULL);
    }
    *found = 1;

    ret = xmlStrchr(expr, '{');
    if (ret != NULL) {
	xmlFree(expr);
	return(NULL);
    }
    ret = xmlDictLookup(style->dict, expr, -1);
    xmlFree(expr);
    return(ret);
}

/**
 * xsltAttrTemplateProcess:
 * @ctxt:  the XSLT transformation context
 * @target:  the element where the attribute will be grafted
 * @attr:  the attribute node of a literal result element
 *
 * Process one attribute of a Literal Result Element (in the stylesheet).
 * Evaluates Attribute Value Templates and copies the attribute over to
 * the result element.
 * This does *not* process attribute sets (xsl:use-attribute-set).
 *
 *
 * Returns the generated attribute node.
 */
xmlAttrPtr
xsltAttrTemplateProcess(xsltTransformContextPtr ctxt, xmlNodePtr target,
	                xmlAttrPtr attr)
{
    const xmlChar *value;
    xmlAttrPtr ret;

    if ((ctxt == NULL) || (attr == NULL) || (target == NULL) ||
        (target->type != XML_ELEMENT_NODE))
	return(NULL);

    if (attr->type != XML_ATTRIBUTE_NODE)
	return(NULL);

    /*
    * Skip all XSLT attributes.
    */
#ifdef XSLT_REFACTORED
    if (attr->psvi == xsltXSLTAttrMarker)
	return(NULL);
#else
    if ((attr->ns != NULL) && xmlStrEqual(attr->ns->href, XSLT_NAMESPACE))
	return(NULL);
#endif
    /*
    * Get the value.
    */
    if (attr->children != NULL) {
	if ((attr->children->type != XML_TEXT_NODE) ||
	    (attr->children->next != NULL))
	{
	    xsltTransformError(ctxt, NULL, attr->parent,
		"Internal error: The children of an attribute node of a "
		"literal result element are not in the expected form.\n");
	    return(NULL);
	}
	value = attr->children->content;
	if (value == NULL)
	    value = xmlDictLookup(ctxt->dict, BAD_CAST "", 0);
    } else
	value = xmlDictLookup(ctxt->dict, BAD_CAST "", 0);
    /*
    * Overwrite duplicates.
    */
    ret = target->properties;
    while (ret != NULL) {
        if (((attr->ns != NULL) == (ret->ns != NULL)) &&
	    xmlStrEqual(ret->name, attr->name) &&
	    ((attr->ns == NULL) || xmlStrEqual(ret->ns->href, attr->ns->href)))
	{
	    break;
	}
        ret = ret->next;
    }
    if (ret != NULL) {
        /* free the existing value */
	xmlFreeNodeList(ret->children);
	ret->children = ret->last = NULL;
	/*
	* Adjust ns-prefix if needed.
	*/
	if ((ret->ns != NULL) &&
	    (! xmlStrEqual(ret->ns->prefix, attr->ns->prefix)))
	{
	    ret->ns = xsltGetNamespace(ctxt, attr->parent, attr->ns, target);
	}
    } else {
        /* create a new attribute */
	if (attr->ns != NULL)
	    ret = xmlNewNsProp(target,
		xsltGetNamespace(ctxt, attr->parent, attr->ns, target),
		    attr->name, NULL);
	else
	    ret = xmlNewNsProp(target, NULL, attr->name, NULL);
    }
    /*
    * Set the value.
    */
    if (ret != NULL) {
        xmlNodePtr text;

        text = xmlNewText(NULL);
	if (text != NULL) {
	    ret->last = ret->children = text;
	    text->parent = (xmlNodePtr) ret;
	    text->doc = ret->doc;

	    if (attr->psvi != NULL) {
		/*
		* Evaluate the Attribute Value Template.
		*/
		xmlChar *val;
		val = xsltEvalAVT(ctxt, attr->psvi, attr->parent);
		if (val == NULL) {
		    /*
		    * TODO: Damn, we need an easy mechanism to report
		    * qualified names!
		    */
		    if (attr->ns) {
			xsltTransformError(ctxt, NULL, attr->parent,
			    "Internal error: Failed to evaluate the AVT "
			    "of attribute '{%s}%s'.\n",
			    attr->ns->href, attr->name);
		    } else {
			xsltTransformError(ctxt, NULL, attr->parent,
			    "Internal error: Failed to evaluate the AVT "
			    "of attribute '%s'.\n",
			    attr->name);
		    }
		    text->content = xmlStrdup(BAD_CAST "");
		} else {
		    text->content = val;
		}
	    } else if ((ctxt->internalized) && (target != NULL) &&
	               (target->doc != NULL) &&
		       (target->doc->dict == ctxt->dict) &&
		       xmlDictOwns(ctxt->dict, value)) {
		text->content = (xmlChar *) value;
	    } else {
		text->content = xmlStrdup(value);
	    }
	}
    } else {
	if (attr->ns) {
	    xsltTransformError(ctxt, NULL, attr->parent,
		"Internal error: Failed to create attribute '{%s}%s'.\n",
		attr->ns->href, attr->name);
	} else {
	    xsltTransformError(ctxt, NULL, attr->parent,
		"Internal error: Failed to create attribute '%s'.\n",
		attr->name);
	}
    }
    return(ret);
}


/**
 * xsltAttrListTemplateProcess:
 * @ctxt:  the XSLT transformation context
 * @target:  the element where the attributes will be grafted
 * @attrs:  the first attribute
 *
 * Processes all attributes of a Literal Result Element.
 * Attribute references are applied via xsl:use-attribute-set
 * attributes.
 * Copies all non XSLT-attributes over to the @target element
 * and evaluates Attribute Value Templates.
 *
 * Called by xsltApplySequenceConstructor() (transform.c).
 *
 * Returns a new list of attribute nodes, or NULL in case of error.
 *         (Don't assign the result to @target->properties; if
 *         the result is NULL, you'll get memory leaks, since the
 *         attributes will be disattached.)
 */
xmlAttrPtr
xsltAttrListTemplateProcess(xsltTransformContextPtr ctxt,
	                    xmlNodePtr target, xmlAttrPtr attrs)
{
    xmlAttrPtr attr, copy, last = NULL;
    xmlNodePtr oldInsert, text;
    xmlNsPtr origNs = NULL, copyNs = NULL;
    const xmlChar *value;
    xmlChar *valueAVT;
    int hasAttr = 0;

    if ((ctxt == NULL) || (target == NULL) || (attrs == NULL) ||
        (target->type != XML_ELEMENT_NODE))
	return(NULL);

    oldInsert = ctxt->insert;
    ctxt->insert = target;

    /*
    * Apply attribute-sets.
    */
    attr = attrs;
    do {
#ifdef XSLT_REFACTORED
	if ((attr->psvi == xsltXSLTAttrMarker) &&
	    xmlStrEqual(attr->name, (const xmlChar *)"use-attribute-sets"))
	{
	    xsltApplyAttributeSet(ctxt, ctxt->node, (xmlNodePtr) attr, NULL);
	}
#else
	if ((attr->ns != NULL) &&
	    xmlStrEqual(attr->name, (const xmlChar *)"use-attribute-sets") &&
	    xmlStrEqual(attr->ns->href, XSLT_NAMESPACE))
	{
	    xsltApplyAttributeSet(ctxt, ctxt->node, (xmlNodePtr) attr, NULL);
	}
#endif
	attr = attr->next;
    } while (attr != NULL);

    if (target->properties != NULL) {
        hasAttr = 1;
    }

    /*
    * Instantiate LRE-attributes.
    */
    attr = attrs;
    do {
	/*
	* Skip XSLT attributes.
	*/
#ifdef XSLT_REFACTORED
	if (attr->psvi == xsltXSLTAttrMarker) {
	    goto next_attribute;
	}
#else
	if ((attr->ns != NULL) &&
	    xmlStrEqual(attr->ns->href, XSLT_NAMESPACE))
	{
	    goto next_attribute;
	}
#endif
	/*
	* Get the value.
	*/
	if (attr->children != NULL) {
	    if ((attr->children->type != XML_TEXT_NODE) ||
		(attr->children->next != NULL))
	    {
		xsltTransformError(ctxt, NULL, attr->parent,
		    "Internal error: The children of an attribute node of a "
		    "literal result element are not in the expected form.\n");
		goto error;
	    }
	    value = attr->children->content;
	    if (value == NULL)
		value = xmlDictLookup(ctxt->dict, BAD_CAST "", 0);
	} else
	    value = xmlDictLookup(ctxt->dict, BAD_CAST "", 0);

	/*
	* Get the namespace. Avoid lookups of same namespaces.
	*/
	if (attr->ns != origNs) {
	    origNs = attr->ns;
	    if (attr->ns != NULL) {
#ifdef XSLT_REFACTORED
		copyNs = xsltGetSpecialNamespace(ctxt, attr->parent,
		    attr->ns->href, attr->ns->prefix, target);
#else
		copyNs = xsltGetNamespace(ctxt, attr->parent,
		    attr->ns, target);
#endif
		if (copyNs == NULL)
		    goto error;
	    } else
		copyNs = NULL;
	}
	/*
	* Create a new attribute.
	*/
        if (hasAttr) {
	    copy = xmlSetNsProp(target, copyNs, attr->name, NULL);
        } else {
            /*
            * Avoid checking for duplicate attributes if there aren't
            * any attribute sets.
            */
	    copy = xmlNewDocProp(target->doc, attr->name, NULL);

	    if (copy != NULL) {
                copy->ns = copyNs;

                /*
                * Attach it to the target element.
                */
                copy->parent = target;
                if (last == NULL) {
                    target->properties = copy;
                    last = copy;
                } else {
                    last->next = copy;
                    copy->prev = last;
                    last = copy;
                }
            }
        }
	if (copy == NULL) {
	    if (attr->ns) {
		xsltTransformError(ctxt, NULL, attr->parent,
		    "Internal error: Failed to create attribute '{%s}%s'.\n",
		    attr->ns->href, attr->name);
	    } else {
		xsltTransformError(ctxt, NULL, attr->parent,
		    "Internal error: Failed to create attribute '%s'.\n",
		    attr->name);
	    }
	    goto error;
	}

	/*
	* Set the value.
	*/
	text = xmlNewText(NULL);
	if (text != NULL) {
	    copy->last = copy->children = text;
	    text->parent = (xmlNodePtr) copy;
	    text->doc = copy->doc;

	    if (attr->psvi != NULL) {
		/*
		* Evaluate the Attribute Value Template.
		*/
		valueAVT = xsltEvalAVT(ctxt, attr->psvi, attr->parent);
		if (valueAVT == NULL) {
		    /*
		    * TODO: Damn, we need an easy mechanism to report
		    * qualified names!
		    */
		    if (attr->ns) {
			xsltTransformError(ctxt, NULL, attr->parent,
			    "Internal error: Failed to evaluate the AVT "
			    "of attribute '{%s}%s'.\n",
			    attr->ns->href, attr->name);
		    } else {
			xsltTransformError(ctxt, NULL, attr->parent,
			    "Internal error: Failed to evaluate the AVT "
			    "of attribute '%s'.\n",
			    attr->name);
		    }
		    text->content = xmlStrdup(BAD_CAST "");
		    goto error;
		} else {
		    text->content = valueAVT;
		}
	    } else if ((ctxt->internalized) &&
		(target->doc != NULL) &&
		(target->doc->dict == ctxt->dict) &&
		xmlDictOwns(ctxt->dict, value))
	    {
		text->content = (xmlChar *) value;
	    } else {
		text->content = xmlStrdup(value);
	    }
            if ((copy != NULL) && (text != NULL) &&
                (xmlIsID(copy->doc, copy->parent, copy)))
                xmlAddID(NULL, copy->doc, text->content, copy);
	}

next_attribute:
	attr = attr->next;
    } while (attr != NULL);

    ctxt->insert = oldInsert;
    return(target->properties);

error:
    ctxt->insert = oldInsert;
    return(NULL);
}


/**
 * xsltTemplateProcess:
 * @ctxt:  the XSLT transformation context
 * @node:  the attribute template node
 *
 * Obsolete. Don't use it.
 *
 * Returns NULL.
 */
xmlNodePtr *
xsltTemplateProcess(xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED, xmlNodePtr node) {
    if (node == NULL)
	return(NULL);

    return(0);
}


