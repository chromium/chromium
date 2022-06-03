#define IN_LIBEXSLT
#include "libexslt/libexslt.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <libxslt/xsltutils.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/extensions.h>
#include <libxslt/transform.h>
#include <libxslt/extra.h>
#include <libxslt/preproc.h>

#include "exslt.h"

static void
exsltNodeSetFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    if (nargs != 1) {
	xmlXPathSetArityError(ctxt);
	return;
    }
    if (xmlXPathStackIsNodeSet (ctxt)) {
	xsltFunctionNodeSet (ctxt, nargs);
	return;
    } else {
	xmlDocPtr fragment;
	xsltTransformContextPtr tctxt = xsltXPathGetTransformContext(ctxt);
	xmlNodePtr txt;
	xmlChar *strval;
	xmlXPathObjectPtr obj;
	/*
	* SPEC EXSLT:
	* "You can also use this function to turn a string into a text
	* node, which is helpful if you want to pass a string to a
	* function that only accepts a node-set."
	*/
	fragment = xsltCreateRVT(tctxt);
	if (fragment == NULL) {
	    xsltTransformError(tctxt, NULL, tctxt->inst,
		"exsltNodeSetFunction: Failed to create a tree fragment.\n");
	    tctxt->state = XSLT_STATE_STOPPED;
	    return;
	}
	xsltRegisterLocalRVT(tctxt, fragment);

	strval = xmlXPathPopString (ctxt);

	txt = xmlNewDocText (fragment, strval);
	xmlAddChild((xmlNodePtr) fragment, txt);
	obj = xmlXPathNewNodeSet(txt);
	if (obj == NULL) {
	    xsltTransformError(tctxt, NULL, tctxt->inst,
		"exsltNodeSetFunction: Failed to create a node set object.\n");
	    tctxt->state = XSLT_STATE_STOPPED;
	}
	if (strval != NULL)
	    xmlFree (strval);

	valuePush (ctxt, obj);
    }
}

static void
exsltObjectTypeFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    xmlXPathObjectPtr obj, ret;

    if (nargs != 1) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    obj = valuePop(ctxt);

    switch (obj->type) {
    case XPATH_STRING:
	ret = xmlXPathNewCString("string");
	break;
    case XPATH_NUMBER:
	ret = xmlXPathNewCString("number");
	break;
    case XPATH_BOOLEAN:
	ret = xmlXPathNewCString("boolean");
	break;
    case XPATH_NODESET:
	ret = xmlXPathNewCString("node-set");
	break;
    case XPATH_XSLT_TREE:
	ret = xmlXPathNewCString("RTF");
	break;
    case XPATH_USERS:
	ret = xmlXPathNewCString("external");
	break;
    default:
	xsltGenericError(xsltGenericErrorContext,
		"object-type() invalid arg\n");
	ctxt->error = XPATH_INVALID_TYPE;
	xmlXPathFreeObject(obj);
	return;
    }
    xmlXPathFreeObject(obj);
    valuePush(ctxt, ret);
}


/**
 * exsltCommonRegister:
 *
 * Registers the EXSLT - Common module
 */

void
exsltCommonRegister (void) {
    xsltRegisterExtModuleFunction((const xmlChar *) "node-set",
				  EXSLT_COMMON_NAMESPACE,
				  exsltNodeSetFunction);
    xsltRegisterExtModuleFunction((const xmlChar *) "object-type",
				  EXSLT_COMMON_NAMESPACE,
				  exsltObjectTypeFunction);
    xsltRegisterExtModuleElement((const xmlChar *) "document",
				 EXSLT_COMMON_NAMESPACE,
				 xsltDocumentComp,
				 xsltDocumentElem);
}
