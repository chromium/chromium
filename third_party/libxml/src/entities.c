/*
 * entities.c : implementation for the XML entities handling
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

/* To avoid EBCDIC trouble when parsing on zOS */
#if defined(__MVS__)
#pragma convert("ISO8859-1")
#endif

#define IN_LIBXML
#include "libxml.h"

#include <string.h>
#include <stdlib.h>

#include <libxml/xmlmemory.h>
#include <libxml/hash.h>
#include <libxml/entities.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlerror.h>
#include <libxml/dict.h>
#include <libxml/xmlsave.h>

#include "private/entities.h"
#include "private/error.h"

/*
 * The XML predefined entities.
 */

static xmlEntity xmlEntityLt = {
    NULL, XML_ENTITY_DECL, BAD_CAST "lt",
    NULL, NULL, NULL, NULL, NULL, NULL,
    BAD_CAST "<", BAD_CAST "<", 1,
    XML_INTERNAL_PREDEFINED_ENTITY,
    NULL, NULL, NULL, NULL, 0, 0, 0
};
static xmlEntity xmlEntityGt = {
    NULL, XML_ENTITY_DECL, BAD_CAST "gt",
    NULL, NULL, NULL, NULL, NULL, NULL,
    BAD_CAST ">", BAD_CAST ">", 1,
    XML_INTERNAL_PREDEFINED_ENTITY,
    NULL, NULL, NULL, NULL, 0, 0, 0
};
static xmlEntity xmlEntityAmp = {
    NULL, XML_ENTITY_DECL, BAD_CAST "amp",
    NULL, NULL, NULL, NULL, NULL, NULL,
    BAD_CAST "&", BAD_CAST "&", 1,
    XML_INTERNAL_PREDEFINED_ENTITY,
    NULL, NULL, NULL, NULL, 0, 0, 0
};
static xmlEntity xmlEntityQuot = {
    NULL, XML_ENTITY_DECL, BAD_CAST "quot",
    NULL, NULL, NULL, NULL, NULL, NULL,
    BAD_CAST "\"", BAD_CAST "\"", 1,
    XML_INTERNAL_PREDEFINED_ENTITY,
    NULL, NULL, NULL, NULL, 0, 0, 0
};
static xmlEntity xmlEntityApos = {
    NULL, XML_ENTITY_DECL, BAD_CAST "apos",
    NULL, NULL, NULL, NULL, NULL, NULL,
    BAD_CAST "'", BAD_CAST "'", 1,
    XML_INTERNAL_PREDEFINED_ENTITY,
    NULL, NULL, NULL, NULL, 0, 0, 0
};

/*
 * xmlFreeEntity:
 * @entity:  an entity
 *
 * Frees the entity.
 */
void
xmlFreeEntity(xmlEntityPtr entity)
{
    xmlDictPtr dict = NULL;

    if (entity == NULL)
        return;

    if (entity->doc != NULL)
        dict = entity->doc->dict;


    if ((entity->children) &&
        (entity == (xmlEntityPtr) entity->children->parent))
        xmlFreeNodeList(entity->children);
    if ((entity->name != NULL) &&
        ((dict == NULL) || (!xmlDictOwns(dict, entity->name))))
        xmlFree((char *) entity->name);
    if (entity->ExternalID != NULL)
        xmlFree((char *) entity->ExternalID);
    if (entity->SystemID != NULL)
        xmlFree((char *) entity->SystemID);
    if (entity->URI != NULL)
        xmlFree((char *) entity->URI);
    if (entity->content != NULL)
        xmlFree((char *) entity->content);
    if (entity->orig != NULL)
        xmlFree((char *) entity->orig);
    xmlFree(entity);
}

/*
 * xmlCreateEntity:
 *
 * internal routine doing the entity node structures allocations
 */
