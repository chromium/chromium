/*
 * functions.c: Implementation of the XSLT extra functions
 *
 * Reference:
 *   http://www.w3.org/TR/1999/REC-xslt-19991116
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 * Bjorn Reese <breese@users.sourceforge.net> for number formatting
 */

#define IN_LIBXSLT
#include "libxslt.h"

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/hash.h>
#include <libxml/xmlerror.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/parserInternals.h>
#include <libxml/uri.h>
#include <libxml/xpointer.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "functions.h"
#include "extensions.h"
#include "numbersInternals.h"
#include "keys.h"
#include "documents.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_FUNCTION
#endif

/*
 * Some versions of DocBook XSL use the vendor string to detect
 * supporting chunking, this is a workaround to be considered
 * in the list of decent XSLT processors <grin/>
 */
#define DOCBOOK_XSL_HACK

/**
 * xsltXPathFunctionLookup:
 * @vctxt:  a void * but the XSLT transformation context actually
 * @name:  the function name
 * @ns_uri:  the function namespace URI
 *
 * This is the entry point when a function is needed by the XPath
 * interpretor.
 *
 * Returns the callback function or NULL if not found
 */
xmlXPathFunction
xsltXPathFunctionLookup (void *vctxt,
			 const xmlChar *name, const xmlChar *ns_uri) {
    xmlXPathContextPtr ctxt = (xmlXPathContextPtr) vctxt;
    xmlXPathFunction ret;

    if ((ctxt == NULL) || (name == NULL) || (ns_uri == NULL))
	return (NULL);

#ifdef WITH_XSLT_DEBUG_FUNCTION
    xsltGenericDebug(xsltGenericDebugContext,
            "Lookup function {%s}%s\n", ns_uri, name);
#endif

    /* give priority to context-level functions */
    /*
    ret = (xmlXPathFunction) xmlHashLookup2(ctxt->funcHash, name, ns_uri);
    */
    XML_CAST_FPTR(ret) = xmlHashLookup2(ctxt->funcHash, name, ns_uri);

    if (ret == NULL)
	ret = xsltExtModuleFunctionLookup(name, ns_uri);

#ifdef WITH_XSLT_DEBUG_FUNCTION
    if (ret != NULL)
        xsltGenericDebug(xsltGenericDebugContext,
            "found function %s\n", name);
#endif
    return(ret);
}


/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/

static void
xsltDocumentFunctionLoadDocument(xmlXPathParserContextPtr ctxt,
                                 const xmlChar* URI, const xmlChar *fragment)
{
    xsltTransformContextPtr tctxt;
    xsltDocumentPtr idoc; /* document info */
    xmlDocPtr doc;
    xmlXPathContextPtr xptrctxt = NULL;
    xmlXPathObjectPtr resObj = NULL;

    (void) xptrctxt;

    tctxt = xsltXPathGetTransformContext(ctxt);
    if (tctxt == NULL) {
	xsltTransformError(NULL, NULL, NULL,
	    "document() : internal error tctxt == NULL\n");
        goto out_fragment;
    }

    idoc = xsltLoadDocument(tctxt, URI);

    if (idoc == NULL) {
	if ((URI == NULL) ||
	    (URI[0] == '#') ||
	    ((tctxt->style->doc != NULL) &&
	    (xmlStrEqual(tctxt->style->doc->URL, URI))))
	{
	    /*
	    * This selects the stylesheet's doc itself.
	    */
	    doc = tctxt->style->doc;
	} else {
            goto out_fragment;
	}
    } else
	doc = idoc->doc;

    if (fragment == NULL) {
	valuePush(ctxt, xmlXPathNewNodeSet((xmlNodePtr) doc));
	return;
    }

    /* use XPointer of HTML location for fragment ID */
#ifdef LIBXML_XPTR_ENABLED
    xptrctxt = xmlXPathNewContext(doc);
    if (xptrctxt == NULL) {
	xsltTransformError(tctxt, NULL, NULL,
	    "document() : internal error xptrctxt == NULL\n");
	goto out_fragment;
    }

#if LIBXML_VERSION >= 20911 || \
    defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
    xptrctxt->opLimit = ctxt->context->opLimit;
    xptrctxt->opCount = ctxt->context->opCount;
    xptrctxt->depth = ctxt->context->depth;

    resObj = xmlXPtrEval(fragment, xptrctxt);

    ctxt->context->opCount = xptrctxt->opCount;
#else
    resObj = xmlXPtrEval(fragment, xptrctxt);
#endif

    xmlXPathFreeContext(xptrctxt);
#endif /* LIBXML_XPTR_ENABLED */

    if ((resObj != NULL) && (resObj->type != XPATH_NODESET)) {
        xsltTransformError(tctxt, NULL, NULL,
            "document() : XPointer does not select a node set: #%s\n",
            fragment);
        xmlXPathFreeObject(resObj);
        resObj = NULL;
    }

out_fragment:
    if (resObj == NULL)
        resObj = xmlXPathNewNodeSet(NULL);
    valuePush(ctxt, resObj);
}

