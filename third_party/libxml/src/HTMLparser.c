/*
 * HTMLparser.c : an HTML parser
 *
 * References:
 *   HTML Living Standard
 *     https://html.spec.whatwg.org/multipage/parsing.html
 *
 * Tokenization now conforms to HTML5. Tree construction still follows
 * a custom, non-standard implementation. See:
 *
 *     https://gitlab.gnome.org/GNOME/libxml2/-/issues/211
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXML
#include "libxml.h"
#ifdef LIBXML_HTML_ENABLED

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <libxml/HTMLparser.h>
#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlerror.h>
#include <libxml/HTMLtree.h>
#include <libxml/entities.h>
#include <libxml/encoding.h>
#include <libxml/xmlIO.h>
#include <libxml/uri.h>

#include "private/buf.h"
#include "private/dict.h"
#include "private/enc.h"
#include "private/error.h"
#include "private/html.h"
#include "private/io.h"
#include "private/memory.h"
#include "private/parser.h"
#include "private/tree.h"

#define HTML_MAX_NAMELEN 1000
#define HTML_MAX_ATTRS 100000000 /* 100 million */
#define HTML_PARSER_BIG_BUFFER_SIZE 1000
#define HTML_PARSER_BUFFER_SIZE 100

#define IS_WS_HTML(c) \
    (((c) == 0x20) || \
     (((c) >= 0x09) && ((c) <= 0x0D) && ((c) != 0x0B)))

#define IS_HEX_DIGIT(c) \
    ((IS_ASCII_DIGIT(c)) || \
     ((((c) | 0x20) >= 'a') && (((c) | 0x20) <= 'f')))

#define IS_UPPER(c) \
    (((c) >= 'A') && ((c) <= 'Z'))

#define IS_ALNUM(c) \
    (IS_ASCII_LETTER(c) || IS_ASCII_DIGIT(c))

typedef enum {
    INSERT_INITIAL = 1,
    INSERT_IN_HEAD = 3,
    INSERT_IN_BODY = 10
} htmlInsertMode;

typedef const unsigned htmlAsciiMask[2];

static htmlAsciiMask MASK_DQ = {
    0,
    1u << ('"' - 32),
};
static htmlAsciiMask MASK_SQ = {
    0,
    1u << ('\'' - 32),
};
static htmlAsciiMask MASK_GT = {
    0,
    1u << ('>' - 32),
};
static htmlAsciiMask MASK_DASH = {
    0,
    1u << ('-' - 32),
};
static htmlAsciiMask MASK_WS_GT = {
    1u << 0x09 | 1u << 0x0A | 1u << 0x0C | 1u << 0x0D,
    1u << (' ' - 32) | 1u << ('>' - 32),
};
static htmlAsciiMask MASK_DQ_GT = {
    0,
    1u << ('"' - 32) | 1u << ('>' - 32),
};
static htmlAsciiMask MASK_SQ_GT = {
    0,
    1u << ('\'' - 32) | 1u << ('>' - 32),
};

static int htmlOmittedDefaultValue = 1;

static int
htmlParseElementInternal(htmlParserCtxtPtr ctxt);

/************************************************************************
 *									*
 *		Some factorized error routines				*
 *									*
 ************************************************************************/

/**
 * htmlErrMemory:
 * @ctxt:  an HTML parser context
 * @extra:  extra information
 *
 * Handle a redefinition of attribute error
 */
static void
htmlErrMemory(xmlParserCtxtPtr ctxt)
{
    xmlCtxtErrMemory(ctxt);
}

/**
 * htmlParseErr:
 * @ctxt:  an HTML parser context
 * @error:  the error number
 * @msg:  the error message
 * @str1:  string infor
 * @str2:  string infor
 *
 * Handle a fatal parser error, i.e. violating Well-Formedness constraints
 */
static void LIBXML_ATTR_FORMAT(3,0)
htmlParseErr(xmlParserCtxtPtr ctxt, xmlParserErrors error,
             const char *msg, const xmlChar *str1, const xmlChar *str2)
{
    xmlCtxtErr(ctxt, NULL, XML_FROM_HTML, error, XML_ERR_ERROR,
               str1, str2, NULL, 0, msg, str1, str2);
}

/************************************************************************
 *									*
 *	Parser stacks related functions and macros		*
 *									*
 ************************************************************************/

/**
 * htmlnamePush:
 * @ctxt:  an HTML parser context
 * @value:  the element name
 *
 * Pushes a new element name on top of the name stack
 *
 * Returns -1 in case of error, the index in the stack otherwise
 */
static int
htmlnamePush(htmlParserCtxtPtr ctxt, const xmlChar * value)
{
    if ((ctxt->html < INSERT_IN_HEAD) && (xmlStrEqual(value, BAD_CAST "head")))
        ctxt->html = INSERT_IN_HEAD;
    if ((ctxt->html < INSERT_IN_BODY) && (xmlStrEqual(value, BAD_CAST "body")))
        ctxt->html = INSERT_IN_BODY;
    if (ctxt->nameNr >= ctxt->nameMax) {
        const xmlChar **tmp;
        int newSize;

        newSize = xmlGrowCapacity(ctxt->nameMax, sizeof(tmp[0]),
                                  10, XML_MAX_ITEMS);
        if (newSize < 0) {
            htmlErrMemory(ctxt);
            return (-1);
        }
        tmp = xmlRealloc(ctxt->nameTab, newSize * sizeof(tmp[0]));
        if (tmp == NULL) {
            htmlErrMemory(ctxt);
            return(-1);
        }
        ctxt->nameTab = tmp;
        ctxt->nameMax = newSize;
    }
    ctxt->nameTab[ctxt->nameNr] = value;
    ctxt->name = value;
    return (ctxt->nameNr++);
}
/**
 * htmlnamePop:
 * @ctxt: an HTML parser context
 *
 * Pops the top element name from the name stack
 *
 * Returns the name just removed
 */
static const xmlChar *
htmlnamePop(htmlParserCtxtPtr ctxt)
{
    const xmlChar *ret;

    if (ctxt->nameNr <= 0)
        return (NULL);
    ctxt->nameNr--;
    if (ctxt->nameNr < 0)
        return (NULL);
    if (ctxt->nameNr > 0)
        ctxt->name = ctxt->nameTab[ctxt->nameNr - 1];
    else
        ctxt->name = NULL;
    ret = ctxt->nameTab[ctxt->nameNr];
    ctxt->nameTab[ctxt->nameNr] = NULL;
    return (ret);
}

/**
 * htmlNodeInfoPush:
 * @ctxt:  an HTML parser context
 * @value:  the node info
 *
 * Pushes a new element name on top of the node info stack
 *
 * Returns 0 in case of error, the index in the stack otherwise
 */
static int
htmlNodeInfoPush(htmlParserCtxtPtr ctxt, htmlParserNodeInfo *value)
{
    if (ctxt->nodeInfoNr >= ctxt->nodeInfoMax) {
        xmlParserNodeInfo *tmp;
        int newSize;

        newSize = xmlGrowCapacity(ctxt->nodeInfoMax, sizeof(tmp[0]),
                                  5, XML_MAX_ITEMS);
        if (newSize < 0) {
            htmlErrMemory(ctxt);
            return (0);
        }
        tmp = xmlRealloc(ctxt->nodeInfoTab, newSize * sizeof(tmp[0]));
        if (tmp == NULL) {
            htmlErrMemory(ctxt);
            return (0);
        }
        ctxt->nodeInfoTab = tmp;
        ctxt->nodeInfoMax = newSize;
    }
    ctxt->nodeInfoTab[ctxt->nodeInfoNr] = *value;
    ctxt->nodeInfo = &ctxt->nodeInfoTab[ctxt->nodeInfoNr];
    return (ctxt->nodeInfoNr++);
}

/**
 * htmlNodeInfoPop:
 * @ctxt:  an HTML parser context
 *
 * Pops the top element name from the node info stack
 *
 * Returns 0 in case of error, the pointer to NodeInfo otherwise
 */
static htmlParserNodeInfo *
htmlNodeInfoPop(htmlParserCtxtPtr ctxt)
{
    if (ctxt->nodeInfoNr <= 0)
        return (NULL);
    ctxt->nodeInfoNr--;
    if (ctxt->nodeInfoNr < 0)
        return (NULL);
    if (ctxt->nodeInfoNr > 0)
        ctxt->nodeInfo = &ctxt->nodeInfoTab[ctxt->nodeInfoNr - 1];
    else
        ctxt->nodeInfo = NULL;
    return &ctxt->nodeInfoTab[ctxt->nodeInfoNr];
}

/*
 * Macros for accessing the content. Those should be used only by the parser,
 * and not exported.
 *
 * Dirty macros, i.e. one need to make assumption on the context to use them
 *
 *   CUR_PTR return the current pointer to the xmlChar to be parsed.
 *   CUR     returns the current xmlChar value, i.e. a 8 bit value if compiled
 *           in ISO-Latin or UTF-8, and the current 16 bit value if compiled
 *           in UNICODE mode. This should be used internally by the parser
 *           only to compare to ASCII values otherwise it would break when
 *           running with UTF-8 encoding.
 *   NXT(n)  returns the n'th next xmlChar. Same as CUR is should be used only
 *           to compare on ASCII based substring.
 *   UPP(n)  returns the n'th next xmlChar converted to uppercase. Same as CUR
 *           it should be used only to compare on ASCII based substring.
 *   SKIP(n) Skip n xmlChar, and must also be used only to skip ASCII defined
 *           strings without newlines within the parser.
 *
 * Clean macros, not dependent of an ASCII context, expect UTF-8 encoding
 *
 *   COPY(to) copy one char to *to, increment CUR_PTR and to accordingly
 */

#define UPPER (toupper(*ctxt->input->cur))

#define SKIP(val) ctxt->input->cur += (val),ctxt->input->col+=(val)

#define NXT(val) ctxt->input->cur[(val)]

#define UPP(val) (toupper(ctxt->input->cur[(val)]))

#define CUR_PTR ctxt->input->cur
#define BASE_PTR ctxt->input->base

#define SHRINK \
    if ((!PARSER_PROGRESSIVE(ctxt)) && \
        (ctxt->input->cur - ctxt->input->base > 2 * INPUT_CHUNK) && \
	(ctxt->input->end - ctxt->input->cur < 2 * INPUT_CHUNK)) \
	xmlParserShrink(ctxt);

#define GROW \
    if ((!PARSER_PROGRESSIVE(ctxt)) && \
        (ctxt->input->end - ctxt->input->cur < INPUT_CHUNK)) \
	xmlParserGrow(ctxt);

#define SKIP_BLANKS htmlSkipBlankChars(ctxt)

/* Imported from XML */

#define CUR (*ctxt->input->cur)

/**
 * htmlFindEncoding:
 * @the HTML parser context
 *
 * Ty to find and encoding in the current data available in the input
 * buffer this is needed to try to switch to the proper encoding when
 * one face a character error.
 * That's an heuristic, since it's operating outside of parsing it could
 * try to use a meta which had been commented out, that's the reason it
 * should only be used in case of error, not as a default.
 *
 * Returns an encoding string or NULL if not found, the string need to
 *   be freed
 */
static xmlChar *
htmlFindEncoding(xmlParserCtxtPtr ctxt) {
    const xmlChar *start, *cur, *end;
    xmlChar *ret;

    if ((ctxt == NULL) || (ctxt->input == NULL) ||
        (ctxt->input->flags & XML_INPUT_HAS_ENCODING))
        return(NULL);
    if ((ctxt->input->cur == NULL) || (ctxt->input->end == NULL))
        return(NULL);

    start = ctxt->input->cur;
    end = ctxt->input->end;
    /* we also expect the input buffer to be zero terminated */
    if (*end != 0)
        return(NULL);

    cur = xmlStrcasestr(start, BAD_CAST "HTTP-EQUIV");
    if (cur == NULL)
        return(NULL);
    cur = xmlStrcasestr(cur, BAD_CAST  "CONTENT");
    if (cur == NULL)
        return(NULL);
    cur = xmlStrcasestr(cur, BAD_CAST  "CHARSET=");
    if (cur == NULL)
        return(NULL);
    cur += 8;
    start = cur;
    while ((IS_ALNUM(*cur)) ||
           (*cur == '-') || (*cur == '_') || (*cur == ':') || (*cur == '/'))
           cur++;
    if (cur == start)
        return(NULL);
    ret = xmlStrndup(start, cur - start);
    if (ret == NULL)
        htmlErrMemory(ctxt);
    return(ret);
}

static int
htmlMaskMatch(htmlAsciiMask mask, unsigned c) {
    if (c >= 64)
        return(0);
    return((mask[c/32] >> (c & 31)) & 1);
}

static int
htmlValidateUtf8(xmlParserCtxtPtr ctxt, const xmlChar *str, size_t len,
                 int partial) {
    unsigned c = str[0];
    int size;

    if (c < 0xC2) {
        goto invalid;
    } else if (c < 0xE0) {
        if (len < 2)
            goto incomplete;
        if ((str[1] & 0xC0) != 0x80)
            goto invalid;
        size = 2;
    } else if (c < 0xF0) {
        unsigned v;

        if (len < 3)
            goto incomplete;

        v = str[1] << 8 | str[2]; /* hint to generate 16-bit load */
        v |= c << 16;

        if (((v & 0x00C0C0) != 0x008080) ||
            ((v & 0x0F2000) == 0x000000) ||
            ((v & 0x0F2000) == 0x0D2000))
            goto invalid;

        size = 3;
    } else {
        unsigned v;

        if (len < 4)
            goto incomplete;

        v = c << 24 | str[1] << 16 | str[2] << 8 | str[3];

        if (((v & 0x00C0C0C0) != 0x00808080) ||
            (v < 0xF0900000) || (v >= 0xF4900000))
            goto invalid;

        size = 4;
    }

    return(size);

incomplete:
    if (partial)
        return(0);

invalid:
    /* Only report the first error */
    if ((ctxt->input->flags & XML_INPUT_ENCODING_ERROR) == 0) {
        htmlParseErr(ctxt, XML_ERR_INVALID_ENCODING,
                     "Invalid bytes in character encoding\n", NULL, NULL);
        ctxt->input->flags |= XML_INPUT_ENCODING_ERROR;
    }

    return(-1);
}

/**
 * htmlSkipBlankChars:
 * @ctxt:  the HTML parser context
 *
 * skip all blanks character found at that point in the input streams.
 *
 * Returns the number of space chars skipped
 */

static int
htmlSkipBlankChars(xmlParserCtxtPtr ctxt) {
    const xmlChar *cur = ctxt->input->cur;
    size_t avail = ctxt->input->end - cur;
    int res = 0;
    int line = ctxt->input->line;
    int col = ctxt->input->col;

    while (!PARSER_STOPPED(ctxt)) {
        if (avail == 0) {
            ctxt->input->cur = cur;
            GROW;
            cur = ctxt->input->cur;
            avail = ctxt->input->end - cur;

            if (avail == 0)
                break;
        }

        if (*cur == '\n') {
            line++;
            col = 1;
        } else if (IS_WS_HTML(*cur)) {
            col++;
        } else {
            break;
        }

        cur += 1;
        avail -= 1;

	if (res < INT_MAX)
	    res++;
    }

    ctxt->input->cur = cur;
    ctxt->input->line = line;
    ctxt->input->col = col;

    if (res > 8)
        GROW;

    return(res);
}



/************************************************************************
 *									*
 *	The list of HTML elements and their properties		*
 *									*
 ************************************************************************/

/*
 *  Start Tag: 1 means the start tag can be omitted
 *  End Tag:   1 means the end tag can be omitted
 *             2 means it's forbidden (empty elements)
 *             3 means the tag is stylistic and should be closed easily
 *  Depr:      this element is deprecated
 *  DTD:       1 means that this element is valid only in the Loose DTD
 *             2 means that this element is valid only in the Frameset DTD
 *
 * Name,Start Tag,End Tag,Save End,Empty,Deprecated,DTD,inline,Description
 */

#define DATA_RCDATA         1
#define DATA_RAWTEXT        2
#define DATA_PLAINTEXT      3
#define DATA_SCRIPT         4
#define DATA_SCRIPT_ESC1    5
#define DATA_SCRIPT_ESC2    6

static const htmlElemDesc
html40ElementTable[] = {
{ "a",		0, 0, 0, 0, 0, 0, 1, "anchor ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "abbr",	0, 0, 0, 0, 0, 0, 1, "abbreviated form",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "acronym",	0, 0, 0, 0, 0, 0, 1, "",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "address",	0, 0, 0, 0, 0, 0, 0, "information on author ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "applet",	0, 0, 0, 0, 1, 1, 2, "java applet ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "area",	0, 2, 2, 1, 0, 0, 0, "client-side image map area ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "b",		0, 3, 0, 0, 0, 0, 1, "bold text style",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "base",	0, 2, 2, 1, 0, 0, 0, "document base uri ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "basefont",	0, 2, 2, 1, 1, 1, 1, "base font size " ,
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "bdo",	0, 0, 0, 0, 0, 0, 1, "i18n bidi over-ride ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "big",	0, 3, 0, 0, 0, 0, 1, "large text style",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "blockquote",	0, 0, 0, 0, 0, 0, 0, "long quotation ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "body",	1, 1, 0, 0, 0, 0, 0, "document body ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "br",		0, 2, 2, 1, 0, 0, 1, "forced line break ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "button",	0, 0, 0, 0, 0, 0, 2, "push button ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "caption",	0, 0, 0, 0, 0, 0, 0, "table caption ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "center",	0, 3, 0, 0, 1, 1, 0, "shorthand for div align=center ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "cite",	0, 0, 0, 0, 0, 0, 1, "citation",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "code",	0, 0, 0, 0, 0, 0, 1, "computer code fragment",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "col",	0, 2, 2, 1, 0, 0, 0, "table column ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "colgroup",	0, 1, 0, 0, 0, 0, 0, "table column group ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "dd",		0, 1, 0, 0, 0, 0, 0, "definition description ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "del",	0, 0, 0, 0, 0, 0, 2, "deleted text ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "dfn",	0, 0, 0, 0, 0, 0, 1, "instance definition",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "dir",	0, 0, 0, 0, 1, 1, 0, "directory list",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "div",	0, 0, 0, 0, 0, 0, 0, "generic language/style container",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "dl",		0, 0, 0, 0, 0, 0, 0, "definition list ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "dt",		0, 1, 0, 0, 0, 0, 0, "definition term ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "em",		0, 3, 0, 0, 0, 0, 1, "emphasis",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "embed",	0, 1, 0, 0, 1, 1, 1, "generic embedded object ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "fieldset",	0, 0, 0, 0, 0, 0, 0, "form control group ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "font",	0, 3, 0, 0, 1, 1, 1, "local change to font ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "form",	0, 0, 0, 0, 0, 0, 0, "interactive form ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "frame",	0, 2, 2, 1, 0, 2, 0, "subwindow " ,
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "frameset",	0, 0, 0, 0, 0, 2, 0, "window subdivision" ,
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "h1",		0, 0, 0, 0, 0, 0, 0, "heading ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "h2",		0, 0, 0, 0, 0, 0, 0, "heading ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "h3",		0, 0, 0, 0, 0, 0, 0, "heading ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "h4",		0, 0, 0, 0, 0, 0, 0, "heading ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "h5",		0, 0, 0, 0, 0, 0, 0, "heading ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "h6",		0, 0, 0, 0, 0, 0, 0, "heading ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "head",	1, 1, 0, 0, 0, 0, 0, "document head ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "hr",		0, 2, 2, 1, 0, 0, 0, "horizontal rule " ,
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "html",	1, 1, 0, 0, 0, 0, 0, "document root element ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "i",		0, 3, 0, 0, 0, 0, 1, "italic text style",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "iframe",	0, 0, 0, 0, 0, 1, 2, "inline subwindow ",
	NULL, NULL, NULL, NULL, NULL,
	DATA_RAWTEXT
},
{ "img",	0, 2, 2, 1, 0, 0, 1, "embedded image ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "input",	0, 2, 2, 1, 0, 0, 1, "form control ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "ins",	0, 0, 0, 0, 0, 0, 2, "inserted text",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "isindex",	0, 2, 2, 1, 1, 1, 0, "single line prompt ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "kbd",	0, 0, 0, 0, 0, 0, 1, "text to be entered by the user",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "label",	0, 0, 0, 0, 0, 0, 1, "form field label text ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "legend",	0, 0, 0, 0, 0, 0, 0, "fieldset legend ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "li",		0, 1, 1, 0, 0, 0, 0, "list item ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "link",	0, 2, 2, 1, 0, 0, 0, "a media-independent link ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "map",	0, 0, 0, 0, 0, 0, 2, "client-side image map ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "menu",	0, 0, 0, 0, 1, 1, 0, "menu list ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "meta",	0, 2, 2, 1, 0, 0, 0, "generic metainformation ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "noembed",	0, 0, 0, 0, 0, 0, 0, "",
	NULL, NULL, NULL, NULL, NULL,
	DATA_RAWTEXT
},
{ "noframes",	0, 0, 0, 0, 0, 2, 0, "alternate content container for non frame-based rendering ",
	NULL, NULL, NULL, NULL, NULL,
	DATA_RAWTEXT
},
{ "noscript",	0, 0, 0, 0, 0, 0, 0, "alternate content container for non script-based rendering ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "object",	0, 0, 0, 0, 0, 0, 2, "generic embedded object ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "ol",		0, 0, 0, 0, 0, 0, 0, "ordered list ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "optgroup",	0, 0, 0, 0, 0, 0, 0, "option group ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "option",	0, 1, 0, 0, 0, 0, 0, "selectable choice " ,
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "p",		0, 1, 0, 0, 0, 0, 0, "paragraph ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "param",	0, 2, 2, 1, 0, 0, 0, "named property value ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "plaintext",	0, 0, 0, 0, 0, 0, 0, "",
	NULL, NULL, NULL, NULL, NULL,
	DATA_PLAINTEXT
},
{ "pre",	0, 0, 0, 0, 0, 0, 0, "preformatted text ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "q",		0, 0, 0, 0, 0, 0, 1, "short inline quotation ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "s",		0, 3, 0, 0, 1, 1, 1, "strike-through text style",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "samp",	0, 0, 0, 0, 0, 0, 1, "sample program output, scripts, etc.",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "script",	0, 0, 0, 0, 0, 0, 2, "script statements ",
	NULL, NULL, NULL, NULL, NULL,
	DATA_SCRIPT
},
{ "select",	0, 0, 0, 0, 0, 0, 1, "option selector ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "small",	0, 3, 0, 0, 0, 0, 1, "small text style",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "span",	0, 0, 0, 0, 0, 0, 1, "generic language/style container ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "strike",	0, 3, 0, 0, 1, 1, 1, "strike-through text",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "strong",	0, 3, 0, 0, 0, 0, 1, "strong emphasis",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "style",	0, 0, 0, 0, 0, 0, 0, "style info ",
	NULL, NULL, NULL, NULL, NULL,
	DATA_RAWTEXT
},
{ "sub",	0, 3, 0, 0, 0, 0, 1, "subscript",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "sup",	0, 3, 0, 0, 0, 0, 1, "superscript ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "table",	0, 0, 0, 0, 0, 0, 0, "",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "tbody",	1, 0, 0, 0, 0, 0, 0, "table body ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "td",		0, 0, 0, 0, 0, 0, 0, "table data cell",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "textarea",	0, 0, 0, 0, 0, 0, 1, "multi-line text field ",
	NULL, NULL, NULL, NULL, NULL,
	DATA_RCDATA
},
{ "tfoot",	0, 1, 0, 0, 0, 0, 0, "table footer ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "th",		0, 1, 0, 0, 0, 0, 0, "table header cell",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "thead",	0, 1, 0, 0, 0, 0, 0, "table header ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "title",	0, 0, 0, 0, 0, 0, 0, "document title ",
	NULL, NULL, NULL, NULL, NULL,
	DATA_RCDATA
},
{ "tr",		0, 0, 0, 0, 0, 0, 0, "table row ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "tt",		0, 3, 0, 0, 0, 0, 1, "teletype or monospaced text style",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "u",		0, 3, 0, 0, 1, 1, 1, "underlined text style",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "ul",		0, 0, 0, 0, 0, 0, 0, "unordered list ",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "var",	0, 0, 0, 0, 0, 0, 1, "instance of a variable or program argument",
	NULL, NULL, NULL, NULL, NULL,
	0
},
{ "xmp",	0, 0, 0, 0, 0, 0, 1, "",
	NULL, NULL, NULL, NULL, NULL,
	DATA_RAWTEXT
}
};

