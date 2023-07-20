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
#include <libxml/valid.h>
#include <libxml/entities.h>
#include <libxml/xmlerror.h>
#include <libxml/encoding.h>
#include <libxml/valid.h>
#include <libxml/xmlIO.h>
#include <libxml/uri.h>
#include <libxml/dict.h>
#include <libxml/SAX.h>
#ifdef LIBXML_CATALOG_ENABLED
#include <libxml/catalog.h>
#endif
#include <libxml/globals.h>
#include <libxml/chvalid.h>

#define CUR(ctxt) ctxt->input->cur
#define END(ctxt) ctxt->input->end
#define VALID_CTXT(ctxt) (CUR(ctxt) <= END(ctxt))

#include "private/buf.h"
#include "private/enc.h"
#include "private/error.h"
#include "private/io.h"
#include "private/parser.h"

/*
 * Various global defaults for parsing
 */

/**
 * xmlCheckVersion:
 * @version: the include version number
 *
 * check the compiled lib version against the include one.
 * This can warn or immediately kill the application
 */
void
xmlCheckVersion(int version) {
    int myversion = LIBXML_VERSION;

    xmlInitParser();

    if ((myversion / 10000) != (version / 10000)) {
	xmlGenericError(xmlGenericErrorContext,
		"Fatal: program compiled against libxml %d using libxml %d\n",
		(version / 10000), (myversion / 10000));
	fprintf(stderr,
		"Fatal: program compiled against libxml %d using libxml %d\n",
		(version / 10000), (myversion / 10000));
    }
    if ((myversion / 100) < (version / 100)) {
	xmlGenericError(xmlGenericErrorContext,
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
 * xmlErrMemory:
 * @ctxt:  an XML parser context
 * @extra:  extra information
 *
 * Handle a redefinition of attribute error
 */
void
xmlErrMemory(xmlParserCtxtPtr ctxt, const char *extra)
{
    if ((ctxt != NULL) && (ctxt->disableSAX != 0) &&
        (ctxt->instate == XML_PARSER_EOF))
	return;
    if (ctxt != NULL) {
        ctxt->errNo = XML_ERR_NO_MEMORY;
        ctxt->instate = XML_PARSER_EOF;
        ctxt->disableSAX = 1;
    }
    if (extra)
        __xmlRaiseError(NULL, NULL, NULL, ctxt, NULL, XML_FROM_PARSER,
                        XML_ERR_NO_MEMORY, XML_ERR_FATAL, NULL, 0, extra,
                        NULL, NULL, 0, 0,
                        "Memory allocation failed : %s\n", extra);
    else
        __xmlRaiseError(NULL, NULL, NULL, ctxt, NULL, XML_FROM_PARSER,
                        XML_ERR_NO_MEMORY, XML_ERR_FATAL, NULL, 0, NULL,
                        NULL, NULL, 0, 0, "Memory allocation failed\n");
}

/**
 * __xmlErrEncoding:
 * @ctxt:  an XML parser context
 * @xmlerr:  the error number
 * @msg:  the error message
 * @str1:  an string info
 * @str2:  an string info
 *
 * Handle an encoding error
 */
void
__xmlErrEncoding(xmlParserCtxtPtr ctxt, xmlParserErrors xmlerr,
                 const char *msg, const xmlChar * str1, const xmlChar * str2)
{
    if ((ctxt != NULL) && (ctxt->disableSAX != 0) &&
        (ctxt->instate == XML_PARSER_EOF))
	return;
    if (ctxt != NULL)
        ctxt->errNo = xmlerr;
    __xmlRaiseError(NULL, NULL, NULL,
                    ctxt, NULL, XML_FROM_PARSER, xmlerr, XML_ERR_FATAL,
                    NULL, 0, (const char *) str1, (const char *) str2,
                    NULL, 0, 0, msg, str1, str2);
    if (ctxt != NULL) {
        ctxt->wellFormed = 0;
        if (ctxt->recovery == 0)
            ctxt->disableSAX = 1;
    }
}

/**
 * xmlErrInternal:
 * @ctxt:  an XML parser context
 * @msg:  the error message
 * @str:  error information
 *
 * Handle an internal error
 */
static void LIBXML_ATTR_FORMAT(2,0)
xmlErrInternal(xmlParserCtxtPtr ctxt, const char *msg, const xmlChar * str)
{
    if ((ctxt != NULL) && (ctxt->disableSAX != 0) &&
        (ctxt->instate == XML_PARSER_EOF))
	return;
    if (ctxt != NULL)
        ctxt->errNo = XML_ERR_INTERNAL_ERROR;
    __xmlRaiseError(NULL, NULL, NULL,
                    ctxt, NULL, XML_FROM_PARSER, XML_ERR_INTERNAL_ERROR,
                    XML_ERR_FATAL, NULL, 0, (const char *) str, NULL, NULL,
                    0, 0, msg, str);
    if (ctxt != NULL) {
        ctxt->wellFormed = 0;
        if (ctxt->recovery == 0)
            ctxt->disableSAX = 1;
    }
}

/**
 * xmlErrEncodingInt:
 * @ctxt:  an XML parser context
 * @error:  the error number
 * @msg:  the error message
 * @val:  an integer value
 *
 * n encoding error
 */
static void LIBXML_ATTR_FORMAT(3,0)
xmlErrEncodingInt(xmlParserCtxtPtr ctxt, xmlParserErrors error,
                  const char *msg, int val)
{
    if ((ctxt != NULL) && (ctxt->disableSAX != 0) &&
        (ctxt->instate == XML_PARSER_EOF))
	return;
    if (ctxt != NULL)
        ctxt->errNo = error;
    __xmlRaiseError(NULL, NULL, NULL,
                    ctxt, NULL, XML_FROM_PARSER, error, XML_ERR_FATAL,
                    NULL, 0, NULL, NULL, NULL, val, 0, msg, val);
    if (ctxt != NULL) {
        ctxt->wellFormed = 0;
        if (ctxt->recovery == 0)
            ctxt->disableSAX = 1;
    }
}

/**
 * xmlIsLetter:
 * @c:  an unicode character (int)
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

/* #define DEBUG_INPUT */
/* #define DEBUG_STACK */
/* #define DEBUG_PUSH */


/* we need to keep enough input to show errors in context */
#define LINE_LEN        80

#ifdef DEBUG_INPUT
#define CHECK_BUFFER(in) check_buffer(in)

static
void check_buffer(xmlParserInputPtr in) {
    if (in->base != xmlBufContent(in->buf->buffer)) {
        xmlGenericError(xmlGenericErrorContext,
		"xmlParserInput: base mismatch problem\n");
    }
    if (in->cur < in->base) {
        xmlGenericError(xmlGenericErrorContext,
		"xmlParserInput: cur < base problem\n");
    }
    if (in->cur > in->base + xmlBufUse(in->buf->buffer)) {
        xmlGenericError(xmlGenericErrorContext,
		"xmlParserInput: cur > base + use problem\n");
    }
    xmlGenericError(xmlGenericErrorContext,"buffer %p : content %x, cur %d, use %d\n",
            (void *) in, (int) xmlBufContent(in->buf->buffer),
            in->cur - in->base, xmlBufUse(in->buf->buffer));
}

#else
#define CHECK_BUFFER(in)
#endif


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
    ctxt->instate = XML_PARSER_EOF;
    ctxt->disableSAX = 1;
    while (ctxt->inputNr > 1)
        xmlFreeInputStream(inputPop(ctxt));
    if (ctxt->input != NULL) {
        /*
	 * in case there was a specific allocation deallocate before
	 * overriding base
	 */
        if (ctxt->input->free != NULL) {
	    ctxt->input->free((xmlChar *) ctxt->input->base);
	    ctxt->input->free = NULL;
	}
        if (ctxt->input->buf != NULL) {
            xmlFreeParserInputBuffer(ctxt->input->buf);
            ctxt->input->buf = NULL;
        }
	ctxt->input->cur = BAD_CAST"";
        ctxt->input->length = 0;
	ctxt->input->base = ctxt->input->cur;
        ctxt->input->end = ctxt->input->cur;
    }
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
 */
int
xmlParserGrow(xmlParserCtxtPtr ctxt) {
    xmlParserInputPtr in = ctxt->input;
    xmlParserInputBufferPtr buf = in->buf;
    ptrdiff_t curEnd = in->end - in->cur;
    ptrdiff_t curBase = in->cur - in->base;
    int ret;

    if (buf == NULL)
        return(0);
    /* Don't grow push parser buffer. */
    if (ctxt->progressive)
        return(0);
    /* Don't grow memory buffers. */
    if ((buf->encoder == NULL) && (buf->readcallback == NULL))
        return(0);

    if (((curEnd > XML_MAX_LOOKUP_LIMIT) ||
         (curBase > XML_MAX_LOOKUP_LIMIT)) &&
        ((ctxt->options & XML_PARSE_HUGE) == 0)) {
        xmlErrInternal(ctxt, "Huge input lookup", NULL);
        xmlHaltParser(ctxt);
	return(-1);
    }

    if (curEnd >= INPUT_CHUNK)
        return(0);

    ret = xmlParserInputBufferGrow(buf, INPUT_CHUNK);
    xmlBufSetInputBaseCur(buf->buffer, in, 0, curBase);

    /* TODO: Get error code from xmlParserInputBufferGrow */
    if (ret < 0) {
        xmlErrInternal(ctxt, "Growing input buffer", NULL);
        xmlHaltParser(ctxt);
    }

    return(ret);
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
#ifdef DEBUG_INPUT
    xmlGenericError(xmlGenericErrorContext, "Grow\n");
#endif
    if (in->buf == NULL) return(-1);
    if (in->base == NULL) return(-1);
    if (in->cur == NULL) return(-1);
    if (in->buf->buffer == NULL) return(-1);

    /* Don't grow memory buffers. */
    if ((in->buf->encoder == NULL) && (in->buf->readcallback == NULL))
        return(0);

    CHECK_BUFFER(in);

    indx = in->cur - in->base;
    if (xmlBufUse(in->buf->buffer) > (unsigned int) indx + INPUT_CHUNK) {

	CHECK_BUFFER(in);

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

    CHECK_BUFFER(in);

    return(ret);
}

/**
 * xmlParserShrink:
 * @ctxt:  an XML parser context
 */
void
xmlParserShrink(xmlParserCtxtPtr ctxt) {
    xmlParserInputPtr in = ctxt->input;
    xmlParserInputBufferPtr buf = in->buf;
    size_t used;

    /* Don't shrink memory buffers. */
    if ((buf == NULL) ||
        ((buf->encoder == NULL) && (buf->readcallback == NULL)))
        return;

    used = in->cur - in->base;
    /*
     * Do not shrink on large buffers whose only a tiny fraction
     * was consumed
     */
    if (used > INPUT_CHUNK) {
	size_t res = xmlBufShrink(buf->buffer, used - LINE_LEN);

	if (res > 0) {
            used -= res;
            if ((res > ULONG_MAX) ||
                (in->consumed > ULONG_MAX - (unsigned long)res))
                in->consumed = ULONG_MAX;
            else
                in->consumed += res;
	}
    }

    xmlBufSetInputBaseCur(buf->buffer, in, 0, used);
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

#ifdef DEBUG_INPUT
    xmlGenericError(xmlGenericErrorContext, "Shrink\n");
#endif
    if (in == NULL) return;
    if (in->buf == NULL) return;
    if (in->base == NULL) return;
    if (in->cur == NULL) return;
    if (in->buf->buffer == NULL) return;

    CHECK_BUFFER(in);

    used = in->cur - in->base;
    /*
     * Do not shrink on large buffers whose only a tiny fraction
     * was consumed
     */
    if (used > INPUT_CHUNK) {
	ret = xmlBufShrink(in->buf->buffer, used - LINE_LEN);
	if (ret > 0) {
            used -= ret;
            if ((ret > ULONG_MAX) ||
                (in->consumed > ULONG_MAX - (unsigned long)ret))
                in->consumed = ULONG_MAX;
            else
                in->consumed += ret;
	}
    }

    if (xmlBufUse(in->buf->buffer) <= INPUT_CHUNK) {
        xmlParserInputBufferRead(in->buf, 2 * INPUT_CHUNK);
    }

    in->base = xmlBufContent(in->buf->buffer);
    if (in->base == NULL) {
        /* TODO: raise error */
        in->base = BAD_CAST "";
        in->cur = in->base;
        in->end = in->base;
        return;
    }
    in->cur = in->base + used;
    in->end = xmlBufEnd(in->buf->buffer);

    CHECK_BUFFER(in);
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
 * Skip to the next char input char.
 */

void
xmlNextChar(xmlParserCtxtPtr ctxt)
{
    if ((ctxt == NULL) || (ctxt->instate == XML_PARSER_EOF) ||
        (ctxt->input == NULL))
        return;

    if (!(VALID_CTXT(ctxt))) {
        xmlErrInternal(ctxt, "Parser input data memory error\n", NULL);
	ctxt->errNo = XML_ERR_INTERNAL_ERROR;
        xmlStopParser(ctxt);
	return;
    }

    if (ctxt->input->end - ctxt->input->cur < INPUT_CHUNK) {
        if (xmlParserGrow(ctxt) < 0)
            return;
        if (ctxt->input->cur >= ctxt->input->end)
            return;
    }

    if (ctxt->charset == XML_CHAR_ENCODING_UTF8) {
        const unsigned char *cur;
        unsigned char c;

        /*
         *   2.11 End-of-Line Handling
         *   the literal two-character sequence "#xD#xA" or a standalone
         *   literal #xD, an XML processor must pass to the application
         *   the single character #xA.
         */
        if (*(ctxt->input->cur) == '\n') {
            ctxt->input->line++; ctxt->input->col = 1;
        } else
            ctxt->input->col++;

        /*
         * We are supposed to handle UTF8, check it's valid
         * From rfc2044: encoding of the Unicode values on UTF-8:
         *
         * UCS-4 range (hex.)           UTF-8 octet sequence (binary)
         * 0000 0000-0000 007F   0xxxxxxx
         * 0000 0080-0000 07FF   110xxxxx 10xxxxxx
         * 0000 0800-0000 FFFF   1110xxxx 10xxxxxx 10xxxxxx
         *
         * Check for the 0x110000 limit too
         */
        cur = ctxt->input->cur;

        c = *cur;
        if (c & 0x80) {
            size_t avail;

            if (c == 0xC0)
	        goto encoding_error;

            avail = ctxt->input->end - ctxt->input->cur;

            if ((avail < 2) || (cur[1] & 0xc0) != 0x80)
                goto encoding_error;
            if ((c & 0xe0) == 0xe0) {
                unsigned int val;

                if ((avail < 3) || (cur[2] & 0xc0) != 0x80)
                    goto encoding_error;
                if ((c & 0xf0) == 0xf0) {
                    if (((c & 0xf8) != 0xf0) ||
                        (avail < 4) || ((cur[3] & 0xc0) != 0x80))
                        goto encoding_error;
                    /* 4-byte code */
                    ctxt->input->cur += 4;
                    val = (cur[0] & 0x7) << 18;
                    val |= (cur[1] & 0x3f) << 12;
                    val |= (cur[2] & 0x3f) << 6;
                    val |= cur[3] & 0x3f;
                } else {
                    /* 3-byte code */
                    ctxt->input->cur += 3;
                    val = (cur[0] & 0xf) << 12;
                    val |= (cur[1] & 0x3f) << 6;
                    val |= cur[2] & 0x3f;
                }
                if (((val > 0xd7ff) && (val < 0xe000)) ||
                    ((val > 0xfffd) && (val < 0x10000)) ||
                    (val >= 0x110000)) {
		xmlErrEncodingInt(ctxt, XML_ERR_INVALID_CHAR,
				  "Char 0x%X out of allowed range\n",
				  val);
                }
            } else
                /* 2-byte code */
                ctxt->input->cur += 2;
        } else
            /* 1-byte code */
            ctxt->input->cur++;
    } else {
        /*
         * Assume it's a fixed length encoding (1) with
         * a compatible encoding for the ASCII set, since
         * XML constructs only use < 128 chars
         */

        if (*(ctxt->input->cur) == '\n') {
            ctxt->input->line++; ctxt->input->col = 1;
        } else
            ctxt->input->col++;
        ctxt->input->cur++;
    }
    return;
encoding_error:
    /*
     * If we detect an UTF8 error that probably mean that the
     * input encoding didn't get properly advertised in the
     * declaration header. Report the error and switch the encoding
     * to ISO-Latin-1 (if you don't like this policy, just declare the
     * encoding !)
     */
    if ((ctxt == NULL) || (ctxt->input == NULL) ||
        (ctxt->input->end - ctxt->input->cur < 4)) {
	__xmlErrEncoding(ctxt, XML_ERR_INVALID_CHAR,
		     "Input is not proper UTF-8, indicate encoding !\n",
		     NULL, NULL);
    } else {
        char buffer[150];

	snprintf(buffer, 149, "Bytes: 0x%02X 0x%02X 0x%02X 0x%02X\n",
			ctxt->input->cur[0], ctxt->input->cur[1],
			ctxt->input->cur[2], ctxt->input->cur[3]);
	__xmlErrEncoding(ctxt, XML_ERR_INVALID_CHAR,
		     "Input is not proper UTF-8, indicate encoding !\n%s",
		     BAD_CAST buffer, NULL);
    }
    ctxt->charset = XML_CHAR_ENCODING_8859_1;
    ctxt->input->cur++;
    return;
}

/**
 * xmlCurrentChar:
 * @ctxt:  the XML parser context
 * @len:  pointer to the length of the char read
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
    if ((ctxt == NULL) || (len == NULL) || (ctxt->input == NULL)) return(0);
    if (ctxt->instate == XML_PARSER_EOF)
	return(0);

    if ((ctxt->input->end - ctxt->input->cur < INPUT_CHUNK) &&
        (xmlParserGrow(ctxt) < 0))
        return(0);

    if ((*ctxt->input->cur >= 0x20) && (*ctxt->input->cur <= 0x7F)) {
	    *len = 1;
	    return(*ctxt->input->cur);
    }
    if (ctxt->charset == XML_CHAR_ENCODING_UTF8) {
	/*
	 * We are supposed to handle UTF8, check it's valid
	 * From rfc2044: encoding of the Unicode values on UTF-8:
	 *
	 * UCS-4 range (hex.)           UTF-8 octet sequence (binary)
	 * 0000 0000-0000 007F   0xxxxxxx
	 * 0000 0080-0000 07FF   110xxxxx 10xxxxxx
	 * 0000 0800-0000 FFFF   1110xxxx 10xxxxxx 10xxxxxx
	 *
	 * Check for the 0x110000 limit too
	 */
	const unsigned char *cur = ctxt->input->cur;
	unsigned char c;
	unsigned int val;

	c = *cur;
	if (c & 0x80) {
            size_t avail;

	    if (((c & 0x40) == 0) || (c == 0xC0))
		goto encoding_error;

            avail = ctxt->input->end - ctxt->input->cur;

	    if ((avail < 2) || (cur[1] & 0xc0) != 0x80)
		goto encoding_error;
	    if ((c & 0xe0) == 0xe0) {
		if ((avail < 3) || (cur[2] & 0xc0) != 0x80)
		    goto encoding_error;
		if ((c & 0xf0) == 0xf0) {
		    if (((c & 0xf8) != 0xf0) ||
			(avail < 4) || ((cur[3] & 0xc0) != 0x80))
			goto encoding_error;
		    /* 4-byte code */
		    *len = 4;
		    val = (cur[0] & 0x7) << 18;
		    val |= (cur[1] & 0x3f) << 12;
		    val |= (cur[2] & 0x3f) << 6;
		    val |= cur[3] & 0x3f;
		    if (val < 0x10000)
			goto encoding_error;
		} else {
		  /* 3-byte code */
		    *len = 3;
		    val = (cur[0] & 0xf) << 12;
		    val |= (cur[1] & 0x3f) << 6;
		    val |= cur[2] & 0x3f;
		    if (val < 0x800)
			goto encoding_error;
		}
	    } else {
	      /* 2-byte code */
		*len = 2;
		val = (cur[0] & 0x1f) << 6;
		val |= cur[1] & 0x3f;
		if (val < 0x80)
		    goto encoding_error;
	    }
	    if (!IS_CHAR(val)) {
	        xmlErrEncodingInt(ctxt, XML_ERR_INVALID_CHAR,
				  "Char 0x%X out of allowed range\n", val);
	    }
	    return(val);
	} else {
	    /* 1-byte code */
	    *len = 1;
	    if ((*ctxt->input->cur == 0) &&
	        (ctxt->input->end > ctxt->input->cur)) {
	        xmlErrEncodingInt(ctxt, XML_ERR_INVALID_CHAR,
				  "Char 0x0 out of allowed range\n", 0);
	    }
	    if (*ctxt->input->cur == 0xD) {
		if (ctxt->input->cur[1] == 0xA) {
		    ctxt->input->cur++;
		}
		return(0xA);
	    }
	    return(*ctxt->input->cur);
	}
    }
    /*
     * Assume it's a fixed length encoding (1) with
     * a compatible encoding for the ASCII set, since
     * XML constructs only use < 128 chars
     */
    *len = 1;
    if (*ctxt->input->cur == 0xD) {
	if (ctxt->input->cur[1] == 0xA) {
	    ctxt->input->cur++;
	}
	return(0xA);
    }
    return(*ctxt->input->cur);
encoding_error:
    /*
     * An encoding problem may arise from a truncated input buffer
     * splitting a character in the middle. In that case do not raise
     * an error but return 0 to indicate an end of stream problem
     */
    if (ctxt->input->end - ctxt->input->cur < 4) {
	*len = 0;
	return(0);
    }

    /*
     * If we detect an UTF8 error that probably mean that the
     * input encoding didn't get properly advertised in the
     * declaration header. Report the error and switch the encoding
     * to ISO-Latin-1 (if you don't like this policy, just declare the
     * encoding !)
     */
    {
        char buffer[150];

	snprintf(&buffer[0], 149, "Bytes: 0x%02X 0x%02X 0x%02X 0x%02X\n",
			ctxt->input->cur[0], ctxt->input->cur[1],
			ctxt->input->cur[2], ctxt->input->cur[3]);
	__xmlErrEncoding(ctxt, XML_ERR_INVALID_CHAR,
		     "Input is not proper UTF-8, indicate encoding !\n%s",
		     BAD_CAST buffer, NULL);
    }
    ctxt->charset = XML_CHAR_ENCODING_8859_1;
    *len = 1;
    return(*ctxt->input->cur);
}

/**
 * xmlStringCurrentChar:
 * @ctxt:  the XML parser context
 * @cur:  pointer to the beginning of the char
 * @len:  pointer to the length of the char read
 *
 * The current char value, if using UTF-8 this may actually span multiple
 * bytes in the input buffer.
 *
 * Returns the current char value and its length
 */

int
xmlStringCurrentChar(xmlParserCtxtPtr ctxt, const xmlChar * cur, int *len)
{
    if ((len == NULL) || (cur == NULL)) return(0);
    if ((ctxt == NULL) || (ctxt->charset == XML_CHAR_ENCODING_UTF8)) {
        /*
         * We are supposed to handle UTF8, check it's valid
         * From rfc2044: encoding of the Unicode values on UTF-8:
         *
         * UCS-4 range (hex.)           UTF-8 octet sequence (binary)
         * 0000 0000-0000 007F   0xxxxxxx
         * 0000 0080-0000 07FF   110xxxxx 10xxxxxx
         * 0000 0800-0000 FFFF   1110xxxx 10xxxxxx 10xxxxxx
         *
         * Check for the 0x110000 limit too
         */
        unsigned char c;
        unsigned int val;

        c = *cur;
        if (c & 0x80) {
            if ((cur[1] & 0xc0) != 0x80)
                goto encoding_error;
            if ((c & 0xe0) == 0xe0) {

                if ((cur[2] & 0xc0) != 0x80)
                    goto encoding_error;
                if ((c & 0xf0) == 0xf0) {
                    if (((c & 0xf8) != 0xf0) || ((cur[3] & 0xc0) != 0x80))
                        goto encoding_error;
                    /* 4-byte code */
                    *len = 4;
                    val = (cur[0] & 0x7) << 18;
                    val |= (cur[1] & 0x3f) << 12;
                    val |= (cur[2] & 0x3f) << 6;
                    val |= cur[3] & 0x3f;
                } else {
                    /* 3-byte code */
                    *len = 3;
                    val = (cur[0] & 0xf) << 12;
                    val |= (cur[1] & 0x3f) << 6;
                    val |= cur[2] & 0x3f;
                }
            } else {
                /* 2-byte code */
                *len = 2;
                val = (cur[0] & 0x1f) << 6;
                val |= cur[1] & 0x3f;
            }
            if (!IS_CHAR(val)) {
	        xmlErrEncodingInt(ctxt, XML_ERR_INVALID_CHAR,
				  "Char 0x%X out of allowed range\n", val);
            }
            return (val);
        } else {
            /* 1-byte code */
            *len = 1;
            return (*cur);
        }
    }
    /*
     * Assume it's a fixed length encoding (1) with
     * a compatible encoding for the ASCII set, since
     * XML constructs only use < 128 chars
     */
    *len = 1;
    return (*cur);
encoding_error:

    /*
     * An encoding problem may arise from a truncated input buffer
     * splitting a character in the middle. In that case do not raise
     * an error but return 0 to indicate an end of stream problem
     */
    if ((ctxt == NULL) || (ctxt->input == NULL) ||
        (ctxt->input->end - ctxt->input->cur < 4)) {
	*len = 0;
	return(0);
    }
    /*
     * If we detect an UTF8 error that probably mean that the
     * input encoding didn't get properly advertised in the
     * declaration header. Report the error and switch the encoding
     * to ISO-Latin-1 (if you don't like this policy, just declare the
     * encoding !)
     */
    {
        char buffer[150];

	snprintf(buffer, 149, "Bytes: 0x%02X 0x%02X 0x%02X 0x%02X\n",
			ctxt->input->cur[0], ctxt->input->cur[1],
			ctxt->input->cur[2], ctxt->input->cur[3]);
	__xmlErrEncoding(ctxt, XML_ERR_INVALID_CHAR,
		     "Input is not proper UTF-8, indicate encoding !\n%s",
		     BAD_CAST buffer, NULL);
    }
    *len = 1;
    return (*cur);
}

/**
 * xmlCopyCharMultiByte:
 * @out:  pointer to an array of xmlChar
 * @val:  the char value
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
	    xmlErrEncodingInt(NULL, XML_ERR_INVALID_CHAR,
		    "Internal error, xmlCopyCharMultiByte 0x%X out of bound\n",
			      val);
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

static xmlCharEncodingHandlerPtr
xmlDetectEBCDIC(xmlParserInputPtr input) {
    xmlChar out[200];
    xmlCharEncodingHandlerPtr handler;
    int inlen, outlen, res, i;

    /*
     * To detect the EBCDIC code page, we convert the first 200 bytes
     * to EBCDIC-US and try to find the encoding declaration.
     */
    handler = xmlGetCharEncodingHandler(XML_CHAR_ENCODING_EBCDIC);
    if (handler == NULL)
        return(NULL);
    outlen = sizeof(out) - 1;
    inlen = input->end - input->cur;
    res = xmlEncInputChunk(handler, out, &outlen, input->cur, &inlen, 0);
    if (res < 0)
        return(handler);
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
            handler = xmlFindCharEncodingHandler((char *) out + start);
            break;
        }
    }

    return(handler);
}

