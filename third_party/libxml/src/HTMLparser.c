/*
 * HTMLparser.c : an HTML 4.0 non-verifying parser
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
#include "private/enc.h"
#include "private/error.h"
#include "private/html.h"
#include "private/io.h"
#include "private/parser.h"
#include "private/tree.h"

#define HTML_MAX_NAMELEN 1000
#define HTML_PARSER_BIG_BUFFER_SIZE 1000
#define HTML_PARSER_BUFFER_SIZE 100

static int htmlOmittedDefaultValue = 1;

xmlChar * htmlDecodeEntities(htmlParserCtxtPtr ctxt, int len,
			     xmlChar end, xmlChar  end2, xmlChar end3);
static void htmlParseComment(htmlParserCtxtPtr ctxt);

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

/**
 * htmlParseErrInt:
 * @ctxt:  an HTML parser context
 * @error:  the error number
 * @msg:  the error message
 * @val:  integer info
 *
 * Handle a fatal parser error, i.e. violating Well-Formedness constraints
 */
static void LIBXML_ATTR_FORMAT(3,0)
htmlParseErrInt(xmlParserCtxtPtr ctxt, xmlParserErrors error,
             const char *msg, int val)
{
    xmlCtxtErr(ctxt, NULL, XML_FROM_HTML, error, XML_ERR_ERROR,
               NULL, NULL, NULL, val, msg, val);
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
    if ((ctxt->html < 3) && (xmlStrEqual(value, BAD_CAST "head")))
        ctxt->html = 3;
    if ((ctxt->html < 10) && (xmlStrEqual(value, BAD_CAST "body")))
        ctxt->html = 10;
    if (ctxt->nameNr >= ctxt->nameMax) {
        size_t newSize = ctxt->nameMax * 2;
        const xmlChar **tmp;

        tmp = xmlRealloc((xmlChar **) ctxt->nameTab,
                         newSize * sizeof(ctxt->nameTab[0]));
        if (tmp == NULL) {
            htmlErrMemory(ctxt);
            return (-1);
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
        if (ctxt->nodeInfoMax == 0)
                ctxt->nodeInfoMax = 5;
        ctxt->nodeInfoMax *= 2;
        ctxt->nodeInfoTab = (htmlParserNodeInfo *)
                         xmlRealloc((htmlParserNodeInfo *)ctxt->nodeInfoTab,
                                    ctxt->nodeInfoMax *
                                    sizeof(ctxt->nodeInfoTab[0]));
        if (ctxt->nodeInfoTab == NULL) {
            htmlErrMemory(ctxt);
            return (0);
        }
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
 *   NEXT    Skip to the next character, this does the proper decoding
 *           in UTF-8 mode. It also pop-up unfinished entities on the fly.
 *   NEXTL(l) Skip the current unicode character of l xmlChars long.
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
#define NEXT xmlNextChar(ctxt)

#define RAW (*ctxt->input->cur)


#define NEXTL(l) do {							\
    if (*(ctxt->input->cur) == '\n') {					\
	ctxt->input->line++; ctxt->input->col = 1;			\
    } else ctxt->input->col++;						\
    ctxt->input->cur += l;						\
  } while (0)

/************
    \
    if (*ctxt->input->cur == '%') xmlParserHandlePEReference(ctxt);	\
    if (*ctxt->input->cur == '&') xmlParserHandleReference(ctxt);
 ************/

#define CUR_CHAR(l) htmlCurrentChar(ctxt, &l)

#define COPY_BUF(l,b,i,v)						\
    if (l == 1) b[i++] = v;						\
    else i += xmlCopyChar(l,&b[i],v)

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
    while (((*cur >= 'A') && (*cur <= 'Z')) ||
           ((*cur >= 'a') && (*cur <= 'z')) ||
           ((*cur >= '0') && (*cur <= '9')) ||
           (*cur == '-') || (*cur == '_') || (*cur == ':') || (*cur == '/'))
           cur++;
    if (cur == start)
        return(NULL);
    ret = xmlStrndup(start, cur - start);
    if (ret == NULL)
        htmlErrMemory(ctxt);
    return(ret);
}

/**
 * htmlCurrentChar:
 * @ctxt:  the HTML parser context
 * @len:  pointer to the length of the char read
 *
 * The current char value, if using UTF-8 this may actually span multiple
 * bytes in the input buffer. Implement the end of line normalization:
 * 2.11 End-of-Line Handling
 * If the encoding is unspecified, in the case we find an ISO-Latin-1
 * char, then the encoding converter is plugged in automatically.
 *
 * Returns the current char value and its length
 */

static int
htmlCurrentChar(xmlParserCtxtPtr ctxt, int *len) {
    const unsigned char *cur;
    unsigned char c;
    unsigned int val;

    if (ctxt->input->end - ctxt->input->cur < INPUT_CHUNK)
        xmlParserGrow(ctxt);

    if ((ctxt->input->flags & XML_INPUT_HAS_ENCODING) == 0) {
        xmlChar * guess;

        /*
         * Assume it's a fixed length encoding (1) with
         * a compatible encoding for the ASCII set, since
         * HTML constructs only use < 128 chars
         */
        if (*ctxt->input->cur < 0x80) {
            if (*ctxt->input->cur == 0) {
                if (ctxt->input->cur < ctxt->input->end) {
                    htmlParseErrInt(ctxt, XML_ERR_INVALID_CHAR,
                                    "Char 0x%X out of allowed range\n", 0);
                    *len = 1;
                    return(' ');
                } else {
                    *len = 0;
                    return(0);
                }
            }
            *len = 1;
            return(*ctxt->input->cur);
        }

        /*
         * Humm this is bad, do an automatic flow conversion
         */
        guess = htmlFindEncoding(ctxt);
        if (guess == NULL) {
            xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_8859_1);
        } else {
            xmlSwitchEncodingName(ctxt, (const char *) guess);
            xmlFree(guess);
        }
        ctxt->input->flags |= XML_INPUT_HAS_ENCODING;
    }

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

        if ((c & 0x40) == 0)
            goto encoding_error;

        avail = ctxt->input->end - ctxt->input->cur;

        if ((avail < 2) || ((cur[1] & 0xc0) != 0x80))
            goto encoding_error;
        if ((c & 0xe0) == 0xe0) {
            if ((avail < 3) || ((cur[2] & 0xc0) != 0x80))
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
            htmlParseErrInt(ctxt, XML_ERR_INVALID_CHAR,
                            "Char 0x%X out of allowed range\n", val);
        }
        return(val);
    } else {
        if (*ctxt->input->cur == 0) {
            if (ctxt->input->cur < ctxt->input->end) {
                htmlParseErrInt(ctxt, XML_ERR_INVALID_CHAR,
                                "Char 0x%X out of allowed range\n", 0);
                *len = 1;
                return(' ');
            } else {
                *len = 0;
                return(0);
            }
        }
        /* 1-byte code */
        *len = 1;
        return(*ctxt->input->cur);
    }

encoding_error:
    xmlCtxtErrIO(ctxt, XML_ERR_INVALID_ENCODING, NULL);

    if ((ctxt->input->flags & XML_INPUT_HAS_ENCODING) == 0)
        xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_8859_1);
    *len = 1;
    return(*ctxt->input->cur);
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
    int res = 0;

    while (IS_BLANK_CH(*(ctxt->input->cur))) {
        if (*(ctxt->input->cur) == '\n') {
            ctxt->input->line++; ctxt->input->col = 1;
        } else ctxt->input->col++;
        ctxt->input->cur++;
        if (*ctxt->input->cur == 0)
            xmlParserGrow(ctxt);
	if (res < INT_MAX)
	    res++;
    }
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
	, subElements , impliedsubelt , Attributes, userdata
 */

/* Definitions and a couple of vars for HTML Elements */

#define FONTSTYLE "tt", "i", "b", "u", "s", "strike", "big", "small"
#define NB_FONTSTYLE 8
#define PHRASE "em", "strong", "dfn", "code", "samp", "kbd", "var", "cite", "abbr", "acronym"
#define NB_PHRASE 10
#define SPECIAL "a", "img", "applet", "embed", "object", "font", "basefont", "br", "script", "map", "q", "sub", "sup", "span", "bdo", "iframe"
#define NB_SPECIAL 16
#define INLINE FONTSTYLE, PHRASE, SPECIAL, FORMCTRL
#define NB_INLINE NB_PCDATA + NB_FONTSTYLE + NB_PHRASE + NB_SPECIAL + NB_FORMCTRL
#define BLOCK HEADING, LIST, "pre", "p", "dl", "div", "center", "noscript", "noframes", "blockquote", "form", "isindex", "hr", "table", "fieldset", "address"
#define NB_BLOCK NB_HEADING + NB_LIST + 14
#define FORMCTRL "input", "select", "textarea", "label", "button"
#define NB_FORMCTRL 5
#define PCDATA
#define NB_PCDATA 0
#define HEADING "h1", "h2", "h3", "h4", "h5", "h6"
#define NB_HEADING 6
#define LIST "ul", "ol", "dir", "menu"
#define NB_LIST 4
#define MODIFIER
#define NB_MODIFIER 0
#define FLOW BLOCK,INLINE
#define NB_FLOW NB_BLOCK + NB_INLINE
#define EMPTY NULL


static const char* const html_flow[] = { FLOW, NULL } ;
static const char* const html_inline[] = { INLINE, NULL } ;

/* placeholders: elts with content but no subelements */
static const char* const html_pcdata[] = { NULL } ;
#define html_cdata html_pcdata


/* ... and for HTML Attributes */

#define COREATTRS "id", "class", "style", "title"
#define NB_COREATTRS 4
#define I18N "lang", "dir"
#define NB_I18N 2
#define EVENTS "onclick", "ondblclick", "onmousedown", "onmouseup", "onmouseover", "onmouseout", "onkeypress", "onkeydown", "onkeyup"
#define NB_EVENTS 9
#define ATTRS COREATTRS,I18N,EVENTS
#define NB_ATTRS NB_NB_COREATTRS + NB_I18N + NB_EVENTS
#define CELLHALIGN "align", "char", "charoff"
#define NB_CELLHALIGN 3
#define CELLVALIGN "valign"
#define NB_CELLVALIGN 1

static const char* const html_attrs[] = { ATTRS, NULL } ;
static const char* const core_i18n_attrs[] = { COREATTRS, I18N, NULL } ;
static const char* const core_attrs[] = { COREATTRS, NULL } ;
static const char* const i18n_attrs[] = { I18N, NULL } ;


/* Other declarations that should go inline ... */
static const char* const a_attrs[] = { ATTRS, "charset", "type", "name",
	"href", "hreflang", "rel", "rev", "accesskey", "shape", "coords",
	"tabindex", "onfocus", "onblur", NULL } ;
static const char* const target_attr[] = { "target", NULL } ;
static const char* const rows_cols_attr[] = { "rows", "cols", NULL } ;
static const char* const alt_attr[] = { "alt", NULL } ;
static const char* const src_alt_attrs[] = { "src", "alt", NULL } ;
static const char* const href_attrs[] = { "href", NULL } ;
static const char* const clear_attrs[] = { "clear", NULL } ;
static const char* const inline_p[] = { INLINE, "p", NULL } ;

static const char* const flow_param[] = { FLOW, "param", NULL } ;
static const char* const applet_attrs[] = { COREATTRS , "codebase",
		"archive", "alt", "name", "height", "width", "align",
		"hspace", "vspace", NULL } ;
static const char* const area_attrs[] = { "shape", "coords", "href", "nohref",
	"tabindex", "accesskey", "onfocus", "onblur", NULL } ;
static const char* const basefont_attrs[] =
	{ "id", "size", "color", "face", NULL } ;
static const char* const quote_attrs[] = { ATTRS, "cite", NULL } ;
static const char* const body_contents[] = { FLOW, "ins", "del", NULL } ;
static const char* const body_attrs[] = { ATTRS, "onload", "onunload", NULL } ;
static const char* const body_depr[] = { "background", "bgcolor", "text",
	"link", "vlink", "alink", NULL } ;
static const char* const button_attrs[] = { ATTRS, "name", "value", "type",
	"disabled", "tabindex", "accesskey", "onfocus", "onblur", NULL } ;


static const char* const col_attrs[] = { ATTRS, "span", "width", CELLHALIGN, CELLVALIGN, NULL } ;
static const char* const col_elt[] = { "col", NULL } ;
static const char* const edit_attrs[] = { ATTRS, "datetime", "cite", NULL } ;
static const char* const compact_attrs[] = { ATTRS, "compact", NULL } ;
static const char* const dl_contents[] = { "dt", "dd", NULL } ;
static const char* const compact_attr[] = { "compact", NULL } ;
static const char* const label_attr[] = { "label", NULL } ;
static const char* const fieldset_contents[] = { FLOW, "legend" } ;
static const char* const font_attrs[] = { COREATTRS, I18N, "size", "color", "face" , NULL } ;
static const char* const form_contents[] = { HEADING, LIST, INLINE, "pre", "p", "div", "center", "noscript", "noframes", "blockquote", "isindex", "hr", "table", "fieldset", "address", NULL } ;
static const char* const form_attrs[] = { ATTRS, "method", "enctype", "accept", "name", "onsubmit", "onreset", "accept-charset", NULL } ;
static const char* const frame_attrs[] = { COREATTRS, "longdesc", "name", "src", "frameborder", "marginwidth", "marginheight", "noresize", "scrolling" , NULL } ;
static const char* const frameset_attrs[] = { COREATTRS, "rows", "cols", "onload", "onunload", NULL } ;
static const char* const frameset_contents[] = { "frameset", "frame", "noframes", NULL } ;
static const char* const head_attrs[] = { I18N, "profile", NULL } ;
static const char* const head_contents[] = { "title", "isindex", "base", "script", "style", "meta", "link", "object", NULL } ;
static const char* const hr_depr[] = { "align", "noshade", "size", "width", NULL } ;
static const char* const version_attr[] = { "version", NULL } ;
static const char* const html_content[] = { "head", "body", "frameset", NULL } ;
static const char* const iframe_attrs[] = { COREATTRS, "longdesc", "name", "src", "frameborder", "marginwidth", "marginheight", "scrolling", "align", "height", "width", NULL } ;
static const char* const img_attrs[] = { ATTRS, "longdesc", "name", "height", "width", "usemap", "ismap", NULL } ;
static const char* const embed_attrs[] = { COREATTRS, "align", "alt", "border", "code", "codebase", "frameborder", "height", "hidden", "hspace", "name", "palette", "pluginspace", "pluginurl", "src", "type", "units", "vspace", "width", NULL } ;
static const char* const input_attrs[] = { ATTRS, "type", "name", "value", "checked", "disabled", "readonly", "size", "maxlength", "src", "alt", "usemap", "ismap", "tabindex", "accesskey", "onfocus", "onblur", "onselect", "onchange", "accept", NULL } ;
static const char* const prompt_attrs[] = { COREATTRS, I18N, "prompt", NULL } ;
static const char* const label_attrs[] = { ATTRS, "for", "accesskey", "onfocus", "onblur", NULL } ;
static const char* const legend_attrs[] = { ATTRS, "accesskey", NULL } ;
static const char* const align_attr[] = { "align", NULL } ;
static const char* const link_attrs[] = { ATTRS, "charset", "href", "hreflang", "type", "rel", "rev", "media", NULL } ;
static const char* const map_contents[] = { BLOCK, "area", NULL } ;
static const char* const name_attr[] = { "name", NULL } ;
static const char* const action_attr[] = { "action", NULL } ;
static const char* const blockli_elt[] = { BLOCK, "li", NULL } ;
static const char* const meta_attrs[] = { I18N, "http-equiv", "name", "scheme", "charset", NULL } ;
static const char* const content_attr[] = { "content", NULL } ;
static const char* const type_attr[] = { "type", NULL } ;
static const char* const noframes_content[] = { "body", FLOW MODIFIER, NULL } ;
static const char* const object_contents[] = { FLOW, "param", NULL } ;
static const char* const object_attrs[] = { ATTRS, "declare", "classid", "codebase", "data", "type", "codetype", "archive", "standby", "height", "width", "usemap", "name", "tabindex", NULL } ;
static const char* const object_depr[] = { "align", "border", "hspace", "vspace", NULL } ;
static const char* const ol_attrs[] = { "type", "compact", "start", NULL} ;
static const char* const option_elt[] = { "option", NULL } ;
static const char* const optgroup_attrs[] = { ATTRS, "disabled", NULL } ;
static const char* const option_attrs[] = { ATTRS, "disabled", "label", "selected", "value", NULL } ;
static const char* const param_attrs[] = { "id", "value", "valuetype", "type", NULL } ;
static const char* const width_attr[] = { "width", NULL } ;
static const char* const pre_content[] = { PHRASE, "tt", "i", "b", "u", "s", "strike", "a", "br", "script", "map", "q", "span", "bdo", "iframe", NULL } ;
static const char* const script_attrs[] = { "charset", "src", "defer", "event", "for", NULL } ;
static const char* const language_attr[] = { "language", NULL } ;
static const char* const select_content[] = { "optgroup", "option", NULL } ;
static const char* const select_attrs[] = { ATTRS, "name", "size", "multiple", "disabled", "tabindex", "onfocus", "onblur", "onchange", NULL } ;
static const char* const style_attrs[] = { I18N, "media", "title", NULL } ;
static const char* const table_attrs[] = { ATTRS, "summary", "width", "border", "frame", "rules", "cellspacing", "cellpadding", "datapagesize", NULL } ;
static const char* const table_depr[] = { "align", "bgcolor", NULL } ;
static const char* const table_contents[] = { "caption", "col", "colgroup", "thead", "tfoot", "tbody", "tr", NULL} ;
static const char* const tr_elt[] = { "tr", NULL } ;
static const char* const talign_attrs[] = { ATTRS, CELLHALIGN, CELLVALIGN, NULL} ;
static const char* const th_td_depr[] = { "nowrap", "bgcolor", "width", "height", NULL } ;
static const char* const th_td_attr[] = { ATTRS, "abbr", "axis", "headers", "scope", "rowspan", "colspan", CELLHALIGN, CELLVALIGN, NULL } ;
static const char* const textarea_attrs[] = { ATTRS, "name", "disabled", "readonly", "tabindex", "accesskey", "onfocus", "onblur", "onselect", "onchange", NULL } ;
static const char* const tr_contents[] = { "th", "td", NULL } ;
static const char* const bgcolor_attr[] = { "bgcolor", NULL } ;
static const char* const li_elt[] = { "li", NULL } ;
static const char* const ul_depr[] = { "type", "compact", NULL} ;
static const char* const dir_attr[] = { "dir", NULL} ;

