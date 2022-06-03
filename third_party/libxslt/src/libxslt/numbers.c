/*
 * numbers.c: Implementation of the XSLT number functions
 *
 * Reference:
 *   http://www.w3.org/TR/1999/REC-xslt-19991116
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 * Bjorn Reese <breese@users.sourceforge.net>
 */

#define IN_LIBXSLT
#include "libxslt.h"

#include <math.h>
#include <limits.h>
#include <float.h>
#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parserInternals.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/encoding.h>
#include "xsltutils.h"
#include "pattern.h"
#include "templates.h"
#include "transform.h"
#include "numbersInternals.h"

#ifndef FALSE
# define FALSE (0 == 1)
# define TRUE (1 == 1)
#endif

#define SYMBOL_QUOTE		((xmlChar)'\'')

#define DEFAULT_TOKEN		'0'
#define DEFAULT_SEPARATOR	"."

#define MAX_TOKENS		1024

typedef struct _xsltFormatToken xsltFormatToken;
typedef xsltFormatToken *xsltFormatTokenPtr;
struct _xsltFormatToken {
    xmlChar	*separator;
    int		 token;
    int		 width;
};

typedef struct _xsltFormat xsltFormat;
typedef xsltFormat *xsltFormatPtr;
struct _xsltFormat {
    xmlChar		*start;
    xsltFormatToken	 tokens[MAX_TOKENS];
    int			 nTokens;
    xmlChar		*end;
};

