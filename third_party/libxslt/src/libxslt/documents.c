/*
 * documents.c: Implementation of the documents handling
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
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "documents.h"
#include "transform.h"
#include "imports.h"
#include "keys.h"
#include "security.h"

#ifdef LIBXML_XINCLUDE_ENABLED
#include <libxml/xinclude.h>
#endif

#define WITH_XSLT_DEBUG_DOCUMENTS

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_DOCUMENTS
#endif

/************************************************************************
 *									*
 *		Hooks for the document loader				*
 *									*
 ************************************************************************/

/**
 * xsltDocDefaultLoaderFunc:
 * @URI: the URI of the document to load
 * @dict: the dictionary to use when parsing that document
 * @options: parsing options, a set of xmlParserOption
 * @ctxt: the context, either a stylesheet or a transformation context
 * @type: the xsltLoadType indicating the kind of loading required
 *
 * Default function to load document not provided by the compilation or
 * transformation API themselve, for example when an xsl:import,
 * xsl:include is found at compilation time or when a document()
 * call is made at runtime.
 *
 * Returns the pointer to the document (which will be modified and
 * freed by the engine later), or NULL in case of error.
 */
static xmlDocPtr
xsltDocDefaultLoaderFunc(const xmlChar * URI, xmlDictPtr dict, int options,
                         void *ctxt ATTRIBUTE_UNUSED,
			 xsltLoadType type ATTRIBUTE_UNUSED)
{
    xmlParserCtxtPtr pctxt;
    xmlParserInputPtr inputStream;
    xmlDocPtr doc;

    pctxt = xmlNewParserCtxt();
    if (pctxt == NULL)
        return(NULL);
    if ((dict != NULL) && (pctxt->dict != NULL)) {
        xmlDictFree(pctxt->dict);
	pctxt->dict = NULL;
    }
    if (dict != NULL) {
	pctxt->dict = dict;
	xmlDictReference(pctxt->dict);
#ifdef WITH_XSLT_DEBUG
	xsltGenericDebug(xsltGenericDebugContext,
                     "Reusing dictionary for document\n");
#endif
    }
    xmlCtxtUseOptions(pctxt, options);
    inputStream = xmlLoadExternalEntity((const char *) URI, NULL, pctxt);
    if (inputStream == NULL) {
        xmlFreeParserCtxt(pctxt);
	return(NULL);
    }
    inputPush(pctxt, inputStream);

    xmlParseDocument(pctxt);

    if (pctxt->wellFormed) {
        doc = pctxt->myDoc;
    }
    else {
        doc = NULL;
        xmlFreeDoc(pctxt->myDoc);
        pctxt->myDoc = NULL;
    }
    xmlFreeParserCtxt(pctxt);

    return(doc);
}


xsltDocLoaderFunc xsltDocDefaultLoader = xsltDocDefaultLoaderFunc;

/**
 * xsltSetLoaderFunc:
 * @f: the new function to handle document loading.
 *
 * Set the new function to load document, if NULL it resets it to the
 * default function.
 */

void
xsltSetLoaderFunc(xsltDocLoaderFunc f) {
    if (f == NULL)
        xsltDocDefaultLoader = xsltDocDefaultLoaderFunc;
    else
        xsltDocDefaultLoader = f;
}

/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/

/**
 * xsltNewDocument:
 * @ctxt: an XSLT transformation context (or NULL)
 * @doc:  a parsed XML document
 *
 * Register a new document, apply key computations
 *
 * Returns a handler to the document
 */
