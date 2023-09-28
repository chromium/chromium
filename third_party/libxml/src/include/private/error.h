#ifndef XML_ERROR_H_PRIVATE__
#define XML_ERROR_H_PRIVATE__

#include <libxml/xmlerror.h>
#include <libxml/xmlversion.h>

struct _xmlNode;

XML_HIDDEN void
__xmlRaiseError(xmlStructuredErrorFunc schannel,
                xmlGenericErrorFunc channel, void *data, void *ctx,
                void *nod, int domain, int code, xmlErrorLevel level,
                const char *file, int line, const char *str1,
                const char *str2, const char *str3, int int1, int col,
	        const char *msg, ...) LIBXML_ATTR_FORMAT(16,17);
XML_HIDDEN void
__xmlSimpleError(int domain, int code, struct _xmlNode *node,
                 const char *msg, const char *extra) LIBXML_ATTR_FORMAT(4,0);
XML_HIDDEN void
xmlGenericErrorDefaultFunc(void *ctx, const char *msg,
                           ...) LIBXML_ATTR_FORMAT(2,3);

#endif /* XML_ERROR_H_PRIVATE__ */
