/*
 * parserInternals.c : Internal routines (and obsolete ones) needed for the
 *                     XML and HTML parsers.
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXML
#include "libxml.h"

#if defined(_WIN32)
#define XML_DIR_SEP '\\'
#else
#define XML_DIR_SEP '/'
#endif

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/entities.h>
#include <libxml/xmlerror.h>
#include <libxml/encoding.h>
#include <libxml/xmlIO.h>
#include <libxml/uri.h>
#include <libxml/dict.h>
#include <libxml/xmlsave.h>
#ifdef LIBXML_CATALOG_ENABLED
#include <libxml/catalog.h>
#endif
#include <libxml/chvalid.h>
#include <libxml/nanohttp.h>

#define CUR(ctxt) ctxt->input->cur
#define END(ctxt) ctxt->input->end

#include "private/buf.h"
#include "private/enc.h"
#include "private/error.h"
#include "private/io.h"
#include "private/memory.h"
#include "private/parser.h"

#ifndef SIZE_MAX
  #define SIZE_MAX ((size_t) -1)
#endif

#define XML_MAX_ERRORS 100

/*
 * XML_MAX_AMPLIFICATION_DEFAULT is the default maximum allowed amplification
 * factor of serialized output after entity expansion.
 */
#define XML_MAX_AMPLIFICATION_DEFAULT 5

/*
 * Various global defaults for parsing
 */

/**
 * xmlCheckVersion:
 * @version: the include version number
 *
 * check the compiled lib version against the include one.
 */
void
xmlCheckVersion(int version) {
    int myversion = LIBXML_VERSION;

    xmlInitParser();

    if ((myversion / 10000) != (version / 10000)) {
	xmlPrintErrorMessage(
		"Fatal: program compiled against libxml %d using libxml %d\n",
		(version / 10000), (myversion / 10000));
    } else if ((myversion / 100) < (version / 100)) {
	xmlPrintErrorMessage(
		"Warning: program compiled against libxml %d using older %d\n",
		(version / 100), (myversion / 100));
    }
}


/************************************************************************
 *									*
 *		Some factorized error routines				*
 *									*
 ************************************************************************/


/**
 * xmlCtxtSetErrorHandler:
 * @ctxt:  an XML parser context
 * @handler:  error handler
 * @data:  data for error handler
 *
 * Register a callback function that will be called on errors and
 * warnings. If handler is NULL, the error handler will be deactivated.
 *
 * This is the recommended way to collect errors from the parser and
 * takes precedence over all other error reporting mechanisms.
 * These are (in order of precedence):
 *
 * - per-context structured handler (xmlCtxtSetErrorHandler)
 * - per-context structured "serror" SAX handler
 * - global structured handler (xmlSetStructuredErrorFunc)
 * - per-context generic "error" and "warning" SAX handlers
 * - global generic handler (xmlSetGenericErrorFunc)
 * - print to stderr
 *
 * Available since 2.13.0.
 */
