/*
 * attrvt.c: Implementation of the XSL Transformation 1.0 engine
 *           attribute value template handling part.
 *
 * References:
 *   http://www.w3.org/TR/1999/REC-xslt-19991116
 *
 *   Michael Kay "XSLT Programmer's Reference" pp 637-643
 *   Writing Multiple Output Files
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
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include "xslt.h"
#include "xsltutils.h"
#include "xsltInternals.h"
#include "templates.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_AVT
#endif

#define MAX_AVT_SEG 10

typedef struct _xsltAttrVT xsltAttrVT;
typedef xsltAttrVT *xsltAttrVTPtr;
struct _xsltAttrVT {
    struct _xsltAttrVT *next; /* next xsltAttrVT */
    int nb_seg;		/* Number of segments */
    int max_seg;	/* max capacity before re-alloc needed */
    int strstart;	/* is the start a string */
    /*
     * the namespaces in scope
     */
    xmlNsPtr *nsList;
    int nsNr;
    /*
     * the content is an alternate of string and xmlXPathCompExprPtr
     */
#if __STDC_VERSION__ >= 199901L
    /* Using a C99 flexible array member avoids false positives under UBSan */
    void *segments[];
#else
    void *segments[1];
#endif
};

/**
 * xsltNewAttrVT:
 * @style:  a XSLT process context
 *
 * Build a new xsltAttrVT structure
 *
 * Returns the structure or NULL in case of error
 */
static xsltAttrVTPtr
xsltNewAttrVT(xsltStylesheetPtr style) {
    xsltAttrVTPtr cur;
    size_t size = sizeof(xsltAttrVT) + MAX_AVT_SEG * sizeof(void*);

    cur = (xsltAttrVTPtr) xmlMalloc(size);
    if (cur == NULL) {
	xsltTransformError(NULL, style, NULL,
		"xsltNewAttrVTPtr : malloc failed\n");
	if (style != NULL) style->errors++;
	return(NULL);
    }
    memset(cur, 0, size);

    cur->nb_seg = 0;
    cur->max_seg = MAX_AVT_SEG;
    cur->strstart = 0;
    cur->next = style->attVTs;
    /*
     * Note: this pointer may be changed by a re-alloc within xsltCompileAttr,
     * so that code may change the stylesheet pointer also!
     */
    style->attVTs = (xsltAttrVTPtr) cur;

    return(cur);
}

/**
 * xsltFreeAttrVT:
 * @avt: pointer to an xsltAttrVT structure
 *
 * Free up the memory associated to the attribute value template
 */
static void
xsltFreeAttrVT(xsltAttrVTPtr avt) {
    int i;

    if (avt == NULL) return;

    if (avt->strstart == 1) {
	for (i = 0;i < avt->nb_seg; i += 2)
	    if (avt->segments[i] != NULL)
		xmlFree((xmlChar *) avt->segments[i]);
	for (i = 1;i < avt->nb_seg; i += 2)
	    xmlXPathFreeCompExpr((xmlXPathCompExprPtr) avt->segments[i]);
    } else {
	for (i = 0;i < avt->nb_seg; i += 2)
	    xmlXPathFreeCompExpr((xmlXPathCompExprPtr) avt->segments[i]);
	for (i = 1;i < avt->nb_seg; i += 2)
	    if (avt->segments[i] != NULL)
		xmlFree((xmlChar *) avt->segments[i]);
    }
    if (avt->nsList != NULL)
        xmlFree(avt->nsList);
    xmlFree(avt);
}

/**
 * xsltFreeAVTList:
 * @avt: pointer to an list of AVT structures
 *
 * Free up the memory associated to the attribute value templates
 */
void
xsltFreeAVTList(void *avt) {
    xsltAttrVTPtr cur = (xsltAttrVTPtr) avt, next;

    while (cur != NULL) {
        next = cur->next;
	xsltFreeAttrVT(cur);
	cur = next;
    }
}
/**
 * xsltSetAttrVTsegment:
 * @ avt: pointer to an xsltAttrVT structure
 * @ val: the value to be set to the next available segment
 *
 * Within xsltCompileAttr there are several places where a value
 * needs to be added to the 'segments' array within the xsltAttrVT
 * structure, and at each place the allocated size may have to be
 * re-allocated.  This routine takes care of that situation.
 *
 * Returns the avt pointer, which may have been changed by a re-alloc
 */
