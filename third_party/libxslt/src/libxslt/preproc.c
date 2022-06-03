/*
 * preproc.c: Preprocessing of style operations
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

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/hash.h>
#include <libxml/uri.h>
#include <libxml/encoding.h>
#include <libxml/xmlerror.h>
#include "xslt.h"
#include "xsltutils.h"
#include "xsltInternals.h"
#include "transform.h"
#include "templates.h"
#include "variables.h"
#include "numbersInternals.h"
#include "preproc.h"
#include "extra.h"
#include "imports.h"
#include "extensions.h"
#include "pattern.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_PREPROC
#endif

const xmlChar *xsltExtMarker = (const xmlChar *) "Extension Element";

/************************************************************************
 *									*
 *			Grammar checks					*
 *									*
 ************************************************************************/

#ifdef XSLT_REFACTORED
    /*
    * Grammar checks are now performed in xslt.c.
    */
#else
/**
 * xsltCheckTopLevelElement:
 * @style: the XSLT stylesheet
 * @inst: the XSLT instruction
 * @err: raise an error or not
 *
 * Check that the instruction is instanciated as a top level element.
 *
 * Returns -1 in case of error, 0 if failed and 1 in case of success
 */
static int
xsltCheckTopLevelElement(xsltStylesheetPtr style, xmlNodePtr inst, int err) {
    xmlNodePtr parent;
    if ((style == NULL) || (inst == NULL) || (inst->ns == NULL))
        return(-1);

    parent = inst->parent;
    if (parent == NULL) {
        if (err) {
	    xsltTransformError(NULL, style, inst,
		    "internal problem: element has no parent\n");
	    style->errors++;
	}
	return(0);
    }
    if ((parent->ns == NULL) || (parent->type != XML_ELEMENT_NODE) ||
        ((parent->ns != inst->ns) &&
	 (!xmlStrEqual(parent->ns->href, inst->ns->href))) ||
	((!xmlStrEqual(parent->name, BAD_CAST "stylesheet")) &&
	 (!xmlStrEqual(parent->name, BAD_CAST "transform")))) {
	if (err) {
	    xsltTransformError(NULL, style, inst,
		    "element %s only allowed as child of stylesheet\n",
			       inst->name);
	    style->errors++;
	}
	return(0);
    }
    return(1);
}

/**
 * xsltCheckInstructionElement:
 * @style: the XSLT stylesheet
 * @inst: the XSLT instruction
 *
 * Check that the instruction is instanciated as an instruction element.
 */
static void
xsltCheckInstructionElement(xsltStylesheetPtr style, xmlNodePtr inst) {
    xmlNodePtr parent;
    int has_ext;

    if ((style == NULL) || (inst == NULL) || (inst->ns == NULL) ||
        (style->literal_result))
        return;

    has_ext = (style->extInfos != NULL);

    parent = inst->parent;
    if (parent == NULL) {
	xsltTransformError(NULL, style, inst,
		"internal problem: element has no parent\n");
	style->errors++;
	return;
    }
    while ((parent != NULL) && (parent->type != XML_DOCUMENT_NODE)) {
        if (((parent->ns == inst->ns) ||
	     ((parent->ns != NULL) &&
	      (xmlStrEqual(parent->ns->href, inst->ns->href)))) &&
	    ((xmlStrEqual(parent->name, BAD_CAST "template")) ||
	     (xmlStrEqual(parent->name, BAD_CAST "param")) ||
	     (xmlStrEqual(parent->name, BAD_CAST "attribute")) ||
	     (xmlStrEqual(parent->name, BAD_CAST "variable")))) {
	    return;
	}

	/*
	 * if we are within an extension element all bets are off
	 * about the semantic there e.g. xsl:param within func:function
	 */
	if ((has_ext) && (parent->ns != NULL) &&
	    (xmlHashLookup(style->extInfos, parent->ns->href) != NULL))
	    return;

        parent = parent->parent;
    }
    xsltTransformError(NULL, style, inst,
	    "element %s only allowed within a template, variable or param\n",
		           inst->name);
    style->errors++;
}

/**
 * xsltCheckParentElement:
 * @style: the XSLT stylesheet
 * @inst: the XSLT instruction
 * @allow1: allowed parent1
 * @allow2: allowed parent2
 *
 * Check that the instruction is instanciated as the childre of one of the
 * possible parents.
 */
static void
xsltCheckParentElement(xsltStylesheetPtr style, xmlNodePtr inst,
                       const xmlChar *allow1, const xmlChar *allow2) {
    xmlNodePtr parent;

    if ((style == NULL) || (inst == NULL) || (inst->ns == NULL) ||
        (style->literal_result))
        return;

    parent = inst->parent;
    if (parent == NULL) {
	xsltTransformError(NULL, style, inst,
		"internal problem: element has no parent\n");
	style->errors++;
	return;
    }
    if (((parent->ns == inst->ns) ||
	 ((parent->ns != NULL) &&
	  (xmlStrEqual(parent->ns->href, inst->ns->href)))) &&
	((xmlStrEqual(parent->name, allow1)) ||
	 (xmlStrEqual(parent->name, allow2)))) {
	return;
    }

    if (style->extInfos != NULL) {
	while ((parent != NULL) && (parent->type != XML_DOCUMENT_NODE)) {
	    /*
	     * if we are within an extension element all bets are off
	     * about the semantic there e.g. xsl:param within func:function
	     */
	    if ((parent->ns != NULL) &&
		(xmlHashLookup(style->extInfos, parent->ns->href) != NULL))
		return;

	    parent = parent->parent;
	}
    }
    xsltTransformError(NULL, style, inst,
		       "element %s is not allowed within that context\n",
		       inst->name);
    style->errors++;
}
#endif

/************************************************************************
 *									*
 *			handling of precomputed data			*
 *									*
 ************************************************************************/

/**
 * xsltNewStylePreComp:
 * @style:  the XSLT stylesheet
 * @type:  the construct type
 *
 * Create a new XSLT Style precomputed block
 *
 * Returns the newly allocated specialized structure
 *         or NULL in case of error
 */
