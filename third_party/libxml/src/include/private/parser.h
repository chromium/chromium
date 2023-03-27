#ifndef XML_PARSER_H_PRIVATE__
#define XML_PARSER_H_PRIVATE__

#include <libxml/parser.h>
#include <libxml/xmlversion.h>

/**
 * XML_VCTXT_DTD_VALIDATED:
 *
 * Set after xmlValidateDtdFinal was called.
 */
#define XML_VCTXT_DTD_VALIDATED (1u << 0)
/**
 * XML_VCTXT_USE_PCTXT:
 *
 * Set if the validation context is part of a parser context.
 */
#define XML_VCTXT_USE_PCTXT (1u << 1)

XML_HIDDEN void
xmlErrMemory(xmlParserCtxtPtr ctxt, const char *extra);
XML_HIDDEN void
__xmlErrEncoding(xmlParserCtxtPtr ctxt, xmlParserErrors xmlerr,
                 const char *msg, const xmlChar *str1,
                 const xmlChar *str2) LIBXML_ATTR_FORMAT(3,0);
XML_HIDDEN void
xmlHaltParser(xmlParserCtxtPtr ctxt);
XML_HIDDEN int
xmlParserGrow(xmlParserCtxtPtr ctxt);
XML_HIDDEN void
xmlParserShrink(xmlParserCtxtPtr ctxt);

#endif /* XML_PARSER_H_PRIVATE__ */