typedef struct {
    const char *oldTag;
    const char *newTag;
} htmlStartCloseEntry;

/*
 * start tags that imply the end of current element
 */
static const htmlStartCloseEntry htmlStartClose[] = {
    { "a", "a" },
    { "a", "fieldset" },
    { "a", "table" },
    { "a", "td" },
    { "a", "th" },
    { "address", "dd" },
    { "address", "dl" },
    { "address", "dt" },
    { "address", "form" },
    { "address", "li" },
    { "address", "ul" },
    { "b", "center" },
    { "b", "p" },
    { "b", "td" },
    { "b", "th" },
    { "big", "p" },
    { "caption", "col" },
    { "caption", "colgroup" },
    { "caption", "tbody" },
    { "caption", "tfoot" },
    { "caption", "thead" },
    { "caption", "tr" },
    { "col", "col" },
    { "col", "colgroup" },
    { "col", "tbody" },
    { "col", "tfoot" },
    { "col", "thead" },
    { "col", "tr" },
    { "colgroup", "colgroup" },
    { "colgroup", "tbody" },
    { "colgroup", "tfoot" },
    { "colgroup", "thead" },
    { "colgroup", "tr" },
    { "dd", "dt" },
    { "dir", "dd" },
    { "dir", "dl" },
    { "dir", "dt" },
    { "dir", "form" },
    { "dir", "ul" },
    { "dl", "form" },
    { "dl", "li" },
    { "dt", "dd" },
    { "dt", "dl" },
    { "font", "center" },
    { "font", "td" },
    { "font", "th" },
    { "form", "form" },
    { "h1", "fieldset" },
    { "h1", "form" },
    { "h1", "li" },
    { "h1", "p" },
    { "h1", "table" },
    { "h2", "fieldset" },
    { "h2", "form" },
    { "h2", "li" },
    { "h2", "p" },
    { "h2", "table" },
    { "h3", "fieldset" },
    { "h3", "form" },
    { "h3", "li" },
    { "h3", "p" },
    { "h3", "table" },
    { "h4", "fieldset" },
    { "h4", "form" },
    { "h4", "li" },
    { "h4", "p" },
    { "h4", "table" },
    { "h5", "fieldset" },
    { "h5", "form" },
    { "h5", "li" },
    { "h5", "p" },
    { "h5", "table" },
    { "h6", "fieldset" },
    { "h6", "form" },
    { "h6", "li" },
    { "h6", "p" },
    { "h6", "table" },
    { "head", "a" },
    { "head", "abbr" },
    { "head", "acronym" },
    { "head", "address" },
    { "head", "b" },
    { "head", "bdo" },
    { "head", "big" },
    { "head", "blockquote" },
    { "head", "body" },
    { "head", "br" },
    { "head", "center" },
    { "head", "cite" },
    { "head", "code" },
    { "head", "dd" },
    { "head", "dfn" },
    { "head", "dir" },
    { "head", "div" },
    { "head", "dl" },
    { "head", "dt" },
    { "head", "em" },
    { "head", "fieldset" },
    { "head", "font" },
    { "head", "form" },
    { "head", "frameset" },
    { "head", "h1" },
    { "head", "h2" },
    { "head", "h3" },
    { "head", "h4" },
    { "head", "h5" },
    { "head", "h6" },
    { "head", "hr" },
    { "head", "i" },
    { "head", "iframe" },
    { "head", "img" },
    { "head", "kbd" },
    { "head", "li" },
    { "head", "listing" },
    { "head", "map" },
    { "head", "menu" },
    { "head", "ol" },
    { "head", "p" },
    { "head", "pre" },
    { "head", "q" },
    { "head", "s" },
    { "head", "samp" },
    { "head", "small" },
    { "head", "span" },
    { "head", "strike" },
    { "head", "strong" },
    { "head", "sub" },
    { "head", "sup" },
    { "head", "table" },
    { "head", "tt" },
    { "head", "u" },
    { "head", "ul" },
    { "head", "var" },
    { "head", "xmp" },
    { "hr", "form" },
    { "i", "center" },
    { "i", "p" },
    { "i", "td" },
    { "i", "th" },
    { "legend", "fieldset" },
    { "li", "li" },
    { "link", "body" },
    { "link", "frameset" },
    { "listing", "dd" },
    { "listing", "dl" },
    { "listing", "dt" },
    { "listing", "fieldset" },
    { "listing", "form" },
    { "listing", "li" },
    { "listing", "table" },
    { "listing", "ul" },
    { "menu", "dd" },
    { "menu", "dl" },
    { "menu", "dt" },
    { "menu", "form" },
    { "menu", "ul" },
    { "ol", "form" },
    { "option", "optgroup" },
    { "option", "option" },
    { "p", "address" },
    { "p", "blockquote" },
    { "p", "body" },
    { "p", "caption" },
    { "p", "center" },
    { "p", "col" },
    { "p", "colgroup" },
    { "p", "dd" },
    { "p", "dir" },
    { "p", "div" },
    { "p", "dl" },
    { "p", "dt" },
    { "p", "fieldset" },
    { "p", "form" },
    { "p", "frameset" },
    { "p", "h1" },
    { "p", "h2" },
    { "p", "h3" },
    { "p", "h4" },
    { "p", "h5" },
    { "p", "h6" },
    { "p", "head" },
    { "p", "hr" },
    { "p", "li" },
    { "p", "listing" },
    { "p", "menu" },
    { "p", "ol" },
    { "p", "p" },
    { "p", "pre" },
    { "p", "table" },
    { "p", "tbody" },
    { "p", "td" },
    { "p", "tfoot" },
    { "p", "th" },
    { "p", "title" },
    { "p", "tr" },
    { "p", "ul" },
    { "p", "xmp" },
    { "pre", "dd" },
    { "pre", "dl" },
    { "pre", "dt" },
    { "pre", "fieldset" },
    { "pre", "form" },
    { "pre", "li" },
    { "pre", "table" },
    { "pre", "ul" },
    { "s", "p" },
    { "script", "noscript" },
    { "small", "p" },
    { "span", "td" },
    { "span", "th" },
    { "strike", "p" },
    { "style", "body" },
    { "style", "frameset" },
    { "tbody", "tbody" },
    { "tbody", "tfoot" },
    { "td", "tbody" },
    { "td", "td" },
    { "td", "tfoot" },
    { "td", "th" },
    { "td", "tr" },
    { "tfoot", "tbody" },
    { "th", "tbody" },
    { "th", "td" },
    { "th", "tfoot" },
    { "th", "th" },
    { "th", "tr" },
    { "thead", "tbody" },
    { "thead", "tfoot" },
    { "title", "body" },
    { "title", "frameset" },
    { "tr", "tbody" },
    { "tr", "tfoot" },
    { "tr", "tr" },
    { "tt", "p" },
    { "u", "p" },
    { "u", "td" },
    { "u", "th" },
    { "ul", "address" },
    { "ul", "form" },
    { "ul", "menu" },
    { "ul", "pre" },
    { "xmp", "dd" },
    { "xmp", "dl" },
    { "xmp", "dt" },
    { "xmp", "fieldset" },
    { "xmp", "form" },
    { "xmp", "li" },
    { "xmp", "table" },
    { "xmp", "ul" }
};

/*
 * The list of HTML attributes which are of content %Script;
 * NOTE: when adding ones, check htmlIsScriptAttribute() since
 *       it assumes the name starts with 'on'
 */
static const char *const htmlScriptAttributes[] = {
    "onclick",
    "ondblclick",
    "onmousedown",
    "onmouseup",
    "onmouseover",
    "onmousemove",
    "onmouseout",
    "onkeypress",
    "onkeydown",
    "onkeyup",
    "onload",
    "onunload",
    "onfocus",
    "onblur",
    "onsubmit",
    "onreset",
    "onchange",
    "onselect"
};

/*
 * This table is used by the htmlparser to know what to do with
 * broken html pages. By assigning different priorities to different
 * elements the parser can decide how to handle extra endtags.
 * Endtags are only allowed to close elements with lower or equal
 * priority.
 */

typedef struct {
    const char *name;
    int priority;
} elementPriority;

static const elementPriority htmlEndPriority[] = {
    {"div",   150},
    {"td",    160},
    {"th",    160},
    {"tr",    170},
    {"thead", 180},
    {"tbody", 180},
    {"tfoot", 180},
    {"table", 190},
    {"head",  200},
    {"body",  200},
    {"html",  220},
    {NULL,    100} /* Default priority */
};

/************************************************************************
 *									*
 *	functions to handle HTML specific data			*
 *									*
 ************************************************************************/

static void
htmlParserFinishElementParsing(htmlParserCtxtPtr ctxt) {
    /*
     * Capture end position and add node
     */
    if ( ctxt->node != NULL && ctxt->record_info ) {
       ctxt->nodeInfo->end_pos = ctxt->input->consumed +
                                (CUR_PTR - ctxt->input->base);
       ctxt->nodeInfo->end_line = ctxt->input->line;
       ctxt->nodeInfo->node = ctxt->node;
       xmlParserAddNodeInfo(ctxt, ctxt->nodeInfo);
       htmlNodeInfoPop(ctxt);
    }
}

/**
 * htmlInitAutoClose:
 *
 * DEPRECATED: This is a no-op.
 */
void
htmlInitAutoClose(void) {
}

static int
htmlCompareTags(const void *key, const void *member) {
    const xmlChar *tag = (const xmlChar *) key;
    const htmlElemDesc *desc = (const htmlElemDesc *) member;

    return(xmlStrcasecmp(tag, BAD_CAST desc->name));
}

/**
 * htmlTagLookup:
 * @tag:  The tag name in lowercase
 *
 * Lookup the HTML tag in the ElementTable
 *
 * Returns the related htmlElemDescPtr or NULL if not found.
 */
const htmlElemDesc *
htmlTagLookup(const xmlChar *tag) {
    if (tag == NULL)
        return(NULL);

    return((const htmlElemDesc *) bsearch(tag, html40ElementTable,
                sizeof(html40ElementTable) / sizeof(htmlElemDesc),
                sizeof(htmlElemDesc), htmlCompareTags));
}

/**
 * htmlGetEndPriority:
 * @name: The name of the element to look up the priority for.
 *
 * Return value: The "endtag" priority.
 **/
static int
htmlGetEndPriority (const xmlChar *name) {
    int i = 0;

    while ((htmlEndPriority[i].name != NULL) &&
	   (!xmlStrEqual((const xmlChar *)htmlEndPriority[i].name, name)))
	i++;

    return(htmlEndPriority[i].priority);
}


static int
htmlCompareStartClose(const void *vkey, const void *member) {
    const htmlStartCloseEntry *key = (const htmlStartCloseEntry *) vkey;
    const htmlStartCloseEntry *entry = (const htmlStartCloseEntry *) member;
    int ret;

    ret = strcmp(key->oldTag, entry->oldTag);
    if (ret == 0)
        ret = strcmp(key->newTag, entry->newTag);

    return(ret);
}

/**
 * htmlCheckAutoClose:
 * @newtag:  The new tag name
 * @oldtag:  The old tag name
 *
 * Checks whether the new tag is one of the registered valid tags for
 * closing old.
 *
 * Returns 0 if no, 1 if yes.
 */
static int
htmlCheckAutoClose(const xmlChar * newtag, const xmlChar * oldtag)
{
    htmlStartCloseEntry key;
    void *res;

    key.oldTag = (const char *) oldtag;
    key.newTag = (const char *) newtag;
    res = bsearch(&key, htmlStartClose,
            sizeof(htmlStartClose) / sizeof(htmlStartCloseEntry),
            sizeof(htmlStartCloseEntry), htmlCompareStartClose);
    return(res != NULL);
}

/**
 * htmlAutoCloseOnClose:
 * @ctxt:  an HTML parser context
 * @newtag:  The new tag name
 * @force:  force the tag closure
 *
 * The HTML DTD allows an ending tag to implicitly close other tags.
 */
static void
htmlAutoCloseOnClose(htmlParserCtxtPtr ctxt, const xmlChar * newtag)
{
    const htmlElemDesc *info;
    int i, priority;

    if (ctxt->options & HTML_PARSE_HTML5)
        return;

    priority = htmlGetEndPriority(newtag);

    for (i = (ctxt->nameNr - 1); i >= 0; i--) {

        if (xmlStrEqual(newtag, ctxt->nameTab[i]))
            break;
        /*
         * A misplaced endtag can only close elements with lower
         * or equal priority, so if we find an element with higher
         * priority before we find an element with
         * matching name, we just ignore this endtag
         */
        if (htmlGetEndPriority(ctxt->nameTab[i]) > priority)
            return;
    }
    if (i < 0)
        return;

    while (!xmlStrEqual(newtag, ctxt->name)) {
        info = htmlTagLookup(ctxt->name);
        if ((info != NULL) && (info->endTag == 3)) {
            htmlParseErr(ctxt, XML_ERR_TAG_NAME_MISMATCH,
	                 "Opening and ending tag mismatch: %s and %s\n",
			 newtag, ctxt->name);
        }
	htmlParserFinishElementParsing(ctxt);
        if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
            ctxt->sax->endElement(ctxt->userData, ctxt->name);
	htmlnamePop(ctxt);
    }
}

/**
 * htmlAutoCloseOnEnd:
 * @ctxt:  an HTML parser context
 *
 * Close all remaining tags at the end of the stream
 */
static void
htmlAutoCloseOnEnd(htmlParserCtxtPtr ctxt)
{
    int i;

    if (ctxt->options & HTML_PARSE_HTML5)
        return;

    if (ctxt->nameNr == 0)
        return;
    for (i = (ctxt->nameNr - 1); i >= 0; i--) {
	htmlParserFinishElementParsing(ctxt);
        if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
            ctxt->sax->endElement(ctxt->userData, ctxt->name);
	htmlnamePop(ctxt);
    }
}

/**
 * htmlAutoClose:
 * @ctxt:  an HTML parser context
 * @newtag:  The new tag name or NULL
 *
 * The HTML DTD allows a tag to implicitly close other tags.
 * The list is kept in htmlStartClose array. This function is
 * called when a new tag has been detected and generates the
 * appropriates closes if possible/needed.
 * If newtag is NULL this mean we are at the end of the resource
 * and we should check
 */
static void
htmlAutoClose(htmlParserCtxtPtr ctxt, const xmlChar * newtag)
{
    if (ctxt->options & HTML_PARSE_HTML5)
        return;

    if (newtag == NULL)
        return;

    while ((ctxt->name != NULL) &&
           (htmlCheckAutoClose(newtag, ctxt->name))) {
	htmlParserFinishElementParsing(ctxt);
        if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
            ctxt->sax->endElement(ctxt->userData, ctxt->name);
	htmlnamePop(ctxt);
    }
}

/**
 * htmlAutoCloseTag:
 * @doc:  the HTML document
 * @name:  The tag name
 * @elem:  the HTML element
 *
 * DEPRECATED: Internal function, don't use.
 *
 * The HTML DTD allows a tag to implicitly close other tags.
 * The list is kept in htmlStartClose array. This function checks
 * if the element or one of it's children would autoclose the
 * given tag.
 *
 * Returns 1 if autoclose, 0 otherwise
 */
int
htmlAutoCloseTag(htmlDocPtr doc, const xmlChar *name, htmlNodePtr elem) {
    htmlNodePtr child;

    if (elem == NULL) return(1);
    if (xmlStrEqual(name, elem->name)) return(0);
    if (htmlCheckAutoClose(elem->name, name)) return(1);
    child = elem->children;
    while (child != NULL) {
        if (htmlAutoCloseTag(doc, name, child)) return(1);
	child = child->next;
    }
    return(0);
}

/**
 * htmlIsAutoClosed:
 * @doc:  the HTML document
 * @elem:  the HTML element
 *
 * DEPRECATED: Internal function, don't use.
 *
 * The HTML DTD allows a tag to implicitly close other tags.
 * The list is kept in htmlStartClose array. This function checks
 * if a tag is autoclosed by one of it's child
 *
 * Returns 1 if autoclosed, 0 otherwise
 */
int
htmlIsAutoClosed(htmlDocPtr doc, htmlNodePtr elem) {
    htmlNodePtr child;

    if (elem == NULL) return(1);
    child = elem->children;
    while (child != NULL) {
	if (htmlAutoCloseTag(doc, elem->name, child)) return(1);
	child = child->next;
    }
    return(0);
}

/**
 * htmlCheckImplied:
 * @ctxt:  an HTML parser context
 * @newtag:  The new tag name
 *
 * The HTML DTD allows a tag to exists only implicitly
 * called when a new tag has been detected and generates the
 * appropriates implicit tags if missing
 */
static void
htmlCheckImplied(htmlParserCtxtPtr ctxt, const xmlChar *newtag) {
    int i;

    if (ctxt->options & (HTML_PARSE_NOIMPLIED | HTML_PARSE_HTML5))
        return;
    if (!htmlOmittedDefaultValue)
	return;
    if (xmlStrEqual(newtag, BAD_CAST"html"))
	return;
    if (ctxt->nameNr <= 0) {
	htmlnamePush(ctxt, BAD_CAST"html");
	if ((ctxt->sax != NULL) && (ctxt->sax->startElement != NULL))
	    ctxt->sax->startElement(ctxt->userData, BAD_CAST"html", NULL);
    }
    if ((xmlStrEqual(newtag, BAD_CAST"body")) || (xmlStrEqual(newtag, BAD_CAST"head")))
        return;
    if ((ctxt->nameNr <= 1) &&
        ((xmlStrEqual(newtag, BAD_CAST"script")) ||
	 (xmlStrEqual(newtag, BAD_CAST"style")) ||
	 (xmlStrEqual(newtag, BAD_CAST"meta")) ||
	 (xmlStrEqual(newtag, BAD_CAST"link")) ||
	 (xmlStrEqual(newtag, BAD_CAST"title")) ||
	 (xmlStrEqual(newtag, BAD_CAST"base")))) {
        if (ctxt->html >= INSERT_IN_HEAD) {
            /* we already saw or generated an <head> before */
            return;
        }
        /*
         * dropped OBJECT ... i you put it first BODY will be
         * assumed !
         */
        htmlnamePush(ctxt, BAD_CAST"head");
        if ((ctxt->sax != NULL) && (ctxt->sax->startElement != NULL))
            ctxt->sax->startElement(ctxt->userData, BAD_CAST"head", NULL);
    } else if ((!xmlStrEqual(newtag, BAD_CAST"noframes")) &&
	       (!xmlStrEqual(newtag, BAD_CAST"frame")) &&
	       (!xmlStrEqual(newtag, BAD_CAST"frameset"))) {
        if (ctxt->html >= INSERT_IN_BODY) {
            /* we already saw or generated a <body> before */
            return;
        }
	for (i = 0;i < ctxt->nameNr;i++) {
	    if (xmlStrEqual(ctxt->nameTab[i], BAD_CAST"body")) {
		return;
	    }
	    if (xmlStrEqual(ctxt->nameTab[i], BAD_CAST"head")) {
		return;
	    }
	}

	htmlnamePush(ctxt, BAD_CAST"body");
	if ((ctxt->sax != NULL) && (ctxt->sax->startElement != NULL))
	    ctxt->sax->startElement(ctxt->userData, BAD_CAST"body", NULL);
    }
}

/**
 * htmlStartCharData
 * @ctxt:  an HTML parser context
 *
 * Prepare for non-whitespace character data.
 */

static void
htmlStartCharData(htmlParserCtxtPtr ctxt) {
    if (ctxt->options & (HTML_PARSE_NOIMPLIED | HTML_PARSE_HTML5))
        return;
    if (!htmlOmittedDefaultValue)
	return;

    if (xmlStrEqual(ctxt->name, BAD_CAST "head"))
        htmlAutoClose(ctxt, BAD_CAST "p");
    htmlCheckImplied(ctxt, BAD_CAST "p");
}

/**
 * htmlIsScriptAttribute:
 * @name:  an attribute name
 *
 * Check if an attribute is of content type Script
 *
 * Returns 1 is the attribute is a script 0 otherwise
 */
int
htmlIsScriptAttribute(const xmlChar *name) {
    unsigned int i;

    if (name == NULL)
      return(0);
    /*
     * all script attributes start with 'on'
     */
    if ((name[0] != 'o') || (name[1] != 'n'))
      return(0);
    for (i = 0;
	 i < sizeof(htmlScriptAttributes)/sizeof(htmlScriptAttributes[0]);
	 i++) {
	if (xmlStrEqual(name, (const xmlChar *) htmlScriptAttributes[i]))
	    return(1);
    }
    return(0);
}

/************************************************************************
 *									*
 *	The list of HTML predefined entities			*
 *									*
 ************************************************************************/