static xmlEntityPtr
xmlCreateEntity(xmlDocPtr doc, const xmlChar *name, int type,
	        const xmlChar *ExternalID, const xmlChar *SystemID,
	        const xmlChar *content) {
    xmlEntityPtr ret;

    ret = (xmlEntityPtr) xmlMalloc(sizeof(xmlEntity));
    if (ret == NULL)
	return(NULL);
    memset(ret, 0, sizeof(xmlEntity));
    ret->doc = doc;
    ret->type = XML_ENTITY_DECL;

    /*
     * fill the structure.
     */
    ret->etype = (xmlEntityType) type;
    if ((doc == NULL) || (doc->dict == NULL))
	ret->name = xmlStrdup(name);
    else
        ret->name = xmlDictLookup(doc->dict, name, -1);
    if (ret->name == NULL)
        goto error;
    if (ExternalID != NULL) {
        ret->ExternalID = xmlStrdup(ExternalID);
        if (ret->ExternalID == NULL)
            goto error;
    }
    if (SystemID != NULL) {
        ret->SystemID = xmlStrdup(SystemID);
        if (ret->SystemID == NULL)
            goto error;
    }
    if (content != NULL) {
        ret->length = xmlStrlen(content);
	ret->content = xmlStrndup(content, ret->length);
        if (ret->content == NULL)
            goto error;
     } else {
        ret->length = 0;
        ret->content = NULL;
    }
    ret->URI = NULL; /* to be computed by the layer knowing
			the defining entity */
    ret->orig = NULL;

    return(ret);

error:
    xmlFreeEntity(ret);
    return(NULL);
}

/**
 * xmlAddEntity:
 * @doc:  the document
 * @extSubset:  add to the external or internal subset
 * @name:  the entity name
 * @type:  the entity type XML_xxx_yyy_ENTITY
 * @ExternalID:  the entity external ID if available
 * @SystemID:  the entity system ID if available
 * @content:  the entity content
 * @out:  pointer to resulting entity (optional)
 *
 * Register a new entity for this document.
 *
 * Available since 2.13.0.
 *
 * Returns an xmlParserErrors error code.
 */
int
xmlAddEntity(xmlDocPtr doc, int extSubset, const xmlChar *name, int type,
	  const xmlChar *ExternalID, const xmlChar *SystemID,
	  const xmlChar *content, xmlEntityPtr *out) {
    xmlDtdPtr dtd;
    xmlDictPtr dict = NULL;
    xmlEntitiesTablePtr table = NULL;
    xmlEntityPtr ret, predef;
    int res;

    if (out != NULL)
        *out = NULL;
    if ((doc == NULL) || (name == NULL))
	return(XML_ERR_ARGUMENT);
    dict = doc->dict;

    if (extSubset)
        dtd = doc->extSubset;
    else
        dtd = doc->intSubset;
    if (dtd == NULL)
        return(XML_DTD_NO_DTD);

    switch (type) {
        case XML_INTERNAL_GENERAL_ENTITY:
        case XML_EXTERNAL_GENERAL_PARSED_ENTITY:
        case XML_EXTERNAL_GENERAL_UNPARSED_ENTITY:
            predef = xmlGetPredefinedEntity(name);
            if (predef != NULL) {
                int valid = 0;

                /* 4.6 Predefined Entities */
                if ((type == XML_INTERNAL_GENERAL_ENTITY) &&
                    (content != NULL)) {
                    int c = predef->content[0];

                    if (((content[0] == c) && (content[1] == 0)) &&
                        ((c == '>') || (c == '\'') || (c == '"'))) {
                        valid = 1;
                    } else if ((content[0] == '&') && (content[1] == '#')) {
                        if (content[2] == 'x') {
                            xmlChar *hex = BAD_CAST "0123456789ABCDEF";
                            xmlChar ref[] = "00;";

                            ref[0] = hex[c / 16 % 16];
                            ref[1] = hex[c % 16];
                            if (xmlStrcasecmp(&content[3], ref) == 0)
                                valid = 1;
                        } else {
                            xmlChar ref[] = "00;";

                            ref[0] = '0' + c / 10 % 10;
                            ref[1] = '0' + c % 10;
                            if (xmlStrEqual(&content[2], ref))
                                valid = 1;
                        }
                    }
                }
                if (!valid)
                    return(XML_ERR_REDECL_PREDEF_ENTITY);
            }
	    if (dtd->entities == NULL) {
		dtd->entities = xmlHashCreateDict(0, dict);
                if (dtd->entities == NULL)
                    return(XML_ERR_NO_MEMORY);
            }
	    table = dtd->entities;
	    break;
        case XML_INTERNAL_PARAMETER_ENTITY:
        case XML_EXTERNAL_PARAMETER_ENTITY:
	    if (dtd->pentities == NULL) {
		dtd->pentities = xmlHashCreateDict(0, dict);
                if (dtd->pentities == NULL)
                    return(XML_ERR_NO_MEMORY);
            }
	    table = dtd->pentities;
	    break;
        default:
	    return(XML_ERR_ARGUMENT);
    }
    ret = xmlCreateEntity(dtd->doc, name, type, ExternalID, SystemID, content);
    if (ret == NULL)
        return(XML_ERR_NO_MEMORY);

    res = xmlHashAdd(table, name, ret);
    if (res < 0) {
        xmlFreeEntity(ret);
        return(XML_ERR_NO_MEMORY);
    } else if (res == 0) {
	/*
	 * entity was already defined at another level.
	 */
        xmlFreeEntity(ret);
	return(XML_WAR_ENTITY_REDEFINED);
    }

    /*
     * Link it to the DTD
     */
    ret->parent = dtd;
    ret->doc = dtd->doc;
    if (dtd->last == NULL) {
	dtd->children = dtd->last = (xmlNodePtr) ret;
    } else {
	dtd->last->next = (xmlNodePtr) ret;
	ret->prev = dtd->last;
	dtd->last = (xmlNodePtr) ret;
    }

    if (out != NULL)
        *out = ret;
    return(0);
}

