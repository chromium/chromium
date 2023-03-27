/*
 * keys.c: Implemetation of the keys support
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
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/hash.h>
#include <libxml/xmlerror.h>
#include <libxml/parserInternals.h>
#include <libxml/xpathInternals.h>
#include <libxml/xpath.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "imports.h"
#include "templates.h"
#include "keys.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_KEYS
#endif

static int
xsltInitDocKeyTable(xsltTransformContextPtr ctxt, const xmlChar *name,
                    const xmlChar *nameURI);

/************************************************************************
 *									*
 *			Type functions					*
 *									*
 ************************************************************************/

/**
 * xsltNewKeyDef:
 * @name:  the key name or NULL
 * @nameURI:  the name URI or NULL
 *
 * Create a new XSLT KeyDef
 *
 * Returns the newly allocated xsltKeyDefPtr or NULL in case of error
 */
static xsltKeyDefPtr
xsltNewKeyDef(const xmlChar *name, const xmlChar *nameURI) {
    xsltKeyDefPtr cur;

    cur = (xsltKeyDefPtr) xmlMalloc(sizeof(xsltKeyDef));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltNewKeyDef : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltKeyDef));
    if (name != NULL)
	cur->name = xmlStrdup(name);
    if (nameURI != NULL)
	cur->nameURI = xmlStrdup(nameURI);
    cur->nsList = NULL;
    return(cur);
}

/**
 * xsltFreeKeyDef:
 * @keyd:  an XSLT key definition
 *
 * Free up the memory allocated by @keyd
 */
static void
xsltFreeKeyDef(xsltKeyDefPtr keyd) {
    if (keyd == NULL)
	return;
    if (keyd->comp != NULL)
	xmlXPathFreeCompExpr(keyd->comp);
    if (keyd->usecomp != NULL)
	xmlXPathFreeCompExpr(keyd->usecomp);
    if (keyd->name != NULL)
	xmlFree(keyd->name);
    if (keyd->nameURI != NULL)
	xmlFree(keyd->nameURI);
    if (keyd->match != NULL)
	xmlFree(keyd->match);
    if (keyd->use != NULL)
	xmlFree(keyd->use);
    if (keyd->nsList != NULL)
        xmlFree(keyd->nsList);
    memset(keyd, -1, sizeof(xsltKeyDef));
    xmlFree(keyd);
}

/**
 * xsltFreeKeyDefList:
 * @keyd:  an XSLT key definition list
 *
 * Free up the memory allocated by all the elements of @keyd
 */
static void
xsltFreeKeyDefList(xsltKeyDefPtr keyd) {
    xsltKeyDefPtr cur;

    while (keyd != NULL) {
	cur = keyd;
	keyd = keyd->next;
	xsltFreeKeyDef(cur);
    }
}

/**
 * xsltNewKeyTable:
 * @name:  the key name or NULL
 * @nameURI:  the name URI or NULL
 *
 * Create a new XSLT KeyTable
 *
 * Returns the newly allocated xsltKeyTablePtr or NULL in case of error
 */
static xsltKeyTablePtr
xsltNewKeyTable(const xmlChar *name, const xmlChar *nameURI) {
    xsltKeyTablePtr cur;

    cur = (xsltKeyTablePtr) xmlMalloc(sizeof(xsltKeyTable));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltNewKeyTable : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltKeyTable));
    if (name != NULL)
	cur->name = xmlStrdup(name);
    if (nameURI != NULL)
	cur->nameURI = xmlStrdup(nameURI);
    cur->keys = xmlHashCreate(0);
    return(cur);
}

static void
xsltFreeNodeSetEntry(void *payload, const xmlChar *name ATTRIBUTE_UNUSED) {
    xmlXPathFreeNodeSet((xmlNodeSetPtr) payload);
}

/**
 * xsltFreeKeyTable:
 * @keyt:  an XSLT key table
 *
 * Free up the memory allocated by @keyt
 */
