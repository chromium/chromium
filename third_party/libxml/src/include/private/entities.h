#ifndef XML_ENTITIES_H_PRIVATE__
#define XML_ENTITIES_H_PRIVATE__

#include <libxml/tree.h>
#include <libxml/xmlstring.h>

/*
 * Entity flags
 *
 * XML_ENT_PARSED: The entity was parsed and `children` points to the
 * content.
 * XML_ENT_CHECKED: The entity was checked for loops.
 */
#define XML_ENT_PARSED      (1<<0)
#define XML_ENT_CHECKED     (1<<1)
#define XML_ENT_EXPANDING   (1<<2)
#define XML_ENT_CHECKED_LT  (1<<3)
#define XML_ENT_CONTAINS_LT (1<<4)

XML_HIDDEN xmlChar *
xmlEncodeAttributeEntities(xmlDocPtr doc, const xmlChar *input);

#endif /* XML_ENTITIES_H_PRIVATE__ */
