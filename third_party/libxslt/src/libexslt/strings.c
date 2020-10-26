#define IN_LIBEXSLT
#include "libexslt/libexslt.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/parser.h>
#include <libxml/encoding.h>
#include <libxml/uri.h>

#include <libxslt/xsltutils.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/extensions.h>

#include "exslt.h"

/**
 * exsltStrTokenizeFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * Splits up a string on the characters of the delimiter string and returns a
 * node set of token elements, each containing one token from the string.
 */
static void
exsltStrTokenizeFunction(xmlXPathParserContextPtr ctxt, int nargs)
{
    xsltTransformContextPtr tctxt;
    xmlChar *str, *delimiters, *cur;
    const xmlChar *token, *delimiter;
    xmlNodePtr node;
    xmlDocPtr container;
    xmlXPathObjectPtr ret = NULL;
    int clen;

    if ((nargs < 1) || (nargs > 2)) {
        xmlXPathSetArityError(ctxt);
        return;
    }

    if (nargs == 2) {
        delimiters = xmlXPathPopString(ctxt);
        if (xmlXPathCheckError(ctxt))
            return;
    } else {
        delimiters = xmlStrdup((const xmlChar *) "\t\r\n ");
    }
    if (delimiters == NULL)
        return;

    str = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt) || (str == NULL)) {
        xmlFree(delimiters);
        return;
    }

    /* Return a result tree fragment */
    tctxt = xsltXPathGetTransformContext(ctxt);
    if (tctxt == NULL) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
	      "exslt:tokenize : internal error tctxt == NULL\n");
	goto fail;
    }

    container = xsltCreateRVT(tctxt);
    if (container != NULL) {
        xsltRegisterLocalRVT(tctxt, container);
        ret = xmlXPathNewNodeSet(NULL);
        if (ret != NULL) {
            for (cur = str, token = str; *cur != 0; cur += clen) {
	        clen = xmlUTF8Strsize(cur, 1);
		if (*delimiters == 0) {	/* empty string case */
		    xmlChar ctmp;
		    ctmp = *(cur+clen);
		    *(cur+clen) = 0;
                    node = xmlNewDocRawNode(container, NULL,
                                       (const xmlChar *) "token", cur);
		    xmlAddChild((xmlNodePtr) container, node);
		    xmlXPathNodeSetAddUnique(ret->nodesetval, node);
                    *(cur+clen) = ctmp; /* restore the changed byte */
                    token = cur + clen;
                } else for (delimiter = delimiters; *delimiter != 0;
				delimiter += xmlUTF8Strsize(delimiter, 1)) {
                    if (!xmlUTF8Charcmp(cur, delimiter)) {
                        if (cur == token) {
                            /* discard empty tokens */
                            token = cur + clen;
                            break;
                        }
                        *cur = 0;	/* terminate the token */
                        node = xmlNewDocRawNode(container, NULL,
                                           (const xmlChar *) "token", token);
			xmlAddChild((xmlNodePtr) container, node);
			xmlXPathNodeSetAddUnique(ret->nodesetval, node);
                        *cur = *delimiter; /* restore the changed byte */
                        token = cur + clen;
                        break;
                    }
                }
            }
            if (token != cur) {
		node = xmlNewDocRawNode(container, NULL,
				    (const xmlChar *) "token", token);
                xmlAddChild((xmlNodePtr) container, node);
	        xmlXPathNodeSetAddUnique(ret->nodesetval, node);
            }
        }
    }

fail:
    if (str != NULL)
        xmlFree(str);
    if (delimiters != NULL)
        xmlFree(delimiters);
    if (ret != NULL)
        valuePush(ctxt, ret);
    else
        valuePush(ctxt, xmlXPathNewNodeSet(NULL));
}

/**
 * exsltStrSplitFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * Splits up a string on a delimiting string and returns a node set of token
 * elements, each containing one token from the string.
 */