static const htmlEntityDesc  html40EntitiesTable[] = {
/*
 * the 4 absolute ones, plus apostrophe.
 */
{ 34,	"quot",	"quotation mark = APL quote, U+0022 ISOnum" },
{ 38,	"amp",	"ampersand, U+0026 ISOnum" },
{ 39,	"apos",	"single quote" },
{ 60,	"lt",	"less-than sign, U+003C ISOnum" },
{ 62,	"gt",	"greater-than sign, U+003E ISOnum" },

/*
 * A bunch still in the 128-255 range
 * Replacing them depend really on the charset used.
 */
{ 160,	"nbsp",	"no-break space = non-breaking space, U+00A0 ISOnum" },
{ 161,	"iexcl","inverted exclamation mark, U+00A1 ISOnum" },
{ 162,	"cent",	"cent sign, U+00A2 ISOnum" },
{ 163,	"pound","pound sign, U+00A3 ISOnum" },
{ 164,	"curren","currency sign, U+00A4 ISOnum" },
{ 165,	"yen",	"yen sign = yuan sign, U+00A5 ISOnum" },
{ 166,	"brvbar","broken bar = broken vertical bar, U+00A6 ISOnum" },
{ 167,	"sect",	"section sign, U+00A7 ISOnum" },
{ 168,	"uml",	"diaeresis = spacing diaeresis, U+00A8 ISOdia" },
{ 169,	"copy",	"copyright sign, U+00A9 ISOnum" },
{ 170,	"ordf",	"feminine ordinal indicator, U+00AA ISOnum" },
{ 171,	"laquo","left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum" },
{ 172,	"not",	"not sign, U+00AC ISOnum" },
{ 173,	"shy",	"soft hyphen = discretionary hyphen, U+00AD ISOnum" },
{ 174,	"reg",	"registered sign = registered trade mark sign, U+00AE ISOnum" },
{ 175,	"macr",	"macron = spacing macron = overline = APL overbar, U+00AF ISOdia" },
{ 176,	"deg",	"degree sign, U+00B0 ISOnum" },
{ 177,	"plusmn","plus-minus sign = plus-or-minus sign, U+00B1 ISOnum" },
{ 178,	"sup2",	"superscript two = superscript digit two = squared, U+00B2 ISOnum" },
{ 179,	"sup3",	"superscript three = superscript digit three = cubed, U+00B3 ISOnum" },
{ 180,	"acute","acute accent = spacing acute, U+00B4 ISOdia" },
{ 181,	"micro","micro sign, U+00B5 ISOnum" },
{ 182,	"para",	"pilcrow sign = paragraph sign, U+00B6 ISOnum" },
{ 183,	"middot","middle dot = Georgian comma Greek middle dot, U+00B7 ISOnum" },
{ 184,	"cedil","cedilla = spacing cedilla, U+00B8 ISOdia" },
{ 185,	"sup1",	"superscript one = superscript digit one, U+00B9 ISOnum" },
{ 186,	"ordm",	"masculine ordinal indicator, U+00BA ISOnum" },
{ 187,	"raquo","right-pointing double angle quotation mark right pointing guillemet, U+00BB ISOnum" },
{ 188,	"frac14","vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum" },
{ 189,	"frac12","vulgar fraction one half = fraction one half, U+00BD ISOnum" },
{ 190,	"frac34","vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum" },
{ 191,	"iquest","inverted question mark = turned question mark, U+00BF ISOnum" },
{ 192,	"Agrave","latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1" },
{ 193,	"Aacute","latin capital letter A with acute, U+00C1 ISOlat1" },
{ 194,	"Acirc","latin capital letter A with circumflex, U+00C2 ISOlat1" },
{ 195,	"Atilde","latin capital letter A with tilde, U+00C3 ISOlat1" },
{ 196,	"Auml",	"latin capital letter A with diaeresis, U+00C4 ISOlat1" },
{ 197,	"Aring","latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1" },
{ 198,	"AElig","latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1" },
{ 199,	"Ccedil","latin capital letter C with cedilla, U+00C7 ISOlat1" },
{ 200,	"Egrave","latin capital letter E with grave, U+00C8 ISOlat1" },
{ 201,	"Eacute","latin capital letter E with acute, U+00C9 ISOlat1" },
{ 202,	"Ecirc","latin capital letter E with circumflex, U+00CA ISOlat1" },
{ 203,	"Euml",	"latin capital letter E with diaeresis, U+00CB ISOlat1" },
{ 204,	"Igrave","latin capital letter I with grave, U+00CC ISOlat1" },
{ 205,	"Iacute","latin capital letter I with acute, U+00CD ISOlat1" },
{ 206,	"Icirc","latin capital letter I with circumflex, U+00CE ISOlat1" },
{ 207,	"Iuml",	"latin capital letter I with diaeresis, U+00CF ISOlat1" },
{ 208,	"ETH",	"latin capital letter ETH, U+00D0 ISOlat1" },
{ 209,	"Ntilde","latin capital letter N with tilde, U+00D1 ISOlat1" },
{ 210,	"Ograve","latin capital letter O with grave, U+00D2 ISOlat1" },
{ 211,	"Oacute","latin capital letter O with acute, U+00D3 ISOlat1" },
{ 212,	"Ocirc","latin capital letter O with circumflex, U+00D4 ISOlat1" },
{ 213,	"Otilde","latin capital letter O with tilde, U+00D5 ISOlat1" },
{ 214,	"Ouml",	"latin capital letter O with diaeresis, U+00D6 ISOlat1" },
{ 215,	"times","multiplication sign, U+00D7 ISOnum" },
{ 216,	"Oslash","latin capital letter O with stroke latin capital letter O slash, U+00D8 ISOlat1" },
{ 217,	"Ugrave","latin capital letter U with grave, U+00D9 ISOlat1" },
{ 218,	"Uacute","latin capital letter U with acute, U+00DA ISOlat1" },
{ 219,	"Ucirc","latin capital letter U with circumflex, U+00DB ISOlat1" },
{ 220,	"Uuml",	"latin capital letter U with diaeresis, U+00DC ISOlat1" },
{ 221,	"Yacute","latin capital letter Y with acute, U+00DD ISOlat1" },
{ 222,	"THORN","latin capital letter THORN, U+00DE ISOlat1" },
{ 223,	"szlig","latin small letter sharp s = ess-zed, U+00DF ISOlat1" },
{ 224,	"agrave","latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1" },
{ 225,	"aacute","latin small letter a with acute, U+00E1 ISOlat1" },
{ 226,	"acirc","latin small letter a with circumflex, U+00E2 ISOlat1" },
{ 227,	"atilde","latin small letter a with tilde, U+00E3 ISOlat1" },
{ 228,	"auml",	"latin small letter a with diaeresis, U+00E4 ISOlat1" },
{ 229,	"aring","latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1" },
{ 230,	"aelig","latin small letter ae = latin small ligature ae, U+00E6 ISOlat1" },
{ 231,	"ccedil","latin small letter c with cedilla, U+00E7 ISOlat1" },
{ 232,	"egrave","latin small letter e with grave, U+00E8 ISOlat1" },
{ 233,	"eacute","latin small letter e with acute, U+00E9 ISOlat1" },
{ 234,	"ecirc","latin small letter e with circumflex, U+00EA ISOlat1" },
{ 235,	"euml",	"latin small letter e with diaeresis, U+00EB ISOlat1" },
{ 236,	"igrave","latin small letter i with grave, U+00EC ISOlat1" },
{ 237,	"iacute","latin small letter i with acute, U+00ED ISOlat1" },
{ 238,	"icirc","latin small letter i with circumflex, U+00EE ISOlat1" },
{ 239,	"iuml",	"latin small letter i with diaeresis, U+00EF ISOlat1" },
{ 240,	"eth",	"latin small letter eth, U+00F0 ISOlat1" },
{ 241,	"ntilde","latin small letter n with tilde, U+00F1 ISOlat1" },
{ 242,	"ograve","latin small letter o with grave, U+00F2 ISOlat1" },
{ 243,	"oacute","latin small letter o with acute, U+00F3 ISOlat1" },
{ 244,	"ocirc","latin small letter o with circumflex, U+00F4 ISOlat1" },
{ 245,	"otilde","latin small letter o with tilde, U+00F5 ISOlat1" },
{ 246,	"ouml",	"latin small letter o with diaeresis, U+00F6 ISOlat1" },
{ 247,	"divide","division sign, U+00F7 ISOnum" },
{ 248,	"oslash","latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1" },
{ 249,	"ugrave","latin small letter u with grave, U+00F9 ISOlat1" },
{ 250,	"uacute","latin small letter u with acute, U+00FA ISOlat1" },
{ 251,	"ucirc","latin small letter u with circumflex, U+00FB ISOlat1" },
{ 252,	"uuml",	"latin small letter u with diaeresis, U+00FC ISOlat1" },
{ 253,	"yacute","latin small letter y with acute, U+00FD ISOlat1" },
{ 254,	"thorn","latin small letter thorn with, U+00FE ISOlat1" },
{ 255,	"yuml",	"latin small letter y with diaeresis, U+00FF ISOlat1" },

{ 338,	"OElig","latin capital ligature OE, U+0152 ISOlat2" },
{ 339,	"oelig","latin small ligature oe, U+0153 ISOlat2" },
{ 352,	"Scaron","latin capital letter S with caron, U+0160 ISOlat2" },
{ 353,	"scaron","latin small letter s with caron, U+0161 ISOlat2" },
{ 376,	"Yuml",	"latin capital letter Y with diaeresis, U+0178 ISOlat2" },

/*
 * Anything below should really be kept as entities references
 */
{ 402,	"fnof",	"latin small f with hook = function = florin, U+0192 ISOtech" },

{ 710,	"circ",	"modifier letter circumflex accent, U+02C6 ISOpub" },
{ 732,	"tilde","small tilde, U+02DC ISOdia" },

{ 913,	"Alpha","greek capital letter alpha, U+0391" },
{ 914,	"Beta",	"greek capital letter beta, U+0392" },
{ 915,	"Gamma","greek capital letter gamma, U+0393 ISOgrk3" },
{ 916,	"Delta","greek capital letter delta, U+0394 ISOgrk3" },
{ 917,	"Epsilon","greek capital letter epsilon, U+0395" },
{ 918,	"Zeta",	"greek capital letter zeta, U+0396" },
{ 919,	"Eta",	"greek capital letter eta, U+0397" },
{ 920,	"Theta","greek capital letter theta, U+0398 ISOgrk3" },
{ 921,	"Iota",	"greek capital letter iota, U+0399" },
{ 922,	"Kappa","greek capital letter kappa, U+039A" },
{ 923,	"Lambda", "greek capital letter lambda, U+039B ISOgrk3" },
{ 924,	"Mu",	"greek capital letter mu, U+039C" },
{ 925,	"Nu",	"greek capital letter nu, U+039D" },
{ 926,	"Xi",	"greek capital letter xi, U+039E ISOgrk3" },
{ 927,	"Omicron","greek capital letter omicron, U+039F" },
{ 928,	"Pi",	"greek capital letter pi, U+03A0 ISOgrk3" },
{ 929,	"Rho",	"greek capital letter rho, U+03A1" },
{ 931,	"Sigma","greek capital letter sigma, U+03A3 ISOgrk3" },
{ 932,	"Tau",	"greek capital letter tau, U+03A4" },
{ 933,	"Upsilon","greek capital letter upsilon, U+03A5 ISOgrk3" },
{ 934,	"Phi",	"greek capital letter phi, U+03A6 ISOgrk3" },
{ 935,	"Chi",	"greek capital letter chi, U+03A7" },
{ 936,	"Psi",	"greek capital letter psi, U+03A8 ISOgrk3" },
{ 937,	"Omega","greek capital letter omega, U+03A9 ISOgrk3" },

{ 945,	"alpha","greek small letter alpha, U+03B1 ISOgrk3" },
{ 946,	"beta",	"greek small letter beta, U+03B2 ISOgrk3" },
{ 947,	"gamma","greek small letter gamma, U+03B3 ISOgrk3" },
{ 948,	"delta","greek small letter delta, U+03B4 ISOgrk3" },
{ 949,	"epsilon","greek small letter epsilon, U+03B5 ISOgrk3" },
{ 950,	"zeta",	"greek small letter zeta, U+03B6 ISOgrk3" },
{ 951,	"eta",	"greek small letter eta, U+03B7 ISOgrk3" },
{ 952,	"theta","greek small letter theta, U+03B8 ISOgrk3" },
{ 953,	"iota",	"greek small letter iota, U+03B9 ISOgrk3" },
{ 954,	"kappa","greek small letter kappa, U+03BA ISOgrk3" },
{ 955,	"lambda","greek small letter lambda, U+03BB ISOgrk3" },
{ 956,	"mu",	"greek small letter mu, U+03BC ISOgrk3" },
{ 957,	"nu",	"greek small letter nu, U+03BD ISOgrk3" },
{ 958,	"xi",	"greek small letter xi, U+03BE ISOgrk3" },
{ 959,	"omicron","greek small letter omicron, U+03BF NEW" },
{ 960,	"pi",	"greek small letter pi, U+03C0 ISOgrk3" },
{ 961,	"rho",	"greek small letter rho, U+03C1 ISOgrk3" },
{ 962,	"sigmaf","greek small letter final sigma, U+03C2 ISOgrk3" },
{ 963,	"sigma","greek small letter sigma, U+03C3 ISOgrk3" },
{ 964,	"tau",	"greek small letter tau, U+03C4 ISOgrk3" },
{ 965,	"upsilon","greek small letter upsilon, U+03C5 ISOgrk3" },
{ 966,	"phi",	"greek small letter phi, U+03C6 ISOgrk3" },
{ 967,	"chi",	"greek small letter chi, U+03C7 ISOgrk3" },
{ 968,	"psi",	"greek small letter psi, U+03C8 ISOgrk3" },
{ 969,	"omega","greek small letter omega, U+03C9 ISOgrk3" },
{ 977,	"thetasym","greek small letter theta symbol, U+03D1 NEW" },
{ 978,	"upsih","greek upsilon with hook symbol, U+03D2 NEW" },
{ 982,	"piv",	"greek pi symbol, U+03D6 ISOgrk3" },

{ 8194,	"ensp",	"en space, U+2002 ISOpub" },
{ 8195,	"emsp",	"em space, U+2003 ISOpub" },
{ 8201,	"thinsp","thin space, U+2009 ISOpub" },
{ 8204,	"zwnj",	"zero width non-joiner, U+200C NEW RFC 2070" },
{ 8205,	"zwj",	"zero width joiner, U+200D NEW RFC 2070" },
{ 8206,	"lrm",	"left-to-right mark, U+200E NEW RFC 2070" },
{ 8207,	"rlm",	"right-to-left mark, U+200F NEW RFC 2070" },
{ 8211,	"ndash","en dash, U+2013 ISOpub" },
{ 8212,	"mdash","em dash, U+2014 ISOpub" },
{ 8216,	"lsquo","left single quotation mark, U+2018 ISOnum" },
{ 8217,	"rsquo","right single quotation mark, U+2019 ISOnum" },
{ 8218,	"sbquo","single low-9 quotation mark, U+201A NEW" },
{ 8220,	"ldquo","left double quotation mark, U+201C ISOnum" },
{ 8221,	"rdquo","right double quotation mark, U+201D ISOnum" },
{ 8222,	"bdquo","double low-9 quotation mark, U+201E NEW" },
{ 8224,	"dagger","dagger, U+2020 ISOpub" },
{ 8225,	"Dagger","double dagger, U+2021 ISOpub" },

{ 8226,	"bull",	"bullet = black small circle, U+2022 ISOpub" },
{ 8230,	"hellip","horizontal ellipsis = three dot leader, U+2026 ISOpub" },

{ 8240,	"permil","per mille sign, U+2030 ISOtech" },

{ 8242,	"prime","prime = minutes = feet, U+2032 ISOtech" },
{ 8243,	"Prime","double prime = seconds = inches, U+2033 ISOtech" },

{ 8249,	"lsaquo","single left-pointing angle quotation mark, U+2039 ISO proposed" },
{ 8250,	"rsaquo","single right-pointing angle quotation mark, U+203A ISO proposed" },

{ 8254,	"oline","overline = spacing overscore, U+203E NEW" },
{ 8260,	"frasl","fraction slash, U+2044 NEW" },

{ 8364,	"euro",	"euro sign, U+20AC NEW" },

{ 8465,	"image","blackletter capital I = imaginary part, U+2111 ISOamso" },
{ 8472,	"weierp","script capital P = power set = Weierstrass p, U+2118 ISOamso" },
{ 8476,	"real",	"blackletter capital R = real part symbol, U+211C ISOamso" },
{ 8482,	"trade","trade mark sign, U+2122 ISOnum" },
{ 8501,	"alefsym","alef symbol = first transfinite cardinal, U+2135 NEW" },
{ 8592,	"larr",	"leftwards arrow, U+2190 ISOnum" },
{ 8593,	"uarr",	"upwards arrow, U+2191 ISOnum" },
{ 8594,	"rarr",	"rightwards arrow, U+2192 ISOnum" },
{ 8595,	"darr",	"downwards arrow, U+2193 ISOnum" },
{ 8596,	"harr",	"left right arrow, U+2194 ISOamsa" },
{ 8629,	"crarr","downwards arrow with corner leftwards = carriage return, U+21B5 NEW" },
{ 8656,	"lArr",	"leftwards double arrow, U+21D0 ISOtech" },
{ 8657,	"uArr",	"upwards double arrow, U+21D1 ISOamsa" },
{ 8658,	"rArr",	"rightwards double arrow, U+21D2 ISOtech" },
{ 8659,	"dArr",	"downwards double arrow, U+21D3 ISOamsa" },
{ 8660,	"hArr",	"left right double arrow, U+21D4 ISOamsa" },

{ 8704,	"forall","for all, U+2200 ISOtech" },
{ 8706,	"part",	"partial differential, U+2202 ISOtech" },
{ 8707,	"exist","there exists, U+2203 ISOtech" },
{ 8709,	"empty","empty set = null set = diameter, U+2205 ISOamso" },
{ 8711,	"nabla","nabla = backward difference, U+2207 ISOtech" },
{ 8712,	"isin",	"element of, U+2208 ISOtech" },
{ 8713,	"notin","not an element of, U+2209 ISOtech" },
{ 8715,	"ni",	"contains as member, U+220B ISOtech" },
{ 8719,	"prod",	"n-ary product = product sign, U+220F ISOamsb" },
{ 8721,	"sum",	"n-ary summation, U+2211 ISOamsb" },
{ 8722,	"minus","minus sign, U+2212 ISOtech" },
{ 8727,	"lowast","asterisk operator, U+2217 ISOtech" },
{ 8730,	"radic","square root = radical sign, U+221A ISOtech" },
{ 8733,	"prop",	"proportional to, U+221D ISOtech" },
{ 8734,	"infin","infinity, U+221E ISOtech" },
{ 8736,	"ang",	"angle, U+2220 ISOamso" },
{ 8743,	"and",	"logical and = wedge, U+2227 ISOtech" },
{ 8744,	"or",	"logical or = vee, U+2228 ISOtech" },
{ 8745,	"cap",	"intersection = cap, U+2229 ISOtech" },
{ 8746,	"cup",	"union = cup, U+222A ISOtech" },
{ 8747,	"int",	"integral, U+222B ISOtech" },
{ 8756,	"there4","therefore, U+2234 ISOtech" },
{ 8764,	"sim",	"tilde operator = varies with = similar to, U+223C ISOtech" },
{ 8773,	"cong",	"approximately equal to, U+2245 ISOtech" },
{ 8776,	"asymp","almost equal to = asymptotic to, U+2248 ISOamsr" },
{ 8800,	"ne",	"not equal to, U+2260 ISOtech" },
{ 8801,	"equiv","identical to, U+2261 ISOtech" },
{ 8804,	"le",	"less-than or equal to, U+2264 ISOtech" },
{ 8805,	"ge",	"greater-than or equal to, U+2265 ISOtech" },
{ 8834,	"sub",	"subset of, U+2282 ISOtech" },
{ 8835,	"sup",	"superset of, U+2283 ISOtech" },
{ 8836,	"nsub",	"not a subset of, U+2284 ISOamsn" },
{ 8838,	"sube",	"subset of or equal to, U+2286 ISOtech" },
{ 8839,	"supe",	"superset of or equal to, U+2287 ISOtech" },
{ 8853,	"oplus","circled plus = direct sum, U+2295 ISOamsb" },
{ 8855,	"otimes","circled times = vector product, U+2297 ISOamsb" },
{ 8869,	"perp",	"up tack = orthogonal to = perpendicular, U+22A5 ISOtech" },
{ 8901,	"sdot",	"dot operator, U+22C5 ISOamsb" },
{ 8968,	"lceil","left ceiling = apl upstile, U+2308 ISOamsc" },
{ 8969,	"rceil","right ceiling, U+2309 ISOamsc" },
{ 8970,	"lfloor","left floor = apl downstile, U+230A ISOamsc" },
{ 8971,	"rfloor","right floor, U+230B ISOamsc" },
{ 9001,	"lang",	"left-pointing angle bracket = bra, U+2329 ISOtech" },
{ 9002,	"rang",	"right-pointing angle bracket = ket, U+232A ISOtech" },
{ 9674,	"loz",	"lozenge, U+25CA ISOpub" },

{ 9824,	"spades","black spade suit, U+2660 ISOpub" },
{ 9827,	"clubs","black club suit = shamrock, U+2663 ISOpub" },
{ 9829,	"hearts","black heart suit = valentine, U+2665 ISOpub" },
{ 9830,	"diams","black diamond suit, U+2666 ISOpub" },

};

/************************************************************************
 *									*
 *		Commodity functions to handle entities			*
 *									*
 ************************************************************************/

/**
 * htmlEntityLookup:
 * @name: the entity name
 *
 * Lookup the given entity in EntitiesTable
 *
 * TODO: the linear scan is really ugly, an hash table is really needed.
 *
 * Returns the associated htmlEntityDescPtr if found, NULL otherwise.
 */
const htmlEntityDesc *
htmlEntityLookup(const xmlChar *name) {
    unsigned int i;

    for (i = 0;i < (sizeof(html40EntitiesTable)/
                    sizeof(html40EntitiesTable[0]));i++) {
        if (xmlStrEqual(name, BAD_CAST html40EntitiesTable[i].name)) {
            return((htmlEntityDescPtr) &html40EntitiesTable[i]);
	}
    }
    return(NULL);
}

static int
htmlCompareEntityDesc(const void *vkey, const void *vdesc) {
    const unsigned *key = vkey;
    const htmlEntityDesc *desc = vdesc;

    return((int) *key - (int) desc->value);
}

/**
 * htmlEntityValueLookup:
 * @value: the entity's unicode value
 *
 * Lookup the given entity in EntitiesTable
 *
 * TODO: the linear scan is really ugly, an hash table is really needed.
 *
 * Returns the associated htmlEntityDescPtr if found, NULL otherwise.
 */
const htmlEntityDesc *
htmlEntityValueLookup(unsigned int value) {
    const htmlEntityDesc *desc;
    size_t nmemb;

    nmemb = sizeof(html40EntitiesTable) / sizeof(html40EntitiesTable[0]);
    desc = bsearch(&value, html40EntitiesTable, nmemb, sizeof(htmlEntityDesc),
                   htmlCompareEntityDesc);

    return(desc);
}

/**
 * htmlUTF8ToHtml:
 * @out:  a pointer to an array of bytes to store the result
 * @outlen:  the length of @out
 * @in:  a pointer to an array of UTF-8 chars
 * @inlen:  the length of @in
 *
 * Take a block of UTF-8 chars in and try to convert it to an ASCII
 * plus HTML entities block of chars out.
 *
 * Returns 0 if success, -2 if the transcoding fails, or -1 otherwise
 * The value of @inlen after return is the number of octets consumed
 *     as the return value is positive, else unpredictable.
 * The value of @outlen after return is the number of octets consumed.
 */
int
htmlUTF8ToHtml(unsigned char* out, int *outlen,
               const unsigned char* in, int *inlen) {
    const unsigned char* instart = in;
    const unsigned char* inend;
    unsigned char* outstart = out;
    unsigned char* outend;
    int ret = XML_ENC_ERR_SPACE;

    if ((out == NULL) || (outlen == NULL) || (inlen == NULL))
        return(XML_ENC_ERR_INTERNAL);

    if (in == NULL) {
        /*
	 * initialization nothing to do
	 */
	*outlen = 0;
	*inlen = 0;
	return(XML_ENC_ERR_SUCCESS);
    }

    inend = in + *inlen;
    outend = out + *outlen;
    while (in < inend) {
        const htmlEntityDesc *ent;
        const char *cp;
        char nbuf[16];
        unsigned c, d;
        int seqlen, len, i;

	d = *in;

	if (d < 0x80) {
            if (out >= outend)
                goto done;
            *out++ = d;
            in += 1;
            continue;
        }

        if (d < 0xE0)      { c = d & 0x1F; seqlen = 2; }
        else if (d < 0xF0) { c = d & 0x0F; seqlen = 3; }
        else               { c = d & 0x07; seqlen = 4; }

	if (inend - in < seqlen)
	    break;

	for (i = 1; i < seqlen; i++) {
	    d = in[i];
	    c <<= 6;
	    c |= d & 0x3F;
	}

        /*
         * Try to lookup a predefined HTML entity for it
         */
        ent = htmlEntityValueLookup(c);

        if (ent == NULL) {
          snprintf(nbuf, sizeof(nbuf), "#%u", c);
          cp = nbuf;
        } else {
          cp = ent->name;
        }

        len = strlen(cp);
        if (outend - out < len + 2)
            goto done;

        *out++ = '&';
        memcpy(out, cp, len);
        out += len;
        *out++ = ';';

        in += seqlen;
    }

    ret = out - outstart;

done:
    *outlen = out - outstart;
    *inlen = in - instart;
    return(ret);
}

/**
 * htmlEncodeEntities:
 * @out:  a pointer to an array of bytes to store the result
 * @outlen:  the length of @out
 * @in:  a pointer to an array of UTF-8 chars
 * @inlen:  the length of @in
 * @quoteChar: the quote character to escape (' or ") or zero.
 *
 * Take a block of UTF-8 chars in and try to convert it to an ASCII
 * plus HTML entities block of chars out.
 *
 * Returns 0 if success, -2 if the transcoding fails, or -1 otherwise
 * The value of @inlen after return is the number of octets consumed
 *     as the return value is positive, else unpredictable.
 * The value of @outlen after return is the number of octets consumed.
 */