static xsltStylePreCompPtr
xsltNewStylePreComp(xsltStylesheetPtr style, xsltStyleType type) {
    xsltStylePreCompPtr cur;
#ifdef XSLT_REFACTORED
    size_t size;
#endif

    if (style == NULL)
        return(NULL);

#ifdef XSLT_REFACTORED
    /*
    * URGENT TODO: Use specialized factory functions in order
    *   to avoid this ugliness.
    */
    switch (type) {
        case XSLT_FUNC_COPY:
            size = sizeof(xsltStyleItemCopy); break;
        case XSLT_FUNC_SORT:
            size = sizeof(xsltStyleItemSort); break;
        case XSLT_FUNC_TEXT:
            size = sizeof(xsltStyleItemText); break;
        case XSLT_FUNC_ELEMENT:
            size = sizeof(xsltStyleItemElement); break;
        case XSLT_FUNC_ATTRIBUTE:
            size = sizeof(xsltStyleItemAttribute); break;
        case XSLT_FUNC_COMMENT:
            size = sizeof(xsltStyleItemComment); break;
        case XSLT_FUNC_PI:
            size = sizeof(xsltStyleItemPI); break;
        case XSLT_FUNC_COPYOF:
            size = sizeof(xsltStyleItemCopyOf); break;
        case XSLT_FUNC_VALUEOF:
            size = sizeof(xsltStyleItemValueOf); break;;
        case XSLT_FUNC_NUMBER:
            size = sizeof(xsltStyleItemNumber); break;
        case XSLT_FUNC_APPLYIMPORTS:
            size = sizeof(xsltStyleItemApplyImports); break;
        case XSLT_FUNC_CALLTEMPLATE:
            size = sizeof(xsltStyleItemCallTemplate); break;
        case XSLT_FUNC_APPLYTEMPLATES:
            size = sizeof(xsltStyleItemApplyTemplates); break;
        case XSLT_FUNC_CHOOSE:
            size = sizeof(xsltStyleItemChoose); break;
        case XSLT_FUNC_IF:
            size = sizeof(xsltStyleItemIf); break;
        case XSLT_FUNC_FOREACH:
            size = sizeof(xsltStyleItemForEach); break;
        case XSLT_FUNC_DOCUMENT:
            size = sizeof(xsltStyleItemDocument); break;
	case XSLT_FUNC_WITHPARAM:
	    size = sizeof(xsltStyleItemWithParam); break;
	case XSLT_FUNC_PARAM:
	    size = sizeof(xsltStyleItemParam); break;
	case XSLT_FUNC_VARIABLE:
	    size = sizeof(xsltStyleItemVariable); break;
	case XSLT_FUNC_WHEN:
	    size = sizeof(xsltStyleItemWhen); break;
	case XSLT_FUNC_OTHERWISE:
	    size = sizeof(xsltStyleItemOtherwise); break;
	default:
	    xsltTransformError(NULL, style, NULL,
		    "xsltNewStylePreComp : invalid type %d\n", type);
	    style->errors++;
	    return(NULL);
    }
    /*
    * Create the structure.
    */
    cur = (xsltStylePreCompPtr) xmlMalloc(size);
    if (cur == NULL) {
	xsltTransformError(NULL, style, NULL,
		"xsltNewStylePreComp : malloc failed\n");
	style->errors++;
	return(NULL);
    }
    memset(cur, 0, size);

#else /* XSLT_REFACTORED */
    /*
    * Old behaviour.
    */
    cur = (xsltStylePreCompPtr) xmlMalloc(sizeof(xsltStylePreComp));
    if (cur == NULL) {
	xsltTransformError(NULL, style, NULL,
		"xsltNewStylePreComp : malloc failed\n");
	style->errors++;
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltStylePreComp));
#endif /* XSLT_REFACTORED */

    /*
    * URGENT TODO: Better to move this to spezialized factory functions.
    */
    cur->type = type;
    switch (cur->type) {
        case XSLT_FUNC_COPY:
            cur->func = xsltCopy;break;
        case XSLT_FUNC_SORT:
            cur->func = xsltSort;break;
        case XSLT_FUNC_TEXT:
            cur->func = xsltText;break;
        case XSLT_FUNC_ELEMENT:
            cur->func = xsltElement;break;
        case XSLT_FUNC_ATTRIBUTE:
            cur->func = xsltAttribute;break;
        case XSLT_FUNC_COMMENT:
            cur->func = xsltComment;break;
        case XSLT_FUNC_PI:
            cur->func = xsltProcessingInstruction;
	    break;
        case XSLT_FUNC_COPYOF:
            cur->func = xsltCopyOf;break;
        case XSLT_FUNC_VALUEOF:
            cur->func = xsltValueOf;break;
        case XSLT_FUNC_NUMBER:
            cur->func = xsltNumber;break;
        case XSLT_FUNC_APPLYIMPORTS:
            cur->func = xsltApplyImports;break;
        case XSLT_FUNC_CALLTEMPLATE:
            cur->func = xsltCallTemplate;break;
        case XSLT_FUNC_APPLYTEMPLATES:
            cur->func = xsltApplyTemplates;break;
        case XSLT_FUNC_CHOOSE:
            cur->func = xsltChoose;break;
        case XSLT_FUNC_IF:
            cur->func = xsltIf;break;
        case XSLT_FUNC_FOREACH:
            cur->func = xsltForEach;break;
        case XSLT_FUNC_DOCUMENT:
            cur->func = xsltDocumentElem;break;
	case XSLT_FUNC_WITHPARAM:
	case XSLT_FUNC_PARAM:
	case XSLT_FUNC_VARIABLE:
	case XSLT_FUNC_WHEN:
	    break;
	default:
	if (cur->func == NULL) {
	    xsltTransformError(NULL, style, NULL,
		    "xsltNewStylePreComp : no function for type %d\n", type);
	    style->errors++;
	}
    }
    cur->next = style->preComps;
    style->preComps = (xsltElemPreCompPtr) cur;

    return(cur);
}

/**
 * xsltFreeStylePreComp:
 * @comp:  an XSLT Style precomputed block
 *
 * Free up the memory allocated by @comp
 */
static void
xsltFreeStylePreComp(xsltStylePreCompPtr comp) {
    if (comp == NULL)
	return;
#ifdef XSLT_REFACTORED
    /*
    * URGENT TODO: Implement destructors.
    */
    switch (comp->type) {
	case XSLT_FUNC_LITERAL_RESULT_ELEMENT:
	    break;
	case XSLT_FUNC_COPY:
            break;
        case XSLT_FUNC_SORT: {
		xsltStyleItemSortPtr item = (xsltStyleItemSortPtr) comp;
		if (item->locale != (xsltLocale)0)
		    xsltFreeLocale(item->locale);
		if (item->comp != NULL)
		    xmlXPathFreeCompExpr(item->comp);
	    }
            break;
        case XSLT_FUNC_TEXT:
            break;
        case XSLT_FUNC_ELEMENT:
            break;
        case XSLT_FUNC_ATTRIBUTE:
            break;
        case XSLT_FUNC_COMMENT:
            break;
        case XSLT_FUNC_PI:
	    break;
        case XSLT_FUNC_COPYOF: {
		xsltStyleItemCopyOfPtr item = (xsltStyleItemCopyOfPtr) comp;
		if (item->comp != NULL)
		    xmlXPathFreeCompExpr(item->comp);
	    }
            break;
        case XSLT_FUNC_VALUEOF: {
		xsltStyleItemValueOfPtr item = (xsltStyleItemValueOfPtr) comp;
		if (item->comp != NULL)
		    xmlXPathFreeCompExpr(item->comp);
	    }
            break;
        case XSLT_FUNC_NUMBER: {
                xsltStyleItemNumberPtr item = (xsltStyleItemNumberPtr) comp;
                if (item->numdata.countPat != NULL)
                    xsltFreeCompMatchList(item->numdata.countPat);
                if (item->numdata.fromPat != NULL)
                    xsltFreeCompMatchList(item->numdata.fromPat);
            }
            break;
        case XSLT_FUNC_APPLYIMPORTS:
            break;
        case XSLT_FUNC_CALLTEMPLATE:
            break;
        case XSLT_FUNC_APPLYTEMPLATES: {
		xsltStyleItemApplyTemplatesPtr item =
		    (xsltStyleItemApplyTemplatesPtr) comp;
		if (item->comp != NULL)
		    xmlXPathFreeCompExpr(item->comp);
	    }
            break;
        case XSLT_FUNC_CHOOSE:
            break;
        case XSLT_FUNC_IF: {
		xsltStyleItemIfPtr item = (xsltStyleItemIfPtr) comp;
		if (item->comp != NULL)
		    xmlXPathFreeCompExpr(item->comp);
	    }
            break;
        case XSLT_FUNC_FOREACH: {
		xsltStyleItemForEachPtr item =
		    (xsltStyleItemForEachPtr) comp;
		if (item->comp != NULL)
		    xmlXPathFreeCompExpr(item->comp);
	    }
            break;
        case XSLT_FUNC_DOCUMENT:
            break;
	case XSLT_FUNC_WITHPARAM: {
		xsltStyleItemWithParamPtr item =
		    (xsltStyleItemWithParamPtr) comp;
		if (item->comp != NULL)
		    xmlXPathFreeCompExpr(item->comp);
	    }
	    break;
	case XSLT_FUNC_PARAM: {
		xsltStyleItemParamPtr item =
		    (xsltStyleItemParamPtr) comp;
		if (item->comp != NULL)
		    xmlXPathFreeCompExpr(item->comp);
	    }
	    break;
	case XSLT_FUNC_VARIABLE: {
		xsltStyleItemVariablePtr item =
		    (xsltStyleItemVariablePtr) comp;
		if (item->comp != NULL)
		    xmlXPathFreeCompExpr(item->comp);
	    }
	    break;
	case XSLT_FUNC_WHEN: {
		xsltStyleItemWhenPtr item =
		    (xsltStyleItemWhenPtr) comp;
		if (item->comp != NULL)
		    xmlXPathFreeCompExpr(item->comp);
	    }
	    break;
	case XSLT_FUNC_OTHERWISE:
	case XSLT_FUNC_FALLBACK:
	case XSLT_FUNC_MESSAGE:
	case XSLT_FUNC_INCLUDE:
	case XSLT_FUNC_ATTRSET:

	    break;
	default:
	    /* TODO: Raise error. */
	    break;
    }
#else
    if (comp->locale != (xsltLocale)0)
	xsltFreeLocale(comp->locale);
    if (comp->comp != NULL)
	xmlXPathFreeCompExpr(comp->comp);
    if (comp->numdata.countPat != NULL)
        xsltFreeCompMatchList(comp->numdata.countPat);
    if (comp->numdata.fromPat != NULL)
        xsltFreeCompMatchList(comp->numdata.fromPat);
    if (comp->nsList != NULL)
	xmlFree(comp->nsList);
#endif

    xmlFree(comp);
}