static void
xsltFreeKeyTable(xsltKeyTablePtr keyt) {
    if (keyt == NULL)
	return;
    if (keyt->name != NULL)
	xmlFree(keyt->name);
    if (keyt->nameURI != NULL)
	xmlFree(keyt->nameURI);
    if (keyt->keys != NULL)
	xmlHashFree(keyt->keys, xsltFreeNodeSetEntry);
    memset(keyt, -1, sizeof(xsltKeyTable));
    xmlFree(keyt);
}

/**
 * xsltFreeKeyTableList:
 * @keyt:  an XSLT key table list
 *
 * Free up the memory allocated by all the elements of @keyt
 */
static void
xsltFreeKeyTableList(xsltKeyTablePtr keyt) {
    xsltKeyTablePtr cur;

    while (keyt != NULL) {
	cur = keyt;
	keyt = keyt->next;
	xsltFreeKeyTable(cur);
    }
}

/************************************************************************
 *									*
 *		The interpreter for the precompiled patterns		*
 *									*
 ************************************************************************/


/**
 * xsltFreeKeys:
 * @style: an XSLT stylesheet
 *
 * Free up the memory used by XSLT keys in a stylesheet
 */
void
xsltFreeKeys(xsltStylesheetPtr style) {
    if (style->keys)
	xsltFreeKeyDefList((xsltKeyDefPtr) style->keys);
}

/**
 * skipString:
 * @cur: the current pointer
 * @end: the current offset
 *
 * skip a string delimited by " or '
 *
 * Returns the byte after the string or -1 in case of error
 */
static int
skipString(const xmlChar *cur, int end) {
    xmlChar limit;

    if ((cur == NULL) || (end < 0)) return(-1);
    if ((cur[end] == '\'') || (cur[end] == '"')) limit = cur[end];
    else return(end);
    end++;
    while (cur[end] != 0) {
        if (cur[end] == limit)
	    return(end + 1);
	end++;
    }
    return(-1);
}

/**
 * skipPredicate:
 * @cur: the current pointer
 * @end: the current offset
 *
 * skip a predicate
 *
 * Returns the byte after the predicate or -1 in case of error
 */
static int
skipPredicate(const xmlChar *cur, int end) {
    int level = 0;

    if ((cur == NULL) || (end < 0)) return(-1);
    if (cur[end] != '[') return(end);
    end++;
    while (cur[end] != 0) {
        if ((cur[end] == '\'') || (cur[end] == '"')) {
	    end = skipString(cur, end);
	    if (end <= 0)
	        return(-1);
	    continue;
	} else if (cur[end] == '[') {
            level += 1;
	} else if (cur[end] == ']') {
            if (level == 0)
	        return(end + 1);
            level -= 1;
        }
	end++;
    }
    return(-1);
}

/**
 * xsltAddKey:
 * @style: an XSLT stylesheet
 * @name:  the key name or NULL
 * @nameURI:  the name URI or NULL
 * @match:  the match value
 * @use:  the use value
 * @inst: the key instruction
 *
 * add a key definition to a stylesheet
 *
 * Returns 0 in case of success, and -1 in case of failure.
 */
