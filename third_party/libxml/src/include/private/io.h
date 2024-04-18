#ifndef XML_IO_H_PRIVATE__
#define XML_IO_H_PRIVATE__

#include <libxml/encoding.h>
#include <libxml/tree.h>
#include <libxml/xmlversion.h>

XML_HIDDEN void
xmlInitIOCallbacks(void);

XML_HIDDEN int
__xmlIOErr(int domain, int code, const char *extra);

XML_HIDDEN int
xmlNoNetExists(const char *filename);

XML_HIDDEN int
xmlParserInputBufferCreateFilenameSafe(const char *URI, xmlCharEncoding enc,
                                       xmlParserInputBufferPtr *out);

XML_HIDDEN xmlParserInputBufferPtr
xmlNewInputBufferString(const char *str, int flags);
XML_HIDDEN xmlParserInputBufferPtr
xmlNewInputBufferMemory(const void *mem, size_t size, int flags,
                        xmlCharEncoding enc);

#ifdef LIBXML_OUTPUT_ENABLED
XML_HIDDEN xmlOutputBufferPtr
xmlAllocOutputBufferInternal(xmlCharEncodingHandlerPtr encoder);
XML_HIDDEN void
xmlOutputBufferWriteQuotedString(xmlOutputBufferPtr buf,
                                 const xmlChar *string);
#endif

#endif /* XML_IO_H_PRIVATE__ */
