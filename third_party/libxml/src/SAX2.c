/*
 * SAX2.c : Default SAX2 handler to build a tree.
 *
 * See Copyright for the status of this software.
 *
 * Daniel Veillard <daniel@veillard.com>
 */


#define IN_LIBXML
#include "libxml.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>
#include <libxml/SAX2.h>
#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/valid.h>
#include <libxml/entities.h>
#include <libxml/xmlerror.h>
#include <libxml/debugXML.h>
#include <libxml/xmlIO.h>
#include <libxml/uri.h>
#include <libxml/valid.h>
#include <libxml/HTMLtree.h>

#include "private/error.h"
#include "private/parser.h"
#include "private/tree.h"

#define XML_MAX_URI_LENGTH 2000

/*
 * xmlSAX2ErrMemory:
 * @ctxt:  an XML validation parser context
 * @msg:   a string to accompany the error message
 */
static void
xmlSAX2ErrMemory(xmlParserCtxtPtr ctxt) {
    xmlCtxtErrMemory(ctxt);
}

/**
 * xmlValidError:
 * @ctxt:  an XML validation parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  extra data
 * @str2:  extra data
 *
 * Handle a validation error
 */
static void LIBXML_ATTR_FORMAT(3,0)
xmlErrValid(xmlParserCtxtPtr ctxt, xmlParserErrors error,
            const char *msg, const xmlChar *str1, const xmlChar *str2)
{
    xmlCtxtErr(ctxt, NULL, XML_FROM_DTD, error, XML_ERR_ERROR,
               str1, str2, NULL, 0, msg, str1, str2);
    if (ctxt != NULL)
	ctxt->valid = 0;
}

/**
 * xmlFatalErrMsg:
 * @ctxt:  an XML parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  an error string
 * @str2:  an error string
 *
 * Handle a fatal parser error, i.e. violating Well-Formedness constraints
 */
static void LIBXML_ATTR_FORMAT(3,0)
xmlFatalErrMsg(xmlParserCtxtPtr ctxt, xmlParserErrors error,
               const char *msg, const xmlChar *str1, const xmlChar *str2)
{
    xmlCtxtErr(ctxt, NULL, XML_FROM_PARSER, error, XML_ERR_FATAL,
               str1, str2, NULL, 0, msg, str1, str2);
}

/**
 * xmlWarnMsg:
 * @ctxt:  an XML parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  an error string
 * @str2:  an error string
 *
 * Handle a parser warning
 */
static void LIBXML_ATTR_FORMAT(3,0)
xmlWarnMsg(xmlParserCtxtPtr ctxt, xmlParserErrors error,
               const char *msg, const xmlChar *str1)
{
    xmlCtxtErr(ctxt, NULL, XML_FROM_PARSER, error, XML_ERR_WARNING,
               str1, NULL, NULL, 0, msg, str1);
}

/**
 * xmlNsWarnMsg:
 * @ctxt:  an XML parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  an error string
 *
 * Handle a namespace warning
 */
static void LIBXML_ATTR_FORMAT(3,0)
xmlNsWarnMsg(xmlParserCtxtPtr ctxt, xmlParserErrors error,
             const char *msg, const xmlChar *str1, const xmlChar *str2)
{
    xmlCtxtErr(ctxt, NULL, XML_FROM_NAMESPACE, error, XML_ERR_WARNING,
               str1, str2, NULL, 0, msg, str1, str2);
}

/**
 * xmlSAX2GetPublicId:
 * @ctx: the user data (XML parser context)
 *
 * Provides the public ID e.g. "-//SGMLSOURCE//DTD DEMO//EN"
 *
 * Returns a xmlChar *
 */
const xmlChar *
xmlSAX2GetPublicId(void *ctx ATTRIBUTE_UNUSED)
{
    /* xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx; */
    return(NULL);
}

/**
 * xmlSAX2GetSystemId:
 * @ctx: the user data (XML parser context)
 *
 * Provides the system ID, basically URL or filename e.g.
 * http://www.sgmlsource.com/dtds/memo.dtd
 *
 * Returns a xmlChar *
 */
const xmlChar *
xmlSAX2GetSystemId(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctx == NULL) || (ctxt->input == NULL)) return(NULL);
    return((const xmlChar *) ctxt->input->filename);
}

/**
 * xmlSAX2GetLineNumber:
 * @ctx: the user data (XML parser context)
 *
 * Provide the line number of the current parsing point.
 *
 * Returns an int
 */
int
xmlSAX2GetLineNumber(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctx == NULL) || (ctxt->input == NULL)) return(0);
    return(ctxt->input->line);
}

/**
 * xmlSAX2GetColumnNumber:
 * @ctx: the user data (XML parser context)
 *
 * Provide the column number of the current parsing point.
 *
 * Returns an int
 */
int
xmlSAX2GetColumnNumber(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctx == NULL) || (ctxt->input == NULL)) return(0);
    return(ctxt->input->col);
}

/**
 * xmlSAX2IsStandalone:
 * @ctx: the user data (XML parser context)
 *
 * Is this document tagged standalone ?
 *
 * Returns 1 if true
 */
int
xmlSAX2IsStandalone(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctx == NULL) || (ctxt->myDoc == NULL)) return(0);
    return(ctxt->myDoc->standalone == 1);
}

/**
 * xmlSAX2HasInternalSubset:
 * @ctx: the user data (XML parser context)
 *
 * Does this document has an internal subset
 *
 * Returns 1 if true
 */
int
xmlSAX2HasInternalSubset(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctxt == NULL) || (ctxt->myDoc == NULL)) return(0);
    return(ctxt->myDoc->intSubset != NULL);
}

/**
 * xmlSAX2HasExternalSubset:
 * @ctx: the user data (XML parser context)
 *
 * Does this document has an external subset
 *
 * Returns 1 if true
 */
int
xmlSAX2HasExternalSubset(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if ((ctxt == NULL) || (ctxt->myDoc == NULL)) return(0);
    return(ctxt->myDoc->extSubset != NULL);
}

/**
 * xmlSAX2InternalSubset:
 * @ctx:  the user data (XML parser context)
 * @name:  the root element name
 * @ExternalID:  the external ID
 * @SystemID:  the SYSTEM ID (e.g. filename or URL)
 *
 * Callback on internal subset declaration.
 */
void
xmlSAX2InternalSubset(void *ctx, const xmlChar *name,
	       const xmlChar *ExternalID, const xmlChar *SystemID)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlDtdPtr dtd;
    if (ctx == NULL) return;

    if (ctxt->myDoc == NULL)
	return;
    dtd = xmlGetIntSubset(ctxt->myDoc);
    if (dtd != NULL) {
	if (ctxt->html)
	    return;
	xmlUnlinkNode((xmlNodePtr) dtd);
	xmlFreeDtd(dtd);
	ctxt->myDoc->intSubset = NULL;
    }
    ctxt->myDoc->intSubset =
	xmlCreateIntSubset(ctxt->myDoc, name, ExternalID, SystemID);
    if (ctxt->myDoc->intSubset == NULL)
        xmlSAX2ErrMemory(ctxt);
}

/**
 * xmlSAX2ExternalSubset:
 * @ctx: the user data (XML parser context)
 * @name:  the root element name
 * @ExternalID:  the external ID
 * @SystemID:  the SYSTEM ID (e.g. filename or URL)
 *
 * Callback on external subset declaration.
 */
void
xmlSAX2ExternalSubset(void *ctx, const xmlChar *name,
	       const xmlChar *ExternalID, const xmlChar *SystemID)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    if (ctx == NULL) return;
    if ((SystemID != NULL) &&
        ((ctxt->options & XML_PARSE_NO_XXE) == 0) &&
        (((ctxt->validate) || (ctxt->loadsubset)) &&
	 (ctxt->wellFormed && ctxt->myDoc))) {
	/*
	 * Try to fetch and parse the external subset.
	 */
	xmlParserInputPtr oldinput;
	int oldinputNr;
	int oldinputMax;
	xmlParserInputPtr *oldinputTab;
	xmlParserInputPtr input = NULL;
	const xmlChar *oldencoding;
        unsigned long consumed;
        size_t buffered;

	/*
	 * Ask the Entity resolver to load the damn thing
	 */
	if ((ctxt->sax != NULL) && (ctxt->sax->resolveEntity != NULL))
	    input = ctxt->sax->resolveEntity(ctxt->userData, ExternalID,
	                                        SystemID);
	if (input == NULL) {
	    return;
	}

	if (xmlNewDtd(ctxt->myDoc, name, ExternalID, SystemID) == NULL) {
            xmlSAX2ErrMemory(ctxt);
            xmlFreeInputStream(input);
            return;
        }

	/*
	 * make sure we won't destroy the main document context
	 */
	oldinput = ctxt->input;
	oldinputNr = ctxt->inputNr;
	oldinputMax = ctxt->inputMax;
	oldinputTab = ctxt->inputTab;
	oldencoding = ctxt->encoding;
	ctxt->encoding = NULL;

	ctxt->inputTab = (xmlParserInputPtr *)
	                 xmlMalloc(5 * sizeof(xmlParserInputPtr));
	if (ctxt->inputTab == NULL) {
	    xmlSAX2ErrMemory(ctxt);
            xmlFreeInputStream(input);
	    ctxt->input = oldinput;
	    ctxt->inputNr = oldinputNr;
	    ctxt->inputMax = oldinputMax;
	    ctxt->inputTab = oldinputTab;
	    ctxt->encoding = oldencoding;
	    return;
	}
	ctxt->inputNr = 0;
	ctxt->inputMax = 5;
	ctxt->input = NULL;
	xmlPushInput(ctxt, input);

	if (input->filename == NULL)
	    input->filename = (char *) xmlCanonicPath(SystemID);
	input->line = 1;
	input->col = 1;
	input->base = ctxt->input->cur;
	input->cur = ctxt->input->cur;
	input->free = NULL;

	/*
	 * let's parse that entity knowing it's an external subset.
	 */
	xmlParseExternalSubset(ctxt, ExternalID, SystemID);

        /*
	 * Free up the external entities
	 */

	while (ctxt->inputNr > 1)
	    xmlPopInput(ctxt);

        consumed = ctxt->input->consumed;
        buffered = ctxt->input->cur - ctxt->input->base;
        if (buffered > ULONG_MAX - consumed)
            consumed = ULONG_MAX;
        else
            consumed += buffered;
        if (consumed > ULONG_MAX - ctxt->sizeentities)
            ctxt->sizeentities = ULONG_MAX;
        else
            ctxt->sizeentities += consumed;

	xmlFreeInputStream(ctxt->input);
        xmlFree(ctxt->inputTab);

	/*
	 * Restore the parsing context of the main entity
	 */
	ctxt->input = oldinput;
	ctxt->inputNr = oldinputNr;
	ctxt->inputMax = oldinputMax;
	ctxt->inputTab = oldinputTab;
	if ((ctxt->encoding != NULL) &&
	    ((ctxt->dict == NULL) ||
	     (!xmlDictOwns(ctxt->dict, ctxt->encoding))))
	    xmlFree((xmlChar *) ctxt->encoding);
	ctxt->encoding = oldencoding;
	/* ctxt->wellFormed = oldwellFormed; */
    }
}

/**
 * xmlSAX2ResolveEntity:
 * @ctx: the user data (XML parser context)
 * @publicId: The public ID of the entity
 * @systemId: The system ID of the entity
 *
 * The entity loader, to control the loading of external entities,
 * the application can either:
 *    - override this xmlSAX2ResolveEntity() callback in the SAX block
 *    - or better use the xmlSetExternalEntityLoader() function to
 *      set up it's own entity resolution routine
 *
 * Returns the xmlParserInputPtr if inlined or NULL for DOM behaviour.
 */
