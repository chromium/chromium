/*
 * namespaces.c: Implementation of the XSLT namespaces handling
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

#ifndef	XSLT_NEED_TRIO
#include <stdio.h>
#else
#include <trio.h>
#endif

#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/hash.h>
#include <libxml/xmlerror.h>
#include <libxml/uri.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "namespaces.h"
#include "imports.h"

/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/

#ifdef XSLT_REFACTORED
static xsltNsAliasPtr
xsltNewNsAlias(xsltCompilerCtxtPtr cctxt)
{
    xsltNsAliasPtr ret;

    if (cctxt == NULL)
	return(NULL);

    ret = (xsltNsAliasPtr) xmlMalloc(sizeof(xsltNsAlias));
    if (ret == NULL) {
	xsltTransformError(NULL, cctxt->style, NULL,
	    "Internal error in xsltNewNsAlias(): Memory allocation failed.\n");
	cctxt->style->errors++;
	return(NULL);
    }
    memset(ret, 0, sizeof(xsltNsAlias));
    /*
    * TODO: Store the item at current stylesheet-level.
    */
    ret->next = cctxt->nsAliases;
    cctxt->nsAliases = ret;

    return(ret);
}
#endif /* XSLT_REFACTORED */
/**
 * xsltNamespaceAlias:
 * @style:  the XSLT stylesheet
 * @node:  the xsl:namespace-alias node
 *
 * Read the stylesheet-prefix and result-prefix attributes, register
 * them as well as the corresponding namespace.
 */
