#define IN_LIBEXSLT
#include "libexslt/libexslt.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/parser.h>
#include <libxml/hash.h>

#include <libxslt/xsltutils.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/extensions.h>

#include "exslt.h"

/**
 * exsltSaxonInit:
 * @ctxt: an XSLT transformation context
 * @URI: the namespace URI for the extension
 *
 * Initializes the SAXON module.
 *
 * Returns the data for this transformation
 */
static void *
exsltSaxonInit (xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED,
		const xmlChar *URI ATTRIBUTE_UNUSED) {
    return xmlHashCreate(1);
}

static void
exsltSaxonFreeCompExprEntry(void *payload,
                            const xmlChar *name ATTRIBUTE_UNUSED) {
    xmlXPathFreeCompExpr((xmlXPathCompExprPtr) payload);
}

/**
 * exsltSaxonShutdown:
 * @ctxt: an XSLT transformation context
 * @URI: the namespace URI for the extension
 * @data: the module data to free up
 *
 * Shutdown the SAXON extension module
 */
static void
exsltSaxonShutdown (xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED,
		    const xmlChar *URI ATTRIBUTE_UNUSED,
		    void *vdata) {
    xmlHashTablePtr data = (xmlHashTablePtr) vdata;
    xmlHashFree(data, exsltSaxonFreeCompExprEntry);
}


/**
 * exsltSaxonExpressionFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * The supplied string must contain an XPath expression. The result of
 * the function is a stored expression, which may be supplied as an
 * argument to other extension functions such as saxon:eval(),
 * saxon:sum() and saxon:distinct(). The result of the expression will
 * usually depend on the current node. The expression may contain
 * references to variables that are in scope at the point where
 * saxon:expression() is called: these variables will be replaced in
 * the stored expression with the values they take at the time
 * saxon:expression() is called, not the values of the variables at
 * the time the stored expression is evaluated.  Similarly, if the
 * expression contains namespace prefixes, these are interpreted in
 * terms of the namespace declarations in scope at the point where the
 * saxon:expression() function is called, not those in scope where the
 * stored expression is evaluated.
 *
 * TODO: current implementation doesn't conform to SAXON behaviour
 * regarding context and namespaces.
 */
static void
exsltSaxonExpressionFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    xmlChar *arg;
    xmlXPathCompExprPtr ret;
    xmlHashTablePtr hash;
    xsltTransformContextPtr tctxt = xsltXPathGetTransformContext(ctxt);

    if (nargs != 1) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    arg = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt) || (arg == NULL)) {
	xmlXPathSetTypeError(ctxt);
	return;
    }

    hash = (xmlHashTablePtr) xsltGetExtData(tctxt,
					    ctxt->context->functionURI);

    ret = xmlHashLookup(hash, arg);

    if (ret == NULL) {
        ret = xmlXPathCtxtCompile(tctxt->xpathCtxt, arg);
        if (ret == NULL) {
            xmlFree(arg);
            xmlXPathSetError(ctxt, XPATH_EXPR_ERROR);
            return;
        }
        if (xmlHashAddEntry(hash, arg, (void *) ret) < 0) {
            xmlXPathFreeCompExpr(ret);
            xmlFree(arg);
            xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
            return;
        }
    }

    xmlFree(arg);

    xmlXPathReturnExternal(ctxt, ret);
}

/**
 * exsltSaxonEvalFunction:
 * @ctxt:  an XPath parser context
 * @nargs:  number of arguments
 *
 * Implements de SAXON eval() function:
 *    object saxon:eval (saxon:stored-expression)
 * Returns the result of evaluating the supplied stored expression.
 * A stored expression may be obtained as the result of calling
 * the saxon:expression() function.
 * The stored expression is evaluated in the current context, that
 * is, the context node is the current node, and the context position
 * and context size are the same as the result of calling position()
 * or last() respectively.
 */
static void
exsltSaxonEvalFunction (xmlXPathParserContextPtr ctxt, int nargs) {
     xmlXPathCompExprPtr expr;
     xmlXPathObjectPtr ret;

     if (nargs != 1) {
	  xmlXPathSetArityError(ctxt);
	  return;
     }

     if (!xmlXPathStackIsExternal(ctxt)) {
	  xmlXPathSetTypeError(ctxt);
	  return;
     }

     expr = (xmlXPathCompExprPtr) xmlXPathPopExternal(ctxt);

     ret = xmlXPathCompiledEval(expr, ctxt->context);
     if (ret == NULL) {
	  xmlXPathSetError(ctxt, XPATH_EXPR_ERROR);
	  return;
     }

     valuePush(ctxt, ret);
}

/**
 * exsltSaxonEvaluateFunction:
 * @ctxt:  an XPath parser context
 * @nargs: number of arguments
 *
 * Implements the SAXON evaluate() function
 *     object saxon:evaluate (string)
 * The supplied string must contain an XPath expression. The result of
 * the function is the result of evaluating the XPath expression. This
 * is useful where an expression needs to be constructed at run-time or
 * passed to the stylesheet as a parameter, for example where the sort
 * key is determined dynamically. The context for the expression (e.g.
 * which variables and namespaces are available) is exactly the same as
 * if the expression were written explicitly at this point in the
 * stylesheet. The function saxon:evaluate(string) is shorthand for
 * saxon:eval(saxon:expression(string)).
 */
