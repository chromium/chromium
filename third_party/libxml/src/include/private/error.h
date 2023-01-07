#ifndef XML_ERROR_H_PRIVATE__
#define XML_ERROR_H_PRIVATE__

#include <libxml/xmlerror.h>
#include <libxml/xmlversion.h>

void
__xmlRaiseError(xmlStructuredErrorFunc schannel,
                xmlGenericErrorFunc channel, void *data, void *ctx,
                void *nod, int domain, int code, xmlErrorLevel level,
                const char *file, int line, const char *str1,
                const char *str2, const char *str3, int int1, int col,
	        const char *msg, ...) LIBXML_ATTR_FORMAT(16,17);
void
__xmlSimpleError(int domain, int code, xmlNodePtr node,
                 const char *msg, const char *extra) LIBXML_ATTR_FORMAT(4,0);
void
xmlGenericErrorDefaultFunc(void *ctx, const char *msg,
                           ...) LIBXML_ATTR_FORMAT(2,3);

#endif /* XML_ERROR_H_PRIVATE__ */
