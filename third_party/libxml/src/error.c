/*
 * error.c: module displaying/handling XML parser errors
 *
 * See Copyright for the status of this software.
 *
 * Daniel Veillard <daniel@veillard.com>
 */

#define IN_LIBXML
#include "libxml.h"

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlmemory.h>

#include "private/error.h"
#include "private/string.h"

/************************************************************************
 *									*
 *			Error struct					*
 *									*
 ************************************************************************/

static int
xmlVSetError(xmlError *err,
             void *ctxt, xmlNodePtr node,
             int domain, int code, xmlErrorLevel level,
             const char *file, int line,
             const char *str1, const char *str2, const char *str3,
             int int1, int col,
             const char *fmt, va_list ap)
{
    char *message = NULL;
    char *fileCopy = NULL;
    char *str1Copy = NULL;
    char *str2Copy = NULL;
    char *str3Copy = NULL;

    if (code == XML_ERR_OK) {
        xmlResetError(err);
        return(0);
    }

    /*
     * Formatting the message
     */
    if (fmt == NULL) {
        message = xmlMemStrdup("No error message provided");
    } else {
        xmlChar *tmp;
        int res;

        res = xmlStrVASPrintf(&tmp, MAX_ERR_MSG_SIZE, fmt, ap);
        if (res < 0)
            goto err_memory;
        message = (char *) tmp;
    }
    if (message == NULL)
        goto err_memory;

    if (file != NULL) {
        fileCopy = (char *) xmlStrdup((const xmlChar *) file);
        if (fileCopy == NULL)
            goto err_memory;
    }
    if (str1 != NULL) {
        str1Copy = (char *) xmlStrdup((const xmlChar *) str1);
        if (str1Copy == NULL)
            goto err_memory;
    }
    if (str2 != NULL) {
        str2Copy = (char *) xmlStrdup((const xmlChar *) str2);
        if (str2Copy == NULL)
            goto err_memory;
    }
    if (str3 != NULL) {
        str3Copy = (char *) xmlStrdup((const xmlChar *) str3);
        if (str3Copy == NULL)
            goto err_memory;
    }

    xmlResetError(err);

    err->domain = domain;
    err->code = code;
    err->message = message;
    err->level = level;
    err->file = fileCopy;
    err->line = line;
    err->str1 = str1Copy;
    err->str2 = str2Copy;
    err->str3 = str3Copy;
    err->int1 = int1;
    err->int2 = col;
    err->node = node;
    err->ctxt = ctxt;

    return(0);

err_memory:
    xmlFree(message);
    xmlFree(fileCopy);
    xmlFree(str1Copy);
    xmlFree(str2Copy);
    xmlFree(str3Copy);
    return(-1);
}

static int LIBXML_ATTR_FORMAT(14,15)
xmlSetError(xmlError *err,
            void *ctxt, xmlNodePtr node,
            int domain, int code, xmlErrorLevel level,
            const char *file, int line,
            const char *str1, const char *str2, const char *str3,
            int int1, int col,
            const char *fmt, ...)
{
    va_list ap;
    int res;

    va_start(ap, fmt);
    res = xmlVSetError(err, ctxt, node, domain, code, level, file, line,
                       str1, str2, str3, int1, col, fmt, ap);
    va_end(ap);

    return(res);
}

static int
xmlVUpdateError(xmlError *err,
                void *ctxt, xmlNodePtr node,
                int domain, int code, xmlErrorLevel level,
                const char *file, int line,
                const char *str1, const char *str2, const char *str3,
                int int1, int col,
                const char *fmt, va_list ap)
{
    int res;

    /*
     * Find first element parent.
     */
    if (node != NULL) {
        int i;

        for (i = 0; i < 10; i++) {
            if ((node->type == XML_ELEMENT_NODE) ||
                (node->parent == NULL))
                break;
            node = node->parent;
        }
    }

    /*
     * Get file and line from node.
     */
    if (node != NULL) {
        if ((file == NULL) && (node->doc != NULL))
            file = (const char *) node->doc->URL;

        if (line == 0) {
            if (node->type == XML_ELEMENT_NODE)
                line = node->line;
            if ((line == 0) || (line == 65535))
                line = xmlGetLineNo(node);
        }
    }

    res = xmlVSetError(err, ctxt, node, domain, code, level, file, line,
                       str1, str2, str3, int1, col, fmt, ap);

    return(res);
}

/************************************************************************
 *									*
 *			Handling of out of context errors		*
 *									*
 ************************************************************************/

/**
 * xmlGenericErrorDefaultFunc:
 * @ctx:  an error context
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 *
 * Default handler for out of context error messages.
 */
void
xmlGenericErrorDefaultFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...) {
    va_list args;

    if (xmlGenericErrorContext == NULL)
	xmlGenericErrorContext = (void *) stderr;

    va_start(args, msg);
    vfprintf((FILE *)xmlGenericErrorContext, msg, args);
    va_end(args);
}

/**
 * initGenericErrorDefaultFunc:
 * @handler:  the handler
 *
 * DEPRECATED: Use xmlSetGenericErrorFunc.
 *
 * Set or reset (if NULL) the default handler for generic errors
 * to the builtin error function.
 */
void
initGenericErrorDefaultFunc(xmlGenericErrorFunc * handler)
{
    if (handler == NULL)
        xmlGenericError = xmlGenericErrorDefaultFunc;
    else
        xmlGenericError = (*handler);
}