int
htmlEncodeEntities(unsigned char* out, int *outlen,
		   const unsigned char* in, int *inlen, int quoteChar) {
    const unsigned char* processed = in;
    const unsigned char* outend;
    const unsigned char* outstart = out;
    const unsigned char* instart = in;
    const unsigned char* inend;
    unsigned int c, d;
    int trailing;

    if ((out == NULL) || (outlen == NULL) || (inlen == NULL) || (in == NULL))
        return(-1);
    outend = out + (*outlen);
    inend = in + (*inlen);
    while (in < inend) {
	d = *in++;
	if      (d < 0x80)  { c= d; trailing= 0; }
	else if (d < 0xC0) {
	    /* trailing byte in leading position */
	    *outlen = out - outstart;
	    *inlen = processed - instart;
	    return(-2);
        } else if (d < 0xE0)  { c= d & 0x1F; trailing= 1; }
        else if (d < 0xF0)  { c= d & 0x0F; trailing= 2; }
        else if (d < 0xF8)  { c= d & 0x07; trailing= 3; }
	else {
	    /* no chance for this in Ascii */
	    *outlen = out - outstart;
	    *inlen = processed - instart;
	    return(-2);
	}

	if (inend - in < trailing)
	    break;

	while (trailing--) {
	    if (((d= *in++) & 0xC0) != 0x80) {
		*outlen = out - outstart;
		*inlen = processed - instart;
		return(-2);
	    }
	    c <<= 6;
	    c |= d & 0x3F;
	}

	/* assertion: c is a single UTF-4 value */
	if ((c < 0x80) && (c != (unsigned int) quoteChar) &&
	    (c != '&') && (c != '<') && (c != '>')) {
	    if (out >= outend)
		break;
	    *out++ = c;
	} else {
	    const htmlEntityDesc * ent;
	    const char *cp;
	    char nbuf[16];
	    int len;

	    /*
	     * Try to lookup a predefined HTML entity for it
	     */
	    ent = htmlEntityValueLookup(c);
	    if (ent == NULL) {
		snprintf(nbuf, sizeof(nbuf), "#%u", c);
		cp = nbuf;
	    }
	    else
		cp = ent->name;
	    len = strlen(cp);
	    if (outend - out < len + 2)
		break;
	    *out++ = '&';
	    memcpy(out, cp, len);
	    out += len;
	    *out++ = ';';
	}
	processed = in;
    }
    *outlen = out - outstart;
    *inlen = processed - instart;
    return(0);
}

/************************************************************************
 *									*
 *		Commodity functions, cleanup needed ?			*
 *									*
 ************************************************************************/
/*
 * all tags allowing pc data from the html 4.01 loose dtd
 * NOTE: it might be more appropriate to integrate this information
 * into the html40ElementTable array but I don't want to risk any
 * binary incompatibility
 */
static const char *allowPCData[] = {
    "a", "abbr", "acronym", "address", "applet", "b", "bdo", "big",
    "blockquote", "body", "button", "caption", "center", "cite", "code",
    "dd", "del", "dfn", "div", "dt", "em", "font", "form", "h1", "h2",
    "h3", "h4", "h5", "h6", "i", "iframe", "ins", "kbd", "label", "legend",
    "li", "noframes", "noscript", "object", "p", "pre", "q", "s", "samp",
    "small", "span", "strike", "strong", "td", "th", "tt", "u", "var"
};

/**
 * areBlanks:
 * @ctxt:  an HTML parser context
 * @str:  a xmlChar *
 * @len:  the size of @str
 *
 * Is this a sequence of blank chars that one can ignore ?
 *
 * Returns 1 if ignorable 0 if whitespace, -1 otherwise.
 */

static int areBlanks(htmlParserCtxtPtr ctxt, const xmlChar *str, int len) {
    unsigned int i;
    int j;
    xmlNodePtr lastChild;
    xmlDtdPtr dtd;

    for (j = 0;j < len;j++)
        if (!(IS_WS_HTML(str[j]))) return(-1);

    if (CUR == 0) return(1);
    if (CUR != '<') return(0);
    if (ctxt->name == NULL)
	return(1);
    if (xmlStrEqual(ctxt->name, BAD_CAST"html"))
	return(1);
    if (xmlStrEqual(ctxt->name, BAD_CAST"head"))
	return(1);

    /* Only strip CDATA children of the body tag for strict HTML DTDs */
    if (xmlStrEqual(ctxt->name, BAD_CAST "body") && ctxt->myDoc != NULL) {
        dtd = xmlGetIntSubset(ctxt->myDoc);
        if (dtd != NULL && dtd->ExternalID != NULL) {
            if (!xmlStrcasecmp(dtd->ExternalID, BAD_CAST "-//W3C//DTD HTML 4.01//EN") ||
                    !xmlStrcasecmp(dtd->ExternalID, BAD_CAST "-//W3C//DTD HTML 4//EN"))
                return(1);
        }
    }

    if (ctxt->node == NULL) return(0);
    lastChild = xmlGetLastChild(ctxt->node);
    while ((lastChild) && (lastChild->type == XML_COMMENT_NODE))
	lastChild = lastChild->prev;
    if (lastChild == NULL) {
        if ((ctxt->node->type != XML_ELEMENT_NODE) &&
            (ctxt->node->content != NULL)) return(0);
	/* keep ws in constructs like ...<b> </b>...
	   for all tags "b" allowing PCDATA */
	for ( i = 0; i < sizeof(allowPCData)/sizeof(allowPCData[0]); i++ ) {
	    if ( xmlStrEqual(ctxt->name, BAD_CAST allowPCData[i]) ) {
		return(0);
	    }
	}
    } else if (xmlNodeIsText(lastChild)) {
        return(0);
    } else {
	/* keep ws in constructs like <p><b>xy</b> <i>z</i><p>
	   for all tags "p" allowing PCDATA */
	for ( i = 0; i < sizeof(allowPCData)/sizeof(allowPCData[0]); i++ ) {
	    if ( xmlStrEqual(lastChild->name, BAD_CAST allowPCData[i]) ) {
		return(0);
	    }
	}
    }
    return(1);
}

/**
 * htmlNewDocNoDtD:
 * @URI:  URI for the dtd, or NULL
 * @ExternalID:  the external ID of the DTD, or NULL
 *
 * Creates a new HTML document without a DTD node if @URI and @ExternalID
 * are NULL
 *
 * Returns a new document, do not initialize the DTD if not provided
 */
htmlDocPtr
htmlNewDocNoDtD(const xmlChar *URI, const xmlChar *ExternalID) {
    xmlDocPtr cur;

    /*
     * Allocate a new document and fill the fields.
     */
    cur = (xmlDocPtr) xmlMalloc(sizeof(xmlDoc));
    if (cur == NULL)
	return(NULL);
    memset(cur, 0, sizeof(xmlDoc));

    cur->type = XML_HTML_DOCUMENT_NODE;
    cur->version = NULL;
    cur->intSubset = NULL;
    cur->doc = cur;
    cur->name = NULL;
    cur->children = NULL;
    cur->extSubset = NULL;
    cur->oldNs = NULL;
    cur->encoding = NULL;
    cur->standalone = 1;
    cur->compression = 0;
    cur->ids = NULL;
    cur->refs = NULL;
    cur->_private = NULL;
    cur->charset = XML_CHAR_ENCODING_UTF8;
    cur->properties = XML_DOC_HTML | XML_DOC_USERBUILT;
    if ((ExternalID != NULL) ||
	(URI != NULL)) {
        xmlDtdPtr intSubset;

	intSubset = xmlCreateIntSubset(cur, BAD_CAST "html", ExternalID, URI);
        if (intSubset == NULL) {
            xmlFree(cur);
            return(NULL);
        }
    }
    if ((xmlRegisterCallbacks) && (xmlRegisterNodeDefaultValue))
	xmlRegisterNodeDefaultValue((xmlNodePtr)cur);
    return(cur);
}

/**
 * htmlNewDoc:
 * @URI:  URI for the dtd, or NULL
 * @ExternalID:  the external ID of the DTD, or NULL
 *
 * Creates a new HTML document
 *
 * Returns a new document
 */
htmlDocPtr
htmlNewDoc(const xmlChar *URI, const xmlChar *ExternalID) {
    if ((URI == NULL) && (ExternalID == NULL))
	return(htmlNewDocNoDtD(
		    BAD_CAST "http://www.w3.org/TR/REC-html40/loose.dtd",
		    BAD_CAST "-//W3C//DTD HTML 4.0 Transitional//EN"));

    return(htmlNewDocNoDtD(URI, ExternalID));
}


/************************************************************************
 *									*
 *			The parser itself				*
 *	Relates to http://www.w3.org/TR/html40				*
 *									*
 ************************************************************************/

/************************************************************************
 *									*
 *			The parser itself				*
 *									*
 ************************************************************************/

/**
 * htmlParseHTMLName:
 * @ctxt:  an HTML parser context
 *
 * parse an HTML tag or attribute name, note that we convert it to lowercase
 * since HTML names are not case-sensitive.
 *
 * Returns the Tag Name parsed or NULL
 */

static xmlHashedString
htmlParseHTMLName(htmlParserCtxtPtr ctxt, int attr) {
    xmlHashedString ret;
    xmlChar buf[HTML_PARSER_BUFFER_SIZE];
    const xmlChar *in;
    size_t avail;
    int eof = PARSER_PROGRESSIVE(ctxt);
    int nbchar = 0;
    int stop = attr ? '=' : ' ';

    in = ctxt->input->cur;
    avail = ctxt->input->end - in;

    while (1) {
        int c, size;

        if ((!eof) && (avail < 32)) {
            size_t oldAvail = avail;

            ctxt->input->cur = in;

            SHRINK;
            xmlParserGrow(ctxt);

            in = ctxt->input->cur;
            avail = ctxt->input->end - in;

            if (oldAvail == avail)
                eof = 1;
        }

        if (avail == 0)
            break;

        c = *in;
        size = 1;

        if ((nbchar != 0) &&
            ((c == '/') || (c == '>') || (c == stop) ||
             (IS_WS_HTML(c))))
            break;

        if (c == 0) {
            if (nbchar + 3 <= HTML_PARSER_BUFFER_SIZE) {
                buf[nbchar++] = 0xEF;
                buf[nbchar++] = 0xBF;
                buf[nbchar++] = 0xBD;
            }
        } else if (c < 0x80) {
            if (nbchar < HTML_PARSER_BUFFER_SIZE) {
                if (IS_UPPER(c))
                    c += 0x20;
                buf[nbchar++] = c;
            }
        } else {
            size = htmlValidateUtf8(ctxt, in, avail, /* partial */ 0);

            if (size > 0) {
                if (nbchar + size <= HTML_PARSER_BUFFER_SIZE) {
                    memcpy(buf + nbchar, in, size);
                    nbchar += size;
                }
            } else {
                size = 1;

                if (nbchar + 3 <= HTML_PARSER_BUFFER_SIZE) {
                    buf[nbchar++] = 0xEF;
                    buf[nbchar++] = 0xBF;
                    buf[nbchar++] = 0xBD;
                }
            }
        }

        in += size;
        avail -= size;
    }

    ctxt->input->cur = in;

    SHRINK;

    ret = xmlDictLookupHashed(ctxt->dict, buf, nbchar);
    if (ret.name == NULL)
        htmlErrMemory(ctxt);

    return(ret);
}

static const short htmlC1Remap[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};

static const xmlChar *
htmlCodePointToUtf8(int c, xmlChar *out, int *osize) {
    int i = 0;
    int bits, hi;

    if ((c >= 0x80) && (c < 0xA0)) {
        c = htmlC1Remap[c - 0x80];
    } else if ((c <= 0) ||
               ((c >= 0xD800) && (c < 0xE000)) ||
               (c > 0x10FFFF)) {
        c = 0xFFFD;
    }

    if      (c <    0x80) { bits =  0; hi = 0x00; }
    else if (c <   0x800) { bits =  6; hi = 0xC0; }
    else if (c < 0x10000) { bits = 12; hi = 0xE0; }
    else                  { bits = 18; hi = 0xF0; }

    out[i++] = (c >> bits) | hi;

    while (bits > 0) {
        bits -= 6;
        out[i++] = ((c >> bits) & 0x3F) | 0x80;
    }

    *osize = i;
    return(out);
}

#include "html5ent.inc"

#define ENT_F_SEMICOLON 0x80u
#define ENT_F_SUBTABLE  0x40u
#define ENT_F_ALL       0xC0u

static const xmlChar *
htmlFindEntityPrefix(const xmlChar *string, size_t slen, int isAttr,
                     int *nlen, int *rlen) {
    const xmlChar *match = NULL;
    unsigned left, right;
    int first = string[0];
    size_t matchLen = 0;
    size_t soff = 1;

    if (slen < 2)
        return(NULL);
    if (!IS_ASCII_LETTER(first))
        return(NULL);

    /*
     * Look up range by first character
     */
    first &= 63;
    left = htmlEntAlpha[first*3] | htmlEntAlpha[first*3+1] << 8;
    right = left + htmlEntAlpha[first*3+2];

    /*
     * Binary search
     */
    while (left < right) {
        const xmlChar *bytes;
        unsigned mid;
        size_t len;
        int cmp;

        mid = left + (right - left) / 2;
        bytes = htmlEntStrings + htmlEntValues[mid];
        len = bytes[0] & ~ENT_F_ALL;

        cmp = string[soff] - bytes[1];

        if (cmp == 0) {
            if (slen < len) {
                cmp = strncmp((const char *) string + soff + 1,
                              (const char *) bytes + 2,
                              slen - 1);
                /* Prefix can never match */
                if (cmp == 0)
                    break;
            } else {
                cmp = strncmp((const char *) string + soff + 1,
                              (const char *) bytes + 2,
                              len - 1);
            }
        }

        if (cmp < 0) {
            right = mid;
        } else if (cmp > 0) {
            left = mid + 1;
        } else {
            int term = soff + len < slen ? string[soff + len] : 0;
            int isAlnum, isTerm;

            isAlnum = IS_ALNUM(term);
            isTerm = ((term == ';') ||
                      ((bytes[0] & ENT_F_SEMICOLON) &&
                       ((!isAttr) ||
                        ((!isAlnum) && (term != '=')))));

            if (isTerm) {
                match = bytes + len + 1;
                matchLen = soff + len;
                if (term == ';')
                    matchLen += 1;
            }

            if (bytes[0] & ENT_F_SUBTABLE) {
                if (isTerm)
                    match += 2;

                if ((isAlnum) && (soff + len < slen)) {
                    left = mid + bytes[len + 1];
                    right = left + bytes[len + 2];
                    soff += len;
                    continue;
                }
            }

            break;
        }
    }

    if (match == NULL)
        return(NULL);

    *nlen = matchLen;
    *rlen = match[0];
    return(match + 1);
}

/**
 * htmlParseData:
 * @ctxt:  an HTML parser context
 * @mask:  mask of terminating characters
 * @comment:  true if parsing a comment
 * @refs:  true if references are allowed
 * @maxLength:  maximum output length
 *
 * Parse data until terminator is reached.
 *
 * Returns the parsed string or NULL in case of errors.
 */

static xmlChar *
htmlParseData(htmlParserCtxtPtr ctxt, htmlAsciiMask mask,
              int comment, int refs, int maxLength) {
    xmlParserInputPtr input = ctxt->input;
    xmlChar *ret = NULL;
    xmlChar *buffer;
    xmlChar utf8Char[4];
    size_t buffer_size;
    size_t used;
    int eof = PARSER_PROGRESSIVE(ctxt);
    int line, col;
    int termSkip = -1;

    used = 0;
    buffer_size = ctxt->spaceMax;
    buffer = (xmlChar *) ctxt->spaceTab;
    if (buffer == NULL) {
        buffer_size = 500;
        buffer = xmlMalloc(buffer_size + 1);
        if (buffer == NULL) {
            htmlErrMemory(ctxt);
            return(NULL);
        }
    }

    line = input->line;
    col = input->col;

    while (!PARSER_STOPPED(ctxt)) {
        const xmlChar *chunk, *in, *repl;
        size_t avail, chunkSize, extraSize;
        int replSize;
        int skip = 0;
        int ncr = 0;
        int ncrSize = 0;
        int cp = 0;

        chunk = input->cur;
        avail = input->end - chunk;
        in = chunk;

        repl = BAD_CAST "";
        replSize = 0;

        while (!PARSER_STOPPED(ctxt)) {
            size_t j;
            int cur, size;

            if ((!eof) && (avail <= 64)) {
                size_t oldAvail = avail;
                size_t off = in - chunk;

                input->cur = in;

                xmlParserGrow(ctxt);

                in = input->cur;
                chunk = in - off;
                input->cur = chunk;
                avail = input->end - in;

                if (oldAvail == avail)
                    eof = 1;
            }

            if (avail == 0) {
                termSkip = 0;
                break;
            }

            cur = *in;
            size = 1;
            col += 1;

            if (htmlMaskMatch(mask, cur)) {
                if (comment) {
                    if (avail < 2) {
                        termSkip = 1;
                    } else if (in[1] == '-') {
                        if  (avail < 3) {
                            termSkip = 2;
                        } else if (in[2] == '>') {
                            termSkip = 3;
                        } else if (in[2] == '!') {
                            if (avail < 4)
                                termSkip = 3;
                            else if (in[3] == '>')
                                termSkip = 4;
                        }
                    }

                    if (termSkip >= 0)
                        break;
                } else {
                    termSkip = 0;
                    break;
                }
            }

            if (ncr) {
                int lc = cur | 0x20;
                int digit;

                if ((cur >= '0') && (cur <= '9')) {
                    digit = cur - '0';
                } else if ((ncr == 16) && (lc >= 'a') && (lc <= 'f')) {
                    digit = (lc - 'a') + 10;
                } else {
                    if (cur == ';') {
                        in += 1;
                        size += 1;
                        ncrSize += 1;
                    }
                    goto next_chunk;
                }

                cp = cp * ncr + digit;
                if (cp >= 0x110000)
                    cp = 0x110000;

                ncrSize += 1;

                goto next_char;
            }

            switch (cur) {
            case '&':
                if (!refs)
                    break;

                j = 1;

                if ((j < avail) && (in[j] == '#')) {
                    j += 1;
                    if (j < avail) {
                        if ((in[j] | 0x20) == 'x') {
                            j += 1;
                            if ((j < avail) && (IS_HEX_DIGIT(in[j]))) {
                                ncr = 16;
                                size = 3;
                                ncrSize = 3;
                                cp = 0;
                            }
                        } else if (IS_ASCII_DIGIT(in[j])) {
                            ncr = 10;
                            size = 2;
                            ncrSize = 2;
                            cp = 0;
                        }
                    }
                } else {
                    repl = htmlFindEntityPrefix(in + j,
                                                avail - j,
                                                /* isAttr */ 1,
                                                &skip, &replSize);
                    if (repl != NULL) {
                        skip += 1;
                        goto next_chunk;
                    }

                    skip = 0;
                }

                break;

            case '\0':
                skip = 1;
                repl = BAD_CAST "\xEF\xBF\xBD";
                replSize = 3;
                goto next_chunk;

            case '\n':
                line += 1;
                col = 1;
                break;

            case '\r':
                skip = 1;
                if (in[1] != 0x0A) {
                    repl = BAD_CAST "\x0A";
                    replSize = 1;
                }
                goto next_chunk;

            default:
                if (cur < 0x80)
                    break;

                if ((input->flags & XML_INPUT_HAS_ENCODING) == 0) {
                    xmlChar * guess;

                    if (in > chunk)
                        goto next_chunk;

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
                    guess = NULL;
#else
                    guess = htmlFindEncoding(ctxt);
#endif
                    if (guess == NULL) {
                        xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_8859_1);
                    } else {
                        xmlSwitchEncodingName(ctxt, (const char *) guess);
                        xmlFree(guess);
                    }
                    input->flags |= XML_INPUT_HAS_ENCODING;

                    eof = PARSER_PROGRESSIVE(ctxt);
                    goto restart;
                }

                size = htmlValidateUtf8(ctxt, in, avail, /* partial */ 0);

                if (size <= 0) {
                    skip = 1;
                    repl = BAD_CAST "\xEF\xBF\xBD";
                    replSize = 3;
                    goto next_chunk;
                }

                break;
            }

next_char:
            in += size;
            avail -= size;
        }

next_chunk:
        if (ncrSize > 0) {
            skip = ncrSize;
            in -= ncrSize;

            repl = htmlCodePointToUtf8(cp, utf8Char, &replSize);
        }

        chunkSize = in - chunk;
        extraSize = chunkSize + replSize;

        if (extraSize > maxLength - used) {
            htmlParseErr(ctxt, XML_ERR_RESOURCE_LIMIT,
                         "value too long\n", NULL, NULL);
            goto error;
        }

        if (extraSize > buffer_size - used) {
            size_t newSize = (used + extraSize) * 2;
            xmlChar *tmp = xmlRealloc(buffer, newSize + 1);

            if (tmp == NULL) {
                htmlErrMemory(ctxt);
                goto error;
            }
            buffer = tmp;
            buffer_size = newSize;
        }

        if (chunkSize > 0) {
            input->cur += chunkSize;
            memcpy(buffer + used, chunk, chunkSize);
            used += chunkSize;
        }

        input->cur += skip;
        if (replSize > 0) {
            memcpy(buffer + used, repl, replSize);
            used += replSize;
        }

        SHRINK;

        if (termSkip >= 0)
            break;

restart:
        ;
    }

    if (termSkip > 0) {
        input->cur += termSkip;
        col += termSkip;
    }

    input->line = line;
    input->col = col;

    ret = xmlMalloc(used + 1);
    if (ret == NULL) {
        htmlErrMemory(ctxt);
    } else {
        memcpy(ret, buffer, used);
        ret[used] = 0;
    }

error:
    ctxt->spaceTab = (void *) buffer;
    ctxt->spaceMax = buffer_size;

    return(ret);
}

/**
 * htmlParseEntityRef:
 * @ctxt:  an HTML parser context
 * @str:  location to store the entity name
 *
 * DEPRECATED: Internal function, don't use.
 *
 * Returns NULL.
 */
const htmlEntityDesc *
htmlParseEntityRef(htmlParserCtxtPtr ctxt ATTRIBUTE_UNUSED,
                   const xmlChar **str ATTRIBUTE_UNUSED) {
    return(NULL);
}

/**
 * htmlParseAttValue:
 * @ctxt:  an HTML parser context
 *
 * parse a value for an attribute
 * Note: the parser won't do substitution of entities here, this
 * will be handled later in xmlStringGetNodeList, unless it was
 * asked for ctxt->replaceEntities != 0
 *
 * Returns the AttValue parsed or NULL.
 */

static xmlChar *
htmlParseAttValue(htmlParserCtxtPtr ctxt) {
    xmlChar *ret = NULL;
    int maxLength = (ctxt->options & HTML_PARSE_HUGE) ?
                    XML_MAX_HUGE_LENGTH :
                    XML_MAX_TEXT_LENGTH;

    if (CUR == '"') {
        SKIP(1);
	ret = htmlParseData(ctxt, MASK_DQ, 0, 1, maxLength);
        if (CUR == '"')
            SKIP(1);
    } else if (CUR == '\'') {
        SKIP(1);
	ret = htmlParseData(ctxt, MASK_SQ, 0, 1, maxLength);
        if (CUR == '\'')
            SKIP(1);
    } else {
	ret = htmlParseData(ctxt, MASK_WS_GT, 0, 1, maxLength);
    }
    return(ret);
}

static void
htmlCharDataSAXCallback(htmlParserCtxtPtr ctxt, const xmlChar *buf,
                        int size, int mode) {
    if ((ctxt->sax == NULL) || (ctxt->disableSAX))
        return;

    if ((mode == 0) || (mode == DATA_RCDATA) ||
        (ctxt->sax->cdataBlock == NULL)) {
        if ((ctxt->name == NULL) ||
            (xmlStrEqual(ctxt->name, BAD_CAST "html")) ||
            (xmlStrEqual(ctxt->name, BAD_CAST "head"))) {
            int i;

            /*
             * Add leading whitespace to html or head elements before
             * calling htmlStartCharData.
             */
            for (i = 0; i < size; i++)
                if (!IS_WS_HTML(buf[i]))
                    break;

            if (i > 0) {
                if (!ctxt->keepBlanks) {
                    if (ctxt->sax->ignorableWhitespace != NULL)
                        ctxt->sax->ignorableWhitespace(ctxt->userData, buf, i);
                } else {
                    if (ctxt->sax->characters != NULL)
                        ctxt->sax->characters(ctxt->userData, buf, i);
                }

                buf += i;
                size -= i;
            }

            if (size <= 0)
                return;

            htmlStartCharData(ctxt);

            if (PARSER_STOPPED(ctxt))
                return;
        }

        if ((mode == 0) &&
            (!ctxt->keepBlanks) &&
            (areBlanks(ctxt, buf, size) > 0)) {
            if (ctxt->sax->ignorableWhitespace != NULL)
                ctxt->sax->ignorableWhitespace(ctxt->userData, buf, size);
        } else {
            if (ctxt->sax->characters != NULL)
                ctxt->sax->characters(ctxt->userData, buf, size);
        }
    } else {
        /*
         * Insert as CDATA, which is the same as HTML_PRESERVE_NODE
         */
        ctxt->sax->cdataBlock(ctxt->userData, buf, size);
    }
}