void
xsltNamespaceAlias(xsltStylesheetPtr style, xmlNodePtr node)
{
    xmlChar *resultPrefix = NULL;
    xmlChar *stylePrefix = NULL;
    xmlNsPtr literalNs = NULL;
    xmlNsPtr targetNs = NULL;

#ifdef XSLT_REFACTORED
    xsltNsAliasPtr alias;

    if ((style == NULL) || (node == NULL))
	return;

    /*
    * SPEC XSLT 1.0:
    *  "If a namespace URI is declared to be an alias for multiple
    *  different namespace URIs, then the declaration with the highest
    *  import precedence is used. It is an error if there is more than
    *  one such declaration. An XSLT processor may signal the error;
    *  if it does not signal the error, it must recover by choosing,
    *  from amongst the declarations with the highest import precedence,
    *  the one that occurs last in the stylesheet."
    *
    * SPEC TODO: Check for the errors mentioned above.
    */
    /*
    * NOTE that the XSLT 2.0 also *does* use the NULL namespace if
    *  "#default" is used and there's no default namespace is scope.
    *  I.e., this is *not* an error.
    *  Most XSLT 1.0 implementations work this way.
    *  The XSLT 1.0 spec has nothing to say on the subject.
    */
    /*
    * Attribute "stylesheet-prefix".
    */
    stylePrefix = xmlGetNsProp(node, (const xmlChar *)"stylesheet-prefix", NULL);
    if (stylePrefix == NULL) {
	xsltTransformError(NULL, style, node,
	    "The attribute 'stylesheet-prefix' is missing.\n");
	return;
    }
    if (xmlStrEqual(stylePrefix, (const xmlChar *)"#default"))
	literalNs = xmlSearchNs(node->doc, node, NULL);
    else {
	literalNs = xmlSearchNs(node->doc, node, stylePrefix);
	if (literalNs == NULL) {
	    xsltTransformError(NULL, style, node,
	        "Attribute 'stylesheet-prefix': There's no namespace "
		"declaration in scope for the prefix '%s'.\n",
		    stylePrefix);
	    goto error;
	}
    }
    /*
    * Attribute "result-prefix".
    */
    resultPrefix = xmlGetNsProp(node, (const xmlChar *)"result-prefix", NULL);
    if (resultPrefix == NULL) {
	xsltTransformError(NULL, style, node,
	    "The attribute 'result-prefix' is missing.\n");
	goto error;
    }
    if (xmlStrEqual(resultPrefix, (const xmlChar *)"#default"))
	targetNs = xmlSearchNs(node->doc, node, NULL);
    else {
	targetNs = xmlSearchNs(node->doc, node, resultPrefix);

        if (targetNs == NULL) {
	   xsltTransformError(NULL, style, node,
	        "Attribute 'result-prefix': There's no namespace "
		"declaration in scope for the prefix '%s'.\n",
		    stylePrefix);
	    goto error;
	}
    }
    /*
     *
     * Same alias for multiple different target namespace URIs:
     *  TODO: The one with the highest import precedence is used.
     *  Example:
     *  <xsl:namespace-alias stylesheet-prefix="foo"
     *                       result-prefix="bar"/>
     *
     *  <xsl:namespace-alias stylesheet-prefix="foo"
     *                       result-prefix="zar"/>
     *
     * Same target namespace URI for multiple different aliases:
     *  All alias-definitions will be used.
     *  Example:
     *  <xsl:namespace-alias stylesheet-prefix="bar"
     *                       result-prefix="foo"/>
     *
     *  <xsl:namespace-alias stylesheet-prefix="zar"
     *                       result-prefix="foo"/>
     * Cases using #default:
     *  <xsl:namespace-alias stylesheet-prefix="#default"
     *                       result-prefix="#default"/>
     *  TODO: Has this an effect at all?
     *
     *  <xsl:namespace-alias stylesheet-prefix="foo"
     *                       result-prefix="#default"/>
     *  From namespace to no namespace.
     *
     *  <xsl:namespace-alias stylesheet-prefix="#default"
     *                       result-prefix="foo"/>
     *  From no namespace to namespace.
     */


     /*
     * Store the ns-node in the alias-object.
    */
    alias = xsltNewNsAlias(XSLT_CCTXT(style));
    if (alias == NULL)
	return;
    alias->literalNs = literalNs;
    alias->targetNs = targetNs;
    XSLT_CCTXT(style)->hasNsAliases = 1;


#else /* XSLT_REFACTORED */
    const xmlChar *literalNsName;
    const xmlChar *targetNsName;


    if ((style == NULL) || (node == NULL))
	return;

    stylePrefix = xmlGetNsProp(node, (const xmlChar *)"stylesheet-prefix", NULL);
    if (stylePrefix == NULL) {
	xsltTransformError(NULL, style, node,
	    "namespace-alias: stylesheet-prefix attribute missing\n");
	return;
    }
    resultPrefix = xmlGetNsProp(node, (const xmlChar *)"result-prefix", NULL);
    if (resultPrefix == NULL) {
	xsltTransformError(NULL, style, node,
	    "namespace-alias: result-prefix attribute missing\n");
	goto error;
    }

    if (xmlStrEqual(stylePrefix, (const xmlChar *)"#default")) {
	literalNs = xmlSearchNs(node->doc, node, NULL);
	if (literalNs == NULL) {
	    literalNsName = NULL;
	} else
	    literalNsName = literalNs->href; /* Yes - set for nsAlias table */
    } else {
	literalNs = xmlSearchNs(node->doc, node, stylePrefix);

	if ((literalNs == NULL) || (literalNs->href == NULL)) {
	    xsltTransformError(NULL, style, node,
	        "namespace-alias: prefix %s not bound to any namespace\n",
					stylePrefix);
	    goto error;
	} else
	    literalNsName = literalNs->href;
    }

    /*
     * When "#default" is used for result, if a default namespace has not
     * been explicitly declared the special value UNDEFINED_DEFAULT_NS is
     * put into the nsAliases table
     */
    if (xmlStrEqual(resultPrefix, (const xmlChar *)"#default")) {
	targetNs = xmlSearchNs(node->doc, node, NULL);
	if (targetNs == NULL) {
	    targetNsName = UNDEFINED_DEFAULT_NS;
	} else
	    targetNsName = targetNs->href;
    } else {
	targetNs = xmlSearchNs(node->doc, node, resultPrefix);

        if ((targetNs == NULL) || (targetNs->href == NULL)) {
	    xsltTransformError(NULL, style, node,
	        "namespace-alias: prefix %s not bound to any namespace\n",
					resultPrefix);
	    goto error;
	} else
	    targetNsName = targetNs->href;
    }
    /*
     * Special case: if #default is used for
     *  the stylesheet-prefix (literal namespace) and there's no default
     *  namespace in scope, we'll use style->defaultAlias for this.
     */
    if (literalNsName == NULL) {
        if (targetNs != NULL) {
	    /*
	    * BUG TODO: Is it not sufficient to have only 1 field for
	    *  this, since subsequently alias declarations will
	    *  overwrite this.
	    *  Example:
	    *   <xsl:namespace-alias result-prefix="foo"
	    *                        stylesheet-prefix="#default"/>
	    *   <xsl:namespace-alias result-prefix="bar"
	    *                        stylesheet-prefix="#default"/>
	    *  The mapping for "foo" won't be visible anymore.
	    */
            style->defaultAlias = targetNs->href;
	}
    } else {
        if (style->nsAliases == NULL)
	    style->nsAliases = xmlHashCreate(10);
        if (style->nsAliases == NULL) {
	    xsltTransformError(NULL, style, node,
	        "namespace-alias: cannot create hash table\n");
	    goto error;
        }
	xmlHashAddEntry((xmlHashTablePtr) style->nsAliases,
	    literalNsName, (void *) targetNsName);
    }
#endif /* else of XSLT_REFACTORED */

error:
    if (stylePrefix != NULL)
	xmlFree(stylePrefix);
    if (resultPrefix != NULL)
	xmlFree(resultPrefix);
}