/************************************************************************
 *									*
 *		    XSLT-1.1 extensions					*
 *									*
 ************************************************************************/

/**
 * xsltDocumentComp:
 * @style:  the XSLT stylesheet
 * @inst:  the instruction in the stylesheet
 * @function:  unused
 *
 * Pre process an XSLT-1.1 document element
 *
 * Returns a precompiled data structure for the element
 */
xsltElemPreCompPtr
xsltDocumentComp(xsltStylesheetPtr style, xmlNodePtr inst,
		 xsltTransformFunction function ATTRIBUTE_UNUSED) {
#ifdef XSLT_REFACTORED
    xsltStyleItemDocumentPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif
    const xmlChar *filename = NULL;

    /*
    * As of 2006-03-30, this function is currently defined in Libxslt
    * to be used for:
    * (in libxslt/extra.c)
    * "output" in XSLT_SAXON_NAMESPACE
    * "write" XSLT_XALAN_NAMESPACE
    * "document" XSLT_XT_NAMESPACE
    * "document" XSLT_NAMESPACE (from the abandoned old working
    *                            draft of XSLT 1.1)
    * (in libexslt/common.c)
    * "document" in EXSLT_COMMON_NAMESPACE
    */
#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemDocumentPtr)
	xsltNewStylePreComp(style, XSLT_FUNC_DOCUMENT);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_DOCUMENT);
#endif

    if (comp == NULL)
	return (NULL);
    comp->inst = inst;
    comp->ver11 = 0;

    if (xmlStrEqual(inst->name, (const xmlChar *) "output")) {
#ifdef WITH_XSLT_DEBUG_EXTRA
	xsltGenericDebug(xsltGenericDebugContext,
	    "Found saxon:output extension\n");
#endif
	/*
	* The element "output" is in the namespace XSLT_SAXON_NAMESPACE
	*   (http://icl.com/saxon)
	* The @file is in no namespace; it is an AVT.
	*   (http://www.computerwizards.com/saxon/doc/extensions.html#saxon:output)
	*
	* TODO: Do we need not to check the namespace here?
	*/
	filename = xsltEvalStaticAttrValueTemplate(style, inst,
			 (const xmlChar *)"file",
			 NULL, &comp->has_filename);
    } else if (xmlStrEqual(inst->name, (const xmlChar *) "write")) {
#ifdef WITH_XSLT_DEBUG_EXTRA
	xsltGenericDebug(xsltGenericDebugContext,
	    "Found xalan:write extension\n");
#endif
	/* the filename need to be interpreted */
	/*
	* TODO: Is "filename need to be interpreted" meant to be a todo?
	*   Where will be the filename of xalan:write be processed?
	*
	* TODO: Do we need not to check the namespace here?
	*   The extension ns is "http://xml.apache.org/xalan/redirect".
	*   See http://xml.apache.org/xalan-j/extensionslib.html.
	*/
    } else if (xmlStrEqual(inst->name, (const xmlChar *) "document")) {
	if (inst->ns != NULL) {
	    if (xmlStrEqual(inst->ns->href, XSLT_NAMESPACE)) {
		/*
		* Mark the instruction as being of
		* XSLT version 1.1 (abandoned).
		*/
		comp->ver11 = 1;
#ifdef WITH_XSLT_DEBUG_EXTRA
		xsltGenericDebug(xsltGenericDebugContext,
		    "Found xslt11:document construct\n");
#endif
	    } else {
		if (xmlStrEqual(inst->ns->href,
		    (const xmlChar *)"http://exslt.org/common")) {
		    /* EXSLT. */
#ifdef WITH_XSLT_DEBUG_EXTRA
		    xsltGenericDebug(xsltGenericDebugContext,
			"Found exslt:document extension\n");
#endif
		} else if (xmlStrEqual(inst->ns->href, XSLT_XT_NAMESPACE)) {
		    /* James Clark's XT. */
#ifdef WITH_XSLT_DEBUG_EXTRA
		    xsltGenericDebug(xsltGenericDebugContext,
			"Found xt:document extension\n");
#endif
		}
	    }
	}
	/*
	* The element "document" is used in conjunction with the
	* following namespaces:
	*
	* 1) XSLT_NAMESPACE (http://www.w3.org/1999/XSL/Transform version 1.1)
	*    <!ELEMENT xsl:document %template;>
	*    <!ATTLIST xsl:document
	*       href %avt; #REQUIRED
	*    @href is an AVT
	*    IMPORTANT: xsl:document was in the abandoned XSLT 1.1 draft,
	*    it was removed and isn't available in XSLT 1.1 anymore.
	*    In XSLT 2.0 it was renamed to xsl:result-document.
	*
	*   All other attributes are identical to the attributes
	*   on xsl:output
	*
	* 2) EXSLT_COMMON_NAMESPACE (http://exslt.org/common)
	*    <exsl:document
	*       href = { uri-reference }
	*    TODO: is @href is an AVT?
	*
	* 3) XSLT_XT_NAMESPACE (http://www.jclark.com/xt)
	*     Example: <xt:document method="xml" href="myFile.xml">
	*    TODO: is @href is an AVT?
	*
	* In all cases @href is in no namespace.
	*/
	filename = xsltEvalStaticAttrValueTemplate(style, inst,
	    (const xmlChar *)"href", NULL, &comp->has_filename);
    }
    if (!comp->has_filename) {
	goto error;
    }
    comp->filename = filename;

error:
    return ((xsltElemPreCompPtr) comp);
}

/************************************************************************
 *									*
 *		Most of the XSLT-1.0 transformations			*
 *									*
 ************************************************************************/

/**
 * xsltSortComp:
 * @style:  the XSLT stylesheet
 * @inst:  the xslt sort node
 *
 * Process the xslt sort node on the source node
 */
static void
xsltSortComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemSortPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif
    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemSortPtr) xsltNewStylePreComp(style, XSLT_FUNC_SORT);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_SORT);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->stype = xsltEvalStaticAttrValueTemplate(style, inst,
			 (const xmlChar *)"data-type",
			 NULL, &comp->has_stype);
    if (comp->stype != NULL) {
	if (xmlStrEqual(comp->stype, (const xmlChar *) "text"))
	    comp->number = 0;
	else if (xmlStrEqual(comp->stype, (const xmlChar *) "number"))
	    comp->number = 1;
	else {
	    xsltTransformError(NULL, style, inst,
		 "xsltSortComp: no support for data-type = %s\n", comp->stype);
	    comp->number = 0; /* use default */
	    if (style != NULL) style->warnings++;
	}
    }
    comp->order = xsltEvalStaticAttrValueTemplate(style, inst,
			      (const xmlChar *)"order",
			      NULL, &comp->has_order);
    if (comp->order != NULL) {
	if (xmlStrEqual(comp->order, (const xmlChar *) "ascending"))
	    comp->descending = 0;
	else if (xmlStrEqual(comp->order, (const xmlChar *) "descending"))
	    comp->descending = 1;
	else {
	    xsltTransformError(NULL, style, inst,
		 "xsltSortComp: invalid value %s for order\n", comp->order);
	    comp->descending = 0; /* use default */
	    if (style != NULL) style->warnings++;
	}
    }
    comp->case_order = xsltEvalStaticAttrValueTemplate(style, inst,
			      (const xmlChar *)"case-order",
			      NULL, &comp->has_use);
    if (comp->case_order != NULL) {
	if (xmlStrEqual(comp->case_order, (const xmlChar *) "upper-first"))
	    comp->lower_first = 0;
	else if (xmlStrEqual(comp->case_order, (const xmlChar *) "lower-first"))
	    comp->lower_first = 1;
	else {
	    xsltTransformError(NULL, style, inst,
		 "xsltSortComp: invalid value %s for order\n", comp->order);
	    comp->lower_first = 0; /* use default */
	    if (style != NULL) style->warnings++;
	}
    }

    comp->lang = xsltEvalStaticAttrValueTemplate(style, inst,
				 (const xmlChar *)"lang",
				 NULL, &comp->has_lang);
    if (comp->lang != NULL) {
	comp->locale = xsltNewLocale(comp->lang);
    }
    else {
        comp->locale = (xsltLocale)0;
    }

    comp->select = xsltGetCNsProp(style, inst,(const xmlChar *)"select", XSLT_NAMESPACE);
    if (comp->select == NULL) {
	/*
	 * The default value of the select attribute is ., which will
	 * cause the string-value of the current node to be used as
	 * the sort key.
	 */
	comp->select = xmlDictLookup(style->dict, BAD_CAST ".", 1);
    }
    comp->comp = xsltXPathCompile(style, comp->select);
    if (comp->comp == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsltSortComp: could not compile select expression '%s'\n",
	                 comp->select);
	if (style != NULL) style->errors++;
    }
    if (inst->children != NULL) {
	xsltTransformError(NULL, style, inst,
	"xsl:sort : is not empty\n");
	if (style != NULL) style->errors++;
    }
}