/**
 * htmlParseCharData:
 * @ctxt:  an HTML parser context
 * @partial: true if the input buffer is incomplete
 *
 * Parse character data and references.
 *
 * Returns 1 if all data was parsed, 0 otherwise.
 */

static int
htmlParseCharData(htmlParserCtxtPtr ctxt, int partial) {
    xmlParserInputPtr input = ctxt->input;
    xmlChar utf8Char[4];
    int complete = 0;
    int done = 0;
    int mode;
    int eof = PARSER_PROGRESSIVE(ctxt);
    int line, col;

    mode = ctxt->endCheckState;

    line = input->line;
    col = input->col;

    while (!PARSER_STOPPED(ctxt)) {
        const xmlChar *chunk, *in, *repl;
        size_t avail;
        int replSize;
        int skip = 0;
        int ncr = 0;
        int ncrSize = 0;
        int cp = 0;

        chunk = input->cur;
        avail = input->end - chunk;
        in = chunk;

        repl = BAD_CAST "";
        replSize = 0;

        while (!PARSER_STOPPED(ctxt)) {
            size_t j;
            int cur, size;

            if (avail <= 64) {
                if (!eof) {
                    size_t oldAvail = avail;
                    size_t off = in - chunk;

                    input->cur = in;

                    xmlParserGrow(ctxt);

                    in = input->cur;
                    chunk = in - off;
                    input->cur = chunk;
                    avail = input->end - in;

                    if (oldAvail == avail)
                        eof = 1;
                }

                if (avail == 0) {
                    if ((partial) && (ncr)) {
                        in -= ncrSize;
                        ncrSize = 0;
                    }

                    done = 1;
                    break;
                }
            }

            /* Accelerator */
            if (!ncr) {
                while (avail > 0) {
                    static const unsigned mask[8] = {
                        0x00002401, 0x10002040,
                        0x00000000, 0x00000000,
                        0xFFFFFFFF, 0xFFFFFFFF,
                        0xFFFFFFFF, 0xFFFFFFFF
                    };
                    cur = *in;
                    if ((1u << (cur & 0x1F)) & mask[cur >> 5])
                        break;
                    col += 1;
                    in += 1;
                    avail -= 1;
                }

                if ((!eof) && (avail <= 64))
                    continue;
                if (avail == 0)
                    continue;
            }

            cur = *in;
            size = 1;
            col += 1;

            if (ncr) {
                int lc = cur | 0x20;
                int digit;

                if ((cur >= '0') && (cur <= '9')) {
                    digit = cur - '0';
                } else if ((ncr == 16) && (lc >= 'a') && (lc <= 'f')) {
                    digit = (lc - 'a') + 10;
                } else {
                    if (cur == ';') {
                        in += 1;
                        size += 1;
                        ncrSize += 1;
                    }
                    goto next_chunk;
                }

                cp = cp * ncr + digit;
                if (cp >= 0x110000)
                    cp = 0x110000;

                ncrSize += 1;

                goto next_char;
            }

            switch (cur) {
            case '<':
                if (mode == 0) {
                    done = 1;
                    complete = 1;
                    goto next_chunk;
                }
                if (mode == DATA_PLAINTEXT)
                    break;

                j = 1;
                if (j < avail) {
                    if ((mode == DATA_SCRIPT) && (in[j] == '!')) {
                        /* Check for comment start */

                        j += 1;
                        if ((j < avail) && (in[j] == '-')) {
                            j += 1;
                            if ((j < avail) && (in[j] == '-'))
                                mode = DATA_SCRIPT_ESC1;
                        }
                    } else {
                        int i = 0;
                        int solidus = 0;

                        /* Check for tag */

                        if (in[j] == '/') {
                            j += 1;
                            solidus = 1;
                        }

                        if ((solidus) || (mode == DATA_SCRIPT_ESC1)) {
                            while ((j < avail) &&
                                   (ctxt->name[i] != 0) &&
                                   (ctxt->name[i] == (in[j] | 0x20))) {
                                i += 1;
                                j += 1;
                            }

                            if ((ctxt->name[i] == 0) && (j < avail)) {
                                int c = in[j];

                                if ((c == '>') || (c == '/') ||
                                    (IS_WS_HTML(c))) {
                                    if ((mode == DATA_SCRIPT_ESC1) &&
                                        (!solidus)) {
                                        mode = DATA_SCRIPT_ESC2;
                                    } else if (mode == DATA_SCRIPT_ESC2) {
                                        mode = DATA_SCRIPT_ESC1;
                                    } else {
                                        complete = 1;
                                        done = 1;
                                        goto next_chunk;
                                    }
                                }
                            }
                        }
                    }
                }

                if ((partial) && (j >= avail)) {
                    done = 1;
                    goto next_chunk;
                }

                break;

            case '-':
                if ((mode != DATA_SCRIPT_ESC1) && (mode != DATA_SCRIPT_ESC2))
                    break;

                /* Check for comment end */

                j = 1;
                if ((j < avail) && (in[j] == '-')) {
                    j += 1;
                    if ((j < avail) && (in[j] == '>'))
                        mode = DATA_SCRIPT;
                }

                if ((partial) && (j >= avail)) {
                    done = 1;
                    goto next_chunk;
                }

                break;

            case '&':
                if ((mode != 0) && (mode != DATA_RCDATA))
                    break;

                j = 1;

                if ((j < avail) && (in[j] == '#')) {
                    j += 1;
                    if (j < avail) {
                        if ((in[j] | 0x20) == 'x') {
                            j += 1;
                            if ((j < avail) && (IS_HEX_DIGIT(in[j]))) {
                                ncr = 16;
                                size = 3;
                                ncrSize = 3;
                                cp = 0;
                            }
                        } else if (IS_ASCII_DIGIT(in[j])) {
                            ncr = 10;
                            size = 2;
                            ncrSize = 2;
                            cp = 0;
                        }
                    }
                } else {
                    if (partial) {
                        int terminated = 0;
                        size_t i;

                        /*
                         * &CounterClockwiseContourIntegral; has 33 bytes.
                         */
                        for (i = 1; i < avail; i++) {
                            if ((i >= 32) ||
                                (!IS_ASCII_LETTER(in[i]) &&
                                 ((i < 2) || !IS_ASCII_DIGIT(in[i])))) {
                                terminated = 1;
                                break;
                            }
                        }

                        if (!terminated) {
                            done = 1;
                            goto next_chunk;
                        }
                    }

                    repl = htmlFindEntityPrefix(in + j,
                                                avail - j,
                                                /* isAttr */ 0,
                                                &skip, &replSize);
                    if (repl != NULL) {
                        skip += 1;
                        goto next_chunk;
                    }

                    skip = 0;
                }

                if ((partial) && (j >= avail)) {
                    done = 1;
                    goto next_chunk;
                }

                break;

            case '\0':
                skip = 1;
                repl = BAD_CAST "\xEF\xBF\xBD";
                replSize = 3;
                goto next_chunk;

            case '\n':
                line += 1;
                col = 1;
                break;

            case '\r':
                if (partial && avail < 2) {
                    done = 1;
                    goto next_chunk;
                }

                skip = 1;
                if (in[1] != 0x0A) {
                    repl = BAD_CAST "\x0A";
                    replSize = 1;
                }
                goto next_chunk;

            default:
                if (cur < 0x80)
                    break;

                if ((input->flags & XML_INPUT_HAS_ENCODING) == 0) {
                    xmlChar * guess;

                    if (in > chunk)
                        goto next_chunk;

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
                    guess = NULL;
#else
                    guess = htmlFindEncoding(ctxt);
#endif
                    if (guess == NULL) {
                        xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_8859_1);
                    } else {
                        xmlSwitchEncodingName(ctxt, (const char *) guess);
                        xmlFree(guess);
                    }
                    input->flags |= XML_INPUT_HAS_ENCODING;

                    eof = PARSER_PROGRESSIVE(ctxt);
                    goto restart;
                }

                size = htmlValidateUtf8(ctxt, in, avail, partial);

                if ((partial) && (size == 0)) {
                    done = 1;
                    goto next_chunk;
                }

                if (size <= 0) {
                    skip = 1;
                    repl = BAD_CAST "\xEF\xBF\xBD";
                    replSize = 3;
                    goto next_chunk;
                }

                break;
            }

next_char:
            in += size;
            avail -= size;
        }

next_chunk:
        if (ncrSize > 0) {
            skip = ncrSize;
            in -= ncrSize;

            repl = htmlCodePointToUtf8(cp, utf8Char, &replSize);
        }

        if (in > chunk) {
            input->cur += in - chunk;
            htmlCharDataSAXCallback(ctxt, chunk, in - chunk, mode);
        }

        input->cur += skip;
        if (replSize > 0)
            htmlCharDataSAXCallback(ctxt, repl, replSize, mode);

        SHRINK;

        if (done)
            break;

restart:
        ;
    }

    input->line = line;
    input->col = col;

    if (complete)
        ctxt->endCheckState = 0;
    else
        ctxt->endCheckState = mode;

    return(complete);
}

/**
 * htmlParseComment:
 * @ctxt:  an HTML parser context
 * @bogus:  true if this is a bogus comment
 *
 * Parse an HTML comment
 */
static void
htmlParseComment(htmlParserCtxtPtr ctxt, int bogus) {
    const xmlChar *comment = BAD_CAST "";
    xmlChar *buf = NULL;
    int maxLength = (ctxt->options & HTML_PARSE_HUGE) ?
                    XML_MAX_HUGE_LENGTH :
                    XML_MAX_TEXT_LENGTH;

    if (bogus) {
        buf = htmlParseData(ctxt, MASK_GT, 0, 0, maxLength);
        if (CUR == '>')
            SKIP(1);
        comment = buf;
    } else {
        if (CUR == '>') {
            SKIP(1);
        } else if ((CUR == '-') && (NXT(1) == '>')) {
            SKIP(2);
        } else {
            buf = htmlParseData(ctxt, MASK_DASH, 1, 0, maxLength);
            comment = buf;
        }
    }

    if (comment == NULL)
        return;

    if ((ctxt->sax != NULL) && (ctxt->sax->comment != NULL) &&
        (!ctxt->disableSAX))
        ctxt->sax->comment(ctxt->userData, comment);

    xmlFree(buf);
}

/**
 * htmlParseCharRef:
 * @ctxt:  an HTML parser context
 *
 * DEPRECATED: Internal function, don't use.
 *
 * Returns 0
 */
int
htmlParseCharRef(htmlParserCtxtPtr ctxt ATTRIBUTE_UNUSED) {
    return(0);
}


/**
 * htmlParseDoctypeLiteral:
 * @ctxt:  an HTML parser context
 *
 * Parse a DOCTYPE SYTSTEM or PUBLIC literal.
 *
 * Returns the literal or NULL in case of error.
 */

static xmlChar *
htmlParseDoctypeLiteral(htmlParserCtxtPtr ctxt) {
    xmlChar *ret;
    int maxLength = (ctxt->options & HTML_PARSE_HUGE) ?
                    XML_MAX_TEXT_LENGTH :
                    XML_MAX_NAME_LENGTH;

    if (CUR == '"') {
        SKIP(1);
        ret = htmlParseData(ctxt, MASK_DQ_GT, 0, 0, maxLength);
        if (CUR == '"')
            SKIP(1);
    } else if (CUR == '\'') {
        SKIP(1);
        ret = htmlParseData(ctxt, MASK_SQ_GT, 0, 0, maxLength);
        if (CUR == '\'')
            SKIP(1);
    } else {
        return(NULL);
    }

    return(ret);
}

static void
htmlSkipBogusDoctype(htmlParserCtxtPtr ctxt) {
    const xmlChar *in;
    size_t avail;
    int eof = PARSER_PROGRESSIVE(ctxt);
    int line, col;

    line = ctxt->input->line;
    col = ctxt->input->col;

    in = ctxt->input->cur;
    avail = ctxt->input->end - in;

    while (!PARSER_STOPPED(ctxt)) {
        int cur;

        if ((!eof) && (avail <= 64)) {
            size_t oldAvail = avail;

            ctxt->input->cur = in;

            xmlParserGrow(ctxt);

            in = ctxt->input->cur;
            avail = ctxt->input->end - in;

            if (oldAvail == avail)
                eof = 1;
        }

        if (avail == 0)
            break;

        col += 1;

        cur = *in;
        if (cur == '>') {
            in += 1;
            break;
        } else if (cur == 0x0A) {
            line += 1;
            col = 1;
        }

        in += 1;
        avail -= 1;

        SHRINK;
    }

    ctxt->input->cur = in;
    ctxt->input->line = line;
    ctxt->input->col = col;
}

/**
 * htmlParseDocTypeDecl:
 * @ctxt:  an HTML parser context
 *
 * Parse a DOCTYPE declaration.
 */

static void
htmlParseDocTypeDecl(htmlParserCtxtPtr ctxt) {
    xmlChar *name = NULL;
    xmlChar *publicId = NULL;
    xmlChar *URI = NULL;
    int maxLength = (ctxt->options & HTML_PARSE_HUGE) ?
                    XML_MAX_TEXT_LENGTH :
                    XML_MAX_NAME_LENGTH;

    /*
     * We know that '<!DOCTYPE' has been detected.
     */
    SKIP(9);

    SKIP_BLANKS;

    if ((ctxt->input->cur < ctxt->input->end) && (CUR != '>')) {
        name = htmlParseData(ctxt, MASK_WS_GT, 0, 0, maxLength);

        if ((ctxt->options & HTML_PARSE_HTML5) && (name != NULL)) {
            xmlChar *cur;

            for (cur = name; *cur; cur++) {
                if (IS_UPPER(*cur))
                    *cur += 0x20;
            }
        }

        SKIP_BLANKS;
    }

    /*
     * Check for SystemID and publicId
     */
    if ((UPPER == 'P') && (UPP(1) == 'U') &&
	(UPP(2) == 'B') && (UPP(3) == 'L') &&
	(UPP(4) == 'I') && (UPP(5) == 'C')) {
        SKIP(6);
        SKIP_BLANKS;
	publicId = htmlParseDoctypeLiteral(ctxt);
	if (publicId == NULL)
            goto bogus;
        SKIP_BLANKS;
	URI = htmlParseDoctypeLiteral(ctxt);
    } else if ((UPPER == 'S') && (UPP(1) == 'Y') &&
               (UPP(2) == 'S') && (UPP(3) == 'T') &&
	       (UPP(4) == 'E') && (UPP(5) == 'M')) {
        SKIP(6);
        SKIP_BLANKS;
	URI = htmlParseDoctypeLiteral(ctxt);
    }

bogus:
    htmlSkipBogusDoctype(ctxt);

    /*
     * Create or update the document accordingly to the DOCTYPE
     */
    if ((ctxt->sax != NULL) && (ctxt->sax->internalSubset != NULL) &&
	(!ctxt->disableSAX))
	ctxt->sax->internalSubset(ctxt->userData, name, publicId, URI);

    xmlFree(name);
    xmlFree(URI);
    xmlFree(publicId);
}

/**
 * htmlParseAttribute:
 * @ctxt:  an HTML parser context
 * @value:  a xmlChar ** used to store the value of the attribute
 *
 * parse an attribute
 *
 * [41] Attribute ::= Name Eq AttValue
 *
 * [25] Eq ::= S? '=' S?
 *
 * With namespace:
 *
 * [NS 11] Attribute ::= QName Eq AttValue
 *
 * Also the case QName == xmlns:??? is handled independently as a namespace
 * definition.
 *
 * Returns the attribute name, and the value in *value.
 */

static xmlHashedString
htmlParseAttribute(htmlParserCtxtPtr ctxt, xmlChar **value) {
    xmlHashedString hname;
    xmlChar *val = NULL;

    *value = NULL;
    hname = htmlParseHTMLName(ctxt, 1);
    if (hname.name == NULL)
        return(hname);

    /*
     * read the value
     */
    SKIP_BLANKS;
    if (CUR == '=') {
        SKIP(1);
	SKIP_BLANKS;
	val = htmlParseAttValue(ctxt);
    }

    *value = val;
    return(hname);
}

/**
 * htmlCheckEncoding:
 * @ctxt:  an HTML parser context
 * @attvalue: the attribute value
 *
 * Checks an http-equiv attribute from a Meta tag to detect
 * the encoding
 * If a new encoding is detected the parser is switched to decode
 * it and pass UTF8
 */
static void
htmlCheckEncoding(htmlParserCtxtPtr ctxt, const xmlChar *attvalue) {
    const xmlChar *encoding;
    xmlChar *copy;

    if (!attvalue)
	return;

    encoding = xmlStrcasestr(attvalue, BAD_CAST"charset");
    if (encoding != NULL) {
	encoding += 7;
    }
    /*
     * skip blank
     */
    if (encoding && IS_WS_HTML(*encoding))
	encoding = xmlStrcasestr(attvalue, BAD_CAST"=");
    if (encoding && *encoding == '=') {
	encoding ++;
        copy = xmlStrdup(encoding);
        if (copy == NULL)
            htmlErrMemory(ctxt);
	xmlSetDeclaredEncoding(ctxt, copy);
    }
}

/**
 * htmlCheckMeta:
 * @ctxt:  an HTML parser context
 * @atts:  the attributes values
 *
 * Checks an attributes from a Meta tag
 */
static void
htmlCheckMeta(htmlParserCtxtPtr ctxt, const xmlChar **atts) {
    int i;
    const xmlChar *att, *value;
    int http = 0;
    const xmlChar *content = NULL;

    if ((ctxt == NULL) || (atts == NULL))
	return;

    i = 0;
    att = atts[i++];
    while (att != NULL) {
	value = atts[i++];
        if (value != NULL) {
            if ((!xmlStrcasecmp(att, BAD_CAST "http-equiv")) &&
                (!xmlStrcasecmp(value, BAD_CAST "Content-Type"))) {
                http = 1;
            } else if (!xmlStrcasecmp(att, BAD_CAST "charset")) {
                xmlChar *copy;

                copy = xmlStrdup(value);
                if (copy == NULL)
                    htmlErrMemory(ctxt);
                xmlSetDeclaredEncoding(ctxt, copy);
            } else if (!xmlStrcasecmp(att, BAD_CAST "content")) {
                content = value;
            }
        }
	att = atts[i++];
    }
    if ((http) && (content != NULL))
	htmlCheckEncoding(ctxt, content);

}

/**
 * htmlAttrHashInsert:
 * @ctxt: parser context
 * @size: size of the hash table
 * @name: attribute name
 * @hashValue: hash value of name
 * @aindex: attribute index (this is a multiple of 5)
 *
 * Inserts a new attribute into the hash table.
 *
 * Returns INT_MAX if no existing attribute was found, the attribute
 * index if an attribute was found, -1 if a memory allocation failed.
 */
static int
htmlAttrHashInsert(xmlParserCtxtPtr ctxt, unsigned size, const xmlChar *name,
                   unsigned hashValue, int aindex) {
    xmlAttrHashBucket *table = ctxt->attrHash;
    xmlAttrHashBucket *bucket;
    unsigned hindex;

    hindex = hashValue & (size - 1);
    bucket = &table[hindex];

    while (bucket->index >= 0) {
        const xmlChar **atts = &ctxt->atts[bucket->index];

        if (name == atts[0])
            return(bucket->index);

        hindex++;
        bucket++;
        if (hindex >= size) {
            hindex = 0;
            bucket = table;
        }
    }

    bucket->index = aindex;

    return(INT_MAX);
}

/**
 * htmlParseStartTag:
 * @ctxt:  an HTML parser context
 *
 * parse a start of tag either for rule element or
 * EmptyElement. In both case we don't parse the tag closing chars.
 *
 * [40] STag ::= '<' Name (S Attribute)* S? '>'
 *
 * [44] EmptyElemTag ::= '<' Name (S Attribute)* S? '/>'
 *
 * With namespace:
 *
 * [NS 8] STag ::= '<' QName (S Attribute)* S? '>'
 *
 * [NS 10] EmptyElement ::= '<' QName (S Attribute)* S? '/>'
 *
 * Returns 0 in case of success, -1 in case of error and 1 if discarded
 */

static void
htmlParseStartTag(htmlParserCtxtPtr ctxt) {
    const xmlChar *name;
    const xmlChar *attname;
    xmlChar *attvalue;
    const xmlChar **atts;
    int nbatts = 0;
    int maxatts;
    int meta = 0;
    int i;
    int discardtag = 0;

    ctxt->endCheckState = 0;

    SKIP(1);

    atts = ctxt->atts;
    maxatts = ctxt->maxatts;

    GROW;
    name = htmlParseHTMLName(ctxt, 0).name;
    if (name == NULL)
        return;
    if (xmlStrEqual(name, BAD_CAST"meta"))
	meta = 1;

    if ((ctxt->options & HTML_PARSE_HTML5) == 0) {
        /*
         * Check for auto-closure of HTML elements.
         */
        htmlAutoClose(ctxt, name);

        /*
         * Check for implied HTML elements.
         */
        htmlCheckImplied(ctxt, name);

        /*
         * Avoid html at any level > 0, head at any level != 1
         * or any attempt to recurse body
         */
        if ((ctxt->nameNr > 0) && (xmlStrEqual(name, BAD_CAST"html"))) {
            htmlParseErr(ctxt, XML_HTML_STRUCURE_ERROR,
                         "htmlParseStartTag: misplaced <html> tag\n",
                         name, NULL);
            discardtag = 1;
            ctxt->depth++;
        }
        if ((ctxt->nameNr != 1) &&
            (xmlStrEqual(name, BAD_CAST"head"))) {
            htmlParseErr(ctxt, XML_HTML_STRUCURE_ERROR,
                         "htmlParseStartTag: misplaced <head> tag\n",
                         name, NULL);
            discardtag = 1;
            ctxt->depth++;
        }
        if (xmlStrEqual(name, BAD_CAST"body")) {
            int indx;
            for (indx = 0;indx < ctxt->nameNr;indx++) {
                if (xmlStrEqual(ctxt->nameTab[indx], BAD_CAST"body")) {
                    htmlParseErr(ctxt, XML_HTML_STRUCURE_ERROR,
                                 "htmlParseStartTag: misplaced <body> tag\n",
                                 name, NULL);
                    discardtag = 1;
                    ctxt->depth++;
                }
            }
        }
    }

    /*
     * Now parse the attributes, it ends up with the ending
     *
     * (S Attribute)* S?
     */
    SKIP_BLANKS;
    while ((ctxt->input->cur < ctxt->input->end) &&
           (CUR != '>') &&
	   ((CUR != '/') || (NXT(1) != '>')) &&
           (PARSER_STOPPED(ctxt) == 0)) {
        xmlHashedString hattname;

        /*  unexpected-solidus-in-tag */
        if (CUR == '/') {
            SKIP(1);
            SKIP_BLANKS;
            continue;
        }
	GROW;
	hattname = htmlParseAttribute(ctxt, &attvalue);
        attname = hattname.name;

        if (attname != NULL) {
	    /*
	     * Add the pair to atts
	     */
	    if (nbatts + 4 > maxatts) {
	        const xmlChar **tmp;
                unsigned *utmp;
                int newSize;

                newSize = xmlGrowCapacity(maxatts,
                                          sizeof(tmp[0]) * 2 + sizeof(utmp[0]),
                                          11, HTML_MAX_ATTRS);
		if (newSize < 0) {
		    htmlErrMemory(ctxt);
		    goto failed;
		}
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
                if (newSize < 2)
                    newSize = 2;
#endif
	        tmp = xmlRealloc(atts, newSize * sizeof(tmp[0]) * 2);
		if (tmp == NULL) {
		    htmlErrMemory(ctxt);
		    goto failed;
		}
                atts = tmp;
		ctxt->atts = tmp;

	        utmp = xmlRealloc(ctxt->attallocs, newSize * sizeof(utmp[0]));
		if (utmp == NULL) {
		    htmlErrMemory(ctxt);
		    goto failed;
		}
                ctxt->attallocs = utmp;

                maxatts = newSize * 2;
		ctxt->maxatts = maxatts;
	    }

            ctxt->attallocs[nbatts/2] = hattname.hashValue;
	    atts[nbatts++] = attname;
	    atts[nbatts++] = attvalue;

            attvalue = NULL;
	}

failed:
        if (attvalue != NULL)
            xmlFree(attvalue);

	SKIP_BLANKS;
    }

    if (ctxt->input->cur >= ctxt->input->end) {
        discardtag = 1;
        goto done;
    }

    /*
     * Verify that attribute names are unique.
     */
    if (nbatts > 2) {
        unsigned attrHashSize;
        int j, k;

        attrHashSize = 4;
        while (attrHashSize / 2 < (unsigned) nbatts / 2)
            attrHashSize *= 2;

        if (attrHashSize > ctxt->attrHashMax) {
            xmlAttrHashBucket *tmp;

            tmp = xmlRealloc(ctxt->attrHash, attrHashSize * sizeof(tmp[0]));
            if (tmp == NULL) {
                htmlErrMemory(ctxt);
                goto done;
            }

            ctxt->attrHash = tmp;
            ctxt->attrHashMax = attrHashSize;
        }

        memset(ctxt->attrHash, -1, attrHashSize * sizeof(ctxt->attrHash[0]));

        for (i = 0, j = 0, k = 0; i < nbatts; i += 2, k++) {
            unsigned hashValue;
            int res;

            attname = atts[i];
            hashValue = ctxt->attallocs[k] | 0x80000000;

            res = htmlAttrHashInsert(ctxt, attrHashSize, attname,
                                    hashValue, j);
            if (res < 0)
                continue;

            if (res == INT_MAX) {
                atts[j] = atts[i];
                atts[j+1] = atts[i+1];
                j += 2;
            } else {
                xmlFree((xmlChar *) atts[i+1]);
            }
        }

        nbatts = j;
    }

    if (nbatts > 0) {
        atts[nbatts] = NULL;
        atts[nbatts + 1] = NULL;

    /*
     * Apple's new libiconv is so broken that you routinely run into
     * issues when fuzz testing (by accident with an uninstrumented
     * libiconv). Here's a harmless (?) example:
     *
     * printf '>'             | iconv -f shift_jis -t utf-8 | hexdump -C
     * printf '\xfc\x00\x00'  | iconv -f shift_jis -t utf-8 | hexdump -C
     * printf '>\xfc\x00\x00' | iconv -f shift_jis -t utf-8 | hexdump -C
     *
     * The last command fails to detect the illegal sequence.
     */
#if !defined(__APPLE__) || \
    !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
        /*
         * Handle specific association to the META tag
         */
        if (meta)
            htmlCheckMeta(ctxt, atts);
#endif
    }

    /*
     * SAX: Start of Element !
     */
    if (!discardtag) {
        if (ctxt->options & HTML_PARSE_HTML5) {
            if (ctxt->nameNr > 0)
                htmlnamePop(ctxt);
        }

	htmlnamePush(ctxt, name);
	if ((ctxt->sax != NULL) && (ctxt->sax->startElement != NULL)) {
	    if (nbatts != 0)
		ctxt->sax->startElement(ctxt->userData, name, atts);
	    else
		ctxt->sax->startElement(ctxt->userData, name, NULL);
	}
    }

done:
    if (atts != NULL) {
        for (i = 1;i < nbatts;i += 2) {
	    if (atts[i] != NULL)
		xmlFree((xmlChar *) atts[i]);
	}
    }
}