/**
 * xmlSetGenericErrorFunc:
 * @ctx:  the new error handling context
 * @handler:  the new handler function
 *
 * DEPRECATED: See xmlSetStructuredErrorFunc for alternatives.
 *
 * Set the global "generic" handler and context for error messages.
 * The generic error handler will only receive fragments of error
 * messages which should be concatenated or printed to a stream.
 *
 * If handler is NULL, use the built-in default handler which prints
 * to stderr.
 *
 * Since this is a global setting, it's a good idea to reset the
 * error handler to its default value after collecting the errors
 * you're interested in.
 *
 * For multi-threaded applications, this must be set separately for
 * each thread.
 */
void
xmlSetGenericErrorFunc(void *ctx, xmlGenericErrorFunc handler) {
    xmlGenericErrorContext = ctx;
    if (handler != NULL)
	xmlGenericError = handler;
    else
	xmlGenericError = xmlGenericErrorDefaultFunc;
}

/**
 * xmlSetStructuredErrorFunc:
 * @ctx:  the new error handling context
 * @handler:  the new handler function
 *
 * DEPRECATED: Use a per-context error handler.
 *
 * It's recommended to use the per-context error handlers instead:
 *
 * - xmlCtxtSetErrorHandler (since 2.13.0)
 * - xmlTextReaderSetStructuredErrorHandler
 * - xmlXPathSetErrorHandler (since 2.13.0)
 * - xmlXIncludeSetErrorHandler (since 2.13.0)
 * - xmlSchemaSetParserStructuredErrors
 * - xmlSchemaSetValidStructuredErrors
 * - xmlRelaxNGSetParserStructuredErrors
 * - xmlRelaxNGSetValidStructuredErrors
 *
 * Set the global "structured" handler and context for error messages.
 * If handler is NULL, the error handler is deactivated.
 *
 * The structured error handler takes precedence over "generic"
 * handlers, even per-context generic handlers.
 *
 * Since this is a global setting, it's a good idea to deactivate the
 * error handler after collecting the errors you're interested in.
 *
 * For multi-threaded applications, this must be set separately for
 * each thread.
 */
void
xmlSetStructuredErrorFunc(void *ctx, xmlStructuredErrorFunc handler) {
    xmlStructuredErrorContext = ctx;
    xmlStructuredError = handler;
}

/************************************************************************
 *									*
 *			Handling of parsing errors			*
 *									*
 ************************************************************************/

/**
 * xmlParserPrintFileInfo:
 * @input:  an xmlParserInputPtr input
 *
 * DEPRECATED: Use xmlFormatError.
 *
 * Displays the associated file and line information for the current input
 */

void
xmlParserPrintFileInfo(xmlParserInputPtr input) {
    if (input != NULL) {
	if (input->filename)
	    xmlGenericError(xmlGenericErrorContext,
		    "%s:%d: ", input->filename,
		    input->line);
	else
	    xmlGenericError(xmlGenericErrorContext,
		    "Entity: line %d: ", input->line);
    }
}

/**
 * xmlParserPrintFileContextInternal:
 * @input:  an xmlParserInputPtr input
 *
 * Displays current context within the input content for error tracking
 */

static void
xmlParserPrintFileContextInternal(xmlParserInputPtr input ,
		xmlGenericErrorFunc channel, void *data ) {
    const xmlChar *cur, *base, *start;
    unsigned int n, col;	/* GCC warns if signed, because compared with sizeof() */
    xmlChar  content[81]; /* space for 80 chars + line terminator */
    xmlChar *ctnt;

    if ((input == NULL) || (input->cur == NULL))
        return;

    cur = input->cur;
    base = input->base;
    /* skip backwards over any end-of-lines */
    while ((cur > base) && ((*(cur) == '\n') || (*(cur) == '\r'))) {
	cur--;
    }
    n = 0;
    /* search backwards for beginning-of-line (to max buff size) */
    while ((n < sizeof(content) - 1) && (cur > base) &&
	   (*cur != '\n') && (*cur != '\r')) {
        cur--;
        n++;
    }
    if ((n > 0) && ((*cur == '\n') || (*cur == '\r'))) {
        cur++;
    } else {
        /* skip over continuation bytes */
        while ((cur < input->cur) && ((*cur & 0xC0) == 0x80))
            cur++;
    }
    /* calculate the error position in terms of the current position */
    col = input->cur - cur;
    /* search forward for end-of-line (to max buff size) */
    n = 0;
    start = cur;
    /* copy selected text to our buffer */
    while ((*cur != 0) && (*(cur) != '\n') && (*(cur) != '\r')) {
        int len = input->end - cur;
        int c = xmlGetUTF8Char(cur, &len);

        if ((c < 0) || (n + len > sizeof(content)-1))
            break;
        cur += len;
	n += len;
    }
    memcpy(content, start, n);
    content[n] = 0;
    /* print out the selected text */
    channel(data ,"%s\n", content);
    /* create blank line with problem pointer */
    n = 0;
    ctnt = content;
    /* (leave buffer space for pointer + line terminator) */
    while ((n<col) && (n++ < sizeof(content)-2) && (*ctnt != 0)) {
	if (*(ctnt) != '\t')
	    *(ctnt) = ' ';
	ctnt++;
    }
    *ctnt++ = '^';
    *ctnt = 0;
    channel(data ,"%s\n", content);
}

