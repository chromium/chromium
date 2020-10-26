#define IN_LIBEXSLT
#include "libexslt/libexslt.h"

#include <string.h>

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/hash.h>
#include <libxml/debugXML.h>

#include <libxslt/xsltutils.h>
#include <libxslt/variables.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/extensions.h>
#include <libxslt/transform.h>
#include <libxslt/imports.h>

#include "exslt.h"

typedef struct _exsltFuncFunctionData exsltFuncFunctionData;
struct _exsltFuncFunctionData {
    int nargs;			/* number of arguments to the function */
    xmlNodePtr content;		/* the func:fuction template content */
};

typedef struct _exsltFuncData exsltFuncData;
struct _exsltFuncData {
    xmlHashTablePtr funcs;	/* pointer to the stylesheet module data */
    xmlXPathObjectPtr result;	/* returned by func:result */
    xsltStackElemPtr ctxtVar;   /* context variable */
    int error;			/* did an error occur? */
};

typedef struct _exsltFuncResultPreComp exsltFuncResultPreComp;
struct _exsltFuncResultPreComp {
    xsltElemPreComp comp;
    xmlXPathCompExprPtr select;
    xmlNsPtr *nsList;
    int nsNr;
};

/* Used for callback function in exsltInitFunc */
typedef struct _exsltFuncImportRegData exsltFuncImportRegData;
struct _exsltFuncImportRegData {
    xsltTransformContextPtr ctxt;
    xmlHashTablePtr hash;
};

static void exsltFuncFunctionFunction (xmlXPathParserContextPtr ctxt,
				       int nargs);
static exsltFuncFunctionData *exsltFuncNewFunctionData(void);

/*static const xmlChar *exsltResultDataID = (const xmlChar *) "EXSLT Result";*/

/**
 * exsltFuncRegisterFunc:
 * @func:  the #exsltFuncFunctionData for the function
 * @ctxt:  an XSLT transformation context
 * @URI:  the function namespace URI
 * @name: the function name
 *
 * Registers a function declared by a func:function element
 */
static void
exsltFuncRegisterFunc (void *payload, void *vctxt,
		       const xmlChar *URI, const xmlChar *name,
		       ATTRIBUTE_UNUSED const xmlChar *ignored) {
    exsltFuncFunctionData *data = (exsltFuncFunctionData *) payload;
    xsltTransformContextPtr ctxt = (xsltTransformContextPtr) vctxt;

    if ((data == NULL) || (ctxt == NULL) || (URI == NULL) || (name == NULL))
	return;

    xsltGenericDebug(xsltGenericDebugContext,
		     "exsltFuncRegisterFunc: register {%s}%s\n",
		     URI, name);
    xsltRegisterExtFunction(ctxt, name, URI,
			    exsltFuncFunctionFunction);
}

/*
 * exsltFuncRegisterImportFunc
 * @data:    the exsltFuncFunctionData for the function
 * @ch:	     structure containing context and hash table
 * @URI:     the function namespace URI
 * @name:    the function name
 *
 * Checks if imported function is already registered in top-level
 * stylesheet.  If not, copies function data and registers function
 */
static void
exsltFuncRegisterImportFunc (void *payload, void *vctxt,
			     const xmlChar *URI, const xmlChar *name,
			     ATTRIBUTE_UNUSED const xmlChar *ignored) {
    exsltFuncFunctionData *data = (exsltFuncFunctionData *) payload;
    exsltFuncImportRegData *ch = (exsltFuncImportRegData *) vctxt;
    exsltFuncFunctionData *func=NULL;

    if ((data == NULL) || (ch == NULL) || (URI == NULL) || (name == NULL))
            return;

    if (ch->ctxt == NULL || ch->hash == NULL)
	return;

    /* Check if already present */
    func = (exsltFuncFunctionData*)xmlHashLookup2(ch->hash, URI, name);
    if (func == NULL) {		/* Not yet present - copy it in */
	func = exsltFuncNewFunctionData();
        if (func == NULL)
            return;
	memcpy(func, data, sizeof(exsltFuncFunctionData));
	if (xmlHashAddEntry2(ch->hash, URI, name, func) < 0) {
	    xsltGenericError(xsltGenericErrorContext,
		    "Failed to register function {%s}%s\n",
		    URI, name);
	} else {		/* Do the registration */
	    xsltGenericDebug(xsltGenericDebugContext,
	            "exsltFuncRegisterImportFunc: register {%s}%s\n",
		    URI, name);
	    xsltRegisterExtFunction(ch->ctxt, name, URI,
		    exsltFuncFunctionFunction);
	}
    }
}