/**
 * htmlParseEndTag:
 * @ctxt:  an HTML parser context
 *
 * parse an end of tag
 *
 * [42] ETag ::= '</' Name S? '>'
 *
 * With namespace
 *
 * [NS 9] ETag ::= '</' QName S? '>'
 *
 * Returns 1 if the current level should be closed.
 */

static void
htmlParseEndTag(htmlParserCtxtPtr ctxt)
{
    const xmlChar *name;
    const xmlChar *oldname;
    int i;

    ctxt->endCheckState = 0;

    SKIP(2);

    if (ctxt->input->cur >= ctxt->input->end) {
        htmlStartCharData(ctxt);
        if ((ctxt->sax != NULL) && (!ctxt->disableSAX) &&
            (ctxt->sax->characters != NULL))
            ctxt->sax->characters(ctxt->userData,
                                  BAD_CAST "</", 2);
        return;
    }

    if (CUR == '>') {
        SKIP(1);
        return;
    }

    if (!IS_ASCII_LETTER(CUR)) {
        htmlParseComment(ctxt, /* bogus */ 1);
        return;
    }

    name = htmlParseHTMLName(ctxt, 0).name;
    if (name == NULL)
        return;

    /*
     * Parse and ignore attributes.
     */
    SKIP_BLANKS;
    while ((ctxt->input->cur < ctxt->input->end) &&
           (CUR != '>') &&
	   ((CUR != '/') || (NXT(1) != '>')) &&
           (ctxt->instate != XML_PARSER_EOF)) {
        xmlChar *attvalue = NULL;

        /*  unexpected-solidus-in-tag */
        if (CUR == '/') {
            SKIP(1);
            SKIP_BLANKS;
            continue;
        }
	GROW;
	htmlParseAttribute(ctxt, &attvalue);
        if (attvalue != NULL)
            xmlFree(attvalue);

	SKIP_BLANKS;
    }

    if (CUR == '>') {
        SKIP(1);
    } else if ((CUR == '/') && (NXT(1) == '>')) {
        SKIP(2);
    } else {
        return;
    }

    if (ctxt->options & HTML_PARSE_HTML5) {
        if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
            ctxt->sax->endElement(ctxt->userData, name);
        return;
    }

    /*
     * if we ignored misplaced tags in htmlParseStartTag don't pop them
     * out now.
     */
    if ((ctxt->depth > 0) &&
        (xmlStrEqual(name, BAD_CAST "html") ||
         xmlStrEqual(name, BAD_CAST "body") ||
	 xmlStrEqual(name, BAD_CAST "head"))) {
	ctxt->depth--;
	return;
    }

    /*
     * If the name read is not one of the element in the parsing stack
     * then return, it's just an error.
     */
    for (i = (ctxt->nameNr - 1); i >= 0; i--) {
        if (xmlStrEqual(name, ctxt->nameTab[i]))
            break;
    }
    if (i < 0) {
        htmlParseErr(ctxt, XML_ERR_TAG_NAME_MISMATCH,
	             "Unexpected end tag : %s\n", name, NULL);
        return;
    }


    /*
     * Check for auto-closure of HTML elements.
     */

    htmlAutoCloseOnClose(ctxt, name);

    /*
     * Well formedness constraints, opening and closing must match.
     * With the exception that the autoclose may have popped stuff out
     * of the stack.
     */
    if ((ctxt->name != NULL) && (!xmlStrEqual(ctxt->name, name))) {
        htmlParseErr(ctxt, XML_ERR_TAG_NAME_MISMATCH,
                     "Opening and ending tag mismatch: %s and %s\n",
                     name, ctxt->name);
    }

    /*
     * SAX: End of Tag
     */
    oldname = ctxt->name;
    if ((oldname != NULL) && (xmlStrEqual(oldname, name))) {
	htmlParserFinishElementParsing(ctxt);
        if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
            ctxt->sax->endElement(ctxt->userData, name);
        htmlnamePop(ctxt);
    }
}

/**
 * htmlParseContent:
 * @ctxt:  an HTML parser context
 *
 * Parse a content: comment, sub-element, reference or text.
 * New version for non recursive htmlParseElementInternal
 */

static void
htmlParseContent(htmlParserCtxtPtr ctxt) {
    GROW;

    while ((PARSER_STOPPED(ctxt) == 0) &&
           (ctxt->input->cur < ctxt->input->end)) {
        int mode;

        mode = ctxt->endCheckState;

        if ((mode == 0) && (CUR == '<')) {
            if (NXT(1) == '/') {
	        htmlParseEndTag(ctxt);
            } else if (NXT(1) == '!') {
                /*
                 * Sometimes DOCTYPE arrives in the middle of the document
                 */
                if ((UPP(2) == 'D') && (UPP(3) == 'O') &&
                    (UPP(4) == 'C') && (UPP(5) == 'T') &&
                    (UPP(6) == 'Y') && (UPP(7) == 'P') &&
                    (UPP(8) == 'E')) {
                    htmlParseDocTypeDecl(ctxt);
                } else if ((NXT(2) == '-') && (NXT(3) == '-')) {
                    SKIP(4);
                    htmlParseComment(ctxt, /* bogus */ 0);
                } else {
                    SKIP(2);
                    htmlParseComment(ctxt, /* bogus */ 1);
                }
            } else if (NXT(1) == '?') {
                SKIP(1);
                htmlParseComment(ctxt, /* bogus */ 1);
            } else if (IS_ASCII_LETTER(NXT(1))) {
                htmlParseElementInternal(ctxt);
            } else {
                htmlStartCharData(ctxt);
                if ((ctxt->sax != NULL) && (!ctxt->disableSAX) &&
                    (ctxt->sax->characters != NULL))
                    ctxt->sax->characters(ctxt->userData, BAD_CAST "<", 1);
                SKIP(1);
            }
        } else {
            htmlParseCharData(ctxt, /* partial */ 0);
        }

        SHRINK;
        GROW;
    }

    if (ctxt->input->cur >= ctxt->input->end)
        htmlAutoCloseOnEnd(ctxt);
}

/**
 * htmlParseElementInternal:
 * @ctxt:  an HTML parser context
 *
 * parse an HTML element, new version, non recursive
 *
 * [39] element ::= EmptyElemTag | STag content ETag
 *
 * [41] Attribute ::= Name Eq AttValue
 */

static int
htmlParseElementInternal(htmlParserCtxtPtr ctxt) {
    const xmlChar *name;
    const htmlElemDesc * info;
    htmlParserNodeInfo node_info = { NULL, 0, 0, 0, 0 };

    if ((ctxt == NULL) || (ctxt->input == NULL))
	return(0);

    /* Capture start position */
    if (ctxt->record_info) {
        node_info.begin_pos = ctxt->input->consumed +
                          (CUR_PTR - ctxt->input->base);
	node_info.begin_line = ctxt->input->line;
    }

    htmlParseStartTag(ctxt);
    name = ctxt->name;
    if (name == NULL)
        return(0);

    if (ctxt->record_info)
        htmlNodeInfoPush(ctxt, &node_info);

    /*
     * Check for an Empty Element labeled the XML/SGML way
     */
    if ((CUR == '/') && (NXT(1) == '>')) {
        SKIP(2);
        htmlParserFinishElementParsing(ctxt);
        if ((ctxt->options & HTML_PARSE_HTML5) == 0) {
            if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
                ctxt->sax->endElement(ctxt->userData, name);
        }
	htmlnamePop(ctxt);
	return(0);
    }

    if (CUR != '>')
        return(0);
    SKIP(1);

    /*
     * Lookup the info for that element.
     */
    info = htmlTagLookup(name);

    /*
     * Check for an Empty Element from DTD definition
     */
    if ((info != NULL) && (info->empty)) {
        htmlParserFinishElementParsing(ctxt);
        if ((ctxt->options & HTML_PARSE_HTML5) == 0) {
            if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
                ctxt->sax->endElement(ctxt->userData, name);
        }
	htmlnamePop(ctxt);
	return(0);
    }

    if (info != NULL)
        ctxt->endCheckState = info->dataMode;

    return(1);
}

/**
 * htmlParseElement:
 * @ctxt:  an HTML parser context
 *
 * DEPRECATED: Internal function, don't use.
 *
 * parse an HTML element, this is highly recursive
 * this is kept for compatibility with previous code versions
 *
 * [39] element ::= EmptyElemTag | STag content ETag
 *
 * [41] Attribute ::= Name Eq AttValue
 */

void
htmlParseElement(htmlParserCtxtPtr ctxt) {
    const xmlChar *oldptr;
    int depth;

    if ((ctxt == NULL) || (ctxt->input == NULL))
	return;

    if (htmlParseElementInternal(ctxt) == 0)
        return;

    /*
     * Parse the content of the element:
     */
    depth = ctxt->nameNr;
    while (CUR != 0) {
	oldptr = ctxt->input->cur;
	htmlParseContent(ctxt);
	if (oldptr==ctxt->input->cur) break;
	if (ctxt->nameNr < depth) break;
    }

    if (CUR == 0) {
	htmlAutoCloseOnEnd(ctxt);
    }
}

/**
 * htmlCtxtParseContentInternal:
 * @ctxt:  parser context
 * @input:  parser input
 *
 * Returns a node list.
 */
xmlNodePtr
htmlCtxtParseContentInternal(htmlParserCtxtPtr ctxt, xmlParserInputPtr input) {
    xmlNodePtr root;
    xmlNodePtr list = NULL;
    xmlChar *rootName = BAD_CAST "#root";

    root = xmlNewDocNode(ctxt->myDoc, NULL, rootName, NULL);
    if (root == NULL) {
        htmlErrMemory(ctxt);
        return(NULL);
    }

    if (xmlCtxtPushInput(ctxt, input) < 0) {
        xmlFreeNode(root);
        return(NULL);
    }

    htmlnamePush(ctxt, rootName);
    nodePush(ctxt, root);

    htmlParseContent(ctxt);

    /*
     * Only check for truncated multi-byte sequences
     */
    xmlParserCheckEOF(ctxt, XML_ERR_INTERNAL_ERROR);

    /* TODO: Use xmlCtxtIsCatastrophicError */
    if (ctxt->errNo != XML_ERR_NO_MEMORY) {
        xmlNodePtr cur;

        /*
         * Unlink newly created node list.
         */
        list = root->children;
        root->children = NULL;
        root->last = NULL;
        for (cur = list; cur != NULL; cur = cur->next)
            cur->parent = NULL;
    }

    nodePop(ctxt);
    htmlnamePop(ctxt);

    xmlCtxtPopInput(ctxt);

    xmlFreeNode(root);
    return(list);
}

/**
 * htmlParseDocument:
 * @ctxt:  an HTML parser context
 *
 * Parse an HTML document and invoke the SAX handlers. This is useful
 * if you're only interested in custom SAX callbacks. If you want a
 * document tree, use htmlCtxtParseDocument.
 *
 * Returns 0, -1 in case of error.
 */

int
htmlParseDocument(htmlParserCtxtPtr ctxt) {
    xmlDtdPtr dtd;

    if ((ctxt == NULL) || (ctxt->input == NULL))
	return(-1);

    if ((ctxt->sax) && (ctxt->sax->setDocumentLocator)) {
        ctxt->sax->setDocumentLocator(ctxt->userData,
                (xmlSAXLocator *) &xmlDefaultSAXLocator);
    }

    xmlDetectEncoding(ctxt);

    /*
     * TODO: Implement HTML5 prescan algorithm
     */

    /*
     * This is wrong but matches long-standing behavior. In most
     * cases, a document starting with an XML declaration will
     * specify UTF-8. The HTML5 prescan algorithm handles
     * XML declarations in a better way.
     */
    if (((ctxt->input->flags & XML_INPUT_HAS_ENCODING) == 0) &&
        (xmlStrncmp(ctxt->input->cur, BAD_CAST "<?xm", 4) == 0))
        xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_UTF8);

    /*
     * Wipe out everything which is before the first '<'
     */
    SKIP_BLANKS;

    if ((ctxt->sax) && (ctxt->sax->startDocument) && (!ctxt->disableSAX))
	ctxt->sax->startDocument(ctxt->userData);

    /*
     * Parse possible comments and PIs before any content
     */
    while (CUR == '<') {
        if ((NXT(1) == '!') && (NXT(2) == '-') && (NXT(3) == '-')) {
            SKIP(4);
            htmlParseComment(ctxt, /* bogus */ 0);
        } else if (NXT(1) == '?') {
            SKIP(1);
            htmlParseComment(ctxt, /* bogus */ 1);
        } else {
            break;
        }
	SKIP_BLANKS;
    }

    /*
     * Then possibly doc type declaration(s) and more Misc
     * (doctypedecl Misc*)?
     */
    if ((CUR == '<') && (NXT(1) == '!') &&
	(UPP(2) == 'D') && (UPP(3) == 'O') &&
	(UPP(4) == 'C') && (UPP(5) == 'T') &&
	(UPP(6) == 'Y') && (UPP(7) == 'P') &&
	(UPP(8) == 'E')) {
        ctxt->instate = XML_PARSER_MISC;
	htmlParseDocTypeDecl(ctxt);
    }
    SKIP_BLANKS;

    /*
     * Parse possible comments and PIs before any content
     */
    ctxt->instate = XML_PARSER_PROLOG;
    while (CUR == '<') {
        if ((NXT(1) == '!') && (NXT(2) == '-') && (NXT(3) == '-')) {
            SKIP(4);
            htmlParseComment(ctxt, /* bogus */ 0);
        } else if (NXT(1) == '?') {
            SKIP(1);
            htmlParseComment(ctxt, /* bogus */ 1);
        } else {
            break;
        }
	SKIP_BLANKS;
    }

    /*
     * Time to start parsing the tree itself
     */
    ctxt->instate = XML_PARSER_CONTENT;
    htmlParseContent(ctxt);

    /*
     * Only check for truncated multi-byte sequences
     */
    xmlParserCheckEOF(ctxt, XML_ERR_INTERNAL_ERROR);

    /*
     * SAX: end of the document processing.
     */
    if ((ctxt->sax) && (ctxt->sax->endDocument != NULL))
        ctxt->sax->endDocument(ctxt->userData);

    if ((!(ctxt->options & HTML_PARSE_NODEFDTD)) && (ctxt->myDoc != NULL)) {
	dtd = xmlGetIntSubset(ctxt->myDoc);
	if (dtd == NULL) {
	    ctxt->myDoc->intSubset =
		xmlCreateIntSubset(ctxt->myDoc, BAD_CAST "html",
		    BAD_CAST "-//W3C//DTD HTML 4.0 Transitional//EN",
		    BAD_CAST "http://www.w3.org/TR/REC-html40/loose.dtd");
            if (ctxt->myDoc->intSubset == NULL)
                htmlErrMemory(ctxt);
        }
    }
    if (! ctxt->wellFormed) return(-1);
    return(0);
}


/************************************************************************
 *									*
 *			Parser contexts handling			*
 *									*
 ************************************************************************/

/**
 * htmlInitParserCtxt:
 * @ctxt:  an HTML parser context
 * @sax:  SAX handler
 * @userData:  user data
 *
 * Initialize a parser context
 *
 * Returns 0 in case of success and -1 in case of error
 */

static int
htmlInitParserCtxt(htmlParserCtxtPtr ctxt, const htmlSAXHandler *sax,
                   void *userData)
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    size_t initialNodeTabSize = 1;
#else
    size_t initialNodeTabSize = 10;
#endif

    if (ctxt == NULL) return(-1);
    memset(ctxt, 0, sizeof(htmlParserCtxt));

    ctxt->dict = xmlDictCreate();
    if (ctxt->dict == NULL)
	return(-1);

    if (ctxt->sax == NULL)
        ctxt->sax = (htmlSAXHandler *) xmlMalloc(sizeof(htmlSAXHandler));
    if (ctxt->sax == NULL)
	return(-1);
    if (sax == NULL) {
        memset(ctxt->sax, 0, sizeof(htmlSAXHandler));
        xmlSAX2InitHtmlDefaultSAXHandler(ctxt->sax);
        ctxt->userData = ctxt;
    } else {
        memcpy(ctxt->sax, sax, sizeof(htmlSAXHandler));
        ctxt->userData = userData ? userData : ctxt;
    }

    /* Allocate the Input stack */
    ctxt->inputTab = (htmlParserInputPtr *)
                      xmlMalloc(sizeof(htmlParserInputPtr));
    if (ctxt->inputTab == NULL)
	return(-1);
    ctxt->inputNr = 0;
    ctxt->inputMax = 1;
    ctxt->input = NULL;
    ctxt->version = NULL;
    ctxt->encoding = NULL;
    ctxt->standalone = -1;
    ctxt->instate = XML_PARSER_START;

    /* Allocate the Node stack */
    ctxt->nodeTab = xmlMalloc(initialNodeTabSize * sizeof(htmlNodePtr));
    if (ctxt->nodeTab == NULL)
	return(-1);
    ctxt->nodeNr = 0;
    ctxt->nodeMax = initialNodeTabSize;
    ctxt->node = NULL;

    /* Allocate the Name stack */
    ctxt->nameTab = xmlMalloc(initialNodeTabSize * sizeof(xmlChar *));
    if (ctxt->nameTab == NULL)
	return(-1);
    ctxt->nameNr = 0;
    ctxt->nameMax = initialNodeTabSize;
    ctxt->name = NULL;

    ctxt->nodeInfoTab = NULL;
    ctxt->nodeInfoNr  = 0;
    ctxt->nodeInfoMax = 0;

    ctxt->myDoc = NULL;
    ctxt->wellFormed = 1;
    ctxt->replaceEntities = 0;
    ctxt->linenumbers = xmlLineNumbersDefaultValue;
    ctxt->keepBlanks = xmlKeepBlanksDefaultValue;
    ctxt->html = INSERT_INITIAL;
    ctxt->vctxt.flags = XML_VCTXT_USE_PCTXT;
    ctxt->vctxt.userData = ctxt;
    ctxt->vctxt.error = xmlParserValidityError;
    ctxt->vctxt.warning = xmlParserValidityWarning;
    ctxt->record_info = 0;
    ctxt->validate = 0;
    ctxt->checkIndex = 0;
    ctxt->catalogs = NULL;
    xmlInitNodeInfoSeq(&ctxt->node_seq);
    return(0);
}

/**
 * htmlFreeParserCtxt:
 * @ctxt:  an HTML parser context
 *
 * Free all the memory used by a parser context. However the parsed
 * document in ctxt->myDoc is not freed.
 */

void
htmlFreeParserCtxt(htmlParserCtxtPtr ctxt)
{
    xmlFreeParserCtxt(ctxt);
}

/**
 * htmlNewParserCtxt:
 *
 * Allocate and initialize a new HTML parser context.
 *
 * This can be used to parse HTML documents into DOM trees with
 * functions like xmlCtxtReadFile or xmlCtxtReadMemory.
 *
 * See htmlCtxtUseOptions for parser options.
 *
 * See xmlCtxtSetErrorHandler for advanced error handling.
 *
 * See htmlNewSAXParserCtxt for custom SAX parsers.
 *
 * Returns the htmlParserCtxtPtr or NULL in case of allocation error
 */

htmlParserCtxtPtr
htmlNewParserCtxt(void)
{
    return(htmlNewSAXParserCtxt(NULL, NULL));
}

/**
 * htmlNewSAXParserCtxt:
 * @sax:  SAX handler
 * @userData:  user data
 *
 * Allocate and initialize a new HTML SAX parser context. If userData
 * is NULL, the parser context will be passed as user data.
 *
 * Available since 2.11.0. If you want support older versions,
 * it's best to invoke htmlNewParserCtxt and set ctxt->sax with
 * struct assignment.
 *
 * Also see htmlNewParserCtxt.
 *
 * Returns the htmlParserCtxtPtr or NULL in case of allocation error
 */

htmlParserCtxtPtr
htmlNewSAXParserCtxt(const htmlSAXHandler *sax, void *userData)
{
    xmlParserCtxtPtr ctxt;

    xmlInitParser();

    ctxt = (xmlParserCtxtPtr) xmlMalloc(sizeof(xmlParserCtxt));
    if (ctxt == NULL)
	return(NULL);
    memset(ctxt, 0, sizeof(xmlParserCtxt));
    if (htmlInitParserCtxt(ctxt, sax, userData) < 0) {
        htmlFreeParserCtxt(ctxt);
	return(NULL);
    }
    return(ctxt);
}

