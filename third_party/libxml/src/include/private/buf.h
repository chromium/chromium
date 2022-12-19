#ifndef XML_BUF_H_PRIVATE__
#define XML_BUF_H_PRIVATE__

#include <libxml/tree.h>

XML_HIDDEN xmlBufPtr
xmlBufCreate(void);
XML_HIDDEN xmlBufPtr
xmlBufCreateSize(size_t size);

XML_HIDDEN int
xmlBufSetAllocationScheme(xmlBufPtr buf, xmlBufferAllocationScheme scheme);
XML_HIDDEN int
xmlBufGetAllocationScheme(xmlBufPtr buf);

XML_HIDDEN void
xmlBufFree(xmlBufPtr buf);
XML_HIDDEN void
xmlBufEmpty(xmlBufPtr buf);

/* size_t xmlBufShrink(xmlBufPtr buf, size_t len); */
XML_HIDDEN int
xmlBufGrow(xmlBufPtr buf, int len);
XML_HIDDEN int
xmlBufResize(xmlBufPtr buf, size_t len);

XML_HIDDEN int
xmlBufAdd(xmlBufPtr buf, const xmlChar *str, int len);
XML_HIDDEN int
xmlBufCat(xmlBufPtr buf, const xmlChar *str);
XML_HIDDEN int
xmlBufCCat(xmlBufPtr buf, const char *str);
XML_HIDDEN int
xmlBufWriteQuotedString(xmlBufPtr buf, const xmlChar *string);

XML_HIDDEN size_t
xmlBufAvail(const xmlBufPtr buf);
XML_HIDDEN size_t
xmlBufLength(const xmlBufPtr buf);
/* size_t xmlBufUse(const xmlBufPtr buf); */
XML_HIDDEN int
xmlBufIsEmpty(const xmlBufPtr buf);
XML_HIDDEN int
xmlBufAddLen(xmlBufPtr buf, size_t len);

/* const xmlChar * xmlBufContent(const xmlBuf *buf); */
/* const xmlChar * xmlBufEnd(xmlBufPtr buf); */

XML_HIDDEN xmlChar *
xmlBufDetach(xmlBufPtr buf);

XML_HIDDEN size_t
xmlBufDump(FILE *file, xmlBufPtr buf);

XML_HIDDEN xmlBufPtr
xmlBufFromBuffer(xmlBufferPtr buffer);
XML_HIDDEN xmlBufferPtr
xmlBufBackToBuffer(xmlBufPtr buf);
XML_HIDDEN int
xmlBufMergeBuffer(xmlBufPtr buf, xmlBufferPtr buffer);

XML_HIDDEN int
xmlBufResetInput(xmlBufPtr buf, xmlParserInputPtr input);
XML_HIDDEN size_t
xmlBufGetInputBase(xmlBufPtr buf, xmlParserInputPtr input);
XML_HIDDEN int
xmlBufSetInputBaseCur(xmlBufPtr buf, xmlParserInputPtr input,
                      size_t base, size_t cur);

#endif /* XML_BUF_H_PRIVATE__ */