static void
exsltStrSplitFunction(xmlXPathParserContextPtr ctxt, int nargs) {
    xsltTransformContextPtr tctxt;
    xmlChar *str, *delimiter, *cur;
    const xmlChar *token;
    xmlNodePtr node;
    xmlDocPtr container;
    xmlXPathObjectPtr ret = NULL;
    int delimiterLength;

    if ((nargs < 1) || (nargs > 2)) {
        xmlXPathSetArityError(ctxt);
        return;
    }

    if (nargs == 2) {
        delimiter = xmlXPathPopString(ctxt);
        if (xmlXPathCheckError(ctxt))
            return;
    } else {
        delimiter = xmlStrdup((const xmlChar *) " ");
    }
    if (delimiter == NULL)
        return;
    delimiterLength = xmlStrlen (delimiter);

    str = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt) || (str == NULL)) {
        xmlFree(delimiter);
        return;
    }

    /* Return a result tree fragment */
    tctxt = xsltXPathGetTransformContext(ctxt);
    if (tctxt == NULL) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
	      "exslt:tokenize : internal error tctxt == NULL\n");
	goto fail;
    }

    /*
    * OPTIMIZE TODO: We are creating an xmlDoc for every split!
    */
    container = xsltCreateRVT(tctxt);
    if (container != NULL) {
        xsltRegisterLocalRVT(tctxt, container);
        ret = xmlXPathNewNodeSet(NULL);
        if (ret != NULL) {
            for (cur = str, token = str; *cur != 0; cur++) {
		if (delimiterLength == 0) {
		    if (cur != token) {
			xmlChar tmp = *cur;
			*cur = 0;
                        node = xmlNewDocRawNode(container, NULL,
                                           (const xmlChar *) "token", token);
			xmlAddChild((xmlNodePtr) container, node);
			xmlXPathNodeSetAddUnique(ret->nodesetval, node);
			*cur = tmp;
			token++;
		    }
		}
		else if (!xmlStrncasecmp(cur, delimiter, delimiterLength)) {
		    if (cur == token) {
			/* discard empty tokens */
			cur = cur + delimiterLength - 1;
			token = cur + 1;
			continue;
		    }
		    *cur = 0;
		    node = xmlNewDocRawNode(container, NULL,
				       (const xmlChar *) "token", token);
		    xmlAddChild((xmlNodePtr) container, node);
		    xmlXPathNodeSetAddUnique(ret->nodesetval, node);
		    *cur = *delimiter;
		    cur = cur + delimiterLength - 1;
		    token = cur + 1;
                }
            }
	    if (token != cur) {
		node = xmlNewDocRawNode(container, NULL,
				   (const xmlChar *) "token", token);
		xmlAddChild((xmlNodePtr) container, node);
		xmlXPathNodeSetAddUnique(ret->nodesetval, node);
	    }
        }
    }

fail:
    if (str != NULL)
        xmlFree(str);
    if (delimiter != NULL)
        xmlFree(delimiter);
    if (ret != NULL)
        valuePush(ctxt, ret);
    else
        valuePush(ctxt, xmlXPathNewNodeSet(NULL));
}

/**
 * exsltStrEncodeUriFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * URI-Escapes a string
 */
static void
exsltStrEncodeUriFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    int escape_all = 1, str_len = 0;
    xmlChar *str = NULL, *ret = NULL, *tmp;

    if ((nargs < 2) || (nargs > 3)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs >= 3) {
        /* check for UTF-8 if encoding was explicitly given;
           we don't support anything else yet */
        tmp = xmlXPathPopString(ctxt);
        if (xmlUTF8Strlen(tmp) != 5 || xmlStrcmp((const xmlChar *)"UTF-8",tmp)) {
	    xmlXPathReturnEmptyString(ctxt);
	    xmlFree(tmp);
	    return;
	}
	xmlFree(tmp);
    }

    escape_all = xmlXPathPopBoolean(ctxt);

    str = xmlXPathPopString(ctxt);
    str_len = xmlUTF8Strlen(str);

    if (str_len <= 0) {
        if (str_len < 0)
            xsltGenericError(xsltGenericErrorContext,
                             "exsltStrEncodeUriFunction: invalid UTF-8\n");
	xmlXPathReturnEmptyString(ctxt);
	xmlFree(str);
	return;
    }

    ret = xmlURIEscapeStr(str,(const xmlChar *)(escape_all?"-_.!~*'()":"-_.!~*'();/?:@&=+$,[]"));
    xmlXPathReturnString(ctxt, ret);

    if (str != NULL)
	xmlFree(str);
}