/**
 * xsltDocumentFunction:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * Implement the document() XSLT function
 *   node-set document(object, node-set?)
 */
void
xsltDocumentFunction(xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlXPathObjectPtr obj, obj2 = NULL;
    xmlChar *base = NULL, *URI;
    xmlChar *newURI = NULL;
    xmlChar *fragment = NULL;

    if ((nargs < 1) || (nargs > 2)) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                         "document() : invalid number of args %d\n",
                         nargs);
        ctxt->error = XPATH_INVALID_ARITY;
        return;
    }
    if (ctxt->value == NULL) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                         "document() : invalid arg value\n");
        ctxt->error = XPATH_INVALID_TYPE;
        return;
    }

    if (nargs == 2) {
        if (ctxt->value->type != XPATH_NODESET) {
            xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                             "document() : invalid arg expecting a nodeset\n");
            ctxt->error = XPATH_INVALID_TYPE;
            return;
        }

        obj2 = valuePop(ctxt);
    }

    if ((ctxt->value != NULL) && (ctxt->value->type == XPATH_NODESET)) {
        int i;
        xmlXPathObjectPtr newobj, ret;

        obj = valuePop(ctxt);
        ret = xmlXPathNewNodeSet(NULL);

        if ((obj != NULL) && (obj->nodesetval != NULL) && (ret != NULL)) {
            for (i = 0; i < obj->nodesetval->nodeNr; i++) {
                valuePush(ctxt,
                          xmlXPathNewNodeSet(obj->nodesetval->nodeTab[i]));
                xmlXPathStringFunction(ctxt, 1);
                if (nargs == 2) {
                    valuePush(ctxt, xmlXPathObjectCopy(obj2));
                } else {
                    valuePush(ctxt,
                              xmlXPathNewNodeSet(obj->nodesetval->
                                                 nodeTab[i]));
                }
                if (ctxt->error)
                    break;
                xsltDocumentFunction(ctxt, 2);
                newobj = valuePop(ctxt);
                if (newobj != NULL) {
                    ret->nodesetval = xmlXPathNodeSetMerge(ret->nodesetval,
                                                           newobj->nodesetval);
                    xmlXPathFreeObject(newobj);
                }
            }
        }

        if (obj != NULL)
            xmlXPathFreeObject(obj);
        if (obj2 != NULL)
            xmlXPathFreeObject(obj2);
        valuePush(ctxt, ret);
        return;
    }
    /*
     * Make sure it's converted to a string
     */
    xmlXPathStringFunction(ctxt, 1);
    if ((ctxt->value == NULL) || (ctxt->value->type != XPATH_STRING)) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                         "document() : invalid arg expecting a string\n");
        ctxt->error = XPATH_INVALID_TYPE;
        if (obj2 != NULL)
            xmlXPathFreeObject(obj2);
        return;
    }
    obj = valuePop(ctxt);
    if (obj->stringval == NULL) {
        valuePush(ctxt, xmlXPathNewNodeSet(NULL));
    } else {
        xsltTransformContextPtr tctxt;
        xmlURIPtr uri;
        const xmlChar *url;

        tctxt = xsltXPathGetTransformContext(ctxt);

        url = obj->stringval;

        uri = xmlParseURI((const char *) url);
        if (uri == NULL) {
            xsltTransformError(tctxt, NULL, NULL,
                "document() : failed to parse URI '%s'\n", url);
            valuePush(ctxt, xmlXPathNewNodeSet(NULL));
            goto error;
        }

        /*
         * check for and remove fragment identifier
         */
        fragment = (xmlChar *)uri->fragment;
        if (fragment != NULL) {
            uri->fragment = NULL;
            newURI = xmlSaveUri(uri);
            url = newURI;
        }
        xmlFreeURI(uri);

        if ((obj2 != NULL) && (obj2->nodesetval != NULL) &&
            (obj2->nodesetval->nodeNr > 0) &&
            IS_XSLT_REAL_NODE(obj2->nodesetval->nodeTab[0])) {
            xmlNodePtr target;

            target = obj2->nodesetval->nodeTab[0];
            if ((target->type == XML_ATTRIBUTE_NODE) ||
	        (target->type == XML_PI_NODE)) {
                target = ((xmlAttrPtr) target)->parent;
            }
            base = xmlNodeGetBase(target->doc, target);
        } else {
            if ((tctxt != NULL) && (tctxt->inst != NULL)) {
                base = xmlNodeGetBase(tctxt->inst->doc, tctxt->inst);
            } else if ((tctxt != NULL) && (tctxt->style != NULL) &&
                       (tctxt->style->doc != NULL)) {
                base = xmlNodeGetBase(tctxt->style->doc,
                                      (xmlNodePtr) tctxt->style->doc);
            }
        }

        URI = xmlBuildURI(url, base);
        if (base != NULL)
            xmlFree(base);
        if (URI == NULL) {
            if ((tctxt != NULL) && (tctxt->style != NULL) &&
                (tctxt->style->doc != NULL) &&
                (xmlStrEqual(URI, tctxt->style->doc->URL))) {
                /* This selects the stylesheet's doc itself. */
                valuePush(ctxt, xmlXPathNewNodeSet((xmlNodePtr) tctxt->style->doc));
            } else {
                valuePush(ctxt, xmlXPathNewNodeSet(NULL));
            }
        } else {
	    xsltDocumentFunctionLoadDocument(ctxt, URI, fragment);
	    xmlFree(URI);
	}
    }