/**
 * xmlGetPredefinedEntity:
 * @name:  the entity name
 *
 * Check whether this name is an predefined entity.
 *
 * Returns NULL if not, otherwise the entity
 */
xmlEntityPtr
xmlGetPredefinedEntity(const xmlChar *name) {
    if (name == NULL) return(NULL);
    switch (name[0]) {
        case 'l':
	    if (xmlStrEqual(name, BAD_CAST "lt"))
	        return(&xmlEntityLt);
	    break;
        case 'g':
	    if (xmlStrEqual(name, BAD_CAST "gt"))
	        return(&xmlEntityGt);
	    break;
        case 'a':
	    if (xmlStrEqual(name, BAD_CAST "amp"))
	        return(&xmlEntityAmp);
	    if (xmlStrEqual(name, BAD_CAST "apos"))
	        return(&xmlEntityApos);
	    break;
        case 'q':
	    if (xmlStrEqual(name, BAD_CAST "quot"))
	        return(&xmlEntityQuot);
	    break;
	default:
	    break;
    }
    return(NULL);
}

/**
 * xmlAddDtdEntity:
 * @doc:  the document
 * @name:  the entity name
 * @type:  the entity type XML_xxx_yyy_ENTITY
 * @ExternalID:  the entity external ID if available
 * @SystemID:  the entity system ID if available
 * @content:  the entity content
 *
 * Register a new entity for this document DTD external subset.
 *
 * Returns a pointer to the entity or NULL in case of error
 */
xmlEntityPtr
xmlAddDtdEntity(xmlDocPtr doc, const xmlChar *name, int type,
	        const xmlChar *ExternalID, const xmlChar *SystemID,
		const xmlChar *content) {
    xmlEntityPtr ret;

    xmlAddEntity(doc, 1, name, type, ExternalID, SystemID, content, &ret);
    return(ret);
}