/**
 * xmlSwitchEncoding:
 * @ctxt:  the parser context
 * @enc:  the encoding value (number)
 *
 * change the input functions when discovering the character encoding
 * of a given entity.
 *
 * Returns 0 in case of success, -1 otherwise
 */
int
xmlSwitchEncoding(xmlParserCtxtPtr ctxt, xmlCharEncoding enc)
{
    xmlCharEncodingHandlerPtr handler;
    int ret;

    if (ctxt == NULL) return(-1);

    /*
     * FIXME: The BOM shouldn't be skipped here, but in the parsing code.
     *
     * Note that we look for a decoded UTF-8 BOM when switching to UTF-16.
     * This is mostly useless but Webkit/Chromium relies on this behavior.
     * See https://bugs.chromium.org/p/chromium/issues/detail?id=1451026
     */
    if ((ctxt->input != NULL) &&
        (ctxt->input->consumed == 0) &&
        (ctxt->input->cur != NULL) &&
        (ctxt->input->cur == ctxt->input->base) &&
        ((enc == XML_CHAR_ENCODING_UTF8) ||
         (enc == XML_CHAR_ENCODING_UTF16LE) ||
         (enc == XML_CHAR_ENCODING_UTF16BE))) {
        /*
         * Errata on XML-1.0 June 20 2001
         * Specific handling of the Byte Order Mark for
         * UTF-8
         */
        if ((ctxt->input->cur[0] == 0xEF) &&
            (ctxt->input->cur[1] == 0xBB) &&
            (ctxt->input->cur[2] == 0xBF)) {
            ctxt->input->cur += 3;
        }
    }

    switch (enc) {
	case XML_CHAR_ENCODING_ERROR:
	    __xmlErrEncoding(ctxt, XML_ERR_UNKNOWN_ENCODING,
	                   "encoding unknown\n", NULL, NULL);
	    return(-1);
	case XML_CHAR_ENCODING_NONE:
	    /* let's assume it's UTF-8 without the XML decl */
	    ctxt->charset = XML_CHAR_ENCODING_UTF8;
	    return(0);
	case XML_CHAR_ENCODING_UTF8:
	    /* default encoding, no conversion should be needed */
	    ctxt->charset = XML_CHAR_ENCODING_UTF8;
	    return(0);
        case XML_CHAR_ENCODING_EBCDIC:
            handler = xmlDetectEBCDIC(ctxt->input);
            break;
        default:
            handler = xmlGetCharEncodingHandler(enc);
            break;
    }
    if (handler == NULL) {
	/*
	 * Default handlers.
	 */
	switch (enc) {
	    case XML_CHAR_ENCODING_ASCII:
		/* default encoding, no conversion should be needed */
		ctxt->charset = XML_CHAR_ENCODING_UTF8;
		return(0);
	    case XML_CHAR_ENCODING_8859_1:
		if ((ctxt->inputNr == 1) &&
		    (ctxt->encoding == NULL) &&
		    (ctxt->input != NULL) &&
		    (ctxt->input->encoding != NULL)) {
		    ctxt->encoding = xmlStrdup(ctxt->input->encoding);
		}
		ctxt->charset = enc;
		return(0);
	    default:
		__xmlErrEncoding(ctxt, XML_ERR_UNSUPPORTED_ENCODING,
                        "encoding not supported: %s\n",
			BAD_CAST xmlGetCharEncodingName(enc), NULL);
                /*
                 * TODO: We could recover from errors in external entities
                 * if we didn't stop the parser. But most callers of this
                 * function don't check the return value.
                 */
                xmlStopParser(ctxt);
                return(-1);
        }
    }
    ret = xmlSwitchInputEncoding(ctxt, ctxt->input, handler);
    if ((ret < 0) || (ctxt->errNo == XML_I18N_CONV_FAILED)) {
        /*
	 * on encoding conversion errors, stop the parser
	 */
        xmlStopParser(ctxt);
	ctxt->errNo = XML_I18N_CONV_FAILED;
    }
    return(ret);
}