xsltDocumentPtr
xsltNewDocument(xsltTransformContextPtr ctxt, xmlDocPtr doc) {
    xsltDocumentPtr cur;

    cur = (xsltDocumentPtr) xmlMalloc(sizeof(xsltDocument));
    if (cur == NULL) {
	xsltTransformError(ctxt, NULL, (xmlNodePtr) doc,
		"xsltNewDocument : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltDocument));
    cur->doc = doc;
    if (ctxt != NULL) {
        if (! XSLT_IS_RES_TREE_FRAG(doc)) {
	    cur->next = ctxt->docList;
	    ctxt->docList = cur;
	}
	/*
	* A key with a specific name for a specific document
	* will only be computed if there's a call to the key()
	* function using that specific name for that specific
	* document. I.e. computation of keys will be done in
	* xsltGetKey() (keys.c) on an on-demand basis.
	*
	* xsltInitCtxtKeys(ctxt, cur); not called here anymore
	*/
    }
    return(cur);
}

/**
 * xsltNewStyleDocument:
 * @style: an XSLT style sheet
 * @doc:  a parsed XML document
 *
 * Register a new document, apply key computations
 *
 * Returns a handler to the document
 */
xsltDocumentPtr
xsltNewStyleDocument(xsltStylesheetPtr style, xmlDocPtr doc) {
    xsltDocumentPtr cur;

    cur = (xsltDocumentPtr) xmlMalloc(sizeof(xsltDocument));
    if (cur == NULL) {
	xsltTransformError(NULL, style, (xmlNodePtr) doc,
		"xsltNewStyleDocument : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltDocument));
    cur->doc = doc;
    if (style != NULL) {
	cur->next = style->docList;
	style->docList = cur;
    }
    return(cur);
}

/**
 * xsltFreeStyleDocuments:
 * @style: an XSLT stylesheet (representing a stylesheet-level)
 *
 * Frees the node-trees (and xsltDocument structures) of all
 * stylesheet-modules of the stylesheet-level represented by
 * the given @style.
 */
void
xsltFreeStyleDocuments(xsltStylesheetPtr style) {
    xsltDocumentPtr doc, cur;
#ifdef XSLT_REFACTORED_XSLT_NSCOMP
    xsltNsMapPtr nsMap;
#endif

    if (style == NULL)
	return;

#ifdef XSLT_REFACTORED_XSLT_NSCOMP
    if (XSLT_HAS_INTERNAL_NSMAP(style))
	nsMap = XSLT_GET_INTERNAL_NSMAP(style);
    else
	nsMap = NULL;
#endif

    cur = style->docList;
    while (cur != NULL) {
	doc = cur;
	cur = cur->next;
#ifdef XSLT_REFACTORED_XSLT_NSCOMP
	/*
	* Restore all changed namespace URIs of ns-decls.
	*/
	if (nsMap)
	    xsltRestoreDocumentNamespaces(nsMap, doc->doc);
#endif
	xsltFreeDocumentKeys(doc);
	if (!doc->main)
	    xmlFreeDoc(doc->doc);
        xmlFree(doc);
    }
}

/**
 * xsltFreeDocuments:
 * @ctxt: an XSLT transformation context
 *
 * Free up all the space used by the loaded documents
 */
void
xsltFreeDocuments(xsltTransformContextPtr ctxt) {
    xsltDocumentPtr doc, cur;

    cur = ctxt->docList;
    while (cur != NULL) {
	doc = cur;
	cur = cur->next;
	xsltFreeDocumentKeys(doc);
	if (!doc->main)
	    xmlFreeDoc(doc->doc);
        xmlFree(doc);
    }
    cur = ctxt->styleList;
    while (cur != NULL) {
	doc = cur;
	cur = cur->next;
	xsltFreeDocumentKeys(doc);
	if (!doc->main)
	    xmlFreeDoc(doc->doc);
        xmlFree(doc);
    }
}

/**
 * xsltLoadDocument:
 * @ctxt: an XSLT transformation context
 * @URI:  the computed URI of the document
 *
 * Try to load a document (not a stylesheet)
 * within the XSLT transformation context
 *
 * Returns the new xsltDocumentPtr or NULL in case of error
 */
xsltDocumentPtr
xsltLoadDocument(xsltTransformContextPtr ctxt, const xmlChar *URI) {
    xsltDocumentPtr ret;
    xmlDocPtr doc;

    if ((ctxt == NULL) || (URI == NULL))
	return(NULL);

    /*
     * Security framework check
     */
    if (ctxt->sec != NULL) {
	int res;

	res = xsltCheckRead(ctxt->sec, ctxt, URI);
	if (res <= 0) {
            if (res == 0)
                xsltTransformError(ctxt, NULL, NULL,
                     "xsltLoadDocument: read rights for %s denied\n",
                                 URI);
	    return(NULL);
	}
    }

    /*
     * Walk the context list to find the document if preparsed
     */
    ret = ctxt->docList;
    while (ret != NULL) {
	if ((ret->doc != NULL) && (ret->doc->URL != NULL) &&
	    (xmlStrEqual(ret->doc->URL, URI)))
	    return(ret);
	ret = ret->next;
    }

    doc = xsltDocDefaultLoader(URI, ctxt->dict, ctxt->parserOptions,
                               (void *) ctxt, XSLT_LOAD_DOCUMENT);

    if (doc == NULL)
	return(NULL);

    if (ctxt->xinclude != 0) {
#ifdef LIBXML_XINCLUDE_ENABLED
#if LIBXML_VERSION >= 20603
	xmlXIncludeProcessFlags(doc, ctxt->parserOptions);
#else
	xmlXIncludeProcess(doc);
#endif
#else
	xsltTransformError(ctxt, NULL, NULL,
	    "xsltLoadDocument(%s) : XInclude processing not compiled in\n",
	                 URI);
#endif
    }
    /*
     * Apply white-space stripping if asked for
     */
    if (xsltNeedElemSpaceHandling(ctxt))
	xsltApplyStripSpaces(ctxt, xmlDocGetRootElement(doc));
    if (ctxt->debugStatus == XSLT_DEBUG_NONE)
	xmlXPathOrderDocElems(doc);

    ret = xsltNewDocument(ctxt, doc);
    return(ret);
}

/**
 * xsltLoadStyleDocument:
 * @style: an XSLT style sheet
 * @URI:  the computed URI of the document
 *
 * Try to load a stylesheet document within the XSLT transformation context
 *
 * Returns the new xsltDocumentPtr or NULL in case of error
 */
xsltDocumentPtr
xsltLoadStyleDocument(xsltStylesheetPtr style, const xmlChar *URI) {
    xsltDocumentPtr ret;
    xmlDocPtr doc;
    xsltSecurityPrefsPtr sec;

    if ((style == NULL) || (URI == NULL))
	return(NULL);

    /*
     * Security framework check
     */
    sec = xsltGetDefaultSecurityPrefs();
    if (sec != NULL) {
	int res;

	res = xsltCheckRead(sec, NULL, URI);
	if (res <= 0) {
            if (res == 0)
                xsltTransformError(NULL, NULL, NULL,
                     "xsltLoadStyleDocument: read rights for %s denied\n",
                                 URI);
	    return(NULL);
	}
    }

    /*
     * Walk the context list to find the document if preparsed
     */
    ret = style->docList;
    while (ret != NULL) {
	if ((ret->doc != NULL) && (ret->doc->URL != NULL) &&
	    (xmlStrEqual(ret->doc->URL, URI)))
	    return(ret);
	ret = ret->next;
    }

    doc = xsltDocDefaultLoader(URI, style->dict, XSLT_PARSE_OPTIONS,
                               (void *) style, XSLT_LOAD_STYLESHEET);
    if (doc == NULL)
	return(NULL);

    ret = xsltNewStyleDocument(style, doc);
    if (ret == NULL)
        xmlFreeDoc(doc);
    return(ret);
}

/**
 * xsltFindDocument:
 * @ctxt: an XSLT transformation context
 * @doc: a parsed XML document
 *
 * Try to find a document within the XSLT transformation context.
 * This will not find document infos for temporary
 * Result Tree Fragments.
 *
 * Returns the desired xsltDocumentPtr or NULL in case of error
 */
xsltDocumentPtr
xsltFindDocument (xsltTransformContextPtr ctxt, xmlDocPtr doc) {
    xsltDocumentPtr ret;

    if ((ctxt == NULL) || (doc == NULL))
	return(NULL);

    /*
     * Walk the context list to find the document
     */
    ret = ctxt->docList;
    while (ret != NULL) {
	if (ret->doc == doc)
	    return(ret);
	ret = ret->next;
    }
    if (doc == ctxt->style->doc)
	return(ctxt->document);
    return(NULL);
}