/**
 * exsltStrDecodeUriFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * reverses URI-Escaping of a string
 */
static void
exsltStrDecodeUriFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    int str_len = 0;
    xmlChar *str = NULL, *ret = NULL, *tmp;

    if ((nargs < 1) || (nargs > 2)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs >= 2) {
        /* check for UTF-8 if encoding was explicitly given;
           we don't support anything else yet */
        tmp = xmlXPathPopString(ctxt);
        if (xmlUTF8Strlen(tmp) != 5 || xmlStrcmp((const xmlChar *)"UTF-8",tmp)) {
	    xmlXPathReturnEmptyString(ctxt);
	    xmlFree(tmp);
	    return;
	}
	xmlFree(tmp);
    }

    str = xmlXPathPopString(ctxt);
    str_len = xmlUTF8Strlen(str);

    if (str_len <= 0) {
        if (str_len < 0)
            xsltGenericError(xsltGenericErrorContext,
                             "exsltStrDecodeUriFunction: invalid UTF-8\n");
	xmlXPathReturnEmptyString(ctxt);
	xmlFree(str);
	return;
    }

    ret = (xmlChar *) xmlURIUnescapeString((const char *)str,0,NULL);
    if (!xmlCheckUTF8(ret)) {
	/* FIXME: instead of throwing away the whole URI, we should
        only discard the invalid sequence(s). How to do that? */
	xmlXPathReturnEmptyString(ctxt);
	xmlFree(str);
	xmlFree(ret);
	return;
    }

    xmlXPathReturnString(ctxt, ret);

    if (str != NULL)
	xmlFree(str);
}

/**
 * exsltStrPaddingFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * Creates a padding string of a certain length.
 */
static void
exsltStrPaddingFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    int number, str_len = 0, str_size = 0;
    double floatval;
    xmlChar *str = NULL;
    xmlBufferPtr buf;

    if ((nargs < 1) || (nargs > 2)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs == 2) {
	str = xmlXPathPopString(ctxt);
	str_len = xmlUTF8Strlen(str);
	str_size = xmlStrlen(str);
    }

    floatval = xmlXPathPopNumber(ctxt);

    if (str_len <= 0) {
        if (str_len < 0) {
            xsltGenericError(xsltGenericErrorContext,
                             "exsltStrPaddingFunction: invalid UTF-8\n");
            xmlXPathReturnEmptyString(ctxt);
            xmlFree(str);
            return;
        }
	if (str != NULL) xmlFree(str);
	str = xmlStrdup((const xmlChar *) " ");
	str_len = 1;
	str_size = 1;
    }

    if (xmlXPathIsNaN(floatval) || floatval < 0.0) {
        number = 0;
    } else if (floatval >= 100000.0) {
        number = 100000;
    }
    else {
        number = (int) floatval;
    }

    if (number <= 0) {
	xmlXPathReturnEmptyString(ctxt);
	xmlFree(str);
	return;
    }

    buf = xmlBufferCreateSize(number);
    if (buf == NULL) {
        xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
	xmlFree(str);
	return;
    }
    xmlBufferSetAllocationScheme(buf, XML_BUFFER_ALLOC_DOUBLEIT);

    while (number >= str_len) {
        xmlBufferAdd(buf, str, str_size);
	number -= str_len;
    }
    if (number > 0) {
	str_size = xmlUTF8Strsize(str, number);
        xmlBufferAdd(buf, str, str_size);
    }

    xmlXPathReturnString(ctxt, xmlBufferDetach(buf));

    xmlBufferFree(buf);
    if (str != NULL)
	xmlFree(str);
}

/**
 * exsltStrAlignFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * Aligns a string within another string.
 */
