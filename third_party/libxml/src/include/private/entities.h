#ifndef XML_ENTITIES_H_PRIVATE__
#define XML_ENTITIES_H_PRIVATE__

#include <libxml/tree.h>
#include <libxml/xmlstring.h>

XML_HIDDEN xmlChar *
xmlEncodeAttributeEntities(xmlDocPtr doc, const xmlChar *input);

#endif /* XML_ENTITIES_H_PRIVATE__ */
