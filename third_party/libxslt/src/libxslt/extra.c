/*
 * extra.c: Implementation of non-standard features
 *
 * Reference:
 *   Michael Kay "XSLT Programmer's Reference" pp 637-643
 *   The node-set() extension function
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXSLT
#include "libxslt.h"

#include <string.h>
#include <stdlib.h>

#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/hash.h>
#include <libxml/xmlerror.h>
#include <libxml/parserInternals.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "extensions.h"
#include "variables.h"
#include "transform.h"
#include "extra.h"
#include "preproc.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_EXTRA
#endif

/************************************************************************
 *									*
 *		Handling of XSLT debugging				*
 *									*
 ************************************************************************/

/**
 * xsltDebug:
 * @ctxt:  an XSLT processing context
 * @node:  The current node
 * @inst:  the instruction in the stylesheet
 * @comp:  precomputed information
 *
 * Process an debug node
 */
void
xsltDebug(xsltTransformContextPtr ctxt, xmlNodePtr node ATTRIBUTE_UNUSED,
          xmlNodePtr inst ATTRIBUTE_UNUSED,
          xsltElemPreCompPtr comp ATTRIBUTE_UNUSED)
{
    int i, j;

    xsltGenericError(xsltGenericErrorContext, "Templates:\n");
    for (i = 0, j = ctxt->templNr - 1; ((i < 15) && (j >= 0)); i++, j--) {
        xsltGenericError(xsltGenericErrorContext, "#%d ", i);
        if (ctxt->templTab[j]->name != NULL)
            xsltGenericError(xsltGenericErrorContext, "name %s ",
                             ctxt->templTab[j]->name);
        if (ctxt->templTab[j]->match != NULL)
            xsltGenericError(xsltGenericErrorContext, "name %s ",
                             ctxt->templTab[j]->match);
        if (ctxt->templTab[j]->mode != NULL)
            xsltGenericError(xsltGenericErrorContext, "name %s ",
                             ctxt->templTab[j]->mode);
        xsltGenericError(xsltGenericErrorContext, "\n");
    }
    xsltGenericError(xsltGenericErrorContext, "Variables:\n");
    for (i = 0, j = ctxt->varsNr - 1; ((i < 15) && (j >= 0)); i++, j--) {
        xsltStackElemPtr cur;

        if (ctxt->varsTab[j] == NULL)
            continue;
        xsltGenericError(xsltGenericErrorContext, "#%d\n", i);
        cur = ctxt->varsTab[j];
        while (cur != NULL) {
            if (cur->comp == NULL) {
                xsltGenericError(xsltGenericErrorContext,
                                 "corrupted !!!\n");
            } else if (cur->comp->type == XSLT_FUNC_PARAM) {
                xsltGenericError(xsltGenericErrorContext, "param ");
            } else if (cur->comp->type == XSLT_FUNC_VARIABLE) {
                xsltGenericError(xsltGenericErrorContext, "var ");
            }
            if (cur->name != NULL)
                xsltGenericError(xsltGenericErrorContext, "%s ",
                                 cur->name);
            else
                xsltGenericError(xsltGenericErrorContext, "noname !!!!");
#ifdef LIBXML_DEBUG_ENABLED
            if (cur->value != NULL) {
                if ((xsltGenericDebugContext == stdout) ||
                    (xsltGenericDebugContext == stderr))
                    xmlXPathDebugDumpObject((FILE*)xsltGenericDebugContext,
                                            cur->value, 1);
            } else {
                xsltGenericError(xsltGenericErrorContext, "NULL !!!!");
            }
#endif
            xsltGenericError(xsltGenericErrorContext, "\n");
            cur = cur->next;
        }

    }
}

/************************************************************************
 *									*
 *		Classic extensions as described by M. Kay		*
 *									*
 ************************************************************************/

/**
 * xsltFunctionNodeSet:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * Implement the node-set() XSLT function
 *   node-set node-set(result-tree)
 *
 * This function is available in libxslt, saxon or xt namespace.
 */
void
xsltFunctionNodeSet(xmlXPathParserContextPtr ctxt, int nargs){
    if (nargs != 1) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"node-set() : expects one result-tree arg\n");
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }
    if ((ctxt->value == NULL) ||
	((ctxt->value->type != XPATH_XSLT_TREE) &&
	 (ctxt->value->type != XPATH_NODESET))) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
	    "node-set() invalid arg expecting a result tree\n");
	ctxt->error = XPATH_INVALID_TYPE;
	return;
    }
    if (ctxt->value->type == XPATH_XSLT_TREE) {
	ctxt->value->type = XPATH_NODESET;
    }
}

/**
 * xsltRegisterExtras:
 * @ctxt:  a XSLT process context
 *
 * Registers the built-in extensions. This function is deprecated, use
 * xsltRegisterAllExtras instead.
 */
void
xsltRegisterExtras(xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED) {
    xsltRegisterAllExtras();
}

/**
 * xsltRegisterAllExtras:
 *
 * Registers the built-in extensions
 */
void
xsltRegisterAllExtras (void) {
    xsltRegisterExtModuleFunction((const xmlChar *) "node-set",
				  XSLT_LIBXSLT_NAMESPACE,
				  xsltFunctionNodeSet);
    xsltRegisterExtModuleFunction((const xmlChar *) "node-set",
				  XSLT_SAXON_NAMESPACE,
				  xsltFunctionNodeSet);
    xsltRegisterExtModuleFunction((const xmlChar *) "node-set",
				  XSLT_XT_NAMESPACE,
				  xsltFunctionNodeSet);
    xsltRegisterExtModuleElement((const xmlChar *) "debug",
				 XSLT_LIBXSLT_NAMESPACE,
				 NULL,
				 xsltDebug);
    xsltRegisterExtModuleElement((const xmlChar *) "output",
				 XSLT_SAXON_NAMESPACE,
				 xsltDocumentComp,
				 xsltDocumentElem);
    xsltRegisterExtModuleElement((const xmlChar *) "write",
				 XSLT_XALAN_NAMESPACE,
				 xsltDocumentComp,
				 xsltDocumentElem);
    xsltRegisterExtModuleElement((const xmlChar *) "document",
				 XSLT_XT_NAMESPACE,
				 xsltDocumentComp,
				 xsltDocumentElem);
    xsltRegisterExtModuleElement((const xmlChar *) "document",
				 XSLT_NAMESPACE,
				 xsltDocumentComp,
				 xsltDocumentElem);
}