xmlParserInputPtr
xmlSAX2ResolveEntity(void *ctx, const xmlChar *publicId, const xmlChar *systemId)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlParserInputPtr ret = NULL;
    xmlChar *URI;
    const xmlChar *base = NULL;
    int res;

    if (ctx == NULL) return(NULL);
    if (ctxt->input != NULL)
	base = BAD_CAST ctxt->input->filename;

    if ((xmlStrlen(systemId) > XML_MAX_URI_LENGTH) ||
        (xmlStrlen(base) > XML_MAX_URI_LENGTH)) {
        xmlFatalErr(ctxt, XML_ERR_RESOURCE_LIMIT, "URI too long");
        return(NULL);
    }
    res = xmlBuildURISafe(systemId, base, &URI);
    if (URI == NULL) {
        if (res < 0)
            xmlSAX2ErrMemory(ctxt);
        else
            xmlWarnMsg(ctxt, XML_ERR_INVALID_URI,
                       "Can't resolve URI: %s\n", systemId);
        return(NULL);
    }
    if (xmlStrlen(URI) > XML_MAX_URI_LENGTH) {
        xmlFatalErr(ctxt, XML_ERR_RESOURCE_LIMIT, "URI too long");
    } else {
        ret = xmlLoadExternalEntity((const char *) URI,
                                    (const char *) publicId, ctxt);
    }

    xmlFree(URI);
    return(ret);
}

/**
 * xmlSAX2GetEntity:
 * @ctx: the user data (XML parser context)
 * @name: The entity name
 *
 * Get an entity by name
 *
 * Returns the xmlEntityPtr if found.
 */
xmlEntityPtr
xmlSAX2GetEntity(void *ctx, const xmlChar *name)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlEntityPtr ret = NULL;

    if (ctx == NULL) return(NULL);

    if (ctxt->inSubset == 0) {
	ret = xmlGetPredefinedEntity(name);
	if (ret != NULL)
	    return(ret);
    }
    if ((ctxt->myDoc != NULL) && (ctxt->myDoc->standalone == 1)) {
	if (ctxt->inSubset == 2) {
	    ctxt->myDoc->standalone = 0;
	    ret = xmlGetDocEntity(ctxt->myDoc, name);
	    ctxt->myDoc->standalone = 1;
	} else {
	    ret = xmlGetDocEntity(ctxt->myDoc, name);
	    if (ret == NULL) {
		ctxt->myDoc->standalone = 0;
		ret = xmlGetDocEntity(ctxt->myDoc, name);
		if (ret != NULL) {
		    xmlFatalErrMsg(ctxt, XML_ERR_NOT_STANDALONE,
	 "Entity(%s) document marked standalone but requires external subset\n",
				   name, NULL);
		}
		ctxt->myDoc->standalone = 1;
	    }
	}
    } else {
	ret = xmlGetDocEntity(ctxt->myDoc, name);
    }
    return(ret);
}

/**
 * xmlSAX2GetParameterEntity:
 * @ctx: the user data (XML parser context)
 * @name: The entity name
 *
 * Get a parameter entity by name
 *
 * Returns the xmlEntityPtr if found.
 */
xmlEntityPtr
xmlSAX2GetParameterEntity(void *ctx, const xmlChar *name)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlEntityPtr ret;

    if (ctx == NULL) return(NULL);

    ret = xmlGetParameterEntity(ctxt->myDoc, name);
    return(ret);
}


/**
 * xmlSAX2EntityDecl:
 * @ctx: the user data (XML parser context)
 * @name:  the entity name
 * @type:  the entity type
 * @publicId: The public ID of the entity
 * @systemId: The system ID of the entity
 * @content: the entity value (without processing).
 *
 * An entity definition has been parsed
 */
void
xmlSAX2EntityDecl(void *ctx, const xmlChar *name, int type,
          const xmlChar *publicId, const xmlChar *systemId, xmlChar *content)
{
    xmlEntityPtr ent;
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    int extSubset;
    int res;

    if ((ctxt == NULL) || (ctxt->myDoc == NULL))
        return;

    extSubset = ctxt->inSubset == 2;
    res = xmlAddEntity(ctxt->myDoc, extSubset, name, type, publicId, systemId,
                       content, &ent);
    switch (res) {
        case XML_ERR_OK:
            break;
        case XML_ERR_NO_MEMORY:
            xmlSAX2ErrMemory(ctxt);
            return;
        case XML_WAR_ENTITY_REDEFINED:
            if (ctxt->pedantic) {
                if (extSubset)
                    xmlWarnMsg(ctxt, res, "Entity(%s) already defined in the"
                               " external subset\n", name);
                else
                    xmlWarnMsg(ctxt, res, "Entity(%s) already defined in the"
                               " internal subset\n", name);
            }
            return;
        case XML_ERR_REDECL_PREDEF_ENTITY:
            /*
             * Technically an error but it's a common mistake to get double
             * escaping according to "4.6 Predefined Entities" wrong.
             */
            xmlWarnMsg(ctxt, res, "Invalid redeclaration of predefined"
                       " entity '%s'", name);
            return;
        default:
            xmlFatalErrMsg(ctxt, XML_ERR_INTERNAL_ERROR,
                           "Unexpected error code from xmlAddEntity\n",
                           NULL, NULL);
            return;
    }

    if ((ent->URI == NULL) && (systemId != NULL)) {
        xmlChar *URI;
        const char *base = NULL;
        int i;

        for (i = ctxt->inputNr - 1; i >= 0; i--) {
            if (ctxt->inputTab[i]->filename != NULL) {
                base = ctxt->inputTab[i]->filename;
                break;
            }
        }

        res = xmlBuildURISafe(systemId, (const xmlChar *) base, &URI);

        if (URI == NULL) {
            if (res < 0) {
                xmlSAX2ErrMemory(ctxt);
            } else {
                xmlWarnMsg(ctxt, XML_ERR_INVALID_URI,
                           "Can't resolve URI: %s\n", systemId);
            }
        } else if (xmlStrlen(URI) > XML_MAX_URI_LENGTH) {
            xmlFatalErr(ctxt, XML_ERR_RESOURCE_LIMIT, "URI too long");
            xmlFree(URI);
        } else {
            ent->URI = URI;
        }
    }
}

/**
 * xmlSAX2AttributeDecl:
 * @ctx: the user data (XML parser context)
 * @elem:  the name of the element
 * @fullname:  the attribute name
 * @type:  the attribute type
 * @def:  the type of default value
 * @defaultValue: the attribute default value
 * @tree:  the tree of enumerated value set
 *
 * An attribute definition has been parsed
 */
void
xmlSAX2AttributeDecl(void *ctx, const xmlChar *elem, const xmlChar *fullname,
              int type, int def, const xmlChar *defaultValue,
	      xmlEnumerationPtr tree)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlAttributePtr attr;
    xmlChar *name = NULL, *prefix = NULL;

    /* Avoid unused variable warning if features are disabled. */
    (void) attr;

    if ((ctxt == NULL) || (ctxt->myDoc == NULL))
        return;

    if ((xmlStrEqual(fullname, BAD_CAST "xml:id")) &&
        (type != XML_ATTRIBUTE_ID)) {
	/*
	 * Raise the error but keep the validity flag
	 */
	int tmp = ctxt->valid;
	xmlErrValid(ctxt, XML_DTD_XMLID_TYPE,
	      "xml:id : attribute type should be ID\n", NULL, NULL);
	ctxt->valid = tmp;
    }
    /* TODO: optimize name/prefix allocation */
    name = xmlSplitQName(ctxt, fullname, &prefix);
    if (name == NULL)
        xmlSAX2ErrMemory(ctxt);
    ctxt->vctxt.valid = 1;
    if (ctxt->inSubset == 1)
	attr = xmlAddAttributeDecl(&ctxt->vctxt, ctxt->myDoc->intSubset, elem,
	       name, prefix, (xmlAttributeType) type,
	       (xmlAttributeDefault) def, defaultValue, tree);
    else if (ctxt->inSubset == 2)
	attr = xmlAddAttributeDecl(&ctxt->vctxt, ctxt->myDoc->extSubset, elem,
	   name, prefix, (xmlAttributeType) type,
	   (xmlAttributeDefault) def, defaultValue, tree);
    else {
        xmlFatalErrMsg(ctxt, XML_ERR_INTERNAL_ERROR,
	     "SAX.xmlSAX2AttributeDecl(%s) called while not in subset\n",
	               name, NULL);
	xmlFree(name);
	xmlFree(prefix);
	xmlFreeEnumeration(tree);
	return;
    }
#ifdef LIBXML_VALID_ENABLED
    if (ctxt->vctxt.valid == 0)
	ctxt->valid = 0;
    if ((attr != NULL) && (ctxt->validate) && (ctxt->wellFormed) &&
        (ctxt->myDoc->intSubset != NULL))
	ctxt->valid &= xmlValidateAttributeDecl(&ctxt->vctxt, ctxt->myDoc,
	                                        attr);
#endif /* LIBXML_VALID_ENABLED */
    if (prefix != NULL)
	xmlFree(prefix);
    if (name != NULL)
	xmlFree(name);
}

/**
 * xmlSAX2ElementDecl:
 * @ctx: the user data (XML parser context)
 * @name:  the element name
 * @type:  the element type
 * @content: the element value tree
 *
 * An element definition has been parsed
 */
void
xmlSAX2ElementDecl(void *ctx, const xmlChar * name, int type,
            xmlElementContentPtr content)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlElementPtr elem = NULL;

    /* Avoid unused variable warning if features are disabled. */
    (void) elem;

    if ((ctxt == NULL) || (ctxt->myDoc == NULL))
        return;

    if (ctxt->inSubset == 1)
        elem = xmlAddElementDecl(&ctxt->vctxt, ctxt->myDoc->intSubset,
                                 name, (xmlElementTypeVal) type, content);
    else if (ctxt->inSubset == 2)
        elem = xmlAddElementDecl(&ctxt->vctxt, ctxt->myDoc->extSubset,
                                 name, (xmlElementTypeVal) type, content);
    else {
        xmlFatalErrMsg(ctxt, XML_ERR_INTERNAL_ERROR,
	     "SAX.xmlSAX2ElementDecl(%s) called while not in subset\n",
	               name, NULL);
        return;
    }
#ifdef LIBXML_VALID_ENABLED
    if (elem == NULL)
        ctxt->valid = 0;
    if (ctxt->validate && ctxt->wellFormed &&
        ctxt->myDoc && ctxt->myDoc->intSubset)
        ctxt->valid &=
            xmlValidateElementDecl(&ctxt->vctxt, ctxt->myDoc, elem);
#endif /* LIBXML_VALID_ENABLED */
}

/**
 * xmlSAX2NotationDecl:
 * @ctx: the user data (XML parser context)
 * @name: The name of the notation
 * @publicId: The public ID of the entity
 * @systemId: The system ID of the entity
 *
 * What to do when a notation declaration has been parsed.
 */