/**
 * exsltFuncInit:
 * @ctxt: an XSLT transformation context
 * @URI: the namespace URI for the extension
 *
 * Initializes the EXSLT - Functions module.
 * Called at transformation-time; merges all
 * functions declared in the import tree taking
 * import precedence into account, i.e. overriding
 * functions with lower import precedence.
 *
 * Returns the data for this transformation
 */
static void *
exsltFuncInit (xsltTransformContextPtr ctxt, const xmlChar *URI) {
    exsltFuncData *ret;
    xsltStylesheetPtr tmp;
    exsltFuncImportRegData ch;
    xmlHashTablePtr hash;

    ret = (exsltFuncData *) xmlMalloc (sizeof(exsltFuncData));
    if (ret == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncInit: not enough memory\n");
	return(NULL);
    }
    memset(ret, 0, sizeof(exsltFuncData));

    ret->result = NULL;
    ret->error = 0;

    ch.hash = (xmlHashTablePtr) xsltStyleGetExtData(ctxt->style, URI);
    ret->funcs = ch.hash;
    xmlHashScanFull(ch.hash, exsltFuncRegisterFunc, ctxt);
    tmp = ctxt->style;
    ch.ctxt = ctxt;
    while ((tmp=xsltNextImport(tmp))!=NULL) {
	hash = xsltGetExtInfo(tmp, URI);
	if (hash != NULL) {
	    xmlHashScanFull(hash, exsltFuncRegisterImportFunc, &ch);
	}
    }

    return(ret);
}

/**
 * exsltFuncShutdown:
 * @ctxt: an XSLT transformation context
 * @URI: the namespace URI for the extension
 * @data: the module data to free up
 *
 * Shutdown the EXSLT - Functions module
 * Called at transformation-time.
 */
static void
exsltFuncShutdown (xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED,
		   const xmlChar *URI ATTRIBUTE_UNUSED,
		   void *vdata) {
    exsltFuncData *data = (exsltFuncData *) vdata;

    if (data->result != NULL)
	xmlXPathFreeObject(data->result);
    xmlFree(data);
}

/**
 * exsltFuncStyleInit:
 * @style: an XSLT stylesheet
 * @URI: the namespace URI for the extension
 *
 * Allocates the stylesheet data for EXSLT - Function
 * Called at compile-time.
 *
 * Returns the allocated data
 */
static void *
exsltFuncStyleInit (xsltStylesheetPtr style ATTRIBUTE_UNUSED,
		    const xmlChar *URI ATTRIBUTE_UNUSED) {
    return xmlHashCreate(1);
}

static void
exsltFuncFreeDataEntry(void *payload, const xmlChar *name ATTRIBUTE_UNUSED) {
    xmlFree(payload);
}

/**
 * exsltFuncStyleShutdown:
 * @style: an XSLT stylesheet
 * @URI: the namespace URI for the extension
 * @data: the stylesheet data to free up
 *
 * Shutdown the EXSLT - Function module
 * Called at compile-time.
 */
static void
exsltFuncStyleShutdown (xsltStylesheetPtr style ATTRIBUTE_UNUSED,
			const xmlChar *URI ATTRIBUTE_UNUSED,
			void *vdata) {
    xmlHashTablePtr data = (xmlHashTablePtr) vdata;
    xmlHashFree(data, exsltFuncFreeDataEntry);
}