int
xsltAddKey(xsltStylesheetPtr style, const xmlChar *name,
	   const xmlChar *nameURI, const xmlChar *match,
	   const xmlChar *use, xmlNodePtr inst) {
    xsltKeyDefPtr key;
    xmlChar *pattern = NULL;
    int current, end, start, i = 0;

    if ((style == NULL) || (name == NULL) || (match == NULL) || (use == NULL))
	return(-1);

#ifdef WITH_XSLT_DEBUG_KEYS
    xsltGenericDebug(xsltGenericDebugContext,
	"Add key %s, match %s, use %s\n", name, match, use);
#endif

    key = xsltNewKeyDef(name, nameURI);
    if (key == NULL)
        return(-1);
    key->match = xmlStrdup(match);
    key->use = xmlStrdup(use);
    key->inst = inst;
    key->nsList = xmlGetNsList(inst->doc, inst);
    if (key->nsList != NULL) {
        while (key->nsList[i] != NULL)
	    i++;
    }
    key->nsNr = i;

    /*
     * Split the | and register it as as many keys
     */
    current = end = 0;
    while (match[current] != 0) {
	start = current;
	while (xmlIsBlank_ch(match[current]))
	    current++;
	end = current;
	while ((match[end] != 0) && (match[end] != '|')) {
	    if (match[end] == '[') {
	        end = skipPredicate(match, end);
		if (end <= 0) {
		    xsltTransformError(NULL, style, inst,
		        "xsl:key : 'match' pattern is malformed: %s",
		        key->match);
		    if (style != NULL) style->errors++;
		    goto error;
		}
	    } else
		end++;
	}
	if (current == end) {
	    xsltTransformError(NULL, style, inst,
			       "xsl:key : 'match' pattern is empty\n");
	    if (style != NULL) style->errors++;
	    goto error;
	}
	if (match[start] != '/') {
	    pattern = xmlStrcat(pattern, (xmlChar *)"//");
	    if (pattern == NULL) {
		if (style != NULL) style->errors++;
		goto error;
	    }
	}
	pattern = xmlStrncat(pattern, &match[start], end - start);
	if (pattern == NULL) {
	    if (style != NULL) style->errors++;
	    goto error;
	}

	if (match[end] == '|') {
	    pattern = xmlStrcat(pattern, (xmlChar *)"|");
	    end++;
	}
	current = end;
    }
    if (pattern == NULL) {
        xsltTransformError(NULL, style, inst,
                           "xsl:key : 'match' pattern is empty\n");
        if (style != NULL) style->errors++;
        goto error;
    }
#ifdef WITH_XSLT_DEBUG_KEYS
    xsltGenericDebug(xsltGenericDebugContext,
	"   resulting pattern %s\n", pattern);
#endif
    /*
    * XSLT-1: "It is an error for the value of either the use
    *  attribute or the match attribute to contain a
    *  VariableReference."
    * TODO: We should report a variable-reference at compile-time.
    *   Maybe a search for "$", if it occurs outside of quotation
    *   marks, could be sufficient.
    */
#ifdef XML_XPATH_NOVAR
    key->comp = xsltXPathCompileFlags(style, pattern, XML_XPATH_NOVAR);
#else
    key->comp = xsltXPathCompile(style, pattern);
#endif
    if (key->comp == NULL) {
	xsltTransformError(NULL, style, inst,
		"xsl:key : 'match' pattern compilation failed '%s'\n",
		         pattern);
	if (style != NULL) style->errors++;
    }
#ifdef XML_XPATH_NOVAR
    key->usecomp = xsltXPathCompileFlags(style, use, XML_XPATH_NOVAR);
#else
    key->usecomp = xsltXPathCompile(style, use);
#endif
    if (key->usecomp == NULL) {
	xsltTransformError(NULL, style, inst,
		"xsl:key : 'use' expression compilation failed '%s'\n",
		         use);
	if (style != NULL) style->errors++;
    }

    /*
     * Sometimes the stylesheet writer use the order to ease the
     * resolution of keys when they are dependant, keep the provided
     * order so add the new one at the end.
     */
    if (style->keys == NULL) {
	style->keys = key;
    } else {
        xsltKeyDefPtr prev = style->keys;

	while (prev->next != NULL)
	    prev = prev->next;

	prev->next = key;
    }
    key->next = NULL;
    key = NULL;

error:
    if (pattern != NULL)
	xmlFree(pattern);
    if (key != NULL)
        xsltFreeKeyDef(key);
    return(0);
}

/**
 * xsltGetKey:
 * @ctxt: an XSLT transformation context
 * @name:  the key name or NULL
 * @nameURI:  the name URI or NULL
 * @value:  the key value to look for
 *
 * Looks up a key of the in current source doc (the document info
 * on @ctxt->document). Computes the key if not already done
 * for the current source doc.
 *
 * Returns the nodeset resulting from the query or NULL
 */