/**
 * xmlAddDocEntity:
 * @doc:  the document
 * @name:  the entity name
 * @type:  the entity type XML_xxx_yyy_ENTITY
 * @ExternalID:  the entity external ID if available
 * @SystemID:  the entity system ID if available
 * @content:  the entity content
 *
 * Register a new entity for this document.
 *
 * Returns a pointer to the entity or NULL in case of error
 */
xmlEntityPtr
xmlAddDocEntity(xmlDocPtr doc, const xmlChar *name, int type,
	        const xmlChar *ExternalID, const xmlChar *SystemID,
	        const xmlChar *content) {
    xmlEntityPtr ret;

    xmlAddEntity(doc, 0, name, type, ExternalID, SystemID, content, &ret);
    return(ret);
}

/**
 * xmlNewEntity:
 * @doc:  the document
 * @name:  the entity name
 * @type:  the entity type XML_xxx_yyy_ENTITY
 * @ExternalID:  the entity external ID if available
 * @SystemID:  the entity system ID if available
 * @content:  the entity content
 *
 * Create a new entity, this differs from xmlAddDocEntity() that if
 * the document is NULL or has no internal subset defined, then an
 * unlinked entity structure will be returned, it is then the responsibility
 * of the caller to link it to the document later or free it when not needed
 * anymore.
 *
 * Returns a pointer to the entity or NULL in case of error
 */
xmlEntityPtr
xmlNewEntity(xmlDocPtr doc, const xmlChar *name, int type,
	     const xmlChar *ExternalID, const xmlChar *SystemID,
	     const xmlChar *content) {
    if ((doc != NULL) && (doc->intSubset != NULL)) {
	return(xmlAddDocEntity(doc, name, type, ExternalID, SystemID, content));
    }
    if (name == NULL)
        return(NULL);
    return(xmlCreateEntity(doc, name, type, ExternalID, SystemID, content));
}

/**
 * xmlGetEntityFromTable:
 * @table:  an entity table
 * @name:  the entity name
 * @parameter:  look for parameter entities
 *
 * Do an entity lookup in the table.
 * returns the corresponding parameter entity, if found.
 *
 * Returns A pointer to the entity structure or NULL if not found.
 */
static xmlEntityPtr
xmlGetEntityFromTable(xmlEntitiesTablePtr table, const xmlChar *name) {
    return((xmlEntityPtr) xmlHashLookup(table, name));
}

/**
 * xmlGetParameterEntity:
 * @doc:  the document referencing the entity
 * @name:  the entity name
 *
 * Do an entity lookup in the internal and external subsets and
 * returns the corresponding parameter entity, if found.
 *
 * Returns A pointer to the entity structure or NULL if not found.
 */
xmlEntityPtr
xmlGetParameterEntity(xmlDocPtr doc, const xmlChar *name) {
    xmlEntitiesTablePtr table;
    xmlEntityPtr ret;

    if (doc == NULL)
	return(NULL);
    if ((doc->intSubset != NULL) && (doc->intSubset->pentities != NULL)) {
	table = (xmlEntitiesTablePtr) doc->intSubset->pentities;
	ret = xmlGetEntityFromTable(table, name);
	if (ret != NULL)
	    return(ret);
    }
    if ((doc->extSubset != NULL) && (doc->extSubset->pentities != NULL)) {
	table = (xmlEntitiesTablePtr) doc->extSubset->pentities;
	return(xmlGetEntityFromTable(table, name));
    }
    return(NULL);
}

/**
 * xmlGetDtdEntity:
 * @doc:  the document referencing the entity
 * @name:  the entity name
 *
 * Do an entity lookup in the DTD entity hash table and
 * returns the corresponding entity, if found.
 * Note: the first argument is the document node, not the DTD node.
 *
 * Returns A pointer to the entity structure or NULL if not found.
 */
xmlEntityPtr
xmlGetDtdEntity(xmlDocPtr doc, const xmlChar *name) {
    xmlEntitiesTablePtr table;

    if (doc == NULL)
	return(NULL);
    if ((doc->extSubset != NULL) && (doc->extSubset->entities != NULL)) {
	table = (xmlEntitiesTablePtr) doc->extSubset->entities;
	return(xmlGetEntityFromTable(table, name));
    }
    return(NULL);
}

