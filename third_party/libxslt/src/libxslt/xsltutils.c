/*
 * xsltutils.c: Utilities for the XSL Transformation 1.0 engine
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

#ifndef	XSLT_NEED_TRIO
#include <stdio.h>
#else
#include <trio.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlIO.h>
#include "xsltutils.h"
#include "templates.h"
#include "xsltInternals.h"
#include "imports.h"
#include "transform.h"

#if defined(_WIN32)
#include <windows.h>
#define XSLT_WIN32_PERFORMANCE_COUNTER
#endif

/************************************************************************
 *									*
 *			Convenience function				*
 *									*
 ************************************************************************/

/**
 * xsltGetCNsProp:
 * @style: the stylesheet
 * @node:  the node
 * @name:  the attribute name
 * @nameSpace:  the URI of the namespace
 *
 * Similar to xmlGetNsProp() but with a slightly different semantic
 *
 * Search and get the value of an attribute associated to a node
 * This attribute has to be anchored in the namespace specified,
 * or has no namespace and the element is in that namespace.
 *
 * This does the entity substitution.
 * This function looks in DTD attribute declaration for #FIXED or
 * default declaration values unless DTD use has been turned off.
 *
 * Returns the attribute value or NULL if not found. The string is allocated
 *         in the stylesheet dictionary.
 */
const xmlChar *
xsltGetCNsProp(xsltStylesheetPtr style, xmlNodePtr node,
              const xmlChar *name, const xmlChar *nameSpace) {
    xmlAttrPtr prop;
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlChar *tmp;
    const xmlChar *ret;

    if ((node == NULL) || (style == NULL) || (style->dict == NULL))
	return(NULL);

    if (nameSpace == NULL)
        return xmlGetProp(node, name);

    if (node->type == XML_NAMESPACE_DECL)
        return(NULL);
    if (node->type == XML_ELEMENT_NODE)
	prop = node->properties;
    else
	prop = NULL;
    while (prop != NULL) {
	/*
	 * One need to have
	 *   - same attribute names
	 *   - and the attribute carrying that namespace
	 */
        if ((xmlStrEqual(prop->name, name)) &&
	    (((prop->ns == NULL) && (node->ns != NULL) &&
	      (xmlStrEqual(node->ns->href, nameSpace))) ||
	     ((prop->ns != NULL) &&
	      (xmlStrEqual(prop->ns->href, nameSpace))))) {

	    tmp = xmlNodeListGetString(node->doc, prop->children, 1);
	    if (tmp == NULL)
	        ret = xmlDictLookup(style->dict, BAD_CAST "", 0);
	    else {
	        ret = xmlDictLookup(style->dict, tmp, -1);
		xmlFree(tmp);
	    }
	    return ret;
        }
	prop = prop->next;
    }
    tmp = NULL;
    /*
     * Check if there is a default declaration in the internal
     * or external subsets
     */
    doc =  node->doc;
    if (doc != NULL) {
        if (doc->intSubset != NULL) {
	    xmlAttributePtr attrDecl;

	    attrDecl = xmlGetDtdAttrDesc(doc->intSubset, node->name, name);
	    if ((attrDecl == NULL) && (doc->extSubset != NULL))
		attrDecl = xmlGetDtdAttrDesc(doc->extSubset, node->name, name);

	    if ((attrDecl != NULL) && (attrDecl->prefix != NULL)) {
	        /*
		 * The DTD declaration only allows a prefix search
		 */
		ns = xmlSearchNs(doc, node, attrDecl->prefix);
		if ((ns != NULL) && (xmlStrEqual(ns->href, nameSpace)))
		    return(xmlDictLookup(style->dict,
		                         attrDecl->defaultValue, -1));
	    }
	}
    }
    return(NULL);
}
/**
 * xsltGetNsProp:
 * @node:  the node
 * @name:  the attribute name
 * @nameSpace:  the URI of the namespace
 *
 * Similar to xmlGetNsProp() but with a slightly different semantic
 *
 * Search and get the value of an attribute associated to a node
 * This attribute has to be anchored in the namespace specified,
 * or has no namespace and the element is in that namespace.
 *
 * This does the entity substitution.
 * This function looks in DTD attribute declaration for #FIXED or
 * default declaration values unless DTD use has been turned off.
 *
 * Returns the attribute value or NULL if not found.
 *     It's up to the caller to free the memory.
 */
xmlChar *
xsltGetNsProp(xmlNodePtr node, const xmlChar *name, const xmlChar *nameSpace) {
    xmlAttrPtr prop;
    xmlDocPtr doc;
    xmlNsPtr ns;

    if (node == NULL)
	return(NULL);

    if (nameSpace == NULL)
        return xmlGetProp(node, name);

    if (node->type == XML_NAMESPACE_DECL)
        return(NULL);
    if (node->type == XML_ELEMENT_NODE)
	prop = node->properties;
    else
	prop = NULL;
    /*
    * TODO: Substitute xmlGetProp() for xmlGetNsProp(), since the former
    * is not namespace-aware and will return an attribute with equal
    * name regardless of its namespace.
    * Example:
    *   <xsl:element foo:name="myName"/>
    *   So this would return "myName" even if an attribute @name
    *   in the XSLT was requested.
    */
    while (prop != NULL) {
	/*
	 * One need to have
	 *   - same attribute names
	 *   - and the attribute carrying that namespace
	 */
        if ((xmlStrEqual(prop->name, name)) &&
	    (((prop->ns == NULL) && (node->ns != NULL) &&
	      (xmlStrEqual(node->ns->href, nameSpace))) ||
	     ((prop->ns != NULL) &&
	      (xmlStrEqual(prop->ns->href, nameSpace))))) {
	    xmlChar *ret;

	    ret = xmlNodeListGetString(node->doc, prop->children, 1);
	    if (ret == NULL) return(xmlStrdup((xmlChar *)""));
	    return(ret);
        }
	prop = prop->next;
    }

    /*
     * Check if there is a default declaration in the internal
     * or external subsets
     */
    doc =  node->doc;
    if (doc != NULL) {
        if (doc->intSubset != NULL) {
	    xmlAttributePtr attrDecl;

	    attrDecl = xmlGetDtdAttrDesc(doc->intSubset, node->name, name);
	    if ((attrDecl == NULL) && (doc->extSubset != NULL))
		attrDecl = xmlGetDtdAttrDesc(doc->extSubset, node->name, name);

	    if ((attrDecl != NULL) && (attrDecl->prefix != NULL)) {
	        /*
		 * The DTD declaration only allows a prefix search
		 */
		ns = xmlSearchNs(doc, node, attrDecl->prefix);
		if ((ns != NULL) && (xmlStrEqual(ns->href, nameSpace)))
		    return(xmlStrdup(attrDecl->defaultValue));
	    }
	}
    }
    return(NULL);
}

/**
 * xsltGetUTF8Char:
 * @utf:  a sequence of UTF-8 encoded bytes
 * @len:  a pointer to @bytes len
 *
 * Read one UTF8 Char from @utf
 * Function copied from libxml2 xmlGetUTF8Char() ... to discard ultimately
 * and use the original API
 *
 * Returns the char value or -1 in case of error and update @len with the
 *        number of bytes used
 */
int
xsltGetUTF8Char(const unsigned char *utf, int *len) {
    unsigned int c;

    if (utf == NULL)
	goto error;
    if (len == NULL)
	goto error;
    if (*len < 1)
	goto error;

    c = utf[0];
    if (c & 0x80) {
	if (*len < 2)
	    goto error;
	if ((utf[1] & 0xc0) != 0x80)
	    goto error;
	if ((c & 0xe0) == 0xe0) {
	    if (*len < 3)
		goto error;
	    if ((utf[2] & 0xc0) != 0x80)
		goto error;
	    if ((c & 0xf0) == 0xf0) {
		if (*len < 4)
		    goto error;
		if ((c & 0xf8) != 0xf0 || (utf[3] & 0xc0) != 0x80)
		    goto error;
		*len = 4;
		/* 4-byte code */
		c = (utf[0] & 0x7) << 18;
		c |= (utf[1] & 0x3f) << 12;
		c |= (utf[2] & 0x3f) << 6;
		c |= utf[3] & 0x3f;
	    } else {
	      /* 3-byte code */
		*len = 3;
		c = (utf[0] & 0xf) << 12;
		c |= (utf[1] & 0x3f) << 6;
		c |= utf[2] & 0x3f;
	    }
	} else {
	  /* 2-byte code */
	    *len = 2;
	    c = (utf[0] & 0x1f) << 6;
	    c |= utf[1] & 0x3f;
	}
    } else {
	/* 1-byte code */
	*len = 1;
    }
    return(c);

error:
    if (len != NULL)
	*len = 0;
    return(-1);
}

/**
 * xsltGetUTF8CharZ:
 * @utf:  a sequence of UTF-8 encoded bytes
 * @len:  a pointer to @bytes len
 *
 * Read one UTF8 Char from a null-terminated string.
 *
 * Returns the char value or -1 in case of error and update @len with the
 *        number of bytes used
 */
int
xsltGetUTF8CharZ(const unsigned char *utf, int *len) {
    unsigned int c;

    if (utf == NULL)
	goto error;
    if (len == NULL)
	goto error;

    c = utf[0];
    if (c & 0x80) {
	if ((utf[1] & 0xc0) != 0x80)
	    goto error;
	if ((c & 0xe0) == 0xe0) {
	    if ((utf[2] & 0xc0) != 0x80)
		goto error;
	    if ((c & 0xf0) == 0xf0) {
		if ((c & 0xf8) != 0xf0 || (utf[3] & 0xc0) != 0x80)
		    goto error;
		*len = 4;
		/* 4-byte code */
		c = (utf[0] & 0x7) << 18;
		c |= (utf[1] & 0x3f) << 12;
		c |= (utf[2] & 0x3f) << 6;
		c |= utf[3] & 0x3f;
	    } else {
	      /* 3-byte code */
		*len = 3;
		c = (utf[0] & 0xf) << 12;
		c |= (utf[1] & 0x3f) << 6;
		c |= utf[2] & 0x3f;
	    }
	} else {
	  /* 2-byte code */
	    *len = 2;
	    c = (utf[0] & 0x1f) << 6;
	    c |= utf[1] & 0x3f;
	}
    } else {
	/* 1-byte code */
	*len = 1;
    }
    return(c);

error:
    if (len != NULL)
	*len = 0;
    return(-1);
}

#ifdef XSLT_REFACTORED