/**
 * xmlParserPrintFileContext:
 * @input:  an xmlParserInputPtr input
 *
 * DEPRECATED: Use xmlFormatError.
 *
 * Displays current context within the input content for error tracking
 */
void
xmlParserPrintFileContext(xmlParserInputPtr input) {
   xmlParserPrintFileContextInternal(input, xmlGenericError,
                                     xmlGenericErrorContext);
}

/**
 * xmlFormatError:
 * @err:  the error
 * @channel:  callback
 * @data:  user data for callback
 *
 * Report a formatted error to a printf-like callback.
 *
 * This can result in a verbose multi-line report including additional
 * information from the parser context.
 *
 * Available since 2.13.0.
 */
void
xmlFormatError(const xmlError *err, xmlGenericErrorFunc channel, void *data)
{
    const char *message;
    const char *file;
    int line;
    int code;
    int domain;
    const xmlChar *name = NULL;
    xmlNodePtr node;
    xmlErrorLevel level;
    xmlParserCtxtPtr ctxt = NULL;
    xmlParserInputPtr input = NULL;
    xmlParserInputPtr cur = NULL;

    if ((err == NULL) || (channel == NULL))
        return;

    message = err->message;
    file = err->file;
    line = err->line;
    code = err->code;
    domain = err->domain;
    level = err->level;
    node = err->node;

    if (code == XML_ERR_OK)
        return;

    if ((domain == XML_FROM_PARSER) || (domain == XML_FROM_HTML) ||
        (domain == XML_FROM_DTD) || (domain == XML_FROM_NAMESPACE) ||
	(domain == XML_FROM_IO) || (domain == XML_FROM_VALID)) {
	ctxt = err->ctxt;
    }

    if ((node != NULL) && (node->type == XML_ELEMENT_NODE) &&
        (domain != XML_FROM_SCHEMASV))
        name = node->name;

    /*
     * Maintain the compatibility with the legacy error handling
     */
    if ((ctxt != NULL) && (ctxt->input != NULL)) {
        input = ctxt->input;
        if ((input->filename == NULL) &&
            (ctxt->inputNr > 1)) {
            cur = input;
            input = ctxt->inputTab[ctxt->inputNr - 2];
        }
        if (input->filename)
            channel(data, "%s:%d: ", input->filename, input->line);
        else if ((line != 0) && (domain == XML_FROM_PARSER))
            channel(data, "Entity: line %d: ", input->line);
    } else {
        if (file != NULL)
            channel(data, "%s:%d: ", file, line);
        else if ((line != 0) &&
	         ((domain == XML_FROM_PARSER) || (domain == XML_FROM_SCHEMASV)||
		  (domain == XML_FROM_SCHEMASP)||(domain == XML_FROM_DTD) ||
		  (domain == XML_FROM_RELAXNGP)||(domain == XML_FROM_RELAXNGV)))
            channel(data, "Entity: line %d: ", line);
    }
    if (name != NULL) {
        channel(data, "element %s: ", name);
    }
    switch (domain) {
        case XML_FROM_PARSER:
            channel(data, "parser ");
            break;
        case XML_FROM_NAMESPACE:
            channel(data, "namespace ");
            break;
        case XML_FROM_DTD:
        case XML_FROM_VALID:
            channel(data, "validity ");
            break;
        case XML_FROM_HTML:
            channel(data, "HTML parser ");
            break;
        case XML_FROM_MEMORY:
            channel(data, "memory ");
            break;
        case XML_FROM_OUTPUT:
            channel(data, "output ");
            break;
        case XML_FROM_IO:
            channel(data, "I/O ");
            break;
        case XML_FROM_XINCLUDE:
            channel(data, "XInclude ");
            break;
        case XML_FROM_XPATH:
            channel(data, "XPath ");
            break;
        case XML_FROM_XPOINTER:
            channel(data, "parser ");
            break;
        case XML_FROM_REGEXP:
            channel(data, "regexp ");
            break;
        case XML_FROM_MODULE:
            channel(data, "module ");
            break;
        case XML_FROM_SCHEMASV:
            channel(data, "Schemas validity ");
            break;
        case XML_FROM_SCHEMASP:
            channel(data, "Schemas parser ");
            break;
        case XML_FROM_RELAXNGP:
            channel(data, "Relax-NG parser ");
            break;
        case XML_FROM_RELAXNGV:
            channel(data, "Relax-NG validity ");
            break;
        case XML_FROM_CATALOG:
            channel(data, "Catalog ");
            break;
        case XML_FROM_C14N:
            channel(data, "C14N ");
            break;
        case XML_FROM_XSLT:
            channel(data, "XSLT ");
            break;
        case XML_FROM_I18N:
            channel(data, "encoding ");
            break;
        case XML_FROM_SCHEMATRONV:
            channel(data, "schematron ");
            break;
        case XML_FROM_BUFFER:
            channel(data, "internal buffer ");
            break;
        case XML_FROM_URI:
            channel(data, "URI ");
            break;
        default:
            break;
    }
    switch (level) {
        case XML_ERR_NONE:
            channel(data, ": ");
            break;
        case XML_ERR_WARNING:
            channel(data, "warning : ");
            break;
        case XML_ERR_ERROR:
            channel(data, "error : ");
            break;
        case XML_ERR_FATAL:
            channel(data, "error : ");
            break;
    }
    if (message != NULL) {
        int len;
	len = xmlStrlen((const xmlChar *) message);
	if ((len > 0) && (message[len - 1] != '\n'))
	    channel(data, "%s\n", message);
	else
	    channel(data, "%s", message);
    } else {
        channel(data, "%s\n", "No error message provided");
    }

    if (ctxt != NULL) {
        if ((input != NULL) &&
            ((input->buf == NULL) || (input->buf->encoder == NULL)) &&
            (code == XML_ERR_INVALID_ENCODING) &&
            (input->cur < input->end)) {
            int i;

            channel(data, "Bytes:");
            for (i = 0; i < 4; i++) {
                if (input->cur + i >= input->end)
                    break;
                channel(data, " 0x%02X", input->cur[i]);
            }
            channel(data, "\n");
        }

        xmlParserPrintFileContextInternal(input, channel, data);

        if (cur != NULL) {
            if (cur->filename)
                channel(data, "%s:%d: \n", cur->filename, cur->line);
            else if ((line != 0) && (domain == XML_FROM_PARSER))
                channel(data, "Entity: line %d: \n", cur->line);
            xmlParserPrintFileContextInternal(cur, channel, data);
        }
    }
    if ((domain == XML_FROM_XPATH) && (err->str1 != NULL) &&
        (err->int1 < 100) &&
	(err->int1 < xmlStrlen((const xmlChar *)err->str1))) {
	xmlChar buf[150];
	int i;

	channel(data, "%s\n", err->str1);
	for (i=0;i < err->int1;i++)
	     buf[i] = ' ';
	buf[i++] = '^';
	buf[i] = 0;
	channel(data, "%s\n", buf);
    }
}