void
xmlSAX2NotationDecl(void *ctx, const xmlChar *name,
	     const xmlChar *publicId, const xmlChar *systemId)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNotationPtr nota = NULL;

    /* Avoid unused variable warning if features are disabled. */
    (void) nota;

    if ((ctxt == NULL) || (ctxt->myDoc == NULL))
        return;

    if ((publicId == NULL) && (systemId == NULL)) {
	xmlFatalErrMsg(ctxt, XML_ERR_NOTATION_PROCESSING,
	     "SAX.xmlSAX2NotationDecl(%s) externalID or PublicID missing\n",
	               name, NULL);
	return;
    } else if (ctxt->inSubset == 1)
	nota = xmlAddNotationDecl(&ctxt->vctxt, ctxt->myDoc->intSubset, name,
                              publicId, systemId);
    else if (ctxt->inSubset == 2)
	nota = xmlAddNotationDecl(&ctxt->vctxt, ctxt->myDoc->extSubset, name,
                              publicId, systemId);
    else {
	xmlFatalErrMsg(ctxt, XML_ERR_NOTATION_PROCESSING,
	     "SAX.xmlSAX2NotationDecl(%s) called while not in subset\n",
	               name, NULL);
	return;
    }
#ifdef LIBXML_VALID_ENABLED
    if (nota == NULL) ctxt->valid = 0;
    if ((ctxt->validate) && (ctxt->wellFormed) &&
        (ctxt->myDoc->intSubset != NULL))
	ctxt->valid &= xmlValidateNotationDecl(&ctxt->vctxt, ctxt->myDoc,
	                                       nota);
#endif /* LIBXML_VALID_ENABLED */
}

/**
 * xmlSAX2UnparsedEntityDecl:
 * @ctx: the user data (XML parser context)
 * @name: The name of the entity
 * @publicId: The public ID of the entity
 * @systemId: The system ID of the entity
 * @notationName: the name of the notation
 *
 * What to do when an unparsed entity declaration is parsed
 */
void
xmlSAX2UnparsedEntityDecl(void *ctx, const xmlChar *name,
		   const xmlChar *publicId, const xmlChar *systemId,
		   const xmlChar *notationName)
{
    xmlSAX2EntityDecl(ctx, name, XML_EXTERNAL_GENERAL_UNPARSED_ENTITY,
                      publicId, systemId, (xmlChar *) notationName);
}

/**
 * xmlSAX2SetDocumentLocator:
 * @ctx: the user data (XML parser context)
 * @loc: A SAX Locator
 *
 * Receive the document locator at startup, actually xmlDefaultSAXLocator
 * Everything is available on the context, so this is useless in our case.
 */
void
xmlSAX2SetDocumentLocator(void *ctx ATTRIBUTE_UNUSED, xmlSAXLocatorPtr loc ATTRIBUTE_UNUSED)
{
}

/**
 * xmlSAX2StartDocument:
 * @ctx: the user data (XML parser context)
 *
 * called when the document start being processed.
 */
void
xmlSAX2StartDocument(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlDocPtr doc;

    if (ctx == NULL) return;

#ifdef LIBXML_HTML_ENABLED
    if (ctxt->html) {
	if (ctxt->myDoc == NULL)
	    ctxt->myDoc = htmlNewDocNoDtD(NULL, NULL);
	if (ctxt->myDoc == NULL) {
	    xmlSAX2ErrMemory(ctxt);
	    return;
	}
	ctxt->myDoc->properties = XML_DOC_HTML;
	ctxt->myDoc->parseFlags = ctxt->options;
    } else
#endif
    {
	doc = ctxt->myDoc = xmlNewDoc(ctxt->version);
	if (doc != NULL) {
	    doc->properties = 0;
	    if (ctxt->options & XML_PARSE_OLD10)
	        doc->properties |= XML_DOC_OLD10;
	    doc->parseFlags = ctxt->options;
	    doc->standalone = ctxt->standalone;
	} else {
	    xmlSAX2ErrMemory(ctxt);
	    return;
	}
	if ((ctxt->dictNames) && (doc != NULL)) {
	    doc->dict = ctxt->dict;
	    xmlDictReference(doc->dict);
	}
    }
    if ((ctxt->myDoc != NULL) && (ctxt->myDoc->URL == NULL) &&
	(ctxt->input != NULL) && (ctxt->input->filename != NULL)) {
	ctxt->myDoc->URL = xmlPathToURI((const xmlChar *)ctxt->input->filename);
	if (ctxt->myDoc->URL == NULL)
	    xmlSAX2ErrMemory(ctxt);
    }
}

/**
 * xmlSAX2EndDocument:
 * @ctx: the user data (XML parser context)
 *
 * called when the document end has been detected.
 */
void
xmlSAX2EndDocument(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlDocPtr doc;

    if (ctx == NULL) return;
#ifdef LIBXML_VALID_ENABLED
    if (ctxt->validate && ctxt->wellFormed &&
        ctxt->myDoc && ctxt->myDoc->intSubset)
	ctxt->valid &= xmlValidateDocumentFinal(&ctxt->vctxt, ctxt->myDoc);
#endif /* LIBXML_VALID_ENABLED */

    doc = ctxt->myDoc;
    if ((doc != NULL) && (doc->encoding == NULL)) {
        const xmlChar *encoding = xmlGetActualEncoding(ctxt);

        if (encoding != NULL) {
            doc->encoding = xmlStrdup(encoding);
            if (doc->encoding == NULL)
                xmlSAX2ErrMemory(ctxt);
        }
    }
}

static void
xmlSAX2AppendChild(xmlParserCtxtPtr ctxt, xmlNodePtr node) {
    xmlNodePtr parent;
    xmlNodePtr last;

    if (ctxt->inSubset == 1) {
	parent = (xmlNodePtr) ctxt->myDoc->intSubset;
    } else if (ctxt->inSubset == 2) {
	parent = (xmlNodePtr) ctxt->myDoc->extSubset;
    } else {
        parent = ctxt->node;
        if (parent == NULL)
            parent = (xmlNodePtr) ctxt->myDoc;
    }

    last = parent->last;
    if (last == NULL) {
        parent->children = node;
    } else {
        last->next = node;
        node->prev = last;
    }

    parent->last = node;
    node->parent = parent;

    if ((node->type != XML_TEXT_NODE) &&
        (ctxt->linenumbers) &&
	(ctxt->input != NULL)) {
        if ((unsigned) ctxt->input->line < (unsigned) USHRT_MAX)
            node->line = ctxt->input->line;
        else
            node->line = USHRT_MAX;
    }
}

#if defined(LIBXML_SAX1_ENABLED) || defined(LIBXML_HTML_ENABLED) || defined(LIBXML_WRITER_ENABLED) || defined(LIBXML_LEGACY_ENABLED)
/**
 * xmlNsErrMsg:
 * @ctxt:  an XML parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  an error string
 * @str2:  an error string
 *
 * Handle a namespace error
 */
static void LIBXML_ATTR_FORMAT(3,0)
xmlNsErrMsg(xmlParserCtxtPtr ctxt, xmlParserErrors error,
            const char *msg, const xmlChar *str1, const xmlChar *str2)
{
    xmlCtxtErr(ctxt, NULL, XML_FROM_NAMESPACE, error, XML_ERR_ERROR,
               str1, str2, NULL, 0, msg, str1, str2);
}

/**
 * xmlSAX2AttributeInternal:
 * @ctx: the user data (XML parser context)
 * @fullname:  The attribute name, including namespace prefix
 * @value:  The attribute value
 * @prefix: the prefix on the element node
 *
 * Handle an attribute that has been read by the parser.
 * The default handling is to convert the attribute into an
 * DOM subtree and past it in a new xmlAttr element added to
 * the element.
 */
static void
xmlSAX2AttributeInternal(void *ctx, const xmlChar *fullname,
             const xmlChar *value, const xmlChar *prefix ATTRIBUTE_UNUSED)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlAttrPtr ret;
    xmlChar *name;
    xmlChar *ns;
    xmlChar *nval;
    xmlNsPtr namespace;

    if (ctxt->html) {
	name = xmlStrdup(fullname);
	ns = NULL;
	namespace = NULL;
    } else {
	/*
	 * Split the full name into a namespace prefix and the tag name
	 */
	name = xmlSplitQName(ctxt, fullname, &ns);
	if ((name != NULL) && (name[0] == 0)) {
	    if (xmlStrEqual(ns, BAD_CAST "xmlns")) {
		xmlNsErrMsg(ctxt, XML_ERR_NS_DECL_ERROR,
			    "invalid namespace declaration '%s'\n",
			    fullname, NULL);
	    } else {
		xmlNsWarnMsg(ctxt, XML_WAR_NS_COLUMN,
			     "Avoid attribute ending with ':' like '%s'\n",
			     fullname, NULL);
	    }
	    if (ns != NULL)
		xmlFree(ns);
	    ns = NULL;
	    xmlFree(name);
	    name = xmlStrdup(fullname);
	}
    }
    if (name == NULL) {
        xmlSAX2ErrMemory(ctxt);
	if (ns != NULL)
	    xmlFree(ns);
	return;
    }

#ifdef LIBXML_HTML_ENABLED
    if ((ctxt->html) &&
        (value == NULL) && (htmlIsBooleanAttr(fullname))) {
            nval = xmlStrdup(fullname);
            if (nval == NULL)
                xmlSAX2ErrMemory(ctxt);
            value = (const xmlChar *) nval;
    } else