/**
 * xsltPointerListAddSize:
 * @list: the pointer list structure
 * @item: the item to be stored
 * @initialSize: the initial size of the list
 *
 * Adds an item to the list.
 *
 * Returns the position of the added item in the list or
 *         -1 in case of an error.
 */
int
xsltPointerListAddSize(xsltPointerListPtr list,
		       void *item,
		       int initialSize)
{
    if (list->items == NULL) {
	if (initialSize <= 0)
	    initialSize = 1;
	list->items = (void **) xmlMalloc(
	    initialSize * sizeof(void *));
	if (list->items == NULL) {
	    xsltGenericError(xsltGenericErrorContext,
	     "xsltPointerListAddSize: memory allocation failure.\n");
	    return(-1);
	}
	list->number = 0;
	list->size = initialSize;
    } else if (list->size <= list->number) {
	list->size *= 2;
	list->items = (void **) xmlRealloc(list->items,
	    list->size * sizeof(void *));
	if (list->items == NULL) {
	    xsltGenericError(xsltGenericErrorContext,
	     "xsltPointerListAddSize: memory re-allocation failure.\n");
	    list->size = 0;
	    return(-1);
	}
    }
    list->items[list->number++] = item;
    return(0);
}

/**
 * xsltPointerListCreate:
 * @initialSize: the initial size for the list
 *
 * Creates an xsltPointerList structure.
 *
 * Returns a xsltPointerList structure or NULL in case of an error.
 */
xsltPointerListPtr
xsltPointerListCreate(int initialSize)
{
    xsltPointerListPtr ret;

    ret = xmlMalloc(sizeof(xsltPointerList));
    if (ret == NULL) {
	xsltGenericError(xsltGenericErrorContext,
	     "xsltPointerListCreate: memory allocation failure.\n");
	return (NULL);
    }
    memset(ret, 0, sizeof(xsltPointerList));
    if (initialSize > 0) {
	xsltPointerListAddSize(ret, NULL, initialSize);
	ret->number = 0;
    }
    return (ret);
}

/**
 * xsltPointerListFree:
 * @list: pointer to the list to be freed
 *
 * Frees the xsltPointerList structure. This does not free
 * the content of the list.
 */
void
xsltPointerListFree(xsltPointerListPtr list)
{
    if (list == NULL)
	return;
    if (list->items != NULL)
	xmlFree(list->items);
    xmlFree(list);
}

/**
 * xsltPointerListClear:
 * @list: pointer to the list to be cleared
 *
 * Resets the list, but does not free the allocated array
 * and does not free the content of the list.
 */
void
xsltPointerListClear(xsltPointerListPtr list)
{
    if (list->items != NULL) {
	xmlFree(list->items);
	list->items = NULL;
    }
    list->number = 0;
    list->size = 0;
}

#endif /* XSLT_REFACTORED */

/************************************************************************
 *									*
 *		Handling of XSLT stylesheets messages			*
 *									*
 ************************************************************************/

/**
 * xsltMessage:
 * @ctxt:  an XSLT processing context
 * @node:  The current node
 * @inst:  The node containing the message instruction
 *
 * Process and xsl:message construct
 */
void
xsltMessage(xsltTransformContextPtr ctxt, xmlNodePtr node, xmlNodePtr inst) {
    xmlGenericErrorFunc error = xsltGenericError;
    void *errctx = xsltGenericErrorContext;
    xmlChar *prop, *message;
    int terminate = 0;

    if ((ctxt == NULL) || (inst == NULL))
	return;

    if (ctxt->error != NULL) {
	error = ctxt->error;
	errctx = ctxt->errctx;
    }

    prop = xmlGetNsProp(inst, (const xmlChar *)"terminate", NULL);
    if (prop != NULL) {
	if (xmlStrEqual(prop, (const xmlChar *)"yes")) {
	    terminate = 1;
	} else if (xmlStrEqual(prop, (const xmlChar *)"no")) {
	    terminate = 0;
	} else {
	    xsltTransformError(ctxt, NULL, inst,
		"xsl:message : terminate expecting 'yes' or 'no'\n");
	}
	xmlFree(prop);
    }
    message = xsltEvalTemplateString(ctxt, node, inst);
    if (message != NULL) {
	int len = xmlStrlen(message);

	error(errctx, "%s", (const char *)message);
	if ((len > 0) && (message[len - 1] != '\n'))
	    error(errctx, "\n");
	xmlFree(message);
    }
    if (terminate)
	ctxt->state = XSLT_STATE_STOPPED;
}

/************************************************************************
 *									*
 *		Handling of out of context errors			*
 *									*
 ************************************************************************/

#define XSLT_GET_VAR_STR(msg, str) {				\
    int       size;						\
    int       chars;						\
    char      *larger;						\
    va_list   ap;						\
								\
    str = (char *) xmlMalloc(150);				\
    if (str == NULL)						\
	return;							\
								\
    size = 150;							\
								\
    while (size < 64000) {					\
	va_start(ap, msg);					\
	chars = vsnprintf(str, size, msg, ap);			\
	va_end(ap);						\
	if ((chars > -1) && (chars < size))			\
	    break;						\
	if (chars > -1)						\
	    size += chars + 1;					\
	else							\
	    size += 100;					\
	if ((larger = (char *) xmlRealloc(str, size)) == NULL) {\
	    xmlFree(str);					\
	    return;						\
	}							\
	str = larger;						\
    }								\
}
/**
 * xsltGenericErrorDefaultFunc:
 * @ctx:  an error context
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 *
 * Default handler for out of context error messages.
 */
static void LIBXSLT_ATTR_FORMAT(2,3)
xsltGenericErrorDefaultFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...) {
    va_list args;

    if (xsltGenericErrorContext == NULL)
	xsltGenericErrorContext = (void *) stderr;

    va_start(args, msg);
    vfprintf((FILE *)xsltGenericErrorContext, msg, args);
    va_end(args);
}

xmlGenericErrorFunc xsltGenericError = xsltGenericErrorDefaultFunc;
void *xsltGenericErrorContext = NULL;


/**
 * xsltSetGenericErrorFunc:
 * @ctx:  the new error handling context
 * @handler:  the new handler function
 *
 * Function to reset the handler and the error context for out of
 * context error messages.
 * This simply means that @handler will be called for subsequent
 * error messages while not parsing nor validating. And @ctx will
 * be passed as first argument to @handler
 * One can simply force messages to be emitted to another FILE * than
 * stderr by setting @ctx to this file handle and @handler to NULL.
 */
void
xsltSetGenericErrorFunc(void *ctx, xmlGenericErrorFunc handler) {
    xsltGenericErrorContext = ctx;
    if (handler != NULL)
	xsltGenericError = handler;
    else
	xsltGenericError = xsltGenericErrorDefaultFunc;
}

/**
 * xsltGenericDebugDefaultFunc:
 * @ctx:  an error context
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 *
 * Default handler for out of context error messages.
 */
static void LIBXSLT_ATTR_FORMAT(2,3)
xsltGenericDebugDefaultFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...) {
    va_list args;

    if (xsltGenericDebugContext == NULL)
	return;

    va_start(args, msg);
    vfprintf((FILE *)xsltGenericDebugContext, msg, args);
    va_end(args);
}

xmlGenericErrorFunc xsltGenericDebug = xsltGenericDebugDefaultFunc;
void *xsltGenericDebugContext = NULL;


/**
 * xsltSetGenericDebugFunc:
 * @ctx:  the new error handling context
 * @handler:  the new handler function
 *
 * Function to reset the handler and the error context for out of
 * context error messages.
 * This simply means that @handler will be called for subsequent
 * error messages while not parsing or validating. And @ctx will
 * be passed as first argument to @handler
 * One can simply force messages to be emitted to another FILE * than
 * stderr by setting @ctx to this file handle and @handler to NULL.
 */
void
xsltSetGenericDebugFunc(void *ctx, xmlGenericErrorFunc handler) {
    xsltGenericDebugContext = ctx;
    if (handler != NULL)
	xsltGenericDebug = handler;
    else
	xsltGenericDebug = xsltGenericDebugDefaultFunc;
}

/**
 * xsltPrintErrorContext:
 * @ctxt:  the transformation context
 * @style:  the stylesheet
 * @node:  the current node being processed
 *
 * Display the context of an error.
 */
void
xsltPrintErrorContext(xsltTransformContextPtr ctxt,
	              xsltStylesheetPtr style, xmlNodePtr node) {
    int line = 0;
    const xmlChar *file = NULL;
    const xmlChar *name = NULL;
    const char *type = "error";
    xmlGenericErrorFunc error = xsltGenericError;
    void *errctx = xsltGenericErrorContext;

    if (ctxt != NULL) {
        if (ctxt->state == XSLT_STATE_OK)
	    ctxt->state = XSLT_STATE_ERROR;
	if (ctxt->error != NULL) {
	    error = ctxt->error;
	    errctx = ctxt->errctx;
	}
    }
    if ((node == NULL) && (ctxt != NULL))
	node = ctxt->inst;

    if (node != NULL)  {
	if ((node->type == XML_DOCUMENT_NODE) ||
	    (node->type == XML_HTML_DOCUMENT_NODE)) {
	    xmlDocPtr doc = (xmlDocPtr) node;

	    file = doc->URL;
	} else {
	    line = xmlGetLineNo(node);
	    if ((node->doc != NULL) && (node->doc->URL != NULL))
		file = node->doc->URL;
	    if (node->name != NULL)
		name = node->name;
	}
    }

    if (ctxt != NULL)
	type = "runtime error";
    else if (style != NULL) {
#ifdef XSLT_REFACTORED
	if (XSLT_CCTXT(style)->errSeverity == XSLT_ERROR_SEVERITY_WARNING)
	    type = "compilation warning";
	else
	    type = "compilation error";
#else
	type = "compilation error";
#endif
    }

    if ((file != NULL) && (line != 0) && (name != NULL))
	error(errctx, "%s: file %s line %d element %s\n",
	      type, file, line, name);
    else if ((file != NULL) && (name != NULL))
	error(errctx, "%s: file %s element %s\n", type, file, name);
    else if ((file != NULL) && (line != 0))
	error(errctx, "%s: file %s line %d\n", type, file, line);
    else if (file != NULL)
	error(errctx, "%s: file %s\n", type, file);
    else if (name != NULL)
	error(errctx, "%s: element %s\n", type, name);
    else
	error(errctx, "%s\n", type);
}

/**
 * xsltSetTransformErrorFunc:
 * @ctxt:  the XSLT transformation context
 * @ctx:  the new error handling context
 * @handler:  the new handler function
 *
 * Function to reset the handler and the error context for out of
 * context error messages specific to a given XSLT transromation.
 *
 * This simply means that @handler will be called for subsequent
 * error messages while running the transformation.
 */