static void
exsltStrAlignFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    xmlChar *str, *padding, *alignment, *ret;
    int str_l, padding_l;

    if ((nargs < 2) || (nargs > 3)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs == 3)
	alignment = xmlXPathPopString(ctxt);
    else
	alignment = NULL;

    padding = xmlXPathPopString(ctxt);
    str = xmlXPathPopString(ctxt);

    str_l = xmlUTF8Strlen (str);
    padding_l = xmlUTF8Strlen (padding);

    if (str_l < 0 || padding_l < 0) {
        xsltGenericError(xsltGenericErrorContext,
                         "exsltStrAlignFunction: invalid UTF-8\n");
        xmlXPathReturnEmptyString(ctxt);
        xmlFree(str);
        xmlFree(padding);
        xmlFree(alignment);
        return;
    }

    if (str_l == padding_l) {
	xmlXPathReturnString (ctxt, str);
	xmlFree(padding);
	xmlFree(alignment);
	return;
    }

    if (str_l > padding_l) {
	ret = xmlUTF8Strndup (str, padding_l);
    } else {
	if (xmlStrEqual(alignment, (const xmlChar *) "right")) {
	    ret = xmlUTF8Strndup (padding, padding_l - str_l);
	    ret = xmlStrcat (ret, str);
	} else if (xmlStrEqual(alignment, (const xmlChar *) "center")) {
	    int left = (padding_l - str_l) / 2;
	    int right_start;

	    ret = xmlUTF8Strndup (padding, left);
	    ret = xmlStrcat (ret, str);

	    right_start = xmlUTF8Strsize (padding, left + str_l);
	    ret = xmlStrcat (ret, padding + right_start);
	} else {
	    int str_s;

	    str_s = xmlUTF8Strsize(padding, str_l);
	    ret = xmlStrdup (str);
	    ret = xmlStrcat (ret, padding + str_s);
	}
    }

    xmlXPathReturnString (ctxt, ret);

    xmlFree(str);
    xmlFree(padding);
    xmlFree(alignment);
}

/**
 * exsltStrConcatFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * Takes a node set and returns the concatenation of the string values
 * of the nodes in that node set.  If the node set is empty, it
 * returns an empty string.
 */
static void
exsltStrConcatFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    xmlXPathObjectPtr obj;
    xmlBufferPtr buf;
    int i;

    if (nargs  != 1) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (!xmlXPathStackIsNodeSet(ctxt)) {
	xmlXPathSetTypeError(ctxt);
	return;
    }

    obj = valuePop (ctxt);

    if (xmlXPathNodeSetIsEmpty(obj->nodesetval)) {
        xmlXPathFreeObject(obj);
	xmlXPathReturnEmptyString(ctxt);
	return;
    }

    buf = xmlBufferCreate();
    if (buf == NULL) {
        xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
        xmlXPathFreeObject(obj);
	return;
    }
    xmlBufferSetAllocationScheme(buf, XML_BUFFER_ALLOC_DOUBLEIT);

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	xmlChar *tmp;
	tmp = xmlXPathCastNodeToString(obj->nodesetval->nodeTab[i]);

        xmlBufferCat(buf, tmp);

	xmlFree(tmp);
    }

    xmlXPathFreeObject (obj);

    xmlXPathReturnString(ctxt, xmlBufferDetach(buf));
    xmlBufferFree(buf);
}

/**
 * exsltStrReturnString:
 * @ctxt: an XPath parser context
 * @str: a string
 * @len: length of string
 *
 * Returns a string as a node set.
 */
static int
exsltStrReturnString(xmlXPathParserContextPtr ctxt, const xmlChar *str,
                     int len)
{
    xsltTransformContextPtr tctxt = xsltXPathGetTransformContext(ctxt);
    xmlDocPtr container;
    xmlNodePtr text_node;
    xmlXPathObjectPtr ret;

    container = xsltCreateRVT(tctxt);
    if (container == NULL) {
        xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
        return(-1);
    }
    xsltRegisterLocalRVT(tctxt, container);

    text_node = xmlNewTextLen(str, len);
    if (text_node == NULL) {
        xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
        return(-1);
    }
    xmlAddChild((xmlNodePtr) container, text_node);

    ret = xmlXPathNewNodeSet(text_node);
    if (ret == NULL) {
        xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
        return(-1);
    }

    valuePush(ctxt, ret);

    return(0);
}