error:
    xmlFree(newURI);
    xmlFree(fragment);
    xmlXPathFreeObject(obj);
    if (obj2 != NULL)
        xmlXPathFreeObject(obj2);
}

/**
 * xsltKeyFunction:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * Implement the key() XSLT function
 *   node-set key(string, object)
 */
void
xsltKeyFunction(xmlXPathParserContextPtr ctxt, int nargs){
    xmlXPathObjectPtr obj1, obj2;

    if (nargs != 2) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"key() : expects two arguments\n");
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }

    /*
    * Get the key's value.
    */
    obj2 = valuePop(ctxt);
    xmlXPathStringFunction(ctxt, 1);
    if ((obj2 == NULL) ||
	(ctxt->value == NULL) || (ctxt->value->type != XPATH_STRING)) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
	    "key() : invalid arg expecting a string\n");
	ctxt->error = XPATH_INVALID_TYPE;
	xmlXPathFreeObject(obj2);

	return;
    }
    /*
    * Get the key's name.
    */
    obj1 = valuePop(ctxt);

    if ((obj2->type == XPATH_NODESET) || (obj2->type == XPATH_XSLT_TREE)) {
	int i;
	xmlXPathObjectPtr newobj, ret;

	ret = xmlXPathNewNodeSet(NULL);
        if (ret == NULL) {
            ctxt->error = XPATH_MEMORY_ERROR;
            xmlXPathFreeObject(obj1);
            xmlXPathFreeObject(obj2);
            return;
        }

	if (obj2->nodesetval != NULL) {
	    for (i = 0; i < obj2->nodesetval->nodeNr; i++) {
		valuePush(ctxt, xmlXPathObjectCopy(obj1));
		valuePush(ctxt,
			  xmlXPathNewNodeSet(obj2->nodesetval->nodeTab[i]));
		xmlXPathStringFunction(ctxt, 1);
		xsltKeyFunction(ctxt, 2);
		newobj = valuePop(ctxt);
                if (newobj != NULL)
		    ret->nodesetval = xmlXPathNodeSetMerge(ret->nodesetval,
						           newobj->nodesetval);
		xmlXPathFreeObject(newobj);
	    }
	}
	valuePush(ctxt, ret);
    } else {
	xmlNodeSetPtr nodelist = NULL;
	xmlChar *key = NULL, *value;
	const xmlChar *keyURI;
	xsltTransformContextPtr tctxt;
	xmlChar *qname, *prefix;
	xmlXPathContextPtr xpctxt = ctxt->context;
	xmlNodePtr tmpNode = NULL;
	xsltDocumentPtr oldDocInfo;

	tctxt = xsltXPathGetTransformContext(ctxt);

	oldDocInfo = tctxt->document;

	if (xpctxt->node == NULL) {
	    xsltTransformError(tctxt, NULL, tctxt->inst,
		"Internal error in xsltKeyFunction(): "
		"The context node is not set on the XPath context.\n");
	    tctxt->state = XSLT_STATE_STOPPED;
	    goto error;
	}
	/*
	 * Get the associated namespace URI if qualified name
	 */
	qname = obj1->stringval;
	key = xmlSplitQName2(qname, &prefix);
	if (key == NULL) {
	    key = xmlStrdup(obj1->stringval);
	    keyURI = NULL;
	    if (prefix != NULL)
		xmlFree(prefix);
	} else {
	    if (prefix != NULL) {
		keyURI = xmlXPathNsLookup(xpctxt, prefix);
		if (keyURI == NULL) {
		    xsltTransformError(tctxt, NULL, tctxt->inst,
			"key() : prefix %s is not bound\n", prefix);
		    /*
		    * TODO: Shouldn't we stop here?
		    */
		}
		xmlFree(prefix);
	    } else {
		keyURI = NULL;
	    }
	}

	/*
	 * Force conversion of first arg to string
	 */
	valuePush(ctxt, obj2);
	xmlXPathStringFunction(ctxt, 1);
	obj2 = valuePop(ctxt);
	if ((obj2 == NULL) || (obj2->type != XPATH_STRING)) {
	    xsltTransformError(tctxt, NULL, tctxt->inst,
		"key() : invalid arg expecting a string\n");
	    ctxt->error = XPATH_INVALID_TYPE;
	    goto error;
	}
	value = obj2->stringval;

	/*
	* We need to ensure that ctxt->document is available for
	* xsltGetKey().
	* First find the relevant doc, which is the context node's
	* owner doc; using context->doc is not safe, since
	* the doc could have been acquired via the document() function,
	* or the doc might be a Result Tree Fragment.
	* FUTURE INFO: In XSLT 2.0 the key() function takes an additional
	* argument indicating the doc to use.
	*/
	if (xpctxt->node->type == XML_NAMESPACE_DECL) {
	    /*
	    * REVISIT: This is a libxml hack! Check xpath.c for details.
	    * The XPath module sets the owner element of a ns-node on
	    * the ns->next field.
	    */
	    if ((((xmlNsPtr) xpctxt->node)->next != NULL) &&
		(((xmlNsPtr) xpctxt->node)->next->type == XML_ELEMENT_NODE))
	    {
		tmpNode = (xmlNodePtr) ((xmlNsPtr) xpctxt->node)->next;
	    }
	} else
	    tmpNode = xpctxt->node;

	if ((tmpNode == NULL) || (tmpNode->doc == NULL)) {
	    xsltTransformError(tctxt, NULL, tctxt->inst,
		"Internal error in xsltKeyFunction(): "
		"Couldn't get the doc of the XPath context node.\n");
	    goto error;
	}

	if ((tctxt->document == NULL) ||
	    (tctxt->document->doc != tmpNode->doc))
	{
	    if (tmpNode->doc->name && (tmpNode->doc->name[0] == ' ')) {
		/*
		* This is a Result Tree Fragment.
		*/
		if (tmpNode->doc->_private == NULL) {
		    tmpNode->doc->_private = xsltNewDocument(tctxt, tmpNode->doc);
		    if (tmpNode->doc->_private == NULL)
			goto error;
		}
		tctxt->document = (xsltDocumentPtr) tmpNode->doc->_private;
	    } else {
		/*
		* May be the initial source doc or a doc acquired via the
		* document() function.
		*/
		tctxt->document = xsltFindDocument(tctxt, tmpNode->doc);
	    }
	    if (tctxt->document == NULL) {
		xsltTransformError(tctxt, NULL, tctxt->inst,
		    "Internal error in xsltKeyFunction(): "
		    "Could not get the document info of a context doc.\n");
		tctxt->state = XSLT_STATE_STOPPED;
		goto error;
	    }
	}
	/*
	* Get/compute the key value.
	*/
	nodelist = xsltGetKey(tctxt, key, keyURI, value);

error:
	tctxt->document = oldDocInfo;
	valuePush(ctxt, xmlXPathWrapNodeSet(
	    xmlXPathNodeSetMerge(NULL, nodelist)));
	if (key != NULL)
	    xmlFree(key);
    }

    if (obj1 != NULL)
	xmlXPathFreeObject(obj1);
    if (obj2 != NULL)
	xmlXPathFreeObject(obj2);
}