static htmlParserCtxtPtr
htmlCreateMemoryParserCtxtInternal(const char *url,
                                   const char *buffer, size_t size,
                                   const char *encoding) {
    xmlParserCtxtPtr ctxt;
    xmlParserInputPtr input;

    if (buffer == NULL)
	return(NULL);

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
	return(NULL);

    input = xmlCtxtNewInputFromMemory(ctxt, url, buffer, size, encoding, 0);
    if (input == NULL) {
	xmlFreeParserCtxt(ctxt);
        return(NULL);
    }

    if (xmlCtxtPushInput(ctxt, input) < 0) {
        xmlFreeInputStream(input);
        xmlFreeParserCtxt(ctxt);
        return(NULL);
    }

    return(ctxt);
}

/**
 * htmlCreateMemoryParserCtxt:
 * @buffer:  a pointer to a char array
 * @size:  the size of the array
 *
 * DEPRECATED: Use htmlNewParserCtxt and htmlCtxtReadMemory.
 *
 * Create a parser context for an HTML in-memory document. The input
 * buffer must not contain any terminating null bytes.
 *
 * Returns the new parser context or NULL
 */
htmlParserCtxtPtr
htmlCreateMemoryParserCtxt(const char *buffer, int size) {
    if (size <= 0)
	return(NULL);

    return(htmlCreateMemoryParserCtxtInternal(NULL, buffer, size, NULL));
}

/**
 * htmlCreateDocParserCtxt:
 * @str:  a pointer to an array of xmlChar
 * @encoding:  encoding (optional)
 *
 * Create a parser context for a null-terminated string.
 *
 * Returns the new parser context or NULL if a memory allocation failed.
 */
static htmlParserCtxtPtr
htmlCreateDocParserCtxt(const xmlChar *str, const char *url,
                        const char *encoding) {
    xmlParserCtxtPtr ctxt;
    xmlParserInputPtr input;

    if (str == NULL)
	return(NULL);

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
	return(NULL);

    input = xmlCtxtNewInputFromString(ctxt, url, (const char *) str,
                                      encoding, 0);
    if (input == NULL) {
	xmlFreeParserCtxt(ctxt);
	return(NULL);
    }

    if (xmlCtxtPushInput(ctxt, input) < 0) {
        xmlFreeInputStream(input);
        xmlFreeParserCtxt(ctxt);
        return(NULL);
    }

    return(ctxt);
}

#ifdef LIBXML_PUSH_ENABLED
/************************************************************************
 *									*
 *	Progressive parsing interfaces				*
 *									*
 ************************************************************************/

typedef enum {
    LSTATE_TAG_NAME = 0,
    LSTATE_BEFORE_ATTR_NAME,
    LSTATE_ATTR_NAME,
    LSTATE_AFTER_ATTR_NAME,
    LSTATE_BEFORE_ATTR_VALUE,
    LSTATE_ATTR_VALUE_DQUOTED,
    LSTATE_ATTR_VALUE_SQUOTED,
    LSTATE_ATTR_VALUE_UNQUOTED
} xmlLookupStates;

/**
 * htmlParseLookupGt:
 * @ctxt:  an HTML parser context
 *
 * Check whether there's enough data in the input buffer to finish parsing
 * a tag. This has to take quotes into account.
 */
static int
htmlParseLookupGt(xmlParserCtxtPtr ctxt) {
    const xmlChar *cur;
    const xmlChar *end = ctxt->input->end;
    int state = ctxt->endCheckState;
    size_t index;

    if (ctxt->checkIndex == 0)
        cur = ctxt->input->cur + 2; /* Skip '<a' or '</' */
    else
        cur = ctxt->input->cur + ctxt->checkIndex;

    while (cur < end) {
        int c = *cur++;

        if (state != LSTATE_ATTR_VALUE_SQUOTED &&
            state != LSTATE_ATTR_VALUE_DQUOTED) {
            if (c == '/' &&
                state != LSTATE_BEFORE_ATTR_VALUE &&
                state != LSTATE_ATTR_VALUE_UNQUOTED) {
                state = LSTATE_BEFORE_ATTR_NAME;
                continue;
            } else if (c == '>') {
                ctxt->checkIndex = 0;
                ctxt->endCheckState = 0;
                return(0);
            }
        }

        switch (state) {
            case LSTATE_TAG_NAME:
                if (IS_WS_HTML(c))
                    state = LSTATE_BEFORE_ATTR_NAME;
                break;

            case LSTATE_BEFORE_ATTR_NAME:
                if (!IS_WS_HTML(c))
                    state = LSTATE_ATTR_NAME;
                break;

            case LSTATE_ATTR_NAME:
                if (c == '=')
                    state = LSTATE_BEFORE_ATTR_VALUE;
                else if (IS_WS_HTML(c))
                    state = LSTATE_AFTER_ATTR_NAME;
                break;

            case LSTATE_AFTER_ATTR_NAME:
                if (c == '=')
                    state = LSTATE_BEFORE_ATTR_VALUE;
                else if (!IS_WS_HTML(c))
                    state = LSTATE_ATTR_NAME;
                break;

            case LSTATE_BEFORE_ATTR_VALUE:
                if (c == '"')
                    state = LSTATE_ATTR_VALUE_DQUOTED;
                else if (c == '\'')
                    state = LSTATE_ATTR_VALUE_SQUOTED;
                else if (!IS_WS_HTML(c))
                    state = LSTATE_ATTR_VALUE_UNQUOTED;
                break;

            case LSTATE_ATTR_VALUE_DQUOTED:
                if (c == '"')
                    state = LSTATE_BEFORE_ATTR_NAME;
                break;

            case LSTATE_ATTR_VALUE_SQUOTED:
                if (c == '\'')
                    state = LSTATE_BEFORE_ATTR_NAME;
                break;

            case LSTATE_ATTR_VALUE_UNQUOTED:
                if (IS_WS_HTML(c))
                    state = LSTATE_BEFORE_ATTR_NAME;
                break;
        }
    }

    index = cur - ctxt->input->cur;
    if (index > LONG_MAX) {
        ctxt->checkIndex = 0;
        ctxt->endCheckState = 0;
        return(0);
    }
    ctxt->checkIndex = index;
    ctxt->endCheckState = state;
    return(-1);
}

/**
 * htmlParseLookupString:
 * @ctxt:  an XML parser context
 * @startDelta: delta to apply at the start
 * @str:  string
 * @strLen:  length of string
 *
 * Check whether the input buffer contains a string.
 */
static int
htmlParseLookupString(xmlParserCtxtPtr ctxt, size_t startDelta,
                      const char *str, size_t strLen, size_t extraLen) {
    const xmlChar *end = ctxt->input->end;
    const xmlChar *cur, *term;
    size_t index, rescan;
    int ret;

    if (ctxt->checkIndex == 0) {
        cur = ctxt->input->cur + startDelta;
    } else {
        cur = ctxt->input->cur + ctxt->checkIndex;
    }

    term = BAD_CAST strstr((const char *) cur, str);
    if ((term != NULL) &&
        ((size_t) (ctxt->input->end - term) >= extraLen + 1)) {
        ctxt->checkIndex = 0;

        if (term - ctxt->input->cur > INT_MAX / 2)
            ret = INT_MAX / 2;
        else
            ret = term - ctxt->input->cur;

        return(ret);
    }

    /* Rescan (strLen + extraLen - 1) characters. */
    rescan = strLen + extraLen - 1;
    if ((size_t) (end - cur) <= rescan)
        end = cur;
    else
        end -= rescan;
    index = end - ctxt->input->cur;
    if (index > INT_MAX / 2) {
        ctxt->checkIndex = 0;
        ret = INT_MAX / 2;
    } else {
        ctxt->checkIndex = index;
        ret = -1;
    }

    return(ret);
}

/**
 * htmlParseLookupCommentEnd:
 * @ctxt: an HTML parser context
 *
 * Try to find a comment end tag in the input stream
 * The search includes "-->" as well as WHATWG-recommended incorrectly-closed tags.
 * (See https://html.spec.whatwg.org/multipage/parsing.html#parse-error-incorrectly-closed-comment)
 * This function has a side effect of (possibly) incrementing ctxt->checkIndex
 * to avoid rescanning sequences of bytes, it DOES change the state of the
 * parser, do not use liberally.
 *
 * Returns the index to the current parsing point if the full sequence is available, -1 otherwise.
 */
static int
htmlParseLookupCommentEnd(htmlParserCtxtPtr ctxt)
{
    int mark = 0;
    int offset;

    while (1) {
	mark = htmlParseLookupString(ctxt, 2, "--", 2, 0);
	if (mark < 0)
            break;
        /*
         * <!-->    is a complete comment, but
         * <!--!>   is not
         * <!---!>  is not
         * <!----!> is
         */
        if ((NXT(mark+2) == '>') ||
	    ((mark >= 4) && (NXT(mark+2) == '!') && (NXT(mark+3) == '>'))) {
            ctxt->checkIndex = 0;
	    break;
	}
        offset = (NXT(mark+2) == '!') ? 3 : 2;
        if (mark + offset >= ctxt->input->end - ctxt->input->cur) {
	    ctxt->checkIndex = mark;
            return(-1);
        }
	ctxt->checkIndex = mark + 1;
    }
    return mark;
}


/**
 * htmlParseTryOrFinish:
 * @ctxt:  an HTML parser context
 * @terminate:  last chunk indicator
 *
 * Try to progress on parsing
 *
 * Returns zero if no parsing was possible
 */
static void
htmlParseTryOrFinish(htmlParserCtxtPtr ctxt, int terminate) {
    while (PARSER_STOPPED(ctxt) == 0) {
        htmlParserInputPtr in;
        size_t avail;

	in = ctxt->input;
	if (in == NULL) break;
	avail = in->end - in->cur;

        switch (ctxt->instate) {
            case XML_PARSER_EOF:
	        /*
		 * Document parsing is done !
		 */
	        return;

            case XML_PARSER_START:
                /*
                 * Very first chars read from the document flow.
                 */
                if ((!terminate) && (avail < 4))
                    return;

                xmlDetectEncoding(ctxt);

                /*
                 * TODO: Implement HTML5 prescan algorithm
                 */

                /*
                 * This is wrong but matches long-standing behavior. In most
                 * cases, a document starting with an XML declaration will
                 * specify UTF-8. The HTML5 prescan algorithm handles
                 * XML declarations in a better way.
                 */
                if (((ctxt->input->flags & XML_INPUT_HAS_ENCODING) == 0) &&
                    (xmlStrncmp(ctxt->input->cur, BAD_CAST "<?xm", 4) == 0)) {
                    xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_UTF8);
                }

                /* fall through */

            case XML_PARSER_XML_DECL:
                if ((ctxt->sax) && (ctxt->sax->setDocumentLocator)) {
                    ctxt->sax->setDocumentLocator(ctxt->userData,
                            (xmlSAXLocator *) &xmlDefaultSAXLocator);
                }
		if ((ctxt->sax) && (ctxt->sax->startDocument) &&
	            (!ctxt->disableSAX))
		    ctxt->sax->startDocument(ctxt->userData);

                /* Allow callback to modify state for tests */
                if ((ctxt->instate == XML_PARSER_START) ||
                    (ctxt->instate == XML_PARSER_XML_DECL))
                    ctxt->instate = XML_PARSER_MISC;
		break;

            case XML_PARSER_START_TAG:
		if ((!terminate) &&
		    (htmlParseLookupGt(ctxt) < 0))
		    return;

                htmlParseElementInternal(ctxt);

		ctxt->instate = XML_PARSER_CONTENT;
                break;

            case XML_PARSER_MISC: /* initial */
            case XML_PARSER_PROLOG: /* before html */
            case XML_PARSER_CONTENT: {
                int mode;

                if ((ctxt->instate == XML_PARSER_MISC) ||
                    (ctxt->instate == XML_PARSER_PROLOG)) {
                    SKIP_BLANKS;
                    avail = in->end - in->cur;
                }

		if (avail < 1)
		    return;
                /*
                 * Note that endCheckState is also used by
                 * xmlParseLookupGt.
                 */
                mode = ctxt->endCheckState;

                if (mode != 0) {
                    if (htmlParseCharData(ctxt, !terminate) == 0)
                        return;
		} else if (in->cur[0] == '<') {
                    int next;

                    if (avail < 2) {
                        if (!terminate)
                            return;
                        next = ' ';
                    } else {
                        next = in->cur[1];
                    }

                    if (next == '!') {
                        if ((!terminate) && (avail < 4))
                            return;
                        if ((in->cur[2] == '-') && (in->cur[3] == '-')) {
                            if ((!terminate) &&
                                (htmlParseLookupCommentEnd(ctxt) < 0))
                                return;
                            SKIP(4);
                            htmlParseComment(ctxt, /* bogus */ 0);
                            /* don't change state */
                            break;
                        }

                        if ((!terminate) && (avail < 9))
                            return;
                        if ((UPP(2) == 'D') && (UPP(3) == 'O') &&
                            (UPP(4) == 'C') && (UPP(5) == 'T') &&
                            (UPP(6) == 'Y') && (UPP(7) == 'P') &&
                            (UPP(8) == 'E')) {
                            if ((!terminate) &&
                                (htmlParseLookupString(ctxt, 9, ">", 1,
                                                       0) < 0))
                                return;
                            htmlParseDocTypeDecl(ctxt);
                            if (ctxt->instate == XML_PARSER_MISC)
                                ctxt->instate = XML_PARSER_PROLOG;
                            else
                                ctxt->instate = XML_PARSER_CONTENT;
                        } else {
                            ctxt->instate = XML_PARSER_CONTENT;
                            if ((!terminate) &&
                                (htmlParseLookupString(ctxt, 2, ">", 1, 0) < 0))
                                return;
                            SKIP(2);
                            htmlParseComment(ctxt, /* bogus */ 1);
                        }
                    } else if (next == '?') {
                        if ((!terminate) &&
                            (htmlParseLookupString(ctxt, 2, ">", 1, 0) < 0))
                            return;
                        SKIP(1);
                        htmlParseComment(ctxt, /* bogus */ 1);
                        /* don't change state */
                    } else if (next == '/') {
                        ctxt->instate = XML_PARSER_END_TAG;
                        ctxt->checkIndex = 0;
                    } else if (IS_ASCII_LETTER(next)) {
                        ctxt->instate = XML_PARSER_START_TAG;
                        ctxt->checkIndex = 0;
                    } else {
                        ctxt->instate = XML_PARSER_CONTENT;
                        htmlStartCharData(ctxt);
                        if ((ctxt->sax != NULL) && (!ctxt->disableSAX) &&
                            (ctxt->sax->characters != NULL))
                            ctxt->sax->characters(ctxt->userData,
                                                  BAD_CAST "<", 1);
                        SKIP(1);
                    }
                } else {
                    ctxt->instate = XML_PARSER_CONTENT;
                    /*
                     * We follow the logic of the XML push parser
                     */
		    if (avail < HTML_PARSER_BIG_BUFFER_SIZE) {
                        if ((!terminate) &&
                            (htmlParseLookupString(ctxt, 0, "<", 1, 0) < 0))
                            return;
                    }
                    ctxt->checkIndex = 0;
                    if (htmlParseCharData(ctxt, !terminate) == 0)
                        return;
		}

		break;
	    }

            case XML_PARSER_END_TAG:
		if ((!terminate) &&
		    (htmlParseLookupGt(ctxt) < 0))
		    return;
		htmlParseEndTag(ctxt);
		ctxt->instate = XML_PARSER_CONTENT;
		ctxt->checkIndex = 0;
	        break;

	    default:
		htmlParseErr(ctxt, XML_ERR_INTERNAL_ERROR,
			     "HPP: internal error\n", NULL, NULL);
		ctxt->instate = XML_PARSER_EOF;
		break;
	}
    }
}

/**
 * htmlParseChunk:
 * @ctxt:  an HTML parser context
 * @chunk:  chunk of memory
 * @size:  size of chunk in bytes
 * @terminate:  last chunk indicator
 *
 * Parse a chunk of memory in push parser mode.
 *
 * Assumes that the parser context was initialized with
 * htmlCreatePushParserCtxt.
 *
 * The last chunk, which will often be empty, must be marked with
 * the @terminate flag. With the default SAX callbacks, the resulting
 * document will be available in ctxt->myDoc. This pointer will not
 * be freed by the library.
 *
 * If the document isn't well-formed, ctxt->myDoc is set to NULL.
 *
 * Returns an xmlParserErrors code (0 on success).
 */
int
htmlParseChunk(htmlParserCtxtPtr ctxt, const char *chunk, int size,
              int terminate) {
    if ((ctxt == NULL) ||
        (ctxt->input == NULL) || (ctxt->input->buf == NULL) ||
        (size < 0) ||
        ((size > 0) && (chunk == NULL)))
	return(XML_ERR_ARGUMENT);
    if (PARSER_STOPPED(ctxt) != 0)
        return(ctxt->errNo);

    if (size > 0)  {
	size_t pos = ctxt->input->cur - ctxt->input->base;
	int res;

	res = xmlParserInputBufferPush(ctxt->input->buf, size, chunk);
        xmlBufUpdateInput(ctxt->input->buf->buffer, ctxt->input, pos);
	if (res < 0) {
            htmlParseErr(ctxt, ctxt->input->buf->error,
                         "xmlParserInputBufferPush failed", NULL, NULL);
            xmlHaltParser(ctxt);
	    return (ctxt->errNo);
	}
    }

    htmlParseTryOrFinish(ctxt, terminate);

    if ((terminate) && (ctxt->instate != XML_PARSER_EOF)) {
        htmlAutoCloseOnEnd(ctxt);

        /*
         * Only check for truncated multi-byte sequences
         */
        xmlParserCheckEOF(ctxt, XML_ERR_INTERNAL_ERROR);

        if ((ctxt->sax) && (ctxt->sax->endDocument != NULL))
            ctxt->sax->endDocument(ctxt->userData);

        if ((!(ctxt->options & HTML_PARSE_NODEFDTD)) &&
            (ctxt->myDoc != NULL)) {
            xmlDtdPtr dtd;
            dtd = xmlGetIntSubset(ctxt->myDoc);
            if (dtd == NULL) {
                ctxt->myDoc->intSubset =
                    xmlCreateIntSubset(ctxt->myDoc, BAD_CAST "html",
                        BAD_CAST "-//W3C//DTD HTML 4.0 Transitional//EN",
                        BAD_CAST "http://www.w3.org/TR/REC-html40/loose.dtd");
                if (ctxt->myDoc->intSubset == NULL)
                    htmlErrMemory(ctxt);
            }
        }

	ctxt->instate = XML_PARSER_EOF;
    }

    return((xmlParserErrors) ctxt->errNo);
}

/************************************************************************
 *									*
 *			User entry points				*
 *									*
 ************************************************************************/

/**
 * htmlCreatePushParserCtxt:
 * @sax:  a SAX handler (optional)
 * @user_data:  The user data returned on SAX callbacks (optional)
 * @chunk:  a pointer to an array of chars (optional)
 * @size:  number of chars in the array
 * @filename:  only used for error reporting (optional)
 * @enc:  encoding (deprecated, pass XML_CHAR_ENCODING_NONE)
 *
 * Create a parser context for using the HTML parser in push mode.
 *
 * Returns the new parser context or NULL if a memory allocation
 * failed.
 */
htmlParserCtxtPtr
htmlCreatePushParserCtxt(htmlSAXHandlerPtr sax, void *user_data,
                         const char *chunk, int size, const char *filename,
			 xmlCharEncoding enc) {
    htmlParserCtxtPtr ctxt;
    htmlParserInputPtr input;
    const char *encoding;

    ctxt = htmlNewSAXParserCtxt(sax, user_data);
    if (ctxt == NULL)
	return(NULL);

    encoding = xmlGetCharEncodingName(enc);
    input = xmlNewPushInput(filename, chunk, size);
    if (input == NULL) {
	htmlFreeParserCtxt(ctxt);
	return(NULL);
    }

    if (xmlCtxtPushInput(ctxt, input) < 0) {
        xmlFreeInputStream(input);
        xmlFreeParserCtxt(ctxt);
        return(NULL);
    }

    if (encoding != NULL)
        xmlSwitchEncodingName(ctxt, encoding);

    return(ctxt);
}
#endif /* LIBXML_PUSH_ENABLED */

/**
 * htmlSAXParseDoc:
 * @cur:  a pointer to an array of xmlChar
 * @encoding:  a free form C string describing the HTML document encoding, or NULL
 * @sax:  the SAX handler block
 * @userData: if using SAX, this pointer will be provided on callbacks.
 *
 * DEPRECATED: Use htmlNewSAXParserCtxt and htmlCtxtReadDoc.
 *
 * Parse an HTML in-memory document. If sax is not NULL, use the SAX callbacks
 * to handle parse events. If sax is NULL, fallback to the default DOM
 * behavior and return a tree.
 *
 * Returns the resulting document tree unless SAX is NULL or the document is
 *     not well formed.
 */

htmlDocPtr
htmlSAXParseDoc(const xmlChar *cur, const char *encoding,
                htmlSAXHandlerPtr sax, void *userData) {
    htmlDocPtr ret;
    htmlParserCtxtPtr ctxt;

    if (cur == NULL)
        return(NULL);

    ctxt = htmlCreateDocParserCtxt(cur, NULL, encoding);
    if (ctxt == NULL)
        return(NULL);

    if (sax != NULL) {
        *ctxt->sax = *sax;
        ctxt->userData = userData;
    }

    htmlParseDocument(ctxt);
    ret = ctxt->myDoc;
    htmlFreeParserCtxt(ctxt);

    return(ret);
}

/**
 * htmlParseDoc:
 * @cur:  a pointer to an array of xmlChar
 * @encoding:  the encoding (optional)
 *
 * DEPRECATED: Use htmlReadDoc.
 *
 * Parse an HTML in-memory document and build a tree.
 *
 * This function uses deprecated global parser options.
 *
 * Returns the resulting document tree
 */

htmlDocPtr
htmlParseDoc(const xmlChar *cur, const char *encoding) {
    return(htmlSAXParseDoc(cur, encoding, NULL, NULL));
}


/**
 * htmlCreateFileParserCtxt:
 * @filename:  the filename
 * @encoding:  optional encoding
 *
 * DEPRECATED: Use htmlNewParserCtxt and htmlCtxtReadFile.
 *
 * Create a parser context to read from a file.
 *
 * A non-NULL encoding overrides encoding declarations in the document.
 *
 * Automatic support for ZLIB/Compress compressed document is provided
 * by default if found at compile-time.
 *
 * Returns the new parser context or NULL if a memory allocation failed.
 */
htmlParserCtxtPtr
htmlCreateFileParserCtxt(const char *filename, const char *encoding)
{
    htmlParserCtxtPtr ctxt;
    htmlParserInputPtr input;

    if (filename == NULL)
        return(NULL);

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL) {
	return(NULL);
    }

    input = xmlCtxtNewInputFromUrl(ctxt, filename, NULL, encoding, 0);
    if (input == NULL) {
	xmlFreeParserCtxt(ctxt);
	return(NULL);
    }
    if (xmlCtxtPushInput(ctxt, input) < 0) {
        xmlFreeInputStream(input);
        xmlFreeParserCtxt(ctxt);
        return(NULL);
    }

    return(ctxt);
}

/**
 * htmlSAXParseFile:
 * @filename:  the filename
 * @encoding:  encoding (optional)
 * @sax:  the SAX handler block
 * @userData: if using SAX, this pointer will be provided on callbacks.
 *
 * DEPRECATED: Use htmlNewSAXParserCtxt and htmlCtxtReadFile.
 *
 * parse an HTML file and build a tree. Automatic support for ZLIB/Compress
 * compressed document is provided by default if found at compile-time.
 * It use the given SAX function block to handle the parsing callback.
 * If sax is NULL, fallback to the default DOM tree building routines.
 *
 * Returns the resulting document tree unless SAX is NULL or the document is
 *     not well formed.
 */

htmlDocPtr
htmlSAXParseFile(const char *filename, const char *encoding, htmlSAXHandlerPtr sax,
                 void *userData) {
    htmlDocPtr ret;
    htmlParserCtxtPtr ctxt;
    htmlSAXHandlerPtr oldsax = NULL;

    ctxt = htmlCreateFileParserCtxt(filename, encoding);
    if (ctxt == NULL) return(NULL);
    if (sax != NULL) {
	oldsax = ctxt->sax;
        ctxt->sax = sax;
        ctxt->userData = userData;
    }

    htmlParseDocument(ctxt);

    ret = ctxt->myDoc;
    if (sax != NULL) {
        ctxt->sax = oldsax;
        ctxt->userData = NULL;
    }
    htmlFreeParserCtxt(ctxt);

    return(ret);
}