/**
 * exsltFuncNewFunctionData:
 *
 * Allocates an #exslFuncFunctionData object
 *
 * Returns the new structure
 */
static exsltFuncFunctionData *
exsltFuncNewFunctionData (void) {
    exsltFuncFunctionData *ret;

    ret = (exsltFuncFunctionData *) xmlMalloc (sizeof(exsltFuncFunctionData));
    if (ret == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncNewFunctionData: not enough memory\n");
	return (NULL);
    }
    memset(ret, 0, sizeof(exsltFuncFunctionData));

    ret->nargs = 0;
    ret->content = NULL;

    return(ret);
}

/**
 * exsltFreeFuncResultPreComp:
 * @comp:  the #exsltFuncResultPreComp to free up
 *
 * Deallocates an #exsltFuncResultPreComp
 */
static void
exsltFreeFuncResultPreComp (xsltElemPreCompPtr ecomp) {
    exsltFuncResultPreComp *comp = (exsltFuncResultPreComp *) ecomp;

    if (comp == NULL)
	return;

    if (comp->select != NULL)
	xmlXPathFreeCompExpr (comp->select);
    if (comp->nsList != NULL)
        xmlFree(comp->nsList);
    xmlFree(comp);
}

/**
 * exsltFuncFunctionFunction:
 * @ctxt:  an XPath parser context
 * @nargs:  the number of arguments
 *
 * Evaluates the func:function element that defines the called function.
 */