/**
 * xmlGetDocEntity:
 * @doc:  the document referencing the entity
 * @name:  the entity name
 *
 * Do an entity lookup in the document entity hash table and
 * returns the corresponding entity, otherwise a lookup is done
 * in the predefined entities too.
 *
 * Returns A pointer to the entity structure or NULL if not found.
 */
xmlEntityPtr
xmlGetDocEntity(const xmlDoc *doc, const xmlChar *name) {
    xmlEntityPtr cur;
    xmlEntitiesTablePtr table;

    if (doc != NULL) {
	if ((doc->intSubset != NULL) && (doc->intSubset->entities != NULL)) {
	    table = (xmlEntitiesTablePtr) doc->intSubset->entities;
	    cur = xmlGetEntityFromTable(table, name);
	    if (cur != NULL)
		return(cur);
	}
	if (doc->standalone != 1) {
	    if ((doc->extSubset != NULL) &&
		(doc->extSubset->entities != NULL)) {
		table = (xmlEntitiesTablePtr) doc->extSubset->entities;
		cur = xmlGetEntityFromTable(table, name);
		if (cur != NULL)
		    return(cur);
	    }
	}
    }
    return(xmlGetPredefinedEntity(name));
}

/*
 * Macro used to grow the current buffer.
 */
#define growBufferReentrant() {						\
    xmlChar *tmp;                                                       \
    size_t new_size = buffer_size * 2;                                  \
    if (new_size < buffer_size) goto mem_error;                         \
    tmp = (xmlChar *) xmlRealloc(buffer, new_size);	                \
    if (tmp == NULL) goto mem_error;                                    \
    buffer = tmp;							\
    buffer_size = new_size;						\
}

/**
 * xmlEncodeEntitiesInternal:
 * @doc:  the document containing the string
 * @input:  A string to convert to XML.
 * @attr: are we handling an attribute value
 *
 * Do a global encoding of a string, replacing the predefined entities
 * and non ASCII values with their entities and CharRef counterparts.
 * Contrary to xmlEncodeEntities, this routine is reentrant, and result
 * must be deallocated.
 *
 * Returns A newly allocated string with the substitution done.
 */