static char alpha_upper_list[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static char alpha_lower_list[] = "abcdefghijklmnopqrstuvwxyz";
static xsltFormatToken default_token;

/*
 * **** Start temp insert ****
 *
 * The following routine xsltUTF8Charcmp will be replaced with calls to
 * the corresponding libxml routine at a later date (when other
 * inter-library dependencies require it).
 */

/**
 * xsltUTF8Charcmp
 * @utf1: pointer to first UTF8 char
 * @utf2: pointer to second UTF8 char
 *
 * returns result of comparing the two UCS4 values
 * as with xmlStrncmp
 */
static int
xsltUTF8Charcmp(xmlChar *utf1, xmlChar *utf2) {
    int len = xmlUTF8Strsize(utf1, 1);

    if (len < 1)
        return -1;
    if (utf1 == NULL ) {
        if (utf2 == NULL)
            return 0;
        return -1;
    }
    return xmlStrncmp(utf1, utf2, len);
}

/***** Stop temp insert *****/
/************************************************************************
 *									*
 *			Utility functions				*
 *									*
 ************************************************************************/

#define IS_SPECIAL(self,letter)			\
    ((xsltUTF8Charcmp((letter), (self)->zeroDigit) == 0)	    ||	\
     (xsltUTF8Charcmp((letter), (self)->digit) == 0)	    ||	\
     (xsltUTF8Charcmp((letter), (self)->decimalPoint) == 0)  ||	\
     (xsltUTF8Charcmp((letter), (self)->grouping) == 0)	    ||	\
     (xsltUTF8Charcmp((letter), (self)->patternSeparator) == 0))

#define IS_DIGIT_ZERO(x) xsltIsDigitZero(x)
#define IS_DIGIT_ONE(x) xsltIsDigitZero((x)-1)

static int
xsltIsDigitZero(unsigned int ch)
{
    /*
     * Reference: ftp://ftp.unicode.org/Public/UNIDATA/UnicodeData.txt
     *
     * There a many more digit ranges in newer Unicode versions. These
     * are only the zeros that match Digit in XML 1.0 (IS_DIGIT macro).
     */
    switch (ch) {
    case 0x0030: case 0x0660: case 0x06F0: case 0x0966:
    case 0x09E6: case 0x0A66: case 0x0AE6: case 0x0B66:
    case 0x0C66: case 0x0CE6: case 0x0D66: case 0x0E50:
    case 0x0ED0: case 0x0F20:
	return TRUE;
    default:
	return FALSE;
    }
}

static void
xsltNumberFormatDecimal(xmlBufferPtr buffer,
			double number,
			int digit_zero,
			int width,
			int digitsPerGroup,
			int groupingCharacter,
			int groupingCharacterLen)
{
    /*
     * This used to be
     *  xmlChar temp_string[sizeof(double) * CHAR_BIT * sizeof(xmlChar) + 4];
     * which would be length 68 on x86 arch.  It was changed to be a longer,
     * fixed length in order to try to cater for (reasonable) UTF8
     * separators and numeric characters.  The max UTF8 char size will be
     * 6 or less, so the value used [500] should be *much* larger than needed
     */
    xmlChar temp_string[500];
    xmlChar *pointer;
    xmlChar temp_char[6];
    int i;
    int val;
    int len;

    /* Build buffer from back */
    pointer = &temp_string[sizeof(temp_string)] - 1;	/* last char */
    *pointer = 0;
    i = 0;
    while (pointer > temp_string) {
	if ((i >= width) && (fabs(number) < 1.0))
	    break; /* for */
	if ((i > 0) && (groupingCharacter != 0) &&
	    (digitsPerGroup > 0) &&
	    ((i % digitsPerGroup) == 0)) {
	    if (pointer - groupingCharacterLen < temp_string) {
	        i = -1;		/* flag error */
		break;
	    }
	    pointer -= groupingCharacterLen;
	    xmlCopyCharMultiByte(pointer, groupingCharacter);
	}

	val = digit_zero + (int)fmod(number, 10.0);
	if (val < 0x80) {			/* shortcut if ASCII */
	    if (pointer <= temp_string) {	/* Check enough room */
	        i = -1;
		break;
	    }
	    *(--pointer) = (xmlChar)val;
	}
	else {
	/*
	 * Here we have a multibyte character.  It's a little messy,
	 * because until we generate the char we don't know how long
	 * it is.  So, we generate it into the buffer temp_char, then
	 * copy from there into temp_string.
	 */
	    len = xmlCopyCharMultiByte(temp_char, val);
	    if ( (pointer - len) < temp_string ) {
	        i = -1;
		break;
	    }
	    pointer -= len;
	    memcpy(pointer, temp_char, len);
	}
	number /= 10.0;
	++i;
    }
    if (i < 0)
        xsltGenericError(xsltGenericErrorContext,
		"xsltNumberFormatDecimal: Internal buffer size exceeded\n");
    xmlBufferCat(buffer, pointer);
}

static void
xsltNumberFormatAlpha(xsltNumberDataPtr data,
		      xmlBufferPtr buffer,
		      double number,
		      int is_upper)
{
    char temp_string[sizeof(double) * CHAR_BIT * sizeof(xmlChar) + 1];
    char *pointer;
    int i;
    char *alpha_list;
    double alpha_size = (double)(sizeof(alpha_upper_list) - 1);

    /*
     * XSLT 1.0 isn't clear on how to handle zero, but XSLT 2.0 says:
     *
     *     For all format tokens other than the first kind above (one that
     *     consists of decimal digits), there may be implementation-defined
     *     lower and upper bounds on the range of numbers that can be
     *     formatted using this format token; indeed, for some numbering
     *     sequences there may be intrinsic limits. [...] Numbers that fall
     *     outside this range must be formatted using the format token 1.
     *
     * The "a" token has an intrinsic lower limit of 1.
     */
    if (number < 1.0) {
        xsltNumberFormatDecimal(buffer, number, '0', 1,
                                data->digitsPerGroup,
                                data->groupingCharacter,
                                data->groupingCharacterLen);
        return;
    }

    /* Build buffer from back */
    pointer = &temp_string[sizeof(temp_string)];
    *(--pointer) = 0;
    alpha_list = (is_upper) ? alpha_upper_list : alpha_lower_list;

    for (i = 1; i < (int)sizeof(temp_string); i++) {
	number--;
	*(--pointer) = alpha_list[((int)fmod(number, alpha_size))];
	number /= alpha_size;
	if (number < 1.0)
	    break; /* for */
    }
    xmlBufferCCat(buffer, pointer);
}

static void
xsltNumberFormatRoman(xsltNumberDataPtr data,
		      xmlBufferPtr buffer,
		      double number,
		      int is_upper)
{
    /*
     * See discussion in xsltNumberFormatAlpha. Also use a reasonable upper
     * bound to avoid denial of service.
     */
    if (number < 1.0 || number > 5000.0) {
        xsltNumberFormatDecimal(buffer, number, '0', 1,
                                data->digitsPerGroup,
                                data->groupingCharacter,
                                data->groupingCharacterLen);
        return;
    }

    /*
     * Based on an example by Jim Walsh
     */
    while (number >= 1000.0) {
	xmlBufferCCat(buffer, (is_upper) ? "M" : "m");
	number -= 1000.0;
    }
    if (number >= 900.0) {
	xmlBufferCCat(buffer, (is_upper) ? "CM" : "cm");
	number -= 900.0;
    }
    while (number >= 500.0) {
	xmlBufferCCat(buffer, (is_upper) ? "D" : "d");
	number -= 500.0;
    }
    if (number >= 400.0) {
	xmlBufferCCat(buffer, (is_upper) ? "CD" : "cd");
	number -= 400.0;
    }
    while (number >= 100.0) {
	xmlBufferCCat(buffer, (is_upper) ? "C" : "c");
	number -= 100.0;
    }
    if (number >= 90.0) {
	xmlBufferCCat(buffer, (is_upper) ? "XC" : "xc");
	number -= 90.0;
    }
    while (number >= 50.0) {
	xmlBufferCCat(buffer, (is_upper) ? "L" : "l");
	number -= 50.0;
    }
    if (number >= 40.0) {
	xmlBufferCCat(buffer, (is_upper) ? "XL" : "xl");
	number -= 40.0;
    }
    while (number >= 10.0) {
	xmlBufferCCat(buffer, (is_upper) ? "X" : "x");
	number -= 10.0;
    }
    if (number >= 9.0) {
	xmlBufferCCat(buffer, (is_upper) ? "IX" : "ix");
	number -= 9.0;
    }
    while (number >= 5.0) {
	xmlBufferCCat(buffer, (is_upper) ? "V" : "v");
	number -= 5.0;
    }
    if (number >= 4.0) {
	xmlBufferCCat(buffer, (is_upper) ? "IV" : "iv");
	number -= 4.0;
    }
    while (number >= 1.0) {
	xmlBufferCCat(buffer, (is_upper) ? "I" : "i");
	number--;
    }
}

static void
xsltNumberFormatTokenize(const xmlChar *format,
			 xsltFormatPtr tokens)
{
    int ix = 0;
    int j;
    int val;
    int len;

    default_token.token = DEFAULT_TOKEN;
    default_token.width = 1;
    default_token.separator = BAD_CAST(DEFAULT_SEPARATOR);


    tokens->start = NULL;
    tokens->tokens[0].separator = NULL;
    tokens->end = NULL;

    /*
     * Insert initial non-alphanumeric token.
     * There is always such a token in the list, even if NULL
     */
    while (! (IS_LETTER(val=xmlStringCurrentChar(NULL, format+ix, &len)) ||
	      IS_DIGIT(val)) ) {
	if (format[ix] == 0)		/* if end of format string */
	    break; /* while */
	ix += len;
    }
    if (ix > 0)
	tokens->start = xmlStrndup(format, ix);


    for (tokens->nTokens = 0; tokens->nTokens < MAX_TOKENS;
	 tokens->nTokens++) {
	if (format[ix] == 0)
	    break; /* for */

	/*
	 * separator has already been parsed (except for the first
	 * number) in tokens->end, recover it.
	 */
	if (tokens->nTokens > 0) {
	    tokens->tokens[tokens->nTokens].separator = tokens->end;
	    tokens->end = NULL;
	}

	val = xmlStringCurrentChar(NULL, format+ix, &len);
	if (IS_DIGIT_ONE(val) ||
		 IS_DIGIT_ZERO(val)) {
	    tokens->tokens[tokens->nTokens].width = 1;
	    while (IS_DIGIT_ZERO(val)) {
		tokens->tokens[tokens->nTokens].width++;
		ix += len;
		val = xmlStringCurrentChar(NULL, format+ix, &len);
	    }
	    if (IS_DIGIT_ONE(val)) {
		tokens->tokens[tokens->nTokens].token = val - 1;
		ix += len;
		val = xmlStringCurrentChar(NULL, format+ix, &len);
	    } else {
                tokens->tokens[tokens->nTokens].token = '0';
                tokens->tokens[tokens->nTokens].width = 1;
            }
	} else if ( (val == 'A') ||
		    (val == 'a') ||
		    (val == 'I') ||
		    (val == 'i') ) {
	    tokens->tokens[tokens->nTokens].token = val;
	    ix += len;
	    val = xmlStringCurrentChar(NULL, format+ix, &len);
	} else {
	    /* XSLT section 7.7
	     * "Any other format token indicates a numbering sequence
	     *  that starts with that token. If an implementation does
	     *  not support a numbering sequence that starts with that
	     *  token, it must use a format token of 1."
	     */
	    tokens->tokens[tokens->nTokens].token = '0';
	    tokens->tokens[tokens->nTokens].width = 1;
	}
	/*
	 * Skip over remaining alphanumeric characters from the Nd
	 * (Number, decimal digit), Nl (Number, letter), No (Number,
	 * other), Lu (Letter, uppercase), Ll (Letter, lowercase), Lt
	 * (Letters, titlecase), Lm (Letters, modifiers), and Lo
	 * (Letters, other (uncased)) Unicode categories. This happens
	 * to correspond to the Letter and Digit classes from XML (and
	 * one wonders why XSLT doesn't refer to these instead).
	 */
	while (IS_LETTER(val) || IS_DIGIT(val)) {
	    ix += len;
	    val = xmlStringCurrentChar(NULL, format+ix, &len);
	}

	/*
	 * Insert temporary non-alphanumeric final tooken.
	 */
	j = ix;
	while (! (IS_LETTER(val) || IS_DIGIT(val))) {
	    if (val == 0)
		break; /* while */
	    ix += len;
	    val = xmlStringCurrentChar(NULL, format+ix, &len);
	}
	if (ix > j)
	    tokens->end = xmlStrndup(&format[j], ix - j);
    }
}

static void
xsltNumberFormatInsertNumbers(xsltNumberDataPtr data,
			      double *numbers,
			      int numbers_max,
			      xsltFormatPtr tokens,
			      xmlBufferPtr buffer)
{
    int i = 0;
    double number;
    xsltFormatTokenPtr token;

    /*
     * Handle initial non-alphanumeric token
     */
    if (tokens->start != NULL)
	 xmlBufferCat(buffer, tokens->start);

    for (i = 0; i < numbers_max; i++) {
	/* Insert number */
	number = numbers[(numbers_max - 1) - i];
        /* Round to nearest like XSLT 2.0 */
        number = floor(number + 0.5);
        /*
         * XSLT 1.0 isn't clear on how to handle negative numbers, but XSLT
         * 2.0 says:
         *
         *     It is a non-recoverable dynamic error if any undiscarded item
         *     in the atomized sequence supplied as the value of the value
         *     attribute of xsl:number cannot be converted to an integer, or
         *     if the resulting integer is less than 0 (zero).
         */
        if (number < 0.0) {
            xsltTransformError(NULL, NULL, NULL,
                    "xsl-number : negative value\n");
            /* Recover by treating negative values as zero. */
            number = 0.0;
        }
	if (i < tokens->nTokens) {
	  /*
	   * The "n"th format token will be used to format the "n"th
	   * number in the list
	   */
	  token = &(tokens->tokens[i]);
	} else if (tokens->nTokens > 0) {
	  /*
	   * If there are more numbers than format tokens, then the
	   * last format token will be used to format the remaining
	   * numbers.
	   */
	  token = &(tokens->tokens[tokens->nTokens - 1]);
	} else {
	  /*
	   * If there are no format tokens, then a format token of
	   * 1 is used to format all numbers.
	   */
	  token = &default_token;
	}

	/* Print separator, except for the first number */
	if (i > 0) {
	    if (token->separator != NULL)
		xmlBufferCat(buffer, token->separator);
	    else
		xmlBufferCCat(buffer, DEFAULT_SEPARATOR);
	}

	switch (xmlXPathIsInf(number)) {
	case -1:
	    xmlBufferCCat(buffer, "-Infinity");
	    break;
	case 1:
	    xmlBufferCCat(buffer, "Infinity");
	    break;
	default:
	    if (xmlXPathIsNaN(number)) {
		xmlBufferCCat(buffer, "NaN");
	    } else {

		switch (token->token) {
		case 'A':
		    xsltNumberFormatAlpha(data, buffer, number, TRUE);
		    break;
		case 'a':
		    xsltNumberFormatAlpha(data, buffer, number, FALSE);
		    break;
		case 'I':
		    xsltNumberFormatRoman(data, buffer, number, TRUE);
		    break;
		case 'i':
		    xsltNumberFormatRoman(data, buffer, number, FALSE);
		    break;
		default:
		    if (IS_DIGIT_ZERO(token->token)) {
			xsltNumberFormatDecimal(buffer,
						number,
						token->token,
						token->width,
						data->digitsPerGroup,
						data->groupingCharacter,
						data->groupingCharacterLen);
		    }
		    break;
		}
	    }

	}
    }

    /*
     * Handle final non-alphanumeric token
     */
    if (tokens->end != NULL)
	 xmlBufferCat(buffer, tokens->end);

}

static int
xsltTestCompMatchCount(xsltTransformContextPtr context,
                       xmlNodePtr node,
                       xsltCompMatchPtr countPat,
                       xmlNodePtr cur)
{
    if (countPat != NULL) {
        return xsltTestCompMatchList(context, node, countPat);
    }
    else {
        /*
         * 7.7 Numbering
         *
         * If count attribute is not specified, then it defaults to the
         * pattern that matches any node with the same node type as the
         * current node and, if the current node has an expanded-name, with
         * the same expanded-name as the current node.
         */
        if (node->type != cur->type)
            return 0;
        if (node->type == XML_NAMESPACE_DECL)
            /*
             * Namespace nodes have no preceding siblings and no parents
             * that are namespace nodes. This means that node == cur.
             */
            return 1;
        /* TODO: Skip node types without expanded names like text nodes. */
        if (!xmlStrEqual(node->name, cur->name))
            return 0;
        if (node->ns == cur->ns)
            return 1;
        if ((node->ns == NULL) || (cur->ns == NULL))
            return 0;
        return (xmlStrEqual(node->ns->href, cur->ns->href));
    }
}

static int
xsltNumberFormatGetAnyLevel(xsltTransformContextPtr context,
			    xmlNodePtr node,
			    xsltCompMatchPtr countPat,
			    xsltCompMatchPtr fromPat,
			    double *array)
{
    int amount = 0;
    int cnt = 0;
    xmlNodePtr cur = node;

    while (cur != NULL) {
	/* process current node */
	if (xsltTestCompMatchCount(context, cur, countPat, node))
	    cnt++;
	if ((fromPat != NULL) &&
	    xsltTestCompMatchList(context, cur, fromPat)) {
	    break; /* while */
	}

	/* Skip to next preceding or ancestor */
	if ((cur->type == XML_DOCUMENT_NODE) ||
#ifdef LIBXML_DOCB_ENABLED
            (cur->type == XML_DOCB_DOCUMENT_NODE) ||
#endif
            (cur->type == XML_HTML_DOCUMENT_NODE))
	    break; /* while */

        if (cur->type == XML_NAMESPACE_DECL) {
            /*
            * The XPath module stores the parent of a namespace node in
            * the ns->next field.
            */
            cur = (xmlNodePtr) ((xmlNsPtr) cur)->next;
        } else if (cur->type == XML_ATTRIBUTE_NODE) {
            cur = cur->parent;
        } else {
            while ((cur->prev != NULL) && ((cur->prev->type == XML_DTD_NODE) ||
                   (cur->prev->type == XML_XINCLUDE_START) ||
                   (cur->prev->type == XML_XINCLUDE_END)))
                cur = cur->prev;
            if (cur->prev != NULL) {
                for (cur = cur->prev; cur->last != NULL; cur = cur->last);
            } else {
                cur = cur->parent;
            }
        }
    }

    array[amount++] = (double) cnt;

    return(amount);
}

static int
xsltNumberFormatGetMultipleLevel(xsltTransformContextPtr context,
				 xmlNodePtr node,
				 xsltCompMatchPtr countPat,
				 xsltCompMatchPtr fromPat,
				 double *array,
				 int max)
{
    int amount = 0;
    int cnt;
    xmlNodePtr oldCtxtNode;
    xmlNodePtr ancestor;
    xmlNodePtr preceding;
    xmlXPathParserContextPtr parser;

    oldCtxtNode = context->xpathCtxt->node;
    parser = xmlXPathNewParserContext(NULL, context->xpathCtxt);
    if (parser) {
	/* ancestor-or-self::*[count] */
	ancestor = node;
	while ((ancestor != NULL) && (ancestor->type != XML_DOCUMENT_NODE)) {
	    if ((fromPat != NULL) &&
		xsltTestCompMatchList(context, ancestor, fromPat))
		break; /* for */

            /*
             * The xmlXPathNext* iterators require that the context node is
             * set to the start node. Calls to xsltTestCompMatch* may also
             * leave the context node in an undefined state, so make sure
             * that the context node is reset before each iterator invocation.
             */

	    if (xsltTestCompMatchCount(context, ancestor, countPat, node)) {
		/* count(preceding-sibling::*) */
		cnt = 1;
                context->xpathCtxt->node = ancestor;
                preceding = xmlXPathNextPrecedingSibling(parser, ancestor);
                while (preceding != NULL) {
	            if (xsltTestCompMatchCount(context, preceding, countPat,
                                               node))
			cnt++;
                    context->xpathCtxt->node = ancestor;
                    preceding =
                        xmlXPathNextPrecedingSibling(parser, preceding);
		}
		array[amount++] = (double)cnt;
		if (amount >= max)
		    break; /* for */
	    }
            context->xpathCtxt->node = node;
            ancestor = xmlXPathNextAncestor(parser, ancestor);
	}
	xmlXPathFreeParserContext(parser);
    }
    context->xpathCtxt->node = oldCtxtNode;
    return amount;
}

static int
xsltNumberFormatGetValue(xmlXPathContextPtr context,
			 xmlNodePtr node,
			 const xmlChar *value,
			 double *number)
{
    int amount = 0;
    xmlBufferPtr pattern;
    xmlXPathObjectPtr obj;

    pattern = xmlBufferCreate();
    if (pattern != NULL) {
	xmlBufferCCat(pattern, "number(");
	xmlBufferCat(pattern, value);
	xmlBufferCCat(pattern, ")");
	context->node = node;
	obj = xmlXPathEvalExpression(xmlBufferContent(pattern),
				     context);
	if (obj != NULL) {
	    *number = obj->floatval;
	    amount++;
	    xmlXPathFreeObject(obj);
	}
	xmlBufferFree(pattern);
    }
    return amount;
}

/**
 * xsltNumberFormat:
 * @ctxt: the XSLT transformation context
 * @data: the formatting information
 * @node: the data to format
 *
 * Convert one number.
 */
void
xsltNumberFormat(xsltTransformContextPtr ctxt,
		 xsltNumberDataPtr data,
		 xmlNodePtr node)
{
    xmlBufferPtr output = NULL;
    int amount, i;
    double number;
    xsltFormat tokens;

    if (data->format != NULL) {
        xsltNumberFormatTokenize(data->format, &tokens);
    }
    else {
        xmlChar *format;

	/* The format needs to be recomputed each time */
        if (data->has_format == 0)
            return;
	format = xsltEvalAttrValueTemplate(ctxt, data->node,
					     (const xmlChar *) "format",
					     XSLT_NAMESPACE);
        if (format == NULL)
            return;
        xsltNumberFormatTokenize(format, &tokens);
	xmlFree(format);
    }

    output = xmlBufferCreate();
    if (output == NULL)
	goto XSLT_NUMBER_FORMAT_END;

    /*
     * Evaluate the XPath expression to find the value(s)
     */
    if (data->value) {
	amount = xsltNumberFormatGetValue(ctxt->xpathCtxt,
					  node,
					  data->value,
					  &number);
	if (amount == 1) {
	    xsltNumberFormatInsertNumbers(data,
					  &number,
					  1,
					  &tokens,
					  output);
	}

    } else if (data->level) {

	if (xmlStrEqual(data->level, (const xmlChar *) "single")) {
	    amount = xsltNumberFormatGetMultipleLevel(ctxt,
						      node,
						      data->countPat,
						      data->fromPat,
						      &number,
						      1);
	    if (amount == 1) {
		xsltNumberFormatInsertNumbers(data,
					      &number,
					      1,
					      &tokens,
					      output);
	    }
	} else if (xmlStrEqual(data->level, (const xmlChar *) "multiple")) {
	    double numarray[1024];
	    int max = sizeof(numarray)/sizeof(numarray[0]);
	    amount = xsltNumberFormatGetMultipleLevel(ctxt,
						      node,
						      data->countPat,
						      data->fromPat,
						      numarray,
						      max);
	    if (amount > 0) {
		xsltNumberFormatInsertNumbers(data,
					      numarray,
					      amount,
					      &tokens,
					      output);
	    }
	} else if (xmlStrEqual(data->level, (const xmlChar *) "any")) {
	    amount = xsltNumberFormatGetAnyLevel(ctxt,
						 node,
						 data->countPat,
						 data->fromPat,
						 &number);
	    if (amount > 0) {
		xsltNumberFormatInsertNumbers(data,
					      &number,
					      1,
					      &tokens,
					      output);
	    }
	}

        /*
         * Unlike `match` patterns, `count` and `from` patterns can contain
         * variable references, so we have to clear the pattern match
         * cache if the "direct" matching algorithm was used.
         */
        if (data->countPat != NULL)
            xsltCompMatchClearCache(ctxt, data->countPat);
        if (data->fromPat != NULL)
            xsltCompMatchClearCache(ctxt, data->fromPat);
    }
    /* Insert number as text node */
    xsltCopyTextString(ctxt, ctxt->insert, xmlBufferContent(output), 0);

    xmlBufferFree(output);

XSLT_NUMBER_FORMAT_END:
    if (tokens.start != NULL)
	xmlFree(tokens.start);
    if (tokens.end != NULL)
	xmlFree(tokens.end);
    for (i = 0;i < tokens.nTokens;i++) {
	if (tokens.tokens[i].separator != NULL)
	    xmlFree(tokens.tokens[i].separator);
    }
}

static int
xsltFormatNumberPreSuffix(xsltDecimalFormatPtr self, xmlChar **format, xsltFormatNumberInfoPtr info)
{
    /* will hold total length of prefix/suffix without quote characters */
    int	count=0;
    int len;

    while (1) {
	/*
	 * prefix / suffix ends at end of string or at
	 * first 'special' character
	 */
	if (**format == 0)
	    return count;
	/* if next character 'escaped' just count it */
	if (**format == SYMBOL_QUOTE) {
	    if (*++(*format) == 0)
		return -1;
	}
	else if (IS_SPECIAL(self, *format))
	    return count;
	/*
	 * else treat percent/per-mille as special cases,
	 * depending on whether +ve or -ve
	 */
	else {
	    /*
	     * for +ve prefix/suffix, allow only a
	     * single occurence of either
	     */
	    if (xsltUTF8Charcmp(*format, self->percent) == 0) {
		if (info->is_multiplier_set)
		    return -1;
		info->multiplier = 100;
		info->is_multiplier_set = TRUE;
	    } else if (xsltUTF8Charcmp(*format, self->permille) == 0) {
		if (info->is_multiplier_set)
		    return -1;
		info->multiplier = 1000;
		info->is_multiplier_set = TRUE;
	    }
	}

	if ((len=xmlUTF8Strsize(*format, 1)) < 1)
	    return -1;
	count += len;
	*format += len;
    }
}

/**
 * xsltFormatNumberConversion:
 * @self: the decimal format
 * @format: the format requested
 * @number: the value to format
 * @result: the place to output the result
 *
 * format-number() uses the JDK 1.1 DecimalFormat class:
 *
 * http://java.sun.com/products/jdk/1.1/docs/api/java.text.DecimalFormat.html
 *
 * Structure:
 *
 *   pattern    := subpattern{;subpattern}
 *   subpattern := {prefix}integer{.fraction}{suffix}
 *   prefix     := '\\u0000'..'\\uFFFD' - specialCharacters
 *   suffix     := '\\u0000'..'\\uFFFD' - specialCharacters
 *   integer    := '#'* '0'* '0'
 *   fraction   := '0'* '#'*
 *
 *   Notation:
 *    X*       0 or more instances of X
 *    (X | Y)  either X or Y.
 *    X..Y     any character from X up to Y, inclusive.
 *    S - T    characters in S, except those in T
 *
 * Special Characters:
 *
 *   Symbol Meaning
 *   0      a digit
 *   #      a digit, zero shows as absent
 *   .      placeholder for decimal separator
 *   ,      placeholder for grouping separator.
 *   ;      separates formats.
 *   -      default negative prefix.
 *   %      multiply by 100 and show as percentage
 *   ?      multiply by 1000 and show as per mille
 *   X      any other characters can be used in the prefix or suffix
 *   '      used to quote special characters in a prefix or suffix.
 *
 * Returns a possible XPath error
 */
xmlXPathError
xsltFormatNumberConversion(xsltDecimalFormatPtr self,
			   xmlChar *format,
			   double number,
			   xmlChar **result)
{
    xmlXPathError status = XPATH_EXPRESSION_OK;
    xmlBufferPtr buffer;
    xmlChar *the_format, *prefix = NULL, *suffix = NULL;
    xmlChar *nprefix, *nsuffix = NULL;
    int	    prefix_length, suffix_length = 0, nprefix_length, nsuffix_length;
    double  scale;
    int	    j, len = 0;
    int     self_grouping_len;
    xsltFormatNumberInfo format_info;
    /*
     * delayed_multiplier allows a 'trailing' percent or
     * permille to be treated as suffix
     */
    int		delayed_multiplier = 0;
    /* flag to show no -ve format present for -ve number */
    char	default_sign = 0;
    /* flag to show error found, should use default format */
    char	found_error = 0;

    if (xmlStrlen(format) <= 0) {
	xsltTransformError(NULL, NULL, NULL,
                "xsltFormatNumberConversion : "
		"Invalid format (0-length)\n");
    }
    *result = NULL;
    switch (xmlXPathIsInf(number)) {
	case -1:
	    if (self->minusSign == NULL)
		*result = xmlStrdup(BAD_CAST "-");
	    else
		*result = xmlStrdup(self->minusSign);
	    /* Intentional fall-through */
	case 1:
	    if ((self == NULL) || (self->infinity == NULL))
		*result = xmlStrcat(*result, BAD_CAST "Infinity");
	    else
		*result = xmlStrcat(*result, self->infinity);
	    return(status);
	default:
	    if (xmlXPathIsNaN(number)) {
		if ((self == NULL) || (self->noNumber == NULL))
		    *result = xmlStrdup(BAD_CAST "NaN");
		else
		    *result = xmlStrdup(self->noNumber);
		return(status);
	    }
    }

    buffer = xmlBufferCreate();
    if (buffer == NULL) {
	return XPATH_MEMORY_ERROR;
    }

    format_info.integer_hash = 0;
    format_info.integer_digits = 0;
    format_info.frac_digits = 0;
    format_info.frac_hash = 0;
    format_info.group = -1;
    format_info.multiplier = 1;
    format_info.add_decimal = FALSE;
    format_info.is_multiplier_set = FALSE;
    format_info.is_negative_pattern = FALSE;

    the_format = format;

    /*
     * First we process the +ve pattern to get percent / permille,
     * as well as main format
     */
    prefix = the_format;
    prefix_length = xsltFormatNumberPreSuffix(self, &the_format, &format_info);
    if (prefix_length < 0) {
	found_error = 1;
	goto OUTPUT_NUMBER;
    }

    /*
     * Here we process the "number" part of the format.  It gets
     * a little messy because of the percent/per-mille - if that
     * appears at the end, it may be part of the suffix instead
     * of part of the number, so the variable delayed_multiplier
     * is used to handle it
     */
    self_grouping_len = xmlStrlen(self->grouping);
    while ((*the_format != 0) &&
	   (xsltUTF8Charcmp(the_format, self->decimalPoint) != 0) &&
	   (xsltUTF8Charcmp(the_format, self->patternSeparator) != 0)) {

	if (delayed_multiplier != 0) {
	    format_info.multiplier = delayed_multiplier;
	    format_info.is_multiplier_set = TRUE;
	    delayed_multiplier = 0;
	}
	if (xsltUTF8Charcmp(the_format, self->digit) == 0) {
	    if (format_info.integer_digits > 0) {
		found_error = 1;
		goto OUTPUT_NUMBER;
	    }
	    format_info.integer_hash++;
	    if (format_info.group >= 0)
		format_info.group++;
	} else if (xsltUTF8Charcmp(the_format, self->zeroDigit) == 0) {
	    format_info.integer_digits++;
	    if (format_info.group >= 0)
		format_info.group++;
	} else if ((self_grouping_len > 0) &&
	    (!xmlStrncmp(the_format, self->grouping, self_grouping_len))) {
	    /* Reset group count */
	    format_info.group = 0;
	    the_format += self_grouping_len;
	    continue;
	} else if (xsltUTF8Charcmp(the_format, self->percent) == 0) {
	    if (format_info.is_multiplier_set) {
		found_error = 1;
		goto OUTPUT_NUMBER;
	    }
	    delayed_multiplier = 100;
	} else  if (xsltUTF8Charcmp(the_format, self->permille) == 0) {
	    if (format_info.is_multiplier_set) {
		found_error = 1;
		goto OUTPUT_NUMBER;
	    }
	    delayed_multiplier = 1000;
	} else
	    break; /* while */

	if ((len=xmlUTF8Strsize(the_format, 1)) < 1) {
	    found_error = 1;
	    goto OUTPUT_NUMBER;
	}
	the_format += len;

    }

    /* We have finished the integer part, now work on fraction */
    if ( (*the_format != 0) &&
         (xsltUTF8Charcmp(the_format, self->decimalPoint) == 0) ) {
        format_info.add_decimal = TRUE;
        if ((len = xmlUTF8Strsize(the_format, 1)) < 1) {
            found_error = 1;
            goto OUTPUT_NUMBER;
        }
	the_format += len;	/* Skip over the decimal */
    }

    while (*the_format != 0) {

	if (xsltUTF8Charcmp(the_format, self->zeroDigit) == 0) {
	    if (format_info.frac_hash != 0) {
		found_error = 1;
		goto OUTPUT_NUMBER;
	    }
	    format_info.frac_digits++;
	} else if (xsltUTF8Charcmp(the_format, self->digit) == 0) {
	    format_info.frac_hash++;
	} else if (xsltUTF8Charcmp(the_format, self->percent) == 0) {
	    if (format_info.is_multiplier_set) {
		found_error = 1;
		goto OUTPUT_NUMBER;
	    }
	    delayed_multiplier = 100;
	    if ((len = xmlUTF8Strsize(the_format, 1)) < 1) {
	        found_error = 1;
		goto OUTPUT_NUMBER;
	    }
	    the_format += len;
	    continue; /* while */
	} else if (xsltUTF8Charcmp(the_format, self->permille) == 0) {
	    if (format_info.is_multiplier_set) {
		found_error = 1;
		goto OUTPUT_NUMBER;
	    }
	    delayed_multiplier = 1000;
	    if  ((len = xmlUTF8Strsize(the_format, 1)) < 1) {
	        found_error = 1;
		goto OUTPUT_NUMBER;
	    }
	    the_format += len;
	    continue; /* while */
	} else if (xsltUTF8Charcmp(the_format, self->grouping) != 0) {
	    break; /* while */
	}
	if ((len = xmlUTF8Strsize(the_format, 1)) < 1) {
	    found_error = 1;
	    goto OUTPUT_NUMBER;
	}
	the_format += len;
	if (delayed_multiplier != 0) {
	    format_info.multiplier = delayed_multiplier;
	    delayed_multiplier = 0;
	    format_info.is_multiplier_set = TRUE;
	}
    }

    /*
     * If delayed_multiplier is set after processing the
     * "number" part, should be in suffix
     */
    if (delayed_multiplier != 0) {
	the_format -= len;
	delayed_multiplier = 0;
    }

    suffix = the_format;
    suffix_length = xsltFormatNumberPreSuffix(self, &the_format, &format_info);
    if ( (suffix_length < 0) ||
	 ((*the_format != 0) &&
	  (xsltUTF8Charcmp(the_format, self->patternSeparator) != 0)) ) {
	found_error = 1;
	goto OUTPUT_NUMBER;
    }

    /*
     * We have processed the +ve prefix, number part and +ve suffix.
     * If the number is -ve, we must substitute the -ve prefix / suffix
     */
    if (number < 0) {
        /*
	 * Note that j is the number of UTF8 chars before the separator,
	 * not the number of bytes! (bug 151975)
	 */
        j =  xmlUTF8Strloc(format, self->patternSeparator);
	if (j < 0) {
	/* No -ve pattern present, so use default signing */
	    default_sign = 1;
	}
	else {
	    /* Skip over pattern separator (accounting for UTF8) */
	    the_format = (xmlChar *)xmlUTF8Strpos(format, j + 1);
	    /*
	     * Flag changes interpretation of percent/permille
	     * in -ve pattern
	     */
	    format_info.is_negative_pattern = TRUE;
	    format_info.is_multiplier_set = FALSE;

	    /* First do the -ve prefix */
	    nprefix = the_format;
	    nprefix_length = xsltFormatNumberPreSuffix(self,
					&the_format, &format_info);
	    if (nprefix_length<0) {
		found_error = 1;
		goto OUTPUT_NUMBER;
	    }

	    while (*the_format != 0) {
		if ( (xsltUTF8Charcmp(the_format, (self)->percent) == 0) ||
		     (xsltUTF8Charcmp(the_format, (self)->permille)== 0) ) {
		    if (format_info.is_multiplier_set) {
			found_error = 1;
			goto OUTPUT_NUMBER;
		    }
		    format_info.is_multiplier_set = TRUE;
		    delayed_multiplier = 1;
		}
		else if (IS_SPECIAL(self, the_format))
		    delayed_multiplier = 0;
		else
		    break; /* while */
		if ((len = xmlUTF8Strsize(the_format, 1)) < 1) {
		    found_error = 1;
		    goto OUTPUT_NUMBER;
		}
		the_format += len;
	    }
	    if (delayed_multiplier != 0) {
		format_info.is_multiplier_set = FALSE;
		the_format -= len;
	    }

	    /* Finally do the -ve suffix */
	    if (*the_format != 0) {
		nsuffix = the_format;
		nsuffix_length = xsltFormatNumberPreSuffix(self,
					&the_format, &format_info);
		if (nsuffix_length < 0) {
		    found_error = 1;
		    goto OUTPUT_NUMBER;
		}
	    }
	    else
		nsuffix_length = 0;
	    if (*the_format != 0) {
		found_error = 1;
		goto OUTPUT_NUMBER;
	    }
	    /*
	     * Here's another Java peculiarity:
	     * if -ve prefix/suffix == +ve ones, discard & use default
	     */
	    if ((nprefix_length != prefix_length) ||
		(nsuffix_length != suffix_length) ||
		((nprefix_length > 0) &&
		 (xmlStrncmp(nprefix, prefix, prefix_length) !=0 )) ||
		((nsuffix_length > 0) &&
		 (xmlStrncmp(nsuffix, suffix, suffix_length) !=0 ))) {
		prefix = nprefix;
		prefix_length = nprefix_length;
		suffix = nsuffix;
		suffix_length = nsuffix_length;
	    } /* else {
		default_sign = 1;
	    }
	    */
	}
    }

OUTPUT_NUMBER:
    if (found_error != 0) {
	xsltTransformError(NULL, NULL, NULL,
                "xsltFormatNumberConversion : "
		"error in format string '%s', using default\n", format);
	default_sign = (number < 0.0) ? 1 : 0;
	prefix_length = suffix_length = 0;
	format_info.integer_hash = 0;
	format_info.integer_digits = 1;
	format_info.frac_digits = 1;
	format_info.frac_hash = 4;
	format_info.group = -1;
	format_info.multiplier = 1;
	format_info.add_decimal = TRUE;
    }

    /* Ready to output our number.  First see if "default sign" is required */
    if (default_sign != 0)
	xmlBufferAdd(buffer, self->minusSign, xmlUTF8Strsize(self->minusSign, 1));

    /* Put the prefix into the buffer */
    for (j = 0; j < prefix_length; ) {
	if (*prefix == SYMBOL_QUOTE)
            prefix++;
        len = xmlUTF8Strsize(prefix, 1);
        xmlBufferAdd(buffer, prefix, len);
        prefix += len;
        j += len;
    }

    /* Next do the integer part of the number */
    number = fabs(number) * (double)format_info.multiplier;
    scale = pow(10.0, (double)(format_info.frac_digits + format_info.frac_hash));
    number = floor((scale * number + 0.5)) / scale;
    if ((self->grouping != NULL) &&
        (self->grouping[0] != 0)) {
        int gchar;

	len = xmlStrlen(self->grouping);
	gchar = xsltGetUTF8Char(self->grouping, &len);
	xsltNumberFormatDecimal(buffer, floor(number), self->zeroDigit[0],
				format_info.integer_digits,
				format_info.group,
				gchar, len);
    } else
	xsltNumberFormatDecimal(buffer, floor(number), self->zeroDigit[0],
				format_info.integer_digits,
				format_info.group,
				',', 1);

    /* Special case: java treats '.#' like '.0', '.##' like '.0#', etc. */
    if ((format_info.integer_digits + format_info.integer_hash +
	 format_info.frac_digits == 0) && (format_info.frac_hash > 0)) {
        ++format_info.frac_digits;
	--format_info.frac_hash;
    }

    /* Add leading zero, if required */
    if ((floor(number) == 0) &&
	(format_info.integer_digits + format_info.frac_digits == 0)) {
        xmlBufferAdd(buffer, self->zeroDigit, xmlUTF8Strsize(self->zeroDigit, 1));
    }

    /* Next the fractional part, if required */
    if (format_info.frac_digits + format_info.frac_hash == 0) {
        if (format_info.add_decimal)
	    xmlBufferAdd(buffer, self->decimalPoint,
			 xmlUTF8Strsize(self->decimalPoint, 1));
    }
    else {
      number -= floor(number);
	if ((number != 0) || (format_info.frac_digits != 0)) {
	    xmlBufferAdd(buffer, self->decimalPoint,
			 xmlUTF8Strsize(self->decimalPoint, 1));
	    number = floor(scale * number + 0.5);
	    for (j = format_info.frac_hash; j > 0; j--) {
		if (fmod(number, 10.0) >= 1.0)
		    break; /* for */
		number /= 10.0;
	    }
	    xsltNumberFormatDecimal(buffer, floor(number), self->zeroDigit[0],
				format_info.frac_digits + j,
				0, 0, 0);
	}
    }
    /* Put the suffix into the buffer */
    for (j = 0; j < suffix_length; ) {
	if (*suffix == SYMBOL_QUOTE)
            suffix++;
        len = xmlUTF8Strsize(suffix, 1);
        xmlBufferAdd(buffer, suffix, len);
        suffix += len;
        j += len;
    }

    *result = xmlStrdup(xmlBufferContent(buffer));
    xmlBufferFree(buffer);
    return status;
}