/**
 * xmlRaiseMemoryError:
 * @schannel: the structured callback channel
 * @channel: the old callback channel
 * @data: the callback data
 * @domain: the domain for the error
 * @error: optional error struct to be filled
 *
 * Update the global and optional error structure, then forward the
 * error to an error handler.
 *
 * This function doesn't make memory allocations which are likely
 * to fail after an OOM error.
 */
void
xmlRaiseMemoryError(xmlStructuredErrorFunc schannel, xmlGenericErrorFunc channel,
                    void *data, int domain, xmlError *error)
{
    xmlError *lastError = &xmlLastError;

    xmlResetLastError();
    lastError->domain = domain;
    lastError->code = XML_ERR_NO_MEMORY;
    lastError->level = XML_ERR_FATAL;

    if (error != NULL) {
        xmlResetError(error);
        error->domain = domain;
        error->code = XML_ERR_NO_MEMORY;
        error->level = XML_ERR_FATAL;
    }

    if (schannel != NULL) {
        schannel(data, lastError);
    } else if (xmlStructuredError != NULL) {
        xmlStructuredError(xmlStructuredErrorContext, lastError);
    } else if (channel != NULL) {
        channel(data, "libxml2: out of memory\n");
    }
}

/**
 * xmlVRaiseError:
 * @schannel: the structured callback channel
 * @channel: the old callback channel
 * @data: the callback data
 * @ctx: the parser context or NULL
 * @node: the current node or NULL
 * @domain: the domain for the error
 * @code: the code for the error
 * @level: the xmlErrorLevel for the error
 * @file: the file source of the error (or NULL)
 * @line: the line of the error or 0 if N/A
 * @str1: extra string info
 * @str2: extra string info
 * @str3: extra string info
 * @int1: extra int info
 * @col: column number of the error or 0 if N/A
 * @msg:  the message to display/transmit
 * @ap:  extra parameters for the message display
 *
 * Update the appropriate global or contextual error structure,
 * then forward the error message down the parser or generic
 * error callback handler
 *
 * Returns 0 on success, -1 if a memory allocation failed.
 */
int
xmlVRaiseError(xmlStructuredErrorFunc schannel,
               xmlGenericErrorFunc channel, void *data, void *ctx,
               xmlNode *node, int domain, int code, xmlErrorLevel level,
               const char *file, int line, const char *str1,
               const char *str2, const char *str3, int int1, int col,
               const char *msg, va_list ap)
{
    xmlParserCtxtPtr ctxt = NULL;
    /* xmlLastError is a macro retrieving the per-thread global. */
    xmlErrorPtr lastError = &xmlLastError;
    xmlErrorPtr to = lastError;

    if (code == XML_ERR_OK)
        return(0);
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (code == XML_ERR_INTERNAL_ERROR) {
        fprintf(stderr, "Unexpected error: %d\n", code);
        abort();
    }
#endif
    if ((xmlGetWarningsDefaultValue == 0) && (level == XML_ERR_WARNING))
        return(0);

    if ((domain == XML_FROM_PARSER) || (domain == XML_FROM_HTML) ||
        (domain == XML_FROM_DTD) || (domain == XML_FROM_NAMESPACE) ||
	(domain == XML_FROM_IO) || (domain == XML_FROM_VALID)) {
	ctxt = (xmlParserCtxtPtr) ctx;

        if (ctxt != NULL)
            to = &ctxt->lastError;
    }

    if (xmlVUpdateError(to, ctxt, node, domain, code, level, file, line,
                        str1, str2, str3, int1, col, msg, ap))
        return(-1);

    if (to != lastError) {
        if (xmlCopyError(to, lastError) < 0)
            return(-1);
    }

    if (schannel != NULL) {
	schannel(data, to);
    } else if (xmlStructuredError != NULL) {
        xmlStructuredError(xmlStructuredErrorContext, to);
    } else if (channel != NULL) {
        /* Don't invoke legacy error handlers */
        if ((channel == xmlGenericErrorDefaultFunc) ||
            (channel == xmlParserError) ||
            (channel == xmlParserWarning) ||
            (channel == xmlParserValidityError) ||
            (channel == xmlParserValidityWarning))
            xmlFormatError(to, xmlGenericError, xmlGenericErrorContext);
        else
	    channel(data, "%s", to->message);
    }

    return(0);
}

