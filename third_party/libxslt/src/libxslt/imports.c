/*
 * imports.c: Implementation of the XSLT imports
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
#include <libxml/parser.h>
#include <libxml/hash.h>
#include <libxml/xmlerror.h>
#include <libxml/uri.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "preproc.h"
#include "imports.h"
#include "documents.h"
#include "security.h"
#include "pattern.h"


/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/
/**
 * xsltFixImportedCompSteps:
 * @master: the "master" stylesheet
 * @style: the stylesheet being imported by the master
 *
 * normalize the comp steps for the stylesheet being imported
 * by the master, together with any imports within that.
 *
 */
static void xsltFixImportedCompSteps(xsltStylesheetPtr master,
			xsltStylesheetPtr style) {
    xsltStylesheetPtr res;
    xmlHashScan(style->templatesHash, xsltNormalizeCompSteps, master);
    master->extrasNr += style->extrasNr;
    for (res = style->imports; res != NULL; res = res->next) {
        xsltFixImportedCompSteps(master, res);
    }
}

#define XSLT_MAX_NESTING 40

static int
xsltCheckCycle(xsltStylesheetPtr style, xmlNodePtr cur, const xmlChar *URI) {
    xsltStylesheetPtr ancestor;
    xsltDocumentPtr docptr;
    int depth;

    /*
     * Check imported stylesheets.
     */
    depth = 0;
    ancestor = style;
    while (ancestor != NULL) {
        if (++depth >= XSLT_MAX_NESTING) {
            xsltTransformError(NULL, style, cur,
               "maximum nesting depth exceeded: %s\n", URI);
            return(-1);
        }
	if (xmlStrEqual(ancestor->doc->URL, URI)) {
            xsltTransformError(NULL, style, cur,
               "recursion detected on imported URL %s\n", URI);
	    return(-1);
        }

        /*
         * Check included stylesheets.
         */
        docptr = ancestor->includes;
        while (docptr != NULL) {
            if (++depth >= XSLT_MAX_NESTING) {
                xsltTransformError(NULL, style, cur,
                   "maximum nesting depth exceeded: %s\n", URI);
                return(-1);
            }
            if (xmlStrEqual(docptr->doc->URL, URI)) {
                xsltTransformError(NULL, style, cur,
                   "recursion detected on included URL %s\n", URI);
                return(-1);
            }
            docptr = docptr->includes;
        }

	ancestor = ancestor->parent;
    }

    return(0);
}

/**
 * xsltParseStylesheetImport:
 * @style:  the XSLT stylesheet
 * @cur:  the import element
 *
 * parse an XSLT stylesheet import element
 *
 * Returns 0 in case of success -1 in case of failure.
 */

int
xsltParseStylesheetImport(xsltStylesheetPtr style, xmlNodePtr cur) {
    int ret = -1;
    xmlDocPtr import = NULL;
    xmlChar *base = NULL;
    xmlChar *uriRef = NULL;
    xmlChar *URI = NULL;
    xsltStylesheetPtr res;
    xsltSecurityPrefsPtr sec;

    if ((cur == NULL) || (style == NULL))
	return (ret);

    uriRef = xmlGetNsProp(cur, (const xmlChar *)"href", NULL);
    if (uriRef == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsl:import : missing href attribute\n");
	goto error;
    }

    base = xmlNodeGetBase(style->doc, cur);
    URI = xmlBuildURI(uriRef, base);
    if (URI == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsl:import : invalid URI reference %s\n", uriRef);
	goto error;
    }

    if (xsltCheckCycle(style, cur, URI) < 0)
        goto error;

    /*
     * Security framework check
     */
    sec = xsltGetDefaultSecurityPrefs();
    if (sec != NULL) {
	int secres;

	secres = xsltCheckRead(sec, NULL, URI);
	if (secres <= 0) {
            if (secres == 0)
                xsltTransformError(NULL, NULL, NULL,
                     "xsl:import: read rights for %s denied\n",
                                 URI);
	    goto error;
	}
    }

    import = xsltDocDefaultLoader(URI, style->dict, XSLT_PARSE_OPTIONS,
                                  (void *) style, XSLT_LOAD_STYLESHEET);
    if (import == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsl:import : unable to load %s\n", URI);
	goto error;
    }

    res = xsltParseStylesheetImportedDoc(import, style);
    if (res != NULL) {
	res->next = style->imports;
	style->imports = res;
	if (style->parent == NULL) {
	    xsltFixImportedCompSteps(style, res);
	}
	ret = 0;
    } else {
	xmlFreeDoc(import);
	}

error:
    if (uriRef != NULL)
	xmlFree(uriRef);
    if (base != NULL)
	xmlFree(base);
    if (URI != NULL)
	xmlFree(URI);

    return (ret);
}

/**
 * xsltParseStylesheetInclude:
 * @style:  the XSLT stylesheet
 * @cur:  the include node
 *
 * parse an XSLT stylesheet include element
 *
 * Returns 0 in case of success -1 in case of failure
 */