xmlNodeSetPtr
xsltGetKey(xsltTransformContextPtr ctxt, const xmlChar *name,
	   const xmlChar *nameURI, const xmlChar *value) {
    xmlNodeSetPtr ret;
    xsltKeyTablePtr table;
    int init_table = 0;

    if ((ctxt == NULL) || (name == NULL) || (value == NULL) ||
	(ctxt->document == NULL))
	return(NULL);

#ifdef WITH_XSLT_DEBUG_KEYS
    xsltGenericDebug(xsltGenericDebugContext,
	"Get key %s, value %s\n", name, value);
#endif

    /*
     * keys are computed only on-demand on first key access for a document
     */
    if ((ctxt->document->nbKeysComputed < ctxt->nbKeys) &&
        (ctxt->keyInitLevel == 0)) {
        /*
	 * If non-recursive behaviour, just try to initialize all keys
	 */
	if (xsltInitAllDocKeys(ctxt))
	    return(NULL);
    }

retry:
    table = (xsltKeyTablePtr) ctxt->document->keys;
    while (table != NULL) {
	if (((nameURI != NULL) == (table->nameURI != NULL)) &&
	    xmlStrEqual(table->name, name) &&
	    xmlStrEqual(table->nameURI, nameURI))
	{
	    ret = (xmlNodeSetPtr)xmlHashLookup(table->keys, value);
	    return(ret);
	}
	table = table->next;
    }

    if ((ctxt->keyInitLevel != 0) && (init_table == 0)) {
        /*
	 * Apparently one key is recursive and this one is needed,
	 * initialize just it, that time and retry
	 */
        xsltInitDocKeyTable(ctxt, name, nameURI);
	init_table = 1;
	goto retry;
    }

    return(NULL);
}


/**
 * xsltInitDocKeyTable:
 *
 * INTERNAL ROUTINE ONLY
 *
 * Check if any keys on the current document need to be computed
 */