/**
 * xsltCopyComp:
 * @style:  the XSLT stylesheet
 * @inst:  the xslt copy node
 *
 * Process the xslt copy node on the source node
 */
static void
xsltCopyComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemCopyPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;
#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemCopyPtr) xsltNewStylePreComp(style, XSLT_FUNC_COPY);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_COPY);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;


    comp->use = xsltGetCNsProp(style, inst, (const xmlChar *)"use-attribute-sets",
				    XSLT_NAMESPACE);
    if (comp->use == NULL)
	comp->has_use = 0;
    else
	comp->has_use = 1;
}

#ifdef XSLT_REFACTORED
    /* Enable if ever needed for xsl:text. */
#else
/**
 * xsltTextComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt text node
 *
 * TODO: This function is obsolete, since xsl:text won't
 *  be compiled, but removed from the tree.
 *
 * Process the xslt text node on the source node
 */
static void
xsltTextComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemTextPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif
    const xmlChar *prop;

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemTextPtr) xsltNewStylePreComp(style, XSLT_FUNC_TEXT);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_TEXT);
#endif
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;
    comp->noescape = 0;

    prop = xsltGetCNsProp(style, inst,
	    (const xmlChar *)"disable-output-escaping",
			XSLT_NAMESPACE);
    if (prop != NULL) {
	if (xmlStrEqual(prop, (const xmlChar *)"yes")) {
	    comp->noescape = 1;
	} else if (!xmlStrEqual(prop,
	    (const xmlChar *)"no")){
	    xsltTransformError(NULL, style, inst,
		"xsl:text: disable-output-escaping allows only yes or no\n");
	    if (style != NULL) style->warnings++;
	}
    }
}
#endif /* else of XSLT_REFACTORED */

/**
 * xsltElementComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt element node
 *
 * Process the xslt element node on the source node
 */
static void
xsltElementComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemElementPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    /*
    * <xsl:element
    *   name = { qname }
    *   namespace = { uri-reference }
    *   use-attribute-sets = qnames>
    *   <!-- Content: template -->
    * </xsl:element>
    */
    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemElementPtr) xsltNewStylePreComp(style, XSLT_FUNC_ELEMENT);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_ELEMENT);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
    * Attribute "name".
    */
    /*
    * TODO: Precompile the AVT. See bug #344894.
    */
    comp->name = xsltEvalStaticAttrValueTemplate(style, inst,
	(const xmlChar *)"name", NULL, &comp->has_name);
    if (! comp->has_name) {
	xsltTransformError(NULL, style, inst,
	    "xsl:element: The attribute 'name' is missing.\n");
	style->errors++;
	goto error;
    }
    /*
    * Attribute "namespace".
    */
    /*
    * TODO: Precompile the AVT. See bug #344894.
    */
    comp->ns = xsltEvalStaticAttrValueTemplate(style, inst,
	(const xmlChar *)"namespace", NULL, &comp->has_ns);

    if (comp->name != NULL) {
	if (xmlValidateQName(comp->name, 0)) {
	    xsltTransformError(NULL, style, inst,
		"xsl:element: The value '%s' of the attribute 'name' is "
		"not a valid QName.\n", comp->name);
	    style->errors++;
	} else {
	    const xmlChar *prefix = NULL, *name;

	    name = xsltSplitQName(style->dict, comp->name, &prefix);
	    if (comp->has_ns == 0) {
		xmlNsPtr ns;

		/*
		* SPEC XSLT 1.0:
		*  "If the namespace attribute is not present, then the QName is
		*  expanded into an expanded-name using the namespace declarations
		*  in effect for the xsl:element element, including any default
		*  namespace declaration.
		*/
		ns = xmlSearchNs(inst->doc, inst, prefix);
		if (ns != NULL) {
		    comp->ns = xmlDictLookup(style->dict, ns->href, -1);
		    comp->has_ns = 1;
#ifdef XSLT_REFACTORED
		    comp->nsPrefix = prefix;
		    comp->name = name;
#else
                    (void)name; /* Suppress unused variable warning. */
#endif
		} else if (prefix != NULL) {
		    xsltTransformError(NULL, style, inst,
			"xsl:element: The prefixed QName '%s' "
			"has no namespace binding in scope in the "
			"stylesheet; this is an error, since the namespace was "
			"not specified by the instruction itself.\n", comp->name);
		    style->errors++;
		}
	    }
	    if ((prefix != NULL) &&
		(!xmlStrncasecmp(prefix, (xmlChar *)"xml", 3)))
	    {
		/*
		* Mark is to be skipped.
		*/
		comp->has_name = 0;
	    }
	}
    }
    /*
    * Attribute "use-attribute-sets",
    */
    comp->use = xsltEvalStaticAttrValueTemplate(style, inst,
		       (const xmlChar *)"use-attribute-sets",
		       NULL, &comp->has_use);

error:
    return;
}

/**
 * xsltAttributeComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt attribute node
 *
 * Process the xslt attribute node on the source node
 */
static void
xsltAttributeComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemAttributePtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    /*
    * <xsl:attribute
    *   name = { qname }
    *   namespace = { uri-reference }>
    *   <!-- Content: template -->
    * </xsl:attribute>
    */
    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemAttributePtr) xsltNewStylePreComp(style,
	XSLT_FUNC_ATTRIBUTE);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_ATTRIBUTE);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
    * Attribute "name".
    */
    /*
    * TODO: Precompile the AVT. See bug #344894.
    */
    comp->name = xsltEvalStaticAttrValueTemplate(style, inst,
				 (const xmlChar *)"name",
				 NULL, &comp->has_name);
    if (! comp->has_name) {
	xsltTransformError(NULL, style, inst,
	    "XSLT-attribute: The attribute 'name' is missing.\n");
	style->errors++;
	return;
    }
    /*
    * Attribute "namespace".
    */
    /*
    * TODO: Precompile the AVT. See bug #344894.
    */
    comp->ns = xsltEvalStaticAttrValueTemplate(style, inst,
	(const xmlChar *)"namespace",
	NULL, &comp->has_ns);

    if (comp->name != NULL) {
	if (xmlValidateQName(comp->name, 0)) {
	    xsltTransformError(NULL, style, inst,
		"xsl:attribute: The value '%s' of the attribute 'name' is "
		"not a valid QName.\n", comp->name);
	    style->errors++;
        } else if (xmlStrEqual(comp->name, BAD_CAST "xmlns")) {
	    xsltTransformError(NULL, style, inst,
                "xsl:attribute: The attribute name 'xmlns' is not allowed.\n");
	    style->errors++;
	} else {
	    const xmlChar *prefix = NULL, *name;

	    name = xsltSplitQName(style->dict, comp->name, &prefix);
	    if (prefix != NULL) {
		if (comp->has_ns == 0) {
		    xmlNsPtr ns;

		    /*
		    * SPEC XSLT 1.0:
		    *  "If the namespace attribute is not present, then the
		    *  QName is expanded into an expanded-name using the
		    *  namespace declarations in effect for the xsl:element
		    *  element, including any default namespace declaration.
		    */
		    ns = xmlSearchNs(inst->doc, inst, prefix);
		    if (ns != NULL) {
			comp->ns = xmlDictLookup(style->dict, ns->href, -1);
			comp->has_ns = 1;
#ifdef XSLT_REFACTORED
			comp->nsPrefix = prefix;
			comp->name = name;
#else
                        (void)name; /* Suppress unused variable warning. */
#endif
		    } else {
			xsltTransformError(NULL, style, inst,
			    "xsl:attribute: The prefixed QName '%s' "
			    "has no namespace binding in scope in the "
			    "stylesheet; this is an error, since the "
			    "namespace was not specified by the instruction "
			    "itself.\n", comp->name);
			style->errors++;
		    }
		}
	    }
	}
    }
}