/**
 * xsltGetSpecialNamespace:
 * @ctxt:  the transformation context
 * @invocNode: the invoking node; e.g. a literal result element/attr;
 *             only used for error reports
 * @nsName:  the namespace name (or NULL)
 * @nsPrefix:  the suggested namespace prefix (or NULL)
 * @target:  the result element on which to anchor a namespace
 *
 * Find a matching (prefix and ns-name) ns-declaration
 * for the requested @nsName and @nsPrefix in the result tree.
 * If none is found then a new ns-declaration will be
 * added to @resultElem. If, in this case, the given prefix is
 * already in use, then a ns-declaration with a modified ns-prefix
 * be we created. Note that this function's priority is to
 * preserve ns-prefixes; it will only change a prefix if there's
 * a namespace clash.
 * If both @nsName and @nsPrefix are NULL, then this will try to
 * "undeclare" a default namespace by declaring an xmlns="".
 *
 * Returns a namespace declaration or NULL.
 */
xmlNsPtr
xsltGetSpecialNamespace(xsltTransformContextPtr ctxt, xmlNodePtr invocNode,
		const xmlChar *nsName, const xmlChar *nsPrefix,
		xmlNodePtr target)
{
    xmlNsPtr ns;
    int prefixOccupied = 0;

    if ((ctxt == NULL) || (target == NULL) ||
	(target->type != XML_ELEMENT_NODE))
	return(NULL);

    /*
    * NOTE: Namespace exclusion and ns-aliasing is performed at
    *  compilation-time in the refactored code; so this need not be done
    *  here (it was in the old code).
    * NOTE: @invocNode was named @cur in the old code and was documented to
    *  be an input node; since it was only used to anchor an error report
    *  somewhere, we can safely change this to @invocNode, which now
    *  will be the XSLT instruction (also a literal result element/attribute),
    *  which was responsible for this call.
    */
    /*
    * OPTIMIZE TODO: This all could be optimized by keeping track of
    *  the ns-decls currently in-scope via a specialized context.
    */
    if ((nsPrefix == NULL) && ((nsName == NULL) || (nsName[0] == 0))) {
	/*
	* NOTE: the "undeclaration" of the default namespace was
	* part of the logic of the old xsltGetSpecialNamespace() code,
	* so we'll keep that mechanism.
	* Related to the old code: bug #302020:
	*/
	/*
	* OPTIMIZE TODO: This all could be optimized by keeping track of
	*  the ns-decls currently in-scope via a specialized context.
	*/
	/*
	* Search on the result element itself.
	*/
	if (target->nsDef != NULL) {
	    ns = target->nsDef;
	    do {
		if (ns->prefix == NULL) {
		    if ((ns->href != NULL) && (ns->href[0] != 0)) {
			/*
			* Raise a namespace normalization error.
			*/
			xsltTransformError(ctxt, NULL, invocNode,
			    "Namespace normalization error: Cannot undeclare "
			    "the default namespace, since the default namespace "
			    "'%s' is already declared on the result element "
			    "'%s'.\n", ns->href, target->name);
			return(NULL);
		    } else {
			/*
			* The default namespace was undeclared on the
			* result element.
			*/
			return(NULL);
		    }
		    break;
		}
		ns = ns->next;
	    } while (ns != NULL);
	}
	if ((target->parent != NULL) &&
	    (target->parent->type == XML_ELEMENT_NODE))
	{
	    /*
	    * The parent element is in no namespace, so assume
	    * that there is no default namespace in scope.
	    */
	    if (target->parent->ns == NULL)
		return(NULL);

	    ns = xmlSearchNs(target->doc, target->parent,
		NULL);
	    /*
	    * Fine if there's no default ns is scope, or if the
	    * default ns was undeclared.
	    */
	    if ((ns == NULL) || (ns->href == NULL) || (ns->href[0] == 0))
		return(NULL);

	    /*
	    * Undeclare the default namespace.
	    */
	    xmlNewNs(target, BAD_CAST "", NULL);
	    /* TODO: Check result */
	    return(NULL);
	}
	return(NULL);
    }
    /*
    * Handle the XML namespace.
    * QUESTION: Is this faster than using xmlStrEqual() anyway?
    */
    if ((nsPrefix != NULL) &&
	(nsPrefix[0] == 'x') && (nsPrefix[1] == 'm') &&
	(nsPrefix[2] == 'l') && (nsPrefix[3] == 0))
    {
	return(xmlSearchNs(target->doc, target, nsPrefix));
    }
    /*
    * First: search on the result element itself.
    */
    if (target->nsDef != NULL) {
	ns = target->nsDef;
	do {
	    if ((ns->prefix == NULL) == (nsPrefix == NULL)) {
		if (ns->prefix == nsPrefix) {
		    if (xmlStrEqual(ns->href, nsName))
			return(ns);
		    prefixOccupied = 1;
		    break;
		} else if (xmlStrEqual(ns->prefix, nsPrefix)) {
		    if (xmlStrEqual(ns->href, nsName))
			return(ns);
		    prefixOccupied = 1;
		    break;
		}
	    }
	    ns = ns->next;
	} while (ns != NULL);
    }
    if (prefixOccupied) {
	/*
	* If the ns-prefix is occupied by an other ns-decl on the
	* result element, then this means:
	* 1) The desired prefix is shadowed
	* 2) There's no way around changing the prefix
	*
	* Try a desperate search for an in-scope ns-decl
	* with a matching ns-name before we use the last option,
	* which is to recreate the ns-decl with a modified prefix.
	*/
	ns = xmlSearchNsByHref(target->doc, target, nsName);
	if (ns != NULL)
	    return(ns);

	/*
	* Fallback to changing the prefix.
	*/
    } else if ((target->parent != NULL) &&
	(target->parent->type == XML_ELEMENT_NODE))
    {
	/*
	* Try to find a matching ns-decl in the ancestor-axis.
	*
	* Check the common case: The parent element of the current
	* result element is in the same namespace (with an equal ns-prefix).
	*/
	if ((target->parent->ns != NULL) &&
	    ((target->parent->ns->prefix != NULL) == (nsPrefix != NULL)))
	{
	    ns = target->parent->ns;

	    if (nsPrefix == NULL) {
		if (xmlStrEqual(ns->href, nsName))
		    return(ns);
	    } else if (xmlStrEqual(ns->prefix, nsPrefix) &&
		xmlStrEqual(ns->href, nsName))
	    {
		return(ns);
	    }
	}
	/*
	* Lookup the remaining in-scope namespaces.
	*/
	ns = xmlSearchNs(target->doc, target->parent, nsPrefix);
	if (ns != NULL) {
	    if (xmlStrEqual(ns->href, nsName))
		return(ns);
	    /*
	    * Now check for a nasty case: We need to ensure that the new
	    * ns-decl won't shadow a prefix in-use by an existing attribute.
	    * <foo xmlns:a="urn:test:a">
	    *   <bar a:a="val-a">
	    *     <xsl:attribute xmlns:a="urn:test:b" name="a:b">
	    *        val-b</xsl:attribute>
	    *   </bar>
	    * </foo>
	    */
	    if (target->properties) {
		xmlAttrPtr attr = target->properties;
		do {
		    if ((attr->ns) &&
			xmlStrEqual(attr->ns->prefix, nsPrefix))
		    {
			/*
			* Bad, this prefix is already in use.
			* Since we'll change the prefix anyway, try
			* a search for a matching ns-decl based on the
			* namespace name.
			*/
			ns = xmlSearchNsByHref(target->doc, target, nsName);
			if (ns != NULL)
			    return(ns);
			goto declare_new_prefix;
		    }
		    attr = attr->next;
		} while (attr != NULL);
	    }
	} else {
	    /*
	    * Either no matching ns-prefix was found or the namespace is
	    * shadowed.
	    * Create a new ns-decl on the current result element.
	    *
	    * Hmm, we could also try to reuse an in-scope
	    * namespace with a matching ns-name but a different
	    * ns-prefix.
	    * What has higher priority?
	    *  1) If keeping the prefix: create a new ns-decl.
	    *  2) If reusal: first lookup ns-names; then fallback
	    *     to creation of a new ns-decl.
	    * REVISIT: this currently uses case 1) although
	    *  the old way was use xmlSearchNsByHref() and to let change
	    *  the prefix.
	    */
#if 0
	    ns = xmlSearchNsByHref(target->doc, target, nsName);
	    if (ns != NULL)
		return(ns);
#endif
	}
	/*
	* Create the ns-decl on the current result element.
	*/
	ns = xmlNewNs(target, nsName, nsPrefix);
	/* TODO: check errors */
	return(ns);
    } else {
	/*
	* This is either the root of the tree or something weird is going on.
	*/
	ns = xmlNewNs(target, nsName, nsPrefix);
	/* TODO: Check result */
	return(ns);
    }

declare_new_prefix:
    /*
    * Fallback: we need to generate a new prefix and declare the namespace
    * on the result element.
    */
    {
	xmlChar pref[30];
	int counter = 1;

	if (nsPrefix == NULL) {
	    nsPrefix = BAD_CAST "ns";
	}

	do {
	    snprintf((char *) pref, 30, "%s_%d", nsPrefix, counter++);
	    ns = xmlSearchNs(target->doc, target, BAD_CAST pref);
	    if (counter > 1000) {
		xsltTransformError(ctxt, NULL, invocNode,
		    "Internal error in xsltAcquireResultInScopeNs(): "
		    "Failed to compute a unique ns-prefix for the "
		    "generated element");
		return(NULL);
	    }
	} while (ns != NULL);
	ns = xmlNewNs(target, nsName, BAD_CAST pref);
	/* TODO: Check result */
	return(ns);
    }
    return(NULL);
}