void
xsltSetTransformErrorFunc(xsltTransformContextPtr ctxt,
                          void *ctx, xmlGenericErrorFunc handler)
{
    ctxt->error = handler;
    ctxt->errctx = ctx;
}

/**
 * xsltTransformError:
 * @ctxt:  an XSLT transformation context
 * @style:  the XSLT stylesheet used
 * @node:  the current node in the stylesheet
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 *
 * Display and format an error messages, gives file, line, position and
 * extra parameters, will use the specific transformation context if available
 */
void
xsltTransformError(xsltTransformContextPtr ctxt,
		   xsltStylesheetPtr style,
		   xmlNodePtr node,
		   const char *msg, ...) {
    xmlGenericErrorFunc error = xsltGenericError;
    void *errctx = xsltGenericErrorContext;
    char * str;

    if (ctxt != NULL) {
        if (ctxt->state == XSLT_STATE_OK)
	    ctxt->state = XSLT_STATE_ERROR;
	if (ctxt->error != NULL) {
	    error = ctxt->error;
	    errctx = ctxt->errctx;
	}
    }
    if ((node == NULL) && (ctxt != NULL))
	node = ctxt->inst;
    xsltPrintErrorContext(ctxt, style, node);
    XSLT_GET_VAR_STR(msg, str);
    error(errctx, "%s", str);
    if (str != NULL)
	xmlFree(str);
}

/************************************************************************
 *									*
 *				QNames					*
 *									*
 ************************************************************************/

/**
 * xsltSplitQName:
 * @dict: a dictionary
 * @name:  the full QName
 * @prefix: the return value
 *
 * Split QNames into prefix and local names, both allocated from a dictionary.
 *
 * Returns: the localname or NULL in case of error.
 */
const xmlChar *
xsltSplitQName(xmlDictPtr dict, const xmlChar *name, const xmlChar **prefix) {
    int len = 0;
    const xmlChar *ret = NULL;

    *prefix = NULL;
    if ((name == NULL) || (dict == NULL)) return(NULL);
    if (name[0] == ':')
        return(xmlDictLookup(dict, name, -1));
    while ((name[len] != 0) && (name[len] != ':')) len++;
    if (name[len] == 0) return(xmlDictLookup(dict, name, -1));
    *prefix = xmlDictLookup(dict, name, len);
    ret = xmlDictLookup(dict, &name[len + 1], -1);
    return(ret);
}

/**
 * xsltGetQNameURI:
 * @node:  the node holding the QName
 * @name:  pointer to the initial QName value
 *
 * This function analyzes @name, if the name contains a prefix,
 * the function seaches the associated namespace in scope for it.
 * It will also replace @name value with the NCName, the old value being
 * freed.
 * Errors in the prefix lookup are signalled by setting @name to NULL.
 *
 * NOTE: the namespace returned is a pointer to the place where it is
 *       defined and hence has the same lifespan as the document holding it.
 *
 * Returns the namespace URI if there is a prefix, or NULL if @name is
 *         not prefixed.
 */
const xmlChar *
xsltGetQNameURI(xmlNodePtr node, xmlChar ** name)
{
    int len = 0;
    xmlChar *qname;
    xmlNsPtr ns;

    if (name == NULL)
	return(NULL);
    qname = *name;
    if ((qname == NULL) || (*qname == 0))
	return(NULL);
    if (node == NULL) {
	xsltGenericError(xsltGenericErrorContext,
		         "QName: no element for namespace lookup %s\n",
			 qname);
	xmlFree(qname);
	*name = NULL;
	return(NULL);
    }

    /* nasty but valid */
    if (qname[0] == ':')
	return(NULL);

    /*
     * we are not trying to validate but just to cut, and yes it will
     * work even if this is a set of UTF-8 encoded chars
     */
    while ((qname[len] != 0) && (qname[len] != ':'))
	len++;

    if (qname[len] == 0)
	return(NULL);

    /*
     * handle xml: separately, this one is magical
     */
    if ((qname[0] == 'x') && (qname[1] == 'm') &&
        (qname[2] == 'l') && (qname[3] == ':')) {
	if (qname[4] == 0)
	    return(NULL);
        *name = xmlStrdup(&qname[4]);
	xmlFree(qname);
	return(XML_XML_NAMESPACE);
    }

    qname[len] = 0;
    ns = xmlSearchNs(node->doc, node, qname);
    if (ns == NULL) {
	xsltGenericError(xsltGenericErrorContext,
		"%s:%s : no namespace bound to prefix %s\n",
		         qname, &qname[len + 1], qname);
	*name = NULL;
	xmlFree(qname);
	return(NULL);
    }
    *name = xmlStrdup(&qname[len + 1]);
    xmlFree(qname);
    return(ns->href);
}

/**
 * xsltGetQNameURI2:
 * @style:  stylesheet pointer
 * @node:   the node holding the QName
 * @name:   pointer to the initial QName value
 *
 * This function is similar to xsltGetQNameURI, but is used when
 * @name is a dictionary entry.
 *
 * Returns the namespace URI if there is a prefix, or NULL if @name is
 * not prefixed.
 */
const xmlChar *
xsltGetQNameURI2(xsltStylesheetPtr style, xmlNodePtr node,
		 const xmlChar **name) {
    int len = 0;
    xmlChar *qname;
    xmlNsPtr ns;

    if (name == NULL)
        return(NULL);
    qname = (xmlChar *)*name;
    if ((qname == NULL) || (*qname == 0))
        return(NULL);
    if (node == NULL) {
        xsltGenericError(xsltGenericErrorContext,
                         "QName: no element for namespace lookup %s\n",
                          qname);
	*name = NULL;
	return(NULL);
    }

    /*
     * we are not trying to validate but just to cut, and yes it will
     * work even if this is a set of UTF-8 encoded chars
     */
    while ((qname[len] != 0) && (qname[len] != ':'))
        len++;

    if (qname[len] == 0)
        return(NULL);

    /*
     * handle xml: separately, this one is magical
     */
    if ((qname[0] == 'x') && (qname[1] == 'm') &&
        (qname[2] == 'l') && (qname[3] == ':')) {
        if (qname[4] == 0)
            return(NULL);
        *name = xmlDictLookup(style->dict, &qname[4], -1);
        return(XML_XML_NAMESPACE);
    }

    qname = xmlStrndup(*name, len);
    ns = xmlSearchNs(node->doc, node, qname);
    if (ns == NULL) {
	if (style) {
	    xsltTransformError(NULL, style, node,
		"No namespace bound to prefix '%s'.\n",
		qname);
	    style->errors++;
	} else {
	    xsltGenericError(xsltGenericErrorContext,
                "%s : no namespace bound to prefix %s\n",
		*name, qname);
	}
        *name = NULL;
        xmlFree(qname);
        return(NULL);
    }
    *name = xmlDictLookup(style->dict, (*name)+len+1, -1);
    xmlFree(qname);
    return(ns->href);
}

/************************************************************************
 *									*
 *				Sorting					*
 *									*
 ************************************************************************/

/**
 * xsltDocumentSortFunction:
 * @list:  the node set
 *
 * reorder the current node list @list accordingly to the document order
 * This function is slow, obsolete and should not be used anymore.
 */
void
xsltDocumentSortFunction(xmlNodeSetPtr list) {
    int i, j;
    int len, tst;
    xmlNodePtr node;

    if (list == NULL)
	return;
    len = list->nodeNr;
    if (len <= 1)
	return;
    /* TODO: sort is really not optimized, does it needs to ? */
    for (i = 0;i < len -1;i++) {
	for (j = i + 1; j < len; j++) {
	    tst = xmlXPathCmpNodes(list->nodeTab[i], list->nodeTab[j]);
	    if (tst == -1) {
		node = list->nodeTab[i];
		list->nodeTab[i] = list->nodeTab[j];
		list->nodeTab[j] = node;
	    }
	}
    }
}

/**
 * xsltComputeSortResultInternal:
 * @ctxt:  a XSLT process context
 * @sort:  xsl:sort node
 * @number:  data-type is number
 * @locale:  transform strings according to locale
 *
 * reorder the current node list accordingly to the set of sorting
 * requirement provided by the array of nodes.
 *
 * Returns a ordered XPath nodeset or NULL in case of error.
 */