/**
 * xsltUnparsedEntityURIFunction:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * Implement the unparsed-entity-uri() XSLT function
 *   string unparsed-entity-uri(string)
 */
void
xsltUnparsedEntityURIFunction(xmlXPathParserContextPtr ctxt, int nargs){
    xmlXPathObjectPtr obj;
    xmlChar *str;

    if ((nargs != 1) || (ctxt->value == NULL)) {
        xsltGenericError(xsltGenericErrorContext,
		"unparsed-entity-uri() : expects one string arg\n");
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }
    obj = valuePop(ctxt);
    if (obj->type != XPATH_STRING) {
	obj = xmlXPathConvertString(obj);
        if (obj == NULL) {
            xmlXPathErr(ctxt, XPATH_MEMORY_ERROR);
            return;
        }
    }

    str = obj->stringval;
    if (str == NULL) {
	valuePush(ctxt, xmlXPathNewString((const xmlChar *)""));
    } else {
	xmlEntityPtr entity;

	entity = xmlGetDocEntity(ctxt->context->doc, str);
	if (entity == NULL) {
	    valuePush(ctxt, xmlXPathNewString((const xmlChar *)""));
	} else {
	    if (entity->URI != NULL)
		valuePush(ctxt, xmlXPathNewString(entity->URI));
	    else
		valuePush(ctxt, xmlXPathNewString((const xmlChar *)""));
	}
    }
    xmlXPathFreeObject(obj);
}