/**
 * xsltGetNamespace:
 * @ctxt:  a transformation context
 * @cur:  the input node
 * @ns:  the namespace
 * @out:  the output node (or its parent)
 *
 * Find a matching (prefix and ns-name) ns-declaration
 * for the requested @ns->prefix and @ns->href in the result tree.
 * If none is found then a new ns-declaration will be
 * added to @resultElem. If, in this case, the given prefix is
 * already in use, then a ns-declaration with a modified ns-prefix
 * be we created.
 *
 * Called by:
 *  - xsltCopyPropList() (*not*  anymore)
 *  - xsltShallowCopyElement()
 *  - xsltCopyTreeInternal() (*not*  anymore)
 *  - xsltApplySequenceConstructor() (*not* in the refactored code),
 *  - xsltElement() (*not* anymore)
 *
 * Returns a namespace declaration or NULL in case of
 *         namespace fixup failures or API or internal errors.
 */
xmlNsPtr
xsltGetNamespace(xsltTransformContextPtr ctxt, xmlNodePtr cur, xmlNsPtr ns,
	         xmlNodePtr out)
{

    if (ns == NULL)
	return(NULL);

#ifdef XSLT_REFACTORED
    /*
    * Namespace exclusion and ns-aliasing is performed at
    * compilation-time in the refactored code.
    * Additionally, aliasing is not intended for non Literal
    * Result Elements.
    */
    return(xsltGetSpecialNamespace(ctxt, cur, ns->href, ns->prefix, out));
#else
    {
	xsltStylesheetPtr style;
	const xmlChar *URI = NULL; /* the replacement URI */

	if ((ctxt == NULL) || (cur == NULL) || (out == NULL))
	    return(NULL);

	style = ctxt->style;
	while (style != NULL) {
	    if (style->nsAliases != NULL)
		URI = (const xmlChar *)
		xmlHashLookup(style->nsAliases, ns->href);
	    if (URI != NULL)
		break;

	    style = xsltNextImport(style);
	}


	if (URI == UNDEFINED_DEFAULT_NS) {
	    return(xsltGetSpecialNamespace(ctxt, cur, NULL, NULL, out));
#if 0
	    /*
	    * TODO: Removed, since wrong. If there was no default
	    * namespace in the stylesheet then this must resolve to
	    * the NULL namespace.
	    */
	    xmlNsPtr dflt;
	    dflt = xmlSearchNs(cur->doc, cur, NULL);
	    if (dflt != NULL)
		URI = dflt->href;
	    else
		return NULL;
#endif
	} else if (URI == NULL)
	    URI = ns->href;

	return(xsltGetSpecialNamespace(ctxt, cur, URI, ns->prefix, out));
    }
#endif
}