static xmlXPathObjectPtr *
xsltComputeSortResultInternal(xsltTransformContextPtr ctxt, xmlNodePtr sort,
                              int number, void *locale) {
#ifdef XSLT_REFACTORED
    xsltStyleItemSortPtr comp;
#else
    const xsltStylePreComp *comp;
#endif
    xmlXPathObjectPtr *results = NULL;
    xmlNodeSetPtr list = NULL;
    xmlXPathObjectPtr res;
    int len = 0;
    int i;
    xmlNodePtr oldNode;
    xmlNodePtr oldInst;
    int	oldPos, oldSize ;
    int oldNsNr;
    xmlNsPtr *oldNamespaces;

    comp = sort->psvi;
    if (comp == NULL) {
	xsltGenericError(xsltGenericErrorContext,
	     "xsl:sort : compilation failed\n");
	return(NULL);
    }

    if ((comp->select == NULL) || (comp->comp == NULL))
	return(NULL);

    list = ctxt->nodeList;
    if ((list == NULL) || (list->nodeNr <= 1))
	return(NULL);

    len = list->nodeNr;

    /* TODO: xsl:sort lang attribute */
    /* TODO: xsl:sort case-order attribute */


    results = xmlMalloc(len * sizeof(xmlXPathObjectPtr));
    if (results == NULL) {
	xsltGenericError(xsltGenericErrorContext,
	     "xsltComputeSortResult: memory allocation failure\n");
	return(NULL);
    }

    oldNode = ctxt->node;
    oldInst = ctxt->inst;
    oldPos = ctxt->xpathCtxt->proximityPosition;
    oldSize = ctxt->xpathCtxt->contextSize;
    oldNsNr = ctxt->xpathCtxt->nsNr;
    oldNamespaces = ctxt->xpathCtxt->namespaces;
    for (i = 0;i < len;i++) {
	ctxt->inst = sort;
	ctxt->xpathCtxt->contextSize = len;
	ctxt->xpathCtxt->proximityPosition = i + 1;
	ctxt->node = list->nodeTab[i];
	ctxt->xpathCtxt->node = ctxt->node;
#ifdef XSLT_REFACTORED
	if (comp->inScopeNs != NULL) {
	    ctxt->xpathCtxt->namespaces = comp->inScopeNs->list;
	    ctxt->xpathCtxt->nsNr = comp->inScopeNs->xpathNumber;
	} else {
	    ctxt->xpathCtxt->namespaces = NULL;
	    ctxt->xpathCtxt->nsNr = 0;
	}
#else
	ctxt->xpathCtxt->namespaces = comp->nsList;
	ctxt->xpathCtxt->nsNr = comp->nsNr;
#endif
	res = xmlXPathCompiledEval(comp->comp, ctxt->xpathCtxt);
	if (res != NULL) {
	    if (res->type != XPATH_STRING)
		res = xmlXPathConvertString(res);
	    if (number)
		res = xmlXPathConvertNumber(res);
        }
        if (res != NULL) {
	    res->index = i;	/* Save original pos for dupl resolv */
	    if (number) {
		if (res->type == XPATH_NUMBER) {
		    results[i] = res;
		} else {
#ifdef WITH_XSLT_DEBUG_PROCESS
		    xsltGenericDebug(xsltGenericDebugContext,
			"xsltComputeSortResult: select didn't evaluate to a number\n");
#endif
		    results[i] = NULL;
		}
	    } else {
		if (res->type == XPATH_STRING) {
		    if (locale != NULL) {
			xmlChar *str = res->stringval;
                        xmlChar *sortKey = ctxt->genSortKey(locale, str);

                        if (sortKey == NULL) {
                            xsltTransformError(ctxt, NULL, sort,
                                "xsltComputeSortResult: sort key is null\n");
                        } else {
                            res->stringval = sortKey;
                            xmlFree(str);
                        }
		    }

		    results[i] = res;
		} else {
#ifdef WITH_XSLT_DEBUG_PROCESS
		    xsltGenericDebug(xsltGenericDebugContext,
			"xsltComputeSortResult: select didn't evaluate to a string\n");
#endif
		    results[i] = NULL;
		}
	    }
	} else {
	    ctxt->state = XSLT_STATE_STOPPED;
	    results[i] = NULL;
	}
    }
    ctxt->node = oldNode;
    ctxt->inst = oldInst;
    ctxt->xpathCtxt->contextSize = oldSize;
    ctxt->xpathCtxt->proximityPosition = oldPos;
    ctxt->xpathCtxt->nsNr = oldNsNr;
    ctxt->xpathCtxt->namespaces = oldNamespaces;

    return(results);
}

/**
 * xsltComputeSortResult:
 * @ctxt:  a XSLT process context
 * @sort:  node list
 *
 * reorder the current node list accordingly to the set of sorting
 * requirement provided by the array of nodes.
 *
 * Returns a ordered XPath nodeset or NULL in case of error.
 */
xmlXPathObjectPtr *
xsltComputeSortResult(xsltTransformContextPtr ctxt, xmlNodePtr sort) {
    const xsltStylePreComp *comp = sort->psvi;
    int number = 0;

    if (comp != NULL)
        number = comp->number;
    return xsltComputeSortResultInternal(ctxt, sort, number,
                                         /* locale */ NULL);
}

/**
 * xsltDefaultSortFunction:
 * @ctxt:  a XSLT process context
 * @sorts:  array of sort nodes
 * @nbsorts:  the number of sorts in the array
 *
 * reorder the current node list accordingly to the set of sorting
 * requirement provided by the arry of nodes.
 */
void
xsltDefaultSortFunction(xsltTransformContextPtr ctxt, xmlNodePtr *sorts,
	           int nbsorts) {
#ifdef XSLT_REFACTORED
    xsltStyleItemSortPtr comp;
#else
    const xsltStylePreComp *comp;
#endif
    xmlXPathObjectPtr *resultsTab[XSLT_MAX_SORT];
    xmlXPathObjectPtr *results = NULL, *res;
    xmlNodeSetPtr list = NULL;
    int len = 0;
    int i, j, incr;
    int tst;
    int depth;
    xmlNodePtr node;
    xmlXPathObjectPtr tmp;
    int number[XSLT_MAX_SORT], desc[XSLT_MAX_SORT];
    void *locale[XSLT_MAX_SORT];

    if ((ctxt == NULL) || (sorts == NULL) || (nbsorts <= 0) ||
	(nbsorts >= XSLT_MAX_SORT))
	return;
    if (sorts[0] == NULL)
	return;
    comp = sorts[0]->psvi;
    if (comp == NULL)
	return;

    list = ctxt->nodeList;
    if ((list == NULL) || (list->nodeNr <= 1))
	return; /* nothing to do */

    for (j = 0; j < nbsorts; j++) {
        xmlChar *lang;

	comp = sorts[j]->psvi;
	if ((comp->stype == NULL) && (comp->has_stype != 0)) {
	    xmlChar *stype =
		xsltEvalAttrValueTemplate(ctxt, sorts[j],
					  BAD_CAST "data-type", NULL);
	    number[j] = 0;
	    if (stype != NULL) {
		if (xmlStrEqual(stype, (const xmlChar *) "text"))
		    ;
		else if (xmlStrEqual(stype, (const xmlChar *) "number"))
		    number[j] = 1;
		else {
		    xsltTransformError(ctxt, NULL, sorts[j],
			  "xsltDoSortFunction: no support for data-type = %s\n",
			  stype);
		}
                xmlFree(stype);
	    }
	} else {
	    number[j] = comp->number;
        }
	if ((comp->order == NULL) && (comp->has_order != 0)) {
	    xmlChar *order = xsltEvalAttrValueTemplate(ctxt, sorts[j],
                                                       BAD_CAST "order", NULL);
	    desc[j] = 0;
	    if (order != NULL) {
		if (xmlStrEqual(order, (const xmlChar *) "ascending"))
		    ;
		else if (xmlStrEqual(order, (const xmlChar *) "descending"))
		    desc[j] = 1;
		else {
		    xsltTransformError(ctxt, NULL, sorts[j],
			     "xsltDoSortFunction: invalid value %s for order\n",
			     order);
		}
                xmlFree(order);
	    }
	} else {
	    desc[j] = comp->descending;
	}
	if ((comp->lang == NULL) && (comp->has_lang != 0)) {
            lang = xsltEvalAttrValueTemplate(ctxt, sorts[j],
						      (xmlChar *) "lang",
						      NULL);
	} else {
            lang = (xmlChar *) comp->lang;
        }
        if (lang != NULL) {
            locale[j] = ctxt->newLocale(lang, comp->lower_first);
            if (lang != comp->lang)
                xmlFree(lang);
        } else {
            locale[j] = NULL;
        }
    }

    len = list->nodeNr;

    resultsTab[0] = xsltComputeSortResultInternal(ctxt, sorts[0], number[0],
                                                  locale[0]);
    for (i = 1;i < XSLT_MAX_SORT;i++)
	resultsTab[i] = NULL;

    results = resultsTab[0];

    comp = sorts[0]->psvi;
    if (results == NULL)
	goto cleanup;

    /* Shell's sort of node-set */
    for (incr = len / 2; incr > 0; incr /= 2) {
	for (i = incr; i < len; i++) {
	    j = i - incr;
	    if (results[i] == NULL)
		continue;

	    while (j >= 0) {
		if (results[j] == NULL)
		    tst = 1;
		else {
		    if (number[0]) {
			/* We make NaN smaller than number in accordance
			   with XSLT spec */
			if (xmlXPathIsNaN(results[j]->floatval)) {
			    if (xmlXPathIsNaN(results[j + incr]->floatval))
				tst = 0;
			    else
				tst = -1;
			} else if (xmlXPathIsNaN(results[j + incr]->floatval))
			    tst = 1;
			else if (results[j]->floatval ==
				results[j + incr]->floatval)
			    tst = 0;
			else if (results[j]->floatval >
				results[j + incr]->floatval)
			    tst = 1;
			else tst = -1;
		    } else {
			tst = xmlStrcmp(results[j]->stringval,
				     results[j + incr]->stringval);
		    }
		    if (desc[0])
			tst = -tst;
		}
		if (tst == 0) {
		    /*
		     * Okay we need to use multi level sorts
		     */
		    depth = 1;
		    while (depth < nbsorts) {
			if (sorts[depth] == NULL)
			    break;
			comp = sorts[depth]->psvi;
			if (comp == NULL)
			    break;

			/*
			 * Compute the result of the next level for the
			 * full set, this might be optimized ... or not
			 */
			if (resultsTab[depth] == NULL)
			    resultsTab[depth] =
                                xsltComputeSortResultInternal(ctxt,
                                                              sorts[depth],
                                                              number[depth],
                                                              locale[depth]);
			res = resultsTab[depth];
			if (res == NULL)
			    break;
			if (res[j] == NULL) {
			    if (res[j+incr] != NULL)
				tst = 1;
			} else if (res[j+incr] == NULL) {
			    tst = -1;
			} else {
			    if (number[depth]) {
				/* We make NaN smaller than number in
				   accordance with XSLT spec */
				if (xmlXPathIsNaN(res[j]->floatval)) {
				    if (xmlXPathIsNaN(res[j +
						incr]->floatval))
					tst = 0;
				    else
				        tst = -1;
				} else if (xmlXPathIsNaN(res[j + incr]->
						floatval))
				    tst = 1;
				else if (res[j]->floatval == res[j + incr]->
						floatval)
				    tst = 0;
				else if (res[j]->floatval >
					res[j + incr]->floatval)
				    tst = 1;
				else tst = -1;
			    } else {
				tst = xmlStrcmp(res[j]->stringval,
					     res[j + incr]->stringval);
			    }
			    if (desc[depth])
				tst = -tst;
			}

			/*
			 * if we still can't differenciate at this level
			 * try one level deeper.
			 */
			if (tst != 0)
			    break;
			depth++;
		    }
		}
		if (tst == 0) {
		    tst = results[j]->index > results[j + incr]->index;
		}
		if (tst > 0) {
		    tmp = results[j];
		    results[j] = results[j + incr];
		    results[j + incr] = tmp;
		    node = list->nodeTab[j];
		    list->nodeTab[j] = list->nodeTab[j + incr];
		    list->nodeTab[j + incr] = node;
		    depth = 1;
		    while (depth < nbsorts) {
			if (sorts[depth] == NULL)
			    break;
			if (resultsTab[depth] == NULL)
			    break;
			res = resultsTab[depth];
			tmp = res[j];
			res[j] = res[j + incr];
			res[j + incr] = tmp;
			depth++;
		    }
		    j -= incr;
		} else
		    break;
	    }
	}
    }

cleanup:
    for (j = 0; j < nbsorts; j++) {
        if (locale[j] != NULL) {
            ctxt->freeLocale(locale[j]);
        }
	if (resultsTab[j] != NULL) {
	    for (i = 0;i < len;i++)
		xmlXPathFreeObject(resultsTab[j][i]);
	    xmlFree(resultsTab[j]);
	}
    }
}