#endif
    {
#ifdef LIBXML_VALID_ENABLED
        /*
         * Do the last stage of the attribute normalization
         * Needed for HTML too:
         *   http://www.w3.org/TR/html4/types.html#h-6.2
         */
        ctxt->vctxt.valid = 1;
        nval = xmlValidCtxtNormalizeAttributeValue(&ctxt->vctxt,
                                               ctxt->myDoc, ctxt->node,
                                               fullname, value);
        if (ctxt->vctxt.valid != 1) {
            ctxt->valid = 0;
        }
        if (nval != NULL)
            value = nval;
#else
        nval = NULL;
#endif /* LIBXML_VALID_ENABLED */
    }

    /*
     * Check whether it's a namespace definition
     */
    if ((!ctxt->html) && (ns == NULL) &&
        (name[0] == 'x') && (name[1] == 'm') && (name[2] == 'l') &&
        (name[3] == 'n') && (name[4] == 's') && (name[5] == 0)) {
	xmlNsPtr nsret;
	xmlChar *val;

        /* Avoid unused variable warning if features are disabled. */
        (void) nsret;

        if (!ctxt->replaceEntities) {
            /* TODO: normalize if needed */
	    val = xmlExpandEntitiesInAttValue(ctxt, value, /* normalize */ 0);
	    if (val == NULL) {
	        xmlSAX2ErrMemory(ctxt);
		if (name != NULL)
		    xmlFree(name);
                if (nval != NULL)
                    xmlFree(nval);
		return;
	    }
	} else {
	    val = (xmlChar *) value;
	}

	if (val[0] != 0) {
	    xmlURIPtr uri;

	    if (xmlParseURISafe((const char *)val, &uri) < 0)
                xmlSAX2ErrMemory(ctxt);
	    if (uri == NULL) {
                xmlNsWarnMsg(ctxt, XML_WAR_NS_URI,
                             "xmlns:%s: %s not a valid URI\n", name, value);
	    } else {
		if (uri->scheme == NULL) {
                    xmlNsWarnMsg(ctxt, XML_WAR_NS_URI_RELATIVE,
                                 "xmlns:%s: URI %s is not absolute\n",
                                 name, value);
		}
		xmlFreeURI(uri);
	    }
	}

	/* a default namespace definition */
	nsret = xmlNewNs(ctxt->node, val, NULL);
        if (nsret == NULL) {
            xmlSAX2ErrMemory(ctxt);
        }
#ifdef LIBXML_VALID_ENABLED
	/*
	 * Validate also for namespace decls, they are attributes from
	 * an XML-1.0 perspective
	 */
        else if (ctxt->validate && ctxt->wellFormed &&
                 ctxt->myDoc && ctxt->myDoc->intSubset) {
	    ctxt->valid &= xmlValidateOneNamespace(&ctxt->vctxt, ctxt->myDoc,
					   ctxt->node, prefix, nsret, val);
        }
#endif /* LIBXML_VALID_ENABLED */
	if (name != NULL)
	    xmlFree(name);
	if (nval != NULL)
	    xmlFree(nval);
	if (val != value)
	    xmlFree(val);
	return;
    }
    if ((!ctxt->html) &&
	(ns != NULL) && (ns[0] == 'x') && (ns[1] == 'm') && (ns[2] == 'l') &&
        (ns[3] == 'n') && (ns[4] == 's') && (ns[5] == 0)) {
	xmlNsPtr nsret;
	xmlChar *val;

        /* Avoid unused variable warning if features are disabled. */
        (void) nsret;

        if (!ctxt->replaceEntities) {
            /* TODO: normalize if needed */
	    val = xmlExpandEntitiesInAttValue(ctxt, value, /* normalize */ 0);
	    if (val == NULL) {
	        xmlSAX2ErrMemory(ctxt);
	        xmlFree(ns);
		if (name != NULL)
		    xmlFree(name);
                if (nval != NULL)
                    xmlFree(nval);
		return;
	    }
	} else {
	    val = (xmlChar *) value;
	}

	if (val[0] == 0) {
	    xmlNsErrMsg(ctxt, XML_NS_ERR_EMPTY,
		        "Empty namespace name for prefix %s\n", name, NULL);
	}
	if ((ctxt->pedantic != 0) && (val[0] != 0)) {
	    xmlURIPtr uri;

	    if (xmlParseURISafe((const char *)val, &uri) < 0)
                xmlSAX2ErrMemory(ctxt);
	    if (uri == NULL) {
	        xmlNsWarnMsg(ctxt, XML_WAR_NS_URI,
			 "xmlns:%s: %s not a valid URI\n", name, value);
	    } else {
		if (uri->scheme == NULL) {
		    xmlNsWarnMsg(ctxt, XML_WAR_NS_URI_RELATIVE,
			   "xmlns:%s: URI %s is not absolute\n", name, value);
		}
		xmlFreeURI(uri);
	    }
	}

	/* a standard namespace definition */
	nsret = xmlNewNs(ctxt->node, val, name);
	xmlFree(ns);

        if (nsret == NULL) {
            xmlSAX2ErrMemory(ctxt);
        }
#ifdef LIBXML_VALID_ENABLED
	/*
	 * Validate also for namespace decls, they are attributes from
	 * an XML-1.0 perspective
	 */
        else if (ctxt->validate && ctxt->wellFormed &&
	         ctxt->myDoc && ctxt->myDoc->intSubset) {
	    ctxt->valid &= xmlValidateOneNamespace(&ctxt->vctxt, ctxt->myDoc,
					   ctxt->node, prefix, nsret, value);
        }
#endif /* LIBXML_VALID_ENABLED */
	if (name != NULL)
	    xmlFree(name);
	if (nval != NULL)
	    xmlFree(nval);
	if (val != value)
	    xmlFree(val);
	return;
    }

    if (ns != NULL) {
        int res;

	res = xmlSearchNsSafe(ctxt->node, ns, &namespace);
        if (res < 0)
            xmlSAX2ErrMemory(ctxt);

	if (namespace == NULL) {
	    xmlNsErrMsg(ctxt, XML_NS_ERR_UNDEFINED_NAMESPACE,
		    "Namespace prefix %s of attribute %s is not defined\n",
		             ns, name);
	} else {
            xmlAttrPtr prop;

            prop = ctxt->node->properties;
            while (prop != NULL) {
                if (prop->ns != NULL) {
                    if ((xmlStrEqual(name, prop->name)) &&
                        ((namespace == prop->ns) ||
                         (xmlStrEqual(namespace->href, prop->ns->href)))) {
                        xmlCtxtErr(ctxt, NULL, XML_FROM_PARSER,
                                   XML_ERR_ATTRIBUTE_REDEFINED, XML_ERR_FATAL,
                                   name, NULL, NULL, 0,
                                   "Attribute %s in %s redefined\n",
                                   name, namespace->href);
                        if (name != NULL)
                            xmlFree(name);
                        goto error;
                    }
                }
                prop = prop->next;
            }
        }
    } else {
	namespace = NULL;
    }

    /* !!!!!! <a toto:arg="" xmlns:toto="http://toto.com"> */
    ret = xmlNewNsPropEatName(ctxt->node, namespace, name, NULL);
    if (ret == NULL) {
        xmlSAX2ErrMemory(ctxt);
        goto error;
    }

    if ((ctxt->replaceEntities == 0) && (!ctxt->html)) {
        if (xmlNodeParseContent((xmlNodePtr) ret, value, INT_MAX) < 0)
            xmlSAX2ErrMemory(ctxt);
    } else if (value != NULL) {
        ret->children = xmlNewDocText(ctxt->myDoc, value);
        if (ret->children == NULL) {
            xmlSAX2ErrMemory(ctxt);
        } else {
            ret->last = ret->children;
            ret->children->parent = (xmlNodePtr) ret;
        }
    }

#ifdef LIBXML_VALID_ENABLED
    if ((!ctxt->html) && ctxt->validate && ctxt->wellFormed &&
        ctxt->myDoc && ctxt->myDoc->intSubset) {

	/*
	 * If we don't substitute entities, the validation should be
	 * done on a value with replaced entities anyway.
	 */
        if (!ctxt->replaceEntities) {
	    xmlChar *val;

            /* TODO: normalize if needed */
	    val = xmlExpandEntitiesInAttValue(ctxt, value, /* normalize */ 0);

	    if (val == NULL)
		ctxt->valid &= xmlValidateOneAttribute(&ctxt->vctxt,
				ctxt->myDoc, ctxt->node, ret, value);
	    else {
		xmlChar *nvalnorm;

		/*
		 * Do the last stage of the attribute normalization
		 * It need to be done twice ... it's an extra burden related
		 * to the ability to keep xmlSAX2References in attributes
		 */
                nvalnorm = xmlValidCtxtNormalizeAttributeValue(
                                 &ctxt->vctxt, ctxt->myDoc,
                                 ctxt->node, fullname, val);
		if (nvalnorm != NULL) {
		    xmlFree(val);
		    val = nvalnorm;
		}

		ctxt->valid &= xmlValidateOneAttribute(&ctxt->vctxt,
			        ctxt->myDoc, ctxt->node, ret, val);
                xmlFree(val);
	    }
	} else {
	    ctxt->valid &= xmlValidateOneAttribute(&ctxt->vctxt, ctxt->myDoc,
					       ctxt->node, ret, value);
	}
    } else
#endif /* LIBXML_VALID_ENABLED */
           if (((ctxt->loadsubset & XML_SKIP_IDS) == 0) &&
               /* Don't create IDs containing entity references */
               (ret->children != NULL) &&
               (ret->children->type == XML_TEXT_NODE) &&
               (ret->children->next == NULL)) {
        xmlChar *content = ret->children->content;
        /*
	 * when validating, the ID registration is done at the attribute
	 * validation level. Otherwise we have to do specific handling here.
	 */
	if (xmlStrEqual(fullname, BAD_CAST "xml:id")) {
	    /*
	     * Add the xml:id value
	     *
	     * Open issue: normalization of the value.
	     */
	    if (xmlValidateNCName(content, 1) != 0) {
	        xmlErrValid(ctxt, XML_DTD_XMLID_VALUE,
		            "xml:id : attribute value %s is not an NCName\n",
		            content, NULL);
	    }
	    xmlAddID(&ctxt->vctxt, ctxt->myDoc, content, ret);
	} else {
            int res = xmlIsID(ctxt->myDoc, ctxt->node, ret);

            if (res < 0)
                xmlCtxtErrMemory(ctxt);
            else if (res > 0)
                xmlAddID(&ctxt->vctxt, ctxt->myDoc, content, ret);
            else if (xmlIsRef(ctxt->myDoc, ctxt->node, ret))
                xmlAddRef(&ctxt->vctxt, ctxt->myDoc, content, ret);
        }
    }

error:
    if (nval != NULL)
	xmlFree(nval);
    if (ns != NULL)
	xmlFree(ns);
}

/*
 * xmlCheckDefaultedAttributes:
 *
 * Check defaulted attributes from the DTD
 */
static void
xmlCheckDefaultedAttributes(xmlParserCtxtPtr ctxt, const xmlChar *name,
	const xmlChar *prefix, const xmlChar **atts) {
    xmlElementPtr elemDecl;
    const xmlChar *att;
    int internal = 1;
    int i;

    elemDecl = xmlGetDtdQElementDesc(ctxt->myDoc->intSubset, name, prefix);
    if (elemDecl == NULL) {
	elemDecl = xmlGetDtdQElementDesc(ctxt->myDoc->extSubset, name, prefix);
	internal = 0;
    }

process_external_subset:

    if (elemDecl != NULL) {
	xmlAttributePtr attr = elemDecl->attributes;
	/*
	 * Check against defaulted attributes from the external subset
	 * if the document is stamped as standalone
	 */
	if ((ctxt->myDoc->standalone == 1) &&
	    (ctxt->myDoc->extSubset != NULL) &&
	    (ctxt->validate)) {
	    while (attr != NULL) {
		if ((attr->defaultValue != NULL) &&
		    (xmlGetDtdQAttrDesc(ctxt->myDoc->extSubset,
					attr->elem, attr->name,
					attr->prefix) == attr) &&
		    (xmlGetDtdQAttrDesc(ctxt->myDoc->intSubset,
					attr->elem, attr->name,
					attr->prefix) == NULL)) {
		    xmlChar *fulln;

		    if (attr->prefix != NULL) {
			fulln = xmlStrdup(attr->prefix);
                        if (fulln != NULL)
			    fulln = xmlStrcat(fulln, BAD_CAST ":");
                        if (fulln != NULL)
			    fulln = xmlStrcat(fulln, attr->name);
		    } else {
			fulln = xmlStrdup(attr->name);
		    }
                    if (fulln == NULL) {
                        xmlSAX2ErrMemory(ctxt);
                        break;
                    }

		    /*
		     * Check that the attribute is not declared in the
		     * serialization
		     */
		    att = NULL;
		    if (atts != NULL) {
			i = 0;
			att = atts[i];
			while (att != NULL) {
			    if (xmlStrEqual(att, fulln))
				break;
			    i += 2;
			    att = atts[i];
			}
		    }
		    if (att == NULL) {
		        xmlErrValid(ctxt, XML_DTD_STANDALONE_DEFAULTED,
      "standalone: attribute %s on %s defaulted from external subset\n",
				    fulln,
				    attr->elem);
		    }
                    xmlFree(fulln);
		}
		attr = attr->nexth;
	    }
	}

	/*
	 * Actually insert defaulted values when needed
	 */
	attr = elemDecl->attributes;
	while (attr != NULL) {
	    /*
	     * Make sure that attributes redefinition occurring in the
	     * internal subset are not overridden by definitions in the
	     * external subset.
	     */
	    if (attr->defaultValue != NULL) {
		/*
		 * the element should be instantiated in the tree if:
		 *  - this is a namespace prefix
		 *  - the user required for completion in the tree
		 *    like XSLT
		 *  - there isn't already an attribute definition
		 *    in the internal subset overriding it.
		 */
		if (((attr->prefix != NULL) &&
		     (xmlStrEqual(attr->prefix, BAD_CAST "xmlns"))) ||
		    ((attr->prefix == NULL) &&
		     (xmlStrEqual(attr->name, BAD_CAST "xmlns"))) ||
		    (ctxt->loadsubset & XML_COMPLETE_ATTRS)) {
		    xmlAttributePtr tst;

		    tst = xmlGetDtdQAttrDesc(ctxt->myDoc->intSubset,
					     attr->elem, attr->name,
					     attr->prefix);
		    if ((tst == attr) || (tst == NULL)) {
		        xmlChar fn[50];
			xmlChar *fulln;

                        fulln = xmlBuildQName(attr->name, attr->prefix, fn, 50);
			if (fulln == NULL) {
			    xmlSAX2ErrMemory(ctxt);
			    return;
			}

			/*
			 * Check that the attribute is not declared in the
			 * serialization
			 */
			att = NULL;
			if (atts != NULL) {
			    i = 0;
			    att = atts[i];
			    while (att != NULL) {
				if (xmlStrEqual(att, fulln))
				    break;
				i += 2;
				att = atts[i];
			    }
			}
			if (att == NULL) {
			    xmlSAX2AttributeInternal(ctxt, fulln,
						 attr->defaultValue, prefix);
			}
			if ((fulln != fn) && (fulln != attr->name))
			    xmlFree(fulln);
		    }
		}
	    }
	    attr = attr->nexth;
	}
	if (internal == 1) {
	    elemDecl = xmlGetDtdQElementDesc(ctxt->myDoc->extSubset,
		                             name, prefix);
	    internal = 0;
	    goto process_external_subset;
	}
    }
}