/**
 * xsltGetPlainNamespace:
 * @ctxt:  a transformation context
 * @cur:  the input node
 * @ns:  the namespace
 * @out:  the result element
 *
 * Obsolete.
 * *Not* called by any Libxslt/Libexslt function.
 * Exaclty the same as xsltGetNamespace().
 *
 * Returns a namespace declaration or NULL in case of
 *         namespace fixup failures or API or internal errors.
 */
xmlNsPtr
xsltGetPlainNamespace(xsltTransformContextPtr ctxt, xmlNodePtr cur,
                      xmlNsPtr ns, xmlNodePtr out)
{
    return(xsltGetNamespace(ctxt, cur, ns, out));
}

/**
 * xsltCopyNamespaceList:
 * @ctxt:  a transformation context
 * @node:  the target node
 * @cur:  the first namespace
 *
 * Do a copy of an namespace list. If @node is non-NULL the
 * new namespaces are added automatically. This handles namespaces
 * aliases.
 * This function is intended only for *internal* use at
 * transformation-time for copying ns-declarations of Literal
 * Result Elements.
 *
 * Called by:
 *   xsltCopyTreeInternal() (transform.c)
 *   xsltShallowCopyElem() (transform.c)
 *
 * REVISIT: This function won't be used in the refactored code.
 *
 * Returns: a new xmlNsPtr, or NULL in case of error.
 */