/**
 * htmlParseFile:
 * @filename:  the filename
 * @encoding:  encoding (optional)
 *
 * Parse an HTML file and build a tree.
 *
 * Returns the resulting document tree
 */

htmlDocPtr
htmlParseFile(const char *filename, const char *encoding) {
    return(htmlSAXParseFile(filename, encoding, NULL, NULL));
}

/**
 * htmlHandleOmittedElem:
 * @val:  int 0 or 1
 *
 * DEPRECATED: Use HTML_PARSE_NOIMPLIED
 *
 * Set and return the previous value for handling HTML omitted tags.
 *
 * Returns the last value for 0 for no handling, 1 for auto insertion.
 */

int
htmlHandleOmittedElem(int val) {
    int old = htmlOmittedDefaultValue;

    htmlOmittedDefaultValue = val;
    return(old);
}

/**
 * htmlElementAllowedHere:
 * @parent: HTML parent element
 * @elt: HTML element
 *
 * DEPRECATED: Don't use.
 *
 * Returns 1
 */
int
htmlElementAllowedHere(const htmlElemDesc* parent ATTRIBUTE_UNUSED,
                       const xmlChar* elt ATTRIBUTE_UNUSED) {
    return(1);
}

/**
 * htmlElementStatusHere:
 * @parent: HTML parent element
 * @elt: HTML element
 *
 * DEPRECATED: Don't use.
 *
 * Returns HTML_VALID
 */
htmlStatus
htmlElementStatusHere(const htmlElemDesc* parent ATTRIBUTE_UNUSED,
                      const htmlElemDesc* elt ATTRIBUTE_UNUSED) {
    return(HTML_VALID);
}

/**
 * htmlAttrAllowed:
 * @elt: HTML element
 * @attr: HTML attribute
 * @legacy: whether to allow deprecated attributes
 *
 * DEPRECATED: Don't use.
 *
 * Returns HTML_VALID
 */
htmlStatus
htmlAttrAllowed(const htmlElemDesc* elt ATTRIBUTE_UNUSED,
                const xmlChar* attr ATTRIBUTE_UNUSED,
                int legacy ATTRIBUTE_UNUSED) {
    return(HTML_VALID);
}

/**
 * htmlNodeStatus:
 * @node: an htmlNodePtr in a tree
 * @legacy: whether to allow deprecated elements (YES is faster here
 *	for Element nodes)
 *
 * DEPRECATED: Don't use.
 *
 * Returns HTML_VALID
 */
htmlStatus
htmlNodeStatus(htmlNodePtr node ATTRIBUTE_UNUSED,
               int legacy ATTRIBUTE_UNUSED) {
    return(HTML_VALID);
}

/************************************************************************
 *									*
 *	New set (2.6.0) of simpler and more flexible APIs		*
 *									*
 ************************************************************************/
/**
 * DICT_FREE:
 * @str:  a string
 *
 * Free a string if it is not owned by the "dict" dictionary in the
 * current scope
 */
#define DICT_FREE(str)						\
	if ((str) && ((!dict) ||				\
	    (xmlDictOwns(dict, (const xmlChar *)(str)) == 0)))	\
	    xmlFree((char *)(str));

/**
 * htmlCtxtReset:
 * @ctxt: an HTML parser context
 *
 * Reset a parser context
 */
void
htmlCtxtReset(htmlParserCtxtPtr ctxt)
{
    xmlParserInputPtr input;
    xmlDictPtr dict;

    if (ctxt == NULL)
        return;

    dict = ctxt->dict;

    while ((input = xmlCtxtPopInput(ctxt)) != NULL) { /* Non consuming */
        xmlFreeInputStream(input);
    }
    ctxt->inputNr = 0;
    ctxt->input = NULL;

    ctxt->spaceNr = 0;
    if (ctxt->spaceTab != NULL) {
	ctxt->spaceTab[0] = -1;
	ctxt->space = &ctxt->spaceTab[0];
    } else {
	ctxt->space = NULL;
    }


    ctxt->nodeNr = 0;
    ctxt->node = NULL;

    ctxt->nameNr = 0;
    ctxt->name = NULL;

    ctxt->nsNr = 0;

    DICT_FREE(ctxt->version);
    ctxt->version = NULL;
    DICT_FREE(ctxt->encoding);
    ctxt->encoding = NULL;
    DICT_FREE(ctxt->extSubURI);
    ctxt->extSubURI = NULL;
    DICT_FREE(ctxt->extSubSystem);
    ctxt->extSubSystem = NULL;

    if (ctxt->directory != NULL) {
        xmlFree(ctxt->directory);
        ctxt->directory = NULL;
    }

    if (ctxt->myDoc != NULL)
        xmlFreeDoc(ctxt->myDoc);
    ctxt->myDoc = NULL;

    ctxt->standalone = -1;
    ctxt->hasExternalSubset = 0;
    ctxt->hasPErefs = 0;
    ctxt->html = INSERT_INITIAL;
    ctxt->instate = XML_PARSER_START;

    ctxt->wellFormed = 1;
    ctxt->nsWellFormed = 1;
    ctxt->disableSAX = 0;
    ctxt->valid = 1;
    ctxt->vctxt.userData = ctxt;
    ctxt->vctxt.flags = XML_VCTXT_USE_PCTXT;
    ctxt->vctxt.error = xmlParserValidityError;
    ctxt->vctxt.warning = xmlParserValidityWarning;
    ctxt->record_info = 0;
    ctxt->checkIndex = 0;
    ctxt->endCheckState = 0;
    ctxt->inSubset = 0;
    ctxt->errNo = XML_ERR_OK;
    ctxt->depth = 0;
    ctxt->catalogs = NULL;
    xmlInitNodeInfoSeq(&ctxt->node_seq);

    if (ctxt->attsDefault != NULL) {
        xmlHashFree(ctxt->attsDefault, xmlHashDefaultDeallocator);
        ctxt->attsDefault = NULL;
    }
    if (ctxt->attsSpecial != NULL) {
        xmlHashFree(ctxt->attsSpecial, NULL);
        ctxt->attsSpecial = NULL;
    }

    ctxt->nbErrors = 0;
    ctxt->nbWarnings = 0;
    if (ctxt->lastError.code != XML_ERR_OK)
        xmlResetError(&ctxt->lastError);
}

static int
htmlCtxtSetOptionsInternal(xmlParserCtxtPtr ctxt, int options, int keepMask)
{
    int allMask;

    if (ctxt == NULL)
        return(-1);

    allMask = HTML_PARSE_RECOVER |
              HTML_PARSE_HTML5 |
              HTML_PARSE_NODEFDTD |
              HTML_PARSE_NOERROR |
              HTML_PARSE_NOWARNING |
              HTML_PARSE_PEDANTIC |
              HTML_PARSE_NOBLANKS |
              HTML_PARSE_NONET |
              HTML_PARSE_NOIMPLIED |
              HTML_PARSE_COMPACT |
              HTML_PARSE_HUGE |
              HTML_PARSE_IGNORE_ENC |
              HTML_PARSE_BIG_LINES;

    ctxt->options = (ctxt->options & keepMask) | (options & allMask);

    /*
     * For some options, struct members are historically the source
     * of truth. See xmlCtxtSetOptionsInternal.
     */
    ctxt->keepBlanks = (options & HTML_PARSE_NOBLANKS) ? 0 : 1;

    /*
     * Recover from character encoding errors
     */
    ctxt->recovery = 1;

    /*
     * Changing SAX callbacks is a bad idea. This should be fixed.
     */
    if (options & HTML_PARSE_NOBLANKS) {
        ctxt->sax->ignorableWhitespace = xmlSAX2IgnorableWhitespace;
    }
    if (options & HTML_PARSE_HUGE) {
        if (ctxt->dict != NULL)
            xmlDictSetLimit(ctxt->dict, 0);
    }

    /*
     * It would be useful to allow this feature.
     */
    ctxt->dictNames = 0;

    ctxt->linenumbers = 1;

    /*
     * Allow XML_PARSE_NOENT which many users set on the HTML parser.
     */
    return(options & ~allMask & ~XML_PARSE_NOENT);
}

/**
 * htmlCtxtSetOptions:
 * @ctxt: an HTML parser context
 * @options:  a bitmask of xmlParserOption values
 *
 * Applies the options to the parser context. Unset options are
 * cleared.
 *
 * Available since 2.14.0. With older versions, you can use
 * htmlCtxtUseOptions.
 *
 * HTML_PARSE_RECOVER
 *
 * No effect as of 2.14.0.
 *
 * HTML_PARSE_HTML5
 *
 * Make the tokenizer emit a SAX callback for each token. This results
 * in unbalanced invocations of startElement and endElement.
 *
 * For now, this is only usable with custom SAX callbacks.
 *
 * HTML_PARSE_NODEFDTD
 *
 * Do not default to a doctype if none was found.
 *
 * HTML_PARSE_NOERROR
 *
 * Disable error and warning reports to the error handlers.
 * Errors are still accessible with xmlCtxtGetLastError.
 *
 * HTML_PARSE_NOWARNING
 *
 * Disable warning reports.
 *
 * HTML_PARSE_PEDANTIC
 *
 * No effect.
 *
 * HTML_PARSE_NOBLANKS
 *
 * Remove some text nodes containing only whitespace from the
 * result document. Which nodes are removed depends on a conservative
 * heuristic. The reindenting feature of the serialization code relies
 * on this option to be set when parsing. Use of this option is
 * DISCOURAGED.
 *
 * HTML_PARSE_NONET
 *
 * No effect.
 *
 * HTML_PARSE_NOIMPLIED
 *
 * Do not add implied html, head or body elements.
 *
 * HTML_PARSE_COMPACT
 *
 * Store small strings directly in the node struct to save
 * memory.
 *
 * HTML_PARSE_HUGE
 *
 * Relax some internal limits.
 *
 * Available since 2.14.0. Use XML_PARSE_HUGE works with older
 * versions.
 *
 * Maximum size of text nodes, tags, comments, CDATA sections
 *
 * normal: 10M
 * huge:    1B
 *
 * Maximum size of names, system literals, pubid literals
 *
 * normal: 50K
 * huge:   10M
 *
 * Maximum nesting depth of elements
 *
 * normal:  256
 * huge:   2048
 *
 * HTML_PARSE_IGNORE_ENC
 *
 * Ignore the encoding in the HTML declaration. This option is
 * mostly unneeded these days. The only effect is to enforce
 * UTF-8 decoding of ASCII-like data.
 *
 * HTML_PARSE_BIG_LINES
 *
 * Enable reporting of line numbers larger than 65535.
 *
 * Available since 2.14.0.
 *
 * Returns 0 in case of success, the set of unknown or unimplemented options
 *         in case of error.
 */
int
htmlCtxtSetOptions(xmlParserCtxtPtr ctxt, int options)
{
    return(htmlCtxtSetOptionsInternal(ctxt, options, 0));
}

/**
 * htmlCtxtUseOptions:
 * @ctxt: an HTML parser context
 * @options:  a combination of htmlParserOption(s)
 *
 * DEPRECATED: Use htmlCtxtSetOptions.
 *
 * Applies the options to the parser context. The following options
 * are never cleared and can only be enabled:
 *
 * HTML_PARSE_NODEFDTD
 * HTML_PARSE_NOERROR
 * HTML_PARSE_NOWARNING
 * HTML_PARSE_NOIMPLIED
 * HTML_PARSE_COMPACT
 * HTML_PARSE_HUGE
 * HTML_PARSE_IGNORE_ENC
 * HTML_PARSE_BIG_LINES
 *
 * Returns 0 in case of success, the set of unknown or unimplemented options
 *         in case of error.
 */
int
htmlCtxtUseOptions(htmlParserCtxtPtr ctxt, int options)
{
    int keepMask;

    /*
     * For historic reasons, some options can only be enabled.
     */
    keepMask = HTML_PARSE_NODEFDTD |
               HTML_PARSE_NOERROR |
               HTML_PARSE_NOWARNING |
               HTML_PARSE_NOIMPLIED |
               HTML_PARSE_COMPACT |
               HTML_PARSE_HUGE |
               HTML_PARSE_IGNORE_ENC |
               HTML_PARSE_BIG_LINES;

    return(htmlCtxtSetOptionsInternal(ctxt, options, keepMask));
}

/**
 * htmlCtxtParseDocument:
 * @ctxt:  an HTML parser context
 * @input:  parser input
 *
 * Parse an HTML document and return the resulting document tree.
 *
 * Available since 2.13.0.
 *
 * Returns the resulting document tree or NULL
 */
htmlDocPtr
htmlCtxtParseDocument(htmlParserCtxtPtr ctxt, xmlParserInputPtr input)
{
    htmlDocPtr ret;

    if ((ctxt == NULL) || (input == NULL)) {
        xmlFatalErr(ctxt, XML_ERR_ARGUMENT, NULL);
        xmlFreeInputStream(input);
        return(NULL);
    }

    /* assert(ctxt->inputNr == 0); */
    while (ctxt->inputNr > 0)
        xmlFreeInputStream(xmlCtxtPopInput(ctxt));

    if (xmlCtxtPushInput(ctxt, input) < 0) {
        xmlFreeInputStream(input);
        return(NULL);
    }

    ctxt->html = INSERT_INITIAL;
    htmlParseDocument(ctxt);

    ret = xmlCtxtGetDocument(ctxt);

    /* assert(ctxt->inputNr == 1); */
    while (ctxt->inputNr > 0)
        xmlFreeInputStream(xmlCtxtPopInput(ctxt));

    return(ret);
}

/**
 * htmlReadDoc:
 * @str:  a pointer to a zero terminated string
 * @url:  only used for error reporting (optoinal)
 * @encoding:  the document encoding (optional)
 * @options:  a combination of htmlParserOptions
 *
 * Convenience function to parse an HTML document from a zero-terminated
 * string.
 *
 * See htmlCtxtReadDoc for details.
 *
 * Returns the resulting document tree.
 */
htmlDocPtr
htmlReadDoc(const xmlChar *str, const char *url, const char *encoding,
            int options)
{
    htmlParserCtxtPtr ctxt;
    xmlParserInputPtr input;
    htmlDocPtr doc = NULL;

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
        return(NULL);

    htmlCtxtUseOptions(ctxt, options);

    input = xmlCtxtNewInputFromString(ctxt, url, (const char *) str, encoding,
                                      XML_INPUT_BUF_STATIC);

    if (input != NULL)
        doc = htmlCtxtParseDocument(ctxt, input);

    htmlFreeParserCtxt(ctxt);
    return(doc);
}

/**
 * htmlReadFile:
 * @filename:  a file or URL
 * @encoding:  the document encoding (optional)
 * @options:  a combination of htmlParserOptions
 *
 * Convenience function to parse an HTML file from the filesystem,
 * the network or a global user-defined resource loader.
 *
 * See htmlCtxtReadFile for details.
 *
 * Returns the resulting document tree.
 */
htmlDocPtr
htmlReadFile(const char *filename, const char *encoding, int options)
{
    htmlParserCtxtPtr ctxt;
    xmlParserInputPtr input;
    htmlDocPtr doc = NULL;

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
        return(NULL);

    htmlCtxtUseOptions(ctxt, options);

    input = xmlCtxtNewInputFromUrl(ctxt, filename, NULL, encoding, 0);

    if (input != NULL)
        doc = htmlCtxtParseDocument(ctxt, input);

    htmlFreeParserCtxt(ctxt);
    return(doc);
}

/**
 * htmlReadMemory:
 * @buffer:  a pointer to a char array
 * @size:  the size of the array
 * @url:  only used for error reporting (optional)
 * @encoding:  the document encoding, or NULL
 * @options:  a combination of htmlParserOption(s)
 *
 * Convenience function to parse an HTML document from memory.
 * The input buffer must not contain any terminating null bytes.
 *
 * See htmlCtxtReadMemory for details.
 *
 * Returns the resulting document tree
 */
htmlDocPtr
htmlReadMemory(const char *buffer, int size, const char *url,
               const char *encoding, int options)
{
    htmlParserCtxtPtr ctxt;
    xmlParserInputPtr input;
    htmlDocPtr doc = NULL;

    if (size < 0)
	return(NULL);

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
        return(NULL);

    htmlCtxtUseOptions(ctxt, options);

    input = xmlCtxtNewInputFromMemory(ctxt, url, buffer, size, encoding,
                                      XML_INPUT_BUF_STATIC);

    if (input != NULL)
        doc = htmlCtxtParseDocument(ctxt, input);

    htmlFreeParserCtxt(ctxt);
    return(doc);
}

/**
 * htmlReadFd:
 * @fd:  an open file descriptor
 * @url:  only used for error reporting (optional)
 * @encoding:  the document encoding, or NULL
 * @options:  a combination of htmlParserOptions
 *
 * Convenience function to parse an HTML document from a
 * file descriptor.
 *
 * NOTE that the file descriptor will not be closed when the
 * context is freed or reset.
 *
 * See htmlCtxtReadFd for details.
 *
 * Returns the resulting document tree
 */
htmlDocPtr
htmlReadFd(int fd, const char *url, const char *encoding, int options)
{
    htmlParserCtxtPtr ctxt;
    xmlParserInputPtr input;
    htmlDocPtr doc = NULL;

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
        return(NULL);

    htmlCtxtUseOptions(ctxt, options);

    input = xmlCtxtNewInputFromFd(ctxt, url, fd, encoding, 0);

    if (input != NULL)
        doc = htmlCtxtParseDocument(ctxt, input);

    htmlFreeParserCtxt(ctxt);
    return(doc);
}

/**
 * htmlReadIO:
 * @ioread:  an I/O read function
 * @ioclose:  an I/O close function (optional)
 * @ioctx:  an I/O handler
 * @url:  only used for error reporting (optional)
 * @encoding:  the document encoding (optional)
 * @options:  a combination of htmlParserOption(s)
 *
 * Convenience function to parse an HTML document from I/O functions
 * and context.
 *
 * See htmlCtxtReadIO for details.
 *
 * Returns the resulting document tree
 */
htmlDocPtr
htmlReadIO(xmlInputReadCallback ioread, xmlInputCloseCallback ioclose,
          void *ioctx, const char *url, const char *encoding, int options)
{
    htmlParserCtxtPtr ctxt;
    xmlParserInputPtr input;
    htmlDocPtr doc = NULL;

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
        return (NULL);

    htmlCtxtUseOptions(ctxt, options);

    input = xmlCtxtNewInputFromIO(ctxt, url, ioread, ioclose, ioctx,
                                  encoding, 0);

    if (input != NULL)
        doc = htmlCtxtParseDocument(ctxt, input);

    htmlFreeParserCtxt(ctxt);
    return(doc);
}

/**
 * htmlCtxtReadDoc:
 * @ctxt:  an HTML parser context
 * @str:  a pointer to a zero terminated string
 * @URL:  only used for error reporting (optional)
 * @encoding:  the document encoding (optional)
 * @options:  a combination of htmlParserOptions
 *
 * Parse an HTML in-memory document and build a tree.
 *
 * See htmlCtxtUseOptions for details.
 *
 * Returns the resulting document tree
 */
htmlDocPtr
htmlCtxtReadDoc(htmlParserCtxtPtr ctxt, const xmlChar *str,
                const char *URL, const char *encoding, int options)
{
    xmlParserInputPtr input;

    if (ctxt == NULL)
        return (NULL);

    htmlCtxtReset(ctxt);
    htmlCtxtUseOptions(ctxt, options);

    input = xmlCtxtNewInputFromString(ctxt, URL, (const char *) str,
                                      encoding, 0);
    if (input == NULL)
        return(NULL);

    return(htmlCtxtParseDocument(ctxt, input));
}

/**
 * htmlCtxtReadFile:
 * @ctxt:  an HTML parser context
 * @filename:  a file or URL
 * @encoding:  the document encoding (optional)
 * @options:  a combination of htmlParserOptions
 *
 * Parse an HTML file from the filesystem, the network or a
 * user-defined resource loader.
 *
 * See htmlCtxtUseOptions for details.
 *
 * Returns the resulting document tree
 */
htmlDocPtr
htmlCtxtReadFile(htmlParserCtxtPtr ctxt, const char *filename,
                const char *encoding, int options)
{
    xmlParserInputPtr input;

    if (ctxt == NULL)
        return (NULL);

    htmlCtxtReset(ctxt);
    htmlCtxtUseOptions(ctxt, options);

    input = xmlCtxtNewInputFromUrl(ctxt, filename, NULL, encoding, 0);
    if (input == NULL)
        return(NULL);

    return(htmlCtxtParseDocument(ctxt, input));
}

/**
 * htmlCtxtReadMemory:
 * @ctxt:  an HTML parser context
 * @buffer:  a pointer to a char array
 * @size:  the size of the array
 * @URL:  only used for error reporting (optional)
 * @encoding:  the document encoding (optinal)
 * @options:  a combination of htmlParserOptions
 *
 * Parse an HTML in-memory document and build a tree. The input buffer must
 * not contain any terminating null bytes.
 *
 * See htmlCtxtUseOptions for details.
 *
 * Returns the resulting document tree
 */
htmlDocPtr
htmlCtxtReadMemory(htmlParserCtxtPtr ctxt, const char *buffer, int size,
                  const char *URL, const char *encoding, int options)
{
    xmlParserInputPtr input;

    if ((ctxt == NULL) || (size < 0))
        return (NULL);

    htmlCtxtReset(ctxt);
    htmlCtxtUseOptions(ctxt, options);

    input = xmlCtxtNewInputFromMemory(ctxt, URL, buffer, size, encoding,
                                      XML_INPUT_BUF_STATIC);
    if (input == NULL)
        return(NULL);

    return(htmlCtxtParseDocument(ctxt, input));
}

/**
 * htmlCtxtReadFd:
 * @ctxt:  an HTML parser context
 * @fd:  an open file descriptor
 * @URL:  only used for error reporting (optional)
 * @encoding:  the document encoding (optinal)
 * @options:  a combination of htmlParserOptions
 *
 * Parse an HTML from a file descriptor and build a tree.
 *
 * See htmlCtxtUseOptions for details.
 *
 * NOTE that the file descriptor will not be closed when the
 * context is freed or reset.
 *
 * Returns the resulting document tree
 */
htmlDocPtr
htmlCtxtReadFd(htmlParserCtxtPtr ctxt, int fd,
              const char *URL, const char *encoding, int options)
{
    xmlParserInputPtr input;

    if (ctxt == NULL)
        return(NULL);

    htmlCtxtReset(ctxt);
    htmlCtxtUseOptions(ctxt, options);

    input = xmlCtxtNewInputFromFd(ctxt, URL, fd, encoding, 0);
    if (input == NULL)
        return(NULL);

    return(htmlCtxtParseDocument(ctxt, input));
}

/**
 * htmlCtxtReadIO:
 * @ctxt:  an HTML parser context
 * @ioread:  an I/O read function
 * @ioclose:  an I/O close function
 * @ioctx:  an I/O handler
 * @URL:  the base URL to use for the document
 * @encoding:  the document encoding, or NULL
 * @options:  a combination of htmlParserOption(s)
 *
 * Parse an HTML document from I/O functions and source and build a tree.
 *
 * See htmlCtxtUseOptions for details.
 *
 * Returns the resulting document tree
 */
htmlDocPtr
htmlCtxtReadIO(htmlParserCtxtPtr ctxt, xmlInputReadCallback ioread,
              xmlInputCloseCallback ioclose, void *ioctx,
	      const char *URL,
              const char *encoding, int options)
{
    xmlParserInputPtr input;

    if (ctxt == NULL)
        return (NULL);

    htmlCtxtReset(ctxt);
    htmlCtxtUseOptions(ctxt, options);

    input = xmlCtxtNewInputFromIO(ctxt, URL, ioread, ioclose, ioctx,
                                  encoding, 0);
    if (input == NULL)
        return(NULL);

    return(htmlCtxtParseDocument(ctxt, input));
}

#endif /* LIBXML_HTML_ENABLED */