void
xmlCtxtSetErrorHandler(xmlParserCtxtPtr ctxt, xmlStructuredErrorFunc handler,
                       void *data)
{
    if (ctxt == NULL)
        return;
    ctxt->errorHandler = handler;
    ctxt->errorCtxt = data;
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
 * xmlCtxtErrMemory:
 * @ctxt:  an XML parser context
 *
 * Handle an out-of-memory error.
 *
 * Available since 2.13.0.
 */
void
xmlCtxtErrMemory(xmlParserCtxtPtr ctxt)
{
    xmlStructuredErrorFunc schannel = NULL;
    xmlGenericErrorFunc channel = NULL;
    void *data;

    if (ctxt == NULL) {
        xmlRaiseMemoryError(NULL, NULL, NULL, XML_FROM_PARSER, NULL);
        return;
    }

    ctxt->errNo = XML_ERR_NO_MEMORY;
    ctxt->instate = XML_PARSER_EOF; /* TODO: Remove after refactoring */
    ctxt->wellFormed = 0;
    ctxt->disableSAX = 2;

    if (ctxt->errorHandler) {
        schannel = ctxt->errorHandler;
        data = ctxt->errorCtxt;
    } else if ((ctxt->sax->initialized == XML_SAX2_MAGIC) &&
        (ctxt->sax->serror != NULL)) {
        schannel = ctxt->sax->serror;
        data = ctxt->userData;
    } else {
        channel = ctxt->sax->error;
        data = ctxt->userData;
    }

    xmlRaiseMemoryError(schannel, channel, data, XML_FROM_PARSER,
                        &ctxt->lastError);
}

/**
 * xmlCtxtErrIO:
 * @ctxt:  parser context
 * @code:  xmlParserErrors code
 * @uri:  filename or URI (optional)
 *
 * If filename is empty, use the one from context input if available.
 *
 * Report an IO error to the parser context.
 */
void
xmlCtxtErrIO(xmlParserCtxtPtr ctxt, int code, const char *uri)
{
    const char *errstr, *msg, *str1, *str2;
    xmlErrorLevel level;

    if (ctxt == NULL)
        return;

    if (((code == XML_IO_ENOENT) ||
         (code == XML_IO_UNKNOWN))) {
        /*
         * Only report a warning if a file could not be found. This should
         * only be done for external entities, but the external entity loader
         * of xsltproc can try multiple paths and assumes that ENOENT doesn't
         * raise an error and aborts parsing.
         */
        if (ctxt->validate == 0)
            level = XML_ERR_WARNING;
        else
            level = XML_ERR_ERROR;
    } else if (code == XML_IO_NETWORK_ATTEMPT) {
        level = XML_ERR_ERROR;
    } else {
        level = XML_ERR_FATAL;
    }

    errstr = xmlErrString(code);

    if (uri == NULL) {
        msg = "%s\n";
        str1 = errstr;
        str2 = NULL;
    } else {
        msg = "failed to load \"%s\": %s\n";
        str1 = uri;
        str2 = errstr;
    }

    xmlCtxtErr(ctxt, NULL, XML_FROM_IO, code, level,
               (const xmlChar *) uri, NULL, NULL, 0,
               msg, str1, str2);
}

/**
 * xmlCtxtIsCatastrophicError:
 * @ctxt:  parser context
 *
 * Returns true if the last error is catastrophic.
 */
int
xmlCtxtIsCatastrophicError(xmlParserCtxtPtr ctxt) {
    if (ctxt == NULL)
        return(1);

    return(xmlIsCatastrophicError(ctxt->lastError.level,
                                  ctxt->lastError.code));
}

/**
 * xmlCtxtVErr:
 * @ctxt:  a parser context
 * @node: the current node or NULL
 * @domain: the domain for the error
 * @code: the code for the error
 * @level: the xmlErrorLevel for the error
 * @str1: extra string info
 * @str2: extra string info
 * @str3: extra string info
 * @int1: extra int info
 * @msg:  the message to display/transmit
 * @ap:  extra parameters for the message display
 *
 * Raise a parser error.
 */
void
xmlCtxtVErr(xmlParserCtxtPtr ctxt, xmlNodePtr node, xmlErrorDomain domain,
            xmlParserErrors code, xmlErrorLevel level,
            const xmlChar *str1, const xmlChar *str2, const xmlChar *str3,
            int int1, const char *msg, va_list ap)
{
    xmlStructuredErrorFunc schannel = NULL;
    xmlGenericErrorFunc channel = NULL;
    void *data = NULL;
    const char *file = NULL;
    int line = 0;
    int col = 0;
    int res;

    if (code == XML_ERR_NO_MEMORY) {
        xmlCtxtErrMemory(ctxt);
        return;
    }

    if (ctxt == NULL) {
        res = xmlVRaiseError(NULL, NULL, NULL, NULL, node, domain, code,
                             level, NULL, 0, (const char *) str1,
                             (const char *) str2, (const char *) str3,
                             int1, 0, msg, ap);
        if (res < 0)
            xmlRaiseMemoryError(NULL, NULL, NULL, XML_FROM_PARSER, NULL);

        return;
    }

    if (PARSER_STOPPED(ctxt))
	return;

    /* Don't overwrite catastrophic errors */
    if (xmlCtxtIsCatastrophicError(ctxt))
        return;

    if (level == XML_ERR_WARNING) {
        if (ctxt->nbWarnings >= XML_MAX_ERRORS)
            return;
        ctxt->nbWarnings += 1;
    } else {
        /* Report at least one fatal error. */
        if ((ctxt->nbErrors >= XML_MAX_ERRORS) &&
            ((level < XML_ERR_FATAL) || (ctxt->wellFormed == 0)) &&
            (!xmlIsCatastrophicError(level, code)))
            return;
        ctxt->nbErrors += 1;
    }

    if (((ctxt->options & XML_PARSE_NOERROR) == 0) &&
        ((level != XML_ERR_WARNING) ||
         ((ctxt->options & XML_PARSE_NOWARNING) == 0))) {
        if (ctxt->errorHandler) {
            schannel = ctxt->errorHandler;
            data = ctxt->errorCtxt;
        } else if ((ctxt->sax->initialized == XML_SAX2_MAGIC) &&
            (ctxt->sax->serror != NULL)) {
            schannel = ctxt->sax->serror;
            data = ctxt->userData;
        } else if ((domain == XML_FROM_VALID) || (domain == XML_FROM_DTD)) {
            if (level == XML_ERR_WARNING)
                channel = ctxt->vctxt.warning;
            else
                channel = ctxt->vctxt.error;
            data = ctxt->vctxt.userData;
        } else {
            if (level == XML_ERR_WARNING)
                channel = ctxt->sax->warning;
            else
                channel = ctxt->sax->error;
            data = ctxt->userData;
        }
    }

    if (ctxt->input != NULL) {
        xmlParserInputPtr input = ctxt->input;

        if ((input->filename == NULL) &&
            (ctxt->inputNr > 1)) {
            input = ctxt->inputTab[ctxt->inputNr - 2];
        }
        file = input->filename;
        line = input->line;
        col = input->col;
    }

    res = xmlVRaiseError(schannel, channel, data, ctxt, node, domain, code,
                         level, file, line, (const char *) str1,
                         (const char *) str2, (const char *) str3, int1, col,
                         msg, ap);

    if (res < 0) {
        xmlCtxtErrMemory(ctxt);
        return;
    }

    if (level >= XML_ERR_ERROR)
        ctxt->errNo = code;
    if (level == XML_ERR_FATAL) {
        ctxt->wellFormed = 0;

        if (xmlCtxtIsCatastrophicError(ctxt))
            ctxt->disableSAX = 2; /* stop parser */
        else if (ctxt->recovery == 0)
            ctxt->disableSAX = 1;
    }
}

/**
 * xmlCtxtErr:
 * @ctxt:  a parser context
 * @node: the current node or NULL
 * @domain: the domain for the error
 * @code: the code for the error
 * @level: the xmlErrorLevel for the error
 * @str1: extra string info
 * @str2: extra string info
 * @str3: extra string info
 * @int1: extra int info
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 *
 * Raise a parser error.
 */
void
xmlCtxtErr(xmlParserCtxtPtr ctxt, xmlNodePtr node, xmlErrorDomain domain,
           xmlParserErrors code, xmlErrorLevel level,
           const xmlChar *str1, const xmlChar *str2, const xmlChar *str3,
           int int1, const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    xmlCtxtVErr(ctxt, node, domain, code, level,
                str1, str2, str3, int1, msg, ap);
    va_end(ap);
}

/**
 * xmlCtxtGetStatus:
 * @ctxt:  an XML parser context
 *
 * Get well-formedness and validation status after parsing. Also
 * reports catastrophic errors which are not related to parsing
 * like out-of-memory, I/O or other errors.
 *
 * Available since 2.14.0.
 *
 * Returns a bitmask of XML_STATUS_* flags ORed together.
 */
xmlParserStatus
xmlCtxtGetStatus(xmlParserCtxt *ctxt) {
    xmlParserStatus bits = 0;

    if (xmlCtxtIsCatastrophicError(ctxt)) {
        bits |= XML_STATUS_CATASTROPHIC_ERROR |
                XML_STATUS_NOT_WELL_FORMED |
                XML_STATUS_NOT_NS_WELL_FORMED;
        if ((ctxt != NULL) && (ctxt->validate))
            bits |= XML_STATUS_DTD_VALIDATION_FAILED;

        return(bits);
    }

    if (!ctxt->wellFormed)
        bits |= XML_STATUS_NOT_WELL_FORMED;
    if (!ctxt->nsWellFormed)
        bits |= XML_STATUS_NOT_NS_WELL_FORMED;
    if ((ctxt->validate) && (!ctxt->valid))
        bits |= XML_STATUS_DTD_VALIDATION_FAILED;

    return(bits);
}

/**
 * xmlFatalErr:
 * @ctxt:  an XML parser context
 * @code:  the error number
 * @info:  extra information string
 *
 * Handle a fatal parser error, i.e. violating Well-Formedness constraints
 */
void
xmlFatalErr(xmlParserCtxtPtr ctxt, xmlParserErrors code, const char *info)
{
    const char *errmsg;

    errmsg = xmlErrString(code);

    if (info == NULL) {
        xmlCtxtErr(ctxt, NULL, XML_FROM_PARSER, code, XML_ERR_FATAL,
                   NULL, NULL, NULL, 0, "%s\n", errmsg);
    } else {
        xmlCtxtErr(ctxt, NULL, XML_FROM_PARSER, code, XML_ERR_FATAL,
                   (const xmlChar *) info, NULL, NULL, 0,
                   "%s: %s\n", errmsg, info);
    }
}

/**
 * xmlIsLetter:
 * @c:  an unicode character (int)
 *
 * DEPRECATED: Internal function, don't use.
 *
 * Check whether the character is allowed by the production
 * [84] Letter ::= BaseChar | Ideographic
 *
 * Returns 0 if not, non-zero otherwise
 */
int
xmlIsLetter(int c) {
    return(IS_BASECHAR(c) || IS_IDEOGRAPHIC(c));
}

/************************************************************************
 *									*
 *		Input handling functions for progressive parsing	*
 *									*
 ************************************************************************/

/* we need to keep enough input to show errors in context */
#define LINE_LEN        80

/**
 * xmlHaltParser:
 * @ctxt:  an XML parser context
 *
 * Blocks further parser processing don't override error
 * for internal use
 */
void
xmlHaltParser(xmlParserCtxtPtr ctxt) {
    if (ctxt == NULL)
        return;
    ctxt->instate = XML_PARSER_EOF; /* TODO: Remove after refactoring */
    ctxt->disableSAX = 2;
}

/**
 * xmlParserInputRead:
 * @in:  an XML parser input
 * @len:  an indicative size for the lookahead
 *
 * DEPRECATED: This function was internal and is deprecated.
 *
 * Returns -1 as this is an error to use it.
 */
int
xmlParserInputRead(xmlParserInputPtr in ATTRIBUTE_UNUSED, int len ATTRIBUTE_UNUSED) {
    return(-1);
}

/**
 * xmlParserGrow:
 * @ctxt:  an XML parser context
 *
 * Grow the input buffer.
 *
 * Returns the number of bytes read or -1 in case of error.
 */
int
xmlParserGrow(xmlParserCtxtPtr ctxt) {
    xmlParserInputPtr in = ctxt->input;
    xmlParserInputBufferPtr buf = in->buf;
    size_t curEnd = in->end - in->cur;
    size_t curBase = in->cur - in->base;
    size_t maxLength = (ctxt->options & XML_PARSE_HUGE) ?
                       XML_MAX_HUGE_LENGTH :
                       XML_MAX_LOOKUP_LIMIT;
    int ret;

    if (buf == NULL)
        return(0);
    /* Don't grow push parser buffer. */
    if (PARSER_PROGRESSIVE(ctxt))
        return(0);
    /* Don't grow memory buffers. */
    if ((buf->encoder == NULL) && (buf->readcallback == NULL))
        return(0);
    if (buf->error != 0)
        return(-1);

    if (curBase > maxLength) {
        xmlFatalErr(ctxt, XML_ERR_RESOURCE_LIMIT,
                    "Buffer size limit exceeded, try XML_PARSE_HUGE\n");
        xmlHaltParser(ctxt);
	return(-1);
    }

    if (curEnd >= INPUT_CHUNK)
        return(0);

    ret = xmlParserInputBufferGrow(buf, INPUT_CHUNK);
    xmlBufUpdateInput(buf->buffer, in, curBase);

    if (ret < 0) {
        xmlCtxtErrIO(ctxt, buf->error, NULL);
    }

    return(ret);
}

/**
 * xmlParserCheckEOF:
 * @ctxt:  parser ctxt
 * @code:  error code
 *
 * Raises an error with @code if the input wasn't consumed
 * completely.
 */
void
xmlParserCheckEOF(xmlParserCtxtPtr ctxt, xmlParserErrors code) {
    xmlParserInputPtr in = ctxt->input;
    xmlParserInputBufferPtr buf;

    if (ctxt->errNo != XML_ERR_OK)
        return;

    if (in->cur < in->end) {
        xmlFatalErr(ctxt, code, NULL);
        return;
    }

    buf = in->buf;
    if ((buf != NULL) && (buf->encoder != NULL)) {
        size_t curBase = in->cur - in->base;
        size_t sizeOut = 64;
        xmlCharEncError ret;

        /*
         * Check for truncated multi-byte sequence
         */
        ret = xmlCharEncInput(buf, &sizeOut, /* flush */ 1);
        xmlBufUpdateInput(buf->buffer, in, curBase);
        if (ret != XML_ENC_ERR_SUCCESS) {
            xmlCtxtErrIO(ctxt, buf->error, NULL);
            return;
        }

        /* Shouldn't happen */
        if (in->cur < in->end)
            xmlFatalErr(ctxt, XML_ERR_INTERNAL_ERROR, "expected EOF");
    }
}

/**
 * xmlParserInputGrow:
 * @in:  an XML parser input
 * @len:  an indicative size for the lookahead
 *
 * DEPRECATED: Don't use.
 *
 * This function increase the input for the parser. It tries to
 * preserve pointers to the input buffer, and keep already read data
 *
 * Returns the amount of char read, or -1 in case of error, 0 indicate the
 * end of this entity
 */
int
xmlParserInputGrow(xmlParserInputPtr in, int len) {
    int ret;
    size_t indx;

    if ((in == NULL) || (len < 0)) return(-1);
    if (in->buf == NULL) return(-1);
    if (in->base == NULL) return(-1);
    if (in->cur == NULL) return(-1);
    if (in->buf->buffer == NULL) return(-1);

    /* Don't grow memory buffers. */
    if ((in->buf->encoder == NULL) && (in->buf->readcallback == NULL))
        return(0);

    indx = in->cur - in->base;
    if (xmlBufUse(in->buf->buffer) > (unsigned int) indx + INPUT_CHUNK) {
        return(0);
    }
    ret = xmlParserInputBufferGrow(in->buf, len);

    in->base = xmlBufContent(in->buf->buffer);
    if (in->base == NULL) {
        in->base = BAD_CAST "";
        in->cur = in->base;
        in->end = in->base;
        return(-1);
    }
    in->cur = in->base + indx;
    in->end = xmlBufEnd(in->buf->buffer);

    return(ret);
}

/**
 * xmlParserShrink:
 * @ctxt:  an XML parser context
 *
 * Shrink the input buffer.
 */
void
xmlParserShrink(xmlParserCtxtPtr ctxt) {
    xmlParserInputPtr in = ctxt->input;
    xmlParserInputBufferPtr buf = in->buf;
    size_t used, res;

    if (buf == NULL)
        return;

    used = in->cur - in->base;

    if (used > LINE_LEN) {
        res = xmlBufShrink(buf->buffer, used - LINE_LEN);

        if (res > 0) {
            used -= res;
            if ((res > ULONG_MAX) ||
                (in->consumed > ULONG_MAX - (unsigned long)res))
                in->consumed = ULONG_MAX;
            else
                in->consumed += res;
        }

        xmlBufUpdateInput(buf->buffer, in, used);
    }
}

/**
 * xmlParserInputShrink:
 * @in:  an XML parser input
 *
 * DEPRECATED: Don't use.
 *
 * This function removes used input for the parser.
 */
void
xmlParserInputShrink(xmlParserInputPtr in) {
    size_t used;
    size_t ret;

    if (in == NULL) return;
    if (in->buf == NULL) return;
    if (in->base == NULL) return;
    if (in->cur == NULL) return;
    if (in->buf->buffer == NULL) return;

    used = in->cur - in->base;

    if (used > LINE_LEN) {
	ret = xmlBufShrink(in->buf->buffer, used - LINE_LEN);
	if (ret > 0) {
            used -= ret;
            if ((ret > ULONG_MAX) ||
                (in->consumed > ULONG_MAX - (unsigned long)ret))
                in->consumed = ULONG_MAX;
            else
                in->consumed += ret;
	}

        xmlBufUpdateInput(in->buf->buffer, in, used);
    }
}

/************************************************************************
 *									*
 *		UTF8 character input and related functions		*
 *									*
 ************************************************************************/

/**
 * xmlNextChar:
 * @ctxt:  the XML parser context
 *
 * DEPRECATED: Internal function, do not use.
 *
 * Skip to the next char input char.
 */

void
xmlNextChar(xmlParserCtxtPtr ctxt)
{
    const unsigned char *cur;
    size_t avail;
    int c;

    if ((ctxt == NULL) || (ctxt->input == NULL))
        return;

    avail = ctxt->input->end - ctxt->input->cur;

    if (avail < INPUT_CHUNK) {
        xmlParserGrow(ctxt);
        if (ctxt->input->cur >= ctxt->input->end)
            return;
        avail = ctxt->input->end - ctxt->input->cur;
    }

    cur = ctxt->input->cur;
    c = *cur;

    if (c < 0x80) {
        if (c == '\n') {
            ctxt->input->cur++;
            ctxt->input->line++;
            ctxt->input->col = 1;
        } else if (c == '\r') {
            /*
             *   2.11 End-of-Line Handling
             *   the literal two-character sequence "#xD#xA" or a standalone
             *   literal #xD, an XML processor must pass to the application
             *   the single character #xA.
             */
            ctxt->input->cur += ((cur[1] == '\n') ? 2 : 1);
            ctxt->input->line++;
            ctxt->input->col = 1;
            return;
        } else {
            ctxt->input->cur++;
            ctxt->input->col++;
        }
    } else {
        ctxt->input->col++;

        if ((avail < 2) || (cur[1] & 0xc0) != 0x80)
            goto encoding_error;

        if (c < 0xe0) {
            /* 2-byte code */
            if (c < 0xc2)
                goto encoding_error;
            ctxt->input->cur += 2;
        } else {
            unsigned int val = (c << 8) | cur[1];

            if ((avail < 3) || (cur[2] & 0xc0) != 0x80)
                goto encoding_error;

            if (c < 0xf0) {
                /* 3-byte code */
                if ((val < 0xe0a0) || ((val >= 0xeda0) && (val < 0xee00)))
                    goto encoding_error;
                ctxt->input->cur += 3;
            } else {
                if ((avail < 4) || ((cur[3] & 0xc0) != 0x80))
                    goto encoding_error;

                /* 4-byte code */
                if ((val < 0xf090) || (val >= 0xf490))
                    goto encoding_error;
                ctxt->input->cur += 4;
            }
        }
    }

    return;

encoding_error:
    /* Only report the first error */
    if ((ctxt->input->flags & XML_INPUT_ENCODING_ERROR) == 0) {
        xmlCtxtErrIO(ctxt, XML_ERR_INVALID_ENCODING, NULL);
        ctxt->input->flags |= XML_INPUT_ENCODING_ERROR;
    }
    ctxt->input->cur++;
}

/**
 * xmlCurrentChar:
 * @ctxt:  the XML parser context
 * @len:  pointer to the length of the char read
 *
 * DEPRECATED: Internal function, do not use.
 *
 * The current char value, if using UTF-8 this may actually span multiple
 * bytes in the input buffer. Implement the end of line normalization:
 * 2.11 End-of-Line Handling
 * Wherever an external parsed entity or the literal entity value
 * of an internal parsed entity contains either the literal two-character
 * sequence "#xD#xA" or a standalone literal #xD, an XML processor
 * must pass to the application the single character #xA.
 * This behavior can conveniently be produced by normalizing all
 * line breaks to #xA on input, before parsing.)
 *
 * Returns the current char value and its length
 */

int
xmlCurrentChar(xmlParserCtxtPtr ctxt, int *len) {
    const unsigned char *cur;
    size_t avail;
    int c;

    if ((ctxt == NULL) || (len == NULL) || (ctxt->input == NULL)) return(0);

    avail = ctxt->input->end - ctxt->input->cur;

    if (avail < INPUT_CHUNK) {
        xmlParserGrow(ctxt);
        avail = ctxt->input->end - ctxt->input->cur;
    }

    cur = ctxt->input->cur;
    c = *cur;

    if (c < 0x80) {
	/* 1-byte code */
        if (c < 0x20) {
            /*
             *   2.11 End-of-Line Handling
             *   the literal two-character sequence "#xD#xA" or a standalone
             *   literal #xD, an XML processor must pass to the application
             *   the single character #xA.
             */
            if (c == '\r') {
                /*
                 * TODO: This function shouldn't change the 'cur' pointer
                 * as side effect, but the NEXTL macro in parser.c relies
                 * on this behavior when incrementing line numbers.
                 */
                if (cur[1] == '\n')
                    ctxt->input->cur++;
                *len = 1;
                c = '\n';
            } else if (c == 0) {
                if (ctxt->input->cur >= ctxt->input->end) {
                    *len = 0;
                } else {
                    *len = 1;
                    /*
                     * TODO: Null bytes should be handled by callers,
                     * but this can be tricky.
                     */
                    xmlFatalErr(ctxt, XML_ERR_INVALID_CHAR,
                            "Char 0x0 out of allowed range\n");
                }
            } else {
                *len = 1;
            }
        } else {
            *len = 1;
        }

        return(c);
    } else {
        int val;

        if (avail < 2)
            goto incomplete_sequence;
        if ((cur[1] & 0xc0) != 0x80)
            goto encoding_error;

        if (c < 0xe0) {
            /* 2-byte code */
            if (c < 0xc2)
                goto encoding_error;
            val = (c & 0x1f) << 6;
            val |= cur[1] & 0x3f;
            *len = 2;
        } else {
            if (avail < 3)
                goto incomplete_sequence;
            if ((cur[2] & 0xc0) != 0x80)
                goto encoding_error;

            if (c < 0xf0) {
                /* 3-byte code */
                val = (c & 0xf) << 12;
                val |= (cur[1] & 0x3f) << 6;
                val |= cur[2] & 0x3f;
                if ((val < 0x800) || ((val >= 0xd800) && (val < 0xe000)))
                    goto encoding_error;
                *len = 3;
            } else {
                if (avail < 4)
                    goto incomplete_sequence;
                if ((cur[3] & 0xc0) != 0x80)
                    goto encoding_error;

                /* 4-byte code */
                val = (c & 0x0f) << 18;
                val |= (cur[1] & 0x3f) << 12;
                val |= (cur[2] & 0x3f) << 6;
                val |= cur[3] & 0x3f;
                if ((val < 0x10000) || (val >= 0x110000))
                    goto encoding_error;
                *len = 4;
            }
        }

        return(val);
    }

encoding_error:
    /* Only report the first error */
    if ((ctxt->input->flags & XML_INPUT_ENCODING_ERROR) == 0) {
        xmlCtxtErrIO(ctxt, XML_ERR_INVALID_ENCODING, NULL);
        ctxt->input->flags |= XML_INPUT_ENCODING_ERROR;
    }
    *len = 1;
    return(XML_INVALID_CHAR);

incomplete_sequence:
    /*
     * An encoding problem may arise from a truncated input buffer
     * splitting a character in the middle. In that case do not raise
     * an error but return 0. This should only happen when push parsing
     * char data.
     */
    *len = 0;
    return(0);
}

/**
 * xmlStringCurrentChar:
 * @ctxt:  the XML parser context
 * @cur:  pointer to the beginning of the char
 * @len:  pointer to the length of the char read
 *
 * DEPRECATED: Internal function, do not use.
 *
 * The current char value, if using UTF-8 this may actually span multiple
 * bytes in the input buffer.
 *
 * Returns the current char value and its length
 */

int
xmlStringCurrentChar(xmlParserCtxtPtr ctxt ATTRIBUTE_UNUSED,
                     const xmlChar *cur, int *len) {
    int c;

    if ((cur == NULL) || (len == NULL))
        return(0);

    /* cur is zero-terminated, so we can lie about its length. */
    *len = 4;
    c = xmlGetUTF8Char(cur, len);

    return((c < 0) ? 0 : c);
}

/**
 * xmlCopyCharMultiByte:
 * @out:  pointer to an array of xmlChar
 * @val:  the char value
 *
 * DEPRECATED: Internal function, don't use.
 *
 * append the char value in the array
 *
 * Returns the number of xmlChar written
 */
int
xmlCopyCharMultiByte(xmlChar *out, int val) {
    if ((out == NULL) || (val < 0)) return(0);
    /*
     * We are supposed to handle UTF8, check it's valid
     * From rfc2044: encoding of the Unicode values on UTF-8:
     *
     * UCS-4 range (hex.)           UTF-8 octet sequence (binary)
     * 0000 0000-0000 007F   0xxxxxxx
     * 0000 0080-0000 07FF   110xxxxx 10xxxxxx
     * 0000 0800-0000 FFFF   1110xxxx 10xxxxxx 10xxxxxx
     */
    if  (val >= 0x80) {
	xmlChar *savedout = out;
	int bits;
	if (val <   0x800) { *out++= (val >>  6) | 0xC0;  bits=  0; }
	else if (val < 0x10000) { *out++= (val >> 12) | 0xE0;  bits=  6;}
	else if (val < 0x110000)  { *out++= (val >> 18) | 0xF0;  bits=  12; }
	else {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
            xmlAbort("xmlCopyCharMultiByte: codepoint out of range\n");
#endif
	    return(0);
	}
	for ( ; bits >= 0; bits-= 6)
	    *out++= ((val >> bits) & 0x3F) | 0x80 ;
	return (out - savedout);
    }
    *out = val;
    return 1;
}

/**
 * xmlCopyChar:
 * @len:  Ignored, compatibility
 * @out:  pointer to an array of xmlChar
 * @val:  the char value
 *
 * DEPRECATED: Don't use.
 *
 * append the char value in the array
 *
 * Returns the number of xmlChar written
 */

int
xmlCopyChar(int len ATTRIBUTE_UNUSED, xmlChar *out, int val) {
    if ((out == NULL) || (val < 0)) return(0);
    /* the len parameter is ignored */
    if  (val >= 0x80) {
	return(xmlCopyCharMultiByte (out, val));
    }
    *out = val;
    return 1;
}

/************************************************************************
 *									*
 *		Commodity functions to switch encodings			*
 *									*
 ************************************************************************/

/**
 * xmlCtxtSetCharEncConvImpl:
 * @ctxt:  parser context
 * @impl:  callback
 * @vctxt:  user data
 *
 * Installs a custom implementation to convert between character
 * encodings.
 *
 * This bypasses legacy feature like global encoding handlers or
 * encoding aliases.
 *
 * Available since 2.14.0.
 */
void
xmlCtxtSetCharEncConvImpl(xmlParserCtxtPtr ctxt, xmlCharEncConvImpl impl,
                          void *vctxt) {
    if (ctxt == NULL)
        return;

    ctxt->convImpl = impl;
    ctxt->convCtxt = vctxt;
}

static xmlParserErrors
xmlDetectEBCDIC(xmlParserCtxtPtr ctxt, xmlCharEncodingHandlerPtr *hout) {
    xmlChar out[200];
    xmlParserInputPtr input = ctxt->input;
    xmlCharEncodingHandlerPtr handler;
    int inlen, outlen, i;
    xmlParserErrors code;
    xmlCharEncError res;

    *hout = NULL;

    /*
     * To detect the EBCDIC code page, we convert the first 200 bytes
     * to IBM037 (EBCDIC-US) and try to find the encoding declaration.
     */
    code = xmlCreateCharEncodingHandler("IBM037", XML_ENC_INPUT,
            ctxt->convImpl, ctxt->convCtxt, &handler);
    if (code != XML_ERR_OK)
        return(code);
    outlen = sizeof(out) - 1;
    inlen = input->end - input->cur;
    res = xmlEncInputChunk(handler, out, &outlen, input->cur, &inlen,
                           /* flush */ 0);
    /*
     * Return the EBCDIC handler if decoding failed. The error will
     * be reported later.
     */
    if (res < 0)
        goto done;
    out[outlen] = 0;

    for (i = 0; i < outlen; i++) {
        if (out[i] == '>')
            break;
        if ((out[i] == 'e') &&
            (xmlStrncmp(out + i, BAD_CAST "encoding", 8) == 0)) {
            int start, cur, quote;

            i += 8;
            while (IS_BLANK_CH(out[i]))
                i += 1;
            if (out[i++] != '=')
                break;
            while (IS_BLANK_CH(out[i]))
                i += 1;
            quote = out[i++];
            if ((quote != '\'') && (quote != '"'))
                break;
            start = i;
            cur = out[i];
            while (((cur >= 'a') && (cur <= 'z')) ||
                   ((cur >= 'A') && (cur <= 'Z')) ||
                   ((cur >= '0') && (cur <= '9')) ||
                   (cur == '.') || (cur == '_') ||
                   (cur == '-'))
                cur = out[++i];
            if (cur != quote)
                break;
            out[i] = 0;
            xmlCharEncCloseFunc(handler);
            code = xmlCreateCharEncodingHandler((char *) out + start,
                    XML_ENC_INPUT, ctxt->convImpl, ctxt->convCtxt,
                    &handler);
            if (code != XML_ERR_OK)
                return(code);
            *hout = handler;
            return(XML_ERR_OK);
        }
    }

done:
    /*
     * Encoding handlers are stateful, so we have to recreate them.
     */
    xmlCharEncCloseFunc(handler);
    code = xmlCreateCharEncodingHandler("IBM037", XML_ENC_INPUT,
            ctxt->convImpl, ctxt->convCtxt, &handler);
    if (code != XML_ERR_OK)
        return(code);
    *hout = handler;
    return(XML_ERR_OK);
}

/**
 * xmlSwitchEncoding:
 * @ctxt:  the parser context
 * @enc:  the encoding value (number)
 *
 * Use encoding specified by enum to decode input data. This overrides
 * the encoding found in the XML declaration.
 *
 * This function can also be used to override the encoding of chunks
 * passed to xmlParseChunk.
 *
 * Returns 0 in case of success, -1 otherwise
 */
int
xmlSwitchEncoding(xmlParserCtxtPtr ctxt, xmlCharEncoding enc)
{
    xmlCharEncodingHandlerPtr handler = NULL;
    int ret;
    xmlParserErrors code;

    if ((ctxt == NULL) || (ctxt->input == NULL))
        return(-1);

    code = xmlLookupCharEncodingHandler(enc, &handler);
    if (code != 0) {
        xmlFatalErr(ctxt, code, NULL);
        return(-1);
    }

    ret = xmlSwitchToEncoding(ctxt, handler);

    if ((ret >= 0) && (enc == XML_CHAR_ENCODING_NONE)) {
        ctxt->input->flags &= ~XML_INPUT_HAS_ENCODING;
    }

    return(ret);
}

/**
 * xmlSwitchInputEncodingName:
 * @ctxt:  the parser context
 * @input:  the input strea,
 * @encoding:  the encoding name
 *
 * Returns 0 in case of success, -1 otherwise
 */
static int
xmlSwitchInputEncodingName(xmlParserCtxtPtr ctxt, xmlParserInputPtr input,
                           const char *encoding) {
    xmlCharEncodingHandlerPtr handler;
    xmlParserErrors res;

    if (encoding == NULL)
        return(-1);

    res = xmlCreateCharEncodingHandler(encoding, XML_ENC_INPUT,
            ctxt->convImpl, ctxt->convCtxt, &handler);
    if (res == XML_ERR_UNSUPPORTED_ENCODING) {
        xmlWarningMsg(ctxt, XML_ERR_UNSUPPORTED_ENCODING,
                      "Unsupported encoding: %s\n", BAD_CAST encoding, NULL);
        return(-1);
    } else if (res != XML_ERR_OK) {
        xmlFatalErr(ctxt, res, encoding);
        return(-1);
    }

    res  = xmlInputSetEncodingHandler(input, handler);
    if (res != XML_ERR_OK) {
        xmlCtxtErrIO(ctxt, res, NULL);
        return(-1);
    }

    return(0);
}

/**
 * xmlSwitchEncodingName:
 * @ctxt:  the parser context
 * @encoding:  the encoding name
 *
 * Use specified encoding to decode input data. This overrides the
 * encoding found in the XML declaration.
 *
 * This function can also be used to override the encoding of chunks
 * passed to xmlParseChunk.
 *
 * Available since 2.13.0.
 *
 * Returns 0 in case of success, -1 otherwise
 */
int
xmlSwitchEncodingName(xmlParserCtxtPtr ctxt, const char *encoding) {
    if (ctxt == NULL)
        return(-1);

    return(xmlSwitchInputEncodingName(ctxt, ctxt->input, encoding));
}

/**
 * xmlInputSetEncodingHandler:
 * @input:  the input stream
 * @handler:  the encoding handler
 *
 * Use encoding handler to decode input data.
 *
 * Closes the handler on error.
 *
 * Returns an xmlParserErrors code.
 */
xmlParserErrors
xmlInputSetEncodingHandler(xmlParserInputPtr input,
                           xmlCharEncodingHandlerPtr handler) {
    xmlParserInputBufferPtr in;
    xmlBufPtr buf;
    xmlParserErrors code = XML_ERR_OK;

    if ((input == NULL) || (input->buf == NULL)) {
        xmlCharEncCloseFunc(handler);
	return(XML_ERR_ARGUMENT);
    }
    in = input->buf;

    input->flags |= XML_INPUT_HAS_ENCODING;

    /*
     * UTF-8 requires no encoding handler.
     */
    if ((handler != NULL) &&
        (xmlStrcasecmp(BAD_CAST handler->name, BAD_CAST "UTF-8") == 0)) {
        xmlCharEncCloseFunc(handler);
        handler = NULL;
    }

    if (in->encoder == handler)
        return(XML_ERR_OK);

    if (in->encoder != NULL) {
        /*
         * Switching encodings during parsing is a really bad idea,
         * but Chromium can switch between ISO-8859-1 and UTF-16 before
         * separate calls to xmlParseChunk.
         *
         * TODO: We should check whether the "raw" input buffer is empty and
         * convert the old content using the old encoder.
         */

        xmlCharEncCloseFunc(in->encoder);
        in->encoder = handler;
        return(XML_ERR_OK);
    }

    buf = xmlBufCreate(XML_IO_BUFFER_SIZE);
    if (buf == NULL) {
        xmlCharEncCloseFunc(handler);
        return(XML_ERR_NO_MEMORY);
    }

    in->encoder = handler;
    in->raw = in->buffer;
    in->buffer = buf;

    /*
     * Is there already some content down the pipe to convert ?
     */
    if (input->end > input->base) {
        size_t processed;
        size_t nbchars;
        xmlCharEncError res;

        /*
         * Shrink the current input buffer.
         * Move it as the raw buffer and create a new input buffer
         */
        processed = input->cur - input->base;
        xmlBufShrink(in->raw, processed);
        input->consumed += processed;
        in->rawconsumed = processed;

        /*
         * If we're push-parsing, we must convert the whole buffer.
         *
         * If we're pull-parsing, we could be parsing from a huge
         * memory buffer which we don't want to convert completely.
         */
        if (input->flags & XML_INPUT_PROGRESSIVE)
            nbchars = SIZE_MAX;
        else
            nbchars = 4000 /* MINLEN */;
        res = xmlCharEncInput(in, &nbchars, /* flush */ 0);
        if (res != XML_ENC_ERR_SUCCESS)
            code = in->error;
    }

    xmlBufResetInput(in->buffer, input);

    return(code);
}

/**
 * xmlSwitchInputEncoding:
 * @ctxt:  the parser context, only for error reporting
 * @input:  the input stream
 * @handler:  the encoding handler
 *
 * DEPRECATED: Internal function, don't use.
 *
 * Use encoding handler to decode input data.
 *
 * Returns 0 in case of success, -1 otherwise
 */
int
xmlSwitchInputEncoding(xmlParserCtxtPtr ctxt, xmlParserInputPtr input,
                       xmlCharEncodingHandlerPtr handler) {
    xmlParserErrors code = xmlInputSetEncodingHandler(input, handler);

    if (code != XML_ERR_OK) {
        xmlCtxtErrIO(ctxt, code, NULL);
        return(-1);
    }

    return(0);
}

/**
 * xmlSwitchToEncoding:
 * @ctxt:  the parser context
 * @handler:  the encoding handler
 *
 * Use encoding handler to decode input data.
 *
 * This function can be used to enforce the encoding of chunks passed
 * to xmlParseChunk.
 *
 * Returns 0 in case of success, -1 otherwise
 */
int
xmlSwitchToEncoding(xmlParserCtxtPtr ctxt, xmlCharEncodingHandlerPtr handler)
{
    xmlParserErrors code;

    if (ctxt == NULL)
        return(-1);

    code = xmlInputSetEncodingHandler(ctxt->input, handler);
    if (code != XML_ERR_OK) {
        xmlCtxtErrIO(ctxt, code, NULL);
        return(-1);
    }

    return(0);
}

/**
 * xmlDetectEncoding:
 * @ctxt:  the parser context
 *
 * Handle optional BOM, detect and switch to encoding.
 *
 * Assumes that there are at least four bytes in the input buffer.
 */
void
xmlDetectEncoding(xmlParserCtxtPtr ctxt) {
    const xmlChar *in;
    xmlCharEncoding enc;
    int bomSize;
    int autoFlag = 0;

    if (xmlParserGrow(ctxt) < 0)
        return;
    in = ctxt->input->cur;
    if (ctxt->input->end - in < 4)
        return;

    if (ctxt->input->flags & XML_INPUT_HAS_ENCODING) {
        /*
         * If the encoding was already set, only skip the BOM which was
         * possibly decoded to UTF-8.
         */
        if ((in[0] == 0xEF) && (in[1] == 0xBB) && (in[2] == 0xBF)) {
            ctxt->input->cur += 3;
        }

        return;
    }

    enc = XML_CHAR_ENCODING_NONE;
    bomSize = 0;

    /*
     * BOM sniffing and detection of initial bytes of an XML
     * declaration.
     *
     * The HTML5 spec doesn't cover UTF-32 (UCS-4) or EBCDIC.
     */
    switch (in[0]) {
        case 0x00:
            if ((!ctxt->html) &&
                (in[1] == 0x00) && (in[2] == 0x00) && (in[3] == 0x3C)) {
                enc = XML_CHAR_ENCODING_UCS4BE;
                autoFlag = XML_INPUT_AUTO_OTHER;
            } else if ((in[1] == 0x3C) && (in[2] == 0x00) && (in[3] == 0x3F)) {
                /*
                 * TODO: The HTML5 spec requires to check that the
                 * next codepoint is an 'x'.
                 */
                enc = XML_CHAR_ENCODING_UTF16BE;
                autoFlag = XML_INPUT_AUTO_UTF16BE;
            }
            break;

        case 0x3C:
            if (in[1] == 0x00) {
                if ((!ctxt->html) &&
                    (in[2] == 0x00) && (in[3] == 0x00)) {
                    enc = XML_CHAR_ENCODING_UCS4LE;
                    autoFlag = XML_INPUT_AUTO_OTHER;
                } else if ((in[2] == 0x3F) && (in[3] == 0x00)) {
                    /*
                     * TODO: The HTML5 spec requires to check that the
                     * next codepoint is an 'x'.
                     */
                    enc = XML_CHAR_ENCODING_UTF16LE;
                    autoFlag = XML_INPUT_AUTO_UTF16LE;
                }
            }
            break;

        case 0x4C:
	    if ((!ctxt->html) &&
                (in[1] == 0x6F) && (in[2] == 0xA7) && (in[3] == 0x94)) {
	        enc = XML_CHAR_ENCODING_EBCDIC;
                autoFlag = XML_INPUT_AUTO_OTHER;
            }
            break;

        case 0xEF:
            if ((in[1] == 0xBB) && (in[2] == 0xBF)) {
                enc = XML_CHAR_ENCODING_UTF8;
                autoFlag = XML_INPUT_AUTO_UTF8;
                bomSize = 3;
            }
            break;

        case 0xFE:
            if (in[1] == 0xFF) {
                enc = XML_CHAR_ENCODING_UTF16BE;
                autoFlag = XML_INPUT_AUTO_UTF16BE;
                bomSize = 2;
            }
            break;

        case 0xFF:
            if (in[1] == 0xFE) {
                enc = XML_CHAR_ENCODING_UTF16LE;
                autoFlag = XML_INPUT_AUTO_UTF16LE;
                bomSize = 2;
            }
            break;
    }

    if (bomSize > 0) {
        ctxt->input->cur += bomSize;
    }

    if (enc != XML_CHAR_ENCODING_NONE) {
        ctxt->input->flags |= autoFlag;

        if (enc == XML_CHAR_ENCODING_EBCDIC) {
            xmlCharEncodingHandlerPtr handler;
            xmlParserErrors res;

            res = xmlDetectEBCDIC(ctxt, &handler);
            if (res != XML_ERR_OK) {
                xmlFatalErr(ctxt, res, "detecting EBCDIC\n");
            } else {
                xmlSwitchToEncoding(ctxt, handler);
            }
        } else {
            xmlSwitchEncoding(ctxt, enc);
        }
    }
}

/**
 * xmlSetDeclaredEncoding:
 * @ctxt:  the parser context
 * @encoding:  declared encoding
 *
 * Set the encoding from a declaration in the document.
 *
 * If no encoding was set yet, switch the encoding. Otherwise, only warn
 * about encoding mismatches.
 *
 * Takes ownership of 'encoding'.
 */
void
xmlSetDeclaredEncoding(xmlParserCtxtPtr ctxt, xmlChar *encoding) {
    if (((ctxt->input->flags & XML_INPUT_HAS_ENCODING) == 0) &&
        ((ctxt->options & XML_PARSE_IGNORE_ENC) == 0)) {
        xmlCharEncodingHandlerPtr handler;
        xmlParserErrors res;

        /*
         * xmlSwitchEncodingName treats unsupported encodings as
         * warnings, but we want it to be an error in an encoding
         * declaration.
         */
        res = xmlCreateCharEncodingHandler((const char *) encoding,
                XML_ENC_INPUT, ctxt->convImpl, ctxt->convCtxt, &handler);
        if (res != XML_ERR_OK) {
            xmlFatalErr(ctxt, res, (const char *) encoding);
            xmlFree(encoding);
            return;
        }

        res  = xmlInputSetEncodingHandler(ctxt->input, handler);
        if (res != XML_ERR_OK) {
            xmlCtxtErrIO(ctxt, res, NULL);
            xmlFree(encoding);
            return;
        }

        ctxt->input->flags |= XML_INPUT_USES_ENC_DECL;
    } else if (ctxt->input->flags & XML_INPUT_AUTO_ENCODING) {
        static const char *allowedUTF8[] = {
            "UTF-8", "UTF8", NULL
        };
        static const char *allowedUTF16LE[] = {
            "UTF-16", "UTF-16LE", "UTF16", NULL
        };
        static const char *allowedUTF16BE[] = {
            "UTF-16", "UTF-16BE", "UTF16", NULL
        };
        const char **allowed = NULL;
        const char *autoEnc = NULL;

        switch (ctxt->input->flags & XML_INPUT_AUTO_ENCODING) {
            case XML_INPUT_AUTO_UTF8:
                allowed = allowedUTF8;
                autoEnc = "UTF-8";
                break;
            case XML_INPUT_AUTO_UTF16LE:
                allowed = allowedUTF16LE;
                autoEnc = "UTF-16LE";
                break;
            case XML_INPUT_AUTO_UTF16BE:
                allowed = allowedUTF16BE;
                autoEnc = "UTF-16BE";
                break;
        }

        if (allowed != NULL) {
            const char **p;
            int match = 0;

            for (p = allowed; *p != NULL; p++) {
                if (xmlStrcasecmp(encoding, BAD_CAST *p) == 0) {
                    match = 1;
                    break;
                }
            }

            if (match == 0) {
                xmlWarningMsg(ctxt, XML_WAR_ENCODING_MISMATCH,
                              "Encoding '%s' doesn't match "
                              "auto-detected '%s'\n",
                              encoding, BAD_CAST autoEnc);
                xmlFree(encoding);
                encoding = xmlStrdup(BAD_CAST autoEnc);
                if (encoding == NULL)
                    xmlCtxtErrMemory(ctxt);
            }
        }
    }

    if (ctxt->encoding != NULL)
        xmlFree((xmlChar *) ctxt->encoding);
    ctxt->encoding = encoding;
}

/**
 * xmlCtxtGetDeclaredEncoding:
 * @ctxt:  parser context
 *
 * Available since 2.14.0.
 *
 * Returns the encoding from the encoding declaration. This can differ
 * from the actual encoding.
 */
const xmlChar *
xmlCtxtGetDeclaredEncoding(xmlParserCtxtPtr ctxt) {
    if (ctxt == NULL)
        return(NULL);

    return(ctxt->encoding);
}

/**
 * xmlGetActualEncoding:
 * @ctxt:  the parser context
 *
 * Returns the actual used to parse the document. This can differ from
 * the declared encoding.
 */
const xmlChar *
xmlGetActualEncoding(xmlParserCtxtPtr ctxt) {
    const xmlChar *encoding = NULL;

    if ((ctxt->input->flags & XML_INPUT_USES_ENC_DECL) ||
        (ctxt->input->flags & XML_INPUT_AUTO_ENCODING)) {
        /* Preserve encoding exactly */
        encoding = ctxt->encoding;
    } else if ((ctxt->input->buf) && (ctxt->input->buf->encoder)) {
        encoding = BAD_CAST ctxt->input->buf->encoder->name;
    } else if (ctxt->input->flags & XML_INPUT_HAS_ENCODING) {
        encoding = BAD_CAST "UTF-8";
    }

    return(encoding);
}

/************************************************************************
 *									*
 *	Commodity functions to handle entities processing		*
 *									*
 ************************************************************************/

/**
 * xmlFreeInputStream:
 * @input:  an xmlParserInputPtr
 *
 * Free up an input stream.
 */
void
xmlFreeInputStream(xmlParserInputPtr input) {
    if (input == NULL) return;

    if (input->filename != NULL) xmlFree((char *) input->filename);
    if (input->version != NULL) xmlFree((char *) input->version);
    if ((input->free != NULL) && (input->base != NULL))
        input->free((xmlChar *) input->base);
    if (input->buf != NULL)
        xmlFreeParserInputBuffer(input->buf);
    xmlFree(input);
}

/**
 * xmlNewInputStream:
 * @ctxt:  an XML parser context
 *
 * DEPRECATED: Use xmlNewInputFromUrl or similar functions.
 *
 * Create a new input stream structure.
 *
 * Returns the new input stream or NULL
 */
xmlParserInputPtr
xmlNewInputStream(xmlParserCtxtPtr ctxt) {
    xmlParserInputPtr input;

    input = (xmlParserInputPtr) xmlMalloc(sizeof(xmlParserInput));
    if (input == NULL) {
        xmlCtxtErrMemory(ctxt);
	return(NULL);
    }
    memset(input, 0, sizeof(xmlParserInput));
    input->line = 1;
    input->col = 1;

    return(input);
}

/**
 * xmlCtxtNewInputFromUrl:
 * @ctxt:  parser context
 * @url:  filename or URL
 * @publicId:  publid ID from doctype (optional)
 * @encoding:  character encoding (optional)
 * @flags:  unused, pass 0
 *
 * Creates a new parser input from the filesystem, the network or
 * a user-defined resource loader.
 *
 * Returns a new parser input.
 */
xmlParserInputPtr
xmlCtxtNewInputFromUrl(xmlParserCtxtPtr ctxt, const char *url,
                       const char *publicId, const char *encoding,
                       xmlParserInputFlags flags ATTRIBUTE_UNUSED) {
    xmlParserInputPtr input;

    if ((ctxt == NULL) || (url == NULL))
	return(NULL);

    input = xmlLoadResource(ctxt, url, publicId, XML_RESOURCE_MAIN_DOCUMENT);
    if (input == NULL)
        return(NULL);

    if (encoding != NULL)
        xmlSwitchInputEncodingName(ctxt, input, encoding);

    return(input);
}

/**
 * xmlNewInputInternal:
 * @buf:  parser input buffer
 * @filename:  filename or URL
 *
 * Internal helper function.
 *
 * Returns a new parser input.
 */
static xmlParserInputPtr
xmlNewInputInternal(xmlParserInputBufferPtr buf, const char *filename) {
    xmlParserInputPtr input;

    input = (xmlParserInputPtr) xmlMalloc(sizeof(xmlParserInput));
    if (input == NULL) {
	xmlFreeParserInputBuffer(buf);
	return(NULL);
    }
    memset(input, 0, sizeof(xmlParserInput));
    input->line = 1;
    input->col = 1;

    input->buf = buf;
    xmlBufResetInput(input->buf->buffer, input);

    if (filename != NULL) {
        input->filename = xmlMemStrdup(filename);
        if (input->filename == NULL) {
            xmlFreeInputStream(input);
            return(NULL);
        }
    }

    return(input);
}

/**
 * xmlNewInputFromMemory:
 * @url:  base URL (optional)
 * @mem:  pointer to char array
 * @size:  size of array
 * @flags:  optimization hints
 *
 * Creates a new parser input to read from a memory area.
 *
 * @url is used as base to resolve external entities and for
 * error reporting.
 *
 * If the XML_INPUT_BUF_STATIC flag is set, the memory area must
 * stay unchanged until parsing has finished. This can avoid
 * temporary copies.
 *
 * If the XML_INPUT_BUF_ZERO_TERMINATED flag is set, the memory
 * area must contain a zero byte after the buffer at position @size.
 * This can avoid temporary copies.
 *
 * Available since 2.14.0.
 *
 * Returns a new parser input or NULL if a memory allocation failed.
 */
xmlParserInputPtr
xmlNewInputFromMemory(const char *url, const void *mem, size_t size,
                      xmlParserInputFlags flags) {
    xmlParserInputBufferPtr buf;

    if (mem == NULL)
	return(NULL);

    buf = xmlNewInputBufferMemory(mem, size, flags, XML_CHAR_ENCODING_NONE);
    if (buf == NULL)
        return(NULL);

    return(xmlNewInputInternal(buf, url));
}

/**
 * xmlCtxtNewInputFromMemory:
 * @ctxt:  parser context
 * @url:  base URL (optional)
 * @mem:  pointer to char array
 * @size:  size of array
 * @encoding:  character encoding (optional)
 * @flags:  optimization hints
 *
 * Returns a new parser input or NULL in case of error.
 */
xmlParserInputPtr
xmlCtxtNewInputFromMemory(xmlParserCtxtPtr ctxt, const char *url,
                          const void *mem, size_t size,
                          const char *encoding, xmlParserInputFlags flags) {
    xmlParserInputPtr input;

    if ((ctxt == NULL) || (mem == NULL))
	return(NULL);

    input = xmlNewInputFromMemory(url, mem, size, flags);
    if (input == NULL) {
        xmlCtxtErrMemory(ctxt);
        return(NULL);
    }

    if (encoding != NULL)
        xmlSwitchInputEncodingName(ctxt, input, encoding);

    return(input);
}

/**
 * xmlNewInputFromString:
 * @url:  base URL (optional)
 * @str:  zero-terminated string
 * @flags:  optimization hints
 *
 * Creates a new parser input to read from a zero-terminated string.
 *
 * @url is used as base to resolve external entities and for
 * error reporting.
 *
 * If the XML_INPUT_BUF_STATIC flag is set, the string must
 * stay unchanged until parsing has finished. This can avoid
 * temporary copies.
 *
 * Available since 2.14.0.
 *
 * Returns a new parser input or NULL if a memory allocation failed.
 */
xmlParserInputPtr
xmlNewInputFromString(const char *url, const char *str,
                      xmlParserInputFlags flags) {
    xmlParserInputBufferPtr buf;

    if (str == NULL)
	return(NULL);

    buf = xmlNewInputBufferString(str, flags);
    if (buf == NULL)
        return(NULL);

    return(xmlNewInputInternal(buf, url));
}

/**
 * xmlCtxtNewInputFromString:
 * @ctxt:  parser context
 * @url:  base URL (optional)
 * @str:  zero-terminated string
 * @encoding:  character encoding (optional)
 * @flags:  optimization hints
 *
 * Returns a new parser input.
 */
xmlParserInputPtr
xmlCtxtNewInputFromString(xmlParserCtxtPtr ctxt, const char *url,
                          const char *str, const char *encoding,
                          xmlParserInputFlags flags) {
    xmlParserInputPtr input;

    if ((ctxt == NULL) || (str == NULL))
	return(NULL);

    input = xmlNewInputFromString(url, str, flags);
    if (input == NULL) {
        xmlCtxtErrMemory(ctxt);
        return(NULL);
    }

    if (encoding != NULL)
        xmlSwitchInputEncodingName(ctxt, input, encoding);

    return(input);
}

/**
 * xmlNewInputFromFd:
 * @url:  base URL (optional)
 * @fd:  file descriptor
 * @flags:  input flags
 *
 * Creates a new parser input to read from a zero-terminated string.
 *
 * @url is used as base to resolve external entities and for
 * error reporting.
 *
 * @fd is closed after parsing has finished.
 *
 * Supported @flags are XML_INPUT_UNZIP to decompress data
 * automatically. This feature is deprecated and will be removed
 * in a future release.
 *
 * Available since 2.14.0.
 *
 * Returns a new parser input or NULL if a memory allocation failed.
 */
xmlParserInputPtr
xmlNewInputFromFd(const char *url, int fd, xmlParserInputFlags flags) {
    xmlParserInputBufferPtr buf;

    if (fd < 0)
	return(NULL);

    buf = xmlAllocParserInputBuffer(XML_CHAR_ENCODING_NONE);
    if (buf == NULL)
        return(NULL);

    if (xmlInputFromFd(buf, fd, flags) != XML_ERR_OK) {
        xmlFreeParserInputBuffer(buf);
        return(NULL);
    }

    return(xmlNewInputInternal(buf, url));
}

/**
 * xmlCtxtNewInputFromFd:
 * @ctxt:  parser context
 * @url:  base URL (optional)
 * @fd:  file descriptor
 * @encoding:  character encoding (optional)
 * @flags:  unused, pass 0
 *
 * Returns a new parser input.
 */
xmlParserInputPtr
xmlCtxtNewInputFromFd(xmlParserCtxtPtr ctxt, const char *url,
                      int fd, const char *encoding,
                      xmlParserInputFlags flags) {
    xmlParserInputPtr input;

    if ((ctxt == NULL) || (fd < 0))
	return(NULL);

    if (ctxt->options & XML_PARSE_UNZIP)
        flags |= XML_INPUT_UNZIP;

    input = xmlNewInputFromFd(url, fd, flags);
    if (input == NULL) {
	xmlCtxtErrMemory(ctxt);
        return(NULL);
    }

    if (encoding != NULL)
        xmlSwitchInputEncodingName(ctxt, input, encoding);

    return(input);
}

/**
 * xmlNewInputFromIO:
 * @url:  base URL (optional)
 * @ioRead:  read callback
 * @ioClose:  close callback (optional)
 * @ioCtxt:  IO context
 * @flags:  unused, pass 0
 *
 * Creates a new parser input to read from input callbacks and
 * cintext.
 *
 * @url is used as base to resolve external entities and for
 * error reporting.
 *
 * @ioRead is called to read new data into a provided buffer.
 * It must return the number of bytes written into the buffer
 * ot a negative xmlParserErrors code on failure.
 *
 * @ioClose is called after parsing has finished.
 *
 * @ioCtxt is an opaque pointer passed to the callbacks.
 *
 * Available since 2.14.0.
 *
 * Returns a new parser input or NULL if a memory allocation failed.
 */
xmlParserInputPtr
xmlNewInputFromIO(const char *url, xmlInputReadCallback ioRead,
                  xmlInputCloseCallback ioClose, void *ioCtxt,
                  xmlParserInputFlags flags ATTRIBUTE_UNUSED) {
    xmlParserInputBufferPtr buf;

    if (ioRead == NULL)
	return(NULL);

    buf = xmlAllocParserInputBuffer(XML_CHAR_ENCODING_NONE);
    if (buf == NULL) {
        if (ioClose != NULL)
            ioClose(ioCtxt);
        return(NULL);
    }

    buf->context = ioCtxt;
    buf->readcallback = ioRead;
    buf->closecallback = ioClose;

    return(xmlNewInputInternal(buf, url));
}

/**
 * xmlCtxtNewInputFromIO:
 * @ctxt:  parser context
 * @url:  base URL (optional)
 * @ioRead:  read callback
 * @ioClose:  close callback (optional)
 * @ioCtxt:  IO context
 * @encoding:  character encoding (optional)
 * @flags:  unused, pass 0
 *
 * Returns a new parser input.
 */
xmlParserInputPtr
xmlCtxtNewInputFromIO(xmlParserCtxtPtr ctxt, const char *url,
                      xmlInputReadCallback ioRead,
                      xmlInputCloseCallback ioClose,
                      void *ioCtxt, const char *encoding,
                      xmlParserInputFlags flags) {
    xmlParserInputPtr input;

    if ((ctxt == NULL) || (ioRead == NULL))
	return(NULL);

    input = xmlNewInputFromIO(url, ioRead, ioClose, ioCtxt, flags);
    if (input == NULL) {
        xmlCtxtErrMemory(ctxt);
        return(NULL);
    }

    if (encoding != NULL)
        xmlSwitchInputEncodingName(ctxt, input, encoding);

    return(input);
}

/**
 * xmlNewPushInput:
 * @url:  base URL (optional)
 * @chunk:  pointer to char array
 * @size:  size of array
 *
 * Creates a new parser input for a push parser.
 *
 * Returns a new parser input or NULL if a memory allocation failed.
 */
xmlParserInputPtr
xmlNewPushInput(const char *url, const char *chunk, int size) {
    xmlParserInputBufferPtr buf;
    xmlParserInputPtr input;

    buf = xmlAllocParserInputBuffer(XML_CHAR_ENCODING_NONE);
    if (buf == NULL)
        return(NULL);

    input = xmlNewInputInternal(buf, url);
    if (input == NULL)
	return(NULL);

    input->flags |= XML_INPUT_PROGRESSIVE;

    if ((size > 0) && (chunk != NULL)) {
        int res;

	res = xmlParserInputBufferPush(input->buf, size, chunk);
        xmlBufResetInput(input->buf->buffer, input);
        if (res < 0) {
            xmlFreeInputStream(input);
            return(NULL);
        }
    }

    return(input);
}

/**
 * xmlNewIOInputStream:
 * @ctxt:  an XML parser context
 * @buf:  an input buffer
 * @enc:  the charset encoding if known
 *
 * Create a new input stream structure encapsulating the @input into
 * a stream suitable for the parser.
 *
 * Returns the new input stream or NULL
 */
xmlParserInputPtr
xmlNewIOInputStream(xmlParserCtxtPtr ctxt, xmlParserInputBufferPtr buf,
	            xmlCharEncoding enc) {
    xmlParserInputPtr input;
    const char *encoding;

    if ((ctxt == NULL) || (buf == NULL))
        return(NULL);

    input = xmlNewInputInternal(buf, NULL);
    if (input == NULL) {
        xmlCtxtErrMemory(ctxt);
	return(NULL);
    }

    encoding = xmlGetCharEncodingName(enc);
    if (encoding != NULL)
        xmlSwitchInputEncodingName(ctxt, input, encoding);

    return(input);
}

/**
 * xmlNewEntityInputStream:
 * @ctxt:  an XML parser context
 * @ent:  an Entity pointer
 *
 * DEPRECATED: Internal function, do not use.
 *
 * Create a new input stream based on an xmlEntityPtr
 *
 * Returns the new input stream or NULL
 */
xmlParserInputPtr
xmlNewEntityInputStream(xmlParserCtxtPtr ctxt, xmlEntityPtr ent) {
    xmlParserInputPtr input;

    if ((ctxt == NULL) || (ent == NULL))
	return(NULL);

    if (ent->content != NULL) {
        input = xmlCtxtNewInputFromString(ctxt, NULL,
                (const char *) ent->content, NULL, XML_INPUT_BUF_STATIC);
    } else if (ent->URI != NULL) {
        xmlResourceType rtype;

        if (ent->etype == XML_EXTERNAL_PARAMETER_ENTITY)
            rtype = XML_RESOURCE_PARAMETER_ENTITY;
        else
            rtype = XML_RESOURCE_GENERAL_ENTITY;

        input = xmlLoadResource(ctxt, (char *) ent->URI,
                                (char *) ent->ExternalID, rtype);
    } else {
        return(NULL);
    }

    if (input == NULL)
        return(NULL);

    input->entity = ent;

    return(input);
}

/**
 * xmlNewStringInputStream:
 * @ctxt:  an XML parser context
 * @buffer:  an memory buffer
 *
 * DEPRECATED: Use xmlNewInputFromString.
 *
 * Create a new input stream based on a memory buffer.
 *
 * Returns the new input stream
 */
xmlParserInputPtr
xmlNewStringInputStream(xmlParserCtxtPtr ctxt, const xmlChar *buffer) {
    return(xmlCtxtNewInputFromString(ctxt, NULL, (const char *) buffer,
                                     NULL, 0));
}


/****************************************************************
 *								*
 *		External entities loading			*
 *								*
 ****************************************************************/

#ifdef LIBXML_CATALOG_ENABLED

/**
 * xmlResolveResourceFromCatalog:
 * @URL:  the URL for the entity to load
 * @ID:  the System ID for the entity to load
 * @ctxt:  the context in which the entity is called or NULL
 *
 * Resolves the URL and ID against the appropriate catalog.
 * This function is used by xmlDefaultExternalEntityLoader and
 * xmlNoNetExternalEntityLoader.
 *
 * Returns a new allocated URL, or NULL.
 */
static xmlChar *
xmlResolveResourceFromCatalog(const char *URL, const char *ID,
                              xmlParserCtxtPtr ctxt) {
    xmlChar *resource = NULL;
    xmlCatalogAllow pref;
    int allowLocal = 0;
    int allowGlobal = 0;

    /*
     * If the resource doesn't exists as a file,
     * try to load it from the resource pointed in the catalogs
     */
    pref = xmlCatalogGetDefaults();

    if ((ctxt != NULL) && (ctxt->catalogs != NULL) &&
        ((pref == XML_CATA_ALLOW_ALL) ||
         (pref == XML_CATA_ALLOW_DOCUMENT)))
        allowLocal = 1;

    if (((ctxt == NULL) ||
         ((ctxt->options & XML_PARSE_NO_SYS_CATALOG) == 0)) &&
        ((pref == XML_CATA_ALLOW_ALL) ||
         (pref == XML_CATA_ALLOW_GLOBAL)))
        allowGlobal = 1;

    if ((pref != XML_CATA_ALLOW_NONE) && (!xmlNoNetExists(URL))) {
	/*
	 * Do a local lookup
	 */
        if (allowLocal) {
	    resource = xmlCatalogLocalResolve(ctxt->catalogs,
					      (const xmlChar *)ID,
					      (const xmlChar *)URL);
        }
	/*
	 * Try a global lookup
	 */
	if ((resource == NULL) && (allowGlobal)) {
	    resource = xmlCatalogResolve((const xmlChar *)ID,
					 (const xmlChar *)URL);
	}
	if ((resource == NULL) && (URL != NULL))
	    resource = xmlStrdup((const xmlChar *) URL);

	/*
	 * TODO: do an URI lookup on the reference
	 */
	if ((resource != NULL) && (!xmlNoNetExists((const char *)resource))) {
	    xmlChar *tmp = NULL;

	    if (allowLocal) {
		tmp = xmlCatalogLocalResolveURI(ctxt->catalogs, resource);
	    }
	    if ((tmp == NULL) && (allowGlobal)) {
		tmp = xmlCatalogResolveURI(resource);
	    }

	    if (tmp != NULL) {
		xmlFree(resource);
		resource = tmp;
	    }
	}
    }

    return resource;
}

#endif

#ifdef LIBXML_HTTP_ENABLED
static xmlParserErrors
xmlCheckHTTPInputInternal(xmlParserInputPtr input) {
    const char *encoding;
    const char *redir;
    const char *mime;
    int code;

    if ((input == NULL) || (input->buf == NULL) ||
        (input->buf->readcallback != xmlIOHTTPRead) ||
        (input->buf->context == NULL))
        return(XML_ERR_OK);

    code = xmlNanoHTTPReturnCode(input->buf->context);
    if (code >= 400) {
        /* fatal error */
        return(XML_IO_LOAD_ERROR);
    }

    mime = xmlNanoHTTPMimeType(input->buf->context);
    if ((xmlStrstr(BAD_CAST mime, BAD_CAST "/xml")) ||
        (xmlStrstr(BAD_CAST mime, BAD_CAST "+xml"))) {
        encoding = xmlNanoHTTPEncoding(input->buf->context);
        if (encoding != NULL) {
            xmlCharEncodingHandlerPtr handler;
            xmlParserErrors res;

            res = xmlOpenCharEncodingHandler(encoding, /* output */ 0,
                                             &handler);
            if (res == 0)
                xmlInputSetEncodingHandler(input, handler);
        }
    }

    redir = xmlNanoHTTPRedir(input->buf->context);
    if (redir != NULL) {
        if (input->filename != NULL)
            xmlFree((xmlChar *) input->filename);
        input->filename = xmlMemStrdup(redir);
        if (input->filename == NULL)
            return(XML_ERR_NO_MEMORY);
    }

    return(XML_ERR_OK);
}
#endif /* LIBXML_HTTP_ENABLED */

/**
 * xmlCheckHTTPInput:
 * @ctxt: an XML parser context
 * @ret: an XML parser input
 *
 * DEPRECATED: Internal function, don't use.
 *
 * Check an input in case it was created from an HTTP stream, in that
 * case it will handle encoding and update of the base URL in case of
 * redirection. It also checks for HTTP errors in which case the input
 * is cleanly freed up and an appropriate error is raised in context
 *
 * Returns the input or NULL in case of HTTP error.
 */
xmlParserInputPtr
xmlCheckHTTPInput(xmlParserCtxtPtr ctxt, xmlParserInputPtr ret) {
    /* Avoid unused variable warning if features are disabled. */
    (void) ctxt;

#ifdef LIBXML_HTTP_ENABLED
    {
        int code = xmlCheckHTTPInputInternal(ret);

        if (code != XML_ERR_OK) {
            if (ret->filename != NULL)
                xmlCtxtErrIO(ctxt, XML_IO_LOAD_ERROR, ret->filename);
            else
                xmlCtxtErrIO(ctxt, XML_IO_LOAD_ERROR, "<null>");
            xmlFreeInputStream(ret);
            return(NULL);
        }
    }
#endif

    return(ret);
}

/**
 * xmlNewInputFromUrl:
 * @filename:  the filename to use as entity
 * @flags:  XML_INPUT flags
 * @out:  pointer to new parser input
 *
 * Create a new input stream based on a file or a URL.
 *
 * The flag XML_INPUT_UNZIP allows decompression.
 *
 * The flag XML_INPUT_NETWORK allows network access.
 *
 * The following resource loaders will be called if they were
 * registered (in order of precedence):
 *
 * - the per-thread xmlParserInputBufferCreateFilenameFunc set with
 *   xmlParserInputBufferCreateFilenameDefault (deprecated)
 * - the default loader which will return
 *   - the result from a matching global input callback set with
 *     xmlRegisterInputCallbacks (deprecated)
 *   - a HTTP resource if support is compiled in.
 *   - a file opened from the filesystem, with automatic detection
 *     of compressed files if support is compiled in.
 *
 * Available since 2.14.0.
 *
 * Returns an xmlParserErrors code.
 */
xmlParserErrors
xmlNewInputFromUrl(const char *filename, xmlParserInputFlags flags,
                   xmlParserInputPtr *out) {
    xmlParserInputBufferPtr buf;
    xmlParserInputPtr input;
    xmlParserErrors code = XML_ERR_OK;

    if (out == NULL)
        return(XML_ERR_ARGUMENT);
    *out = NULL;
    if (filename == NULL)
        return(XML_ERR_ARGUMENT);

    if (xmlParserInputBufferCreateFilenameValue != NULL) {
        buf = xmlParserInputBufferCreateFilenameValue(filename,
                XML_CHAR_ENCODING_NONE);
        if (buf == NULL)
            code = XML_IO_ENOENT;
    } else {
        code = xmlParserInputBufferCreateUrl(filename, XML_CHAR_ENCODING_NONE,
                                             flags, &buf);
    }
    if (code != XML_ERR_OK)
	return(code);

    input = xmlNewInputInternal(buf, filename);
    if (input == NULL)
	return(XML_ERR_NO_MEMORY);

#ifdef LIBXML_HTTP_ENABLED
    code = xmlCheckHTTPInputInternal(input);
    if (code != XML_ERR_OK) {
        xmlFreeInputStream(input);
        return(code);
    }
#endif

    *out = input;
    return(XML_ERR_OK);
}

/**
 * xmlNewInputFromFile:
 * @ctxt:  an XML parser context
 * @filename:  the filename to use as entity
 *
 * DEPRECATED: Use xmlNewInputFromUrl.
 *
 * Create a new input stream based on a file or an URL.
 *
 * Returns the new input stream or NULL in case of error
 */
xmlParserInputPtr
xmlNewInputFromFile(xmlParserCtxtPtr ctxt, const char *filename) {
    xmlParserInputPtr input;
    xmlParserInputFlags flags = 0;
    xmlParserErrors code;

    if ((ctxt == NULL) || (filename == NULL))
        return(NULL);

    if (ctxt->options & XML_PARSE_UNZIP)
        flags |= XML_INPUT_UNZIP;
    if ((ctxt->options & XML_PARSE_NONET) == 0)
        flags |= XML_INPUT_NETWORK;

    code = xmlNewInputFromUrl(filename, flags, &input);
    if (code != XML_ERR_OK) {
        xmlCtxtErrIO(ctxt, code, filename);
        return(NULL);
    }

    return(input);
}

/**
 * xmlDefaultExternalEntityLoader:
 * @URL:  the URL for the entity to load
 * @ID:  the System ID for the entity to load
 * @ctxt:  the context in which the entity is called or NULL
 *
 * By default we don't load external entities, yet.
 *
 * Returns a new allocated xmlParserInputPtr, or NULL.
 */
static xmlParserInputPtr
xmlDefaultExternalEntityLoader(const char *url, const char *ID,
                               xmlParserCtxtPtr ctxt)
{
    xmlParserInputPtr input = NULL;
    char *resource = NULL;

    (void) ID;

    if (url == NULL)
        return(NULL);

#ifdef LIBXML_CATALOG_ENABLED
    resource = (char *) xmlResolveResourceFromCatalog(url, ID, ctxt);
    if (resource != NULL)
	url = resource;
#endif

    if ((ctxt != NULL) &&
        (ctxt->options & XML_PARSE_NONET) &&
        (xmlStrncasecmp(BAD_CAST url, BAD_CAST "http://", 7) == 0)) {
        xmlCtxtErrIO(ctxt, XML_IO_NETWORK_ATTEMPT, url);
    } else {
        input = xmlNewInputFromFile(ctxt, url);
    }

    if (resource != NULL)
	xmlFree(resource);
    return(input);
}

/**
 * xmlNoNetExternalEntityLoader:
 * @URL:  the URL for the entity to load
 * @ID:  the System ID for the entity to load
 * @ctxt:  the context in which the entity is called or NULL
 *
 * DEPRECATED: Use XML_PARSE_NONET.
 *
 * A specific entity loader disabling network accesses, though still
 * allowing local catalog accesses for resolution.
 *
 * Returns a new allocated xmlParserInputPtr, or NULL.
 */
xmlParserInputPtr
xmlNoNetExternalEntityLoader(const char *URL, const char *ID,
                             xmlParserCtxtPtr ctxt) {
    int oldOptions = 0;
    xmlParserInputPtr input;

    if (ctxt != NULL) {
        oldOptions = ctxt->options;
        ctxt->options |= XML_PARSE_NONET;
    }

    input = xmlDefaultExternalEntityLoader(URL, ID, ctxt);

    if (ctxt != NULL)
        ctxt->options = oldOptions;

    return(input);
}

/*
 * This global has to die eventually
 */
static xmlExternalEntityLoader
xmlCurrentExternalEntityLoader = xmlDefaultExternalEntityLoader;

/**
 * xmlSetExternalEntityLoader:
 * @f:  the new entity resolver function
 *
 * DEPRECATED: This is a global setting and not thread-safe. Use
 * xmlCtxtSetResourceLoader or similar functions.
 *
 * Changes the default external entity resolver function for the
 * application.
 */
void
xmlSetExternalEntityLoader(xmlExternalEntityLoader f) {
    xmlCurrentExternalEntityLoader = f;
}

/**
 * xmlGetExternalEntityLoader:
 *
 * DEPRECATED: See xmlSetExternalEntityLoader.
 *
 * Get the default external entity resolver function for the application
 *
 * Returns the xmlExternalEntityLoader function pointer
 */
xmlExternalEntityLoader
xmlGetExternalEntityLoader(void) {
    return(xmlCurrentExternalEntityLoader);
}

/**
 * xmlCtxtSetResourceLoader:
 * @ctxt:  parser context
 * @loader:  callback
 * @vctxt:  user data
 *
 * Installs a custom callback to load documents, DTDs or external
 * entities.
 *
 * If @vctxt is NULL, the parser context will be passed.
 *
 * Available since 2.14.0.
 */
void
xmlCtxtSetResourceLoader(xmlParserCtxtPtr ctxt, xmlResourceLoader loader,
                         void *vctxt) {
    if (ctxt == NULL)
        return;

    ctxt->resourceLoader = loader;
    ctxt->resourceCtxt = vctxt;
}

/**
 * xmlLoadResource:
 * @ctxt:  parser context
 * @url:  the URL for the entity to load
 * @publicId:  the Public ID for the entity to load
 * @type:  resource type
 *
 * Returns the xmlParserInputPtr or NULL in case of error.
 */
xmlParserInputPtr
xmlLoadResource(xmlParserCtxtPtr ctxt, const char *url, const char *publicId,
                xmlResourceType type) {
    char *canonicFilename;
    xmlParserInputPtr ret;

    if (url == NULL)
        return(NULL);

    if ((ctxt != NULL) && (ctxt->resourceLoader != NULL)) {
        char *resource = NULL;
        void *userData;
        xmlParserInputFlags flags = 0;
        int code;

#ifdef LIBXML_CATALOG_ENABLED
        resource = (char *) xmlResolveResourceFromCatalog(url, publicId, ctxt);
        if (resource != NULL)
            url = resource;
#endif

        if (ctxt->options & XML_PARSE_UNZIP)
            flags |= XML_INPUT_UNZIP;
        if ((ctxt->options & XML_PARSE_NONET) == 0)
            flags |= XML_INPUT_NETWORK;

        userData = ctxt->resourceCtxt;
        if (userData == NULL)
            userData = ctxt;

        code = ctxt->resourceLoader(userData, url, publicId, type,
                                    flags, &ret);
        if (code != XML_ERR_OK) {
            xmlCtxtErrIO(ctxt, code, url);
            ret = NULL;
        }
        if (resource != NULL)
            xmlFree(resource);
        return(ret);
    }

    canonicFilename = (char *) xmlCanonicPath((const xmlChar *) url);
    if (canonicFilename == NULL) {
        xmlCtxtErrMemory(ctxt);
        return(NULL);
    }

    ret = xmlCurrentExternalEntityLoader(canonicFilename, publicId, ctxt);
    xmlFree(canonicFilename);
    return(ret);
}

/**
 * xmlLoadExternalEntity:
 * @URL:  the URL for the entity to load
 * @ID:  the Public ID for the entity to load
 * @ctxt:  the context in which the entity is called or NULL
 *
 * @URL is a filename or URL. If if contains the substring "://",
 * it is assumed to be a Legacy Extended IRI. Otherwise, it is
 * treated as a filesystem path.
 *
 * @ID is an optional XML public ID, typically from a doctype
 * declaration. It is used for catalog lookups.
 *
 * If catalog lookup is enabled (default is yes) and URL or ID are
 * found in system or local XML catalogs, URL is replaced with the
 * result. Then the following resource loaders will be called if
 * they were registered (in order of precedence):
 *
 * - the resource loader set with xmlCtxtSetResourceLoader
 * - the global external entity loader set with
 *   xmlSetExternalEntityLoader (without catalog resolution,
 *   deprecated)
 * - the per-thread xmlParserInputBufferCreateFilenameFunc set with
 *   xmlParserInputBufferCreateFilenameDefault (deprecated)
 * - the default loader which will return
 *   - the result from a matching global input callback set with
 *     xmlRegisterInputCallbacks (deprecated)
 *   - a HTTP resource if support is compiled in.
 *   - a file opened from the filesystem, with automatic detection
 *     of compressed files if support is compiled in.
 *
 * Returns the xmlParserInputPtr or NULL
 */
xmlParserInputPtr
xmlLoadExternalEntity(const char *URL, const char *ID,
                      xmlParserCtxtPtr ctxt) {
    return(xmlLoadResource(ctxt, URL, ID, XML_RESOURCE_UNKNOWN));
}

/************************************************************************
 *									*
 *		Commodity functions to handle parser contexts		*
 *									*
 ************************************************************************/

/**
 * xmlInitSAXParserCtxt:
 * @ctxt:  XML parser context
 * @sax:  SAX handlert
 * @userData:  user data
 *
 * Initialize a SAX parser context
 *
 * Returns 0 in case of success and -1 in case of error
 */

static int
xmlInitSAXParserCtxt(xmlParserCtxtPtr ctxt, const xmlSAXHandler *sax,
                     void *userData)
{
    xmlParserInputPtr input;
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    size_t initialNodeTabSize = 1;
#else
    size_t initialNodeTabSize = 10;
#endif

    if (ctxt == NULL)
        return(-1);

    if (ctxt->dict == NULL)
	ctxt->dict = xmlDictCreate();
    if (ctxt->dict == NULL)
	return(-1);

    if (ctxt->sax == NULL)
	ctxt->sax = (xmlSAXHandler *) xmlMalloc(sizeof(xmlSAXHandler));
    if (ctxt->sax == NULL)
	return(-1);
    if (sax == NULL) {
	memset(ctxt->sax, 0, sizeof(xmlSAXHandler));
        xmlSAXVersion(ctxt->sax, 2);
        ctxt->userData = ctxt;
    } else {
	if (sax->initialized == XML_SAX2_MAGIC) {
	    memcpy(ctxt->sax, sax, sizeof(xmlSAXHandler));
        } else {
	    memset(ctxt->sax, 0, sizeof(xmlSAXHandler));
	    memcpy(ctxt->sax, sax, sizeof(xmlSAXHandlerV1));
        }
        ctxt->userData = userData ? userData : ctxt;
    }

    ctxt->maxatts = 0;
    ctxt->atts = NULL;
    /* Allocate the Input stack */
    if (ctxt->inputTab == NULL) {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        size_t initialSize = 1;
#else
        size_t initialSize = 5;
#endif

	ctxt->inputTab = xmlMalloc(initialSize * sizeof(xmlParserInputPtr));
	ctxt->inputMax = initialSize;
    }
    if (ctxt->inputTab == NULL)
	return(-1);
    while ((input = xmlCtxtPopInput(ctxt)) != NULL) { /* Non consuming */
        xmlFreeInputStream(input);
    }
    ctxt->inputNr = 0;
    ctxt->input = NULL;

    ctxt->version = NULL;
    ctxt->encoding = NULL;
    ctxt->standalone = -1;
    ctxt->hasExternalSubset = 0;
    ctxt->hasPErefs = 0;
    ctxt->html = 0;
    ctxt->instate = XML_PARSER_START;

    /* Allocate the Node stack */
    if (ctxt->nodeTab == NULL) {
	ctxt->nodeTab = xmlMalloc(initialNodeTabSize * sizeof(xmlNodePtr));
	ctxt->nodeMax = initialNodeTabSize;
    }
    if (ctxt->nodeTab == NULL)
	return(-1);
    ctxt->nodeNr = 0;
    ctxt->node = NULL;

    /* Allocate the Name stack */
    if (ctxt->nameTab == NULL) {
	ctxt->nameTab = xmlMalloc(initialNodeTabSize * sizeof(xmlChar *));
	ctxt->nameMax = initialNodeTabSize;
    }
    if (ctxt->nameTab == NULL)
	return(-1);
    ctxt->nameNr = 0;
    ctxt->name = NULL;

    /* Allocate the space stack */
    if (ctxt->spaceTab == NULL) {
	ctxt->spaceTab = xmlMalloc(initialNodeTabSize * sizeof(int));
	ctxt->spaceMax = initialNodeTabSize;
    }
    if (ctxt->spaceTab == NULL)
	return(-1);
    ctxt->spaceNr = 1;
    ctxt->spaceTab[0] = -1;
    ctxt->space = &ctxt->spaceTab[0];
    ctxt->myDoc = NULL;
    ctxt->wellFormed = 1;
    ctxt->nsWellFormed = 1;
    ctxt->valid = 1;

    ctxt->options = XML_PARSE_NODICT;

    /*
     * Initialize some parser options from deprecated global variables.
     * Note that the "modern" API taking options arguments or
     * xmlCtxtSetOptions will ignore these defaults. They're only
     * relevant if old API functions like xmlParseFile are used.
     */
    ctxt->loadsubset = xmlLoadExtDtdDefaultValue;
    if (ctxt->loadsubset) {
        ctxt->options |= XML_PARSE_DTDLOAD;
    }
    ctxt->validate = xmlDoValidityCheckingDefaultValue;
    if (ctxt->validate) {
        ctxt->options |= XML_PARSE_DTDVALID;
    }
    ctxt->pedantic = xmlPedanticParserDefaultValue;
    if (ctxt->pedantic) {
        ctxt->options |= XML_PARSE_PEDANTIC;
    }
    ctxt->linenumbers = xmlLineNumbersDefaultValue;
    ctxt->keepBlanks = xmlKeepBlanksDefaultValue;
    if (ctxt->keepBlanks == 0) {
	ctxt->sax->ignorableWhitespace = xmlSAX2IgnorableWhitespace;
	ctxt->options |= XML_PARSE_NOBLANKS;
    }
    ctxt->replaceEntities = xmlSubstituteEntitiesDefaultValue;
    if (ctxt->replaceEntities) {
        ctxt->options |= XML_PARSE_NOENT;
    }
    if (xmlGetWarningsDefaultValue == 0)
        ctxt->options |= XML_PARSE_NOWARNING;

    ctxt->vctxt.flags = XML_VCTXT_USE_PCTXT;
    ctxt->vctxt.userData = ctxt;
    ctxt->vctxt.error = xmlParserValidityError;
    ctxt->vctxt.warning = xmlParserValidityWarning;

    ctxt->record_info = 0;
    ctxt->checkIndex = 0;
    ctxt->inSubset = 0;
    ctxt->errNo = XML_ERR_OK;
    ctxt->depth = 0;
    ctxt->catalogs = NULL;
    ctxt->sizeentities = 0;
    ctxt->sizeentcopy = 0;
    ctxt->input_id = 1;
    ctxt->maxAmpl = XML_MAX_AMPLIFICATION_DEFAULT;
    xmlInitNodeInfoSeq(&ctxt->node_seq);

    if (ctxt->nsdb == NULL) {
        ctxt->nsdb = xmlParserNsCreate();
        if (ctxt->nsdb == NULL)
            return(-1);
    }

    return(0);
}

/**
 * xmlInitParserCtxt:
 * @ctxt:  an XML parser context
 *
 * DEPRECATED: Internal function which will be made private in a future
 * version.
 *
 * Initialize a parser context
 *
 * Returns 0 in case of success and -1 in case of error
 */

int
xmlInitParserCtxt(xmlParserCtxtPtr ctxt)
{
    return(xmlInitSAXParserCtxt(ctxt, NULL, NULL));
}

/**
 * xmlFreeParserCtxt:
 * @ctxt:  an XML parser context
 *
 * Free all the memory used by a parser context. However the parsed
 * document in ctxt->myDoc is not freed.
 */

void
xmlFreeParserCtxt(xmlParserCtxtPtr ctxt)
{
    xmlParserInputPtr input;

    if (ctxt == NULL) return;

    while ((input = xmlCtxtPopInput(ctxt)) != NULL) { /* Non consuming */
        xmlFreeInputStream(input);
    }
    if (ctxt->spaceTab != NULL) xmlFree(ctxt->spaceTab);
    if (ctxt->nameTab != NULL) xmlFree((xmlChar * *)ctxt->nameTab);
    if (ctxt->nodeTab != NULL) xmlFree(ctxt->nodeTab);
    if (ctxt->nodeInfoTab != NULL) xmlFree(ctxt->nodeInfoTab);
    if (ctxt->inputTab != NULL) xmlFree(ctxt->inputTab);
    if (ctxt->version != NULL) xmlFree((char *) ctxt->version);
    if (ctxt->encoding != NULL) xmlFree((char *) ctxt->encoding);
    if (ctxt->extSubURI != NULL) xmlFree((char *) ctxt->extSubURI);
    if (ctxt->extSubSystem != NULL) xmlFree((char *) ctxt->extSubSystem);
#ifdef LIBXML_SAX1_ENABLED
    if ((ctxt->sax != NULL) &&
        (ctxt->sax != (xmlSAXHandlerPtr) &xmlDefaultSAXHandler))
#else
    if (ctxt->sax != NULL)
#endif /* LIBXML_SAX1_ENABLED */
        xmlFree(ctxt->sax);
    if (ctxt->directory != NULL) xmlFree(ctxt->directory);
    if (ctxt->vctxt.nodeTab != NULL) xmlFree(ctxt->vctxt.nodeTab);
    if (ctxt->atts != NULL) xmlFree((xmlChar * *)ctxt->atts);
    if (ctxt->dict != NULL) xmlDictFree(ctxt->dict);
    if (ctxt->nsTab != NULL) xmlFree(ctxt->nsTab);
    if (ctxt->nsdb != NULL) xmlParserNsFree(ctxt->nsdb);
    if (ctxt->attrHash != NULL) xmlFree(ctxt->attrHash);
    if (ctxt->pushTab != NULL) xmlFree(ctxt->pushTab);
    if (ctxt->attallocs != NULL) xmlFree(ctxt->attallocs);
    if (ctxt->attsDefault != NULL)
        xmlHashFree(ctxt->attsDefault, xmlHashDefaultDeallocator);
    if (ctxt->attsSpecial != NULL)
        xmlHashFree(ctxt->attsSpecial, NULL);
    if (ctxt->freeElems != NULL) {
        xmlNodePtr cur, next;

	cur = ctxt->freeElems;
	while (cur != NULL) {
	    next = cur->next;
	    xmlFree(cur);
	    cur = next;
	}
    }
    if (ctxt->freeAttrs != NULL) {
        xmlAttrPtr cur, next;

	cur = ctxt->freeAttrs;
	while (cur != NULL) {
	    next = cur->next;
	    xmlFree(cur);
	    cur = next;
	}
    }
    /*
     * cleanup the error strings
     */
    if (ctxt->lastError.message != NULL)
        xmlFree(ctxt->lastError.message);
    if (ctxt->lastError.file != NULL)
        xmlFree(ctxt->lastError.file);
    if (ctxt->lastError.str1 != NULL)
        xmlFree(ctxt->lastError.str1);
    if (ctxt->lastError.str2 != NULL)
        xmlFree(ctxt->lastError.str2);
    if (ctxt->lastError.str3 != NULL)
        xmlFree(ctxt->lastError.str3);

#ifdef LIBXML_CATALOG_ENABLED
    if (ctxt->catalogs != NULL)
	xmlCatalogFreeLocal(ctxt->catalogs);
#endif
    xmlFree(ctxt);
}

/**
 * xmlNewParserCtxt:
 *
 * Allocate and initialize a new parser context.
 *
 * Returns the xmlParserCtxtPtr or NULL
 */

xmlParserCtxtPtr
xmlNewParserCtxt(void)
{
    return(xmlNewSAXParserCtxt(NULL, NULL));
}

/**
 * xmlNewSAXParserCtxt:
 * @sax:  SAX handler
 * @userData:  user data
 *
 * Allocate and initialize a new SAX parser context. If userData is NULL,
 * the parser context will be passed as user data.
 *
 * Available since 2.11.0. If you want support older versions,
 * it's best to invoke xmlNewParserCtxt and set ctxt->sax with
 * struct assignment.
 *
 * Returns the xmlParserCtxtPtr or NULL if memory allocation failed.
 */

xmlParserCtxtPtr
xmlNewSAXParserCtxt(const xmlSAXHandler *sax, void *userData)
{
    xmlParserCtxtPtr ctxt;

    xmlInitParser();

    ctxt = (xmlParserCtxtPtr) xmlMalloc(sizeof(xmlParserCtxt));
    if (ctxt == NULL)
	return(NULL);
    memset(ctxt, 0, sizeof(xmlParserCtxt));
    if (xmlInitSAXParserCtxt(ctxt, sax, userData) < 0) {
        xmlFreeParserCtxt(ctxt);
	return(NULL);
    }
    return(ctxt);
}

/**
 * xmlCtxtGetPrivate:
 * @ctxt:  parser context
 *
 * Available since 2.14.0.
 *
 * Returns the private application data.
 */
void *
xmlCtxtGetPrivate(xmlParserCtxtPtr ctxt) {
    if (ctxt == NULL)
        return(NULL);

    return(ctxt->_private);
}

/**
 * xmlCtxtSetPrivate:
 * @ctxt:  parser context
 * @priv:  private application data
 *
 * Available since 2.14.0.
 *
 * Set the private application data.
 */
void
xmlCtxtSetPrivate(xmlParserCtxtPtr ctxt, void *priv) {
    if (ctxt == NULL)
        return;

    ctxt->_private = priv;
}

/**
 * xmlCtxtGetCatalogs:
 * @ctxt:  parser context
 *
 * Available since 2.14.0.
 *
 * Returns the local catalogs.
 */
void *
xmlCtxtGetCatalogs(xmlParserCtxtPtr ctxt) {
    if (ctxt == NULL)
        return(NULL);

    return(ctxt->catalogs);
}

/**
 * xmlCtxtSetCatalogs:
 * @ctxt:  parser context
 * @catalogs:  catalogs pointer
 *
 * Available since 2.14.0.
 *
 * Set the local catalogs.
 */
void
xmlCtxtSetCatalogs(xmlParserCtxtPtr ctxt, void *catalogs) {
    if (ctxt == NULL)
        return;

    ctxt->catalogs = catalogs;
}

/**
 * xmlCtxtGetDict:
 * @ctxt:  parser context
 *
 * Available since 2.14.0.
 *
 * Returns the dictionary.
 */
xmlDictPtr
xmlCtxtGetDict(xmlParserCtxtPtr ctxt) {
    if (ctxt == NULL)
        return(NULL);

    return(ctxt->dict);
}

/**
 * xmlCtxtSetDict:
 * @ctxt:  parser context
 * @dict:  dictionary
 *
 * Available since 2.14.0.
 *
 * Set the dictionary. This should only be done immediately after
 * creating a parser context.
 */
void
xmlCtxtSetDict(xmlParserCtxtPtr ctxt, xmlDictPtr dict) {
    if (ctxt == NULL)
        return;

    if (ctxt->dict != NULL)
        xmlDictFree(ctxt->dict);

    xmlDictReference(dict);
    ctxt->dict = dict;
}

/**
 * xmlCtxtGetSaxHandler:
 * @ctxt:  parser context
 *
 * Available since 2.14.0.
 *
 * Returns the SAX handler struct. This is not a copy and must not
 * be freed. Handlers can be updated.
 */
xmlSAXHandler *
xmlCtxtGetSaxHandler(xmlParserCtxtPtr ctxt) {
    if (ctxt == NULL)
        return(NULL);

    return(ctxt->sax);
}

/**
 * xmlCtxtSetSaxHandler:
 * @ctxt:  parser context
 * @sax:  SAX handler
 *
 * Available since 2.14.0.
 *
 * Set the SAX handler struct to a copy of @sax.
 *
 * Returns 0 on success or -1 if arguments are invalid or a memory
 * allocation failed.
 */
int
xmlCtxtSetSaxHandler(xmlParserCtxtPtr ctxt, const xmlSAXHandler *sax) {
    xmlSAXHandler *copy;

    if ((ctxt == NULL) || (sax == NULL))
        return(-1);

    copy = xmlMalloc(sizeof(*copy));
    if (copy == NULL)
        return(-1);

    memcpy(copy, sax, sizeof(*copy));
    ctxt->sax = copy;

    return(0);
}

/**
 * xmlCtxtGetDocument:
 * @ctxt:  parser context
 *
 * Available since 2.14.0.
 *
 * Returns the parsed document or NULL if a fatal error occurred when
 * parsing. The document must be freed by the caller. Resets the
 * context's document to NULL.
 */
xmlDocPtr
xmlCtxtGetDocument(xmlParserCtxtPtr ctxt) {
    xmlDocPtr doc;

    if (ctxt == NULL)
        return(NULL);

    if ((ctxt->wellFormed) ||
        (((ctxt->recovery) || (ctxt->html)) &&
         (!xmlCtxtIsCatastrophicError(ctxt)))) {
        doc = ctxt->myDoc;
    } else {
        if (ctxt->errNo == XML_ERR_OK)
            xmlFatalErr(ctxt, XML_ERR_INTERNAL_ERROR, "unknown error");
        doc = NULL;
        xmlFreeDoc(ctxt->myDoc);
    }
    ctxt->myDoc = NULL;

    return(doc);
}

/**
 * xmlCtxtIsHtml:
 * @ctxt:  parser context
 *
 * Available since 2.14.0.
 *
 * Returns 1 if this is a HTML parser context, 0 otherwise.
 */
int
xmlCtxtIsHtml(xmlParserCtxtPtr ctxt) {
    if (ctxt == NULL)
        return(0);

    return(ctxt->html ? 1 : 0);
}

/**
 * xmlCtxtIsStopped:
 * @ctxt:  parser context
 *
 * Available since 2.14.0.
 *
 * Returns 1 if the parser is stopped, 0 otherwise.
 */
int
xmlCtxtIsStopped(xmlParserCtxtPtr ctxt) {
    if (ctxt == NULL)
        return(0);

    return(ctxt->disableSAX != 0);
}

#ifdef LIBXML_VALID_ENABLED
/**
 * xmlCtxtGetValidCtxt:
 * @ctxt:  parser context
 *
 * Available since 2.14.0.
 *
 * Returns the validation context.
 */
xmlValidCtxtPtr
xmlCtxtGetValidCtxt(xmlParserCtxtPtr ctxt) {
    if (ctxt == NULL)
        return(NULL);

    return(&ctxt->vctxt);
}
#endif

/************************************************************************
 *									*
 *		Handling of node information				*
 *									*
 ************************************************************************/

/**
 * xmlClearParserCtxt:
 * @ctxt:  an XML parser context
 *
 * Clear (release owned resources) and reinitialize a parser context
 */

void
xmlClearParserCtxt(xmlParserCtxtPtr ctxt)
{
  if (ctxt==NULL)
    return;
  xmlClearNodeInfoSeq(&ctxt->node_seq);
  xmlCtxtReset(ctxt);
}


/**
 * xmlParserFindNodeInfo:
 * @ctx:  an XML parser context
 * @node:  an XML node within the tree
 *
 * DEPRECATED: Don't use.
 *
 * Find the parser node info struct for a given node
 *
 * Returns an xmlParserNodeInfo block pointer or NULL
 */
const xmlParserNodeInfo *
xmlParserFindNodeInfo(xmlParserCtxtPtr ctx, xmlNodePtr node)
{
    unsigned long pos;

    if ((ctx == NULL) || (node == NULL))
        return (NULL);
    /* Find position where node should be at */
    pos = xmlParserFindNodeInfoIndex(&ctx->node_seq, node);
    if (pos < ctx->node_seq.length
        && ctx->node_seq.buffer[pos].node == node)
        return &ctx->node_seq.buffer[pos];
    else
        return NULL;
}


/**
 * xmlInitNodeInfoSeq:
 * @seq:  a node info sequence pointer
 *
 * DEPRECATED: Don't use.
 *
 * -- Initialize (set to initial state) node info sequence
 */
void
xmlInitNodeInfoSeq(xmlParserNodeInfoSeqPtr seq)
{
    if (seq == NULL)
        return;
    seq->length = 0;
    seq->maximum = 0;
    seq->buffer = NULL;
}

/**
 * xmlClearNodeInfoSeq:
 * @seq:  a node info sequence pointer
 *
 * DEPRECATED: Don't use.
 *
 * -- Clear (release memory and reinitialize) node
 *   info sequence
 */
void
xmlClearNodeInfoSeq(xmlParserNodeInfoSeqPtr seq)
{
    if (seq == NULL)
        return;
    if (seq->buffer != NULL)
        xmlFree(seq->buffer);
    xmlInitNodeInfoSeq(seq);
}

/**
 * xmlParserFindNodeInfoIndex:
 * @seq:  a node info sequence pointer
 * @node:  an XML node pointer
 *
 * DEPRECATED: Don't use.
 *
 * xmlParserFindNodeInfoIndex : Find the index that the info record for
 *   the given node is or should be at in a sorted sequence
 *
 * Returns a long indicating the position of the record
 */
unsigned long
xmlParserFindNodeInfoIndex(xmlParserNodeInfoSeqPtr seq,
                           xmlNodePtr node)
{
    unsigned long upper, lower, middle;
    int found = 0;

    if ((seq == NULL) || (node == NULL))
        return ((unsigned long) -1);

    /* Do a binary search for the key */
    lower = 1;
    upper = seq->length;
    middle = 0;
    while (lower <= upper && !found) {
        middle = lower + (upper - lower) / 2;
        if (node == seq->buffer[middle - 1].node)
            found = 1;
        else if (node < seq->buffer[middle - 1].node)
            upper = middle - 1;
        else
            lower = middle + 1;
    }

    /* Return position */
    if (middle == 0 || seq->buffer[middle - 1].node < node)
        return middle;
    else
        return middle - 1;
}


/**
 * xmlParserAddNodeInfo:
 * @ctxt:  an XML parser context
 * @info:  a node info sequence pointer
 *
 * DEPRECATED: Don't use.
 *
 * Insert node info record into the sorted sequence
 */
void
xmlParserAddNodeInfo(xmlParserCtxtPtr ctxt,
                     xmlParserNodeInfoPtr info)
{
    unsigned long pos;

    if ((ctxt == NULL) || (info == NULL)) return;

    /* Find pos and check to see if node is already in the sequence */
    pos = xmlParserFindNodeInfoIndex(&ctxt->node_seq, (xmlNodePtr)
                                     info->node);

    if ((pos < ctxt->node_seq.length) &&
        (ctxt->node_seq.buffer != NULL) &&
        (ctxt->node_seq.buffer[pos].node == info->node)) {
        ctxt->node_seq.buffer[pos] = *info;
    }

    /* Otherwise, we need to add new node to buffer */
    else {
        if (ctxt->node_seq.length + 1 > ctxt->node_seq.maximum) {
            xmlParserNodeInfo *tmp;
            int newSize;

            newSize = xmlGrowCapacity(ctxt->node_seq.maximum, sizeof(tmp[0]),
                                      4, XML_MAX_ITEMS);
            if (newSize < 0) {
		xmlCtxtErrMemory(ctxt);
                return;
            }
            tmp = xmlRealloc(ctxt->node_seq.buffer, newSize * sizeof(tmp[0]));
            if (tmp == NULL) {
		xmlCtxtErrMemory(ctxt);
                return;
            }
            ctxt->node_seq.buffer = tmp;
            ctxt->node_seq.maximum = newSize;
        }

        /* If position is not at end, move elements out of the way */
        if (pos != ctxt->node_seq.length) {
            unsigned long i;

            for (i = ctxt->node_seq.length; i > pos; i--)
                ctxt->node_seq.buffer[i] = ctxt->node_seq.buffer[i - 1];
        }

        /* Copy element and increase length */
        ctxt->node_seq.buffer[pos] = *info;
        ctxt->node_seq.length++;
    }
}

/************************************************************************
 *									*
 *		Defaults settings					*
 *									*
 ************************************************************************/
/**
 * xmlPedanticParserDefault:
 * @val:  int 0 or 1
 *
 * DEPRECATED: Use the modern options API with XML_PARSE_PEDANTIC.
 *
 * Set and return the previous value for enabling pedantic warnings.
 *
 * Returns the last value for 0 for no substitution, 1 for substitution.
 */

int
xmlPedanticParserDefault(int val) {
    int old = xmlPedanticParserDefaultValue;

    xmlPedanticParserDefaultValue = val;
    return(old);
}

/**
 * xmlLineNumbersDefault:
 * @val:  int 0 or 1
 *
 * DEPRECATED: The modern options API always enables line numbers.
 *
 * Set and return the previous value for enabling line numbers in elements
 * contents. This may break on old application and is turned off by default.
 *
 * Returns the last value for 0 for no substitution, 1 for substitution.
 */

int
xmlLineNumbersDefault(int val) {
    int old = xmlLineNumbersDefaultValue;

    xmlLineNumbersDefaultValue = val;
    return(old);
}

/**
 * xmlSubstituteEntitiesDefault:
 * @val:  int 0 or 1
 *
 * DEPRECATED: Use the modern options API with XML_PARSE_NOENT.
 *
 * Set and return the previous value for default entity support.
 * Initially the parser always keep entity references instead of substituting
 * entity values in the output. This function has to be used to change the
 * default parser behavior
 * SAX::substituteEntities() has to be used for changing that on a file by
 * file basis.
 *
 * Returns the last value for 0 for no substitution, 1 for substitution.
 */

int
xmlSubstituteEntitiesDefault(int val) {
    int old = xmlSubstituteEntitiesDefaultValue;

    xmlSubstituteEntitiesDefaultValue = val;
    return(old);
}

/**
 * xmlKeepBlanksDefault:
 * @val:  int 0 or 1
 *
 * DEPRECATED: Use the modern options API with XML_PARSE_NOBLANKS.
 *
 * Set and return the previous value for default blanks text nodes support.
 * The 1.x version of the parser used an heuristic to try to detect
 * ignorable white spaces. As a result the SAX callback was generating
 * xmlSAX2IgnorableWhitespace() callbacks instead of characters() one, and when
 * using the DOM output text nodes containing those blanks were not generated.
 * The 2.x and later version will switch to the XML standard way and
 * ignorableWhitespace() are only generated when running the parser in
 * validating mode and when the current element doesn't allow CDATA or
 * mixed content.
 * This function is provided as a way to force the standard behavior
 * on 1.X libs and to switch back to the old mode for compatibility when
 * running 1.X client code on 2.X . Upgrade of 1.X code should be done
 * by using xmlIsBlankNode() commodity function to detect the "empty"
 * nodes generated.
 * This value also affect autogeneration of indentation when saving code
 * if blanks sections are kept, indentation is not generated.
 *
 * Returns the last value for 0 for no substitution, 1 for substitution.
 */

int
xmlKeepBlanksDefault(int val) {
    int old = xmlKeepBlanksDefaultValue;

    xmlKeepBlanksDefaultValue = val;
#ifdef LIBXML_OUTPUT_ENABLED
    if (!val)
        xmlIndentTreeOutput = 1;
#endif
    return(old);
}