static xmlChar *
xmlEncodeEntitiesInternal(xmlDocPtr doc, const xmlChar *input, int attr) {
    const xmlChar *cur = input;
    xmlChar *buffer = NULL;
    xmlChar *out = NULL;
    size_t buffer_size = 0;
    int html = 0;

    if (input == NULL) return(NULL);
    if (doc != NULL)
        html = (doc->type == XML_HTML_DOCUMENT_NODE);

    /*
     * allocate an translation buffer.
     */
    buffer_size = 1000;
    buffer = (xmlChar *) xmlMalloc(buffer_size);
    if (buffer == NULL)
	return(NULL);
    out = buffer;

    while (*cur != '\0') {
        size_t indx = out - buffer;
        if (indx + 100 > buffer_size) {

	    growBufferReentrant();
	    out = &buffer[indx];
	}

	/*
	 * By default one have to encode at least '<', '>', '"' and '&' !
	 */
	if (*cur == '<') {
	    const xmlChar *end;

	    /*
	     * Special handling of server side include in HTML attributes
	     */
	    if (html && attr &&
	        (cur[1] == '!') && (cur[2] == '-') && (cur[3] == '-') &&
	        ((end = xmlStrstr(cur, BAD_CAST "-->")) != NULL)) {
	        while (cur != end) {
		    *out++ = *cur++;
		    indx = out - buffer;
		    if (indx + 100 > buffer_size) {
			growBufferReentrant();
			out = &buffer[indx];
		    }
		}
		*out++ = *cur++;
		*out++ = *cur++;
		*out++ = *cur++;
		continue;
	    }
	    *out++ = '&';
	    *out++ = 'l';
	    *out++ = 't';
	    *out++ = ';';
	} else if (*cur == '>') {
	    *out++ = '&';
	    *out++ = 'g';
	    *out++ = 't';
	    *out++ = ';';
	} else if (*cur == '&') {
	    /*
	     * Special handling of &{...} construct from HTML 4, see
	     * http://www.w3.org/TR/html401/appendix/notes.html#h-B.7.1
	     */
	    if (html && attr && (cur[1] == '{') &&
	        (strchr((const char *) cur, '}'))) {
	        while (*cur != '}') {
		    *out++ = *cur++;
		    indx = out - buffer;
		    if (indx + 100 > buffer_size) {
			growBufferReentrant();
			out = &buffer[indx];
		    }
		}
		*out++ = *cur++;
		continue;
	    }
	    *out++ = '&';
	    *out++ = 'a';
	    *out++ = 'm';
	    *out++ = 'p';
	    *out++ = ';';
	} else if (((*cur >= 0x20) && (*cur < 0x80)) ||
	    (*cur == '\n') || (*cur == '\t') || ((html) && (*cur == '\r'))) {
	    /*
	     * default case, just copy !
	     */
	    *out++ = *cur;
	} else if (*cur >= 0x80) {
	    if (((doc != NULL) && (doc->encoding != NULL)) || (html)) {
		/*
		 * Bjørn Reese <br@sseusa.com> provided the patch
	        xmlChar xc;
	        xc = (*cur & 0x3F) << 6;
	        if (cur[1] != 0) {
		    xc += *(++cur) & 0x3F;
		    *out++ = xc;
	        } else
		 */
		*out++ = *cur;
	    } else {
		/*
		 * We assume we have UTF-8 input.
		 * It must match either:
		 *   110xxxxx 10xxxxxx
		 *   1110xxxx 10xxxxxx 10xxxxxx
		 *   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		 * That is:
		 *   cur[0] is 11xxxxxx
		 *   cur[1] is 10xxxxxx
		 *   cur[2] is 10xxxxxx if cur[0] is 111xxxxx
		 *   cur[3] is 10xxxxxx if cur[0] is 1111xxxx
		 *   cur[0] is not 11111xxx
		 */
		char buf[13], *ptr;
		int val, l;

                l = 4;
                val = xmlGetUTF8Char(cur, &l);
                if (val < 0) {
                    val = 0xFFFD;
                    cur++;
                } else {
                    if (!IS_CHAR(val))
                        val = 0xFFFD;
                    cur += l;
		}
		/*
		 * We could do multiple things here. Just save as a char ref
		 */
		snprintf(buf, sizeof(buf), "&#x%X;", val);
		buf[sizeof(buf) - 1] = 0;
		ptr = buf;
		while (*ptr != 0) *out++ = *ptr++;
		continue;
	    }
	} else if (IS_BYTE_CHAR(*cur)) {
	    char buf[11], *ptr;

	    snprintf(buf, sizeof(buf), "&#%d;", *cur);
	    buf[sizeof(buf) - 1] = 0;
            ptr = buf;
	    while (*ptr != 0) *out++ = *ptr++;
	}
	cur++;
    }
    *out = 0;
    return(buffer);

mem_error:
    xmlFree(buffer);
    return(NULL);
}

/**
 * xmlEncodeAttributeEntities:
 * @doc:  the document containing the string
 * @input:  A string to convert to XML.
 *
 * Do a global encoding of a string, replacing the predefined entities
 * and non ASCII values with their entities and CharRef counterparts for
 * attribute values.
 *
 * Returns A newly allocated string with the substitution done.
 */
xmlChar *
xmlEncodeAttributeEntities(xmlDocPtr doc, const xmlChar *input) {
    return xmlEncodeEntitiesInternal(doc, input, 1);
}