xmlNsPtr
xsltCopyNamespaceList(xsltTransformContextPtr ctxt, xmlNodePtr node,
	              xmlNsPtr cur) {
    xmlNsPtr ret = NULL, tmp;
    xmlNsPtr p = NULL,q;

    if (cur == NULL)
	return(NULL);
    if (cur->type != XML_NAMESPACE_DECL)
	return(NULL);

    /*
     * One can add namespaces only on element nodes
     */
    if ((node != NULL) && (node->type != XML_ELEMENT_NODE))
	node = NULL;

    while (cur != NULL) {
	if (cur->type != XML_NAMESPACE_DECL)
	    break;

	/*
	 * Avoid duplicating namespace declarations in the tree if
	 * a matching declaration is in scope.
	 */
	if (node != NULL) {
	    if ((node->ns != NULL) &&
		(xmlStrEqual(node->ns->prefix, cur->prefix)) &&
	(xmlStrEqual(node->ns->href, cur->href))) {
		cur = cur->next;
		continue;
	    }
	    tmp = xmlSearchNs(node->doc, node, cur->prefix);
	    if ((tmp != NULL) && (xmlStrEqual(tmp->href, cur->href))) {
		cur = cur->next;
		continue;
	    }
	}
#ifdef XSLT_REFACTORED
	/*
	* Namespace exclusion and ns-aliasing is performed at
	* compilation-time in the refactored code.
	*/
	q = xmlNewNs(node, cur->href, cur->prefix);
	if (p == NULL) {
	    ret = p = q;
	} else {
	    p->next = q;
	    p = q;
	}
#else
	/*
	* TODO: Remove this if the refactored code gets enabled.
	*/
	if (!xmlStrEqual(cur->href, XSLT_NAMESPACE)) {
	    const xmlChar *URI;
	    /* TODO apply cascading */
	    URI = (const xmlChar *) xmlHashLookup(ctxt->style->nsAliases,
		                                  cur->href);
	    if (URI == UNDEFINED_DEFAULT_NS) {
		cur = cur->next;
	        continue;
	    }
	    if (URI != NULL) {
		q = xmlNewNs(node, URI, cur->prefix);
	    } else {
		q = xmlNewNs(node, cur->href, cur->prefix);
	    }
	    if (p == NULL) {
		ret = p = q;
	    } else {
		p->next = q;
		p = q;
	    }
	}
#endif
	cur = cur->next;
    }
    return(ret);
}