/**
 * xmlSAX2StartElement:
 * @ctx: the user data (XML parser context)
 * @fullname:  The element name, including namespace prefix
 * @atts:  An array of name/value attributes pairs, NULL terminated
 *
 * called when an opening tag has been processed.
 */
void
xmlSAX2StartElement(void *ctx, const xmlChar *fullname, const xmlChar **atts)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr ret;
    xmlNodePtr parent;
    xmlNsPtr ns;
    xmlChar *name;
    xmlChar *prefix;
    const xmlChar *att;
    const xmlChar *value;
    int i;

    if ((ctx == NULL) || (fullname == NULL) || (ctxt->myDoc == NULL)) return;

    /*
     * First check on validity:
     */
    if (ctxt->validate && (ctxt->myDoc->extSubset == NULL) &&
        ((ctxt->myDoc->intSubset == NULL) ||
	 ((ctxt->myDoc->intSubset->notations == NULL) &&
	  (ctxt->myDoc->intSubset->elements == NULL) &&
	  (ctxt->myDoc->intSubset->attributes == NULL) &&
	  (ctxt->myDoc->intSubset->entities == NULL)))) {
	xmlErrValid(ctxt, XML_ERR_NO_DTD,
	  "Validation failed: no DTD found !", NULL, NULL);
	ctxt->validate = 0;
    }

    if (ctxt->html) {
        prefix = NULL;
        name = xmlStrdup(fullname);
    } else {
        /*
         * Split the full name into a namespace prefix and the tag name
         */
        name = xmlSplitQName(ctxt, fullname, &prefix);
        if (name == NULL) {
            xmlSAX2ErrMemory(ctxt);
            return;
        }
    }

    /*
     * Note : the namespace resolution is deferred until the end of the
     *        attributes parsing, since local namespace can be defined as
     *        an attribute at this level.
     */
    ret = xmlNewDocNodeEatName(ctxt->myDoc, NULL, name, NULL);
    if (ret == NULL) {
	xmlFree(prefix);
	xmlSAX2ErrMemory(ctxt);
        return;
    }
    ctxt->nodemem = -1;

    /* Initialize parent before pushing node */
    parent = ctxt->node;
    if (parent == NULL)
        parent = (xmlNodePtr) ctxt->myDoc;

    /*
     * Link the child element
     */
    xmlSAX2AppendChild(ctxt, ret);

    /*
     * We are parsing a new node.
     */
    if (nodePush(ctxt, ret) < 0) {
        xmlUnlinkNode(ret);
        xmlFreeNode(ret);
        if (prefix != NULL)
            xmlFree(prefix);
        return;
    }

    if (!ctxt->html) {
        int res;

        /*
         * Insert all the defaulted attributes from the DTD especially
         * namespaces
         */
        if ((ctxt->myDoc->intSubset != NULL) ||
            (ctxt->myDoc->extSubset != NULL)) {
            xmlCheckDefaultedAttributes(ctxt, name, prefix, atts);
        }

        /*
         * process all the attributes whose name start with "xmlns"
         */
        if (atts != NULL) {
            i = 0;
            att = atts[i++];
            value = atts[i++];
	    while ((att != NULL) && (value != NULL)) {
		if ((att[0] == 'x') && (att[1] == 'm') && (att[2] == 'l') &&
		    (att[3] == 'n') && (att[4] == 's'))
		    xmlSAX2AttributeInternal(ctxt, att, value, prefix);

		att = atts[i++];
		value = atts[i++];
	    }
        }

        /*
         * Search the namespace, note that since the attributes have been
         * processed, the local namespaces are available.
         */
        res = xmlSearchNsSafe(ret, prefix, &ns);
        if (res < 0)
            xmlSAX2ErrMemory(ctxt);
        if ((ns == NULL) && (parent != NULL)) {
            res = xmlSearchNsSafe(parent, prefix, &ns);
            if (res < 0)
                xmlSAX2ErrMemory(ctxt);
        }
        if ((prefix != NULL) && (ns == NULL)) {
            xmlNsWarnMsg(ctxt, XML_NS_ERR_UNDEFINED_NAMESPACE,
                         "Namespace prefix %s is not defined\n",
                         prefix, NULL);
            ns = xmlNewNs(ret, NULL, prefix);
            if (ns == NULL)
                xmlSAX2ErrMemory(ctxt);
        }

        /*
         * set the namespace node, making sure that if the default namespace
         * is unbound on a parent we simply keep it NULL
         */
        if ((ns != NULL) && (ns->href != NULL) &&
            ((ns->href[0] != 0) || (ns->prefix != NULL)))
            xmlSetNs(ret, ns);
    }

    /*
     * process all the other attributes
     */
    if (atts != NULL) {
        i = 0;
	att = atts[i++];
	value = atts[i++];
	if (ctxt->html) {
	    while (att != NULL) {
		xmlSAX2AttributeInternal(ctxt, att, value, NULL);
		att = atts[i++];
		value = atts[i++];
	    }
	} else {
	    while ((att != NULL) && (value != NULL)) {
		if ((att[0] != 'x') || (att[1] != 'm') || (att[2] != 'l') ||
		    (att[3] != 'n') || (att[4] != 's'))
		    xmlSAX2AttributeInternal(ctxt, att, value, NULL);

		/*
		 * Next ones
		 */
		att = atts[i++];
		value = atts[i++];
	    }
	}
    }

#ifdef LIBXML_VALID_ENABLED
    /*
     * If it's the Document root, finish the DTD validation and
     * check the document root element for validity
     */
    if ((ctxt->validate) &&
        ((ctxt->vctxt.flags & XML_VCTXT_DTD_VALIDATED) == 0)) {
	int chk;

	chk = xmlValidateDtdFinal(&ctxt->vctxt, ctxt->myDoc);
	if (chk <= 0)
	    ctxt->valid = 0;
	if (chk < 0)
	    ctxt->wellFormed = 0;
	ctxt->valid &= xmlValidateRoot(&ctxt->vctxt, ctxt->myDoc);
	ctxt->vctxt.flags |= XML_VCTXT_DTD_VALIDATED;
    }
#endif /* LIBXML_VALID_ENABLED */

    if (prefix != NULL)
	xmlFree(prefix);

}

/**
 * xmlSAX2EndElement:
 * @ctx: the user data (XML parser context)
 * @name:  The element name
 *
 * called when the end of an element has been detected.
 */
void
xmlSAX2EndElement(void *ctx, const xmlChar *name ATTRIBUTE_UNUSED)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;

    if (ctx == NULL) return;

    ctxt->nodemem = -1;

#ifdef LIBXML_VALID_ENABLED
    if (ctxt->validate && ctxt->wellFormed &&
        ctxt->myDoc && ctxt->myDoc->intSubset)
        ctxt->valid &= xmlValidateOneElement(&ctxt->vctxt, ctxt->myDoc,
					     ctxt->node);
#endif /* LIBXML_VALID_ENABLED */


    /*
     * end of parsing of this node.
     */
    nodePop(ctxt);
}
#endif /* LIBXML_SAX1_ENABLED || LIBXML_HTML_ENABLED || LIBXML_LEGACY_ENABLED */

/*
 * xmlSAX2TextNode:
 * @ctxt:  the parser context
 * @str:  the input string
 * @len: the string length
 *
 * Callback for a text node
 *
 * Returns the newly allocated string or NULL if not needed or error
 */
static xmlNodePtr
xmlSAX2TextNode(xmlParserCtxtPtr ctxt, const xmlChar *str, int len) {
    xmlNodePtr ret;
    const xmlChar *intern = NULL;

    /*
     * Allocate
     */
    if (ctxt->freeElems != NULL) {
	ret = ctxt->freeElems;
	ctxt->freeElems = ret->next;
	ctxt->freeElemsNr--;
    } else {
	ret = (xmlNodePtr) xmlMalloc(sizeof(xmlNode));
    }
    if (ret == NULL) {
        xmlCtxtErrMemory(ctxt);
	return(NULL);
    }
    memset(ret, 0, sizeof(xmlNode));
    /*
     * intern the formatting blanks found between tags, or the
     * very short strings
     */
    if (ctxt->dictNames) {
        xmlChar cur = str[len];

	if ((len < (int) (2 * sizeof(void *))) &&
	    (ctxt->options & XML_PARSE_COMPACT)) {
	    /* store the string in the node overriding properties and nsDef */
	    xmlChar *tmp = (xmlChar *) &(ret->properties);
	    memcpy(tmp, str, len);
	    tmp[len] = 0;
	    intern = tmp;
	} else if ((len <= 3) && ((cur == '"') || (cur == '\'') ||
	    ((cur == '<') && (str[len + 1] != '!')))) {
	    intern = xmlDictLookup(ctxt->dict, str, len);
            if (intern == NULL) {
                xmlSAX2ErrMemory(ctxt);
                xmlFree(ret);
                return(NULL);
            }
	} else if (IS_BLANK_CH(*str) && (len < 60) && (cur == '<') &&
	           (str[len + 1] != '!')) {
	    int i;

	    for (i = 1;i < len;i++) {
		if (!IS_BLANK_CH(str[i])) goto skip;
	    }
	    intern = xmlDictLookup(ctxt->dict, str, len);
            if (intern == NULL) {
                xmlSAX2ErrMemory(ctxt);
                xmlFree(ret);
                return(NULL);
            }
	}
    }
skip:
    ret->type = XML_TEXT_NODE;

    ret->name = xmlStringText;
    if (intern == NULL) {
	ret->content = xmlStrndup(str, len);
	if (ret->content == NULL) {
	    xmlSAX2ErrMemory(ctxt);
	    xmlFree(ret);
	    return(NULL);
	}
    } else
	ret->content = (xmlChar *) intern;

    if ((__xmlRegisterCallbacks) && (xmlRegisterNodeDefaultValue))
	xmlRegisterNodeDefaultValue(ret);
    return(ret);
}