static xsltAttrVTPtr
xsltSetAttrVTsegment(xsltAttrVTPtr avt, void *val) {
    if (avt->nb_seg >= avt->max_seg) {
        size_t size = sizeof(xsltAttrVT) +
                      (avt->max_seg + MAX_AVT_SEG) * sizeof(void *);
	xsltAttrVTPtr tmp = (xsltAttrVTPtr) xmlRealloc(avt, size);
	if (tmp == NULL) {
            xsltFreeAttrVT(avt);
	    return NULL;
	}
        avt = tmp;
	memset(&avt->segments[avt->nb_seg], 0, MAX_AVT_SEG*sizeof(void *));
	avt->max_seg += MAX_AVT_SEG;
    }
    avt->segments[avt->nb_seg++] = val;
    return avt;
}

/**
 * xsltCompileAttr:
 * @style:  a XSLT process context
 * @attr: the attribute coming from the stylesheet.
 *
 * Precompile an attribute in a stylesheet, basically it checks if it is
 * an attribute value template, and if yes, establish some structures needed
 * to process it at transformation time.
 */
void
xsltCompileAttr(xsltStylesheetPtr style, xmlAttrPtr attr) {
    const xmlChar *str;
    const xmlChar *cur;
    xmlChar *ret = NULL;
    xmlChar *expr = NULL;
    xsltAttrVTPtr avt;
    int i = 0, lastavt = 0;

    if ((style == NULL) || (attr == NULL) || (attr->children == NULL))
        return;
    if ((attr->children->type != XML_TEXT_NODE) ||
        (attr->children->next != NULL)) {
        xsltTransformError(NULL, style, attr->parent,
	    "Attribute '%s': The content is expected to be a single text "
	    "node when compiling an AVT.\n", attr->name);
	style->errors++;
	return;
    }
    str = attr->children->content;
    if ((xmlStrchr(str, '{') == NULL) &&
        (xmlStrchr(str, '}') == NULL)) return;

#ifdef WITH_XSLT_DEBUG_AVT
    xsltGenericDebug(xsltGenericDebugContext,
		    "Found AVT %s: %s\n", attr->name, str);
#endif
    if (attr->psvi != NULL) {
#ifdef WITH_XSLT_DEBUG_AVT
	xsltGenericDebug(xsltGenericDebugContext,
			"AVT %s: already compiled\n", attr->name);
#endif
        return;
    }
    /*
    * Create a new AVT object.
    */
    avt = xsltNewAttrVT(style);
    if (avt == NULL)
	return;
    attr->psvi = avt;

    avt->nsList = xmlGetNsList(attr->doc, attr->parent);
    if (avt->nsList != NULL) {
	while (avt->nsList[i] != NULL)
	    i++;
    }
    avt->nsNr = i;

    cur = str;
    while (*cur != 0) {
	if (*cur == '{') {
	    if (*(cur+1) == '{') {	/* escaped '{' */
	        cur++;
		ret = xmlStrncat(ret, str, cur - str);
		cur++;
		str = cur;
		continue;
	    }
	    if (*(cur+1) == '}') {	/* skip empty AVT */
		ret = xmlStrncat(ret, str, cur - str);
	        cur += 2;
		str = cur;
		continue;
	    }
	    if ((ret != NULL) || (cur - str > 0)) {
		ret = xmlStrncat(ret, str, cur - str);
		str = cur;
		if (avt->nb_seg == 0)
		    avt->strstart = 1;
		if ((avt = xsltSetAttrVTsegment(avt, (void *) ret)) == NULL)
		    goto error;
		ret = NULL;
		lastavt = 0;
	    }

	    cur++;
	    while ((*cur != 0) && (*cur != '}')) {
		/* Need to check for literal (bug539741) */
		if ((*cur == '\'') || (*cur == '"')) {
		    char delim = *(cur++);
		    while ((*cur != 0) && (*cur != delim))
			cur++;
		    if (*cur != 0)
			cur++;	/* skip the ending delimiter */
		} else
		    cur++;
	    }
	    if (*cur == 0) {
	        xsltTransformError(NULL, style, attr->parent,
		     "Attribute '%s': The AVT has an unmatched '{'.\n",
		     attr->name);
		style->errors++;
		goto error;
	    }
	    str++;
	    expr = xmlStrndup(str, cur - str);
	    if (expr == NULL) {
		/*
		* TODO: What needs to be done here?
		*/
	        XSLT_TODO
		goto error;
	    } else {
		xmlXPathCompExprPtr comp;

		comp = xsltXPathCompile(style, expr);
		if (comp == NULL) {
		    xsltTransformError(NULL, style, attr->parent,
			 "Attribute '%s': Failed to compile the expression "
			 "'%s' in the AVT.\n", attr->name, expr);
		    style->errors++;
		    goto error;
		}
		if (avt->nb_seg == 0)
		    avt->strstart = 0;
		if (lastavt == 1) {
		    if ((avt = xsltSetAttrVTsegment(avt, NULL)) == NULL)
		        goto error;
		}
		if ((avt = xsltSetAttrVTsegment(avt, (void *) comp)) == NULL)
		    goto error;
		lastavt = 1;
		xmlFree(expr);
		expr = NULL;
	    }
	    cur++;
	    str = cur;
	} else if (*cur == '}') {
	    cur++;
	    if (*cur == '}') {	/* escaped '}' */
		ret = xmlStrncat(ret, str, cur - str);
		cur++;
		str = cur;
		continue;
	    } else {
	        xsltTransformError(NULL, style, attr->parent,
		     "Attribute '%s': The AVT has an unmatched '}'.\n",
		     attr->name);
		goto error;
	    }
	} else
	    cur++;
    }
    if ((ret != NULL) || (cur - str > 0)) {
	ret = xmlStrncat(ret, str, cur - str);
	str = cur;
	if (avt->nb_seg == 0)
	    avt->strstart = 1;
	if ((avt = xsltSetAttrVTsegment(avt, (void *) ret)) == NULL)
	    goto error;
	ret = NULL;
    }

error:
    if (avt == NULL) {
        xsltTransformError(NULL, style, attr->parent,
		"xsltCompileAttr: malloc problem\n");
    } else {
        if (attr->psvi != avt) {  /* may have changed from realloc */
            attr->psvi = avt;
	    /*
	     * This is a "hack", but I can't see any clean method of
	     * doing it.  If a re-alloc has taken place, then the pointer
	     * for this AVT may have changed.  style->attVTs was set by
	     * xsltNewAttrVT, so it needs to be re-set to the new value!
	     */
	    style->attVTs = avt;
	}
    }
    if (ret != NULL)
	xmlFree(ret);
    if (expr != NULL)
	xmlFree(expr);
}