static void
exsltFuncFunctionFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    xmlXPathObjectPtr oldResult, ret;
    exsltFuncData *data;
    exsltFuncFunctionData *func;
    xmlNodePtr paramNode, oldInsert, oldXPNode, fake;
    int oldBase;
    void *oldCtxtVar;
    xsltStackElemPtr params = NULL, param;
    xsltTransformContextPtr tctxt = xsltXPathGetTransformContext(ctxt);
    int i, notSet;
    struct objChain {
	struct objChain *next;
	xmlXPathObjectPtr obj;
    };
    struct objChain	*savedObjChain = NULL, *savedObj;

    /*
     * retrieve func:function template
     */
    data = (exsltFuncData *) xsltGetExtData (tctxt,
					     EXSLT_FUNCTIONS_NAMESPACE);
    oldResult = data->result;
    data->result = NULL;

    func = (exsltFuncFunctionData*) xmlHashLookup2 (data->funcs,
						    ctxt->context->functionURI,
						    ctxt->context->function);
    if (func == NULL) {
        /* Should never happen */
        xsltGenericError(xsltGenericErrorContext,
                         "{%s}%s: not found\n",
                         ctxt->context->functionURI, ctxt->context->function);
        ctxt->error = XPATH_UNKNOWN_FUNC_ERROR;
        return;
    }

    /*
     * params handling
     */
    if (nargs > func->nargs) {
	xsltGenericError(xsltGenericErrorContext,
			 "{%s}%s: called with too many arguments\n",
			 ctxt->context->functionURI, ctxt->context->function);
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }
    if (func->content != NULL) {
	paramNode = func->content->prev;
    }
    else
	paramNode = NULL;
    if ((paramNode == NULL) && (func->nargs != 0)) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncFunctionFunction: nargs != 0 and "
			 "param == NULL\n");
	return;
    }

    /*
    * When a function is called recursively during evaluation of its
    * arguments, the recursion check in xsltApplySequenceConstructor
    * isn't reached.
    */
    if (tctxt->depth >= tctxt->maxTemplateDepth) {
        xsltTransformError(tctxt, NULL, NULL,
            "exsltFuncFunctionFunction: Potentially infinite recursion "
            "detected in function {%s}%s.\n",
            ctxt->context->functionURI, ctxt->context->function);
        tctxt->state = XSLT_STATE_STOPPED;
        return;
    }
    tctxt->depth++;

    /* Evaluating templates can change the XPath context node. */
    oldXPNode = tctxt->xpathCtxt->node;

    /*
     * We have a problem with the evaluation of function parameters.
     * The original library code did not evaluate XPath expressions until
     * the last moment.  After version 1.1.17 of the libxslt, the logic
     * of other parts of the library was changed, and the evaluation of
     * XPath expressions within parameters now takes place as soon as the
     * parameter is parsed/evaluated (xsltParseStylesheetCallerParam).
     * This means that the parameters need to be evaluated in lexical
     * order (since a variable is "in scope" as soon as it is declared).
     * However, on entry to this routine, the values (from the caller) are
     * in reverse order (held on the XPath context variable stack).  To
     * accomplish what is required, I have added code to pop the XPath
     * objects off of the stack at the beginning and save them, then use
     * them (in the reverse order) as the params are evaluated.  This
     * requires an xmlMalloc/xmlFree for each param set by the caller,
     * which is not very nice.  There is probably a much better solution
     * (like change other code to delay the evaluation).
     */
    /*
     * In order to give the function params and variables a new 'scope'
     * we change varsBase in the context.
     */
    oldBase = tctxt->varsBase;
    tctxt->varsBase = tctxt->varsNr;
    /* If there are any parameters */
    if (paramNode != NULL) {
        /* Fetch the stored argument values from the caller */
	for (i = 0; i < nargs; i++) {
	    savedObj = xmlMalloc(sizeof(struct objChain));
	    savedObj->next = savedObjChain;
	    savedObj->obj = valuePop(ctxt);
	    savedObjChain = savedObj;
	}

	/*
	 * Prepare to process params in reverse order.  First, go to
	 * the beginning of the param chain.
	 */
	for (i = 1; i <= func->nargs; i++) {
	    if (paramNode->prev == NULL)
	        break;
	    paramNode = paramNode->prev;
	}
	/*
	 * i has total # params found, nargs is number which are present
	 * as arguments from the caller
	 * Calculate the number of un-set parameters
	 */
	notSet = func->nargs - nargs;
	for (; i > 0; i--) {
	    param = xsltParseStylesheetCallerParam (tctxt, paramNode);
	    if (i > notSet) {	/* if parameter value set */
		param->computed = 1;
		if (param->value != NULL)
		    xmlXPathFreeObject(param->value);
		savedObj = savedObjChain;	/* get next val from chain */
		param->value = savedObj->obj;
		savedObjChain = savedObjChain->next;
		xmlFree(savedObj);
	    }
	    xsltLocalVariablePush(tctxt, param, -1);
	    param->next = params;
	    params = param;
	    paramNode = paramNode->next;
	}
    }
    /*
     * Actual processing. The context variable is cleared and restored
     * when func:result is evaluated.
     */
    fake = xmlNewDocNode(tctxt->output, NULL,
			 (const xmlChar *)"fake", NULL);
    oldInsert = tctxt->insert;
    oldCtxtVar = data->ctxtVar;
    data->ctxtVar = tctxt->contextVariable;
    tctxt->insert = fake;
    tctxt->contextVariable = NULL;
    xsltApplyOneTemplate (tctxt, tctxt->node,
			  func->content, NULL, NULL);
    xsltLocalVariablePop(tctxt, tctxt->varsBase, -2);
    tctxt->insert = oldInsert;
    tctxt->contextVariable = data->ctxtVar;
    tctxt->varsBase = oldBase;	/* restore original scope */
    data->ctxtVar = oldCtxtVar;
    if (params != NULL)
	xsltFreeStackElemList(params);
    tctxt->xpathCtxt->node = oldXPNode;

    if (data->error != 0)
        goto error;

    if (data->result != NULL) {
	ret = data->result;
        /*
        * IMPORTANT: This enables previously tree fragments marked as
        * being results of a function, to be garbage-collected after
        * the calling process exits.
        */
        xsltFlagRVTs(tctxt, ret, XSLT_RVT_LOCAL);
    } else
	ret = xmlXPathNewCString("");

    data->result = oldResult;

    /*
     * It is an error if the instantiation of the template results in
     * the generation of result nodes.
     */
    if (fake->children != NULL) {
#ifdef LIBXML_DEBUG_ENABLED
	xmlDebugDumpNode (stderr, fake, 1);
#endif
	xsltGenericError(xsltGenericErrorContext,
			 "{%s}%s: cannot write to result tree while "
			 "executing a function\n",
			 ctxt->context->functionURI, ctxt->context->function);
	xmlFreeNode(fake);
        xmlXPathFreeObject(ret);
	goto error;
    }
    xmlFreeNode(fake);
    valuePush(ctxt, ret);