/**
 * __xmlRaiseError:
 * @schannel: the structured callback channel
 * @channel: the old callback channel
 * @data: the callback data
 * @ctx: the parser context or NULL
 * @nod: the node or NULL
 * @domain: the domain for the error
 * @code: the code for the error
 * @level: the xmlErrorLevel for the error
 * @file: the file source of the error (or NULL)
 * @line: the line of the error or 0 if N/A
 * @str1: extra string info
 * @str2: extra string info
 * @str3: extra string info
 * @int1: extra int info
 * @col: column number of the error or 0 if N/A
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 *
 * Update the appropriate global or contextual error structure,
 * then forward the error message down the parser or generic
 * error callback handler
 *
 * Returns 0 on success, -1 if a memory allocation failed.
 */
int
__xmlRaiseError(xmlStructuredErrorFunc schannel,
                xmlGenericErrorFunc channel, void *data, void *ctx,
                xmlNode *node, int domain, int code, xmlErrorLevel level,
                const char *file, int line, const char *str1,
                const char *str2, const char *str3, int int1, int col,
                const char *msg, ...)
{
    va_list ap;
    int res;

    va_start(ap, msg);
    res = xmlVRaiseError(schannel, channel, data, ctx, node, domain, code,
                         level, file, line, str1, str2, str3, int1, col, msg,
                         ap);
    va_end(ap);

    return(res);
}

static void
xmlVFormatLegacyError(void *ctx, const char *level,
                      const char *fmt, va_list ap) {
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlParserInputPtr input = NULL;
    xmlParserInputPtr cur = NULL;
    xmlChar *str = NULL;

    if (ctxt != NULL) {
	input = ctxt->input;
	if ((input != NULL) && (input->filename == NULL) &&
	    (ctxt->inputNr > 1)) {
	    cur = input;
	    input = ctxt->inputTab[ctxt->inputNr - 2];
	}
	xmlParserPrintFileInfo(input);
    }

    xmlGenericError(xmlGenericErrorContext, "%s: ", level);

    xmlStrVASPrintf(&str, MAX_ERR_MSG_SIZE, fmt, ap);
    if (str != NULL) {
        xmlGenericError(xmlGenericErrorContext, "%s", (char *) str);
	xmlFree(str);
    }

    if (ctxt != NULL) {
	xmlParserPrintFileContext(input);
	if (cur != NULL) {
	    xmlParserPrintFileInfo(cur);
	    xmlGenericError(xmlGenericErrorContext, "\n");
	    xmlParserPrintFileContext(cur);
	}
    }
}

/**
 * xmlParserError:
 * @ctx:  an XML parser context
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 *
 * Display and format an error messages, gives file, line, position and
 * extra parameters.
 */
void
xmlParserError(void *ctx, const char *msg ATTRIBUTE_UNUSED, ...)
{
    va_list ap;

    va_start(ap, msg);
    xmlVFormatLegacyError(ctx, "error", msg, ap);
    va_end(ap);
}

/**
 * xmlParserWarning:
 * @ctx:  an XML parser context
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 *
 * Display and format a warning messages, gives file, line, position and
 * extra parameters.
 */
void
xmlParserWarning(void *ctx, const char *msg ATTRIBUTE_UNUSED, ...)
{
    va_list ap;

    va_start(ap, msg);
    xmlVFormatLegacyError(ctx, "warning", msg, ap);
    va_end(ap);
}

/**
 * xmlParserValidityError:
 * @ctx:  an XML parser context
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 *
 * Display and format an validity error messages, gives file,
 * line, position and extra parameters.
 */
void
xmlParserValidityError(void *ctx, const char *msg ATTRIBUTE_UNUSED, ...)
{
    va_list ap;

    va_start(ap, msg);
    xmlVFormatLegacyError(ctx, "validity error", msg, ap);
    va_end(ap);
}

/**
 * xmlParserValidityWarning:
 * @ctx:  an XML parser context
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 *
 * Display and format a validity warning messages, gives file, line,
 * position and extra parameters.
 */
void
xmlParserValidityWarning(void *ctx, const char *msg ATTRIBUTE_UNUSED, ...)
{
    va_list ap;

    va_start(ap, msg);
    xmlVFormatLegacyError(ctx, "validity warning", msg, ap);
    va_end(ap);
}


/************************************************************************
 *									*
 *			Extended Error Handling				*
 *									*
 ************************************************************************/

/**
 * xmlGetLastError:
 *
 * Get the last global error registered. This is per thread if compiled
 * with thread support.
 *
 * Returns a pointer to the error
 */