/**
 * xmlSwitchInputEncoding:
 * @ctxt:  the parser context
 * @input:  the input stream
 * @handler:  the encoding handler
 *
 * change the input functions when discovering the character encoding
 * of a given entity.
 *
 * Returns 0 in case of success, -1 otherwise
 */
int
xmlSwitchInputEncoding(xmlParserCtxtPtr ctxt, xmlParserInputPtr input,
                       xmlCharEncodingHandlerPtr handler)
{
    int nbchars;
    xmlParserInputBufferPtr in;

    if (handler == NULL)
        return (-1);
    if (input == NULL)
        return (-1);
    in = input->buf;
    if (in == NULL) {
	xmlErrInternal(ctxt,
                "static memory buffer doesn't support encoding\n", NULL);
        /*
         * Callers assume that the input buffer takes ownership of the
         * encoding handler. xmlCharEncCloseFunc frees unregistered
         * handlers and avoids a memory leak.
         */
        xmlCharEncCloseFunc(handler);
	return (-1);
    }

    if (in->encoder != NULL) {
        if (in->encoder == handler)
            return (0);

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
        return (0);
    }

    ctxt->charset = XML_CHAR_ENCODING_UTF8;
    in->encoder = handler;

    /*
     * Is there already some content down the pipe to convert ?
     */
    if (xmlBufIsEmpty(in->buffer) == 0) {
        size_t processed, use, consumed;

        /*
         * FIXME: The BOM shouldn't be skipped here, but in the parsing code.
         */

        /*
         * Specific handling of the Byte Order Mark for
         * UTF-16
         */
        if ((handler->name != NULL) &&
            (!strcmp(handler->name, "UTF-16LE") ||
             !strcmp(handler->name, "UTF-16")) &&
            (input->cur[0] == 0xFF) && (input->cur[1] == 0xFE)) {
            input->cur += 2;
        }
        if ((handler->name != NULL) &&
            (!strcmp(handler->name, "UTF-16BE")) &&
            (input->cur[0] == 0xFE) && (input->cur[1] == 0xFF)) {
            input->cur += 2;
        }
        /*
         * Errata on XML-1.0 June 20 2001
         * Specific handling of the Byte Order Mark for
         * UTF-8
         */
        if ((handler->name != NULL) &&
            (!strcmp(handler->name, "UTF-8")) &&
            (input->cur[0] == 0xEF) &&
            (input->cur[1] == 0xBB) && (input->cur[2] == 0xBF)) {
            input->cur += 3;
        }

        /*
         * Shrink the current input buffer.
         * Move it as the raw buffer and create a new input buffer
         */
        processed = input->cur - input->base;
        xmlBufShrink(in->buffer, processed);
        input->consumed += processed;
        in->raw = in->buffer;
        in->buffer = xmlBufCreate();
        in->rawconsumed = processed;
        use = xmlBufUse(in->raw);

        /*
         * TODO: We must flush and decode the whole buffer to make functions
         * like xmlReadMemory work with a user-provided encoding. If the
         * encoding is specified directly, we should probably set
         * XML_PARSE_IGNORE_ENC in xmlDoRead to avoid switching encodings
         * twice. Then we could set "flush" to false which should save
         * a considerable amount of memory when parsing from memory.
         * It's probably even possible to remove this whole if-block
         * completely.
         */
        nbchars = xmlCharEncInput(in, 1);
        xmlBufResetInput(in->buffer, input);
        if (nbchars < 0) {
            /* TODO: This could be an out of memory or an encoding error. */
            xmlErrInternal(ctxt,
                           "switching encoding: encoder error\n",
                           NULL);
            xmlHaltParser(ctxt);
            return (-1);
        }
        consumed = use - xmlBufUse(in->raw);
        if ((consumed > ULONG_MAX) ||
            (in->rawconsumed > ULONG_MAX - (unsigned long)consumed))
            in->rawconsumed = ULONG_MAX;
        else
	    in->rawconsumed += consumed;
    }
    return (0);
}