#ifdef LIBXML_VALID_ENABLED
/*
 * xmlSAX2DecodeAttrEntities:
 * @ctxt:  the parser context
 * @str:  the input string
 * @len: the string length
 *
 * Remove the entities from an attribute value
 *
 * Returns the newly allocated string or NULL if not needed or error
 */
static xmlChar *
xmlSAX2DecodeAttrEntities(xmlParserCtxtPtr ctxt, const xmlChar *str,
                          const xmlChar *end) {
    const xmlChar *in;

    in = str;
    while (in < end)
        if (*in++ == '&')
	    goto decode;
    return(NULL);
decode:
    /*
     * If the value contains '&', we can be sure it was allocated and is
     * zero-terminated.
     */
    /* TODO: normalize if needed */
    return(xmlExpandEntitiesInAttValue(ctxt, str, /* normalize */ 0));
}
#endif /* LIBXML_VALID_ENABLED */

/**
 * xmlSAX2AttributeNs:
 * @ctx: the user data (XML parser context)
 * @localname:  the local name of the attribute
 * @prefix:  the attribute namespace prefix if available
 * @URI:  the attribute namespace name if available
 * @value:  Start of the attribute value
 * @valueend: end of the attribute value
 *
 * Handle an attribute that has been read by the parser.
 * The default handling is to convert the attribute into an
 * DOM subtree and past it in a new xmlAttr element added to
 * the element.
 *
 * Returns the new attribute or NULL in case of error.
 */
static xmlAttrPtr
xmlSAX2AttributeNs(xmlParserCtxtPtr ctxt,
                   const xmlChar * localname,
                   const xmlChar * prefix,
		   const xmlChar * value,
		   const xmlChar * valueend)
{
    xmlAttrPtr ret;
    xmlNsPtr namespace = NULL;
    xmlChar *dup = NULL;

    /*
     * Note: if prefix == NULL, the attribute is not in the default namespace
     */
    if (prefix != NULL) {
	namespace = xmlParserNsLookupSax(ctxt, prefix);
	if ((namespace == NULL) && (xmlStrEqual(prefix, BAD_CAST "xml"))) {
            int res;

	    res = xmlSearchNsSafe(ctxt->node, prefix, &namespace);
            if (res < 0)
                xmlSAX2ErrMemory(ctxt);
	}
    }

    /*
     * allocate the node
     */
    if (ctxt->freeAttrs != NULL) {
        ret = ctxt->freeAttrs;
	ctxt->freeAttrs = ret->next;
	ctxt->freeAttrsNr--;
    } else {
        ret = xmlMalloc(sizeof(*ret));
        if (ret == NULL) {
            xmlSAX2ErrMemory(ctxt);
            return(NULL);
        }
    }

    memset(ret, 0, sizeof(xmlAttr));
    ret->type = XML_ATTRIBUTE_NODE;

    /*
     * xmlParseBalancedChunkMemoryRecover had a bug that could result in
     * a mismatch between ctxt->node->doc and ctxt->myDoc. We use
     * ctxt->node->doc here, but we should somehow make sure that the
     * document pointers match.
     */

    /* assert(ctxt->node->doc == ctxt->myDoc); */

    ret->parent = ctxt->node;
    ret->doc = ctxt->node->doc;
    ret->ns = namespace;

    if (ctxt->dictNames) {
        ret->name = localname;
    } else {
        ret->name = xmlStrdup(localname);
        if (ret->name == NULL)
            xmlSAX2ErrMemory(ctxt);
    }

    if ((__xmlRegisterCallbacks) && (xmlRegisterNodeDefaultValue))
        xmlRegisterNodeDefaultValue((xmlNodePtr)ret);

    if ((ctxt->replaceEntities == 0) && (!ctxt->html)) {
	xmlNodePtr tmp;

	/*
	 * We know that if there is an entity reference, then
	 * the string has been dup'ed and terminates with 0
	 * otherwise with ' or "
	 */
	if (*valueend != 0) {
	    tmp = xmlSAX2TextNode(ctxt, value, valueend - value);
	    ret->children = tmp;
	    ret->last = tmp;
	    if (tmp != NULL) {
		tmp->doc = ret->doc;
		tmp->parent = (xmlNodePtr) ret;
	    }
	} else if (valueend > value) {
            if (xmlNodeParseContent((xmlNodePtr) ret, value,
                                    valueend - value) < 0)
                xmlSAX2ErrMemory(ctxt);
	}
    } else if (value != NULL) {
	xmlNodePtr tmp;

	tmp = xmlSAX2TextNode(ctxt, value, valueend - value);
	ret->children = tmp;
	ret->last = tmp;
	if (tmp != NULL) {
	    tmp->doc = ret->doc;
	    tmp->parent = (xmlNodePtr) ret;
	}
    }

#ifdef LIBXML_VALID_ENABLED
    if ((!ctxt->html) && ctxt->validate && ctxt->wellFormed &&
        ctxt->myDoc && ctxt->myDoc->intSubset) {
	/*
	 * If we don't substitute entities, the validation should be
	 * done on a value with replaced entities anyway.
	 */
        if (!ctxt->replaceEntities) {
	    dup = xmlSAX2DecodeAttrEntities(ctxt, value, valueend);
	    if (dup == NULL) {
	        if (*valueend == 0) {
		    ctxt->valid &= xmlValidateOneAttribute(&ctxt->vctxt,
				    ctxt->myDoc, ctxt->node, ret, value);
		} else {
		    /*
		     * That should already be normalized.
		     * cheaper to finally allocate here than duplicate
		     * entry points in the full validation code
		     */
		    dup = xmlStrndup(value, valueend - value);
                    if (dup == NULL)
                        xmlSAX2ErrMemory(ctxt);

		    ctxt->valid &= xmlValidateOneAttribute(&ctxt->vctxt,
				    ctxt->myDoc, ctxt->node, ret, dup);
		}
	    } else {
	        /*
		 * dup now contains a string of the flattened attribute
		 * content with entities substituted. Check if we need to
		 * apply an extra layer of normalization.
		 * It need to be done twice ... it's an extra burden related
		 * to the ability to keep references in attributes
		 */
		if (ctxt->attsSpecial != NULL) {
		    xmlChar *nvalnorm;
		    xmlChar fn[50];
		    xmlChar *fullname;

		    fullname = xmlBuildQName(localname, prefix, fn, 50);
                    if (fullname == NULL) {
                        xmlSAX2ErrMemory(ctxt);
                    } else {
			ctxt->vctxt.valid = 1;
		        nvalnorm = xmlValidCtxtNormalizeAttributeValue(
			                 &ctxt->vctxt, ctxt->myDoc,
					 ctxt->node, fullname, dup);
			if (ctxt->vctxt.valid != 1)
			    ctxt->valid = 0;

			if ((fullname != fn) && (fullname != localname))
			    xmlFree(fullname);
			if (nvalnorm != NULL) {
			    xmlFree(dup);
			    dup = nvalnorm;
			}
		    }
		}

		ctxt->valid &= xmlValidateOneAttribute(&ctxt->vctxt,
			        ctxt->myDoc, ctxt->node, ret, dup);
	    }
	} else {
	    /*
	     * if entities already have been substituted, then
	     * the attribute as passed is already normalized
	     */
	    dup = xmlStrndup(value, valueend - value);
            if (dup == NULL)
                xmlSAX2ErrMemory(ctxt);

	    ctxt->valid &= xmlValidateOneAttribute(&ctxt->vctxt,
	                             ctxt->myDoc, ctxt->node, ret, dup);
	}
    } else
#endif /* LIBXML_VALID_ENABLED */
           if (((ctxt->loadsubset & XML_SKIP_IDS) == 0) &&
               /* Don't create IDs containing entity references */
               (ret->children != NULL) &&
               (ret->children->type == XML_TEXT_NODE) &&
               (ret->children->next == NULL)) {
        xmlChar *content = ret->children->content;
        /*
	 * when validating, the ID registration is done at the attribute
	 * validation level. Otherwise we have to do specific handling here.
	 */
        if ((prefix == ctxt->str_xml) &&
	           (localname[0] == 'i') && (localname[1] == 'd') &&
		   (localname[2] == 0)) {
	    /*
	     * Add the xml:id value
	     *
	     * Open issue: normalization of the value.
	     */
	    if (xmlValidateNCName(content, 1) != 0) {
	        xmlErrValid(ctxt, XML_DTD_XMLID_VALUE,
                            "xml:id : attribute value %s is not an NCName\n",
                            content, NULL);
	    }
	    xmlAddID(&ctxt->vctxt, ctxt->myDoc, content, ret);
	} else {
            int res = xmlIsID(ctxt->myDoc, ctxt->node, ret);

            if (res < 0)
                xmlCtxtErrMemory(ctxt);
            else if (res > 0)
                xmlAddID(&ctxt->vctxt, ctxt->myDoc, content, ret);
            else if (xmlIsRef(ctxt->myDoc, ctxt->node, ret))
                xmlAddRef(&ctxt->vctxt, ctxt->myDoc, content, ret);
	}
    }
    if (dup != NULL)
	xmlFree(dup);

    return(ret);
}

/**
 * xmlSAX2StartElementNs:
 * @ctx:  the user data (XML parser context)
 * @localname:  the local name of the element
 * @prefix:  the element namespace prefix if available
 * @URI:  the element namespace name if available
 * @nb_namespaces:  number of namespace definitions on that node
 * @namespaces:  pointer to the array of prefix/URI pairs namespace definitions
 * @nb_attributes:  the number of attributes on that node
 * @nb_defaulted:  the number of defaulted attributes.
 * @attributes:  pointer to the array of (localname/prefix/URI/value/end)
 *               attribute values.
 *
 * SAX2 callback when an element start has been detected by the parser.
 * It provides the namespace information for the element, as well as
 * the new namespace declarations on the element.
 */