#define DECL (const char**)

static const htmlElemDesc
html40ElementTable[] = {
{ "a",		0, 0, 0, 0, 0, 0, 1, "anchor ",
	DECL html_inline , NULL , DECL a_attrs , DECL target_attr, NULL
},
{ "abbr",	0, 0, 0, 0, 0, 0, 1, "abbreviated form",
	DECL html_inline , NULL , DECL html_attrs, NULL, NULL
},
{ "acronym",	0, 0, 0, 0, 0, 0, 1, "",
	DECL html_inline , NULL , DECL html_attrs, NULL, NULL
},
{ "address",	0, 0, 0, 0, 0, 0, 0, "information on author ",
	DECL inline_p  , NULL , DECL html_attrs, NULL, NULL
},
{ "applet",	0, 0, 0, 0, 1, 1, 2, "java applet ",
	DECL flow_param , NULL , NULL , DECL applet_attrs, NULL
},
{ "area",	0, 2, 2, 1, 0, 0, 0, "client-side image map area ",
	EMPTY ,  NULL , DECL area_attrs , DECL target_attr, DECL alt_attr
},
{ "b",		0, 3, 0, 0, 0, 0, 1, "bold text style",
	DECL html_inline , NULL , DECL html_attrs, NULL, NULL
},
{ "base",	0, 2, 2, 1, 0, 0, 0, "document base uri ",
	EMPTY , NULL , NULL , DECL target_attr, DECL href_attrs
},
{ "basefont",	0, 2, 2, 1, 1, 1, 1, "base font size " ,
	EMPTY , NULL , NULL, DECL basefont_attrs, NULL
},
{ "bdo",	0, 0, 0, 0, 0, 0, 1, "i18n bidi over-ride ",
	DECL html_inline , NULL , DECL core_i18n_attrs, NULL, DECL dir_attr
},
{ "big",	0, 3, 0, 0, 0, 0, 1, "large text style",
	DECL html_inline , NULL , DECL html_attrs, NULL, NULL
},
{ "blockquote",	0, 0, 0, 0, 0, 0, 0, "long quotation ",
	DECL html_flow , NULL , DECL quote_attrs , NULL, NULL
},
{ "body",	1, 1, 0, 0, 0, 0, 0, "document body ",
	DECL body_contents , "div" , DECL body_attrs, DECL body_depr, NULL
},
{ "br",		0, 2, 2, 1, 0, 0, 1, "forced line break ",
	EMPTY , NULL , DECL core_attrs, DECL clear_attrs , NULL
},
{ "button",	0, 0, 0, 0, 0, 0, 2, "push button ",
	DECL html_flow MODIFIER , NULL , DECL button_attrs, NULL, NULL
},
{ "caption",	0, 0, 0, 0, 0, 0, 0, "table caption ",
	DECL html_inline , NULL , DECL html_attrs, NULL, NULL
},
{ "center",	0, 3, 0, 0, 1, 1, 0, "shorthand for div align=center ",
	DECL html_flow , NULL , NULL, DECL html_attrs, NULL
},
{ "cite",	0, 0, 0, 0, 0, 0, 1, "citation",
	DECL html_inline , NULL , DECL html_attrs, NULL, NULL
},
{ "code",	0, 0, 0, 0, 0, 0, 1, "computer code fragment",
	DECL html_inline , NULL , DECL html_attrs, NULL, NULL
},
{ "col",	0, 2, 2, 1, 0, 0, 0, "table column ",
	EMPTY , NULL , DECL col_attrs , NULL, NULL
},
{ "colgroup",	0, 1, 0, 0, 0, 0, 0, "table column group ",
	DECL col_elt , "col" , DECL col_attrs , NULL, NULL
},
{ "dd",		0, 1, 0, 0, 0, 0, 0, "definition description ",
	DECL html_flow , NULL , DECL html_attrs, NULL, NULL
},
{ "del",	0, 0, 0, 0, 0, 0, 2, "deleted text ",
	DECL html_flow , NULL , DECL edit_attrs , NULL, NULL
},
{ "dfn",	0, 0, 0, 0, 0, 0, 1, "instance definition",
	DECL html_inline , NULL , DECL html_attrs, NULL, NULL
},
{ "dir",	0, 0, 0, 0, 1, 1, 0, "directory list",
	DECL blockli_elt, "li" , NULL, DECL compact_attrs, NULL
},
{ "div",	0, 0, 0, 0, 0, 0, 0, "generic language/style container",
	DECL html_flow, NULL, DECL html_attrs, DECL align_attr, NULL
},
{ "dl",		0, 0, 0, 0, 0, 0, 0, "definition list ",
	DECL dl_contents , "dd" , DECL html_attrs, DECL compact_attr, NULL
},
{ "dt",		0, 1, 0, 0, 0, 0, 0, "definition term ",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "em",		0, 3, 0, 0, 0, 0, 1, "emphasis",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "embed",	0, 1, 0, 0, 1, 1, 1, "generic embedded object ",
	EMPTY, NULL, DECL embed_attrs, NULL, NULL
},
{ "fieldset",	0, 0, 0, 0, 0, 0, 0, "form control group ",
	DECL fieldset_contents , NULL, DECL html_attrs, NULL, NULL
},
{ "font",	0, 3, 0, 0, 1, 1, 1, "local change to font ",
	DECL html_inline, NULL, NULL, DECL font_attrs, NULL
},
{ "form",	0, 0, 0, 0, 0, 0, 0, "interactive form ",
	DECL form_contents, "fieldset", DECL form_attrs , DECL target_attr, DECL action_attr
},
{ "frame",	0, 2, 2, 1, 0, 2, 0, "subwindow " ,
	EMPTY, NULL, NULL, DECL frame_attrs, NULL
},
{ "frameset",	0, 0, 0, 0, 0, 2, 0, "window subdivision" ,
	DECL frameset_contents, "noframes" , NULL , DECL frameset_attrs, NULL
},
{ "h1",		0, 0, 0, 0, 0, 0, 0, "heading ",
	DECL html_inline, NULL, DECL html_attrs, DECL align_attr, NULL
},
{ "h2",		0, 0, 0, 0, 0, 0, 0, "heading ",
	DECL html_inline, NULL, DECL html_attrs, DECL align_attr, NULL
},
{ "h3",		0, 0, 0, 0, 0, 0, 0, "heading ",
	DECL html_inline, NULL, DECL html_attrs, DECL align_attr, NULL
},
{ "h4",		0, 0, 0, 0, 0, 0, 0, "heading ",
	DECL html_inline, NULL, DECL html_attrs, DECL align_attr, NULL
},
{ "h5",		0, 0, 0, 0, 0, 0, 0, "heading ",
	DECL html_inline, NULL, DECL html_attrs, DECL align_attr, NULL
},
{ "h6",		0, 0, 0, 0, 0, 0, 0, "heading ",
	DECL html_inline, NULL, DECL html_attrs, DECL align_attr, NULL
},
{ "head",	1, 1, 0, 0, 0, 0, 0, "document head ",
	DECL head_contents, NULL, DECL head_attrs, NULL, NULL
},
{ "hr",		0, 2, 2, 1, 0, 0, 0, "horizontal rule " ,
	EMPTY, NULL, DECL html_attrs, DECL hr_depr, NULL
},
{ "html",	1, 1, 0, 0, 0, 0, 0, "document root element ",
	DECL html_content , NULL , DECL i18n_attrs, DECL version_attr, NULL
},
{ "i",		0, 3, 0, 0, 0, 0, 1, "italic text style",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "iframe",	0, 0, 0, 0, 0, 1, 2, "inline subwindow ",
	DECL html_flow, NULL, NULL, DECL iframe_attrs, NULL
},
{ "img",	0, 2, 2, 1, 0, 0, 1, "embedded image ",
	EMPTY, NULL, DECL img_attrs, DECL align_attr, DECL src_alt_attrs
},
{ "input",	0, 2, 2, 1, 0, 0, 1, "form control ",
	EMPTY, NULL, DECL input_attrs , DECL align_attr, NULL
},
{ "ins",	0, 0, 0, 0, 0, 0, 2, "inserted text",
	DECL html_flow, NULL, DECL edit_attrs, NULL, NULL
},
{ "isindex",	0, 2, 2, 1, 1, 1, 0, "single line prompt ",
	EMPTY, NULL, NULL, DECL prompt_attrs, NULL
},
{ "kbd",	0, 0, 0, 0, 0, 0, 1, "text to be entered by the user",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "label",	0, 0, 0, 0, 0, 0, 1, "form field label text ",
	DECL html_inline MODIFIER, NULL, DECL label_attrs , NULL, NULL
},
{ "legend",	0, 0, 0, 0, 0, 0, 0, "fieldset legend ",
	DECL html_inline, NULL, DECL legend_attrs , DECL align_attr, NULL
},
{ "li",		0, 1, 1, 0, 0, 0, 0, "list item ",
	DECL html_flow, NULL, DECL html_attrs, NULL, NULL
},
{ "link",	0, 2, 2, 1, 0, 0, 0, "a media-independent link ",
	EMPTY, NULL, DECL link_attrs, DECL target_attr, NULL
},
{ "map",	0, 0, 0, 0, 0, 0, 2, "client-side image map ",
	DECL map_contents , NULL, DECL html_attrs , NULL, DECL name_attr
},
{ "menu",	0, 0, 0, 0, 1, 1, 0, "menu list ",
	DECL blockli_elt , NULL, NULL, DECL compact_attrs, NULL
},
{ "meta",	0, 2, 2, 1, 0, 0, 0, "generic metainformation ",
	EMPTY, NULL, DECL meta_attrs , NULL , DECL content_attr
},
{ "noframes",	0, 0, 0, 0, 0, 2, 0, "alternate content container for non frame-based rendering ",
	DECL noframes_content, "body" , DECL html_attrs, NULL, NULL
},
{ "noscript",	0, 0, 0, 0, 0, 0, 0, "alternate content container for non script-based rendering ",
	DECL html_flow, "div", DECL html_attrs, NULL, NULL
},
{ "object",	0, 0, 0, 0, 0, 0, 2, "generic embedded object ",
	DECL object_contents , "div" , DECL object_attrs, DECL object_depr, NULL
},
{ "ol",		0, 0, 0, 0, 0, 0, 0, "ordered list ",
	DECL li_elt , "li" , DECL html_attrs, DECL ol_attrs, NULL
},
{ "optgroup",	0, 0, 0, 0, 0, 0, 0, "option group ",
	DECL option_elt , "option", DECL optgroup_attrs, NULL, DECL label_attr
},
{ "option",	0, 1, 0, 0, 0, 0, 0, "selectable choice " ,
	DECL html_pcdata, NULL, DECL option_attrs, NULL, NULL
},
{ "p",		0, 1, 0, 0, 0, 0, 0, "paragraph ",
	DECL html_inline, NULL, DECL html_attrs, DECL align_attr, NULL
},
{ "param",	0, 2, 2, 1, 0, 0, 0, "named property value ",
	EMPTY, NULL, DECL param_attrs, NULL, DECL name_attr
},
{ "pre",	0, 0, 0, 0, 0, 0, 0, "preformatted text ",
	DECL pre_content, NULL, DECL html_attrs, DECL width_attr, NULL
},
{ "q",		0, 0, 0, 0, 0, 0, 1, "short inline quotation ",
	DECL html_inline, NULL, DECL quote_attrs, NULL, NULL
},
{ "s",		0, 3, 0, 0, 1, 1, 1, "strike-through text style",
	DECL html_inline, NULL, NULL, DECL html_attrs, NULL
},
{ "samp",	0, 0, 0, 0, 0, 0, 1, "sample program output, scripts, etc.",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "script",	0, 0, 0, 0, 0, 0, 2, "script statements ",
	DECL html_cdata, NULL, DECL script_attrs, DECL language_attr, DECL type_attr
},
{ "select",	0, 0, 0, 0, 0, 0, 1, "option selector ",
	DECL select_content, NULL, DECL select_attrs, NULL, NULL
},
{ "small",	0, 3, 0, 0, 0, 0, 1, "small text style",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "span",	0, 0, 0, 0, 0, 0, 1, "generic language/style container ",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "strike",	0, 3, 0, 0, 1, 1, 1, "strike-through text",
	DECL html_inline, NULL, NULL, DECL html_attrs, NULL
},
{ "strong",	0, 3, 0, 0, 0, 0, 1, "strong emphasis",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "style",	0, 0, 0, 0, 0, 0, 0, "style info ",
	DECL html_cdata, NULL, DECL style_attrs, NULL, DECL type_attr
},
{ "sub",	0, 3, 0, 0, 0, 0, 1, "subscript",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "sup",	0, 3, 0, 0, 0, 0, 1, "superscript ",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "table",	0, 0, 0, 0, 0, 0, 0, "",
	DECL table_contents , "tr" , DECL table_attrs , DECL table_depr, NULL
},
{ "tbody",	1, 0, 0, 0, 0, 0, 0, "table body ",
	DECL tr_elt , "tr" , DECL talign_attrs, NULL, NULL
},
{ "td",		0, 0, 0, 0, 0, 0, 0, "table data cell",
	DECL html_flow, NULL, DECL th_td_attr, DECL th_td_depr, NULL
},
{ "textarea",	0, 0, 0, 0, 0, 0, 1, "multi-line text field ",
	DECL html_pcdata, NULL, DECL textarea_attrs, NULL, DECL rows_cols_attr
},
{ "tfoot",	0, 1, 0, 0, 0, 0, 0, "table footer ",
	DECL tr_elt , "tr" , DECL talign_attrs, NULL, NULL
},
{ "th",		0, 1, 0, 0, 0, 0, 0, "table header cell",
	DECL html_flow, NULL, DECL th_td_attr, DECL th_td_depr, NULL
},
{ "thead",	0, 1, 0, 0, 0, 0, 0, "table header ",
	DECL tr_elt , "tr" , DECL talign_attrs, NULL, NULL
},
{ "title",	0, 0, 0, 0, 0, 0, 0, "document title ",
	DECL html_pcdata, NULL, DECL i18n_attrs, NULL, NULL
},
{ "tr",		0, 0, 0, 0, 0, 0, 0, "table row ",
	DECL tr_contents , "td" , DECL talign_attrs, DECL bgcolor_attr, NULL
},
{ "tt",		0, 3, 0, 0, 0, 0, 1, "teletype or monospaced text style",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
},
{ "u",		0, 3, 0, 0, 1, 1, 1, "underlined text style",
	DECL html_inline, NULL, NULL, DECL html_attrs, NULL
},
{ "ul",		0, 0, 0, 0, 0, 0, 0, "unordered list ",
	DECL li_elt , "li" , DECL html_attrs, DECL ul_depr, NULL
},
{ "var",	0, 0, 0, 0, 0, 0, 1, "instance of a variable or program argument",
	DECL html_inline, NULL, DECL html_attrs, NULL, NULL
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
 * The list of HTML elements which are supposed not to have
 * CDATA content and where a p element will be implied
 *
 * TODO: extend that list by reading the HTML SGML DTD on
 *       implied paragraph
 */
static const char *const htmlNoContentElements[] = {
    "html",
    "head",
    NULL
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

    if (ctxt->nameNr == 0)
        return;
    for (i = (ctxt->nameNr - 1); i >= 0; i--) {
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
    if (newtag == NULL)
        return;

    while ((ctxt->name != NULL) &&
           (htmlCheckAutoClose(newtag, ctxt->name))) {
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

    if (ctxt->options & HTML_PARSE_NOIMPLIED)
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
        if (ctxt->html >= 3) {
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
        if (ctxt->html >= 10) {
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
 * htmlCheckParagraph
 * @ctxt:  an HTML parser context
 *
 * Check whether a p element need to be implied before inserting
 * characters in the current element.
 *
 * Returns 1 if a paragraph has been inserted, 0 if not and -1
 *         in case of error.
 */

static int
htmlCheckParagraph(htmlParserCtxtPtr ctxt) {
    const xmlChar *tag;
    int i;

    if (ctxt == NULL)
	return(-1);
    tag = ctxt->name;
    if (tag == NULL) {
	htmlAutoClose(ctxt, BAD_CAST"p");
	htmlCheckImplied(ctxt, BAD_CAST"p");
	htmlnamePush(ctxt, BAD_CAST"p");
	if ((ctxt->sax != NULL) && (ctxt->sax->startElement != NULL))
	    ctxt->sax->startElement(ctxt->userData, BAD_CAST"p", NULL);
	return(1);
    }
    if (!htmlOmittedDefaultValue)
	return(0);
    for (i = 0; htmlNoContentElements[i] != NULL; i++) {
	if (xmlStrEqual(tag, BAD_CAST htmlNoContentElements[i])) {
	    htmlAutoClose(ctxt, BAD_CAST"p");
	    htmlCheckImplied(ctxt, BAD_CAST"p");
	    htmlnamePush(ctxt, BAD_CAST"p");
	    if ((ctxt->sax != NULL) && (ctxt->sax->startElement != NULL))
		ctxt->sax->startElement(ctxt->userData, BAD_CAST"p", NULL);
	    return(1);
	}
    }
    return(0);
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

/*
 * Macro used to grow the current buffer.
 */
#define growBuffer(buffer) {						\
    xmlChar *tmp;							\
    buffer##_size *= 2;							\
    tmp = (xmlChar *) xmlRealloc(buffer, buffer##_size); 		\
    if (tmp == NULL) {							\
	htmlErrMemory(ctxt);			\
	xmlFree(buffer);						\
	return(NULL);							\
    }									\
    buffer = tmp;							\
}

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
 * UTF8ToHtml:
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
UTF8ToHtml(unsigned char* out, int *outlen,
              const unsigned char* in, int *inlen) {
    const unsigned char* processed = in;
    const unsigned char* outend;
    const unsigned char* outstart = out;
    const unsigned char* instart = in;
    const unsigned char* inend;
    unsigned int c, d;
    int trailing;

    if ((out == NULL) || (outlen == NULL) || (inlen == NULL)) return(-1);
    if (in == NULL) {
        /*
	 * initialization nothing to do
	 */
	*outlen = 0;
	*inlen = 0;
	return(0);
    }
    inend = in + (*inlen);
    outend = out + (*outlen);
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

	if (inend - in < trailing) {
	    break;
	}

	for ( ; trailing; trailing--) {
	    if ((in >= inend) || (((d= *in++) & 0xC0) != 0x80))
		break;
	    c <<= 6;
	    c |= d & 0x3F;
	}

	/* assertion: c is a single UTF-4 value */
	if (c < 0x80) {
	    if (out + 1 >= outend)
		break;
	    *out++ = c;
	} else {
	    int len;
	    const htmlEntityDesc * ent;
	    const char *cp;
	    char nbuf[16];

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
	    if (out + 2 + len >= outend)
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
 * Returns 1 if ignorable 0 otherwise.
 */

static int areBlanks(htmlParserCtxtPtr ctxt, const xmlChar *str, int len) {
    unsigned int i;
    int j;
    xmlNodePtr lastChild;
    xmlDtdPtr dtd;

    for (j = 0;j < len;j++)
        if (!(IS_BLANK_CH(str[j]))) return(0);

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
    if ((__xmlRegisterCallbacks) && (xmlRegisterNodeDefaultValue))
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

static const xmlChar * htmlParseNameComplex(xmlParserCtxtPtr ctxt);

static void
htmlSkipBogusComment(htmlParserCtxtPtr ctxt) {
    int c;

    htmlParseErr(ctxt, XML_HTML_INCORRECTLY_OPENED_COMMENT,
                 "Incorrectly opened comment\n", NULL, NULL);

    while (PARSER_STOPPED(ctxt) == 0) {
        c = CUR;
        if (c == 0)
            break;
        NEXT;
        if (c == '>')
            break;
    }
}

/**
 * htmlParseHTMLName:
 * @ctxt:  an HTML parser context
 *
 * parse an HTML tag or attribute name, note that we convert it to lowercase
 * since HTML names are not case-sensitive.
 *
 * Returns the Tag Name parsed or NULL
 */

static const xmlChar *
htmlParseHTMLName(htmlParserCtxtPtr ctxt) {
    const xmlChar *ret;
    int i = 0;
    xmlChar loc[HTML_PARSER_BUFFER_SIZE];

    if (!IS_ASCII_LETTER(CUR) && (CUR != '_') &&
        (CUR != ':') && (CUR != '.')) return(NULL);

    while ((i < HTML_PARSER_BUFFER_SIZE) &&
           ((IS_ASCII_LETTER(CUR)) || (IS_ASCII_DIGIT(CUR)) ||
	   (CUR == ':') || (CUR == '-') || (CUR == '_') ||
           (CUR == '.'))) {
	if ((CUR >= 'A') && (CUR <= 'Z')) loc[i] = CUR + 0x20;
        else loc[i] = CUR;
	i++;

	NEXT;
    }

    ret = xmlDictLookup(ctxt->dict, loc, i);
    if (ret == NULL)
        htmlErrMemory(ctxt);

    return(ret);
}


/**
 * htmlParseHTMLName_nonInvasive:
 * @ctxt:  an HTML parser context
 *
 * parse an HTML tag or attribute name, note that we convert it to lowercase
 * since HTML names are not case-sensitive, this doesn't consume the data
 * from the stream, it's a look-ahead
 *
 * Returns the Tag Name parsed or NULL
 */

static const xmlChar *
htmlParseHTMLName_nonInvasive(htmlParserCtxtPtr ctxt) {
    int i = 0;
    xmlChar loc[HTML_PARSER_BUFFER_SIZE];
    const xmlChar *ret;

    if (!IS_ASCII_LETTER(NXT(1)) && (NXT(1) != '_') &&
        (NXT(1) != ':')) return(NULL);

    while ((i < HTML_PARSER_BUFFER_SIZE) &&
           ((IS_ASCII_LETTER(NXT(1+i))) || (IS_ASCII_DIGIT(NXT(1+i))) ||
	   (NXT(1+i) == ':') || (NXT(1+i) == '-') || (NXT(1+i) == '_'))) {
	if ((NXT(1+i) >= 'A') && (NXT(1+i) <= 'Z')) loc[i] = NXT(1+i) + 0x20;
        else loc[i] = NXT(1+i);
	i++;
    }

    ret = xmlDictLookup(ctxt->dict, loc, i);
    if (ret == NULL)
        htmlErrMemory(ctxt);

    return(ret);
}


/**
 * htmlParseName:
 * @ctxt:  an HTML parser context
 *
 * parse an HTML name, this routine is case sensitive.
 *
 * Returns the Name parsed or NULL
 */

static const xmlChar *
htmlParseName(htmlParserCtxtPtr ctxt) {
    const xmlChar *in;
    const xmlChar *ret;
    int count = 0;

    GROW;

    /*
     * Accelerator for simple ASCII names
     */
    in = ctxt->input->cur;
    if (((*in >= 0x61) && (*in <= 0x7A)) ||
	((*in >= 0x41) && (*in <= 0x5A)) ||
	(*in == '_') || (*in == ':')) {
	in++;
	while (((*in >= 0x61) && (*in <= 0x7A)) ||
	       ((*in >= 0x41) && (*in <= 0x5A)) ||
	       ((*in >= 0x30) && (*in <= 0x39)) ||
	       (*in == '_') || (*in == '-') ||
	       (*in == ':') || (*in == '.'))
	    in++;

	if (in == ctxt->input->end)
	    return(NULL);

	if ((*in > 0) && (*in < 0x80)) {
	    count = in - ctxt->input->cur;
	    ret = xmlDictLookup(ctxt->dict, ctxt->input->cur, count);
            if (ret == NULL)
                htmlErrMemory(ctxt);
	    ctxt->input->cur = in;
	    ctxt->input->col += count;
	    return(ret);
	}
    }
    return(htmlParseNameComplex(ctxt));
}

static const xmlChar *
htmlParseNameComplex(xmlParserCtxtPtr ctxt) {
    int len = 0, l;
    int c;
    int maxLength = (ctxt->options & XML_PARSE_HUGE) ?
                    XML_MAX_TEXT_LENGTH :
                    XML_MAX_NAME_LENGTH;
    const xmlChar *base = ctxt->input->base;
    const xmlChar *ret;

    /*
     * Handler for more complex cases
     */
    c = CUR_CHAR(l);
    if ((c == ' ') || (c == '>') || (c == '/') || /* accelerators */
	(!IS_LETTER(c) && (c != '_') &&
         (c != ':'))) {
	return(NULL);
    }

    while ((c != ' ') && (c != '>') && (c != '/') && /* test bigname.xml */
	   ((IS_LETTER(c)) || (IS_DIGIT(c)) ||
            (c == '.') || (c == '-') ||
	    (c == '_') || (c == ':') ||
	    (IS_COMBINING(c)) ||
	    (IS_EXTENDER(c)))) {
	len += l;
        if (len > maxLength) {
            htmlParseErr(ctxt, XML_ERR_NAME_TOO_LONG, "name too long", NULL, NULL);
            return(NULL);
        }
	NEXTL(l);
	c = CUR_CHAR(l);
	if (ctxt->input->base != base) {
	    /*
	     * We changed encoding from an unknown encoding
	     * Input buffer changed location, so we better start again
	     */
	    return(htmlParseNameComplex(ctxt));
	}
    }

    if (ctxt->input->cur - ctxt->input->base < len) {
        /* Sanity check */
	htmlParseErr(ctxt, XML_ERR_INTERNAL_ERROR,
                     "unexpected change of input buffer", NULL, NULL);
        return (NULL);
    }

    ret = xmlDictLookup(ctxt->dict, ctxt->input->cur - len, len);
    if (ret == NULL)
        htmlErrMemory(ctxt);

    return(ret);
}


/**
 * htmlParseHTMLAttribute:
 * @ctxt:  an HTML parser context
 * @stop:  a char stop value
 *
 * parse an HTML attribute value till the stop (quote), if
 * stop is 0 then it stops at the first space
 *
 * Returns the attribute parsed or NULL
 */

static xmlChar *
htmlParseHTMLAttribute(htmlParserCtxtPtr ctxt, const xmlChar stop) {
    xmlChar *buffer = NULL;
    int buffer_size = 0;
    int maxLength = (ctxt->options & XML_PARSE_HUGE) ?
                    XML_MAX_HUGE_LENGTH :
                    XML_MAX_TEXT_LENGTH;
    xmlChar *out = NULL;
    const xmlChar *name = NULL;
    const xmlChar *cur = NULL;
    const htmlEntityDesc * ent;

    /*
     * allocate a translation buffer.
     */
    buffer_size = HTML_PARSER_BUFFER_SIZE;
    buffer = (xmlChar *) xmlMallocAtomic(buffer_size);
    if (buffer == NULL) {
	htmlErrMemory(ctxt);
	return(NULL);
    }
    out = buffer;

    /*
     * Ok loop until we reach one of the ending chars
     */
    while ((PARSER_STOPPED(ctxt) == 0) &&
           (CUR != 0) && (CUR != stop)) {
	if ((stop == 0) && (CUR == '>')) break;
	if ((stop == 0) && (IS_BLANK_CH(CUR))) break;
        if (CUR == '&') {
	    if (NXT(1) == '#') {
		unsigned int c;
		int bits;

		c = htmlParseCharRef(ctxt);
		if      (c <    0x80)
		        { *out++  = c;                bits= -6; }
		else if (c <   0x800)
		        { *out++  =((c >>  6) & 0x1F) | 0xC0;  bits=  0; }
		else if (c < 0x10000)
		        { *out++  =((c >> 12) & 0x0F) | 0xE0;  bits=  6; }
		else
		        { *out++  =((c >> 18) & 0x07) | 0xF0;  bits= 12; }

		for ( ; bits >= 0; bits-= 6) {
		    *out++  = ((c >> bits) & 0x3F) | 0x80;
		}

		if (out - buffer > buffer_size - 100) {
			int indx = out - buffer;

			growBuffer(buffer);
			out = &buffer[indx];
		}
	    } else {
		ent = htmlParseEntityRef(ctxt, &name);
		if (name == NULL) {
		    *out++ = '&';
		    if (out - buffer > buffer_size - 100) {
			int indx = out - buffer;

			growBuffer(buffer);
			out = &buffer[indx];
		    }
		} else if (ent == NULL) {
		    *out++ = '&';
		    cur = name;
		    while (*cur != 0) {
			if (out - buffer > buffer_size - 100) {
			    int indx = out - buffer;

			    growBuffer(buffer);
			    out = &buffer[indx];
			}
			*out++ = *cur++;
		    }
		} else {
		    unsigned int c;
		    int bits;

		    if (out - buffer > buffer_size - 100) {
			int indx = out - buffer;

			growBuffer(buffer);
			out = &buffer[indx];
		    }
		    c = ent->value;
		    if      (c <    0x80)
			{ *out++  = c;                bits= -6; }
		    else if (c <   0x800)
			{ *out++  =((c >>  6) & 0x1F) | 0xC0;  bits=  0; }
		    else if (c < 0x10000)
			{ *out++  =((c >> 12) & 0x0F) | 0xE0;  bits=  6; }
		    else
			{ *out++  =((c >> 18) & 0x07) | 0xF0;  bits= 12; }

		    for ( ; bits >= 0; bits-= 6) {
			*out++  = ((c >> bits) & 0x3F) | 0x80;
		    }
		}
	    }
	} else {
	    unsigned int c;
	    int bits, l;

	    if (out - buffer > buffer_size - 100) {
		int indx = out - buffer;

		growBuffer(buffer);
		out = &buffer[indx];
	    }
	    c = CUR_CHAR(l);
	    if      (c <    0x80)
		    { *out++  = c;                bits= -6; }
	    else if (c <   0x800)
		    { *out++  =((c >>  6) & 0x1F) | 0xC0;  bits=  0; }
	    else if (c < 0x10000)
		    { *out++  =((c >> 12) & 0x0F) | 0xE0;  bits=  6; }
	    else
		    { *out++  =((c >> 18) & 0x07) | 0xF0;  bits= 12; }

	    for ( ; bits >= 0; bits-= 6) {
		*out++  = ((c >> bits) & 0x3F) | 0x80;
	    }
	    NEXTL(l);
	}
        if (out - buffer > maxLength) {
            htmlParseErr(ctxt, XML_ERR_ATTRIBUTE_NOT_FINISHED,
                         "attribute value too long\n", NULL, NULL);
            xmlFree(buffer);
            return(NULL);
        }
    }
    *out = 0;
    return(buffer);
}

/**
 * htmlParseEntityRef:
 * @ctxt:  an HTML parser context
 * @str:  location to store the entity name
 *
 * DEPRECATED: Internal function, don't use.
 *
 * parse an HTML ENTITY references
 *
 * [68] EntityRef ::= '&' Name ';'
 *
 * Returns the associated htmlEntityDescPtr if found, or NULL otherwise,
 *         if non-NULL *str will have to be freed by the caller.
 */
const htmlEntityDesc *
htmlParseEntityRef(htmlParserCtxtPtr ctxt, const xmlChar **str) {
    const xmlChar *name;
    const htmlEntityDesc * ent = NULL;

    if (str != NULL) *str = NULL;
    if ((ctxt == NULL) || (ctxt->input == NULL)) return(NULL);

    if (CUR == '&') {
        NEXT;
        name = htmlParseName(ctxt);
	if (name == NULL) {
	    htmlParseErr(ctxt, XML_ERR_NAME_REQUIRED,
	                 "htmlParseEntityRef: no name\n", NULL, NULL);
	} else {
	    GROW;
	    if (CUR == ';') {
	        if (str != NULL)
		    *str = name;

		/*
		 * Lookup the entity in the table.
		 */
		ent = htmlEntityLookup(name);
		if (ent != NULL) /* OK that's ugly !!! */
		    NEXT;
	    } else {
		htmlParseErr(ctxt, XML_ERR_ENTITYREF_SEMICOL_MISSING,
		             "htmlParseEntityRef: expecting ';'\n",
			     NULL, NULL);
	        if (str != NULL)
		    *str = name;
	    }
	}
    }
    return(ent);
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

    if (CUR == '"') {
        NEXT;
	ret = htmlParseHTMLAttribute(ctxt, '"');
        if (CUR != '"') {
	    htmlParseErr(ctxt, XML_ERR_ATTRIBUTE_NOT_FINISHED,
	                 "AttValue: \" expected\n", NULL, NULL);
	} else
	    NEXT;
    } else if (CUR == '\'') {
        NEXT;
	ret = htmlParseHTMLAttribute(ctxt, '\'');
        if (CUR != '\'') {
	    htmlParseErr(ctxt, XML_ERR_ATTRIBUTE_NOT_FINISHED,
	                 "AttValue: ' expected\n", NULL, NULL);
	} else
	    NEXT;
    } else {
        /*
	 * That's an HTMLism, the attribute value may not be quoted
	 */
	ret = htmlParseHTMLAttribute(ctxt, 0);
	if (ret == NULL) {
	    htmlParseErr(ctxt, XML_ERR_ATTRIBUTE_WITHOUT_VALUE,
	                 "AttValue: no value found\n", NULL, NULL);
	}
    }
    return(ret);
}

/**
 * htmlParseSystemLiteral:
 * @ctxt:  an HTML parser context
 *
 * parse an HTML Literal
 *
 * [11] SystemLiteral ::= ('"' [^"]* '"') | ("'" [^']* "'")
 *
 * Returns the SystemLiteral parsed or NULL
 */

static xmlChar *
htmlParseSystemLiteral(htmlParserCtxtPtr ctxt) {
    size_t len = 0, startPosition = 0;
    int err = 0;
    int quote;
    xmlChar *ret = NULL;

    if ((CUR != '"') && (CUR != '\'')) {
	htmlParseErr(ctxt, XML_ERR_LITERAL_NOT_STARTED,
	             "SystemLiteral \" or ' expected\n", NULL, NULL);
        return(NULL);
    }
    quote = CUR;
    NEXT;

    if (CUR_PTR < BASE_PTR)
        return(ret);
    startPosition = CUR_PTR - BASE_PTR;

    while ((PARSER_STOPPED(ctxt) == 0) &&
           (CUR != 0) && (CUR != quote)) {
        /* TODO: Handle UTF-8 */
        if (!IS_CHAR_CH(CUR)) {
            htmlParseErrInt(ctxt, XML_ERR_INVALID_CHAR,
                            "Invalid char in SystemLiteral 0x%X\n", CUR);
            err = 1;
        }
        NEXT;
        len++;
    }
    if (CUR != quote) {
        htmlParseErr(ctxt, XML_ERR_LITERAL_NOT_FINISHED,
                     "Unfinished SystemLiteral\n", NULL, NULL);
    } else {
        if (err == 0) {
            ret = xmlStrndup((BASE_PTR+startPosition), len);
            if (ret == NULL) {
                htmlErrMemory(ctxt);
                return(NULL);
            }
        }
        NEXT;
    }

    return(ret);
}

/**
 * htmlParsePubidLiteral:
 * @ctxt:  an HTML parser context
 *
 * parse an HTML public literal
 *
 * [12] PubidLiteral ::= '"' PubidChar* '"' | "'" (PubidChar - "'")* "'"
 *
 * Returns the PubidLiteral parsed or NULL.
 */

static xmlChar *
htmlParsePubidLiteral(htmlParserCtxtPtr ctxt) {
    size_t len = 0, startPosition = 0;
    int err = 0;
    int quote;
    xmlChar *ret = NULL;

    if ((CUR != '"') && (CUR != '\'')) {
	htmlParseErr(ctxt, XML_ERR_LITERAL_NOT_STARTED,
	             "PubidLiteral \" or ' expected\n", NULL, NULL);
        return(NULL);
    }
    quote = CUR;
    NEXT;

    /*
     * Name ::= (Letter | '_') (NameChar)*
     */
    if (CUR_PTR < BASE_PTR)
        return(ret);
    startPosition = CUR_PTR - BASE_PTR;

    while ((PARSER_STOPPED(ctxt) == 0) &&
           (CUR != 0) && (CUR != quote)) {
        if (!IS_PUBIDCHAR_CH(CUR)) {
            htmlParseErrInt(ctxt, XML_ERR_INVALID_CHAR,
                            "Invalid char in PubidLiteral 0x%X\n", CUR);
            err = 1;
        }
        len++;
        NEXT;
    }

    if (CUR != quote) {
        htmlParseErr(ctxt, XML_ERR_LITERAL_NOT_FINISHED,
                     "Unfinished PubidLiteral\n", NULL, NULL);
    } else {
        if (err == 0) {
            ret = xmlStrndup((BASE_PTR + startPosition), len);
            if (ret == NULL) {
                htmlErrMemory(ctxt);
                return(NULL);
            }
        }
        NEXT;
    }

    return(ret);
}

/**
 * htmlParseScript:
 * @ctxt:  an HTML parser context
 *
 * parse the content of an HTML SCRIPT or STYLE element
 * http://www.w3.org/TR/html4/sgml/dtd.html#Script
 * http://www.w3.org/TR/html4/sgml/dtd.html#StyleSheet
 * http://www.w3.org/TR/html4/types.html#type-script
 * http://www.w3.org/TR/html4/types.html#h-6.15
 * http://www.w3.org/TR/html4/appendix/notes.html#h-B.3.2.1
 *
 * Script data ( %Script; in the DTD) can be the content of the SCRIPT
 * element and the value of intrinsic event attributes. User agents must
 * not evaluate script data as HTML markup but instead must pass it on as
 * data to a script engine.
 * NOTES:
 * - The content is passed like CDATA
 * - the attributes for style and scripting "onXXX" are also described
 *   as CDATA but SGML allows entities references in attributes so their
 *   processing is identical as other attributes
 */
static void
htmlParseScript(htmlParserCtxtPtr ctxt) {
    xmlChar buf[HTML_PARSER_BIG_BUFFER_SIZE + 5];
    int nbchar = 0;
    int cur,l;

    cur = CUR_CHAR(l);
    while (cur != 0) {
	if ((cur == '<') && (NXT(1) == '/')) {
            /*
             * One should break here, the specification is clear:
             * Authors should therefore escape "</" within the content.
             * Escape mechanisms are specific to each scripting or
             * style sheet language.
             *
             * In recovery mode, only break if end tag match the
             * current tag, effectively ignoring all tags inside the
             * script/style block and treating the entire block as
             * CDATA.
             */
            if (ctxt->recovery) {
                if (xmlStrncasecmp(ctxt->name, ctxt->input->cur+2,
				   xmlStrlen(ctxt->name)) == 0)
                {
                    break; /* while */
                } else {
		    htmlParseErr(ctxt, XML_ERR_TAG_NAME_MISMATCH,
				 "Element %s embeds close tag\n",
		                 ctxt->name, NULL);
		}
            } else {
                if (((NXT(2) >= 'A') && (NXT(2) <= 'Z')) ||
                    ((NXT(2) >= 'a') && (NXT(2) <= 'z')))
                {
                    break; /* while */
                }
            }
	}
        if (IS_CHAR(cur)) {
	    COPY_BUF(l,buf,nbchar,cur);
        } else {
            htmlParseErrInt(ctxt, XML_ERR_INVALID_CHAR,
                            "Invalid char in CDATA 0x%X\n", cur);
        }
	NEXTL(l);
	if (nbchar >= HTML_PARSER_BIG_BUFFER_SIZE) {
            buf[nbchar] = 0;
	    if (ctxt->sax->cdataBlock!= NULL) {
		/*
		 * Insert as CDATA, which is the same as HTML_PRESERVE_NODE
		 */
		ctxt->sax->cdataBlock(ctxt->userData, buf, nbchar);
	    } else if (ctxt->sax->characters != NULL) {
		ctxt->sax->characters(ctxt->userData, buf, nbchar);
	    }
	    nbchar = 0;
            SHRINK;
	}
	cur = CUR_CHAR(l);
    }

    if ((nbchar != 0) && (ctxt->sax != NULL) && (!ctxt->disableSAX)) {
        buf[nbchar] = 0;
	if (ctxt->sax->cdataBlock!= NULL) {
	    /*
	     * Insert as CDATA, which is the same as HTML_PRESERVE_NODE
	     */
	    ctxt->sax->cdataBlock(ctxt->userData, buf, nbchar);
	} else if (ctxt->sax->characters != NULL) {
	    ctxt->sax->characters(ctxt->userData, buf, nbchar);
	}
    }
}


/**
 * htmlParseCharDataInternal:
 * @ctxt:  an HTML parser context
 * @readahead: optional read ahead character in ascii range
 *
 * parse a CharData section.
 * if we are within a CDATA section ']]>' marks an end of section.
 *
 * [14] CharData ::= [^<&]* - ([^<&]* ']]>' [^<&]*)
 */

static void
htmlParseCharDataInternal(htmlParserCtxtPtr ctxt, int readahead) {
    xmlChar buf[HTML_PARSER_BIG_BUFFER_SIZE + 6];
    int nbchar = 0;
    int cur, l;

    if (readahead)
        buf[nbchar++] = readahead;

    cur = CUR_CHAR(l);
    while ((cur != '<') &&
           (cur != '&') &&
	   (cur != 0) &&
           (!PARSER_STOPPED(ctxt))) {
	if (!(IS_CHAR(cur))) {
	    htmlParseErrInt(ctxt, XML_ERR_INVALID_CHAR,
	                "Invalid char in CDATA 0x%X\n", cur);
	} else {
	    COPY_BUF(l,buf,nbchar,cur);
	}
	NEXTL(l);
	if (nbchar >= HTML_PARSER_BIG_BUFFER_SIZE) {
            buf[nbchar] = 0;

	    /*
	     * Ok the segment is to be consumed as chars.
	     */
	    if ((ctxt->sax != NULL) && (!ctxt->disableSAX)) {
		if (areBlanks(ctxt, buf, nbchar)) {
		    if (ctxt->keepBlanks) {
			if (ctxt->sax->characters != NULL)
			    ctxt->sax->characters(ctxt->userData, buf, nbchar);
		    } else {
			if (ctxt->sax->ignorableWhitespace != NULL)
			    ctxt->sax->ignorableWhitespace(ctxt->userData,
			                                   buf, nbchar);
		    }
		} else {
		    htmlCheckParagraph(ctxt);
		    if (ctxt->sax->characters != NULL)
			ctxt->sax->characters(ctxt->userData, buf, nbchar);
		}
	    }
	    nbchar = 0;
            SHRINK;
	}
	cur = CUR_CHAR(l);
    }
    if (nbchar != 0) {
        buf[nbchar] = 0;

	/*
	 * Ok the segment is to be consumed as chars.
	 */
	if ((ctxt->sax != NULL) && (!ctxt->disableSAX)) {
	    if (areBlanks(ctxt, buf, nbchar)) {
		if (ctxt->keepBlanks) {
		    if (ctxt->sax->characters != NULL)
			ctxt->sax->characters(ctxt->userData, buf, nbchar);
		} else {
		    if (ctxt->sax->ignorableWhitespace != NULL)
			ctxt->sax->ignorableWhitespace(ctxt->userData,
			                               buf, nbchar);
		}
	    } else {
		htmlCheckParagraph(ctxt);
		if (ctxt->sax->characters != NULL)
		    ctxt->sax->characters(ctxt->userData, buf, nbchar);
	    }
	}
    }
}

/**
 * htmlParseCharData:
 * @ctxt:  an HTML parser context
 *
 * parse a CharData section.
 * if we are within a CDATA section ']]>' marks an end of section.
 *
 * [14] CharData ::= [^<&]* - ([^<&]* ']]>' [^<&]*)
 */

static void
htmlParseCharData(htmlParserCtxtPtr ctxt) {
    htmlParseCharDataInternal(ctxt, 0);
}

/**
 * htmlParseExternalID:
 * @ctxt:  an HTML parser context
 * @publicID:  a xmlChar** receiving PubidLiteral
 *
 * Parse an External ID or a Public ID
 *
 * [75] ExternalID ::= 'SYSTEM' S SystemLiteral
 *                   | 'PUBLIC' S PubidLiteral S SystemLiteral
 *
 * [83] PublicID ::= 'PUBLIC' S PubidLiteral
 *
 * Returns the function returns SystemLiteral and in the second
 *                case publicID receives PubidLiteral, is strict is off
 *                it is possible to return NULL and have publicID set.
 */

static xmlChar *
htmlParseExternalID(htmlParserCtxtPtr ctxt, xmlChar **publicID) {
    xmlChar *URI = NULL;

    if ((UPPER == 'S') && (UPP(1) == 'Y') &&
         (UPP(2) == 'S') && (UPP(3) == 'T') &&
	 (UPP(4) == 'E') && (UPP(5) == 'M')) {
        SKIP(6);
	if (!IS_BLANK_CH(CUR)) {
	    htmlParseErr(ctxt, XML_ERR_SPACE_REQUIRED,
	                 "Space required after 'SYSTEM'\n", NULL, NULL);
	}
        SKIP_BLANKS;
	URI = htmlParseSystemLiteral(ctxt);
	if (URI == NULL) {
	    htmlParseErr(ctxt, XML_ERR_URI_REQUIRED,
	                 "htmlParseExternalID: SYSTEM, no URI\n", NULL, NULL);
        }
    } else if ((UPPER == 'P') && (UPP(1) == 'U') &&
	       (UPP(2) == 'B') && (UPP(3) == 'L') &&
	       (UPP(4) == 'I') && (UPP(5) == 'C')) {
        SKIP(6);
	if (!IS_BLANK_CH(CUR)) {
	    htmlParseErr(ctxt, XML_ERR_SPACE_REQUIRED,
	                 "Space required after 'PUBLIC'\n", NULL, NULL);
	}
        SKIP_BLANKS;
	*publicID = htmlParsePubidLiteral(ctxt);
	if (*publicID == NULL) {
	    htmlParseErr(ctxt, XML_ERR_PUBID_REQUIRED,
	                 "htmlParseExternalID: PUBLIC, no Public Identifier\n",
			 NULL, NULL);
	}
        SKIP_BLANKS;
        if ((CUR == '"') || (CUR == '\'')) {
	    URI = htmlParseSystemLiteral(ctxt);
	}
    }
    return(URI);
}

/**
 * htmlParsePI:
 * @ctxt:  an HTML parser context
 *
 * Parse an XML Processing Instruction. HTML5 doesn't allow processing
 * instructions, so this will be removed at some point.
 */
static void
htmlParsePI(htmlParserCtxtPtr ctxt) {
    xmlChar *buf = NULL;
    int len = 0;
    int size = HTML_PARSER_BUFFER_SIZE;
    int cur, l;
    int maxLength = (ctxt->options & XML_PARSE_HUGE) ?
                    XML_MAX_HUGE_LENGTH :
                    XML_MAX_TEXT_LENGTH;
    const xmlChar *target;
    xmlParserInputState state;

    if ((RAW == '<') && (NXT(1) == '?')) {
	state = ctxt->instate;
        ctxt->instate = XML_PARSER_PI;
	/*
	 * this is a Processing Instruction.
	 */
	SKIP(2);

	/*
	 * Parse the target name and check for special support like
	 * namespace.
	 */
        target = htmlParseName(ctxt);
	if (target != NULL) {
	    if (RAW == '>') {
		SKIP(1);

		/*
		 * SAX: PI detected.
		 */
		if ((ctxt->sax) && (!ctxt->disableSAX) &&
		    (ctxt->sax->processingInstruction != NULL))
		    ctxt->sax->processingInstruction(ctxt->userData,
		                                     target, NULL);
                goto done;
	    }
	    buf = (xmlChar *) xmlMallocAtomic(size);
	    if (buf == NULL) {
		htmlErrMemory(ctxt);
		return;
	    }
	    cur = CUR;
	    if (!IS_BLANK(cur)) {
		htmlParseErr(ctxt, XML_ERR_SPACE_REQUIRED,
			  "ParsePI: PI %s space expected\n", target, NULL);
	    }
            SKIP_BLANKS;
	    cur = CUR_CHAR(l);
	    while ((cur != 0) && (cur != '>')) {
		if (len + 5 >= size) {
		    xmlChar *tmp;

		    size *= 2;
		    tmp = (xmlChar *) xmlRealloc(buf, size);
		    if (tmp == NULL) {
			htmlErrMemory(ctxt);
			xmlFree(buf);
			return;
		    }
		    buf = tmp;
		}
                if (IS_CHAR(cur)) {
		    COPY_BUF(l,buf,len,cur);
                } else {
                    htmlParseErrInt(ctxt, XML_ERR_INVALID_CHAR,
                                    "Invalid char in processing instruction "
                                    "0x%X\n", cur);
                }
                if (len > maxLength) {
                    htmlParseErr(ctxt, XML_ERR_PI_NOT_FINISHED,
                                 "PI %s too long", target, NULL);
                    xmlFree(buf);
                    goto done;
                }
		NEXTL(l);
		cur = CUR_CHAR(l);
	    }
	    buf[len] = 0;
	    if (cur != '>') {
		htmlParseErr(ctxt, XML_ERR_PI_NOT_FINISHED,
		      "ParsePI: PI %s never end ...\n", target, NULL);
	    } else {
		SKIP(1);

		/*
		 * SAX: PI detected.
		 */
		if ((ctxt->sax) && (!ctxt->disableSAX) &&
		    (ctxt->sax->processingInstruction != NULL))
		    ctxt->sax->processingInstruction(ctxt->userData,
		                                     target, buf);
	    }
	    xmlFree(buf);
	} else {
	    htmlParseErr(ctxt, XML_ERR_PI_NOT_STARTED,
                         "PI is not started correctly", NULL, NULL);
	}

done:
	ctxt->instate = state;
    }
}

/**
 * htmlParseComment:
 * @ctxt:  an HTML parser context
 *
 * Parse an HTML comment
 */
static void
htmlParseComment(htmlParserCtxtPtr ctxt) {
    xmlChar *buf = NULL;
    int len;
    int size = HTML_PARSER_BUFFER_SIZE;
    int q, ql;
    int r, rl;
    int cur, l;
    int next, nl;
    int maxLength = (ctxt->options & XML_PARSE_HUGE) ?
                    XML_MAX_HUGE_LENGTH :
                    XML_MAX_TEXT_LENGTH;
    xmlParserInputState state;

    /*
     * Check that there is a comment right here.
     */
    if ((RAW != '<') || (NXT(1) != '!') ||
        (NXT(2) != '-') || (NXT(3) != '-')) return;

    state = ctxt->instate;
    ctxt->instate = XML_PARSER_COMMENT;
    SKIP(4);
    buf = (xmlChar *) xmlMallocAtomic(size);
    if (buf == NULL) {
        htmlErrMemory(ctxt);
	return;
    }
    len = 0;
    buf[len] = 0;
    q = CUR_CHAR(ql);
    if (q == 0)
        goto unfinished;
    if (q == '>') {
        htmlParseErr(ctxt, XML_ERR_COMMENT_ABRUPTLY_ENDED, "Comment abruptly ended", NULL, NULL);
        cur = '>';
        goto finished;
    }
    NEXTL(ql);
    r = CUR_CHAR(rl);
    if (r == 0)
        goto unfinished;
    if (q == '-' && r == '>') {
        htmlParseErr(ctxt, XML_ERR_COMMENT_ABRUPTLY_ENDED, "Comment abruptly ended", NULL, NULL);
        cur = '>';
        goto finished;
    }
    NEXTL(rl);
    cur = CUR_CHAR(l);
    while ((cur != 0) &&
           ((cur != '>') ||
	    (r != '-') || (q != '-'))) {
	NEXTL(l);
	next = CUR_CHAR(nl);

	if ((q == '-') && (r == '-') && (cur == '!') && (next == '>')) {
	  htmlParseErr(ctxt, XML_ERR_COMMENT_NOT_FINISHED,
		       "Comment incorrectly closed by '--!>'", NULL, NULL);
	  cur = '>';
	  break;
	}

	if (len + 5 >= size) {
	    xmlChar *tmp;

	    size *= 2;
	    tmp = (xmlChar *) xmlRealloc(buf, size);
	    if (tmp == NULL) {
	        xmlFree(buf);
	        htmlErrMemory(ctxt);
		return;
	    }
	    buf = tmp;
	}
        if (IS_CHAR(q)) {
	    COPY_BUF(ql,buf,len,q);
        } else {
            htmlParseErrInt(ctxt, XML_ERR_INVALID_CHAR,
                            "Invalid char in comment 0x%X\n", q);
        }
        if (len > maxLength) {
            htmlParseErr(ctxt, XML_ERR_COMMENT_NOT_FINISHED,
                         "comment too long", NULL, NULL);
            xmlFree(buf);
            ctxt->instate = state;
            return;
        }

	q = r;
	ql = rl;
	r = cur;
	rl = l;
	cur = next;
	l = nl;
    }
finished:
    buf[len] = 0;
    if (cur == '>') {
        NEXT;
	if ((ctxt->sax != NULL) && (ctxt->sax->comment != NULL) &&
	    (!ctxt->disableSAX))
	    ctxt->sax->comment(ctxt->userData, buf);
	xmlFree(buf);
	ctxt->instate = state;
	return;
    }

unfinished:
    htmlParseErr(ctxt, XML_ERR_COMMENT_NOT_FINISHED,
		 "Comment not terminated \n<!--%.50s\n", buf, NULL);
    xmlFree(buf);
}

/**
 * htmlParseCharRef:
 * @ctxt:  an HTML parser context
 *
 * DEPRECATED: Internal function, don't use.
 *
 * parse Reference declarations
 *
 * [66] CharRef ::= '&#' [0-9]+ ';' |
 *                  '&#x' [0-9a-fA-F]+ ';'
 *
 * Returns the value parsed (as an int)
 */
int
htmlParseCharRef(htmlParserCtxtPtr ctxt) {
    int val = 0;

    if ((ctxt == NULL) || (ctxt->input == NULL))
        return(0);
    if ((CUR == '&') && (NXT(1) == '#') &&
        ((NXT(2) == 'x') || NXT(2) == 'X')) {
	SKIP(3);
	while (CUR != ';') {
	    if ((CUR >= '0') && (CUR <= '9')) {
                if (val < 0x110000)
	            val = val * 16 + (CUR - '0');
            } else if ((CUR >= 'a') && (CUR <= 'f')) {
                if (val < 0x110000)
	            val = val * 16 + (CUR - 'a') + 10;
            } else if ((CUR >= 'A') && (CUR <= 'F')) {
                if (val < 0x110000)
	            val = val * 16 + (CUR - 'A') + 10;
            } else {
	        htmlParseErr(ctxt, XML_ERR_INVALID_HEX_CHARREF,
		             "htmlParseCharRef: missing semicolon\n",
			     NULL, NULL);
		break;
	    }
	    NEXT;
	}
	if (CUR == ';')
	    NEXT;
    } else if  ((CUR == '&') && (NXT(1) == '#')) {
	SKIP(2);
	while (CUR != ';') {
	    if ((CUR >= '0') && (CUR <= '9')) {
                if (val < 0x110000)
	            val = val * 10 + (CUR - '0');
            } else {
	        htmlParseErr(ctxt, XML_ERR_INVALID_DEC_CHARREF,
		             "htmlParseCharRef: missing semicolon\n",
			     NULL, NULL);
		break;
	    }
	    NEXT;
	}
	if (CUR == ';')
	    NEXT;
    } else {
	htmlParseErr(ctxt, XML_ERR_INVALID_CHARREF,
	             "htmlParseCharRef: invalid value\n", NULL, NULL);
    }
    /*
     * Check the value IS_CHAR ...
     */
    if (IS_CHAR(val)) {
        return(val);
    } else if (val >= 0x110000) {
	htmlParseErr(ctxt, XML_ERR_INVALID_CHAR,
		     "htmlParseCharRef: value too large\n", NULL, NULL);
    } else {
	htmlParseErrInt(ctxt, XML_ERR_INVALID_CHAR,
			"htmlParseCharRef: invalid xmlChar value %d\n",
			val);
    }
    return(0);
}


/**
 * htmlParseDocTypeDecl:
 * @ctxt:  an HTML parser context
 *
 * parse a DOCTYPE declaration
 *
 * [28] doctypedecl ::= '<!DOCTYPE' S Name (S ExternalID)? S?
 *                      ('[' (markupdecl | PEReference | S)* ']' S?)? '>'
 */

static void
htmlParseDocTypeDecl(htmlParserCtxtPtr ctxt) {
    const xmlChar *name;
    xmlChar *ExternalID = NULL;
    xmlChar *URI = NULL;

    /*
     * We know that '<!DOCTYPE' has been detected.
     */
    SKIP(9);

    SKIP_BLANKS;

    /*
     * Parse the DOCTYPE name.
     */
    name = htmlParseName(ctxt);
    if (name == NULL) {
	htmlParseErr(ctxt, XML_ERR_NAME_REQUIRED,
	             "htmlParseDocTypeDecl : no DOCTYPE name !\n",
		     NULL, NULL);
    }
    /*
     * Check that upper(name) == "HTML" !!!!!!!!!!!!!
     */

    SKIP_BLANKS;

    /*
     * Check for SystemID and ExternalID
     */
    URI = htmlParseExternalID(ctxt, &ExternalID);
    SKIP_BLANKS;

    /*
     * We should be at the end of the DOCTYPE declaration.
     */
    if (CUR != '>') {
	htmlParseErr(ctxt, XML_ERR_DOCTYPE_NOT_FINISHED,
	             "DOCTYPE improperly terminated\n", NULL, NULL);
        /* Ignore bogus content */
        while ((CUR != 0) && (CUR != '>') &&
               (PARSER_STOPPED(ctxt) == 0))
            NEXT;
    }
    if (CUR == '>')
        NEXT;

    /*
     * Create or update the document accordingly to the DOCTYPE
     */
    if ((ctxt->sax != NULL) && (ctxt->sax->internalSubset != NULL) &&
	(!ctxt->disableSAX))
	ctxt->sax->internalSubset(ctxt->userData, name, ExternalID, URI);

    /*
     * Cleanup, since we don't use all those identifiers
     */
    if (URI != NULL) xmlFree(URI);
    if (ExternalID != NULL) xmlFree(ExternalID);
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

static const xmlChar *
htmlParseAttribute(htmlParserCtxtPtr ctxt, xmlChar **value) {
    const xmlChar *name;
    xmlChar *val = NULL;

    *value = NULL;
    name = htmlParseHTMLName(ctxt);
    if (name == NULL) {
	htmlParseErr(ctxt, XML_ERR_NAME_REQUIRED,
	             "error parsing attribute name\n", NULL, NULL);
        return(NULL);
    }

    /*
     * read the value
     */
    SKIP_BLANKS;
    if (CUR == '=') {
        NEXT;
	SKIP_BLANKS;
	val = htmlParseAttValue(ctxt);
    }

    *value = val;
    return(name);
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
    if (encoding && IS_BLANK_CH(*encoding))
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

static int
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

    if ((ctxt == NULL) || (ctxt->input == NULL))
	return -1;
    if (CUR != '<') return -1;
    NEXT;

    atts = ctxt->atts;
    maxatts = ctxt->maxatts;

    GROW;
    name = htmlParseHTMLName(ctxt);
    if (name == NULL) {
	htmlParseErr(ctxt, XML_ERR_NAME_REQUIRED,
	             "htmlParseStartTag: invalid element name\n",
		     NULL, NULL);
	/* Dump the bogus tag like browsers do */
	while ((CUR != 0) && (CUR != '>') &&
               (PARSER_STOPPED(ctxt) == 0))
	    NEXT;
        return -1;
    }
    if (xmlStrEqual(name, BAD_CAST"meta"))
	meta = 1;

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

    /*
     * Now parse the attributes, it ends up with the ending
     *
     * (S Attribute)* S?
     */
    SKIP_BLANKS;
    while ((CUR != 0) &&
           (CUR != '>') &&
	   ((CUR != '/') || (NXT(1) != '>')) &&
           (PARSER_STOPPED(ctxt) == 0)) {
	GROW;
	attname = htmlParseAttribute(ctxt, &attvalue);
        if (attname != NULL) {

	    /*
	     * Well formedness requires at most one declaration of an attribute
	     */
	    for (i = 0; i < nbatts;i += 2) {
	        if (xmlStrEqual(atts[i], attname)) {
		    htmlParseErr(ctxt, XML_ERR_ATTRIBUTE_REDEFINED,
		                 "Attribute %s redefined\n", attname, NULL);
		    if (attvalue != NULL)
			xmlFree(attvalue);
		    goto failed;
		}
	    }

	    /*
	     * Add the pair to atts
	     */
	    if (atts == NULL) {
	        maxatts = 22; /* allow for 10 attrs by default */
	        atts = (const xmlChar **)
		       xmlMalloc(maxatts * sizeof(xmlChar *));
		if (atts == NULL) {
		    htmlErrMemory(ctxt);
		    if (attvalue != NULL)
			xmlFree(attvalue);
		    goto failed;
		}
		ctxt->atts = atts;
		ctxt->maxatts = maxatts;
	    } else if (nbatts + 4 > maxatts) {
	        const xmlChar **n;

	        maxatts *= 2;
	        n = (const xmlChar **) xmlRealloc((void *) atts,
					     maxatts * sizeof(const xmlChar *));
		if (n == NULL) {
		    htmlErrMemory(ctxt);
		    if (attvalue != NULL)
			xmlFree(attvalue);
		    goto failed;
		}
		atts = n;
		ctxt->atts = atts;
		ctxt->maxatts = maxatts;
	    }
	    atts[nbatts++] = attname;
	    atts[nbatts++] = attvalue;
	    atts[nbatts] = NULL;
	    atts[nbatts + 1] = NULL;
	}
	else {
	    if (attvalue != NULL)
	        xmlFree(attvalue);
	    /* Dump the bogus attribute string up to the next blank or
	     * the end of the tag. */
	    while ((CUR != 0) &&
	           !(IS_BLANK_CH(CUR)) && (CUR != '>') &&
		   ((CUR != '/') || (NXT(1) != '>')) &&
                   (PARSER_STOPPED(ctxt) == 0))
		NEXT;
	}

failed:
	SKIP_BLANKS;
    }

    /*
     * Handle specific association to the META tag
     */
    if (meta && (nbatts != 0))
	htmlCheckMeta(ctxt, atts);

    /*
     * SAX: Start of Element !
     */
    if (!discardtag) {
	htmlnamePush(ctxt, name);
	if ((ctxt->sax != NULL) && (ctxt->sax->startElement != NULL)) {
	    if (nbatts != 0)
		ctxt->sax->startElement(ctxt->userData, name, atts);
	    else
		ctxt->sax->startElement(ctxt->userData, name, NULL);
	}
    }

    if (atts != NULL) {
        for (i = 1;i < nbatts;i += 2) {
	    if (atts[i] != NULL)
		xmlFree((xmlChar *) atts[i]);
	}
    }

    return(discardtag);
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

static int
htmlParseEndTag(htmlParserCtxtPtr ctxt)
{
    const xmlChar *name;
    const xmlChar *oldname;
    int i, ret;

    if ((CUR != '<') || (NXT(1) != '/')) {
        htmlParseErr(ctxt, XML_ERR_LTSLASH_REQUIRED,
	             "htmlParseEndTag: '</' not found\n", NULL, NULL);
        return (0);
    }
    SKIP(2);

    name = htmlParseHTMLName(ctxt);
    if (name == NULL)
        return (0);
    /*
     * We should definitely be at the ending "S? '>'" part
     */
    SKIP_BLANKS;
    if (CUR != '>') {
        htmlParseErr(ctxt, XML_ERR_GT_REQUIRED,
	             "End tag : expected '>'\n", NULL, NULL);
        /* Skip to next '>' */
        while ((PARSER_STOPPED(ctxt) == 0) &&
               (CUR != 0) && (CUR != '>'))
            NEXT;
    }
    if (CUR == '>')
        NEXT;

    /*
     * if we ignored misplaced tags in htmlParseStartTag don't pop them
     * out now.
     */
    if ((ctxt->depth > 0) &&
        (xmlStrEqual(name, BAD_CAST "html") ||
         xmlStrEqual(name, BAD_CAST "body") ||
	 xmlStrEqual(name, BAD_CAST "head"))) {
	ctxt->depth--;
	return (0);
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
        return (0);
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
        if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
            ctxt->sax->endElement(ctxt->userData, name);
	htmlNodeInfoPop(ctxt);
        htmlnamePop(ctxt);
        ret = 1;
    } else {
        ret = 0;
    }

    return (ret);
}


/**
 * htmlParseReference:
 * @ctxt:  an HTML parser context
 *
 * parse and handle entity references in content,
 * this will end-up in a call to character() since this is either a
 * CharRef, or a predefined entity.
 */
static void
htmlParseReference(htmlParserCtxtPtr ctxt) {
    const htmlEntityDesc * ent;
    xmlChar out[6];
    const xmlChar *name;
    if (CUR != '&') return;

    if (NXT(1) == '#') {
	unsigned int c;
	int bits, i = 0;

	c = htmlParseCharRef(ctxt);
	if (c == 0)
	    return;

        if      (c <    0x80) { out[i++]= c;                bits= -6; }
        else if (c <   0x800) { out[i++]=((c >>  6) & 0x1F) | 0xC0;  bits=  0; }
        else if (c < 0x10000) { out[i++]=((c >> 12) & 0x0F) | 0xE0;  bits=  6; }
        else                  { out[i++]=((c >> 18) & 0x07) | 0xF0;  bits= 12; }

        for ( ; bits >= 0; bits-= 6) {
            out[i++]= ((c >> bits) & 0x3F) | 0x80;
        }
	out[i] = 0;

	htmlCheckParagraph(ctxt);
	if ((ctxt->sax != NULL) && (ctxt->sax->characters != NULL))
	    ctxt->sax->characters(ctxt->userData, out, i);
    } else {
	ent = htmlParseEntityRef(ctxt, &name);
	if (name == NULL) {
	    htmlCheckParagraph(ctxt);
	    if ((ctxt->sax != NULL) && (ctxt->sax->characters != NULL))
	        ctxt->sax->characters(ctxt->userData, BAD_CAST "&", 1);
	    return;
	}
	if ((ent == NULL) || !(ent->value > 0)) {
	    htmlCheckParagraph(ctxt);
	    if ((ctxt->sax != NULL) && (ctxt->sax->characters != NULL)) {
		ctxt->sax->characters(ctxt->userData, BAD_CAST "&", 1);
		ctxt->sax->characters(ctxt->userData, name, xmlStrlen(name));
		/* ctxt->sax->characters(ctxt->userData, BAD_CAST ";", 1); */
	    }
	} else {
	    unsigned int c;
	    int bits, i = 0;

	    c = ent->value;
	    if      (c <    0x80)
	            { out[i++]= c;                bits= -6; }
	    else if (c <   0x800)
	            { out[i++]=((c >>  6) & 0x1F) | 0xC0;  bits=  0; }
	    else if (c < 0x10000)
	            { out[i++]=((c >> 12) & 0x0F) | 0xE0;  bits=  6; }
	    else
	            { out[i++]=((c >> 18) & 0x07) | 0xF0;  bits= 12; }

	    for ( ; bits >= 0; bits-= 6) {
		out[i++]= ((c >> bits) & 0x3F) | 0x80;
	    }
	    out[i] = 0;

	    htmlCheckParagraph(ctxt);
	    if ((ctxt->sax != NULL) && (ctxt->sax->characters != NULL))
		ctxt->sax->characters(ctxt->userData, out, i);
	}
    }
}

/**
 * htmlParseContent:
 * @ctxt:  an HTML parser context
 *
 * Parse a content: comment, sub-element, reference or text.
 * Kept for compatibility with old code
 */

static void
htmlParseContent(htmlParserCtxtPtr ctxt) {
    xmlChar *currentNode;
    int depth;
    const xmlChar *name;

    currentNode = xmlStrdup(ctxt->name);
    depth = ctxt->nameNr;
    while (!PARSER_STOPPED(ctxt)) {
        GROW;

	/*
	 * Our tag or one of it's parent or children is ending.
	 */
        if ((CUR == '<') && (NXT(1) == '/')) {
	    if (htmlParseEndTag(ctxt) &&
		((currentNode != NULL) || (ctxt->nameNr == 0))) {
		if (currentNode != NULL)
		    xmlFree(currentNode);
		return;
	    }
	    continue; /* while */
        }

	else if ((CUR == '<') &&
	         ((IS_ASCII_LETTER(NXT(1))) ||
		  (NXT(1) == '_') || (NXT(1) == ':'))) {
	    name = htmlParseHTMLName_nonInvasive(ctxt);
	    if (name == NULL) {
	        htmlParseErr(ctxt, XML_ERR_NAME_REQUIRED,
			 "htmlParseStartTag: invalid element name\n",
			 NULL, NULL);
	        /* Dump the bogus tag like browsers do */
                while ((CUR != 0) && (CUR != '>'))
	            NEXT;

	        if (currentNode != NULL)
	            xmlFree(currentNode);
	        return;
	    }

	    if (ctxt->name != NULL) {
	        if (htmlCheckAutoClose(name, ctxt->name) == 1) {
	            htmlAutoClose(ctxt, name);
	            continue;
	        }
	    }
	}

	/*
	 * Has this node been popped out during parsing of
	 * the next element
	 */
        if ((ctxt->nameNr > 0) && (depth >= ctxt->nameNr) &&
	    (!xmlStrEqual(currentNode, ctxt->name)))
	     {
	    if (currentNode != NULL) xmlFree(currentNode);
	    return;
	}

	if ((CUR != 0) && ((xmlStrEqual(currentNode, BAD_CAST"script")) ||
	    (xmlStrEqual(currentNode, BAD_CAST"style")))) {
	    /*
	     * Handle SCRIPT/STYLE separately
	     */
	    htmlParseScript(ctxt);
	}

        else if ((CUR == '<') && (NXT(1) == '!')) {
            /*
             * Sometimes DOCTYPE arrives in the middle of the document
             */
            if ((UPP(2) == 'D') && (UPP(3) == 'O') &&
                (UPP(4) == 'C') && (UPP(5) == 'T') &&
                (UPP(6) == 'Y') && (UPP(7) == 'P') &&
                (UPP(8) == 'E')) {
                htmlParseErr(ctxt, XML_HTML_STRUCURE_ERROR,
                             "Misplaced DOCTYPE declaration\n",
                             BAD_CAST "DOCTYPE" , NULL);
                htmlParseDocTypeDecl(ctxt);
            }
            /*
             * First case :  a comment
             */
            else if ((NXT(2) == '-') && (NXT(3) == '-')) {
                htmlParseComment(ctxt);
            }
            else {
                htmlSkipBogusComment(ctxt);
            }
        }

        /*
         * Second case : a Processing Instruction.
         */
        else if ((CUR == '<') && (NXT(1) == '?')) {
            htmlParsePI(ctxt);
        }

        /*
         * Third case :  a sub-element.
         */
        else if ((CUR == '<') && IS_ASCII_LETTER(NXT(1))) {
            htmlParseElement(ctxt);
        }
        else if (CUR == '<') {
            if ((ctxt->sax != NULL) && (!ctxt->disableSAX) &&
                (ctxt->sax->characters != NULL))
                ctxt->sax->characters(ctxt->userData, BAD_CAST "<", 1);
            NEXT;
        }

        /*
         * Fourth case : a reference. If if has not been resolved,
         *    parsing returns it's Name, create the node
         */
        else if (CUR == '&') {
            htmlParseReference(ctxt);
        }

        /*
         * Fifth case : end of the resource
         */
        else if (CUR == 0) {
            htmlAutoCloseOnEnd(ctxt);
            break;
        }

        /*
         * Last case, text. Note that References are handled directly.
         */
        else {
            htmlParseCharData(ctxt);
        }

        SHRINK;
        GROW;
    }
    if (currentNode != NULL) xmlFree(currentNode);
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
    const xmlChar *name;
    xmlChar *currentNode = NULL;
    const htmlElemDesc * info;
    htmlParserNodeInfo node_info;
    int failed;
    int depth;
    const xmlChar *oldptr;

    if ((ctxt == NULL) || (ctxt->input == NULL))
	return;

    /* Capture start position */
    if (ctxt->record_info) {
        node_info.begin_pos = ctxt->input->consumed +
                          (CUR_PTR - ctxt->input->base);
	node_info.begin_line = ctxt->input->line;
    }

    failed = htmlParseStartTag(ctxt);
    name = ctxt->name;
    if ((failed == -1) || (name == NULL)) {
	if (CUR == '>')
	    NEXT;
        return;
    }

    /*
     * Lookup the info for that element.
     */
    info = htmlTagLookup(name);
    if (info == NULL) {
	htmlParseErr(ctxt, XML_HTML_UNKNOWN_TAG,
	             "Tag %s invalid\n", name, NULL);
    }

    /*
     * Check for an Empty Element labeled the XML/SGML way
     */
    if ((CUR == '/') && (NXT(1) == '>')) {
        SKIP(2);
	if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
	    ctxt->sax->endElement(ctxt->userData, name);
	htmlnamePop(ctxt);
	return;
    }

    if (CUR == '>') {
        NEXT;
    } else {
	htmlParseErr(ctxt, XML_ERR_GT_REQUIRED,
	             "Couldn't find end of Start Tag %s\n", name, NULL);

	/*
	 * end of parsing of this node.
	 */
	if (xmlStrEqual(name, ctxt->name)) {
	    nodePop(ctxt);
	    htmlnamePop(ctxt);
	}

	/*
	 * Capture end position and add node
	 */
	if (ctxt->record_info) {
	   node_info.end_pos = ctxt->input->consumed +
			      (CUR_PTR - ctxt->input->base);
	   node_info.end_line = ctxt->input->line;
	   node_info.node = ctxt->node;
	   xmlParserAddNodeInfo(ctxt, &node_info);
	}
	return;
    }

    /*
     * Check for an Empty Element from DTD definition
     */
    if ((info != NULL) && (info->empty)) {
	if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
	    ctxt->sax->endElement(ctxt->userData, name);
	htmlnamePop(ctxt);
	return;
    }

    /*
     * Parse the content of the element:
     */
    currentNode = xmlStrdup(ctxt->name);
    depth = ctxt->nameNr;
    while (CUR != 0) {
	oldptr = ctxt->input->cur;
	htmlParseContent(ctxt);
	if (oldptr==ctxt->input->cur) break;
	if (ctxt->nameNr < depth) break;
    }

    /*
     * Capture end position and add node
     */
    if ( currentNode != NULL && ctxt->record_info ) {
       node_info.end_pos = ctxt->input->consumed +
                          (CUR_PTR - ctxt->input->base);
       node_info.end_line = ctxt->input->line;
       node_info.node = ctxt->node;
       xmlParserAddNodeInfo(ctxt, &node_info);
    }
    if (CUR == 0) {
	htmlAutoCloseOnEnd(ctxt);
    }

    if (currentNode != NULL)
	xmlFree(currentNode);
}

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
    if (CUR == 0) {
       htmlAutoCloseOnEnd(ctxt);
    }
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

static void
htmlParseElementInternal(htmlParserCtxtPtr ctxt) {
    const xmlChar *name;
    const htmlElemDesc * info;
    htmlParserNodeInfo node_info = { NULL, 0, 0, 0, 0 };
    int failed;

    if ((ctxt == NULL) || (ctxt->input == NULL))
	return;

    /* Capture start position */
    if (ctxt->record_info) {
        node_info.begin_pos = ctxt->input->consumed +
                          (CUR_PTR - ctxt->input->base);
	node_info.begin_line = ctxt->input->line;
    }

    failed = htmlParseStartTag(ctxt);
    name = ctxt->name;
    if ((failed == -1) || (name == NULL)) {
	if (CUR == '>')
	    NEXT;
        return;
    }

    /*
     * Lookup the info for that element.
     */
    info = htmlTagLookup(name);
    if (info == NULL) {
	htmlParseErr(ctxt, XML_HTML_UNKNOWN_TAG,
	             "Tag %s invalid\n", name, NULL);
    }

    /*
     * Check for an Empty Element labeled the XML/SGML way
     */
    if ((CUR == '/') && (NXT(1) == '>')) {
        SKIP(2);
	if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
	    ctxt->sax->endElement(ctxt->userData, name);
	htmlnamePop(ctxt);
	return;
    }

    if (CUR == '>') {
        NEXT;
    } else {
	htmlParseErr(ctxt, XML_ERR_GT_REQUIRED,
	             "Couldn't find end of Start Tag %s\n", name, NULL);

	/*
	 * end of parsing of this node.
	 */
	if (xmlStrEqual(name, ctxt->name)) {
	    nodePop(ctxt);
	    htmlnamePop(ctxt);
	}

        if (ctxt->record_info)
            htmlNodeInfoPush(ctxt, &node_info);
        htmlParserFinishElementParsing(ctxt);
	return;
    }

    /*
     * Check for an Empty Element from DTD definition
     */
    if ((info != NULL) && (info->empty)) {
	if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
	    ctxt->sax->endElement(ctxt->userData, name);
	htmlnamePop(ctxt);
	return;
    }

    if (ctxt->record_info)
        htmlNodeInfoPush(ctxt, &node_info);
}

/**
 * htmlParseContentInternal:
 * @ctxt:  an HTML parser context
 *
 * Parse a content: comment, sub-element, reference or text.
 * New version for non recursive htmlParseElementInternal
 */

static void
htmlParseContentInternal(htmlParserCtxtPtr ctxt) {
    xmlChar *currentNode;
    int depth;
    const xmlChar *name;

    depth = ctxt->nameNr;
    if (depth <= 0) {
        currentNode = NULL;
    } else {
        currentNode = xmlStrdup(ctxt->name);
        if (currentNode == NULL) {
            htmlErrMemory(ctxt);
            return;
        }
    }
    while (PARSER_STOPPED(ctxt) == 0) {
        GROW;

	/*
	 * Our tag or one of it's parent or children is ending.
	 */
        if ((CUR == '<') && (NXT(1) == '/')) {
	    if (htmlParseEndTag(ctxt) &&
		((currentNode != NULL) || (ctxt->nameNr == 0))) {
		if (currentNode != NULL)
		    xmlFree(currentNode);

	        depth = ctxt->nameNr;
                if (depth <= 0) {
                    currentNode = NULL;
                } else {
                    currentNode = xmlStrdup(ctxt->name);
                    if (currentNode == NULL) {
                        htmlErrMemory(ctxt);
                        break;
                    }
                }
	    }
	    continue; /* while */
        }

	else if ((CUR == '<') &&
	         ((IS_ASCII_LETTER(NXT(1))) ||
		  (NXT(1) == '_') || (NXT(1) == ':'))) {
	    name = htmlParseHTMLName_nonInvasive(ctxt);
	    if (name == NULL) {
	        htmlParseErr(ctxt, XML_ERR_NAME_REQUIRED,
			 "htmlParseStartTag: invalid element name\n",
			 NULL, NULL);
	        /* Dump the bogus tag like browsers do */
	        while ((CUR == 0) && (CUR != '>'))
	            NEXT;

	        htmlParserFinishElementParsing(ctxt);
	        if (currentNode != NULL)
	            xmlFree(currentNode);

                if (ctxt->name == NULL) {
                    currentNode = NULL;
                } else {
                    currentNode = xmlStrdup(ctxt->name);
                    if (currentNode == NULL) {
                        htmlErrMemory(ctxt);
                        break;
                    }
                }
	        depth = ctxt->nameNr;
	        continue;
	    }

	    if (ctxt->name != NULL) {
	        if (htmlCheckAutoClose(name, ctxt->name) == 1) {
	            htmlAutoClose(ctxt, name);
	            continue;
	        }
	    }
	}

	/*
	 * Has this node been popped out during parsing of
	 * the next element
	 */
        if ((ctxt->nameNr > 0) && (depth >= ctxt->nameNr) &&
	    (!xmlStrEqual(currentNode, ctxt->name)))
	     {
	    htmlParserFinishElementParsing(ctxt);
	    if (currentNode != NULL) xmlFree(currentNode);

            if (ctxt->name == NULL) {
                currentNode = NULL;
            } else {
                currentNode = xmlStrdup(ctxt->name);
                if (currentNode == NULL) {
                    htmlErrMemory(ctxt);
                    break;
                }
            }
	    depth = ctxt->nameNr;
	    continue;
	}

	if ((CUR != 0) && ((xmlStrEqual(currentNode, BAD_CAST"script")) ||
	    (xmlStrEqual(currentNode, BAD_CAST"style")))) {
	    /*
	     * Handle SCRIPT/STYLE separately
	     */
	    htmlParseScript(ctxt);
	}

        else if ((CUR == '<') && (NXT(1) == '!')) {
            /*
             * Sometimes DOCTYPE arrives in the middle of the document
             */
            if ((UPP(2) == 'D') && (UPP(3) == 'O') &&
                (UPP(4) == 'C') && (UPP(5) == 'T') &&
                (UPP(6) == 'Y') && (UPP(7) == 'P') &&
                (UPP(8) == 'E')) {
                htmlParseErr(ctxt, XML_HTML_STRUCURE_ERROR,
                             "Misplaced DOCTYPE declaration\n",
                             BAD_CAST "DOCTYPE" , NULL);
                htmlParseDocTypeDecl(ctxt);
            }
            /*
             * First case :  a comment
             */
            else if ((NXT(2) == '-') && (NXT(3) == '-')) {
                htmlParseComment(ctxt);
            }
            else {
                htmlSkipBogusComment(ctxt);
            }
        }

        /*
         * Second case : a Processing Instruction.
         */
        else if ((CUR == '<') && (NXT(1) == '?')) {
            htmlParsePI(ctxt);
        }

        /*
         * Third case :  a sub-element.
         */
        else if ((CUR == '<') && IS_ASCII_LETTER(NXT(1))) {
            htmlParseElementInternal(ctxt);
            if (currentNode != NULL) xmlFree(currentNode);

            if (ctxt->name == NULL) {
                currentNode = NULL;
            } else {
                currentNode = xmlStrdup(ctxt->name);
                if (currentNode == NULL) {
                    htmlErrMemory(ctxt);
                    break;
                }
            }
            depth = ctxt->nameNr;
        }
        else if (CUR == '<') {
            if ((ctxt->sax != NULL) && (!ctxt->disableSAX) &&
                (ctxt->sax->characters != NULL))
                ctxt->sax->characters(ctxt->userData, BAD_CAST "<", 1);
            NEXT;
        }

        /*
         * Fourth case : a reference. If if has not been resolved,
         *    parsing returns it's Name, create the node
         */
        else if (CUR == '&') {
            htmlParseReference(ctxt);
        }

        /*
         * Fifth case : end of the resource
         */
        else if (CUR == 0) {
            htmlAutoCloseOnEnd(ctxt);
            break;
        }

        /*
         * Last case, text. Note that References are handled directly.
         */
        else {
            htmlParseCharData(ctxt);
        }

        SHRINK;
        GROW;
    }
    if (currentNode != NULL) xmlFree(currentNode);
}

/**
 * htmlParseContent:
 * @ctxt:  an HTML parser context
 *
 * Parse a content: comment, sub-element, reference or text.
 * This is the entry point when called from parser.c
 */

void
__htmlParseContent(void *ctxt) {
    if (ctxt != NULL)
	htmlParseContentInternal((htmlParserCtxtPtr) ctxt);
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

    /*
     * Document locator is unused. Only for backward compatibility.
     */
    if ((ctxt->sax) && (ctxt->sax->setDocumentLocator)) {
        xmlSAXLocator copy = xmlDefaultSAXLocator;
        ctxt->sax->setDocumentLocator(ctxt->userData, &copy);
    }

    xmlDetectEncoding(ctxt);

    /*
     * This is wrong but matches long-standing behavior. In most cases,
     * a document starting with an XML declaration will specify UTF-8.
     */
    if (((ctxt->input->flags & XML_INPUT_HAS_ENCODING) == 0) &&
        (xmlStrncmp(ctxt->input->cur, BAD_CAST "<?xm", 4) == 0))
        xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_UTF8);

    /*
     * Wipe out everything which is before the first '<'
     */
    SKIP_BLANKS;
    if (CUR == 0) {
	htmlParseErr(ctxt, XML_ERR_DOCUMENT_EMPTY,
	             "Document is empty\n", NULL, NULL);
    }

    if ((ctxt->sax) && (ctxt->sax->startDocument) && (!ctxt->disableSAX))
	ctxt->sax->startDocument(ctxt->userData);

    /*
     * Parse possible comments and PIs before any content
     */
    while (((CUR == '<') && (NXT(1) == '!') &&
            (NXT(2) == '-') && (NXT(3) == '-')) ||
	   ((CUR == '<') && (NXT(1) == '?'))) {
        htmlParseComment(ctxt);
        htmlParsePI(ctxt);
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
	htmlParseDocTypeDecl(ctxt);
    }
    SKIP_BLANKS;

    /*
     * Parse possible comments and PIs before any content
     */
    while ((PARSER_STOPPED(ctxt) == 0) &&
           (((CUR == '<') && (NXT(1) == '!') &&
             (NXT(2) == '-') && (NXT(3) == '-')) ||
	    ((CUR == '<') && (NXT(1) == '?')))) {
        htmlParseComment(ctxt);
        htmlParsePI(ctxt);
	SKIP_BLANKS;
    }

    /*
     * Time to start parsing the tree itself
     */
    htmlParseContentInternal(ctxt);

    /*
     * autoclose
     */
    if (CUR == 0)
	htmlAutoCloseOnEnd(ctxt);


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
                      xmlMalloc(5 * sizeof(htmlParserInputPtr));
    if (ctxt->inputTab == NULL)
	return(-1);
    ctxt->inputNr = 0;
    ctxt->inputMax = 5;
    ctxt->input = NULL;
    ctxt->version = NULL;
    ctxt->encoding = NULL;
    ctxt->standalone = -1;
    ctxt->instate = XML_PARSER_START;

    /* Allocate the Node stack */
    ctxt->nodeTab = (htmlNodePtr *) xmlMalloc(10 * sizeof(htmlNodePtr));
    if (ctxt->nodeTab == NULL)
	return(-1);
    ctxt->nodeNr = 0;
    ctxt->nodeMax = 10;
    ctxt->node = NULL;

    /* Allocate the Name stack */
    ctxt->nameTab = (const xmlChar **) xmlMalloc(10 * sizeof(xmlChar *));
    if (ctxt->nameTab == NULL)
	return(-1);
    ctxt->nameNr = 0;
    ctxt->nameMax = 10;
    ctxt->name = NULL;

    ctxt->nodeInfoTab = NULL;
    ctxt->nodeInfoNr  = 0;
    ctxt->nodeInfoMax = 0;

    ctxt->myDoc = NULL;
    ctxt->wellFormed = 1;
    ctxt->replaceEntities = 0;
    ctxt->linenumbers = xmlLineNumbersDefaultValue;
    ctxt->keepBlanks = xmlKeepBlanksDefaultValue;
    ctxt->html = 1;
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
 * See xmlNewInputURL, xmlNewInputMemory, xmlNewInputIO and similar
 * functions for advanced input control.
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

    input = xmlNewInputMemory(ctxt, url, buffer, size, encoding, 0);
    if (input == NULL) {
	xmlFreeParserCtxt(ctxt);
        return(NULL);
    }

    inputPush(ctxt, input);

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

    input = xmlNewInputString(ctxt, url, (const char *) str, encoding, 0);
    if (input == NULL) {
	xmlFreeParserCtxt(ctxt);
	return(NULL);
    }

    inputPush(ctxt, input);

    return(ctxt);
}

#ifdef LIBXML_PUSH_ENABLED
/************************************************************************
 *									*
 *	Progressive parsing interfaces				*
 *									*
 ************************************************************************/

/**
 * htmlParseLookupSequence:
 * @ctxt:  an HTML parser context
 * @first:  the first char to lookup
 * @next:  the next char to lookup or zero
 * @third:  the next char to lookup or zero
 * @ignoreattrval: skip over attribute values
 *
 * Try to find if a sequence (first, next, third) or  just (first next) or
 * (first) is available in the input stream.
 * This function has a side effect of (possibly) incrementing ctxt->checkIndex
 * to avoid rescanning sequences of bytes, it DOES change the state of the
 * parser, do not use liberally.
 * This is basically similar to xmlParseLookupSequence()
 *
 * Returns the index to the current parsing point if the full sequence
 *      is available, -1 otherwise.
 */
static int
htmlParseLookupSequence(htmlParserCtxtPtr ctxt, xmlChar first,
                        xmlChar next, xmlChar third, int ignoreattrval)
{
    size_t base, len;
    htmlParserInputPtr in;
    const xmlChar *buf;
    int quote;

    in = ctxt->input;
    if (in == NULL)
        return (-1);

    base = ctxt->checkIndex;
    quote = ctxt->endCheckState;

    buf = in->cur;
    len = in->end - in->cur;

    /* take into account the sequence length */
    if (third)
        len -= 2;
    else if (next)
        len--;
    for (; base < len; base++) {
        if (base >= INT_MAX / 2) {
            ctxt->checkIndex = 0;
            ctxt->endCheckState = 0;
            return (base - 2);
        }
        if (ignoreattrval) {
            if (quote) {
                if (buf[base] == quote)
                    quote = 0;
                continue;
            }
            if (buf[base] == '"' || buf[base] == '\'') {
                quote = buf[base];
                continue;
            }
        }
        if (buf[base] == first) {
            if (third != 0) {
                if ((buf[base + 1] != next) || (buf[base + 2] != third))
                    continue;
            } else if (next != 0) {
                if (buf[base + 1] != next)
                    continue;
            }
            ctxt->checkIndex = 0;
            ctxt->endCheckState = 0;
            return (base);
        }
    }
    ctxt->checkIndex = base;
    ctxt->endCheckState = quote;
    return (-1);
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
 * This wraps to htmlParseLookupSequence()
 *
 * Returns the index to the current parsing point if the full sequence is available, -1 otherwise.
 */
static int
htmlParseLookupCommentEnd(htmlParserCtxtPtr ctxt)
{
    int mark = 0;
    int offset;

    while (1) {
	mark = htmlParseLookupSequence(ctxt, '-', '-', 0, 0);
	if (mark < 0)
            break;
        if ((NXT(mark+2) == '>') ||
	    ((NXT(mark+2) == '!') && (NXT(mark+3) == '>'))) {
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
static int
htmlParseTryOrFinish(htmlParserCtxtPtr ctxt, int terminate) {
    int ret = 0;
    htmlParserInputPtr in;
    ptrdiff_t avail = 0;
    xmlChar cur, next;

    htmlParserNodeInfo node_info;

    while (PARSER_STOPPED(ctxt) == 0) {

	in = ctxt->input;
	if (in == NULL) break;
	avail = in->end - in->cur;
	if ((avail == 0) && (terminate)) {
	    htmlAutoCloseOnEnd(ctxt);
	    if ((ctxt->nameNr == 0) && (ctxt->instate != XML_PARSER_EOF)) {
		/*
		 * SAX: end of the document processing.
		 */
		ctxt->instate = XML_PARSER_EOF;
		if ((ctxt->sax) && (ctxt->sax->endDocument != NULL))
		    ctxt->sax->endDocument(ctxt->userData);
	    }
	}
        if (avail < 1)
	    goto done;
        /*
         * This is done to make progress and avoid an infinite loop
         * if a parsing attempt was aborted by hitting a NUL byte. After
         * changing htmlCurrentChar, this probably isn't necessary anymore.
         * We should consider removing this check.
         */
	cur = in->cur[0];
	if (cur == 0) {
	    SKIP(1);
	    continue;
	}

        switch (ctxt->instate) {
            case XML_PARSER_EOF:
	        /*
		 * Document parsing is done !
		 */
	        goto done;
            case XML_PARSER_START:
                /*
                 * This is wrong but matches long-standing behavior. In most
                 * cases, a document starting with an XML declaration will
                 * specify UTF-8.
                 */
                if (((ctxt->input->flags & XML_INPUT_HAS_ENCODING) == 0) &&
                    (xmlStrncmp(ctxt->input->cur, BAD_CAST "<?xm", 4) == 0)) {
                    xmlSwitchEncoding(ctxt, XML_CHAR_ENCODING_UTF8);
                }

	        /*
		 * Very first chars read from the document flow.
		 */
		cur = in->cur[0];
		if (IS_BLANK_CH(cur)) {
		    SKIP_BLANKS;
                    avail = in->end - in->cur;
		}
                if ((ctxt->sax) && (ctxt->sax->setDocumentLocator)) {
                    xmlSAXLocator copy = xmlDefaultSAXLocator;
                    ctxt->sax->setDocumentLocator(ctxt->userData, &copy);
                }
		if ((ctxt->sax) && (ctxt->sax->startDocument) &&
	            (!ctxt->disableSAX))
		    ctxt->sax->startDocument(ctxt->userData);

		cur = in->cur[0];
		next = in->cur[1];
		if ((cur == '<') && (next == '!') &&
		    (UPP(2) == 'D') && (UPP(3) == 'O') &&
		    (UPP(4) == 'C') && (UPP(5) == 'T') &&
		    (UPP(6) == 'Y') && (UPP(7) == 'P') &&
		    (UPP(8) == 'E')) {
		    if ((!terminate) &&
		        (htmlParseLookupSequence(ctxt, '>', 0, 0, 1) < 0))
			goto done;
		    htmlParseDocTypeDecl(ctxt);
		    ctxt->instate = XML_PARSER_PROLOG;
                } else {
		    ctxt->instate = XML_PARSER_MISC;
		}
		break;
            case XML_PARSER_MISC:
		SKIP_BLANKS;
                avail = in->end - in->cur;
		/*
		 * no chars in buffer
		 */
		if (avail < 1)
		    goto done;
		/*
		 * not enough chars in buffer
		 */
		if (avail < 2) {
		    if (!terminate)
			goto done;
		    else
			next = ' ';
		} else {
		    next = in->cur[1];
		}
		cur = in->cur[0];
	        if ((cur == '<') && (next == '!') &&
		    (in->cur[2] == '-') && (in->cur[3] == '-')) {
		    if ((!terminate) && (htmlParseLookupCommentEnd(ctxt) < 0))
			goto done;
		    htmlParseComment(ctxt);
		    ctxt->instate = XML_PARSER_MISC;
	        } else if ((cur == '<') && (next == '?')) {
		    if ((!terminate) &&
		        (htmlParseLookupSequence(ctxt, '>', 0, 0, 0) < 0))
			goto done;
		    htmlParsePI(ctxt);
		    ctxt->instate = XML_PARSER_MISC;
		} else if ((cur == '<') && (next == '!') &&
		    (UPP(2) == 'D') && (UPP(3) == 'O') &&
		    (UPP(4) == 'C') && (UPP(5) == 'T') &&
		    (UPP(6) == 'Y') && (UPP(7) == 'P') &&
		    (UPP(8) == 'E')) {
		    if ((!terminate) &&
		        (htmlParseLookupSequence(ctxt, '>', 0, 0, 1) < 0))
			goto done;
		    htmlParseDocTypeDecl(ctxt);
		    ctxt->instate = XML_PARSER_PROLOG;
		} else if ((cur == '<') && (next == '!') &&
		           (avail < 9)) {
		    goto done;
		} else {
		    ctxt->instate = XML_PARSER_CONTENT;
		}
		break;
            case XML_PARSER_PROLOG:
		SKIP_BLANKS;
                avail = in->end - in->cur;
		if (avail < 2)
		    goto done;
		cur = in->cur[0];
		next = in->cur[1];
		if ((cur == '<') && (next == '!') &&
		    (in->cur[2] == '-') && (in->cur[3] == '-')) {
		    if ((!terminate) && (htmlParseLookupCommentEnd(ctxt) < 0))
			goto done;
		    htmlParseComment(ctxt);
		    ctxt->instate = XML_PARSER_PROLOG;
	        } else if ((cur == '<') && (next == '?')) {
		    if ((!terminate) &&
		        (htmlParseLookupSequence(ctxt, '>', 0, 0, 0) < 0))
			goto done;
		    htmlParsePI(ctxt);
		    ctxt->instate = XML_PARSER_PROLOG;
		} else if ((cur == '<') && (next == '!') &&
		           (avail < 4)) {
		    goto done;
		} else {
		    ctxt->instate = XML_PARSER_CONTENT;
		}
		break;
            case XML_PARSER_EPILOG:
                avail = in->end - in->cur;
		if (avail < 1)
		    goto done;
		cur = in->cur[0];
		if (IS_BLANK_CH(cur)) {
		    htmlParseCharData(ctxt);
		    goto done;
		}
		if (avail < 2)
		    goto done;
		next = in->cur[1];
	        if ((cur == '<') && (next == '!') &&
		    (in->cur[2] == '-') && (in->cur[3] == '-')) {
		    if ((!terminate) && (htmlParseLookupCommentEnd(ctxt) < 0))
			goto done;
		    htmlParseComment(ctxt);
		    ctxt->instate = XML_PARSER_EPILOG;
	        } else if ((cur == '<') && (next == '?')) {
		    if ((!terminate) &&
		        (htmlParseLookupSequence(ctxt, '>', 0, 0, 0) < 0))
			goto done;
		    htmlParsePI(ctxt);
		    ctxt->instate = XML_PARSER_EPILOG;
		} else if ((cur == '<') && (next == '!') &&
		           (avail < 4)) {
		    goto done;
		} else {
		    ctxt->errNo = XML_ERR_DOCUMENT_END;
		    ctxt->wellFormed = 0;
		    ctxt->instate = XML_PARSER_EOF;
		    if ((ctxt->sax) && (ctxt->sax->endDocument != NULL))
			ctxt->sax->endDocument(ctxt->userData);
		    goto done;
		}
		break;
            case XML_PARSER_START_TAG: {
	        const xmlChar *name;
		int failed;
		const htmlElemDesc * info;

		/*
		 * no chars in buffer
		 */
		if (avail < 1)
		    goto done;
		/*
		 * not enough chars in buffer
		 */
		if (avail < 2) {
		    if (!terminate)
			goto done;
		    else
			next = ' ';
		} else {
		    next = in->cur[1];
		}
		cur = in->cur[0];
	        if (cur != '<') {
		    ctxt->instate = XML_PARSER_CONTENT;
		    break;
		}
		if (next == '/') {
		    ctxt->instate = XML_PARSER_END_TAG;
		    ctxt->checkIndex = 0;
		    break;
		}
		if ((!terminate) &&
		    (htmlParseLookupSequence(ctxt, '>', 0, 0, 1) < 0))
		    goto done;

                /* Capture start position */
	        if (ctxt->record_info) {
	             node_info.begin_pos = ctxt->input->consumed +
	                                (CUR_PTR - ctxt->input->base);
	             node_info.begin_line = ctxt->input->line;
	        }


		failed = htmlParseStartTag(ctxt);
		name = ctxt->name;
		if ((failed == -1) ||
		    (name == NULL)) {
		    if (CUR == '>')
			NEXT;
		    break;
		}

		/*
		 * Lookup the info for that element.
		 */
		info = htmlTagLookup(name);
		if (info == NULL) {
		    htmlParseErr(ctxt, XML_HTML_UNKNOWN_TAG,
		                 "Tag %s invalid\n", name, NULL);
		}

		/*
		 * Check for an Empty Element labeled the XML/SGML way
		 */
		if ((CUR == '/') && (NXT(1) == '>')) {
		    SKIP(2);
		    if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
			ctxt->sax->endElement(ctxt->userData, name);
		    htmlnamePop(ctxt);
		    ctxt->instate = XML_PARSER_CONTENT;
		    break;
		}

		if (CUR == '>') {
		    NEXT;
		} else {
		    htmlParseErr(ctxt, XML_ERR_GT_REQUIRED,
		                 "Couldn't find end of Start Tag %s\n",
				 name, NULL);

		    /*
		     * end of parsing of this node.
		     */
		    if (xmlStrEqual(name, ctxt->name)) {
			nodePop(ctxt);
			htmlnamePop(ctxt);
		    }

		    if (ctxt->record_info)
		        htmlNodeInfoPush(ctxt, &node_info);

		    ctxt->instate = XML_PARSER_CONTENT;
		    break;
		}

		/*
		 * Check for an Empty Element from DTD definition
		 */
		if ((info != NULL) && (info->empty)) {
		    if ((ctxt->sax != NULL) && (ctxt->sax->endElement != NULL))
			ctxt->sax->endElement(ctxt->userData, name);
		    htmlnamePop(ctxt);
		}

                if (ctxt->record_info)
	            htmlNodeInfoPush(ctxt, &node_info);

		ctxt->instate = XML_PARSER_CONTENT;
                break;
	    }
            case XML_PARSER_CONTENT: {
		xmlChar chr[2] = { 0, 0 };

                /*
		 * Handle preparsed entities and charRef
		 */
		if ((avail == 1) && (terminate)) {
		    cur = in->cur[0];
		    if ((cur != '<') && (cur != '&')) {
			if (ctxt->sax != NULL) {
                            chr[0] = cur;
			    if (IS_BLANK_CH(cur)) {
				if (ctxt->keepBlanks) {
				    if (ctxt->sax->characters != NULL)
					ctxt->sax->characters(
						ctxt->userData, chr, 1);
				} else {
				    if (ctxt->sax->ignorableWhitespace != NULL)
					ctxt->sax->ignorableWhitespace(
						ctxt->userData, chr, 1);
				}
			    } else {
				htmlCheckParagraph(ctxt);
				if (ctxt->sax->characters != NULL)
				    ctxt->sax->characters(
					    ctxt->userData, chr, 1);
			    }
			}
			ctxt->checkIndex = 0;
			in->cur++;
			break;
		    }
		}
		if (avail < 2)
		    goto done;
		cur = in->cur[0];
		next = in->cur[1];
		if ((xmlStrEqual(ctxt->name, BAD_CAST"script")) ||
		    (xmlStrEqual(ctxt->name, BAD_CAST"style"))) {
		    /*
		     * Handle SCRIPT/STYLE separately
		     */
		    if (!terminate) {
		        int idx;
			xmlChar val;

			idx = htmlParseLookupSequence(ctxt, '<', '/', 0, 0);
			if (idx < 0)
			    goto done;
		        val = in->cur[idx + 2];
			if (val == 0) { /* bad cut of input */
                            /*
                             * FIXME: htmlParseScript checks for additional
                             * characters after '</'.
                             */
                            ctxt->checkIndex = idx;
			    goto done;
                        }
		    }
		    htmlParseScript(ctxt);
		    if ((cur == '<') && (next == '/')) {
			ctxt->instate = XML_PARSER_END_TAG;
			ctxt->checkIndex = 0;
			break;
		    }
		} else if ((cur == '<') && (next == '!')) {
                    if (avail < 4)
                        goto done;
                    /*
                     * Sometimes DOCTYPE arrives in the middle of the document
                     */
                    if ((UPP(2) == 'D') && (UPP(3) == 'O') &&
                        (UPP(4) == 'C') && (UPP(5) == 'T') &&
                        (UPP(6) == 'Y') && (UPP(7) == 'P') &&
                        (UPP(8) == 'E')) {
                        if ((!terminate) &&
                            (htmlParseLookupSequence(ctxt, '>', 0, 0, 1) < 0))
                            goto done;
                        htmlParseErr(ctxt, XML_HTML_STRUCURE_ERROR,
                                     "Misplaced DOCTYPE declaration\n",
                                     BAD_CAST "DOCTYPE" , NULL);
                        htmlParseDocTypeDecl(ctxt);
                    } else if ((in->cur[2] == '-') && (in->cur[3] == '-')) {
                        if ((!terminate) &&
                            (htmlParseLookupCommentEnd(ctxt) < 0))
                            goto done;
                        htmlParseComment(ctxt);
                        ctxt->instate = XML_PARSER_CONTENT;
                    } else {
                        if ((!terminate) &&
                            (htmlParseLookupSequence(ctxt, '>', 0, 0, 0) < 0))
                            goto done;
                        htmlSkipBogusComment(ctxt);
                    }
                } else if ((cur == '<') && (next == '?')) {
                    if ((!terminate) &&
                        (htmlParseLookupSequence(ctxt, '>', 0, 0, 0) < 0))
                        goto done;
                    htmlParsePI(ctxt);
                    ctxt->instate = XML_PARSER_CONTENT;
                } else if ((cur == '<') && (next == '/')) {
                    ctxt->instate = XML_PARSER_END_TAG;
                    ctxt->checkIndex = 0;
                    break;
                } else if ((cur == '<') && IS_ASCII_LETTER(next)) {
                    if ((!terminate) && (next == 0))
                        goto done;
                    ctxt->instate = XML_PARSER_START_TAG;
                    ctxt->checkIndex = 0;
                    break;
                } else if (cur == '<') {
                    if ((ctxt->sax != NULL) && (!ctxt->disableSAX) &&
                        (ctxt->sax->characters != NULL))
                        ctxt->sax->characters(ctxt->userData,
                                              BAD_CAST "<", 1);
                    NEXT;
                } else {
                    /*
                     * check that the text sequence is complete
                     * before handing out the data to the parser
                     * to avoid problems with erroneous end of
                     * data detection.
                     */
                    if ((!terminate) &&
                        (htmlParseLookupSequence(ctxt, '<', 0, 0, 0) < 0))
                        goto done;
                    ctxt->checkIndex = 0;
                    while ((PARSER_STOPPED(ctxt) == 0) &&
                           (cur != '<') && (in->cur < in->end)) {
                        if (cur == '&') {
                            htmlParseReference(ctxt);
                        } else {
                            htmlParseCharData(ctxt);
                        }
                        cur = in->cur[0];
                    }
		}

		break;
	    }
            case XML_PARSER_END_TAG:
		if (avail < 2)
		    goto done;
		if ((!terminate) &&
		    (htmlParseLookupSequence(ctxt, '>', 0, 0, 0) < 0))
		    goto done;
		htmlParseEndTag(ctxt);
		if (ctxt->nameNr == 0) {
		    ctxt->instate = XML_PARSER_EPILOG;
		} else {
		    ctxt->instate = XML_PARSER_CONTENT;
		}
		ctxt->checkIndex = 0;
	        break;
	    default:
		htmlParseErr(ctxt, XML_ERR_INTERNAL_ERROR,
			     "HPP: internal error\n", NULL, NULL);
		ctxt->instate = XML_PARSER_EOF;
		break;
	}
    }
done:
    if ((avail == 0) && (terminate)) {
	htmlAutoCloseOnEnd(ctxt);
	if ((ctxt->nameNr == 0) && (ctxt->instate != XML_PARSER_EOF)) {
	    /*
	     * SAX: end of the document processing.
	     */
	    ctxt->instate = XML_PARSER_EOF;
	    if ((ctxt->sax) && (ctxt->sax->endDocument != NULL))
		ctxt->sax->endDocument(ctxt->userData);
	}
    }
    if ((!(ctxt->options & HTML_PARSE_NODEFDTD)) && (ctxt->myDoc != NULL) &&
	((terminate) || (ctxt->instate == XML_PARSER_EOF) ||
	 (ctxt->instate == XML_PARSER_EPILOG))) {
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
    return(ret);
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
    if ((ctxt == NULL) || (ctxt->input == NULL))
	return(XML_ERR_ARGUMENT);
    if (PARSER_STOPPED(ctxt) != 0)
        return(ctxt->errNo);
    if ((size > 0) && (chunk != NULL) && (ctxt->input != NULL) &&
        (ctxt->input->buf != NULL))  {
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
    if (terminate) {
	if (ctxt->instate != XML_PARSER_EOF) {
	    if ((ctxt->sax) && (ctxt->sax->endDocument != NULL))
		ctxt->sax->endDocument(ctxt->userData);
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
    input = xmlInputCreatePush(filename, chunk, size);
    if (input == NULL) {
	htmlFreeParserCtxt(ctxt);
	return(NULL);
    }

    inputPush(ctxt, input);

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

    input = xmlNewInputURL(ctxt, filename, NULL, encoding, 0);
    if (input == NULL) {
	xmlFreeParserCtxt(ctxt);
	return(NULL);
    }
    inputPush(ctxt, input);

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
 * See xmlNewInputURL for details.
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
 * Checks whether an HTML element may be a direct child of a parent element.
 * Note - doesn't check for deprecated elements
 *
 * Returns 1 if allowed; 0 otherwise.
 */
int
htmlElementAllowedHere(const htmlElemDesc* parent, const xmlChar* elt) {
  const char** p ;

  if ( ! elt || ! parent || ! parent->subelts )
	return 0 ;

  for ( p = parent->subelts; *p; ++p )
    if ( !xmlStrcmp((const xmlChar *)*p, elt) )
      return 1 ;

  return 0 ;
}
/**
 * htmlElementStatusHere:
 * @parent: HTML parent element
 * @elt: HTML element
 *
 * Checks whether an HTML element may be a direct child of a parent element.
 * and if so whether it is valid or deprecated.
 *
 * Returns one of HTML_VALID, HTML_DEPRECATED, HTML_INVALID
 */
htmlStatus
htmlElementStatusHere(const htmlElemDesc* parent, const htmlElemDesc* elt) {
  if ( ! parent || ! elt )
    return HTML_INVALID ;
  if ( ! htmlElementAllowedHere(parent, (const xmlChar*) elt->name ) )
    return HTML_INVALID ;

  return ( elt->dtd == 0 ) ? HTML_VALID : HTML_DEPRECATED ;
}
/**
 * htmlAttrAllowed:
 * @elt: HTML element
 * @attr: HTML attribute
 * @legacy: whether to allow deprecated attributes
 *
 * Checks whether an attribute is valid for an element
 * Has full knowledge of Required and Deprecated attributes
 *
 * Returns one of HTML_REQUIRED, HTML_VALID, HTML_DEPRECATED, HTML_INVALID
 */
htmlStatus
htmlAttrAllowed(const htmlElemDesc* elt, const xmlChar* attr, int legacy) {
  const char** p ;

  if ( !elt || ! attr )
	return HTML_INVALID ;

  if ( elt->attrs_req )
    for ( p = elt->attrs_req; *p; ++p)
      if ( !xmlStrcmp((const xmlChar*)*p, attr) )
        return HTML_REQUIRED ;

  if ( elt->attrs_opt )
    for ( p = elt->attrs_opt; *p; ++p)
      if ( !xmlStrcmp((const xmlChar*)*p, attr) )
        return HTML_VALID ;

  if ( legacy && elt->attrs_depr )
    for ( p = elt->attrs_depr; *p; ++p)
      if ( !xmlStrcmp((const xmlChar*)*p, attr) )
        return HTML_DEPRECATED ;

  return HTML_INVALID ;
}
/**
 * htmlNodeStatus:
 * @node: an htmlNodePtr in a tree
 * @legacy: whether to allow deprecated elements (YES is faster here
 *	for Element nodes)
 *
 * Checks whether the tree node is valid.  Experimental (the author
 *     only uses the HTML enhancements in a SAX parser)
 *
 * Return: for Element nodes, a return from htmlElementAllowedHere (if
 *	legacy allowed) or htmlElementStatusHere (otherwise).
 *	for Attribute nodes, a return from htmlAttrAllowed
 *	for other nodes, HTML_NA (no checks performed)
 */
htmlStatus
htmlNodeStatus(htmlNodePtr node, int legacy) {
  if ( ! node )
    return HTML_INVALID ;

  switch ( node->type ) {
    case XML_ELEMENT_NODE:
      return legacy
	? ( htmlElementAllowedHere (
		htmlTagLookup(node->parent->name) , node->name
		) ? HTML_VALID : HTML_INVALID )
	: htmlElementStatusHere(
		htmlTagLookup(node->parent->name) ,
		htmlTagLookup(node->name) )
	;
    case XML_ATTRIBUTE_NODE:
      return htmlAttrAllowed(
	htmlTagLookup(node->parent->name) , node->name, legacy) ;
    default: return HTML_NA ;
  }
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

    while ((input = inputPop(ctxt)) != NULL) { /* Non consuming */
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
    if (ctxt->myDoc != NULL)
        xmlFreeDoc(ctxt->myDoc);
    ctxt->myDoc = NULL;

    ctxt->standalone = -1;
    ctxt->hasExternalSubset = 0;
    ctxt->hasPErefs = 0;
    ctxt->html = 1;
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

/**
 * htmlCtxtUseOptions:
 * @ctxt: an HTML parser context
 * @options:  a combination of htmlParserOption(s)
 *
 * Applies the options to the parser context
 *
 * Returns 0 in case of success, the set of unknown or unimplemented options
 *         in case of error.
 */
int
htmlCtxtUseOptions(htmlParserCtxtPtr ctxt, int options)
{
    if (ctxt == NULL)
        return(-1);

    if (options & HTML_PARSE_NOWARNING) {
        ctxt->sax->warning = NULL;
        ctxt->vctxt.warning = NULL;
        options -= XML_PARSE_NOWARNING;
	ctxt->options |= XML_PARSE_NOWARNING;
    }
    if (options & HTML_PARSE_NOERROR) {
        ctxt->sax->error = NULL;
        ctxt->vctxt.error = NULL;
        ctxt->sax->fatalError = NULL;
        options -= XML_PARSE_NOERROR;
	ctxt->options |= XML_PARSE_NOERROR;
    }
    if (options & HTML_PARSE_PEDANTIC) {
        ctxt->pedantic = 1;
        options -= XML_PARSE_PEDANTIC;
	ctxt->options |= XML_PARSE_PEDANTIC;
    } else
        ctxt->pedantic = 0;
    if (options & XML_PARSE_NOBLANKS) {
        ctxt->keepBlanks = 0;
        ctxt->sax->ignorableWhitespace = xmlSAX2IgnorableWhitespace;
        options -= XML_PARSE_NOBLANKS;
	ctxt->options |= XML_PARSE_NOBLANKS;
    } else
        ctxt->keepBlanks = 1;
    if (options & HTML_PARSE_RECOVER) {
        ctxt->recovery = 1;
	options -= HTML_PARSE_RECOVER;
    } else
        ctxt->recovery = 0;
    if (options & HTML_PARSE_COMPACT) {
	ctxt->options |= HTML_PARSE_COMPACT;
        options -= HTML_PARSE_COMPACT;
    }
    if (options & XML_PARSE_HUGE) {
	ctxt->options |= XML_PARSE_HUGE;
        options -= XML_PARSE_HUGE;
    }
    if (options & HTML_PARSE_NODEFDTD) {
	ctxt->options |= HTML_PARSE_NODEFDTD;
        options -= HTML_PARSE_NODEFDTD;
    }
    if (options & HTML_PARSE_IGNORE_ENC) {
	ctxt->options |= HTML_PARSE_IGNORE_ENC;
        options -= HTML_PARSE_IGNORE_ENC;
    }
    if (options & HTML_PARSE_NOIMPLIED) {
        ctxt->options |= HTML_PARSE_NOIMPLIED;
        options -= HTML_PARSE_NOIMPLIED;
    }
    ctxt->dictNames = 0;
    ctxt->linenumbers = 1;
    return (options);
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

    if ((ctxt == NULL) || (input == NULL))
        return(NULL);

    /* assert(ctxt->inputNr == 0); */
    while (ctxt->inputNr > 0)
        xmlFreeInputStream(inputPop(ctxt));

    if (inputPush(ctxt, input) < 0) {
        xmlFreeInputStream(input);
        return(NULL);
    }

    ctxt->html = 1;
    htmlParseDocument(ctxt);

    if (ctxt->errNo != XML_ERR_NO_MEMORY) {
        ret = ctxt->myDoc;
    } else {
        ret = NULL;
        xmlFreeDoc(ctxt->myDoc);
    }
    ctxt->myDoc = NULL;

    /* assert(ctxt->inputNr == 1); */
    while (ctxt->inputNr > 0)
        xmlFreeInputStream(inputPop(ctxt));

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
    htmlDocPtr doc;

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
        return(NULL);

    htmlCtxtUseOptions(ctxt, options);

    input = xmlNewInputString(ctxt, url, (const char *) str, encoding,
                              XML_INPUT_BUF_STATIC);

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
    htmlDocPtr doc;

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
        return(NULL);

    htmlCtxtUseOptions(ctxt, options);

    input = xmlNewInputURL(ctxt, filename, NULL, encoding, 0);

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
    htmlDocPtr doc;

    if (size < 0)
	return(NULL);

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
        return(NULL);

    htmlCtxtUseOptions(ctxt, options);

    input = xmlNewInputMemory(ctxt, url, buffer, size, encoding,
                              XML_INPUT_BUF_STATIC);

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
    htmlDocPtr doc;

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
        return(NULL);

    htmlCtxtUseOptions(ctxt, options);

    input = xmlNewInputFd(ctxt, url, fd, encoding, 0);

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
    htmlDocPtr doc;

    ctxt = htmlNewParserCtxt();
    if (ctxt == NULL)
        return (NULL);

    htmlCtxtUseOptions(ctxt, options);

    input = xmlNewInputIO(ctxt, url, ioread, ioclose, ioctx, encoding, 0);

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

    input = xmlNewInputString(ctxt, URL, (const char *) str, encoding, 0);

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
 * See xmlNewInputURL and htmlCtxtUseOptions for details.
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

    input = xmlNewInputURL(ctxt, filename, NULL, encoding, 0);

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

    input = xmlNewInputMemory(ctxt, URL, buffer, size, encoding,
                              XML_INPUT_BUF_STATIC);

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

    input = xmlNewInputFd(ctxt, URL, fd, encoding, 0);

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
 * See xmlNewInputIO and htmlCtxtUseOptions for details.
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

    input = xmlNewInputIO(ctxt, URL, ioread, ioclose, ioctx, encoding, 0);

    return(htmlCtxtParseDocument(ctxt, input));
}

#endif /* LIBXML_HTML_ENABLED */