/**
 * exsltStrReplaceFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * Takes a string, and two node sets and returns the string with all strings in
 * the first node set replaced by all strings in the second node set.
 */
static void
exsltStrReplaceFunction (xmlXPathParserContextPtr ctxt, int nargs) {
    int i, i_empty, n, slen0, rlen0, *slen, *rlen;
    void *mem = NULL;
    const xmlChar *src, *start;
    xmlChar *string, *search_str = NULL, *replace_str = NULL;
    xmlChar **search, **replace;
    xmlNodeSetPtr search_set = NULL, replace_set = NULL;
    xmlBufferPtr buf;

    if (nargs  != 3) {
        xmlXPathSetArityError(ctxt);
        return;
    }

    /* get replace argument */

    if (!xmlXPathStackIsNodeSet(ctxt))
        replace_str = xmlXPathPopString(ctxt);
    else
        replace_set = xmlXPathPopNodeSet(ctxt);

    if (xmlXPathCheckError(ctxt))
        goto fail_replace;

    /* get search argument */

    if (!xmlXPathStackIsNodeSet(ctxt)) {
        search_str = xmlXPathPopString(ctxt);
        n = 1;
    }
    else {
        search_set = xmlXPathPopNodeSet(ctxt);
        n = search_set != NULL ? search_set->nodeNr : 0;
    }

    if (xmlXPathCheckError(ctxt))
        goto fail_search;

    /* get string argument */

    string = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt))
        goto fail_string;

    /* check for empty search node list */

    if (n <= 0) {
        exsltStrReturnString(ctxt, string, xmlStrlen(string));
        goto done_empty_search;
    }

    /* allocate memory for string pointer and length arrays */

    if (n == 1) {
        search = &search_str;
        replace = &replace_str;
        slen = &slen0;
        rlen = &rlen0;
    }
    else {
        mem = xmlMalloc(2 * n * (sizeof(const xmlChar *) + sizeof(int)));
        if (mem == NULL) {
            xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
            goto fail_malloc;
        }
        search = (xmlChar **) mem;
        replace = search + n;
        slen = (int *) (replace + n);
        rlen = slen + n;
    }

    /* process arguments */

    i_empty = -1;

    for (i=0; i<n; ++i) {
        if (search_set != NULL) {
            search[i] = xmlXPathCastNodeToString(search_set->nodeTab[i]);
            if (search[i] == NULL) {
                n = i;
                goto fail_process_args;
            }
        }

        slen[i] = xmlStrlen(search[i]);
        if (i_empty < 0 && slen[i] == 0)
            i_empty = i;

        if (replace_set != NULL) {
            if (i < replace_set->nodeNr) {
                replace[i] = xmlXPathCastNodeToString(replace_set->nodeTab[i]);
                if (replace[i] == NULL) {
                    n = i + 1;
                    goto fail_process_args;
                }
            }
            else
                replace[i] = NULL;
        }
        else {
            if (i == 0)
                replace[i] = replace_str;
            else
                replace[i] = NULL;
        }

        if (replace[i] == NULL)
            rlen[i] = 0;
        else
            rlen[i] = xmlStrlen(replace[i]);
    }

    if (i_empty >= 0 && rlen[i_empty] == 0)
        i_empty = -1;

    /* replace operation */

    buf = xmlBufferCreate();
    if (buf == NULL) {
        xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
        goto fail_buffer;
    }
    xmlBufferSetAllocationScheme(buf, XML_BUFFER_ALLOC_DOUBLEIT);
    src = string;
    start = string;

    while (*src != 0) {
        int max_len = 0, i_match = 0;

        for (i=0; i<n; ++i) {
            if (*src == search[i][0] &&
                slen[i] > max_len &&
                xmlStrncmp(src, search[i], slen[i]) == 0)
            {
                i_match = i;
                max_len = slen[i];
            }
        }

        if (max_len == 0) {
            if (i_empty >= 0 && start < src) {
                if (xmlBufferAdd(buf, start, src - start) ||
                    xmlBufferAdd(buf, replace[i_empty], rlen[i_empty]))
                {
                    xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
                    goto fail_buffer_add;
                }
                start = src;
            }

            src += xmlUTF8Strsize(src, 1);
        }
        else {
            if ((start < src &&
                 xmlBufferAdd(buf, start, src - start)) ||
                (rlen[i_match] &&
                 xmlBufferAdd(buf, replace[i_match], rlen[i_match])))
            {
                xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
                goto fail_buffer_add;
            }

            src += slen[i_match];
            start = src;
        }
    }

    if (start < src && xmlBufferAdd(buf, start, src - start)) {
        xmlXPathSetError(ctxt, XPATH_MEMORY_ERROR);
        goto fail_buffer_add;
    }

    /* create result node set */

    exsltStrReturnString(ctxt, xmlBufferContent(buf), xmlBufferLength(buf));

    /* clean up */