/**
 * xmlSwitchToEncoding:
 * @ctxt:  the parser context
 * @handler:  the encoding handler
 *
 * change the input functions when discovering the character encoding
 * of a given entity.
 *
 * Returns 0 in case of success, -1 otherwise
 */
int
xmlSwitchToEncoding(xmlParserCtxtPtr ctxt, xmlCharEncodingHandlerPtr handler)
{
    if (ctxt == NULL)
        return(-1);
    return(xmlSwitchInputEncoding(ctxt, ctxt->input, handler));
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
    if (input->directory != NULL) xmlFree((char *) input->directory);
    if (input->encoding != NULL) xmlFree((char *) input->encoding);
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
 * Create a new input stream structure.
 *
 * Returns the new input stream or NULL
 */
xmlParserInputPtr
xmlNewInputStream(xmlParserCtxtPtr ctxt) {
    xmlParserInputPtr input;

    input = (xmlParserInputPtr) xmlMalloc(sizeof(xmlParserInput));
    if (input == NULL) {
        xmlErrMemory(ctxt,  "couldn't allocate a new input stream\n");
	return(NULL);
    }
    memset(input, 0, sizeof(xmlParserInput));
    input->line = 1;
    input->col = 1;
    input->standalone = -1;

    /*
     * If the context is NULL the id cannot be initialized, but that
     * should not happen while parsing which is the situation where
     * the id is actually needed.
     */
    if (ctxt != NULL) {
        if (input->id >= INT_MAX) {
            xmlErrMemory(ctxt, "Input ID overflow\n");
            return(NULL);
        }
        input->id = ctxt->input_id++;
    }

    return(input);
}

/**
 * xmlNewIOInputStream:
 * @ctxt:  an XML parser context
 * @input:  an I/O Input
 * @enc:  the charset encoding if known
 *
 * Create a new input stream structure encapsulating the @input into
 * a stream suitable for the parser.
 *
 * Returns the new input stream or NULL
 */
xmlParserInputPtr
xmlNewIOInputStream(xmlParserCtxtPtr ctxt, xmlParserInputBufferPtr input,
	            xmlCharEncoding enc) {
    xmlParserInputPtr inputStream;

    if (input == NULL) return(NULL);
    if (xmlParserDebugEntities)
	xmlGenericError(xmlGenericErrorContext, "new input from I/O\n");
    inputStream = xmlNewInputStream(ctxt);
    if (inputStream == NULL) {
	return(NULL);
    }
    inputStream->filename = NULL;
    inputStream->buf = input;
    xmlBufResetInput(inputStream->buf->buffer, inputStream);

    if (enc != XML_CHAR_ENCODING_NONE) {
        xmlSwitchEncoding(ctxt, enc);
    }

    return(inputStream);
}

/**
 * xmlNewEntityInputStream:
 * @ctxt:  an XML parser context
 * @entity:  an Entity pointer
 *
 * Create a new input stream based on an xmlEntityPtr
 *
 * Returns the new input stream or NULL
 */
xmlParserInputPtr
xmlNewEntityInputStream(xmlParserCtxtPtr ctxt, xmlEntityPtr entity) {
    xmlParserInputPtr input;

    if (entity == NULL) {
        xmlErrInternal(ctxt, "xmlNewEntityInputStream entity = NULL\n",
	               NULL);
	return(NULL);
    }
    if (xmlParserDebugEntities)
	xmlGenericError(xmlGenericErrorContext,
		"new input from entity: %s\n", entity->name);
    if (entity->content == NULL) {
	switch (entity->etype) {
            case XML_EXTERNAL_GENERAL_UNPARSED_ENTITY:
	        xmlErrInternal(ctxt, "Cannot parse entity %s\n",
		               entity->name);
                break;
            case XML_EXTERNAL_GENERAL_PARSED_ENTITY:
            case XML_EXTERNAL_PARAMETER_ENTITY:
		input = xmlLoadExternalEntity((char *) entity->URI,
		       (char *) entity->ExternalID, ctxt);
                if (input != NULL)
                    input->entity = entity;
                return(input);
            case XML_INTERNAL_GENERAL_ENTITY:
	        xmlErrInternal(ctxt,
		      "Internal entity %s without content !\n",
		               entity->name);
                break;
            case XML_INTERNAL_PARAMETER_ENTITY:
	        xmlErrInternal(ctxt,
		      "Internal parameter entity %s without content !\n",
		               entity->name);
                break;
            case XML_INTERNAL_PREDEFINED_ENTITY:
	        xmlErrInternal(ctxt,
		      "Predefined entity %s without content !\n",
		               entity->name);
                break;
	}
	return(NULL);
    }
    input = xmlNewInputStream(ctxt);
    if (input == NULL) {
	return(NULL);
    }
    if (entity->URI != NULL)
	input->filename = (char *) xmlStrdup((xmlChar *) entity->URI);
    input->base = entity->content;
    if (entity->length == 0)
        entity->length = xmlStrlen(entity->content);
    input->cur = entity->content;
    input->length = entity->length;
    input->end = &entity->content[input->length];
    input->entity = entity;
    return(input);
}

/**
 * xmlNewStringInputStream:
 * @ctxt:  an XML parser context
 * @buffer:  an memory buffer
 *
 * Create a new input stream based on a memory buffer.
 * Returns the new input stream
 */
xmlParserInputPtr
xmlNewStringInputStream(xmlParserCtxtPtr ctxt, const xmlChar *buffer) {
    xmlParserInputPtr input;
    xmlParserInputBufferPtr buf;

    if (buffer == NULL) {
        xmlErrInternal(ctxt, "xmlNewStringInputStream string = NULL\n",
	               NULL);
	return(NULL);
    }
    if (xmlParserDebugEntities)
	xmlGenericError(xmlGenericErrorContext,
		"new fixed input: %.30s\n", buffer);
    buf = xmlParserInputBufferCreateMem((const char *) buffer,
                                        xmlStrlen(buffer),
                                        XML_CHAR_ENCODING_NONE);
    if (buf == NULL) {
	xmlErrMemory(ctxt, NULL);
        return(NULL);
    }
    input = xmlNewInputStream(ctxt);
    if (input == NULL) {
        xmlErrMemory(ctxt,  "couldn't allocate a new input stream\n");
	xmlFreeParserInputBuffer(buf);
	return(NULL);
    }
    input->buf = buf;
    xmlBufResetInput(input->buf->buffer, input);
    return(input);
}

/**
 * xmlNewInputFromFile:
 * @ctxt:  an XML parser context
 * @filename:  the filename to use as entity
 *
 * Create a new input stream based on a file or an URL.
 *
 * Returns the new input stream or NULL in case of error
 */
xmlParserInputPtr
xmlNewInputFromFile(xmlParserCtxtPtr ctxt, const char *filename) {
    xmlParserInputBufferPtr buf;
    xmlParserInputPtr inputStream;
    char *directory = NULL;
    xmlChar *URI = NULL;

    if (xmlParserDebugEntities)
	xmlGenericError(xmlGenericErrorContext,
		"new input from file: %s\n", filename);
    if (ctxt == NULL) return(NULL);
    buf = xmlParserInputBufferCreateFilename(filename, XML_CHAR_ENCODING_NONE);
    if (buf == NULL) {
	if (filename == NULL)
	    __xmlLoaderErr(ctxt,
	                   "failed to load external entity: NULL filename \n",
			   NULL);
	else
	    __xmlLoaderErr(ctxt, "failed to load external entity \"%s\"\n",
			   (const char *) filename);
	return(NULL);
    }

    inputStream = xmlNewInputStream(ctxt);
    if (inputStream == NULL) {
	xmlFreeParserInputBuffer(buf);
	return(NULL);
    }

    inputStream->buf = buf;
    inputStream = xmlCheckHTTPInput(ctxt, inputStream);
    if (inputStream == NULL)
        return(NULL);

    if (inputStream->filename == NULL)
	URI = xmlStrdup((xmlChar *) filename);
    else
	URI = xmlStrdup((xmlChar *) inputStream->filename);
    directory = xmlParserGetDirectory((const char *) URI);
    if (inputStream->filename != NULL) xmlFree((char *)inputStream->filename);
    inputStream->filename = (char *) xmlCanonicPath((const xmlChar *) URI);
    if (URI != NULL) xmlFree((char *) URI);
    inputStream->directory = directory;

    xmlBufResetInput(inputStream->buf->buffer, inputStream);
    if ((ctxt->directory == NULL) && (directory != NULL))
        ctxt->directory = (char *) xmlStrdup((const xmlChar *) directory);
    return(inputStream);
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

    if(ctxt==NULL) {
        xmlErrInternal(NULL, "Got NULL parser context\n", NULL);
        return(-1);
    }

    xmlInitParser();

    if (ctxt->dict == NULL)
	ctxt->dict = xmlDictCreate();
    if (ctxt->dict == NULL) {
        xmlErrMemory(NULL, "cannot initialize parser context\n");
	return(-1);
    }
    xmlDictSetLimit(ctxt->dict, XML_MAX_DICTIONARY_LIMIT);

    if (ctxt->sax == NULL)
	ctxt->sax = (xmlSAXHandler *) xmlMalloc(sizeof(xmlSAXHandler));
    if (ctxt->sax == NULL) {
        xmlErrMemory(NULL, "cannot initialize parser context\n");
	return(-1);
    }
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
	ctxt->inputTab = (xmlParserInputPtr *)
		    xmlMalloc(5 * sizeof(xmlParserInputPtr));
	ctxt->inputMax = 5;
    }
    if (ctxt->inputTab == NULL) {
        xmlErrMemory(NULL, "cannot initialize parser context\n");
	ctxt->inputNr = 0;
	ctxt->inputMax = 0;
	ctxt->input = NULL;
	return(-1);
    }
    while ((input = inputPop(ctxt)) != NULL) { /* Non consuming */
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
    ctxt->external = 0;
    ctxt->instate = XML_PARSER_START;
    ctxt->token = 0;
    ctxt->directory = NULL;

    /* Allocate the Node stack */
    if (ctxt->nodeTab == NULL) {
	ctxt->nodeTab = (xmlNodePtr *) xmlMalloc(10 * sizeof(xmlNodePtr));
	ctxt->nodeMax = 10;
    }
    if (ctxt->nodeTab == NULL) {
        xmlErrMemory(NULL, "cannot initialize parser context\n");
	ctxt->nodeNr = 0;
	ctxt->nodeMax = 0;
	ctxt->node = NULL;
	ctxt->inputNr = 0;
	ctxt->inputMax = 0;
	ctxt->input = NULL;
	return(-1);
    }
    ctxt->nodeNr = 0;
    ctxt->node = NULL;

    /* Allocate the Name stack */
    if (ctxt->nameTab == NULL) {
	ctxt->nameTab = (const xmlChar **) xmlMalloc(10 * sizeof(xmlChar *));
	ctxt->nameMax = 10;
    }
    if (ctxt->nameTab == NULL) {
        xmlErrMemory(NULL, "cannot initialize parser context\n");
	ctxt->nodeNr = 0;
	ctxt->nodeMax = 0;
	ctxt->node = NULL;
	ctxt->inputNr = 0;
	ctxt->inputMax = 0;
	ctxt->input = NULL;
	ctxt->nameNr = 0;
	ctxt->nameMax = 0;
	ctxt->name = NULL;
	return(-1);
    }
    ctxt->nameNr = 0;
    ctxt->name = NULL;

    /* Allocate the space stack */
    if (ctxt->spaceTab == NULL) {
	ctxt->spaceTab = (int *) xmlMalloc(10 * sizeof(int));
	ctxt->spaceMax = 10;
    }
    if (ctxt->spaceTab == NULL) {
        xmlErrMemory(NULL, "cannot initialize parser context\n");
	ctxt->nodeNr = 0;
	ctxt->nodeMax = 0;
	ctxt->node = NULL;
	ctxt->inputNr = 0;
	ctxt->inputMax = 0;
	ctxt->input = NULL;
	ctxt->nameNr = 0;
	ctxt->nameMax = 0;
	ctxt->name = NULL;
	ctxt->spaceNr = 0;
	ctxt->spaceMax = 0;
	ctxt->space = NULL;
	return(-1);
    }
    ctxt->spaceNr = 1;
    ctxt->spaceMax = 10;
    ctxt->spaceTab[0] = -1;
    ctxt->space = &ctxt->spaceTab[0];
    ctxt->myDoc = NULL;
    ctxt->wellFormed = 1;
    ctxt->nsWellFormed = 1;
    ctxt->valid = 1;
    ctxt->loadsubset = xmlLoadExtDtdDefaultValue;
    if (ctxt->loadsubset) {
        ctxt->options |= XML_PARSE_DTDLOAD;
    }
    ctxt->validate = xmlDoValidityCheckingDefaultValue;
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

    ctxt->vctxt.flags = XML_VCTXT_USE_PCTXT;
    ctxt->vctxt.userData = ctxt;
    ctxt->vctxt.error = xmlParserValidityError;
    ctxt->vctxt.warning = xmlParserValidityWarning;
    if (ctxt->validate) {
	if (xmlGetWarningsDefaultValue == 0)
	    ctxt->vctxt.warning = NULL;
	else
	    ctxt->vctxt.warning = xmlParserValidityWarning;
	ctxt->vctxt.nodeMax = 0;
        ctxt->options |= XML_PARSE_DTDVALID;
    }
    ctxt->replaceEntities = xmlSubstituteEntitiesDefaultValue;
    if (ctxt->replaceEntities) {
        ctxt->options |= XML_PARSE_NOENT;
    }
    ctxt->record_info = 0;
    ctxt->checkIndex = 0;
    ctxt->inSubset = 0;
    ctxt->errNo = XML_ERR_OK;
    ctxt->depth = 0;
    ctxt->charset = XML_CHAR_ENCODING_UTF8;
    ctxt->catalogs = NULL;
    ctxt->sizeentities = 0;
    ctxt->sizeentcopy = 0;
    ctxt->input_id = 1;
    xmlInitNodeInfoSeq(&ctxt->node_seq);
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

    while ((input = inputPop(ctxt)) != NULL) { /* Non consuming */
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
    if (ctxt->directory != NULL) xmlFree((char *) ctxt->directory);
    if (ctxt->vctxt.nodeTab != NULL) xmlFree(ctxt->vctxt.nodeTab);
    if (ctxt->atts != NULL) xmlFree((xmlChar * *)ctxt->atts);
    if (ctxt->dict != NULL) xmlDictFree(ctxt->dict);
    if (ctxt->nsTab != NULL) xmlFree((char *) ctxt->nsTab);
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
 * Returns the xmlParserCtxtPtr or NULL if memory allocation failed.
 */

xmlParserCtxtPtr
xmlNewSAXParserCtxt(const xmlSAXHandler *sax, void *userData)
{
    xmlParserCtxtPtr ctxt;

    ctxt = (xmlParserCtxtPtr) xmlMalloc(sizeof(xmlParserCtxt));
    if (ctxt == NULL) {
	xmlErrMemory(NULL, "cannot allocate parser context\n");
	return(NULL);
    }
    memset(ctxt, 0, sizeof(xmlParserCtxt));
    if (xmlInitSAXParserCtxt(ctxt, sax, userData) < 0) {
        xmlFreeParserCtxt(ctxt);
	return(NULL);
    }
    return(ctxt);
}

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
xmlParserFindNodeInfo(const xmlParserCtxtPtr ctx, const xmlNodePtr node)
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
xmlParserFindNodeInfoIndex(const xmlParserNodeInfoSeqPtr seq,
                           const xmlNodePtr node)
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
                     const xmlParserNodeInfoPtr info)
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
        if ((ctxt->node_seq.length + 1 > ctxt->node_seq.maximum) ||
	    (ctxt->node_seq.buffer == NULL)) {
            xmlParserNodeInfo *tmp_buffer;
            unsigned int byte_size;

            if (ctxt->node_seq.maximum == 0)
                ctxt->node_seq.maximum = 2;
            byte_size = (sizeof(*ctxt->node_seq.buffer) *
			(2 * ctxt->node_seq.maximum));

            if (ctxt->node_seq.buffer == NULL)
                tmp_buffer = (xmlParserNodeInfo *) xmlMalloc(byte_size);
            else
                tmp_buffer =
                    (xmlParserNodeInfo *) xmlRealloc(ctxt->node_seq.buffer,
                                                     byte_size);

            if (tmp_buffer == NULL) {
		xmlErrMemory(ctxt, "failed to allocate buffer\n");
                return;
            }
            ctxt->node_seq.buffer = tmp_buffer;
            ctxt->node_seq.maximum *= 2;
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
    if (!val) xmlIndentTreeOutput = 1;
    return(old);
}