/**
 * xsltFormatNumberFunction:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * Implement the format-number() XSLT function
 *   string format-number(number, string, string?)
 */
void
xsltFormatNumberFunction(xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlXPathObjectPtr numberObj = NULL;
    xmlXPathObjectPtr formatObj = NULL;
    xmlXPathObjectPtr decimalObj = NULL;
    xsltStylesheetPtr sheet;
    xsltDecimalFormatPtr formatValues = NULL;
    xmlChar *result;
    const xmlChar *ncname;
    const xmlChar *prefix = NULL;
    const xmlChar *nsUri = NULL;
    xsltTransformContextPtr tctxt;

    tctxt = xsltXPathGetTransformContext(ctxt);
    if ((tctxt == NULL) || (tctxt->inst == NULL))
	return;
    sheet = tctxt->style;
    if (sheet == NULL)
	return;
    formatValues = sheet->decimalFormat;

    switch (nargs) {
    case 3:
        if ((ctxt->value != NULL) && (ctxt->value->type != XPATH_STRING))
            xmlXPathStringFunction(ctxt, 1);
	decimalObj = valuePop(ctxt);
        ncname = xsltSplitQName(sheet->dict, decimalObj->stringval, &prefix);
        if (prefix != NULL) {
            xmlNsPtr ns = xmlSearchNs(tctxt->inst->doc, tctxt->inst, prefix);
            if (ns == NULL) {
                xsltTransformError(tctxt, NULL, NULL,
                    "format-number : No namespace found for QName '%s:%s'\n",
                    prefix, ncname);
                sheet->errors++;
                ncname = NULL;
            }
            else {
                nsUri = ns->href;
            }
        }
        if (ncname != NULL) {
	    formatValues = xsltDecimalFormatGetByQName(sheet, nsUri, ncname);
        }
	if (formatValues == NULL) {
	    xsltTransformError(tctxt, NULL, NULL,
		    "format-number() : undeclared decimal format '%s'\n",
		    decimalObj->stringval);
	}
	/* Intentional fall-through */
    case 2:
        if ((ctxt->value != NULL) && (ctxt->value->type != XPATH_STRING))
            xmlXPathStringFunction(ctxt, 1);
	formatObj = valuePop(ctxt);
        if ((ctxt->value != NULL) && (ctxt->value->type != XPATH_NUMBER))
            xmlXPathNumberFunction(ctxt, 1);
	numberObj = valuePop(ctxt);
	break;
    default:
	xmlXPathErr(ctxt, XPATH_INVALID_ARITY);
        return;
    }

    if ((ctxt->error == 0) &&
        (formatValues != NULL) && (formatObj != NULL) && (numberObj != NULL)) {
	if (xsltFormatNumberConversion(formatValues,
				       formatObj->stringval,
				       numberObj->floatval,
				       &result) == XPATH_EXPRESSION_OK) {
	    valuePush(ctxt, xmlXPathNewString(result));
	    xmlFree(result);
	}
    }

    xmlXPathFreeObject(numberObj);
    xmlXPathFreeObject(formatObj);
    xmlXPathFreeObject(decimalObj);
}

/**
 * xsltGenerateIdFunction:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * Implement the generate-id() XSLT function
 *   string generate-id(node-set?)
 */