fail_buffer_add:
    xmlBufferFree(buf);

fail_buffer:
fail_process_args:
    if (search_set != NULL) {
        for (i=0; i<n; ++i)
            xmlFree(search[i]);
    }
    if (replace_set != NULL) {
        for (i=0; i<n; ++i) {
            if (replace[i] != NULL)
                xmlFree(replace[i]);
        }
    }

    if (mem != NULL)
        xmlFree(mem);

fail_malloc:
done_empty_search:
    xmlFree(string);

fail_string:
    if (search_set != NULL)
        xmlXPathFreeNodeSet(search_set);
    else
        xmlFree(search_str);

fail_search:
    if (replace_set != NULL)
        xmlXPathFreeNodeSet(replace_set);
    else
        xmlFree(replace_str);

fail_replace:
    return;
}

/**
 * exsltStrRegister:
 *
 * Registers the EXSLT - Strings module
 */

void
exsltStrRegister (void) {
    xsltRegisterExtModuleFunction ((const xmlChar *) "tokenize",
				   EXSLT_STRINGS_NAMESPACE,
				   exsltStrTokenizeFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "split",
				   EXSLT_STRINGS_NAMESPACE,
				   exsltStrSplitFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "encode-uri",
				   EXSLT_STRINGS_NAMESPACE,
				   exsltStrEncodeUriFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "decode-uri",
				   EXSLT_STRINGS_NAMESPACE,
				   exsltStrDecodeUriFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "padding",
				   EXSLT_STRINGS_NAMESPACE,
				   exsltStrPaddingFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "align",
				   EXSLT_STRINGS_NAMESPACE,
				   exsltStrAlignFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "concat",
				   EXSLT_STRINGS_NAMESPACE,
				   exsltStrConcatFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "replace",
				   EXSLT_STRINGS_NAMESPACE,
				   exsltStrReplaceFunction);
}

/**
 * exsltStrXpathCtxtRegister:
 *
 * Registers the EXSLT - Strings module for use outside XSLT
 */
int
exsltStrXpathCtxtRegister (xmlXPathContextPtr ctxt, const xmlChar *prefix)
{
    if (ctxt
        && prefix
        && !xmlXPathRegisterNs(ctxt,
                               prefix,
                               (const xmlChar *) EXSLT_STRINGS_NAMESPACE)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "encode-uri",
                                   (const xmlChar *) EXSLT_STRINGS_NAMESPACE,
                                   exsltStrEncodeUriFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "decode-uri",
                                   (const xmlChar *) EXSLT_STRINGS_NAMESPACE,
                                   exsltStrDecodeUriFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "padding",
                                   (const xmlChar *) EXSLT_STRINGS_NAMESPACE,
                                   exsltStrPaddingFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "align",
                                   (const xmlChar *) EXSLT_STRINGS_NAMESPACE,
                                   exsltStrAlignFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "concat",
                                   (const xmlChar *) EXSLT_STRINGS_NAMESPACE,
                                   exsltStrConcatFunction)) {
        return 0;
    }
    return -1;
}
