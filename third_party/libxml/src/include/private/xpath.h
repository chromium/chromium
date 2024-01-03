#ifndef XML_XPATH_H_PRIVATE__
#define XML_XPATH_H_PRIVATE__

#include <libxml/xpath.h>

XML_HIDDEN void
xmlInitXPathInternal(void);

#ifdef LIBXML_XPATH_ENABLED
XML_HIDDEN void
xmlXPathErrMemory(xmlXPathContextPtr ctxt, const char *extra);
XML_HIDDEN void
xmlXPathPErrMemory(xmlXPathParserContextPtr ctxt, const char *extra);
#endif

#endif /* XML_XPATH_H_PRIVATE__ */