static xsltSortFunc xsltSortFunction = xsltDefaultSortFunction;

/**
 * xsltDoSortFunction:
 * @ctxt:  a XSLT process context
 * @sorts:  array of sort nodes
 * @nbsorts:  the number of sorts in the array
 *
 * reorder the current node list accordingly to the set of sorting
 * requirement provided by the arry of nodes.
 * This is a wrapper function, the actual function used is specified
 * using xsltSetCtxtSortFunc() to set the context specific sort function,
 * or xsltSetSortFunc() to set the global sort function.
 * If a sort function is set on the context, this will get called.
 * Otherwise the global sort function is called.
 */
void
xsltDoSortFunction(xsltTransformContextPtr ctxt, xmlNodePtr * sorts,
                   int nbsorts)
{
    if (ctxt->sortfunc != NULL)
	(ctxt->sortfunc)(ctxt, sorts, nbsorts);
    else if (xsltSortFunction != NULL)
        xsltSortFunction(ctxt, sorts, nbsorts);
}

/**
 * xsltSetSortFunc:
 * @handler:  the new handler function
 *
 * DEPRECATED: Use xsltSetCtxtLocaleHandlers.
 *
 * Function to reset the global handler for XSLT sorting.
 * If the handler is NULL, the default sort function will be used.
 */
void
xsltSetSortFunc(xsltSortFunc handler) {
    if (handler != NULL)
	xsltSortFunction = handler;
    else
	xsltSortFunction = xsltDefaultSortFunction;
}

/**
 * xsltSetCtxtSortFunc:
 * @ctxt:  a XSLT process context
 * @handler:  the new handler function
 *
 * DEPRECATED: Use xsltSetCtxtLocaleHandlers.
 *
 * Function to set the handler for XSLT sorting
 * for the specified context.
 * If the handler is NULL, then the global
 * sort function will be called
 */
void
xsltSetCtxtSortFunc(xsltTransformContextPtr ctxt, xsltSortFunc handler) {
    ctxt->sortfunc = handler;
}

/**
 * xsltSetCtxtLocaleHandlers:
 * @ctxt:  an XSLT transform context
 * @newLocale:  locale constructor
 * @freeLocale:  locale destructor
 * @genSortKey:  sort key generator
 *
 * Set the locale handlers.
 */
void
xsltSetCtxtLocaleHandlers(xsltTransformContextPtr ctxt,
                          xsltNewLocaleFunc newLocale,
                          xsltFreeLocaleFunc freeLocale,
                          xsltGenSortKeyFunc genSortKey) {
    if (ctxt == NULL)
        return;

    ctxt->newLocale = newLocale;
    ctxt->freeLocale = freeLocale;
    ctxt->genSortKey = genSortKey;
}

/************************************************************************
 *									*
 *				Parsing options				*
 *									*
 ************************************************************************/

/**
 * xsltSetCtxtParseOptions:
 * @ctxt:  a XSLT process context
 * @options:  a combination of libxml2 xmlParserOption
 *
 * Change the default parser option passed by the XSLT engine to the
 * parser when using document() loading.
 *
 * Returns the previous options or -1 in case of error
 */
int
xsltSetCtxtParseOptions(xsltTransformContextPtr ctxt, int options)
{
    int oldopts;

    if (ctxt == NULL)
        return(-1);
    oldopts = ctxt->parserOptions;
    if (ctxt->xinclude)
        oldopts |= XML_PARSE_XINCLUDE;
    ctxt->parserOptions = options;
    if (options & XML_PARSE_XINCLUDE)
        ctxt->xinclude = 1;
    else
        ctxt->xinclude = 0;
    return(oldopts);
}

/************************************************************************
 *									*
 *				Output					*
 *									*
 ************************************************************************/

/**
 * xsltSaveResultTo:
 * @buf:  an output buffer
 * @result:  the result xmlDocPtr
 * @style:  the stylesheet
 *
 * Save the result @result obtained by applying the @style stylesheet
 * to an I/O output channel @buf
 *
 * Returns the number of byte written or -1 in case of failure.
 */
int
xsltSaveResultTo(xmlOutputBufferPtr buf, xmlDocPtr result,
	       xsltStylesheetPtr style) {
    const xmlChar *encoding;
    int base;
    const xmlChar *method;
    int indent;

    if ((buf == NULL) || (result == NULL) || (style == NULL))
	return(-1);
    if ((result->children == NULL) ||
	((result->children->type == XML_DTD_NODE) &&
	 (result->children->next == NULL)))
	return(0);

    if ((style->methodURI != NULL) &&
	((style->method == NULL) ||
	 (!xmlStrEqual(style->method, (const xmlChar *) "xhtml")))) {
        xsltGenericError(xsltGenericErrorContext,
		"xsltSaveResultTo : unknown output method\n");
        return(-1);
    }

    base = buf->written;

    XSLT_GET_IMPORT_PTR(method, style, method)
    XSLT_GET_IMPORT_PTR(encoding, style, encoding)
    XSLT_GET_IMPORT_INT(indent, style, indent);

    if ((method == NULL) && (result->type == XML_HTML_DOCUMENT_NODE))
	method = (const xmlChar *) "html";

    if ((method != NULL) &&
	(xmlStrEqual(method, (const xmlChar *) "html"))) {
	if (encoding != NULL) {
	    htmlSetMetaEncoding(result, (const xmlChar *) encoding);
	} else {
	    htmlSetMetaEncoding(result, (const xmlChar *) "UTF-8");
	}
	if (indent == -1)
	    indent = 1;
	htmlDocContentDumpFormatOutput(buf, result, (const char *) encoding,
		                       indent);
	xmlOutputBufferFlush(buf);
    } else if ((method != NULL) &&
	(xmlStrEqual(method, (const xmlChar *) "xhtml"))) {
	if (encoding != NULL) {
	    htmlSetMetaEncoding(result, (const xmlChar *) encoding);
	} else {
	    htmlSetMetaEncoding(result, (const xmlChar *) "UTF-8");
	}
	htmlDocContentDumpOutput(buf, result, (const char *) encoding);
	xmlOutputBufferFlush(buf);
    } else if ((method != NULL) &&
	       (xmlStrEqual(method, (const xmlChar *) "text"))) {
	xmlNodePtr cur;

	cur = result->children;
	while (cur != NULL) {
	    if (cur->type == XML_TEXT_NODE)
		xmlOutputBufferWriteString(buf, (const char *) cur->content);

	    /*
	     * Skip to next node
	     */
	    if (cur->children != NULL) {
		if ((cur->children->type != XML_ENTITY_DECL) &&
		    (cur->children->type != XML_ENTITY_REF_NODE) &&
		    (cur->children->type != XML_ENTITY_NODE)) {
		    cur = cur->children;
		    continue;
		}
	    }
	    if (cur->next != NULL) {
		cur = cur->next;
		continue;
	    }

	    do {
		cur = cur->parent;
		if (cur == NULL)
		    break;
		if (cur == (xmlNodePtr) style->doc) {
		    cur = NULL;
		    break;
		}
		if (cur->next != NULL) {
		    cur = cur->next;
		    break;
		}
	    } while (cur != NULL);
	}
	xmlOutputBufferFlush(buf);
    } else {
	int omitXmlDecl;
	int standalone;

	XSLT_GET_IMPORT_INT(omitXmlDecl, style, omitXmlDeclaration);
	XSLT_GET_IMPORT_INT(standalone, style, standalone);

	if (omitXmlDecl != 1) {
	    xmlOutputBufferWriteString(buf, "<?xml version=");
	    if (result->version != NULL) {
		xmlOutputBufferWriteString(buf, "\"");
		xmlOutputBufferWriteString(buf, (const char *)result->version);
		xmlOutputBufferWriteString(buf, "\"");
	    } else
		xmlOutputBufferWriteString(buf, "\"1.0\"");
	    if (encoding == NULL) {
		if (result->encoding != NULL)
		    encoding = result->encoding;
		else if (result->charset != XML_CHAR_ENCODING_UTF8)
		    encoding = (const xmlChar *)
			       xmlGetCharEncodingName((xmlCharEncoding)
			                              result->charset);
	    }
	    if (encoding != NULL) {
		xmlOutputBufferWriteString(buf, " encoding=");
		xmlOutputBufferWriteString(buf, "\"");
		xmlOutputBufferWriteString(buf, (const char *) encoding);
		xmlOutputBufferWriteString(buf, "\"");
	    }
	    switch (standalone) {
		case 0:
		    xmlOutputBufferWriteString(buf, " standalone=\"no\"");
		    break;
		case 1:
		    xmlOutputBufferWriteString(buf, " standalone=\"yes\"");
		    break;
		default:
		    break;
	    }
	    xmlOutputBufferWriteString(buf, "?>\n");
	}
	if (result->children != NULL) {
            xmlNodePtr children = result->children;
	    xmlNodePtr child = children;

            /*
             * Hack to avoid quadratic behavior when scanning
             * result->children in xmlGetIntSubset called by
             * xmlNodeDumpOutput.
             */
            result->children = NULL;

	    while (child != NULL) {
		xmlNodeDumpOutput(buf, result, child, 0, (indent == 1),
			          (const char *) encoding);
		if (indent && ((child->type == XML_DTD_NODE) ||
		    ((child->type == XML_COMMENT_NODE) &&
		     (child->next != NULL))))
		    xmlOutputBufferWriteString(buf, "\n");
		child = child->next;
	    }
	    if (indent)
			xmlOutputBufferWriteString(buf, "\n");

            result->children = children;
	}
	xmlOutputBufferFlush(buf);
    }
    return(buf->written - base);
}

/**
 * xsltSaveResultToFilename:
 * @URL:  a filename or URL
 * @result:  the result xmlDocPtr
 * @style:  the stylesheet
 * @compression:  the compression factor (0 - 9 included)
 *
 * Save the result @result obtained by applying the @style stylesheet
 * to a file or @URL
 *
 * Returns the number of byte written or -1 in case of failure.
 */
