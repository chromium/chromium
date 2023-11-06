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

#define XML_INPUT_HAS_ENCODING      (1u << 0)
#define XML_INPUT_AUTO_ENCODING     (7u << 1)
#define XML_INPUT_AUTO_UTF8         (1u << 1)
#define XML_INPUT_AUTO_UTF16LE      (2u << 1)
#define XML_INPUT_AUTO_UTF16BE      (3u << 1)
#define XML_INPUT_AUTO_OTHER        (4u << 1)
#define XML_INPUT_USES_ENC_DECL     (1u << 4)
#define XML_INPUT_ENCODING_ERROR    (1u << 5)

XML_HIDDEN void
xmlErrMemory(xmlParserCtxtPtr ctxt, const char *extra);
XML_HIDDEN void
xmlFatalErr(xmlParserCtxtPtr ctxt, xmlParserErrors error, const char *info);
XML_HIDDEN void LIBXML_ATTR_FORMAT(3,0)
xmlWarningMsg(xmlParserCtxtPtr ctxt, xmlParserErrors error,
              const char *msg, const xmlChar *str1, const xmlChar *str2);
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

XML_HIDDEN void
xmlDetectEncoding(xmlParserCtxtPtr ctxt);
XML_HIDDEN void
xmlSetDeclaredEncoding(xmlParserCtxtPtr ctxt, xmlChar *encoding);

XML_HIDDEN xmlParserNsData *
xmlParserNsCreate(void);
XML_HIDDEN void
xmlParserNsFree(xmlParserNsData *nsdb);
/*
 * These functions allow SAX handlers to attach extra data to namespaces
 * efficiently and should be made public.
 */
XML_HIDDEN int
xmlParserNsUpdateSax(xmlParserCtxtPtr ctxt, const xmlChar *prefix,
                     void *saxData);
XML_HIDDEN void *
xmlParserNsLookupSax(xmlParserCtxtPtr ctxt, const xmlChar *prefix);

#endif /* XML_PARSER_H_PRIVATE__ */