static void
exsltSaxonEvaluateFunction (xmlXPathParserContextPtr ctxt, int nargs) {
     if (nargs != 1) {
	  xmlXPathSetArityError(ctxt);
	  return;
     }

     exsltSaxonExpressionFunction(ctxt, 1);
     exsltSaxonEvalFunction(ctxt, 1);
}

/**
 * exsltSaxonSystemIdFunction:
 * @ctxt:  an XPath parser context
 * @nargs: number of arguments
 *
 * Implements the SAXON systemId() function
 *     string saxon:systemId ()
 * This function returns the system ID of the document being styled.
 */
static void
exsltSaxonSystemIdFunction(xmlXPathParserContextPtr ctxt, int nargs)
{
    if (ctxt == NULL)
        return;
    if (nargs != 0) {
        xmlXPathSetArityError(ctxt);
        return;
    }

    if ((ctxt->context) && (ctxt->context->doc) &&
        (ctxt->context->doc->URL))
	valuePush(ctxt, xmlXPathNewString(ctxt->context->doc->URL));
    else
	valuePush(ctxt, xmlXPathNewString(BAD_CAST ""));
}

/**
 * exsltSaxonLineNumberFunction:
 * @ctxt:  an XPath parser context
 * @nargs: number of arguments
 *
 * Implements the SAXON line-number() function
 *     integer saxon:line-number()
 *
 * This returns the line number of the context node in the source document
 * within the entity that contains it. There are no arguments. If line numbers
 * are not maintained for the current document, the function returns -1. (To
 * ensure that line numbers are maintained, use the -l option on the command
 * line)
 *
 * The extension has been extended to have the following form:
 *     integer saxon:line-number([node-set-1])
 * If a node-set is given, this extension will return the line number of the
 * node in the argument node-set that is first in document order.
 */
static void
exsltSaxonLineNumberFunction(xmlXPathParserContextPtr ctxt, int nargs) {
    xmlNodePtr cur = NULL;
    xmlXPathObjectPtr obj = NULL;
    long lineNo = -1;

    if (nargs == 0) {
	cur = ctxt->context->node;
    } else if (nargs == 1) {
	xmlNodeSetPtr nodelist;
	int i;

	if ((ctxt->value == NULL) || (ctxt->value->type != XPATH_NODESET)) {
	    xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"saxon:line-number() : invalid arg expecting a node-set\n");
	    ctxt->error = XPATH_INVALID_TYPE;
	    return;
	}

	obj = valuePop(ctxt);
	nodelist = obj->nodesetval;
	if ((nodelist != NULL) && (nodelist->nodeNr > 0)) {
            cur = nodelist->nodeTab[0];
            for (i = 1;i < nodelist->nodeNr;i++) {
                int ret = xmlXPathCmpNodes(cur, nodelist->nodeTab[i]);
                if (ret == -1)
                    cur = nodelist->nodeTab[i];
            }
        }
    } else {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"saxon:line-number() : invalid number of args %d\n",
		nargs);
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }

    if ((cur != NULL) && (cur->type == XML_NAMESPACE_DECL)) {
        /*
        * The XPath module sets the owner element of a ns-node on
        * the ns->next field.
        */
        cur = (xmlNodePtr) ((xmlNsPtr) cur)->next;
        if (cur == NULL || cur->type != XML_ELEMENT_NODE) {
            xsltGenericError(xsltGenericErrorContext,
                "Internal error in exsltSaxonLineNumberFunction: "
                "Cannot retrieve the doc of a namespace node.\n");
            cur = NULL;
        }
    }

    if (cur != NULL)
        lineNo = xmlGetLineNo(cur);

    valuePush(ctxt, xmlXPathNewFloat(lineNo));

    xmlXPathFreeObject(obj);
}

/**
 * exsltSaxonRegister:
 *
 * Registers the SAXON extension module
 */
void
exsltSaxonRegister (void) {
     xsltRegisterExtModule (SAXON_NAMESPACE,
			    exsltSaxonInit,
			    exsltSaxonShutdown);
     xsltRegisterExtModuleFunction((const xmlChar *) "expression",
				   SAXON_NAMESPACE,
				   exsltSaxonExpressionFunction);
     xsltRegisterExtModuleFunction((const xmlChar *) "eval",
				   SAXON_NAMESPACE,
				   exsltSaxonEvalFunction);
     xsltRegisterExtModuleFunction((const xmlChar *) "evaluate",
				   SAXON_NAMESPACE,
				   exsltSaxonEvaluateFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "line-number",
				   SAXON_NAMESPACE,
				   exsltSaxonLineNumberFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "systemId",
				   SAXON_NAMESPACE,
				   exsltSaxonSystemIdFunction);
}
