#ifndef XML_SAVE_H_PRIVATE__
#define XML_SAVE_H_PRIVATE__

#include <libxml/tree.h>
#include <libxml/xmlversion.h>

#ifdef LIBXML_OUTPUT_ENABLED

void xmlBufAttrSerializeTxtContent(xmlBufPtr buf, xmlDocPtr doc,
                                   xmlAttrPtr attr, const xmlChar * string);
void xmlNsListDumpOutput(xmlOutputBufferPtr buf, xmlNsPtr cur);

#endif /* LIBXML_OUTPUT_ENABLED */

#endif /* XML_SAVE_H_PRIVATE__ */