const xmlError *
xmlGetLastError(void)
{
    if (xmlLastError.code == XML_ERR_OK)
        return (NULL);
    return (&xmlLastError);
}

/**
 * xmlResetError:
 * @err: pointer to the error.
 *
 * Cleanup the error.
 */
void
xmlResetError(xmlErrorPtr err)
{
    if (err == NULL)
        return;
    if (err->code == XML_ERR_OK)
        return;
    if (err->message != NULL)
        xmlFree(err->message);
    if (err->file != NULL)
        xmlFree(err->file);
    if (err->str1 != NULL)
        xmlFree(err->str1);
    if (err->str2 != NULL)
        xmlFree(err->str2);
    if (err->str3 != NULL)
        xmlFree(err->str3);
    memset(err, 0, sizeof(xmlError));
    err->code = XML_ERR_OK;
}

/**
 * xmlResetLastError:
 *
 * Cleanup the last global error registered. For parsing error
 * this does not change the well-formedness result.
 */
void
xmlResetLastError(void)
{
    if (xmlLastError.code == XML_ERR_OK)
        return;
    xmlResetError(&xmlLastError);
}

/**
 * xmlCtxtGetLastError:
 * @ctx:  an XML parser context
 *
 * Get the last parsing error registered.
 *
 * Returns NULL if no error occurred or a pointer to the error
 */
const xmlError *
xmlCtxtGetLastError(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;

    if (ctxt == NULL)
        return (NULL);
    if (ctxt->lastError.code == XML_ERR_OK)
        return (NULL);
    return (&ctxt->lastError);
}

/**
 * xmlCtxtResetLastError:
 * @ctx:  an XML parser context
 *
 * Cleanup the last global error registered. For parsing error
 * this does not change the well-formedness result.
 */
void
xmlCtxtResetLastError(void *ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;

    if (ctxt == NULL)
        return;
    ctxt->errNo = XML_ERR_OK;
    if (ctxt->lastError.code == XML_ERR_OK)
        return;
    xmlResetError(&ctxt->lastError);
}

/**
 * xmlCopyError:
 * @from:  a source error
 * @to:  a target error
 *
 * Save the original error to the new place.
 *
 * Returns 0 in case of success and -1 in case of error.
 */
int
xmlCopyError(const xmlError *from, xmlErrorPtr to) {
    const char *fmt = NULL;

    if ((from == NULL) || (to == NULL))
        return(-1);

    if (from->message != NULL)
        fmt = "%s";

    return(xmlSetError(to, from->ctxt, from->node,
                       from->domain, from->code, from->level,
                       from->file, from->line,
                       from->str1, from->str2, from->str3,
                       from->int1, from->int2,
                       fmt, from->message));
}

/**
 * xmlErrString:
 * @code:  an xmlParserErrors code
 *
 * Returns an error message for a code.
 */