/**
 * xsltCommentComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt comment node
 *
 * Process the xslt comment node on the source node
 */
static void
xsltCommentComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemCommentPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemCommentPtr) xsltNewStylePreComp(style, XSLT_FUNC_COMMENT);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_COMMENT);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;
}

/**
 * xsltProcessingInstructionComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt processing-instruction node
 *
 * Process the xslt processing-instruction node on the source node
 */
static void
xsltProcessingInstructionComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemPIPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemPIPtr) xsltNewStylePreComp(style, XSLT_FUNC_PI);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_PI);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->name = xsltEvalStaticAttrValueTemplate(style, inst,
				 (const xmlChar *)"name",
				 XSLT_NAMESPACE, &comp->has_name);
}

/**
 * xsltCopyOfComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt copy-of node
 *
 * Process the xslt copy-of node on the source node
 */
static void
xsltCopyOfComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemCopyOfPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemCopyOfPtr) xsltNewStylePreComp(style, XSLT_FUNC_COPYOF);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_COPYOF);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:copy-of : select is missing\n");
	if (style != NULL) style->errors++;
	return;
    }
    comp->comp = xsltXPathCompile(style, comp->select);
    if (comp->comp == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:copy-of : could not compile select expression '%s'\n",
	                 comp->select);
	if (style != NULL) style->errors++;
    }
}

/**
 * xsltValueOfComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt value-of node
 *
 * Process the xslt value-of node on the source node
 */
static void
xsltValueOfComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemValueOfPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif
    const xmlChar *prop;

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemValueOfPtr) xsltNewStylePreComp(style, XSLT_FUNC_VALUEOF);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_VALUEOF);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    prop = xsltGetCNsProp(style, inst,
	    (const xmlChar *)"disable-output-escaping",
			XSLT_NAMESPACE);
    if (prop != NULL) {
	if (xmlStrEqual(prop, (const xmlChar *)"yes")) {
	    comp->noescape = 1;
	} else if (!xmlStrEqual(prop,
				(const xmlChar *)"no")){
	    xsltTransformError(NULL, style, inst,
"xsl:value-of : disable-output-escaping allows only yes or no\n");
	    if (style != NULL) style->warnings++;
	}
    }
    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:value-of : select is missing\n");
	if (style != NULL) style->errors++;
	return;
    }
    comp->comp = xsltXPathCompile(style, comp->select);
    if (comp->comp == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:value-of : could not compile select expression '%s'\n",
	                 comp->select);
	if (style != NULL) style->errors++;
    }
}

static void
xsltGetQNameProperty(xsltStylesheetPtr style, xmlNodePtr inst,
		     const xmlChar *propName,
		     int mandatory,
		     int *hasProp, const xmlChar **nsName,
		     const xmlChar** localName)
{
    const xmlChar *prop;

    if (nsName)
	*nsName = NULL;
    if (localName)
	*localName = NULL;
    if (hasProp)
	*hasProp = 0;

    prop = xsltGetCNsProp(style, inst, propName, XSLT_NAMESPACE);
    if (prop == NULL) {
	if (mandatory) {
	    xsltTransformError(NULL, style, inst,
		"The attribute '%s' is missing.\n", propName);
	    style->errors++;
	    return;
	}
    } else {
        const xmlChar *URI;

	if (xmlValidateQName(prop, 0)) {
	    xsltTransformError(NULL, style, inst,
		"The value '%s' of the attribute "
		"'%s' is not a valid QName.\n", prop, propName);
	    style->errors++;
	    return;
	} else {
	    /*
	    * @prop will be in the string dict afterwards, @URI not.
	    */
	    URI = xsltGetQNameURI2(style, inst, &prop);
	    if (prop == NULL) {
		style->errors++;
	    } else {
		if (localName)
		    *localName = prop;
		if (hasProp)
		    *hasProp = 1;
		if (URI != NULL) {
		    /*
		    * Fixes bug #308441: Put the ns-name in the dict
		    * in order to pointer compare names during XPath's
		    * variable lookup.
		    */
		    if (nsName)
			*nsName = xmlDictLookup(style->dict, URI, -1);
		    /* comp->has_ns = 1; */
		}
	    }
	}
    }
    return;
}

/**
 * xsltWithParamComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt with-param node
 *
 * Process the xslt with-param node on the source node
 * Allowed parents: xsl:call-template, xsl:apply-templates.
 * <xsl:with-param
 *  name = qname
 *  select = expression>
 *  <!-- Content: template -->
 * </xsl:with-param>
 */
static void
xsltWithParamComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemWithParamPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemWithParamPtr) xsltNewStylePreComp(style, XSLT_FUNC_WITHPARAM);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_WITHPARAM);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
    * Attribute "name".
    */
    xsltGetQNameProperty(style, inst, BAD_CAST "name",
	1, &(comp->has_name), &(comp->ns), &(comp->name));
    if (comp->ns)
	comp->has_ns = 1;
    /*
    * Attribute "select".
    */
    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select != NULL) {
	comp->comp = xsltXPathCompile(style, comp->select);
	if (comp->comp == NULL) {
	    xsltTransformError(NULL, style, inst,
		 "XSLT-with-param: Failed to compile select "
		 "expression '%s'\n", comp->select);
	    style->errors++;
	}
	if (inst->children != NULL) {
	    xsltTransformError(NULL, style, inst,
		"XSLT-with-param: The content should be empty since "
		"the attribute select is present.\n");
	    style->warnings++;
	}
    }
}

/**
 * xsltNumberComp:
 * @style: an XSLT compiled stylesheet
 * @cur:   the xslt number node
 *
 * Process the xslt number node on the source node
 */