int
xsltSaveResultToFilename(const char *URL, xmlDocPtr result,
			 xsltStylesheetPtr style, int compression) {
    xmlOutputBufferPtr buf;
    const xmlChar *encoding;
    int ret;

    if ((URL == NULL) || (result == NULL) || (style == NULL))
	return(-1);
    if (result->children == NULL)
	return(0);

    XSLT_GET_IMPORT_PTR(encoding, style, encoding)
    if (encoding != NULL) {
	xmlCharEncodingHandlerPtr encoder = NULL;

        /* Don't use UTF-8 dummy encoder */
        if ((xmlStrcasecmp(encoding, BAD_CAST "UTF-8") != 0) &&
            (xmlStrcasecmp(encoding, BAD_CAST "UTF8") != 0))
	    encoder = xmlFindCharEncodingHandler((char *) encoding);
	buf = xmlOutputBufferCreateFilename(URL, encoder, compression);
    } else {
	buf = xmlOutputBufferCreateFilename(URL, NULL, compression);
    }
    if (buf == NULL)
	return(-1);
    xsltSaveResultTo(buf, result, style);
    ret = xmlOutputBufferClose(buf);
    return(ret);
}

/**
 * xsltSaveResultToFile:
 * @file:  a FILE * I/O
 * @result:  the result xmlDocPtr
 * @style:  the stylesheet
 *
 * Save the result @result obtained by applying the @style stylesheet
 * to an open FILE * I/O.
 * This does not close the FILE @file
 *
 * Returns the number of bytes written or -1 in case of failure.
 */
int
xsltSaveResultToFile(FILE *file, xmlDocPtr result, xsltStylesheetPtr style) {
    xmlOutputBufferPtr buf;
    const xmlChar *encoding;
    int ret;

    if ((file == NULL) || (result == NULL) || (style == NULL))
	return(-1);
    if (result->children == NULL)
	return(0);

    XSLT_GET_IMPORT_PTR(encoding, style, encoding)
    if (encoding != NULL) {
	xmlCharEncodingHandlerPtr encoder = NULL;

        /* Don't use UTF-8 dummy encoder */
        if ((xmlStrcasecmp(encoding, BAD_CAST "UTF-8") != 0) &&
            (xmlStrcasecmp(encoding, BAD_CAST "UTF8") != 0))
	    encoder = xmlFindCharEncodingHandler((char *) encoding);
	buf = xmlOutputBufferCreateFile(file, encoder);
    } else {
	buf = xmlOutputBufferCreateFile(file, NULL);
    }

    if (buf == NULL)
	return(-1);
    xsltSaveResultTo(buf, result, style);
    ret = xmlOutputBufferClose(buf);
    return(ret);
}

/**
 * xsltSaveResultToFd:
 * @fd:  a file descriptor
 * @result:  the result xmlDocPtr
 * @style:  the stylesheet
 *
 * Save the result @result obtained by applying the @style stylesheet
 * to an open file descriptor
 * This does not close the descriptor.
 *
 * Returns the number of bytes written or -1 in case of failure.
 */
int
xsltSaveResultToFd(int fd, xmlDocPtr result, xsltStylesheetPtr style) {
    xmlOutputBufferPtr buf;
    const xmlChar *encoding;
    int ret;

    if ((fd < 0) || (result == NULL) || (style == NULL))
	return(-1);
    if (result->children == NULL)
	return(0);

    XSLT_GET_IMPORT_PTR(encoding, style, encoding)
    if (encoding != NULL) {
	xmlCharEncodingHandlerPtr encoder = NULL;

        /* Don't use UTF-8 dummy encoder */
        if ((xmlStrcasecmp(encoding, BAD_CAST "UTF-8") != 0) &&
            (xmlStrcasecmp(encoding, BAD_CAST "UTF8") != 0))
	    encoder = xmlFindCharEncodingHandler((char *) encoding);
	buf = xmlOutputBufferCreateFd(fd, encoder);
    } else {
	buf = xmlOutputBufferCreateFd(fd, NULL);
    }
    if (buf == NULL)
	return(-1);
    xsltSaveResultTo(buf, result, style);
    ret = xmlOutputBufferClose(buf);
    return(ret);
}

/**
 * xsltSaveResultToString:
 * @doc_txt_ptr:  Memory pointer for allocated XML text
 * @doc_txt_len:  Length of the generated XML text
 * @result:  the result xmlDocPtr
 * @style:  the stylesheet
 *
 * Save the result @result obtained by applying the @style stylesheet
 * to a new allocated string.
 *
 * Returns 0 in case of success and -1 in case of error
 */
int
xsltSaveResultToString(xmlChar **doc_txt_ptr, int * doc_txt_len,
		       xmlDocPtr result, xsltStylesheetPtr style) {
    xmlOutputBufferPtr buf;
    const xmlChar *encoding;

    *doc_txt_ptr = NULL;
    *doc_txt_len = 0;
    if (result->children == NULL)
	return(0);

    XSLT_GET_IMPORT_PTR(encoding, style, encoding)
    if (encoding != NULL) {
	xmlCharEncodingHandlerPtr encoder = NULL;

        /* Don't use UTF-8 dummy encoder */
        if ((xmlStrcasecmp(encoding, BAD_CAST "UTF-8") != 0) &&
            (xmlStrcasecmp(encoding, BAD_CAST "UTF8") != 0))
	    encoder = xmlFindCharEncodingHandler((char *) encoding);
	buf = xmlAllocOutputBuffer(encoder);
        if (buf == NULL)
            xmlCharEncCloseFunc(encoder);
    } else {
	buf = xmlAllocOutputBuffer(NULL);
    }
    if (buf == NULL)
	return(-1);
    xsltSaveResultTo(buf, result, style);
#ifdef LIBXML2_NEW_BUFFER
    if (buf->conv != NULL) {
	*doc_txt_len = xmlBufUse(buf->conv);
	*doc_txt_ptr = xmlStrndup(xmlBufContent(buf->conv), *doc_txt_len);
    } else {
	*doc_txt_len = xmlBufUse(buf->buffer);
	*doc_txt_ptr = xmlStrndup(xmlBufContent(buf->buffer), *doc_txt_len);
    }
#else
    if (buf->conv != NULL) {
	*doc_txt_len = buf->conv->use;
	*doc_txt_ptr = xmlStrndup(buf->conv->content, *doc_txt_len);
    } else {
	*doc_txt_len = buf->buffer->use;
	*doc_txt_ptr = xmlStrndup(buf->buffer->content, *doc_txt_len);
    }
#endif
    (void)xmlOutputBufferClose(buf);
    return 0;
}

/**
 * xsltGetSourceNodeFlags:
 * @node:  Node from source document
 *
 * Returns the flags for a source node.
 */
int
xsltGetSourceNodeFlags(xmlNodePtr node) {
    /*
     * Squeeze the bit flags into the upper bits of
     *
     * - 'int properties' member in struct _xmlDoc
     * - 'xmlAttributeType atype' member in struct _xmlAttr
     * - 'unsigned short extra' member in struct _xmlNode
     */
    switch (node->type) {
        case XML_DOCUMENT_NODE:
        case XML_HTML_DOCUMENT_NODE:
            return ((xmlDocPtr) node)->properties >> 27;

        case XML_ATTRIBUTE_NODE:
            return ((xmlAttrPtr) node)->atype >> 27;

        case XML_ELEMENT_NODE:
        case XML_TEXT_NODE:
        case XML_CDATA_SECTION_NODE:
        case XML_PI_NODE:
        case XML_COMMENT_NODE:
            return node->extra >> 12;

        default:
            return 0;
    }
}

/**
 * xsltSetSourceNodeFlags:
 * @node:  Node from source document
 * @flags:  Flags
 *
 * Sets the specified flags to 1.
 *
 * Returns 0 on success, -1 on error.
 */
int
xsltSetSourceNodeFlags(xsltTransformContextPtr ctxt, xmlNodePtr node,
                       int flags) {
    if (node->doc == ctxt->initialContextDoc)
        ctxt->sourceDocDirty = 1;

    switch (node->type) {
        case XML_DOCUMENT_NODE:
        case XML_HTML_DOCUMENT_NODE:
            ((xmlDocPtr) node)->properties |= flags << 27;
            return 0;

        case XML_ATTRIBUTE_NODE:
            ((xmlAttrPtr) node)->atype |= flags << 27;
            return 0;

        case XML_ELEMENT_NODE:
        case XML_TEXT_NODE:
        case XML_CDATA_SECTION_NODE:
        case XML_PI_NODE:
        case XML_COMMENT_NODE:
            node->extra |= flags << 12;
            return 0;

        default:
            return -1;
    }
}

/**
 * xsltClearSourceNodeFlags:
 * @node:  Node from source document
 * @flags:  Flags
 *
 * Sets the specified flags to 0.
 *
 * Returns 0 on success, -1 on error.
 */
int
xsltClearSourceNodeFlags(xmlNodePtr node, int flags) {
    switch (node->type) {
        case XML_DOCUMENT_NODE:
        case XML_HTML_DOCUMENT_NODE:
            ((xmlDocPtr) node)->properties &= ~(flags << 27);
            return 0;

        case XML_ATTRIBUTE_NODE:
            ((xmlAttrPtr) node)->atype &= ~(flags << 27);
            return 0;

        case XML_ELEMENT_NODE:
        case XML_TEXT_NODE:
        case XML_CDATA_SECTION_NODE:
        case XML_PI_NODE:
        case XML_COMMENT_NODE:
            node->extra &= ~(flags << 12);
            return 0;

        default:
            return -1;
    }
}

/**
 * xsltGetPSVIPtr:
 * @cur:  Node
 *
 * Returns a pointer to the psvi member of a node or NULL on error.
 */
void **
xsltGetPSVIPtr(xmlNodePtr cur) {
    switch (cur->type) {
        case XML_DOCUMENT_NODE:
        case XML_HTML_DOCUMENT_NODE:
            return &((xmlDocPtr) cur)->psvi;

        case XML_ATTRIBUTE_NODE:
            return &((xmlAttrPtr) cur)->psvi;

        case XML_ELEMENT_NODE:
        case XML_TEXT_NODE:
        case XML_CDATA_SECTION_NODE:
        case XML_PI_NODE:
        case XML_COMMENT_NODE:
            return &cur->psvi;

        default:
            return NULL;
    }
}

#ifdef WITH_PROFILER

/************************************************************************
 *									*
 *		Generating profiling information			*
 *									*
 ************************************************************************/

static long calibration = -1;

/**
 * xsltCalibrateTimestamps:
 *
 * Used for to calibrate the xsltTimestamp() function
 * Should work if launched at startup and we don't loose our quantum :-)
 *
 * Returns the number of milliseconds used by xsltTimestamp()
 */