const char *
xmlErrString(xmlParserErrors code) {
    const char *errmsg;

    switch (code) {
        case XML_ERR_INVALID_HEX_CHARREF:
            errmsg = "CharRef: invalid hexadecimal value";
            break;
        case XML_ERR_INVALID_DEC_CHARREF:
            errmsg = "CharRef: invalid decimal value";
            break;
        case XML_ERR_INVALID_CHARREF:
            errmsg = "CharRef: invalid value";
            break;
        case XML_ERR_INTERNAL_ERROR:
            errmsg = "internal error";
            break;
        case XML_ERR_PEREF_AT_EOF:
            errmsg = "PEReference at end of document";
            break;
        case XML_ERR_PEREF_IN_PROLOG:
            errmsg = "PEReference in prolog";
            break;
        case XML_ERR_PEREF_IN_EPILOG:
            errmsg = "PEReference in epilog";
            break;
        case XML_ERR_PEREF_NO_NAME:
            errmsg = "PEReference: no name";
            break;
        case XML_ERR_PEREF_SEMICOL_MISSING:
            errmsg = "PEReference: expecting ';'";
            break;
        case XML_ERR_ENTITY_LOOP:
            errmsg = "Detected an entity reference loop";
            break;
        case XML_ERR_ENTITY_NOT_STARTED:
            errmsg = "EntityValue: \" or ' expected";
            break;
        case XML_ERR_ENTITY_PE_INTERNAL:
            errmsg = "PEReferences forbidden in internal subset";
            break;
        case XML_ERR_ENTITY_NOT_FINISHED:
            errmsg = "EntityValue: \" or ' expected";
            break;
        case XML_ERR_ATTRIBUTE_NOT_STARTED:
            errmsg = "AttValue: \" or ' expected";
            break;
        case XML_ERR_LT_IN_ATTRIBUTE:
            errmsg = "Unescaped '<' not allowed in attributes values";
            break;
        case XML_ERR_LITERAL_NOT_STARTED:
            errmsg = "SystemLiteral \" or ' expected";
            break;
        case XML_ERR_LITERAL_NOT_FINISHED:
            errmsg = "Unfinished System or Public ID \" or ' expected";
            break;
        case XML_ERR_MISPLACED_CDATA_END:
            errmsg = "Sequence ']]>' not allowed in content";
            break;
        case XML_ERR_URI_REQUIRED:
            errmsg = "SYSTEM or PUBLIC, the URI is missing";
            break;
        case XML_ERR_PUBID_REQUIRED:
            errmsg = "PUBLIC, the Public Identifier is missing";
            break;
        case XML_ERR_HYPHEN_IN_COMMENT:
            errmsg = "Comment must not contain '--' (double-hyphen)";
            break;
        case XML_ERR_PI_NOT_STARTED:
            errmsg = "xmlParsePI : no target name";
            break;
        case XML_ERR_RESERVED_XML_NAME:
            errmsg = "Invalid PI name";
            break;
        case XML_ERR_NOTATION_NOT_STARTED:
            errmsg = "NOTATION: Name expected here";
            break;
        case XML_ERR_NOTATION_NOT_FINISHED:
            errmsg = "'>' required to close NOTATION declaration";
            break;
        case XML_ERR_VALUE_REQUIRED:
            errmsg = "Entity value required";
            break;
        case XML_ERR_URI_FRAGMENT:
            errmsg = "Fragment not allowed";
            break;
        case XML_ERR_ATTLIST_NOT_STARTED:
            errmsg = "'(' required to start ATTLIST enumeration";
            break;
        case XML_ERR_NMTOKEN_REQUIRED:
            errmsg = "NmToken expected in ATTLIST enumeration";
            break;
        case XML_ERR_ATTLIST_NOT_FINISHED:
            errmsg = "')' required to finish ATTLIST enumeration";
            break;
        case XML_ERR_MIXED_NOT_STARTED:
            errmsg = "MixedContentDecl : '|' or ')*' expected";
            break;
        case XML_ERR_PCDATA_REQUIRED:
            errmsg = "MixedContentDecl : '#PCDATA' expected";
            break;
        case XML_ERR_ELEMCONTENT_NOT_STARTED:
            errmsg = "ContentDecl : Name or '(' expected";
            break;
        case XML_ERR_ELEMCONTENT_NOT_FINISHED:
            errmsg = "ContentDecl : ',' '|' or ')' expected";
            break;
        case XML_ERR_PEREF_IN_INT_SUBSET:
            errmsg =
                "PEReference: forbidden within markup decl in internal subset";
            break;
        case XML_ERR_GT_REQUIRED:
            errmsg = "expected '>'";
            break;
        case XML_ERR_CONDSEC_INVALID:
            errmsg = "XML conditional section '[' expected";
            break;
        case XML_ERR_INT_SUBSET_NOT_FINISHED:
            errmsg = "Content error in the internal subset";
            break;
        case XML_ERR_EXT_SUBSET_NOT_FINISHED:
            errmsg = "Content error in the external subset";
            break;
        case XML_ERR_CONDSEC_INVALID_KEYWORD:
            errmsg =
                "conditional section INCLUDE or IGNORE keyword expected";
            break;
        case XML_ERR_CONDSEC_NOT_FINISHED:
            errmsg = "XML conditional section not closed";
            break;
        case XML_ERR_XMLDECL_NOT_STARTED:
            errmsg = "Text declaration '<?xml' required";
            break;
        case XML_ERR_XMLDECL_NOT_FINISHED:
            errmsg = "parsing XML declaration: '?>' expected";
            break;
        case XML_ERR_EXT_ENTITY_STANDALONE:
            errmsg = "external parsed entities cannot be standalone";
            break;
        case XML_ERR_ENTITYREF_SEMICOL_MISSING:
            errmsg = "EntityRef: expecting ';'";
            break;
        case XML_ERR_DOCTYPE_NOT_FINISHED:
            errmsg = "DOCTYPE improperly terminated";
            break;
        case XML_ERR_LTSLASH_REQUIRED:
            errmsg = "EndTag: '</' not found";
            break;
        case XML_ERR_EQUAL_REQUIRED:
            errmsg = "expected '='";
            break;
        case XML_ERR_STRING_NOT_CLOSED:
            errmsg = "String not closed expecting \" or '";
            break;
        case XML_ERR_STRING_NOT_STARTED:
            errmsg = "String not started expecting ' or \"";
            break;
        case XML_ERR_ENCODING_NAME:
            errmsg = "Invalid XML encoding name";
            break;
        case XML_ERR_STANDALONE_VALUE:
            errmsg = "standalone accepts only 'yes' or 'no'";
            break;
        case XML_ERR_DOCUMENT_EMPTY:
            errmsg = "Document is empty";
            break;
        case XML_ERR_DOCUMENT_END:
            errmsg = "Extra content at the end of the document";
            break;
        case XML_ERR_NOT_WELL_BALANCED:
            errmsg = "chunk is not well balanced";
            break;
        case XML_ERR_EXTRA_CONTENT:
            errmsg = "extra content at the end of well balanced chunk";
            break;
        case XML_ERR_VERSION_MISSING:
            errmsg = "Malformed declaration expecting version";
            break;
        case XML_ERR_NAME_TOO_LONG:
            errmsg = "Name too long";
            break;
        case XML_ERR_INVALID_ENCODING:
            errmsg = "Invalid bytes in character encoding";
            break;
        case XML_ERR_RESOURCE_LIMIT:
            errmsg = "Resource limit exceeded";
            break;
        case XML_ERR_ARGUMENT:
            errmsg = "Invalid argument";
            break;
        case XML_ERR_SYSTEM:
            errmsg = "Out of system resources";
            break;
        case XML_ERR_REDECL_PREDEF_ENTITY:
            errmsg = "Invalid redeclaration of predefined entity";
            break;
        case XML_ERR_UNSUPPORTED_ENCODING:
            errmsg = "Unsupported encoding";
            break;
        case XML_ERR_INVALID_CHAR:
            errmsg = "Invalid character";
            break;

        case XML_IO_UNKNOWN:
            errmsg = "Unknown IO error"; break;
        case XML_IO_EACCES:
            errmsg = "Permission denied"; break;
        case XML_IO_EAGAIN:
            errmsg = "Resource temporarily unavailable"; break;
        case XML_IO_EBADF:
            errmsg = "Bad file descriptor"; break;
        case XML_IO_EBADMSG:
            errmsg = "Bad message"; break;
        case XML_IO_EBUSY:
            errmsg = "Resource busy"; break;
        case XML_IO_ECANCELED:
            errmsg = "Operation canceled"; break;
        case XML_IO_ECHILD:
            errmsg = "No child processes"; break;
        case XML_IO_EDEADLK:
            errmsg = "Resource deadlock avoided"; break;
        case XML_IO_EDOM:
            errmsg = "Domain error"; break;
        case XML_IO_EEXIST:
            errmsg = "File exists"; break;
        case XML_IO_EFAULT:
            errmsg = "Bad address"; break;
        case XML_IO_EFBIG:
            errmsg = "File too large"; break;
        case XML_IO_EINPROGRESS:
            errmsg = "Operation in progress"; break;
        case XML_IO_EINTR:
            errmsg = "Interrupted function call"; break;
        case XML_IO_EINVAL:
            errmsg = "Invalid argument"; break;
        case XML_IO_EIO:
            errmsg = "Input/output error"; break;
        case XML_IO_EISDIR:
            errmsg = "Is a directory"; break;
        case XML_IO_EMFILE:
            errmsg = "Too many open files"; break;
        case XML_IO_EMLINK:
            errmsg = "Too many links"; break;
        case XML_IO_EMSGSIZE:
            errmsg = "Inappropriate message buffer length"; break;
        case XML_IO_ENAMETOOLONG:
            errmsg = "Filename too long"; break;
        case XML_IO_ENFILE:
            errmsg = "Too many open files in system"; break;
        case XML_IO_ENODEV:
            errmsg = "No such device"; break;
        case XML_IO_ENOENT:
            errmsg = "No such file or directory"; break;
        case XML_IO_ENOEXEC:
            errmsg = "Exec format error"; break;
        case XML_IO_ENOLCK:
            errmsg = "No locks available"; break;
        case XML_IO_ENOMEM:
            errmsg = "Not enough space"; break;
        case XML_IO_ENOSPC:
            errmsg = "No space left on device"; break;
        case XML_IO_ENOSYS:
            errmsg = "Function not implemented"; break;
        case XML_IO_ENOTDIR:
            errmsg = "Not a directory"; break;
        case XML_IO_ENOTEMPTY:
            errmsg = "Directory not empty"; break;
        case XML_IO_ENOTSUP:
            errmsg = "Not supported"; break;
        case XML_IO_ENOTTY:
            errmsg = "Inappropriate I/O control operation"; break;
        case XML_IO_ENXIO:
            errmsg = "No such device or address"; break;
        case XML_IO_EPERM:
            errmsg = "Operation not permitted"; break;
        case XML_IO_EPIPE:
            errmsg = "Broken pipe"; break;
        case XML_IO_ERANGE:
            errmsg = "Result too large"; break;
        case XML_IO_EROFS:
            errmsg = "Read-only file system"; break;
        case XML_IO_ESPIPE:
            errmsg = "Invalid seek"; break;
        case XML_IO_ESRCH:
            errmsg = "No such process"; break;
        case XML_IO_ETIMEDOUT:
            errmsg = "Operation timed out"; break;
        case XML_IO_EXDEV:
            errmsg = "Improper link"; break;
        case XML_IO_NETWORK_ATTEMPT:
            errmsg = "Attempt to load network entity"; break;
        case XML_IO_ENCODER:
            errmsg = "encoder error"; break;
        case XML_IO_FLUSH:
            errmsg = "flush error"; break;
        case XML_IO_WRITE:
            errmsg = "write error"; break;
        case XML_IO_NO_INPUT:
            errmsg = "no input"; break;
        case XML_IO_BUFFER_FULL:
            errmsg = "buffer full"; break;
        case XML_IO_LOAD_ERROR:
            errmsg = "loading error"; break;
        case XML_IO_ENOTSOCK:
            errmsg = "not a socket"; break;
        case XML_IO_EISCONN:
            errmsg = "already connected"; break;
        case XML_IO_ECONNREFUSED:
            errmsg = "connection refused"; break;
        case XML_IO_ENETUNREACH:
            errmsg = "unreachable network"; break;
        case XML_IO_EADDRINUSE:
            errmsg = "address in use"; break;
        case XML_IO_EALREADY:
            errmsg = "already in use"; break;
        case XML_IO_EAFNOSUPPORT:
            errmsg = "unknown address family"; break;
        case XML_IO_UNSUPPORTED_PROTOCOL:
            errmsg = "unsupported protocol"; break;

        default:
            errmsg = "Unregistered error message";
    }

    return(errmsg);
}