void
xsltGenerateIdFunction(xmlXPathParserContextPtr ctxt, int nargs){
    xsltTransformContextPtr tctxt;
    xmlNodePtr cur = NULL;
    xmlXPathObjectPtr obj = NULL;
    char *str;
    const xmlChar *nsPrefix = NULL;
    void **psviPtr;
    unsigned long id;
    size_t size, nsPrefixSize = 0;

    tctxt = xsltXPathGetTransformContext(ctxt);

    if (nargs == 0) {
	cur = ctxt->context->node;
    } else if (nargs == 1) {
	xmlNodeSetPtr nodelist;
	int i, ret;

	if ((ctxt->value == NULL) || (ctxt->value->type != XPATH_NODESET)) {
	    ctxt->error = XPATH_INVALID_TYPE;
	    xsltTransformError(tctxt, NULL, NULL,
		"generate-id() : invalid arg expecting a node-set\n");
            goto out;
	}
	obj = valuePop(ctxt);
	nodelist = obj->nodesetval;
	if ((nodelist == NULL) || (nodelist->nodeNr <= 0)) {
	    valuePush(ctxt, xmlXPathNewCString(""));
	    goto out;
	}
	cur = nodelist->nodeTab[0];
	for (i = 1;i < nodelist->nodeNr;i++) {
	    ret = xmlXPathCmpNodes(cur, nodelist->nodeTab[i]);
	    if (ret == -1)
	        cur = nodelist->nodeTab[i];
	}
    } else {
	xsltTransformError(tctxt, NULL, NULL,
		"generate-id() : invalid number of args %d\n", nargs);
	ctxt->error = XPATH_INVALID_ARITY;
	goto out;
    }

    size = 30; /* for "id%lu" */

    if (cur->type == XML_NAMESPACE_DECL) {
        xmlNsPtr ns = (xmlNsPtr) cur;

        nsPrefix = ns->prefix;
        if (nsPrefix == NULL)
            nsPrefix = BAD_CAST "";
        nsPrefixSize = xmlStrlen(nsPrefix);
        /* For "ns" and hex-encoded string */
        size += nsPrefixSize * 2 + 2;

        /* Parent is stored in 'next'. */
        cur = (xmlNodePtr) ns->next;
    }

    psviPtr = xsltGetPSVIPtr(cur);
    if (psviPtr == NULL) {
        xsltTransformError(tctxt, NULL, NULL,
                "generate-id(): invalid node type %d\n", cur->type);
        ctxt->error = XPATH_INVALID_TYPE;
        goto out;
    }

    if (xsltGetSourceNodeFlags(cur) & XSLT_SOURCE_NODE_HAS_ID) {
        id = (unsigned long) (size_t) *psviPtr;
    } else {
        if (cur->type == XML_TEXT_NODE && cur->line == USHRT_MAX) {
            /* Text nodes store big line numbers in psvi. */
            cur->line = 0;
        } else if (*psviPtr != NULL) {
            xsltTransformError(tctxt, NULL, NULL,
                    "generate-id(): psvi already set\n");
            ctxt->error = XPATH_MEMORY_ERROR;
            goto out;
        }

        if (tctxt->currentId == ULONG_MAX) {
            xsltTransformError(tctxt, NULL, NULL,
                    "generate-id(): id overflow\n");
            ctxt->error = XPATH_MEMORY_ERROR;
            goto out;
        }

        id = ++tctxt->currentId;
        *psviPtr = (void *) (size_t) id;
        xsltSetSourceNodeFlags(tctxt, cur, XSLT_SOURCE_NODE_HAS_ID);
    }

    str = xmlMalloc(size);
    if (str == NULL) {
        xsltTransformError(tctxt, NULL, NULL,
                "generate-id(): out of memory\n");
        ctxt->error = XPATH_MEMORY_ERROR;
        goto out;
    }
    if (nsPrefix == NULL) {
        snprintf(str, size, "id%lu", id);
    } else {
        size_t i, j;

        snprintf(str, size, "id%luns", id);

        /*
         * Only ASCII alphanumerics are allowed, so we hex-encode the prefix.
         */
        j = strlen(str);
        for (i = 0; i < nsPrefixSize; i++) {
            int v;

            v = nsPrefix[i] >> 4;
            str[j++] = v < 10 ? '0' + v : 'A' + (v - 10);
            v = nsPrefix[i] & 15;
            str[j++] = v < 10 ? '0' + v : 'A' + (v - 10);
        }
        str[j] = '\0';
    }
    valuePush(ctxt, xmlXPathWrapString(BAD_CAST str));

out:
    xmlXPathFreeObject(obj);
}

/**
 * xsltSystemPropertyFunction:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * Implement the system-property() XSLT function
 *   object system-property(string)
 */