/**
 * xsltCopyNamespace:
 * @ctxt:  a transformation context
 * @elem:  the target element node
 * @ns:  the namespace node
 *
 * Copies a namespace node (declaration). If @elem is not NULL,
 * then the new namespace will be declared on @elem.
 *
 * Returns: a new xmlNsPtr, or NULL in case of an error.
 */
xmlNsPtr
xsltCopyNamespace(xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED,
		  xmlNodePtr elem, xmlNsPtr ns)
{
    if ((ns == NULL) || (ns->type != XML_NAMESPACE_DECL))
	return(NULL);
    /*
     * One can add namespaces only on element nodes
     */
    if ((elem != NULL) && (elem->type != XML_ELEMENT_NODE))
	return(xmlNewNs(NULL, ns->href, ns->prefix));
    else
	return(xmlNewNs(elem, ns->href, ns->prefix));
}


/**
 * xsltFreeNamespaceAliasHashes:
 * @style: an XSLT stylesheet
 *
 * Free up the memory used by namespaces aliases
 */
void
xsltFreeNamespaceAliasHashes(xsltStylesheetPtr style) {
    if (style->nsAliases != NULL)
	xmlHashFree((xmlHashTablePtr) style->nsAliases, NULL);
    style->nsAliases = NULL;
}