#if !defined(XSLT_WIN32_PERFORMANCE_COUNTER) && \
    (defined(HAVE_CLOCK_GETTIME) || defined(HAVE_GETTIMEOFDAY))
static long
xsltCalibrateTimestamps(void) {
    register int i;

    for (i = 0;i < 999;i++)
	xsltTimestamp();
    return(xsltTimestamp() / 1000);
}
#endif

/**
 * xsltCalibrateAdjust:
 * @delta:  a negative dealy value found
 *
 * Used for to correct the calibration for xsltTimestamp()
 */
void
xsltCalibrateAdjust(long delta) {
    calibration += delta;
}

/**
 * xsltTimestamp:
 *
 * Used for gathering profiling data
 *
 * Returns the number of tenth of milliseconds since the beginning of the
 * profiling
 */
long
xsltTimestamp(void)
{
#ifdef XSLT_WIN32_PERFORMANCE_COUNTER
    BOOL ok;
    LARGE_INTEGER performanceCount;
    LARGE_INTEGER performanceFrequency;
    LONGLONG quadCount;
    double seconds;
    static LONGLONG startupQuadCount = 0;
    static LONGLONG startupQuadFreq = 0;

    ok = QueryPerformanceCounter(&performanceCount);
    if (!ok)
        return 0;
    quadCount = performanceCount.QuadPart;
    if (calibration < 0) {
        calibration = 0;
        ok = QueryPerformanceFrequency(&performanceFrequency);
        if (!ok)
            return 0;
        startupQuadFreq = performanceFrequency.QuadPart;
        startupQuadCount = quadCount;
        return (0);
    }
    if (startupQuadFreq == 0)
        return 0;
    seconds = (quadCount - startupQuadCount) / (double) startupQuadFreq;
    return (long) (seconds * XSLT_TIMESTAMP_TICS_PER_SEC);

#else /* XSLT_WIN32_PERFORMANCE_COUNTER */
#ifdef HAVE_CLOCK_GETTIME
#  if defined(CLOCK_MONOTONIC)
#    define XSLT_CLOCK CLOCK_MONOTONIC
#  elif defined(CLOCK_HIGHRES)
#    define XSLT_CLOCK CLOCK_HIGHRES
#  else
#    define XSLT_CLOCK CLOCK_REALTIME
#  endif
    static struct timespec startup;
    struct timespec cur;
    long tics;

    if (calibration < 0) {
        clock_gettime(XSLT_CLOCK, &startup);
        calibration = 0;
        calibration = xsltCalibrateTimestamps();
        clock_gettime(XSLT_CLOCK, &startup);
        return (0);
    }

    clock_gettime(XSLT_CLOCK, &cur);
    tics = (cur.tv_sec - startup.tv_sec) * XSLT_TIMESTAMP_TICS_PER_SEC;
    tics += (cur.tv_nsec - startup.tv_nsec) /
                          (1000000000l / XSLT_TIMESTAMP_TICS_PER_SEC);

    tics -= calibration;
    return(tics);

#elif HAVE_GETTIMEOFDAY
    static struct timeval startup;
    struct timeval cur;
    long tics;

    if (calibration < 0) {
        gettimeofday(&startup, NULL);
        calibration = 0;
        calibration = xsltCalibrateTimestamps();
        gettimeofday(&startup, NULL);
        return (0);
    }

    gettimeofday(&cur, NULL);
    tics = (cur.tv_sec - startup.tv_sec) * XSLT_TIMESTAMP_TICS_PER_SEC;
    tics += (cur.tv_usec - startup.tv_usec) /
                          (1000000l / XSLT_TIMESTAMP_TICS_PER_SEC);

    tics -= calibration;
    return(tics);
#else

    /* Neither gettimeofday() nor Win32 performance counter available */

    return (0);

#endif /* HAVE_GETTIMEOFDAY */
#endif /* XSLT_WIN32_PERFORMANCE_COUNTER */
}

static char *
pretty_templ_match(xsltTemplatePtr templ) {
  static char dst[1001];
  char *src = (char *)templ->match;
  int i=0,j;

  /* strip white spaces */
  for (j=0; i<1000 && src[j]; i++,j++) {
      for(;src[j]==' ';j++);
      dst[i]=src[j];
  }
  if(i<998 && templ->mode) {
    /* append [mode] */
    dst[i++]='[';
    src=(char *)templ->mode;
    for (j=0; i<999 && src[j]; i++,j++) {
      dst[i]=src[j];
    }
    dst[i++]=']';
  }
  dst[i]='\0';
  return dst;
}

#define MAX_TEMPLATES 10000

/**
 * xsltSaveProfiling:
 * @ctxt:  an XSLT context
 * @output:  a FILE * for saving the information
 *
 * Save the profiling information on @output
 */
void
xsltSaveProfiling(xsltTransformContextPtr ctxt, FILE *output) {
    int nb, i,j,k,l;
    int max;
    int total;
    unsigned long totalt;
    xsltTemplatePtr *templates;
    xsltStylesheetPtr style;
    xsltTemplatePtr templ1,templ2;
    int *childt;

    if ((output == NULL) || (ctxt == NULL))
	return;
    if (ctxt->profile == 0)
	return;

    nb = 0;
    max = MAX_TEMPLATES;
    templates = xmlMalloc(max * sizeof(xsltTemplatePtr));
    if (templates == NULL)
	return;

    style = ctxt->style;
    while (style != NULL) {
	templ1 = style->templates;
	while (templ1 != NULL) {
	    if (nb >= max)
		break;

	    if (templ1->nbCalls > 0)
		templates[nb++] = templ1;
	    templ1 = templ1->next;
	}

	style = xsltNextImport(style);
    }

    for (i = 0;i < nb -1;i++) {
	for (j = i + 1; j < nb; j++) {
	    if ((templates[i]->time <= templates[j]->time) ||
		((templates[i]->time == templates[j]->time) &&
	         (templates[i]->nbCalls <= templates[j]->nbCalls))) {
		templ1 = templates[j];
		templates[j] = templates[i];
		templates[i] = templ1;
	    }
	}
    }


    /* print flat profile */

    fprintf(output, "%6s%20s%20s%10s  Calls Tot 100us Avg\n\n",
	    "number", "match", "name", "mode");
    total = 0;
    totalt = 0;
    for (i = 0;i < nb;i++) {
         templ1 = templates[i];
	fprintf(output, "%5d ", i);
	if (templ1->match != NULL) {
	    if (xmlStrlen(templ1->match) > 20)
		fprintf(output, "%s\n%26s", templ1->match, "");
	    else
		fprintf(output, "%20s", templ1->match);
	} else {
	    fprintf(output, "%20s", "");
	}
	if (templ1->name != NULL) {
	    if (xmlStrlen(templ1->name) > 20)
		fprintf(output, "%s\n%46s", templ1->name, "");
	    else
		fprintf(output, "%20s", templ1->name);
	} else {
	    fprintf(output, "%20s", "");
	}
	if (templ1->mode != NULL) {
	    if (xmlStrlen(templ1->mode) > 10)
		fprintf(output, "%s\n%56s", templ1->mode, "");
	    else
		fprintf(output, "%10s", templ1->mode);
	} else {
	    fprintf(output, "%10s", "");
	}
	fprintf(output, " %6d", templ1->nbCalls);
	fprintf(output, " %6ld %6ld\n", templ1->time,
		templ1->time / templ1->nbCalls);
	total += templ1->nbCalls;
	totalt += templ1->time;
    }
    fprintf(output, "\n%30s%26s %6d %6ld\n", "Total", "", total, totalt);


    /* print call graph */

    childt = xmlMalloc((nb + 1) * sizeof(int));
    if (childt == NULL)
	return;

    /* precalculate children times */
    for (i = 0; i < nb; i++) {
        templ1 = templates[i];

        childt[i] = 0;
        for (k = 0; k < nb; k++) {
            templ2 = templates[k];
            for (l = 0; l < templ2->templNr; l++) {
                if (templ2->templCalledTab[l] == templ1) {
                    childt[i] +=templ2->time;
                }
            }
        }
    }
    childt[i] = 0;

    fprintf(output, "\nindex %% time    self  children    called     name\n");

    for (i = 0; i < nb; i++) {
        char ix_str[20], timep_str[20], times_str[20], timec_str[20], called_str[20];
        unsigned long t;

        templ1 = templates[i];
        /* callers */
        for (j = 0; j < templ1->templNr; j++) {
            templ2 = templ1->templCalledTab[j];
            for (k = 0; k < nb; k++) {
              if (templates[k] == templ2)
                break;
            }
            t=templ2?templ2->time:totalt;
            snprintf(times_str,sizeof(times_str),"%8.3f",(float)t/XSLT_TIMESTAMP_TICS_PER_SEC);
            snprintf(timec_str,sizeof(timec_str),"%8.3f",(float)childt[k]/XSLT_TIMESTAMP_TICS_PER_SEC);
            snprintf(called_str,sizeof(called_str),"%6d/%d",
                templ1->templCountTab[j], /* number of times caller calls 'this' */
                templ1->nbCalls);         /* total number of calls to 'this' */

            fprintf(output, "             %-8s %-8s %-12s     %s [%d]\n",
                times_str,timec_str,called_str,
                (templ2?(templ2->name?(char *)templ2->name:pretty_templ_match(templ2)):"-"),k);
        }
        /* this */
        snprintf(ix_str,sizeof(ix_str),"[%d]",i);
        snprintf(timep_str,sizeof(timep_str),"%6.2f",(float)templ1->time*100.0/totalt);
        snprintf(times_str,sizeof(times_str),"%8.3f",(float)templ1->time/XSLT_TIMESTAMP_TICS_PER_SEC);
        snprintf(timec_str,sizeof(timec_str),"%8.3f",(float)childt[i]/XSLT_TIMESTAMP_TICS_PER_SEC);
        fprintf(output, "%-5s %-6s %-8s %-8s %6d     %s [%d]\n",
            ix_str, timep_str,times_str,timec_str,
            templ1->nbCalls,
            templ1->name?(char *)templ1->name:pretty_templ_match(templ1),i);
        /* callees
         * - go over templates[0..nb] and their templCalledTab[]
         * - print those where we in the the call-stack
         */
        total = 0;
        for (k = 0; k < nb; k++) {
            templ2 = templates[k];
            for (l = 0; l < templ2->templNr; l++) {
                if (templ2->templCalledTab[l] == templ1) {
                    total+=templ2->templCountTab[l];
                }
            }
        }
        for (k = 0; k < nb; k++) {
            templ2 = templates[k];
            for (l = 0; l < templ2->templNr; l++) {
                if (templ2->templCalledTab[l] == templ1) {
                    snprintf(times_str,sizeof(times_str),"%8.3f",(float)templ2->time/XSLT_TIMESTAMP_TICS_PER_SEC);
                    snprintf(timec_str,sizeof(timec_str),"%8.3f",(float)childt[k]/XSLT_TIMESTAMP_TICS_PER_SEC);
                    snprintf(called_str,sizeof(called_str),"%6d/%d",
                        templ2->templCountTab[l], /* number of times 'this' calls callee */
                        total);                   /* total number of calls from 'this' */
                    fprintf(output, "             %-8s %-8s %-12s     %s [%d]\n",
                        times_str,timec_str,called_str,
                        templ2->name?(char *)templ2->name:pretty_templ_match(templ2),k);
                }
            }
        }
        fprintf(output, "-----------------------------------------------\n");
    }

    fprintf(output, "\f\nIndex by function name\n");
    for (i = 0; i < nb; i++) {
        templ1 = templates[i];
        fprintf(output, "[%d] %s (%s:%d)\n",
            i, templ1->name?(char *)templ1->name:pretty_templ_match(templ1),
            templ1->style->doc->URL,templ1->elem->line);
    }

    fprintf(output, "\f\n");
    xmlFree(childt);

    xmlFree(templates);
}