void
xsltSystemPropertyFunction(xmlXPathParserContextPtr ctxt, int nargs){
    xmlXPathObjectPtr obj;
    xmlChar *prefix, *name;
    const xmlChar *nsURI = NULL;

    if (nargs != 1) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"system-property() : expects one string arg\n");
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }
    if ((ctxt->value == NULL) || (ctxt->value->type != XPATH_STRING)) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
	    "system-property() : invalid arg expecting a string\n");
	ctxt->error = XPATH_INVALID_TYPE;
	return;
    }
    obj = valuePop(ctxt);
    if (obj->stringval == NULL) {
	valuePush(ctxt, xmlXPathNewString((const xmlChar *)""));
    } else {
	name = xmlSplitQName2(obj->stringval, &prefix);
	if (name == NULL) {
	    name = xmlStrdup(obj->stringval);
	} else {
	    nsURI = xmlXPathNsLookup(ctxt->context, prefix);
	    if (nsURI == NULL) {
		xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		    "system-property() : prefix %s is not bound\n", prefix);
	    }
	}

	if (xmlStrEqual(nsURI, XSLT_NAMESPACE)) {
#ifdef DOCBOOK_XSL_HACK
	    if (xmlStrEqual(name, (const xmlChar *)"vendor")) {
		xsltStylesheetPtr sheet;
		xsltTransformContextPtr tctxt;

		tctxt = xsltXPathGetTransformContext(ctxt);
		if ((tctxt != NULL) && (tctxt->inst != NULL) &&
		    (xmlStrEqual(tctxt->inst->name, BAD_CAST "variable")) &&
		    (tctxt->inst->parent != NULL) &&
		    (xmlStrEqual(tctxt->inst->parent->name,
				 BAD_CAST "template")))
		    sheet = tctxt->style;
		else
		    sheet = NULL;
		if ((sheet != NULL) && (sheet->doc != NULL) &&
		    (sheet->doc->URL != NULL) &&
		    (xmlStrstr(sheet->doc->URL,
			       (const xmlChar *)"chunk") != NULL)) {
		    valuePush(ctxt, xmlXPathNewString(
			(const xmlChar *)"libxslt (SAXON 6.2 compatible)"));

		} else {
		    valuePush(ctxt, xmlXPathNewString(
			(const xmlChar *)XSLT_DEFAULT_VENDOR));
		}
	    } else
#else
	    if (xmlStrEqual(name, (const xmlChar *)"vendor")) {
		valuePush(ctxt, xmlXPathNewString(
			  (const xmlChar *)XSLT_DEFAULT_VENDOR));
	    } else
#endif
	    if (xmlStrEqual(name, (const xmlChar *)"version")) {
		valuePush(ctxt, xmlXPathNewString(
		    (const xmlChar *)XSLT_DEFAULT_VERSION));
	    } else if (xmlStrEqual(name, (const xmlChar *)"vendor-url")) {
		valuePush(ctxt, xmlXPathNewString(
		    (const xmlChar *)XSLT_DEFAULT_URL));
	    } else {
		valuePush(ctxt, xmlXPathNewString((const xmlChar *)""));
	    }
	} else {
	    valuePush(ctxt, xmlXPathNewString((const xmlChar *)""));
        }
	if (name != NULL)
	    xmlFree(name);
	if (prefix != NULL)
	    xmlFree(prefix);
    }
    xmlXPathFreeObject(obj);
}

/**
 * xsltElementAvailableFunction:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * Implement the element-available() XSLT function
 *   boolean element-available(string)
 */
void
xsltElementAvailableFunction(xmlXPathParserContextPtr ctxt, int nargs){
    xmlXPathObjectPtr obj;
    xmlChar *prefix, *name;
    const xmlChar *nsURI = NULL;
    xsltTransformContextPtr tctxt;

    if (nargs != 1) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"element-available() : expects one string arg\n");
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }
    xmlXPathStringFunction(ctxt, 1);
    if ((ctxt->value == NULL) || (ctxt->value->type != XPATH_STRING)) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
	    "element-available() : invalid arg expecting a string\n");
	ctxt->error = XPATH_INVALID_TYPE;
	return;
    }
    obj = valuePop(ctxt);
    tctxt = xsltXPathGetTransformContext(ctxt);
    if ((tctxt == NULL) || (tctxt->inst == NULL)) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"element-available() : internal error tctxt == NULL\n");
	xmlXPathFreeObject(obj);
	valuePush(ctxt, xmlXPathNewBoolean(0));
	return;
    }


    name = xmlSplitQName2(obj->stringval, &prefix);
    if (name == NULL) {
	xmlNsPtr ns;

	name = xmlStrdup(obj->stringval);
	ns = xmlSearchNs(tctxt->inst->doc, tctxt->inst, NULL);
	if (ns != NULL) nsURI = ns->href;
    } else {
	nsURI = xmlXPathNsLookup(ctxt->context, prefix);
	if (nsURI == NULL) {
	    xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"element-available() : prefix %s is not bound\n", prefix);
	}
    }

    if (xsltExtElementLookup(tctxt, name, nsURI) != NULL) {
	valuePush(ctxt, xmlXPathNewBoolean(1));
    } else {
	valuePush(ctxt, xmlXPathNewBoolean(0));
    }

    xmlXPathFreeObject(obj);
    if (name != NULL)
	xmlFree(name);
    if (prefix != NULL)
	xmlFree(prefix);
}