/**
 * xmlEncodeEntitiesReentrant:
 * @doc:  the document containing the string
 * @input:  A string to convert to XML.
 *
 * Do a global encoding of a string, replacing the predefined entities
 * and non ASCII values with their entities and CharRef counterparts.
 * Contrary to xmlEncodeEntities, this routine is reentrant, and result
 * must be deallocated.
 *
 * Returns A newly allocated string with the substitution done.
 */
xmlChar *
xmlEncodeEntitiesReentrant(xmlDocPtr doc, const xmlChar *input) {
    return xmlEncodeEntitiesInternal(doc, input, 0);
}

/**
 * xmlEncodeSpecialChars:
 * @doc:  the document containing the string
 * @input:  A string to convert to XML.
 *
 * Do a global encoding of a string, replacing the predefined entities
 * this routine is reentrant, and result must be deallocated.
 *
 * Returns A newly allocated string with the substitution done.
 */
xmlChar *
xmlEncodeSpecialChars(const xmlDoc *doc ATTRIBUTE_UNUSED, const xmlChar *input) {
    const xmlChar *cur = input;
    xmlChar *buffer = NULL;
    xmlChar *out = NULL;
    size_t buffer_size = 0;
    if (input == NULL) return(NULL);

    /*
     * allocate an translation buffer.
     */
    buffer_size = 1000;
    buffer = (xmlChar *) xmlMalloc(buffer_size);
    if (buffer == NULL)
	return(NULL);
    out = buffer;

    while (*cur != '\0') {
        size_t indx = out - buffer;
        if (indx + 10 > buffer_size) {

	    growBufferReentrant();
	    out = &buffer[indx];
	}

	/*
	 * By default one have to encode at least '<', '>', '"' and '&' !
	 */
	if (*cur == '<') {
	    *out++ = '&';
	    *out++ = 'l';
	    *out++ = 't';
	    *out++ = ';';
	} else if (*cur == '>') {
	    *out++ = '&';
	    *out++ = 'g';
	    *out++ = 't';
	    *out++ = ';';
	} else if (*cur == '&') {
	    *out++ = '&';
	    *out++ = 'a';
	    *out++ = 'm';
	    *out++ = 'p';
	    *out++ = ';';
	} else if (*cur == '"') {
	    *out++ = '&';
	    *out++ = 'q';
	    *out++ = 'u';
	    *out++ = 'o';
	    *out++ = 't';
	    *out++ = ';';
	} else if (*cur == '\r') {
	    *out++ = '&';
	    *out++ = '#';
	    *out++ = '1';
	    *out++ = '3';
	    *out++ = ';';
	} else {
	    /*
	     * Works because on UTF-8, all extended sequences cannot
	     * result in bytes in the ASCII range.
	     */
	    *out++ = *cur;
	}
	cur++;
    }
    *out = 0;
    return(buffer);

mem_error:
    xmlFree(buffer);
    return(NULL);
}

/**
 * xmlCreateEntitiesTable:
 *
 * create and initialize an empty entities hash table.
 * This really doesn't make sense and should be deprecated
 *
 * Returns the xmlEntitiesTablePtr just created or NULL in case of error.
 */
xmlEntitiesTablePtr
xmlCreateEntitiesTable(void) {
    return((xmlEntitiesTablePtr) xmlHashCreate(0));
}

/**
 * xmlFreeEntityWrapper:
 * @entity:  An entity
 * @name:  its name
 *
 * Deallocate the memory used by an entities in the hash table.
 */
static void
xmlFreeEntityWrapper(void *entity, const xmlChar *name ATTRIBUTE_UNUSED) {
    if (entity != NULL)
	xmlFreeEntity((xmlEntityPtr) entity);
}

/**
 * xmlFreeEntitiesTable:
 * @table:  An entity table
 *
 * Deallocate the memory used by an entities hash table.
 */
void
xmlFreeEntitiesTable(xmlEntitiesTablePtr table) {
    xmlHashFree(table, xmlFreeEntityWrapper);
}

#ifdef LIBXML_TREE_ENABLED
/**
 * xmlCopyEntity:
 * @ent:  An entity
 *
 * Build a copy of an entity
 *
 * Returns the new xmlEntitiesPtr or NULL in case of error.
 */
