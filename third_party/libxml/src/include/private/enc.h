#ifndef XML_ENC_H_PRIVATE__
#define XML_ENC_H_PRIVATE__

#include <libxml/encoding.h>
#include <libxml/tree.h>

XML_HIDDEN void
xmlInitEncodingInternal(void);

XML_HIDDEN int
xmlCharEncFirstLineInput(xmlParserInputBufferPtr input, int len);
XML_HIDDEN int
xmlCharEncInput(xmlParserInputBufferPtr input, int flush);
XML_HIDDEN int
xmlCharEncOutput(xmlOutputBufferPtr output, int init);

#endif /* XML_ENC_H_PRIVATE__ */