error:
    tctxt->depth--;
}


static void
exsltFuncFunctionComp (xsltStylesheetPtr style, xmlNodePtr inst) {
    xmlChar *name, *prefix;
    xmlNsPtr ns;
    xmlHashTablePtr data;
    exsltFuncFunctionData *func;

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
	return;

    {
	xmlChar *qname;

	qname = xmlGetProp(inst, (const xmlChar *) "name");
	name = xmlSplitQName2 (qname, &prefix);
	xmlFree(qname);
    }
    if ((name == NULL) || (prefix == NULL)) {
	xsltGenericError(xsltGenericErrorContext,
			 "func:function: not a QName\n");
	if (name != NULL)
	    xmlFree(name);
	return;
    }
    /* namespace lookup */
    ns = xmlSearchNs (inst->doc, inst, prefix);
    if (ns == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "func:function: undeclared prefix %s\n",
			 prefix);
	xmlFree(name);
	xmlFree(prefix);
	return;
    }
    xmlFree(prefix);

    xsltParseTemplateContent(style, inst);

    /*
     * Create function data
     */
    func = exsltFuncNewFunctionData();
    if (func == NULL) {
        xmlFree(name);
        return;
    }
    func->content = inst->children;
    while (IS_XSLT_ELEM(func->content) &&
	   IS_XSLT_NAME(func->content, "param")) {
	func->content = func->content->next;
	func->nargs++;
    }

    /*
     * Register the function data such that it can be retrieved
     * by exslFuncFunctionFunction
     */
#ifdef XSLT_REFACTORED
    /*
    * Ensure that the hash table will be stored in the *current*
    * stylesheet level in order to correctly evaluate the
    * import precedence.
    */
    data = (xmlHashTablePtr)
	xsltStyleStylesheetLevelGetExtData(style,
	    EXSLT_FUNCTIONS_NAMESPACE);
#else
    data = (xmlHashTablePtr)
	xsltStyleGetExtData (style, EXSLT_FUNCTIONS_NAMESPACE);
#endif
    if (data == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncFunctionComp: no stylesheet data\n");
	xmlFree(name);
        xmlFree(func);
	return;
    }

    if (xmlHashAddEntry2 (data, ns->href, name, func) < 0) {
	xsltTransformError(NULL, style, inst,
	    "Failed to register function {%s}%s\n",
			 ns->href, name);
	style->errors++;
        xmlFree(func);
    } else {
	xsltGenericDebug(xsltGenericDebugContext,
			 "exsltFuncFunctionComp: register {%s}%s\n",
			 ns->href, name);
    }
    xmlFree(name);
}