/**
 * xsltEvalAVT:
 * @ctxt: the XSLT transformation context
 * @avt: the prevompiled attribute value template info
 * @node: the node hosting the attribute
 *
 * Process the given AVT, and return the new string value.
 *
 * Returns the computed string value or NULL, must be deallocated by the
 *         caller.
 */
xmlChar *
xsltEvalAVT(xsltTransformContextPtr ctxt, void *avt, xmlNodePtr node) {
    xmlChar *ret = NULL, *tmp;
    xmlXPathCompExprPtr comp;
    xsltAttrVTPtr cur = (xsltAttrVTPtr) avt;
    int i;
    int str;

    if ((ctxt == NULL) || (avt == NULL) || (node == NULL))
        return(NULL);
    str = cur->strstart;
    for (i = 0;i < cur->nb_seg;i++) {
        if (str) {
	    ret = xmlStrcat(ret, (const xmlChar *) cur->segments[i]);
	} else {
	    comp = (xmlXPathCompExprPtr) cur->segments[i];
	    tmp = xsltEvalXPathStringNs(ctxt, comp, cur->nsNr, cur->nsList);
	    if (tmp != NULL) {
	        if (ret != NULL) {
		    ret = xmlStrcat(ret, tmp);
		    xmlFree(tmp);
		} else {
		    ret = tmp;
		}
	    }
	}
	str = !str;
    }
    return(ret);
}