void
xmlSAX2StartElementNs(void *ctx,
                      const xmlChar *localname,
		      const xmlChar *prefix,
		      const xmlChar *URI,
		      int nb_namespaces,
		      const xmlChar **namespaces,
		      int nb_attributes,
		      int nb_defaulted,
		      const xmlChar **attributes)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr ret;
    xmlNsPtr last = NULL, ns;
    const xmlChar *uri, *pref;
    xmlChar *lname = NULL;
    int i, j;

    if (ctx == NULL) return;
    /*
     * First check on validity:
     */
    if (ctxt->validate && (ctxt->myDoc->extSubset == NULL) &&
        ((ctxt->myDoc->intSubset == NULL) ||
	 ((ctxt->myDoc->intSubset->notations == NULL) &&
	  (ctxt->myDoc->intSubset->elements == NULL) &&
	  (ctxt->myDoc->intSubset->attributes == NULL) &&
	  (ctxt->myDoc->intSubset->entities == NULL)))) {
	xmlErrValid(ctxt, XML_DTD_NO_DTD,
	  "Validation failed: no DTD found !", NULL, NULL);
	ctxt->validate = 0;
    }

    /*
     * Take care of the rare case of an undefined namespace prefix
     */
    if ((prefix != NULL) && (URI == NULL)) {
        if (ctxt->dictNames) {
	    const xmlChar *fullname;

	    fullname = xmlDictQLookup(ctxt->dict, prefix, localname);
	    if (fullname == NULL) {
                xmlSAX2ErrMemory(ctxt);
                return;
            }
	    localname = fullname;
	} else {
	    lname = xmlBuildQName(localname, prefix, NULL, 0);
            if (lname == NULL) {
                xmlSAX2ErrMemory(ctxt);
                return;
            }
	}
    }
    /*
     * allocate the node
     */
    if (ctxt->freeElems != NULL) {
        ret = ctxt->freeElems;
	ctxt->freeElems = ret->next;
	ctxt->freeElemsNr--;
	memset(ret, 0, sizeof(xmlNode));
        ret->doc = ctxt->myDoc;
	ret->type = XML_ELEMENT_NODE;

	if (ctxt->dictNames)
	    ret->name = localname;
	else {
	    if (lname == NULL)
		ret->name = xmlStrdup(localname);
	    else
	        ret->name = lname;
	    if (ret->name == NULL) {
	        xmlSAX2ErrMemory(ctxt);
                xmlFree(ret);
		return;
	    }
	}
	if ((__xmlRegisterCallbacks) && (xmlRegisterNodeDefaultValue))
	    xmlRegisterNodeDefaultValue(ret);
    } else {
	if (ctxt->dictNames)
	    ret = xmlNewDocNodeEatName(ctxt->myDoc, NULL,
	                               (xmlChar *) localname, NULL);
	else if (lname == NULL)
	    ret = xmlNewDocNode(ctxt->myDoc, NULL, localname, NULL);
	else
	    ret = xmlNewDocNodeEatName(ctxt->myDoc, NULL,
	                               (xmlChar *) lname, NULL);
	if (ret == NULL) {
	    xmlSAX2ErrMemory(ctxt);
	    return;
	}
    }

    /*
     * Build the namespace list
     */
    for (i = 0,j = 0;j < nb_namespaces;j++) {
        pref = namespaces[i++];
	uri = namespaces[i++];
	ns = xmlNewNs(NULL, uri, pref);
	if (ns != NULL) {
	    if (last == NULL) {
	        ret->nsDef = last = ns;
	    } else {
	        last->next = ns;
		last = ns;
	    }
	    if ((URI != NULL) && (prefix == pref))
		ret->ns = ns;
	} else {
            xmlSAX2ErrMemory(ctxt);
	    continue;
	}

        xmlParserNsUpdateSax(ctxt, pref, ns);

#ifdef LIBXML_VALID_ENABLED
	if ((!ctxt->html) && ctxt->validate && ctxt->wellFormed &&
	    ctxt->myDoc && ctxt->myDoc->intSubset) {
	    ctxt->valid &= xmlValidateOneNamespace(&ctxt->vctxt, ctxt->myDoc,
	                                           ret, prefix, ns, uri);
	}
#endif /* LIBXML_VALID_ENABLED */
    }
    ctxt->nodemem = -1;

    /*
     * Link the child element
     */
    xmlSAX2AppendChild(ctxt, ret);

    /*
     * We are parsing a new node.
     */
    if (nodePush(ctxt, ret) < 0) {
        xmlUnlinkNode(ret);
        xmlFreeNode(ret);
        return;
    }

    /*
     * Insert the defaulted attributes from the DTD only if requested:
     */
    if ((nb_defaulted != 0) &&
        ((ctxt->loadsubset & XML_COMPLETE_ATTRS) == 0))
	nb_attributes -= nb_defaulted;

    /*
     * Search the namespace if it wasn't already found
     * Note that, if prefix is NULL, this searches for the default Ns
     */
    if ((URI != NULL) && (ret->ns == NULL)) {
        ret->ns = xmlParserNsLookupSax(ctxt, prefix);
	if ((ret->ns == NULL) && (xmlStrEqual(prefix, BAD_CAST "xml"))) {
            int res;

	    res = xmlSearchNsSafe(ret, prefix, &ret->ns);
            if (res < 0)
                xmlSAX2ErrMemory(ctxt);
	}
	if (ret->ns == NULL) {
	    ns = xmlNewNs(ret, NULL, prefix);
	    if (ns == NULL) {

	        xmlSAX2ErrMemory(ctxt);
		return;
	    }
            if (prefix != NULL)
                xmlNsWarnMsg(ctxt, XML_NS_ERR_UNDEFINED_NAMESPACE,
                             "Namespace prefix %s was not found\n",
                             prefix, NULL);
            else
                xmlNsWarnMsg(ctxt, XML_NS_ERR_UNDEFINED_NAMESPACE,
                             "Namespace default prefix was not found\n",
                             NULL, NULL);
	}
    }

    /*
     * process all the other attributes
     */
    if (nb_attributes > 0) {
        xmlAttrPtr prev = NULL;

        for (j = 0,i = 0;i < nb_attributes;i++,j+=5) {
            xmlAttrPtr attr = NULL;

	    /*
	     * Handle the rare case of an undefined attribute prefix
	     */
	    if ((attributes[j+1] != NULL) && (attributes[j+2] == NULL)) {
		if (ctxt->dictNames) {
		    const xmlChar *fullname;

		    fullname = xmlDictQLookup(ctxt->dict, attributes[j+1],
		                              attributes[j]);
		    if (fullname == NULL) {
                        xmlSAX2ErrMemory(ctxt);
                        return;
                    }
                    attr = xmlSAX2AttributeNs(ctxt, fullname, NULL,
                                              attributes[j+3],
                                              attributes[j+4]);
                    goto have_attr;
		} else {
		    lname = xmlBuildQName(attributes[j], attributes[j+1],
		                          NULL, 0);
		    if (lname == NULL) {
                        xmlSAX2ErrMemory(ctxt);
                        return;
                    }
                    attr = xmlSAX2AttributeNs(ctxt, lname, NULL,
                                              attributes[j+3],
                                              attributes[j+4]);
                    xmlFree(lname);
                    goto have_attr;
		}
	    }
            attr = xmlSAX2AttributeNs(ctxt, attributes[j], attributes[j+1],
                                      attributes[j+3], attributes[j+4]);
have_attr:
            if (attr == NULL)
                continue;

            /* link at the end to preserve order */
            if (prev == NULL) {
                ctxt->node->properties = attr;
            } else {
                prev->next = attr;
                attr->prev = prev;
            }

            prev = attr;
	}
    }

#ifdef LIBXML_VALID_ENABLED
    /*
     * If it's the Document root, finish the DTD validation and
     * check the document root element for validity
     */
    if ((ctxt->validate) &&
        ((ctxt->vctxt.flags & XML_VCTXT_DTD_VALIDATED) == 0)) {
	int chk;

	chk = xmlValidateDtdFinal(&ctxt->vctxt, ctxt->myDoc);
	if (chk <= 0)
	    ctxt->valid = 0;
	if (chk < 0)
	    ctxt->wellFormed = 0;
	ctxt->valid &= xmlValidateRoot(&ctxt->vctxt, ctxt->myDoc);
	ctxt->vctxt.flags |= XML_VCTXT_DTD_VALIDATED;
    }
#endif /* LIBXML_VALID_ENABLED */
}

/**
 * xmlSAX2EndElementNs:
 * @ctx:  the user data (XML parser context)
 * @localname:  the local name of the element
 * @prefix:  the element namespace prefix if available
 * @URI:  the element namespace name if available
 *
 * SAX2 callback when an element end has been detected by the parser.
 * It provides the namespace information for the element.
 */
void
xmlSAX2EndElementNs(void *ctx,
                    const xmlChar * localname ATTRIBUTE_UNUSED,
                    const xmlChar * prefix ATTRIBUTE_UNUSED,
		    const xmlChar * URI ATTRIBUTE_UNUSED)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;

    if (ctx == NULL) return;
    ctxt->nodemem = -1;

#ifdef LIBXML_VALID_ENABLED
    if (ctxt->validate && ctxt->wellFormed &&
        ctxt->myDoc && ctxt->myDoc->intSubset)
        ctxt->valid &= xmlValidateOneElement(&ctxt->vctxt, ctxt->myDoc,
                                             ctxt->node);
#endif /* LIBXML_VALID_ENABLED */

    /*
     * end of parsing of this node.
     */
    nodePop(ctxt);
}

/**
 * xmlSAX2Reference:
 * @ctx: the user data (XML parser context)
 * @name:  The entity name
 *
 * called when an entity xmlSAX2Reference is detected.
 */
void
xmlSAX2Reference(void *ctx, const xmlChar *name)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr ret;

    if (ctx == NULL) return;
    ret = xmlNewReference(ctxt->myDoc, name);
    if (ret == NULL) {
        xmlSAX2ErrMemory(ctxt);
        return;
    }

    xmlSAX2AppendChild(ctxt, ret);
}

/**
 * xmlSAX2Text:
 * @ctx: the user data (XML parser context)
 * @ch:  a xmlChar string
 * @len: the number of xmlChar
 * @type: text or cdata
 *
 * Append characters.
 */
static void
xmlSAX2Text(xmlParserCtxtPtr ctxt, const xmlChar *ch, int len,
            xmlElementType type)
{
    xmlNodePtr lastChild;

    if (ctxt == NULL) return;
    /*
     * Handle the data if any. If there is no child
     * add it as content, otherwise if the last child is text,
     * concatenate it, else create a new node of type text.
     */

    if (ctxt->node == NULL) {
        return;
    }
    lastChild = ctxt->node->last;

    /*
     * Here we needed an accelerator mechanism in case of very large
     * elements. Use an attribute in the structure !!!
     */
    if (lastChild == NULL) {
        if (type == XML_TEXT_NODE)
            lastChild = xmlSAX2TextNode(ctxt, ch, len);
        else
            lastChild = xmlNewCDataBlock(ctxt->myDoc, ch, len);
	if (lastChild != NULL) {
	    ctxt->node->children = lastChild;
	    ctxt->node->last = lastChild;
	    lastChild->parent = ctxt->node;
	    lastChild->doc = ctxt->node->doc;
	    ctxt->nodelen = len;
	    ctxt->nodemem = len + 1;
	} else {
	    xmlSAX2ErrMemory(ctxt);
	    return;
	}
    } else {
	int coalesceText = (lastChild != NULL) &&
	    (lastChild->type == type) &&
	    ((type != XML_TEXT_NODE) ||
             (lastChild->name == xmlStringText));
	if ((coalesceText) && (ctxt->nodemem != 0)) {
            int maxLength = (ctxt->options & XML_PARSE_HUGE) ?
                            XML_MAX_HUGE_LENGTH :
                            XML_MAX_TEXT_LENGTH;

	    /*
	     * The whole point of maintaining nodelen and nodemem,
	     * xmlTextConcat is too costly, i.e. compute length,
	     * reallocate a new buffer, move data, append ch. Here
	     * We try to minimize realloc() uses and avoid copying
	     * and recomputing length over and over.
	     */
	    if (lastChild->content == (xmlChar *)&(lastChild->properties)) {
		lastChild->content = xmlStrdup(lastChild->content);
		lastChild->properties = NULL;
	    } else if ((ctxt->nodemem == ctxt->nodelen + 1) &&
	               (xmlDictOwns(ctxt->dict, lastChild->content))) {
		lastChild->content = xmlStrdup(lastChild->content);
	    }
	    if (lastChild->content == NULL) {
		xmlSAX2ErrMemory(ctxt);
		return;
 	    }
            if ((len > maxLength) || (ctxt->nodelen > maxLength - len)) {
                xmlFatalErr(ctxt, XML_ERR_RESOURCE_LIMIT,
                            "Text node too long, try XML_PARSE_HUGE");
                xmlHaltParser(ctxt);
                return;
            }
	    if (ctxt->nodelen + len >= ctxt->nodemem) {
		xmlChar *newbuf;
		int size;

		size = ctxt->nodemem > INT_MAX - len ?
                       INT_MAX :
                       ctxt->nodemem + len;
		size = size > INT_MAX / 2 ? INT_MAX : size * 2;
                newbuf = (xmlChar *) xmlRealloc(lastChild->content,size);
		if (newbuf == NULL) {
		    xmlSAX2ErrMemory(ctxt);
		    return;
		}
		ctxt->nodemem = size;
		lastChild->content = newbuf;
	    }
	    memcpy(&lastChild->content[ctxt->nodelen], ch, len);
	    ctxt->nodelen += len;
	    lastChild->content[ctxt->nodelen] = 0;
	} else if (coalesceText) {
	    if (xmlTextConcat(lastChild, ch, len)) {
		xmlSAX2ErrMemory(ctxt);
	    }
	    if (ctxt->node->children != NULL) {
		ctxt->nodelen = xmlStrlen(lastChild->content);
		ctxt->nodemem = ctxt->nodelen + 1;
	    }
	} else {
	    /* Mixed content, first time */
            if (type == XML_TEXT_NODE) {
                lastChild = xmlSAX2TextNode(ctxt, ch, len);
                if (lastChild != NULL)
                    lastChild->doc = ctxt->myDoc;
            } else
                lastChild = xmlNewCDataBlock(ctxt->myDoc, ch, len);
	    if (lastChild == NULL) {
                xmlSAX2ErrMemory(ctxt);
            } else {
		xmlSAX2AppendChild(ctxt, lastChild);
		if (ctxt->node->children != NULL) {
		    ctxt->nodelen = len;
		    ctxt->nodemem = len + 1;
		}
	    }
	}
    }

    if ((lastChild != NULL) &&
        (type == XML_TEXT_NODE) &&
        (ctxt->linenumbers) &&
        (ctxt->input != NULL)) {
        if ((unsigned) ctxt->input->line < (unsigned) USHRT_MAX)
            lastChild->line = ctxt->input->line;
        else {
            lastChild->line = USHRT_MAX;
            if (ctxt->options & XML_PARSE_BIG_LINES)
                lastChild->psvi = (void *) (ptrdiff_t) ctxt->input->line;
        }
    }
}