static xsltElemPreCompPtr
exsltFuncResultComp (xsltStylesheetPtr style, xmlNodePtr inst,
		     xsltTransformFunction function) {
    xmlNodePtr test;
    xmlChar *sel;
    exsltFuncResultPreComp *ret;

    if ((style == NULL) || (inst == NULL) || (inst->type != XML_ELEMENT_NODE))
        return (NULL);

    /*
     * "Validity" checking
     */
    /* it is an error to have any following sibling elements aside
     * from the xsl:fallback element.
     */
    for (test = inst->next; test != NULL; test = test->next) {
	if (test->type != XML_ELEMENT_NODE)
	    continue;
	if (IS_XSLT_ELEM(test) && IS_XSLT_NAME(test, "fallback"))
	    continue;
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncResultElem: only xsl:fallback is "
			 "allowed to follow func:result\n");
	style->errors++;
	return (NULL);
    }
    /* it is an error for a func:result element to not be a descendant
     * of func:function.
     * it is an error if a func:result occurs within a func:result
     * element.
     * it is an error if instanciating the content of a variable
     * binding element (i.e. xsl:variable, xsl:param) results in the
     * instanciation of a func:result element.
     */
    for (test = inst->parent; test != NULL; test = test->parent) {
	if (IS_XSLT_ELEM(test) &&
	    IS_XSLT_NAME(test, "stylesheet")) {
	    xsltGenericError(xsltGenericErrorContext,
			     "func:result element not a descendant "
			     "of a func:function\n");
	    style->errors++;
	    return (NULL);
	}
	if ((test->ns != NULL) &&
	    (xmlStrEqual(test->ns->href, EXSLT_FUNCTIONS_NAMESPACE))) {
	    if (xmlStrEqual(test->name, (const xmlChar *) "function")) {
		break;
	    }
	    if (xmlStrEqual(test->name, (const xmlChar *) "result")) {
		xsltGenericError(xsltGenericErrorContext,
				 "func:result element not allowed within"
				 " another func:result element\n");
	        style->errors++;
		return (NULL);
	    }
	}
	if (IS_XSLT_ELEM(test) &&
	    (IS_XSLT_NAME(test, "variable") ||
	     IS_XSLT_NAME(test, "param"))) {
	    xsltGenericError(xsltGenericErrorContext,
			     "func:result element not allowed within"
			     " a variable binding element\n");
            style->errors++;
	    return (NULL);
	}
    }

    /*
     * Precomputation
     */
    ret = (exsltFuncResultPreComp *)
	xmlMalloc (sizeof(exsltFuncResultPreComp));
    if (ret == NULL) {
	xsltPrintErrorContext(NULL, NULL, NULL);
        xsltGenericError(xsltGenericErrorContext,
                         "exsltFuncResultComp : malloc failed\n");
        style->errors++;
        return (NULL);
    }
    memset(ret, 0, sizeof(exsltFuncResultPreComp));

    xsltInitElemPreComp ((xsltElemPreCompPtr) ret, style, inst, function,
		 exsltFreeFuncResultPreComp);
    ret->select = NULL;

    /*
     * Precompute the select attribute
     */
    sel = xmlGetNsProp(inst, (const xmlChar *) "select", NULL);
    if (sel != NULL) {
	ret->select = xsltXPathCompileFlags(style, sel, 0);
	xmlFree(sel);
    }
    /*
     * Precompute the namespace list
     */
    ret->nsList = xmlGetNsList(inst->doc, inst);
    if (ret->nsList != NULL) {
        int i = 0;
        while (ret->nsList[i] != NULL)
	    i++;
	ret->nsNr = i;
    }
    return ((xsltElemPreCompPtr) ret);
}