static void
xsltNumberComp(xsltStylesheetPtr style, xmlNodePtr cur) {
#ifdef XSLT_REFACTORED
    xsltStyleItemNumberPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif
    const xmlChar *prop;

    if ((style == NULL) || (cur == NULL) || (cur->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemNumberPtr) xsltNewStylePreComp(style, XSLT_FUNC_NUMBER);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_NUMBER);
#endif

    if (comp == NULL)
	return;
    cur->psvi = comp;

    comp->numdata.doc = cur->doc;
    comp->numdata.node = cur;
    comp->numdata.value = xsltGetCNsProp(style, cur, (const xmlChar *)"value",
	                                XSLT_NAMESPACE);

    prop = xsltEvalStaticAttrValueTemplate(style, cur,
			 (const xmlChar *)"format",
			 XSLT_NAMESPACE, &comp->numdata.has_format);
    if (comp->numdata.has_format == 0) {
	comp->numdata.format = xmlDictLookup(style->dict, BAD_CAST "" , 0);
    } else {
	comp->numdata.format = prop;
    }

    comp->numdata.count = xsltGetCNsProp(style, cur, (const xmlChar *)"count",
                                         XSLT_NAMESPACE);
    comp->numdata.from = xsltGetCNsProp(style, cur, (const xmlChar *)"from",
                                        XSLT_NAMESPACE);

    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"count", XSLT_NAMESPACE);
    if (prop != NULL) {
	comp->numdata.countPat = xsltCompilePattern(prop, cur->doc, cur, style,
                                                    NULL);
    }

    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"from", XSLT_NAMESPACE);
    if (prop != NULL) {
	comp->numdata.fromPat = xsltCompilePattern(prop, cur->doc, cur, style,
                                                   NULL);
    }

    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"level", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (xmlStrEqual(prop, BAD_CAST("single")) ||
	    xmlStrEqual(prop, BAD_CAST("multiple")) ||
	    xmlStrEqual(prop, BAD_CAST("any"))) {
	    comp->numdata.level = prop;
	} else {
	    xsltTransformError(NULL, style, cur,
			 "xsl:number : invalid value %s for level\n", prop);
	    if (style != NULL) style->warnings++;
	}
    }

    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"lang", XSLT_NAMESPACE);
    if (prop != NULL) {
	    xsltTransformError(NULL, style, cur,
		 "xsl:number : lang attribute not implemented\n");
	XSLT_TODO; /* xsl:number lang attribute */
    }

    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"letter-value", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (xmlStrEqual(prop, BAD_CAST("alphabetic"))) {
	    xsltTransformError(NULL, style, cur,
		 "xsl:number : letter-value 'alphabetic' not implemented\n");
	    if (style != NULL) style->warnings++;
	    XSLT_TODO; /* xsl:number letter-value attribute alphabetic */
	} else if (xmlStrEqual(prop, BAD_CAST("traditional"))) {
	    xsltTransformError(NULL, style, cur,
		 "xsl:number : letter-value 'traditional' not implemented\n");
	    if (style != NULL) style->warnings++;
	    XSLT_TODO; /* xsl:number letter-value attribute traditional */
	} else {
	    xsltTransformError(NULL, style, cur,
		     "xsl:number : invalid value %s for letter-value\n", prop);
	    if (style != NULL) style->warnings++;
	}
    }

    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"grouping-separator",
	                XSLT_NAMESPACE);
    if (prop != NULL) {
        comp->numdata.groupingCharacterLen = xmlStrlen(prop);
	comp->numdata.groupingCharacter =
	    xsltGetUTF8Char(prop, &(comp->numdata.groupingCharacterLen));
        if (comp->numdata.groupingCharacter < 0)
            comp->numdata.groupingCharacter = 0;
    }

    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"grouping-size", XSLT_NAMESPACE);
    if (prop != NULL) {
	sscanf((char *)prop, "%d", &comp->numdata.digitsPerGroup);
    } else {
	comp->numdata.groupingCharacter = 0;
    }

    /* Set default values */
    if (comp->numdata.value == NULL) {
	if (comp->numdata.level == NULL) {
	    comp->numdata.level = xmlDictLookup(style->dict,
	                                        BAD_CAST"single", 6);
	}
    }

}

/**
 * xsltApplyImportsComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt apply-imports node
 *
 * Process the xslt apply-imports node on the source node
 */
static void
xsltApplyImportsComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemApplyImportsPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemApplyImportsPtr) xsltNewStylePreComp(style, XSLT_FUNC_APPLYIMPORTS);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_APPLYIMPORTS);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;
}

/**
 * xsltCallTemplateComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt call-template node
 *
 * Process the xslt call-template node on the source node
 */
static void
xsltCallTemplateComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemCallTemplatePtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemCallTemplatePtr)
	xsltNewStylePreComp(style, XSLT_FUNC_CALLTEMPLATE);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_CALLTEMPLATE);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
     * Attribute "name".
     */
    xsltGetQNameProperty(style, inst, BAD_CAST "name",
	1, &(comp->has_name), &(comp->ns), &(comp->name));
    if (comp->ns)
	comp->has_ns = 1;
}

/**
 * xsltApplyTemplatesComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the apply-templates node
 *
 * Process the apply-templates node on the source node
 */
static void
xsltApplyTemplatesComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemApplyTemplatesPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemApplyTemplatesPtr)
	xsltNewStylePreComp(style, XSLT_FUNC_APPLYTEMPLATES);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_APPLYTEMPLATES);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
     * Attribute "mode".
     */
    xsltGetQNameProperty(style, inst, BAD_CAST "mode",
	0, NULL, &(comp->modeURI), &(comp->mode));
    /*
    * Attribute "select".
    */
    comp->select = xsltGetCNsProp(style, inst, BAD_CAST "select",
	XSLT_NAMESPACE);
    if (comp->select != NULL) {
	comp->comp = xsltXPathCompile(style, comp->select);
	if (comp->comp == NULL) {
	    xsltTransformError(NULL, style, inst,
		"XSLT-apply-templates: could not compile select "
		"expression '%s'\n", comp->select);
	     style->errors++;
	}
    }
    /* TODO: handle (or skip) the xsl:sort and xsl:with-param */
}

/**
 * xsltChooseComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt choose node
 *
 * Process the xslt choose node on the source node
 */
static void
xsltChooseComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemChoosePtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemChoosePtr)
	xsltNewStylePreComp(style, XSLT_FUNC_CHOOSE);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_CHOOSE);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;
}

/**
 * xsltIfComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt if node
 *
 * Process the xslt if node on the source node
 */
static void
xsltIfComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemIfPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemIfPtr)
	xsltNewStylePreComp(style, XSLT_FUNC_IF);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_IF);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->test = xsltGetCNsProp(style, inst, (const xmlChar *)"test", XSLT_NAMESPACE);
    if (comp->test == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:if : test is not defined\n");
	if (style != NULL) style->errors++;
	return;
    }
    comp->comp = xsltXPathCompile(style, comp->test);
    if (comp->comp == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:if : could not compile test expression '%s'\n",
	                 comp->test);
	if (style != NULL) style->errors++;
    }
}

/**
 * xsltWhenComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt if node
 *
 * Process the xslt if node on the source node
 */
static void
xsltWhenComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemWhenPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemWhenPtr)
	xsltNewStylePreComp(style, XSLT_FUNC_WHEN);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_WHEN);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->test = xsltGetCNsProp(style, inst, (const xmlChar *)"test", XSLT_NAMESPACE);
    if (comp->test == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:when : test is not defined\n");
	if (style != NULL) style->errors++;
	return;
    }
    comp->comp = xsltXPathCompile(style, comp->test);
    if (comp->comp == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:when : could not compile test expression '%s'\n",
	                 comp->test);
	if (style != NULL) style->errors++;
    }
}

/**
 * xsltForEachComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt for-each node
 *
 * Process the xslt for-each node on the source node
 */
static void
xsltForEachComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemForEachPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemForEachPtr)
	xsltNewStylePreComp(style, XSLT_FUNC_FOREACH);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_FOREACH);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select == NULL) {
	xsltTransformError(NULL, style, inst,
		"xsl:for-each : select is missing\n");
	if (style != NULL) style->errors++;
    } else {
	comp->comp = xsltXPathCompile(style, comp->select);
	if (comp->comp == NULL) {
	    xsltTransformError(NULL, style, inst,
     "xsl:for-each : could not compile select expression '%s'\n",
			     comp->select);
	    if (style != NULL) style->errors++;
	}
    }
    /* TODO: handle and skip the xsl:sort */
}

/**
 * xsltVariableComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt variable node
 *
 * Process the xslt variable node on the source node
 */
static void
xsltVariableComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemVariablePtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemVariablePtr)
	xsltNewStylePreComp(style, XSLT_FUNC_VARIABLE);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_VARIABLE);
#endif

    if (comp == NULL)
	return;

    inst->psvi = comp;
    comp->inst = inst;
    /*
     * The full template resolution can be done statically
     */

    /*
    * Attribute "name".
    */
    xsltGetQNameProperty(style, inst, BAD_CAST "name",
	1, &(comp->has_name), &(comp->ns), &(comp->name));
    if (comp->ns)
	comp->has_ns = 1;
    /*
    * Attribute "select".
    */
    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select != NULL) {
#ifndef XSLT_REFACTORED
        xmlNodePtr cur;
#endif
	comp->comp = xsltXPathCompile(style, comp->select);
	if (comp->comp == NULL) {
	    xsltTransformError(NULL, style, inst,
		"XSLT-variable: Failed to compile the XPath expression '%s'.\n",
		comp->select);
	    style->errors++;
	}
#ifdef XSLT_REFACTORED
	if (inst->children != NULL) {
	    xsltTransformError(NULL, style, inst,
		"XSLT-variable: There must be no child nodes, since the "
		"attribute 'select' was specified.\n");
	    style->errors++;
	}
#else
        for (cur = inst->children; cur != NULL; cur = cur->next) {
            if (cur->type != XML_COMMENT_NODE &&
                (cur->type != XML_TEXT_NODE || !xsltIsBlank(cur->content)))
            {
                xsltTransformError(NULL, style, inst,
                    "XSLT-variable: There must be no child nodes, since the "
                    "attribute 'select' was specified.\n");
                style->errors++;
            }
        }
#endif
    }
}