/**
 * xmlSAX2Characters:
 * @ctx: the user data (XML parser context)
 * @ch:  a xmlChar string
 * @len: the number of xmlChar
 *
 * receiving some chars from the parser.
 */
void
xmlSAX2Characters(void *ctx, const xmlChar *ch, int len)
{
    xmlSAX2Text((xmlParserCtxtPtr) ctx, ch, len, XML_TEXT_NODE);
}

/**
 * xmlSAX2IgnorableWhitespace:
 * @ctx: the user data (XML parser context)
 * @ch:  a xmlChar string
 * @len: the number of xmlChar
 *
 * receiving some ignorable whitespaces from the parser.
 * UNUSED: by default the DOM building will use xmlSAX2Characters
 */
void
xmlSAX2IgnorableWhitespace(void *ctx ATTRIBUTE_UNUSED, const xmlChar *ch ATTRIBUTE_UNUSED, int len ATTRIBUTE_UNUSED)
{
}

/**
 * xmlSAX2ProcessingInstruction:
 * @ctx: the user data (XML parser context)
 * @target:  the target name
 * @data: the PI data's
 *
 * A processing instruction has been parsed.
 */
void
xmlSAX2ProcessingInstruction(void *ctx, const xmlChar *target,
                      const xmlChar *data)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr ret;

    if (ctx == NULL) return;

    ret = xmlNewDocPI(ctxt->myDoc, target, data);
    if (ret == NULL) {
        xmlSAX2ErrMemory(ctxt);
        return;
    }

    xmlSAX2AppendChild(ctxt, ret);
}

/**
 * xmlSAX2Comment:
 * @ctx: the user data (XML parser context)
 * @value:  the xmlSAX2Comment content
 *
 * A xmlSAX2Comment has been parsed.
 */
void
xmlSAX2Comment(void *ctx, const xmlChar *value)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlNodePtr ret;

    if (ctx == NULL) return;

    ret = xmlNewDocComment(ctxt->myDoc, value);
    if (ret == NULL) {
        xmlSAX2ErrMemory(ctxt);
        return;
    }

    xmlSAX2AppendChild(ctxt, ret);
}

/**
 * xmlSAX2CDataBlock:
 * @ctx: the user data (XML parser context)
 * @value:  The pcdata content
 * @len:  the block length
 *
 * called when a pcdata block has been parsed
 */
void
xmlSAX2CDataBlock(void *ctx, const xmlChar *value, int len)
{
    xmlSAX2Text((xmlParserCtxtPtr) ctx, value, len, XML_CDATA_SECTION_NODE);
}

static int xmlSAX2DefaultVersionValue = 2;

#ifdef LIBXML_SAX1_ENABLED
/**
 * xmlSAXDefaultVersion:
 * @version:  the version, 1 or 2
 *
 * DEPRECATED: Use parser option XML_PARSE_SAX1.
 *
 * Set the default version of SAX used globally by the library.
 * By default, during initialization the default is set to 2.
 * Note that it is generally a better coding style to use
 * xmlSAXVersion() to set up the version explicitly for a given
 * parsing context.
 *
 * Returns the previous value in case of success and -1 in case of error.
 */
int
xmlSAXDefaultVersion(int version)
{
    int ret = xmlSAX2DefaultVersionValue;

    if ((version != 1) && (version != 2))
        return(-1);
    xmlSAX2DefaultVersionValue = version;
    return(ret);
}
#endif /* LIBXML_SAX1_ENABLED */

/**
 * xmlSAXVersion:
 * @hdlr:  the SAX handler
 * @version:  the version, 1 or 2
 *
 * Initialize the default XML SAX handler according to the version
 *
 * Returns 0 in case of success and -1 in case of error.
 */
int
xmlSAXVersion(xmlSAXHandler *hdlr, int version)
{
    if (hdlr == NULL) return(-1);
    if (version == 2) {
	hdlr->startElementNs = xmlSAX2StartElementNs;
	hdlr->endElementNs = xmlSAX2EndElementNs;
	hdlr->serror = NULL;
	hdlr->initialized = XML_SAX2_MAGIC;
#ifdef LIBXML_SAX1_ENABLED
    } else if (version == 1) {
	hdlr->initialized = 1;
#endif /* LIBXML_SAX1_ENABLED */
    } else
        return(-1);
#ifdef LIBXML_SAX1_ENABLED
    hdlr->startElement = xmlSAX2StartElement;
    hdlr->endElement = xmlSAX2EndElement;
#else
    hdlr->startElement = NULL;
    hdlr->endElement = NULL;
#endif /* LIBXML_SAX1_ENABLED */
    hdlr->internalSubset = xmlSAX2InternalSubset;
    hdlr->externalSubset = xmlSAX2ExternalSubset;
    hdlr->isStandalone = xmlSAX2IsStandalone;
    hdlr->hasInternalSubset = xmlSAX2HasInternalSubset;
    hdlr->hasExternalSubset = xmlSAX2HasExternalSubset;
    hdlr->resolveEntity = xmlSAX2ResolveEntity;
    hdlr->getEntity = xmlSAX2GetEntity;
    hdlr->getParameterEntity = xmlSAX2GetParameterEntity;
    hdlr->entityDecl = xmlSAX2EntityDecl;
    hdlr->attributeDecl = xmlSAX2AttributeDecl;
    hdlr->elementDecl = xmlSAX2ElementDecl;
    hdlr->notationDecl = xmlSAX2NotationDecl;
    hdlr->unparsedEntityDecl = xmlSAX2UnparsedEntityDecl;
    hdlr->setDocumentLocator = xmlSAX2SetDocumentLocator;
    hdlr->startDocument = xmlSAX2StartDocument;
    hdlr->endDocument = xmlSAX2EndDocument;
    hdlr->reference = xmlSAX2Reference;
    hdlr->characters = xmlSAX2Characters;
    hdlr->cdataBlock = xmlSAX2CDataBlock;
    hdlr->ignorableWhitespace = xmlSAX2Characters;
    hdlr->processingInstruction = xmlSAX2ProcessingInstruction;
    hdlr->comment = xmlSAX2Comment;
    hdlr->warning = xmlParserWarning;
    hdlr->error = xmlParserError;
    hdlr->fatalError = xmlParserError;

    return(0);
}

/**
 * xmlSAX2InitDefaultSAXHandler:
 * @hdlr:  the SAX handler
 * @warning:  flag if non-zero sets the handler warning procedure
 *
 * Initialize the default XML SAX2 handler
 */
void
xmlSAX2InitDefaultSAXHandler(xmlSAXHandler *hdlr, int warning)
{
    if ((hdlr == NULL) || (hdlr->initialized != 0))
	return;

    xmlSAXVersion(hdlr, xmlSAX2DefaultVersionValue);
    if (warning == 0)
	hdlr->warning = NULL;
    else
	hdlr->warning = xmlParserWarning;
}

/**
 * xmlDefaultSAXHandlerInit:
 *
 * DEPRECATED: This function is a no-op. Call xmlInitParser to
 * initialize the library.
 *
 * Initialize the default SAX2 handler
 */
void
xmlDefaultSAXHandlerInit(void)
{
}

#ifdef LIBXML_HTML_ENABLED

/**
 * xmlSAX2InitHtmlDefaultSAXHandler:
 * @hdlr:  the SAX handler
 *
 * Initialize the default HTML SAX2 handler
 */
void
xmlSAX2InitHtmlDefaultSAXHandler(xmlSAXHandler *hdlr)
{
    if ((hdlr == NULL) || (hdlr->initialized != 0))
	return;

    hdlr->internalSubset = xmlSAX2InternalSubset;
    hdlr->externalSubset = NULL;
    hdlr->isStandalone = NULL;
    hdlr->hasInternalSubset = NULL;
    hdlr->hasExternalSubset = NULL;
    hdlr->resolveEntity = NULL;
    hdlr->getEntity = xmlSAX2GetEntity;
    hdlr->getParameterEntity = NULL;
    hdlr->entityDecl = NULL;
    hdlr->attributeDecl = NULL;
    hdlr->elementDecl = NULL;
    hdlr->notationDecl = NULL;
    hdlr->unparsedEntityDecl = NULL;
    hdlr->setDocumentLocator = xmlSAX2SetDocumentLocator;
    hdlr->startDocument = xmlSAX2StartDocument;
    hdlr->endDocument = xmlSAX2EndDocument;
    hdlr->startElement = xmlSAX2StartElement;
    hdlr->endElement = xmlSAX2EndElement;
    hdlr->reference = NULL;
    hdlr->characters = xmlSAX2Characters;
    hdlr->cdataBlock = xmlSAX2CDataBlock;
    hdlr->ignorableWhitespace = xmlSAX2IgnorableWhitespace;
    hdlr->processingInstruction = xmlSAX2ProcessingInstruction;
    hdlr->comment = xmlSAX2Comment;
    hdlr->warning = xmlParserWarning;
    hdlr->error = xmlParserError;
    hdlr->fatalError = xmlParserError;

    hdlr->initialized = 1;
}

/**
 * htmlDefaultSAXHandlerInit:
 *
 * DEPRECATED: This function is a no-op. Call xmlInitParser to
 * initialize the library.
 */
void
htmlDefaultSAXHandlerInit(void)
{
}

#endif /* LIBXML_HTML_ENABLED */
