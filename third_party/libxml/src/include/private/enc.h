#ifndef XML_ENC_H_PRIVATE__
#define XML_ENC_H_PRIVATE__

#include <libxml/encoding.h>
#include <libxml/tree.h>

int xmlCharEncFirstLineInput(xmlParserInputBufferPtr input, int len);
int xmlCharEncInput(xmlParserInputBufferPtr input, int flush);
int xmlCharEncOutput(xmlOutputBufferPtr output, int init);

#endif /* XML_ENC_H_PRIVATE__ */