/**
 * xsltParamComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt param node
 *
 * Process the xslt param node on the source node
 */
static void
xsltParamComp(xsltStylesheetPtr style, xmlNodePtr inst) {
#ifdef XSLT_REFACTORED
    xsltStyleItemParamPtr comp;
#else
    xsltStylePreCompPtr comp;
#endif

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

#ifdef XSLT_REFACTORED
    comp = (xsltStyleItemParamPtr)
	xsltNewStylePreComp(style, XSLT_FUNC_PARAM);
#else
    comp = xsltNewStylePreComp(style, XSLT_FUNC_PARAM);
#endif

    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
     * Attribute "name".
     */
    xsltGetQNameProperty(style, inst, BAD_CAST "name",
	1, &(comp->has_name), &(comp->ns), &(comp->name));
    if (comp->ns)
	comp->has_ns = 1;
    /*
    * Attribute "select".
    */
    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select != NULL) {
	comp->comp = xsltXPathCompile(style, comp->select);
	if (comp->comp == NULL) {
	    xsltTransformError(NULL, style, inst,
		"XSLT-param: could not compile select expression '%s'.\n",
		comp->select);
	    style->errors++;
	}
	if (inst->children != NULL) {
	    xsltTransformError(NULL, style, inst,
		"XSLT-param: The content should be empty since the "
		"attribute 'select' is present.\n");
	    style->warnings++;
	}
    }
}

/************************************************************************
 *									*
 *		    Generic interface					*
 *									*
 ************************************************************************/

/**
 * xsltFreeStylePreComps:
 * @style:  an XSLT transformation context
 *
 * Free up the memory allocated by all precomputed blocks
 */
void
xsltFreeStylePreComps(xsltStylesheetPtr style) {
    xsltElemPreCompPtr cur, next;

    if (style == NULL)
	return;

    cur = style->preComps;
    while (cur != NULL) {
	next = cur->next;
	if (cur->type == XSLT_FUNC_EXTENSION)
	    cur->free(cur);
	else
	    xsltFreeStylePreComp((xsltStylePreCompPtr) cur);
	cur = next;
    }
}

#ifdef XSLT_REFACTORED

/**
 * xsltStylePreCompute:
 * @style:  the XSLT stylesheet
 * @node:  the element in the XSLT namespace
 *
 * Precompute an XSLT element.
 * This expects the type of the element to be already
 * set in style->compCtxt->inode->type;
 */
void
xsltStylePreCompute(xsltStylesheetPtr style, xmlNodePtr node) {
    /*
    * The xsltXSLTElemMarker marker was set beforehand by
    *  the parsing mechanism for all elements in the XSLT namespace.
    */
    if (style == NULL) {
	if ((node != NULL) && (node->type == XML_ELEMENT_NODE))
	    node->psvi = NULL;
	return;
    }
    if (node == NULL)
	return;
    if (! IS_XSLT_ELEM_FAST(node))
	return;

    node->psvi = NULL;
    if (XSLT_CCTXT(style)->inode->type != 0) {
	switch (XSLT_CCTXT(style)->inode->type) {
	    case XSLT_FUNC_APPLYTEMPLATES:
		xsltApplyTemplatesComp(style, node);
		break;
	    case XSLT_FUNC_WITHPARAM:
		xsltWithParamComp(style, node);
		break;
	    case XSLT_FUNC_VALUEOF:
		xsltValueOfComp(style, node);
		break;
	    case XSLT_FUNC_COPY:
		xsltCopyComp(style, node);
		break;
	    case XSLT_FUNC_COPYOF:
		xsltCopyOfComp(style, node);
		break;
	    case XSLT_FUNC_IF:
		xsltIfComp(style, node);
		break;
	    case XSLT_FUNC_CHOOSE:
		xsltChooseComp(style, node);
		break;
	    case XSLT_FUNC_WHEN:
		xsltWhenComp(style, node);
		break;
	    case XSLT_FUNC_OTHERWISE:
		/* NOP yet */
		return;
	    case XSLT_FUNC_FOREACH:
		xsltForEachComp(style, node);
		break;
	    case XSLT_FUNC_APPLYIMPORTS:
		xsltApplyImportsComp(style, node);
		break;
	    case XSLT_FUNC_ATTRIBUTE:
		xsltAttributeComp(style, node);
		break;
	    case XSLT_FUNC_ELEMENT:
		xsltElementComp(style, node);
		break;
	    case XSLT_FUNC_SORT:
		xsltSortComp(style, node);
		break;
	    case XSLT_FUNC_COMMENT:
		xsltCommentComp(style, node);
		break;
	    case XSLT_FUNC_NUMBER:
		xsltNumberComp(style, node);
		break;
	    case XSLT_FUNC_PI:
		xsltProcessingInstructionComp(style, node);
		break;
	    case XSLT_FUNC_CALLTEMPLATE:
		xsltCallTemplateComp(style, node);
		break;
	    case XSLT_FUNC_PARAM:
		xsltParamComp(style, node);
		break;
	    case XSLT_FUNC_VARIABLE:
		xsltVariableComp(style, node);
		break;
	    case XSLT_FUNC_FALLBACK:
		/* NOP yet */
		return;
	    case XSLT_FUNC_DOCUMENT:
		/* The extra one */
		node->psvi = (void *) xsltDocumentComp(style, node,
		    xsltDocumentElem);
		break;
	    case XSLT_FUNC_MESSAGE:
		/* NOP yet */
		return;
	    default:
		/*
		* NOTE that xsl:text, xsl:template, xsl:stylesheet,
		*  xsl:transform, xsl:import, xsl:include are not expected
		*  to be handed over to this function.
		*/
		xsltTransformError(NULL, style, node,
		    "Internal error: (xsltStylePreCompute) cannot handle "
		    "the XSLT element '%s'.\n", node->name);
		style->errors++;
		return;
	}
    } else {
	/*
	* Fallback to string comparison.
	*/
	if (IS_XSLT_NAME(node, "apply-templates")) {
	    xsltApplyTemplatesComp(style, node);
	} else if (IS_XSLT_NAME(node, "with-param")) {
	    xsltWithParamComp(style, node);
	} else if (IS_XSLT_NAME(node, "value-of")) {
	    xsltValueOfComp(style, node);
	} else if (IS_XSLT_NAME(node, "copy")) {
	    xsltCopyComp(style, node);
	} else if (IS_XSLT_NAME(node, "copy-of")) {
	    xsltCopyOfComp(style, node);
	} else if (IS_XSLT_NAME(node, "if")) {
	    xsltIfComp(style, node);
	} else if (IS_XSLT_NAME(node, "choose")) {
	    xsltChooseComp(style, node);
	} else if (IS_XSLT_NAME(node, "when")) {
	    xsltWhenComp(style, node);
	} else if (IS_XSLT_NAME(node, "otherwise")) {
	    /* NOP yet */
	    return;
	} else if (IS_XSLT_NAME(node, "for-each")) {
	    xsltForEachComp(style, node);
	} else if (IS_XSLT_NAME(node, "apply-imports")) {
	    xsltApplyImportsComp(style, node);
	} else if (IS_XSLT_NAME(node, "attribute")) {
	    xsltAttributeComp(style, node);
	} else if (IS_XSLT_NAME(node, "element")) {
	    xsltElementComp(style, node);
	} else if (IS_XSLT_NAME(node, "sort")) {
	    xsltSortComp(style, node);
	} else if (IS_XSLT_NAME(node, "comment")) {
	    xsltCommentComp(style, node);
	} else if (IS_XSLT_NAME(node, "number")) {
	    xsltNumberComp(style, node);
	} else if (IS_XSLT_NAME(node, "processing-instruction")) {
	    xsltProcessingInstructionComp(style, node);
	} else if (IS_XSLT_NAME(node, "call-template")) {
	    xsltCallTemplateComp(style, node);
	} else if (IS_XSLT_NAME(node, "param")) {
	    xsltParamComp(style, node);
	} else if (IS_XSLT_NAME(node, "variable")) {
	    xsltVariableComp(style, node);
	} else if (IS_XSLT_NAME(node, "fallback")) {
	    /* NOP yet */
	    return;
	} else if (IS_XSLT_NAME(node, "document")) {
	    /* The extra one */
	    node->psvi = (void *) xsltDocumentComp(style, node,
		xsltDocumentElem);
	} else if (IS_XSLT_NAME(node, "output")) {
	    /* Top-level */
	    return;
	} else if (IS_XSLT_NAME(node, "preserve-space")) {
	    /* Top-level */
	    return;
	} else if (IS_XSLT_NAME(node, "strip-space")) {
	    /* Top-level */
	    return;
	} else if (IS_XSLT_NAME(node, "key")) {
	    /* Top-level */
	    return;
	} else if (IS_XSLT_NAME(node, "message")) {
	    return;
	} else if (IS_XSLT_NAME(node, "attribute-set")) {
	    /* Top-level */
	    return;
	} else if (IS_XSLT_NAME(node, "namespace-alias")) {
	    /* Top-level */
	    return;
	} else if (IS_XSLT_NAME(node, "decimal-format")) {
	    /* Top-level */
	    return;
	} else if (IS_XSLT_NAME(node, "include")) {
	    /* Top-level */
	} else {
	    /*
	    * NOTE that xsl:text, xsl:template, xsl:stylesheet,
	    *  xsl:transform, xsl:import, xsl:include are not expected
	    *  to be handed over to this function.
	    */
	    xsltTransformError(NULL, style, node,
		"Internal error: (xsltStylePreCompute) cannot handle "
		"the XSLT element '%s'.\n", node->name);
		style->errors++;
	    return;
	}
    }
    /*
    * Assign the current list of in-scope namespaces to the
    * item. This is needed for XPath expressions.
    */
    if (node->psvi != NULL) {
	((xsltStylePreCompPtr) node->psvi)->inScopeNs =
	    XSLT_CCTXT(style)->inode->inScopeNs;
    }
}

