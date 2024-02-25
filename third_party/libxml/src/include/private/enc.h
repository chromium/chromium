#ifndef XML_ENC_H_PRIVATE__
#define XML_ENC_H_PRIVATE__

#include <libxml/encoding.h>
#include <libxml/tree.h>

XML_HIDDEN void
xmlInitEncodingInternal(void);

XML_HIDDEN int
xmlEncInputChunk(xmlCharEncodingHandler *handler, unsigned char *out,
                 int *outlen, const unsigned char *in, int *inlen);
XML_HIDDEN int
xmlCharEncInput(xmlParserInputBufferPtr input);
XML_HIDDEN int
xmlCharEncOutput(xmlOutputBufferPtr output, int init);

#endif /* XML_ENC_H_PRIVATE__ */