static int
xsltInitDocKeyTable(xsltTransformContextPtr ctxt, const xmlChar *name,
                    const xmlChar *nameURI)
{
    xsltStylesheetPtr style;
    xsltKeyDefPtr keyd = NULL;
    int found = 0;

#ifdef KEY_INIT_DEBUG
fprintf(stderr, "xsltInitDocKeyTable %s\n", name);
#endif

    style = ctxt->style;
    while (style != NULL) {
	keyd = (xsltKeyDefPtr) style->keys;
	while (keyd != NULL) {
	    if (((keyd->nameURI != NULL) ==
		 (nameURI != NULL)) &&
		xmlStrEqual(keyd->name, name) &&
		xmlStrEqual(keyd->nameURI, nameURI))
	    {
		xsltInitCtxtKey(ctxt, ctxt->document, keyd);
		if (ctxt->document->nbKeysComputed == ctxt->nbKeys)
		    return(0);
		found = 1;
	    }
	    keyd = keyd->next;
	}
	style = xsltNextImport(style);
    }
    if (found == 0) {
#ifdef WITH_XSLT_DEBUG_KEYS
	XSLT_TRACE(ctxt,XSLT_TRACE_KEYS,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltInitDocKeyTable: did not found %s\n", name));
#endif
	xsltTransformError(ctxt, NULL, keyd? keyd->inst : NULL,
	    "Failed to find key definition for %s\n", name);
	ctxt->state = XSLT_STATE_STOPPED;
        return(-1);
    }
#ifdef KEY_INIT_DEBUG
fprintf(stderr, "xsltInitDocKeyTable %s done\n", name);
#endif
    return(0);
}

/**
 * xsltInitAllDocKeys:
 * @ctxt: transformation context
 *
 * INTERNAL ROUTINE ONLY
 *
 * Check if any keys on the current document need to be computed
 *
 * Returns 0 in case of success, -1 in case of failure
 */
int
xsltInitAllDocKeys(xsltTransformContextPtr ctxt)
{
    xsltStylesheetPtr style;
    xsltKeyDefPtr keyd;
    xsltKeyTablePtr table;

    if (ctxt == NULL)
	return(-1);

#ifdef KEY_INIT_DEBUG
fprintf(stderr, "xsltInitAllDocKeys %d %d\n",
        ctxt->document->nbKeysComputed, ctxt->nbKeys);
#endif

    if (ctxt->document->nbKeysComputed == ctxt->nbKeys)
	return(0);


    /*
    * TODO: This could be further optimized
    */
    style = ctxt->style;
    while (style) {
	keyd = (xsltKeyDefPtr) style->keys;
	while (keyd != NULL) {
#ifdef KEY_INIT_DEBUG
fprintf(stderr, "Init key %s\n", keyd->name);
#endif
	    /*
	    * Check if keys with this QName have been already
	    * computed.
	    */
	    table = (xsltKeyTablePtr) ctxt->document->keys;
	    while (table) {
		if (((keyd->nameURI != NULL) == (table->nameURI != NULL)) &&
		    xmlStrEqual(keyd->name, table->name) &&
		    xmlStrEqual(keyd->nameURI, table->nameURI))
		{
		    break;
		}
		table = table->next;
	    }
	    if (table == NULL) {
		/*
		* Keys with this QName have not been yet computed.
		*/
		xsltInitDocKeyTable(ctxt, keyd->name, keyd->nameURI);
	    }
	    keyd = keyd->next;
	}
	style = xsltNextImport(style);
    }
#ifdef KEY_INIT_DEBUG
fprintf(stderr, "xsltInitAllDocKeys: done\n");
#endif
    return(0);
}

/**
 * xsltInitCtxtKey:
 * @ctxt: an XSLT transformation context
 * @idoc:  the document information (holds key values)
 * @keyDef: the key definition
 *
 * Computes the key tables this key and for the current input document.
 *
 * Returns: 0 on success, -1 on error
 */
int
xsltInitCtxtKey(xsltTransformContextPtr ctxt, xsltDocumentPtr idoc,
	        xsltKeyDefPtr keyDef)
{
    int i, len, k;
    xmlNodeSetPtr matchList = NULL, keylist;
    xmlXPathObjectPtr matchRes = NULL, useRes = NULL;
    xmlChar *str = NULL;
    xsltKeyTablePtr table;
    xmlNodePtr oldInst, cur;
    xmlNodePtr oldContextNode;
    xsltDocumentPtr oldDocInfo;
    int	oldXPPos, oldXPSize;
    xmlNodePtr oldXPNode;
    xmlDocPtr oldXPDoc;
    int oldXPNsNr;
    xmlNsPtr *oldXPNamespaces;
    xmlXPathContextPtr xpctxt;

#ifdef KEY_INIT_DEBUG
fprintf(stderr, "xsltInitCtxtKey %s : %d\n", keyDef->name, ctxt->keyInitLevel);
#endif

    if ((keyDef->comp == NULL) || (keyDef->usecomp == NULL))
	return(-1);

    /*
     * Detect recursive keys
     */
    if (ctxt->keyInitLevel > ctxt->nbKeys) {
#ifdef WITH_XSLT_DEBUG_KEYS
	XSLT_TRACE(ctxt,XSLT_TRACE_KEYS,
	           xsltGenericDebug(xsltGenericDebugContext,
		       "xsltInitCtxtKey: key definition of %s is recursive\n",
		       keyDef->name));
#endif
	xsltTransformError(ctxt, NULL, keyDef->inst,
	    "Key definition for %s is recursive\n", keyDef->name);
	ctxt->state = XSLT_STATE_STOPPED;
        return(-1);
    }
    ctxt->keyInitLevel++;

    xpctxt = ctxt->xpathCtxt;
    idoc->nbKeysComputed++;
    /*
    * Save context state.
    */
    oldInst = ctxt->inst;
    oldDocInfo = ctxt->document;
    oldContextNode = ctxt->node;

    oldXPNode = xpctxt->node;
    oldXPDoc = xpctxt->doc;
    oldXPPos = xpctxt->proximityPosition;
    oldXPSize = xpctxt->contextSize;
    oldXPNsNr = xpctxt->nsNr;
    oldXPNamespaces = xpctxt->namespaces;

    /*
    * Set up contexts.
    */
    ctxt->document = idoc;
    ctxt->node = (xmlNodePtr) idoc->doc;
    ctxt->inst = keyDef->inst;

    xpctxt->doc = idoc->doc;
    xpctxt->node = (xmlNodePtr) idoc->doc;
    /* TODO : clarify the use of namespaces in keys evaluation */
    xpctxt->namespaces = keyDef->nsList;
    xpctxt->nsNr = keyDef->nsNr;

    /*
    * Evaluate the 'match' expression of the xsl:key.
    * TODO: The 'match' is a *pattern*.
    */
    matchRes = xmlXPathCompiledEval(keyDef->comp, xpctxt);
    if (matchRes == NULL) {

#ifdef WITH_XSLT_DEBUG_KEYS
	XSLT_TRACE(ctxt,XSLT_TRACE_KEYS,xsltGenericDebug(xsltGenericDebugContext,
	     "xsltInitCtxtKey: %s evaluation failed\n", keyDef->match));
#endif
	xsltTransformError(ctxt, NULL, keyDef->inst,
	    "Failed to evaluate the 'match' expression.\n");
	ctxt->state = XSLT_STATE_STOPPED;
	goto error;
    } else {
	if (matchRes->type == XPATH_NODESET) {
	    matchList = matchRes->nodesetval;

#ifdef WITH_XSLT_DEBUG_KEYS
	    if (matchList != NULL)
		XSLT_TRACE(ctxt,XSLT_TRACE_KEYS,xsltGenericDebug(xsltGenericDebugContext,
		     "xsltInitCtxtKey: %s evaluates to %d nodes\n",
				 keyDef->match, matchList->nodeNr));
#endif
	} else {
	    /*
	    * Is not a node set, but must be.
	    */
#ifdef WITH_XSLT_DEBUG_KEYS
	    XSLT_TRACE(ctxt,XSLT_TRACE_KEYS,xsltGenericDebug(xsltGenericDebugContext,
		 "xsltInitCtxtKey: %s is not a node set\n", keyDef->match));
#endif
	    xsltTransformError(ctxt, NULL, keyDef->inst,
		"The 'match' expression did not evaluate to a node set.\n");
	    ctxt->state = XSLT_STATE_STOPPED;
	    goto error;
	}
    }
    if ((matchList == NULL) || (matchList->nodeNr <= 0))
	goto exit;

    /**
     * Multiple key definitions for the same name are allowed, so
     * we must check if the key is already present for this doc
     */
    table = (xsltKeyTablePtr) idoc->keys;
    while (table != NULL) {
        if (xmlStrEqual(table->name, keyDef->name) &&
	    (((keyDef->nameURI == NULL) && (table->nameURI == NULL)) ||
	     ((keyDef->nameURI != NULL) && (table->nameURI != NULL) &&
	      (xmlStrEqual(table->nameURI, keyDef->nameURI)))))
	    break;
	table = table->next;
    }
    /**
     * If the key was not previously defined, create it now and
     * chain it to the list of keys for the doc
     */
    if (table == NULL) {
        table = xsltNewKeyTable(keyDef->name, keyDef->nameURI);
        if (table == NULL)
	    goto error;
        table->next = idoc->keys;
        idoc->keys = table;
    }

    /*
    * SPEC XSLT 1.0 (XSLT 2.0 does not clarify the context size!)
    * "...the use attribute of the xsl:key element is evaluated with x as
    "  the current node and with a node list containing just x as the
    *  current node list"
    */
    xpctxt->contextSize = 1;
    xpctxt->proximityPosition = 1;

    for (i = 0; i < matchList->nodeNr; i++) {
	cur = matchList->nodeTab[i];
	if (! IS_XSLT_REAL_NODE(cur))
	    continue;
        ctxt->node = cur;
	xpctxt->node = cur;
	/*
	* Process the 'use' of the xsl:key.
	* SPEC XSLT 1.0:
	* "The use attribute is an expression specifying the values of
	*  the key; the expression is evaluated once for each node that
	*  matches the pattern."
	*/
	if (useRes != NULL)
	    xmlXPathFreeObject(useRes);
	useRes = xmlXPathCompiledEval(keyDef->usecomp, xpctxt);
	if (useRes == NULL) {
	    xsltTransformError(ctxt, NULL, keyDef->inst,
		"Failed to evaluate the 'use' expression.\n");
	    ctxt->state = XSLT_STATE_STOPPED;
	    break;
	}
	if (useRes->type == XPATH_NODESET) {
	    if ((useRes->nodesetval != NULL) &&
		(useRes->nodesetval->nodeNr != 0))
	    {
		len = useRes->nodesetval->nodeNr;
		str = xmlXPathCastNodeToString(useRes->nodesetval->nodeTab[0]);
	    } else {
		continue;
	    }
	} else {
	    len = 1;
	    if (useRes->type == XPATH_STRING) {
		/*
		* Consume the string value.
		*/
		str = useRes->stringval;
		useRes->stringval = NULL;
	    } else {
		str = xmlXPathCastToString(useRes);
	    }
	}
	/*
	* Process all strings.
	*/
	k = 0;
	while (1) {
	    if (str == NULL)
		goto next_string;

#ifdef WITH_XSLT_DEBUG_KEYS
	    XSLT_TRACE(ctxt,XSLT_TRACE_KEYS,xsltGenericDebug(xsltGenericDebugContext,
		"xsl:key : node associated to ('%s', '%s')\n", keyDef->name, str));
#endif

	    keylist = xmlHashLookup(table->keys, str);
	    if (keylist == NULL) {
		keylist = xmlXPathNodeSetCreate(cur);
		if (keylist == NULL)
		    goto error;
		if (xmlHashAddEntry(table->keys, str, keylist) < 0) {
                    xmlXPathFreeNodeSet(keylist);
                    goto error;
                }
	    } else {
		/*
		* TODO: How do we know if this function failed?
		*/
		xmlXPathNodeSetAdd(keylist, cur);
	    }
            xsltSetSourceNodeFlags(ctxt, cur, XSLT_SOURCE_NODE_HAS_KEY);
	    xmlFree(str);
	    str = NULL;

next_string:
	    k++;
	    if (k >= len)
		break;
	    str = xmlXPathCastNodeToString(useRes->nodesetval->nodeTab[k]);
	}
    }

exit:
error:
    ctxt->keyInitLevel--;
    /*
    * Restore context state.
    */
    xpctxt->node = oldXPNode;
    xpctxt->doc = oldXPDoc;
    xpctxt->nsNr = oldXPNsNr;
    xpctxt->namespaces = oldXPNamespaces;
    xpctxt->proximityPosition = oldXPPos;
    xpctxt->contextSize = oldXPSize;

    ctxt->node = oldContextNode;
    ctxt->document = oldDocInfo;
    ctxt->inst = oldInst;

    if (str)
	xmlFree(str);
    if (useRes != NULL)
	xmlXPathFreeObject(useRes);
    if (matchRes != NULL)
	xmlXPathFreeObject(matchRes);
    return(0);
}

/**
 * xsltInitCtxtKeys:
 * @ctxt:  an XSLT transformation context
 * @idoc:  a document info
 *
 * Computes all the keys tables for the current input document.
 * Should be done before global varibales are initialized.
 * NOTE: Not used anymore in the refactored code.
 */
void
xsltInitCtxtKeys(xsltTransformContextPtr ctxt, xsltDocumentPtr idoc) {
    xsltStylesheetPtr style;
    xsltKeyDefPtr keyDef;

    if ((ctxt == NULL) || (idoc == NULL))
	return;

#ifdef KEY_INIT_DEBUG
fprintf(stderr, "xsltInitCtxtKeys on document\n");
#endif

#ifdef WITH_XSLT_DEBUG_KEYS
    if ((idoc->doc != NULL) && (idoc->doc->URL != NULL))
	XSLT_TRACE(ctxt,XSLT_TRACE_KEYS,xsltGenericDebug(xsltGenericDebugContext, "Initializing keys on %s\n",
		     idoc->doc->URL));
#endif
    style = ctxt->style;
    while (style != NULL) {
	keyDef = (xsltKeyDefPtr) style->keys;
	while (keyDef != NULL) {
	    xsltInitCtxtKey(ctxt, idoc, keyDef);

	    keyDef = keyDef->next;
	}

	style = xsltNextImport(style);
    }

#ifdef KEY_INIT_DEBUG
fprintf(stderr, "xsltInitCtxtKeys on document: done\n");
#endif

}

/**
 * xsltFreeDocumentKeys:
 * @idoc: a XSLT document
 *
 * Free the keys associated to a document
 */
void
xsltFreeDocumentKeys(xsltDocumentPtr idoc) {
    if (idoc != NULL)
        xsltFreeKeyTableList(idoc->keys);
}