/************************************************************************
 *									*
 *		Fetching profiling information				*
 *									*
 ************************************************************************/

/**
 * xsltGetProfileInformation:
 * @ctxt:  a transformation context
 *
 * This function should be called after the transformation completed
 * to extract template processing profiling information if available.
 * The information is returned as an XML document tree like
 * <?xml version="1.0"?>
 * <profile>
 * <template rank="1" match="*" name=""
 *         mode="" calls="6" time="48" average="8"/>
 * <template rank="2" match="item2|item3" name=""
 *         mode="" calls="10" time="30" average="3"/>
 * <template rank="3" match="item1" name=""
 *         mode="" calls="5" time="17" average="3"/>
 * </profile>
 * The caller will need to free up the returned tree with xmlFreeDoc()
 *
 * Returns the xmlDocPtr corresponding to the result or NULL if not available.
 */

xmlDocPtr
xsltGetProfileInformation(xsltTransformContextPtr ctxt)
{
    xmlDocPtr ret = NULL;
    xmlNodePtr root, child;
    char buf[100];

    xsltStylesheetPtr style;
    xsltTemplatePtr *templates;
    xsltTemplatePtr templ;
    int nb = 0, max = 0, i, j;

    if (!ctxt)
        return NULL;

    if (!ctxt->profile)
        return NULL;

    nb = 0;
    max = 10000;
    templates =
        (xsltTemplatePtr *) xmlMalloc(max * sizeof(xsltTemplatePtr));
    if (templates == NULL)
        return NULL;

    /*
     * collect all the templates in an array
     */
    style = ctxt->style;
    while (style != NULL) {
        templ = style->templates;
        while (templ != NULL) {
            if (nb >= max)
                break;

            if (templ->nbCalls > 0)
                templates[nb++] = templ;
            templ = templ->next;
        }

        style = (xsltStylesheetPtr) xsltNextImport(style);
    }

    /*
     * Sort the array by time spent
     */
    for (i = 0; i < nb - 1; i++) {
        for (j = i + 1; j < nb; j++) {
            if ((templates[i]->time <= templates[j]->time) ||
                ((templates[i]->time == templates[j]->time) &&
                 (templates[i]->nbCalls <= templates[j]->nbCalls))) {
                templ = templates[j];
                templates[j] = templates[i];
                templates[i] = templ;
            }
        }
    }

    /*
     * Generate a document corresponding to the results.
     */
    ret = xmlNewDoc(BAD_CAST "1.0");
    root = xmlNewDocNode(ret, NULL, BAD_CAST "profile", NULL);
    xmlDocSetRootElement(ret, root);

    for (i = 0; i < nb; i++) {
        child = xmlNewChild(root, NULL, BAD_CAST "template", NULL);
        snprintf(buf, sizeof(buf), "%d", i + 1);
        xmlSetProp(child, BAD_CAST "rank", BAD_CAST buf);
        xmlSetProp(child, BAD_CAST "match", BAD_CAST templates[i]->match);
        xmlSetProp(child, BAD_CAST "name", BAD_CAST templates[i]->name);
        xmlSetProp(child, BAD_CAST "mode", BAD_CAST templates[i]->mode);

        snprintf(buf, sizeof(buf), "%d", templates[i]->nbCalls);
        xmlSetProp(child, BAD_CAST "calls", BAD_CAST buf);

        snprintf(buf, sizeof(buf), "%ld", templates[i]->time);
        xmlSetProp(child, BAD_CAST "time", BAD_CAST buf);

        snprintf(buf, sizeof(buf), "%ld", templates[i]->time / templates[i]->nbCalls);
        xmlSetProp(child, BAD_CAST "average", BAD_CAST buf);
    };

    xmlFree(templates);

    return ret;
}

#endif /* WITH_PROFILER */

/************************************************************************
 *									*
 *		Hooks for libxml2 XPath					*
 *									*
 ************************************************************************/

/**
 * xsltXPathCompileFlags:
 * @style: the stylesheet
 * @str:  the XPath expression
 * @flags: extra compilation flags to pass down to libxml2 XPath
 *
 * Compile an XPath expression
 *
 * Returns the xmlXPathCompExprPtr resulting from the compilation or NULL.
 *         the caller has to free the object.
 */
xmlXPathCompExprPtr
xsltXPathCompileFlags(xsltStylesheetPtr style, const xmlChar *str, int flags) {
    xmlXPathContextPtr xpathCtxt;
    xmlXPathCompExprPtr ret;

    if (style != NULL) {
        xpathCtxt = style->principal->xpathCtxt;
	if (xpathCtxt == NULL)
	    return NULL;
	xpathCtxt->dict = style->dict;
    } else {
	xpathCtxt = xmlXPathNewContext(NULL);
	if (xpathCtxt == NULL)
	    return NULL;
    }
    xpathCtxt->flags = flags;

    /*
    * Compile the expression.
    */
    ret = xmlXPathCtxtCompile(xpathCtxt, str);

    if (style == NULL) {
	xmlXPathFreeContext(xpathCtxt);
    }
    /*
     * TODO: there is a lot of optimizations which should be possible
     *       like variable slot precomputations, function precomputations, etc.
     */

    return(ret);
}

/**
 * xsltXPathCompile:
 * @style: the stylesheet
 * @str:  the XPath expression
 *
 * Compile an XPath expression
 *
 * Returns the xmlXPathCompExprPtr resulting from the compilation or NULL.
 *         the caller has to free the object.
 */
xmlXPathCompExprPtr
xsltXPathCompile(xsltStylesheetPtr style, const xmlChar *str) {
    return(xsltXPathCompileFlags(style, str, 0));
}

/************************************************************************
 *									*
 *		Hooks for the debugger					*
 *									*
 ************************************************************************/

int xslDebugStatus;

/**
 * xsltGetDebuggerStatus:
 *
 * Get xslDebugStatus.
 *
 * Returns the value of xslDebugStatus.
 */
int
xsltGetDebuggerStatus(void)
{
    return(xslDebugStatus);
}

#ifdef WITH_DEBUGGER

/*
 * There is currently only 3 debugging callback defined
 * Debugger callbacks are disabled by default
 */
#define XSLT_CALLBACK_NUMBER 3

typedef struct _xsltDebuggerCallbacks xsltDebuggerCallbacks;
typedef xsltDebuggerCallbacks *xsltDebuggerCallbacksPtr;
struct _xsltDebuggerCallbacks {
    xsltHandleDebuggerCallback handler;
    xsltAddCallCallback add;
    xsltDropCallCallback drop;
};

static xsltDebuggerCallbacks xsltDebuggerCurrentCallbacks = {
    NULL, /* handler */
    NULL, /* add */
    NULL  /* drop */
};

/**
 * xsltSetDebuggerStatus:
 * @value : the value to be set
 *
 * This function sets the value of xslDebugStatus.
 */
void
xsltSetDebuggerStatus(int value)
{
    xslDebugStatus = value;
}

/**
 * xsltSetDebuggerCallbacks:
 * @no : number of callbacks
 * @block : the block of callbacks
 *
 * This function allow to plug a debugger into the XSLT library
 * @block points to a block of memory containing the address of @no
 * callback routines.
 *
 * Returns 0 in case of success and -1 in case of error
 */
int
xsltSetDebuggerCallbacks(int no, void *block)
{
    xsltDebuggerCallbacksPtr callbacks;

    if ((block == NULL) || (no != XSLT_CALLBACK_NUMBER))
	return(-1);

    callbacks = (xsltDebuggerCallbacksPtr) block;
    xsltDebuggerCurrentCallbacks.handler = callbacks->handler;
    xsltDebuggerCurrentCallbacks.add  = callbacks->add;
    xsltDebuggerCurrentCallbacks.drop  = callbacks->drop;
    return(0);
}

/**
 * xslHandleDebugger:
 * @cur : source node being executed
 * @node : data node being processed
 * @templ : temlate that applies to node
 * @ctxt : the xslt transform context
 *
 * If either cur or node are a breakpoint, or xslDebugStatus in state
 *   where debugging must occcur at this time then transfer control
 *   to the xslDebugBreak function
 */
void
xslHandleDebugger(xmlNodePtr cur, xmlNodePtr node, xsltTemplatePtr templ,
	          xsltTransformContextPtr ctxt)
{
    if (xsltDebuggerCurrentCallbacks.handler != NULL)
	xsltDebuggerCurrentCallbacks.handler(cur, node, templ, ctxt);
}

/**
 * xslAddCall:
 * @templ : current template being applied
 * @source : the source node being processed
 *
 * Add template "call" to call stack
 * Returns : 1 on sucess 0 otherwise an error may be printed if
 *            WITH_XSLT_DEBUG_BREAKPOINTS is defined
 */
int
xslAddCall(xsltTemplatePtr templ, xmlNodePtr source)
{
    if (xsltDebuggerCurrentCallbacks.add != NULL)
	return(xsltDebuggerCurrentCallbacks.add(templ, source));
    return(0);
}

/**
 * xslDropCall:
 *
 * Drop the topmost item off the call stack
 */
void
xslDropCall(void)
{
    if (xsltDebuggerCurrentCallbacks.drop != NULL)
	xsltDebuggerCurrentCallbacks.drop();
}

#endif /* WITH_DEBUGGER */