int
xsltParseStylesheetInclude(xsltStylesheetPtr style, xmlNodePtr cur) {
    int ret = -1;
    xmlDocPtr oldDoc;
    xmlChar *base = NULL;
    xmlChar *uriRef = NULL;
    xmlChar *URI = NULL;
    xsltStylesheetPtr result;
    xsltDocumentPtr include;
    int oldNopreproc;

    if ((cur == NULL) || (style == NULL))
	return (ret);

    uriRef = xmlGetNsProp(cur, (const xmlChar *)"href", NULL);
    if (uriRef == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsl:include : missing href attribute\n");
	goto error;
    }

    base = xmlNodeGetBase(style->doc, cur);
    URI = xmlBuildURI(uriRef, base);
    if (URI == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsl:include : invalid URI reference %s\n", uriRef);
	goto error;
    }

    if (xsltCheckCycle(style, cur, URI) < 0)
        goto error;

    include = xsltLoadStyleDocument(style, URI);
    if (include == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsl:include : unable to load %s\n", URI);
	goto error;
    }
#ifdef XSLT_REFACTORED
    if (IS_XSLT_ELEM_FAST(cur) && (cur->psvi != NULL)) {
	((xsltStyleItemIncludePtr) cur->psvi)->include = include;
    } else {
	xsltTransformError(NULL, style, cur,
	    "Internal error: (xsltParseStylesheetInclude) "
	    "The xsl:include element was not compiled.\n", URI);
	style->errors++;
    }
#endif
    oldDoc = style->doc;
    style->doc = include->doc;
    /* chain to stylesheet for recursion checking */
    include->includes = style->includes;
    style->includes = include;
    oldNopreproc = style->nopreproc;
    style->nopreproc = include->preproc;
    /*
    * TODO: This will change some values of the
    *  including stylesheet with every included module
    *  (e.g. excluded-result-prefixes)
    *  We need to strictly seperate such stylesheet-owned values.
    */
    result = xsltParseStylesheetProcess(style, include->doc);
    style->nopreproc = oldNopreproc;
    include->preproc = 1;
    style->includes = include->includes;
    style->doc = oldDoc;
    if (result == NULL) {
	ret = -1;
	goto error;
    }
    ret = 0;

error:
    if (uriRef != NULL)
	xmlFree(uriRef);
    if (base != NULL)
	xmlFree(base);
    if (URI != NULL)
	xmlFree(URI);

    return (ret);
}

/**
 * xsltNextImport:
 * @cur:  the current XSLT stylesheet
 *
 * Find the next stylesheet in import precedence.
 *
 * Returns the next stylesheet or NULL if it was the last one
 */

xsltStylesheetPtr
xsltNextImport(xsltStylesheetPtr cur) {
    if (cur == NULL)
	return(NULL);
    if (cur->imports != NULL)
	return(cur->imports);
    if (cur->next != NULL)
	return(cur->next) ;
    do {
	cur = cur->parent;
	if (cur == NULL) break;
	if (cur->next != NULL) return(cur->next);
    } while (cur != NULL);
    return(cur);
}

/**
 * xsltNeedElemSpaceHandling:
 * @ctxt:  an XSLT transformation context
 *
 * Checks whether that stylesheet requires white-space stripping
 *
 * Returns 1 if space should be stripped, 0 if not
 */

int
xsltNeedElemSpaceHandling(xsltTransformContextPtr ctxt) {
    xsltStylesheetPtr style;

    if (ctxt == NULL)
	return(0);
    style = ctxt->style;
    while (style != NULL) {
	if (style->stripSpaces != NULL)
	    return(1);
	style = xsltNextImport(style);
    }
    return(0);
}

/**
 * xsltFindElemSpaceHandling:
 * @ctxt:  an XSLT transformation context
 * @node:  an XML node
 *
 * Find strip-space or preserve-space information for an element
 * respect the import precedence or the wildcards
 *
 * Returns 1 if space should be stripped, 0 if not, and 2 if everything
 *         should be CDTATA wrapped.
 */

int
xsltFindElemSpaceHandling(xsltTransformContextPtr ctxt, xmlNodePtr node) {
    xsltStylesheetPtr style;
    const xmlChar *val;

    if ((ctxt == NULL) || (node == NULL))
	return(0);
    style = ctxt->style;
    while (style != NULL) {
	if (node->ns != NULL) {
	    val = (const xmlChar *)
	      xmlHashLookup2(style->stripSpaces, node->name, node->ns->href);
            if (val == NULL) {
                val = (const xmlChar *)
                    xmlHashLookup2(style->stripSpaces, BAD_CAST "*",
                                   node->ns->href);
            }
	} else {
	    val = (const xmlChar *)
		  xmlHashLookup2(style->stripSpaces, node->name, NULL);
	}
	if (val != NULL) {
	    if (xmlStrEqual(val, (xmlChar *) "strip"))
		return(1);
	    if (xmlStrEqual(val, (xmlChar *) "preserve"))
		return(0);
	}
	if (style->stripAll == 1)
	    return(1);
	if (style->stripAll == -1)
	    return(0);

	style = xsltNextImport(style);
    }
    return(0);
}

/**
 * xsltFindTemplate:
 * @ctxt:  an XSLT transformation context
 * @name: the template name
 * @nameURI: the template name URI
 *
 * Finds the named template, apply import precedence rule.
 * REVISIT TODO: We'll change the nameURI fields of
 *  templates to be in the string dict, so if the
 *  specified @nameURI is in the same dict, then use pointer
 *  comparison. Check if this can be done in a sane way.
 *  Maybe this function is not needed internally at
 *  transformation-time if we hard-wire the called templates
 *  to the caller.
 *
 * Returns the xsltTemplatePtr or NULL if not found
 */
xsltTemplatePtr
xsltFindTemplate(xsltTransformContextPtr ctxt, const xmlChar *name,
	         const xmlChar *nameURI) {
    xsltTemplatePtr cur;
    xsltStylesheetPtr style;

    if ((ctxt == NULL) || (name == NULL))
	return(NULL);
    style = ctxt->style;
    while (style != NULL) {
        if (style->namedTemplates != NULL) {
            cur = (xsltTemplatePtr)
                xmlHashLookup2(style->namedTemplates, name, nameURI);
            if (cur != NULL)
                return(cur);
        }

	style = xsltNextImport(style);
    }
    return(NULL);
}