static void *
xmlCopyEntity(void *payload, const xmlChar *name ATTRIBUTE_UNUSED) {
    xmlEntityPtr ent = (xmlEntityPtr) payload;
    xmlEntityPtr cur;

    cur = (xmlEntityPtr) xmlMalloc(sizeof(xmlEntity));
    if (cur == NULL)
	return(NULL);
    memset(cur, 0, sizeof(xmlEntity));
    cur->type = XML_ENTITY_DECL;

    cur->etype = ent->etype;
    if (ent->name != NULL) {
	cur->name = xmlStrdup(ent->name);
        if (cur->name == NULL)
            goto error;
    }
    if (ent->ExternalID != NULL) {
	cur->ExternalID = xmlStrdup(ent->ExternalID);
        if (cur->ExternalID == NULL)
            goto error;
    }
    if (ent->SystemID != NULL) {
	cur->SystemID = xmlStrdup(ent->SystemID);
        if (cur->SystemID == NULL)
            goto error;
    }
    if (ent->content != NULL) {
	cur->content = xmlStrdup(ent->content);
        if (cur->content == NULL)
            goto error;
    }
    if (ent->orig != NULL) {
	cur->orig = xmlStrdup(ent->orig);
        if (cur->orig == NULL)
            goto error;
    }
    if (ent->URI != NULL) {
	cur->URI = xmlStrdup(ent->URI);
        if (cur->URI == NULL)
            goto error;
    }
    return(cur);

error:
    xmlFreeEntity(cur);
    return(NULL);
}

/**
 * xmlCopyEntitiesTable:
 * @table:  An entity table
 *
 * Build a copy of an entity table.
 *
 * Returns the new xmlEntitiesTablePtr or NULL in case of error.
 */
xmlEntitiesTablePtr
xmlCopyEntitiesTable(xmlEntitiesTablePtr table) {
    return(xmlHashCopySafe(table, xmlCopyEntity, xmlFreeEntityWrapper));
}
#endif /* LIBXML_TREE_ENABLED */

#ifdef LIBXML_OUTPUT_ENABLED

/**
 * xmlDumpEntityDecl:
 * @buf:  An XML buffer.
 * @ent:  An entity table
 *
 * This will dump the content of the entity table as an XML DTD definition
 */
void
xmlDumpEntityDecl(xmlBufferPtr buf, xmlEntityPtr ent) {
    xmlSaveCtxtPtr save;

    if ((buf == NULL) || (ent == NULL))
        return;

    save = xmlSaveToBuffer(buf, NULL, 0);
    xmlSaveTree(save, (xmlNodePtr) ent);
    if (xmlSaveFinish(save) != XML_ERR_OK)
        xmlFree(xmlBufferDetach(buf));
}

/**
 * xmlDumpEntityDeclScan:
 * @ent:  An entity table
 * @buf:  An XML buffer.
 *
 * When using the hash table scan function, arguments need to be reversed
 */
static void
xmlDumpEntityDeclScan(void *ent, void *save,
                      const xmlChar *name ATTRIBUTE_UNUSED) {
    xmlSaveTree(save, ent);
}

/**
 * xmlDumpEntitiesTable:
 * @buf:  An XML buffer.
 * @table:  An entity table
 *
 * This will dump the content of the entity table as an XML DTD definition
 */
void
xmlDumpEntitiesTable(xmlBufferPtr buf, xmlEntitiesTablePtr table) {
    xmlSaveCtxtPtr save;

    if ((buf == NULL) || (table == NULL))
        return;

    save = xmlSaveToBuffer(buf, NULL, 0);
    xmlHashScan(table, xmlDumpEntityDeclScan, save);
    if (xmlSaveFinish(save) != XML_ERR_OK)
        xmlFree(xmlBufferDetach(buf));
}
#endif /* LIBXML_OUTPUT_ENABLED */