/**
 * xsltFunctionAvailableFunction:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * Implement the function-available() XSLT function
 *   boolean function-available(string)
 */
void
xsltFunctionAvailableFunction(xmlXPathParserContextPtr ctxt, int nargs){
    xmlXPathObjectPtr obj;
    xmlChar *prefix, *name;
    const xmlChar *nsURI = NULL;

    if (nargs != 1) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"function-available() : expects one string arg\n");
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }
    xmlXPathStringFunction(ctxt, 1);
    if ((ctxt->value == NULL) || (ctxt->value->type != XPATH_STRING)) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
	    "function-available() : invalid arg expecting a string\n");
	ctxt->error = XPATH_INVALID_TYPE;
	return;
    }
    obj = valuePop(ctxt);

    name = xmlSplitQName2(obj->stringval, &prefix);
    if (name == NULL) {
	name = xmlStrdup(obj->stringval);
    } else {
	nsURI = xmlXPathNsLookup(ctxt->context, prefix);
	if (nsURI == NULL) {
	    xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"function-available() : prefix %s is not bound\n", prefix);
	}
    }

    if (xmlXPathFunctionLookupNS(ctxt->context, name, nsURI) != NULL) {
	valuePush(ctxt, xmlXPathNewBoolean(1));
    } else {
	valuePush(ctxt, xmlXPathNewBoolean(0));
    }

    xmlXPathFreeObject(obj);
    if (name != NULL)
	xmlFree(name);
    if (prefix != NULL)
	xmlFree(prefix);
}

/**
 * xsltCurrentFunction:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * Implement the current() XSLT function
 *   node-set current()
 */
static void
xsltCurrentFunction(xmlXPathParserContextPtr ctxt, int nargs){
    xsltTransformContextPtr tctxt;

    if (nargs != 0) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"current() : function uses no argument\n");
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }
    tctxt = xsltXPathGetTransformContext(ctxt);
    if (tctxt == NULL) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"current() : internal error tctxt == NULL\n");
	valuePush(ctxt, xmlXPathNewNodeSet(NULL));
    } else {
	valuePush(ctxt, xmlXPathNewNodeSet(tctxt->node)); /* current */
    }
}

/************************************************************************
 *									*
 *		Registration of XSLT and libxslt functions		*
 *									*
 ************************************************************************/

/**
 * xsltRegisterAllFunctions:
 * @ctxt:  the XPath context
 *
 * Registers all default XSLT functions in this context
 */
void
xsltRegisterAllFunctions(xmlXPathContextPtr ctxt)
{
    xmlXPathRegisterFunc(ctxt, (const xmlChar *) "current",
                         xsltCurrentFunction);
    xmlXPathRegisterFunc(ctxt, (const xmlChar *) "document",
                         xsltDocumentFunction);
    xmlXPathRegisterFunc(ctxt, (const xmlChar *) "key", xsltKeyFunction);
    xmlXPathRegisterFunc(ctxt, (const xmlChar *) "unparsed-entity-uri",
                         xsltUnparsedEntityURIFunction);
    xmlXPathRegisterFunc(ctxt, (const xmlChar *) "format-number",
                         xsltFormatNumberFunction);
    xmlXPathRegisterFunc(ctxt, (const xmlChar *) "generate-id",
                         xsltGenerateIdFunction);
    xmlXPathRegisterFunc(ctxt, (const xmlChar *) "system-property",
                         xsltSystemPropertyFunction);
    xmlXPathRegisterFunc(ctxt, (const xmlChar *) "element-available",
                         xsltElementAvailableFunction);
    xmlXPathRegisterFunc(ctxt, (const xmlChar *) "function-available",
                         xsltFunctionAvailableFunction);
}