#else

/**
 * xsltStylePreCompute:
 * @style:  the XSLT stylesheet
 * @inst:  the instruction in the stylesheet
 *
 * Precompute an XSLT stylesheet element
 */
void
xsltStylePreCompute(xsltStylesheetPtr style, xmlNodePtr inst) {
    /*
    * URGENT TODO: Normally inst->psvi Should never be reserved here,
    *   BUT: since if we include the same stylesheet from
    *   multiple imports, then the stylesheet will be parsed
    *   again. We simply must not try to compute the stylesheet again.
    * TODO: Get to the point where we don't need to query the
    *   namespace- and local-name of the node, but can evaluate this
    *   using cctxt->style->inode->category;
    */
    if ((inst == NULL) || (inst->type != XML_ELEMENT_NODE) ||
        (inst->psvi != NULL))
	return;

    if (IS_XSLT_ELEM(inst)) {
	xsltStylePreCompPtr cur;

	if (IS_XSLT_NAME(inst, "apply-templates")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltApplyTemplatesComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "with-param")) {
	    xsltCheckParentElement(style, inst, BAD_CAST "apply-templates",
	                           BAD_CAST "call-template");
	    xsltWithParamComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "value-of")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltValueOfComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "copy")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltCopyComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "copy-of")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltCopyOfComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "if")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltIfComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "when")) {
	    xsltCheckParentElement(style, inst, BAD_CAST "choose", NULL);
	    xsltWhenComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "choose")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltChooseComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "for-each")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltForEachComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "apply-imports")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltApplyImportsComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "attribute")) {
	    xmlNodePtr parent = inst->parent;

	    if ((parent == NULL) ||
	        (parent->type != XML_ELEMENT_NODE) || (parent->ns == NULL) ||
		((parent->ns != inst->ns) &&
		 (!xmlStrEqual(parent->ns->href, inst->ns->href))) ||
		(!xmlStrEqual(parent->name, BAD_CAST "attribute-set"))) {
		xsltCheckInstructionElement(style, inst);
	    }
	    xsltAttributeComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "element")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltElementComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "text")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltTextComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "sort")) {
	    xsltCheckParentElement(style, inst, BAD_CAST "apply-templates",
	                           BAD_CAST "for-each");
	    xsltSortComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "comment")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltCommentComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "number")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltNumberComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "processing-instruction")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltProcessingInstructionComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "call-template")) {
	    xsltCheckInstructionElement(style, inst);
	    xsltCallTemplateComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "param")) {
	    if (xsltCheckTopLevelElement(style, inst, 0) == 0)
	        xsltCheckInstructionElement(style, inst);
	    xsltParamComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "variable")) {
	    if (xsltCheckTopLevelElement(style, inst, 0) == 0)
	        xsltCheckInstructionElement(style, inst);
	    xsltVariableComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "otherwise")) {
	    xsltCheckParentElement(style, inst, BAD_CAST "choose", NULL);
	    xsltCheckInstructionElement(style, inst);
	    return;
	} else if (IS_XSLT_NAME(inst, "template")) {
	    xsltCheckTopLevelElement(style, inst, 1);
	    return;
	} else if (IS_XSLT_NAME(inst, "output")) {
	    xsltCheckTopLevelElement(style, inst, 1);
	    return;
	} else if (IS_XSLT_NAME(inst, "preserve-space")) {
	    xsltCheckTopLevelElement(style, inst, 1);
	    return;
	} else if (IS_XSLT_NAME(inst, "strip-space")) {
	    xsltCheckTopLevelElement(style, inst, 1);
	    return;
	} else if ((IS_XSLT_NAME(inst, "stylesheet")) ||
	           (IS_XSLT_NAME(inst, "transform"))) {
	    xmlNodePtr parent = inst->parent;

	    if ((parent == NULL) || (parent->type != XML_DOCUMENT_NODE)) {
		xsltTransformError(NULL, style, inst,
		    "element %s only allowed only as root element\n",
				   inst->name);
		style->errors++;
	    }
	    return;
	} else if (IS_XSLT_NAME(inst, "key")) {
	    xsltCheckTopLevelElement(style, inst, 1);
	    return;
	} else if (IS_XSLT_NAME(inst, "message")) {
	    xsltCheckInstructionElement(style, inst);
	    return;
	} else if (IS_XSLT_NAME(inst, "attribute-set")) {
	    xsltCheckTopLevelElement(style, inst, 1);
	    return;
	} else if (IS_XSLT_NAME(inst, "namespace-alias")) {
	    xsltCheckTopLevelElement(style, inst, 1);
	    return;
	} else if (IS_XSLT_NAME(inst, "include")) {
	    xsltCheckTopLevelElement(style, inst, 1);
	    return;
	} else if (IS_XSLT_NAME(inst, "import")) {
	    xsltCheckTopLevelElement(style, inst, 1);
	    return;
	} else if (IS_XSLT_NAME(inst, "decimal-format")) {
	    xsltCheckTopLevelElement(style, inst, 1);
	    return;
	} else if (IS_XSLT_NAME(inst, "fallback")) {
	    xsltCheckInstructionElement(style, inst);
	    return;
	} else if (IS_XSLT_NAME(inst, "document")) {
	    xsltCheckInstructionElement(style, inst);
	    inst->psvi = (void *) xsltDocumentComp(style, inst,
				xsltDocumentElem);
	} else if ((style == NULL) || (style->forwards_compatible == 0)) {
	    xsltTransformError(NULL, style, inst,
		 "xsltStylePreCompute: unknown xsl:%s\n", inst->name);
	    if (style != NULL) style->warnings++;
	}

	cur = (xsltStylePreCompPtr) inst->psvi;
	/*
	* A ns-list is build for every XSLT item in the
	* node-tree. This is needed for XPath expressions.
	*/
	if (cur != NULL) {
	    int i = 0;

	    cur->nsList = xmlGetNsList(inst->doc, inst);
            if (cur->nsList != NULL) {
		while (cur->nsList[i] != NULL)
		    i++;
	    }
	    cur->nsNr = i;
	}
    } else {
	inst->psvi =
	    (void *) xsltPreComputeExtModuleElement(style, inst);

	/*
	 * Unknown element, maybe registered at the context
	 * level. Mark it for later recognition.
	 */
	if (inst->psvi == NULL)
	    inst->psvi = (void *) xsltExtMarker;
    }
}
#endif /* XSLT_REFACTORED */