static void
exsltFuncResultElem (xsltTransformContextPtr ctxt,
	             xmlNodePtr node ATTRIBUTE_UNUSED, xmlNodePtr inst,
		     xsltElemPreCompPtr ecomp) {
    exsltFuncResultPreComp *comp = (exsltFuncResultPreComp *) ecomp;
    exsltFuncData *data;
    xmlXPathObjectPtr ret;


    /* It is an error if instantiating the content of the
     * func:function element results in the instantiation of more than
     * one func:result elements.
     */
    data = (exsltFuncData *) xsltGetExtData (ctxt, EXSLT_FUNCTIONS_NAMESPACE);
    if (data == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltFuncReturnElem: data == NULL\n");
	return;
    }
    if (data->result != NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "func:result already instanciated\n");
	data->error = 1;
	return;
    }
    /*
     * Restore context variable, so that it will receive the function
     * result RVTs.
     */
    ctxt->contextVariable = data->ctxtVar;
    /*
     * Processing
     */
    if (comp->select != NULL) {
	xmlNsPtr *oldXPNsList;
	int oldXPNsNr;
	xmlNodePtr oldXPContextNode;
	/* If the func:result element has a select attribute, then the
	 * value of the attribute must be an expression and the
	 * returned value is the object that results from evaluating
	 * the expression. In this case, the content must be empty.
	 */
	if (inst->children != NULL) {
	    xsltGenericError(xsltGenericErrorContext,
			     "func:result content must be empty if"
			     " the function has a select attribute\n");
	    data->error = 1;
	    return;
	}
	oldXPNsList = ctxt->xpathCtxt->namespaces;
	oldXPNsNr = ctxt->xpathCtxt->nsNr;
	oldXPContextNode = ctxt->xpathCtxt->node;

	ctxt->xpathCtxt->namespaces = comp->nsList;
	ctxt->xpathCtxt->nsNr = comp->nsNr;
        ctxt->xpathCtxt->node = ctxt->node;

	ret = xmlXPathCompiledEval(comp->select, ctxt->xpathCtxt);

	ctxt->xpathCtxt->node = oldXPContextNode;
	ctxt->xpathCtxt->nsNr = oldXPNsNr;
	ctxt->xpathCtxt->namespaces = oldXPNsList;

	if (ret == NULL) {
	    xsltGenericError(xsltGenericErrorContext,
			     "exsltFuncResultElem: ret == NULL\n");
	    return;
	}
	/*
	* Mark it as a function result in order to avoid garbage
	* collecting of tree fragments before the function exits.
	*/
	xsltFlagRVTs(ctxt, ret, XSLT_RVT_FUNC_RESULT);
    } else if (inst->children != NULL) {
	/* If the func:result element does not have a select attribute
	 * and has non-empty content (i.e. the func:result element has
	 * one or more child nodes), then the content of the
	 * func:result element specifies the value.
	 */
	xmlNodePtr oldInsert;
	xmlDocPtr container;

	container = xsltCreateRVT(ctxt);
	if (container == NULL) {
	    xsltGenericError(xsltGenericErrorContext,
			     "exsltFuncResultElem: out of memory\n");
	    data->error = 1;
	    return;
	}
        /* Mark as function result. */
        xsltRegisterLocalRVT(ctxt, container);
        container->psvi = XSLT_RVT_FUNC_RESULT;

	oldInsert = ctxt->insert;
	ctxt->insert = (xmlNodePtr) container;
	xsltApplyOneTemplate (ctxt, ctxt->node,
			      inst->children, NULL, NULL);
	ctxt->insert = oldInsert;

	ret = xmlXPathNewValueTree((xmlNodePtr) container);
	if (ret == NULL) {
	    xsltGenericError(xsltGenericErrorContext,
			     "exsltFuncResultElem: ret == NULL\n");
	    data->error = 1;
	} else {
	    ret->boolval = 0; /* Freeing is not handled there anymore */
	}
    } else {
	/* If the func:result element has empty content and does not
	 * have a select attribute, then the returned value is an
	 * empty string.
	 */
	ret = xmlXPathNewCString("");
    }
    data->result = ret;
}

/**
 * exsltFuncRegister:
 *
 * Registers the EXSLT - Functions module
 */
void
exsltFuncRegister (void) {
    xsltRegisterExtModuleFull (EXSLT_FUNCTIONS_NAMESPACE,
		       exsltFuncInit,
		       exsltFuncShutdown,
		       exsltFuncStyleInit,
		       exsltFuncStyleShutdown);

    xsltRegisterExtModuleTopLevel ((const xmlChar *) "function",
				   EXSLT_FUNCTIONS_NAMESPACE,
				   exsltFuncFunctionComp);
    xsltRegisterExtModuleElement ((const xmlChar *) "result",
			  EXSLT_FUNCTIONS_NAMESPACE,
			  exsltFuncResultComp,
			  exsltFuncResultElem);
}
