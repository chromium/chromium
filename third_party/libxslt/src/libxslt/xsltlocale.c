/*
 * xsltlocale.c: locale handling
 *
 * Reference:
 * RFC 3066: Tags for the Identification of Languages
 * http://www.ietf.org/rfc/rfc3066.txt
 * ISO 639-1, ISO 3166-1
 *
 * Author: Nick Wellnhofer
 * winapi port: Roumen Petrov
 */

#define IN_LIBXSLT
#include "libxslt.h"

#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/threads.h>

#include "xsltlocale.h"
#include "xsltutils.h"

#define XSLT_LOCALE_NONE

#define TOUPPER(c) (c & ~0x20)
#define TOLOWER(c) (c | 0x20)
#define ISALPHA(c) ((unsigned)(TOUPPER(c) - 'A') < 26)

/*without terminating null character*/
#define XSLTMAX_ISO639LANGLEN		8
#define XSLTMAX_ISO3166CNTRYLEN		8
					/* <lang>-<cntry> */
#define XSLTMAX_LANGTAGLEN		(XSLTMAX_ISO639LANGLEN+1+XSLTMAX_ISO3166CNTRYLEN)

static const xmlChar* xsltDefaultRegion(const xmlChar *localeName);

#ifdef XSLT_LOCALE_WINAPI
xmlRMutexPtr xsltLocaleMutex = NULL;

struct xsltRFC1766Info_s {
      /*note typedef unsigned char xmlChar !*/
    xmlChar    tag[XSLTMAX_LANGTAGLEN+1];
    LCID       lcid;
};
typedef struct xsltRFC1766Info_s xsltRFC1766Info;

static int xsltLocaleListSize = 0;
static xsltRFC1766Info *xsltLocaleList = NULL;


static void *
xslt_locale_WINAPI(const xmlChar *languageTag) {
    int k;
    xsltRFC1766Info *p = xsltLocaleList;

    for (k=0; k<xsltLocaleListSize; k++, p++)
	if (xmlStrcmp(p->tag, languageTag) == 0)
            return(&p->lcid);
    return(NULL);
}

static void xsltEnumSupportedLocales(void);
#endif

/**
 * xsltFreeLocales:
 *
 * Cleanup function for the locale support on shutdown
 */
void
xsltFreeLocales(void) {
#ifdef XSLT_LOCALE_WINAPI
    xmlRMutexLock(xsltLocaleMutex);
    xmlFree(xsltLocaleList);
    xsltLocaleList = NULL;
    xmlRMutexUnlock(xsltLocaleMutex);
#endif
}

/**
 * xsltNewLocale:
 * @languageTag: RFC 3066 language tag
 *
 * Creates a new locale of an opaque system dependent type based on the
 * language tag.
 *
 * Returns the locale or NULL on error or if no matching locale was found
 */
void *
xsltNewLocale(const xmlChar *languageTag, int lowerFirst ATTRIBUTE_UNUSED) {
#ifdef XSLT_LOCALE_POSIX
    locale_t locale;
    char localeName[XSLTMAX_LANGTAGLEN+7]; /* 7 chars for ".UTF-8\0" */
    const xmlChar *p = languageTag;
    const char *region = NULL;
    char *q = localeName;
    int i, llen;

    /* Convert something like "pt-br" to "pt_BR.UTF-8" */

    if (languageTag == NULL)
	return(NULL);

    for (i=0; i<XSLTMAX_ISO639LANGLEN && ISALPHA(*p); ++i)
	*q++ = TOLOWER(*p++);

    if (i == 0)
	return(NULL);

    llen = i;

    if (*p) {
	if (*p++ != '-')
	    return(NULL);
        *q++ = '_';

	for (i=0; i<XSLTMAX_ISO3166CNTRYLEN && ISALPHA(*p); ++i)
	    *q++ = TOUPPER(*p++);

	if (i == 0 || *p)
	    return(NULL);

        memcpy(q, ".UTF-8", 7);
        locale = newlocale(LC_ALL_MASK, localeName, NULL);
        if (locale != NULL)
            return(locale);

        /* Continue without using country code */

        q = localeName + llen;
    }

    /* Try locale without territory, e.g. for Esperanto (eo) */

    memcpy(q, ".UTF-8", 7);
    locale = newlocale(LC_ALL_MASK, localeName, NULL);
    if (locale != NULL)
        return(locale);

    /* Try to find most common country for language */

    if (llen != 2)
        return(NULL);

    region = (char *)xsltDefaultRegion((xmlChar *)localeName);
    if (region == NULL)
        return(NULL);

    q = localeName + llen;
    *q++ = '_';
    *q++ = region[0];
    *q++ = region[1];
    memcpy(q, ".UTF-8", 7);
    locale = newlocale(LC_ALL_MASK, localeName, NULL);

    return(locale);
#endif

#ifdef XSLT_LOCALE_WINAPI
{
    void          *locale = NULL;
    xmlChar       localeName[XSLTMAX_LANGTAGLEN+1];
    xmlChar       *q = localeName;
    const xmlChar *p = languageTag;
    int           i, llen;
    const xmlChar *region = NULL;

    if (languageTag == NULL) goto end;

    xsltEnumSupportedLocales();

    for (i=0; i<XSLTMAX_ISO639LANGLEN && ISALPHA(*p); ++i)
	*q++ = TOLOWER(*p++);
    if (i == 0) goto end;

    llen = i;
    *q++ = '-';
    if (*p) { /*if country tag is given*/
	if (*p++ != '-') goto end;

	for (i=0; i<XSLTMAX_ISO3166CNTRYLEN && ISALPHA(*p); ++i)
	    *q++ = TOUPPER(*p++);
	if (i == 0 || *p) goto end;

	*q = '\0';
	locale = xslt_locale_WINAPI(localeName);
	if (locale != (xsltLocale)0) goto end;
    }
    /* Try to find most common country for language */
    region = xsltDefaultRegion(localeName);
    if (region == NULL) goto end;

    strcpy((char *) localeName + llen + 1, (char *) region);
    locale = xslt_locale_WINAPI(localeName);
end:
    return(locale);
}
#endif

#ifdef XSLT_LOCALE_NONE
    return(NULL);
#endif
}

static const xmlChar*
xsltDefaultRegion(const xmlChar *localeName) {
    xmlChar c;
    /* region should be xmlChar, but gcc warns on all string assignments */
    const char *region = NULL;

    c = localeName[1];
    /* This is based on the locales from glibc 2.3.3 */

    switch (localeName[0]) {
        case 'a':
            if (c == 'a' || c == 'm') region = "ET";
            else if (c == 'f') region = "ZA";
            else if (c == 'n') region = "ES";
            else if (c == 'r') region = "AE";
            else if (c == 'z') region = "AZ";
            break;
        case 'b':
            if (c == 'e') region = "BY";
            else if (c == 'g') region = "BG";
            else if (c == 'n') region = "BD";
            else if (c == 'r') region = "FR";
            else if (c == 's') region = "BA";
            break;
        case 'c':
            if (c == 'a') region = "ES";
            else if (c == 's') region = "CZ";
            else if (c == 'y') region = "GB";
            break;
        case 'd':
            if (c == 'a') region = "DK";
            else if (c == 'e') region = "DE";
            break;
        case 'e':
            if (c == 'l') region = "GR";
            else if (c == 'n' || c == 'o') region = "US";
            else if (c == 's' || c == 'u') region = "ES";
            else if (c == 't') region = "EE";
            break;
        case 'f':
            if (c == 'a') region = "IR";
            else if (c == 'i') region = "FI";
            else if (c == 'o') region = "FO";
            else if (c == 'r') region = "FR";
            break;
        case 'g':
            if (c == 'a') region = "IE";
            else if (c == 'l') region = "ES";
            else if (c == 'v') region = "GB";
            break;
        case 'h':
            if (c == 'e') region = "IL";
            else if (c == 'i') region = "IN";
            else if (c == 'r') region = "HT";
            else if (c == 'u') region = "HU";
            break;
        case 'i':
            if (c == 'd') region = "ID";
            else if (c == 's') region = "IS";
            else if (c == 't') region = "IT";
            else if (c == 'w') region = "IL";
            break;
        case 'j':
            if (c == 'a') region = "JP";
            break;
        case 'k':
            if (c == 'l') region = "GL";
            else if (c == 'o') region = "KR";
            else if (c == 'w') region = "GB";
            break;
        case 'l':
            if (c == 't') region = "LT";
            else if (c == 'v') region = "LV";
            break;
        case 'm':
            if (c == 'k') region = "MK";
            else if (c == 'l' || c == 'r') region = "IN";
            else if (c == 'n') region = "MN";
            else if (c == 's') region = "MY";
            else if (c == 't') region = "MT";
            break;
        case 'n':
            if (c == 'b' || c == 'n' || c == 'o') region = "NO";
            else if (c == 'e') region = "NP";
            else if (c == 'l') region = "NL";
            break;
        case 'o':
            if (c == 'm') region = "ET";
            break;
        case 'p':
            if (c == 'a') region = "IN";
            else if (c == 'l') region = "PL";
            else if (c == 't') region = "PT";
            break;
        case 'r':
            if (c == 'o') region = "RO";
            else if (c == 'u') region = "RU";
            break;
        case 's':
            switch (c) {
                case 'e': region = "NO"; break;
                case 'h': region = "YU"; break;
                case 'k': region = "SK"; break;
                case 'l': region = "SI"; break;
                case 'o': region = "ET"; break;
                case 'q': region = "AL"; break;
                case 't': region = "ZA"; break;
                case 'v': region = "SE"; break;
            }
            break;
        case 't':
            if (c == 'a' || c == 'e') region = "IN";
            else if (c == 'h') region = "TH";
            else if (c == 'i') region = "ER";
            else if (c == 'r') region = "TR";
            else if (c == 't') region = "RU";
            break;
        case 'u':
            if (c == 'k') region = "UA";
            else if (c == 'r') region = "PK";
            break;
        case 'v':
            if (c == 'i') region = "VN";
            break;
        case 'w':
            if (c == 'a') region = "BE";
            break;
        case 'x':
            if (c == 'h') region = "ZA";
            break;
        case 'z':
            if (c == 'h') region = "CN";
            else if (c == 'u') region = "ZA";
            break;
    }
    return((xmlChar *)region);
}

/**
 * xsltFreeLocale:
 * @locale: the locale to free
 *
 * Frees a locale created with xsltNewLocale
 */
void
xsltFreeLocale(void *locale) {
#ifdef XSLT_LOCALE_POSIX
    if (locale != NULL)
        freelocale(locale);
#else
    (void) locale;
#endif
}

/**
 * xsltStrxfrm:
 * @locale: locale created with xsltNewLocale
 * @string: UTF-8 string to transform
 *
 * Transforms a string according to locale. The transformed string must be
 * freed with xmlFree.
 *
 * Returns the transformed string or NULL on error
 */
xmlChar *
xsltStrxfrm(void *vlocale, const xmlChar *string)
{
#ifdef XSLT_LOCALE_NONE
    return(NULL);
#else
    xmlChar *xstr;

#ifdef XSLT_LOCALE_POSIX
    size_t xstrlen, r;

    xstrlen = strxfrm_l(NULL, (const char *)string, 0, vlocale) + 1;
    xstr = (xmlChar *) xmlMalloc(xstrlen);
    if (xstr == NULL) {
	xsltTransformError(NULL, NULL, NULL,
	    "xsltStrxfrm : out of memory error\n");
	return(NULL);
    }

    r = strxfrm_l((char *)xstr, (const char *)string, xstrlen, vlocale);

    if (r >= xstrlen) {
	xsltTransformError(NULL, NULL, NULL, "xsltStrxfrm : strxfrm failed\n");
        xmlFree(xstr);
        return(NULL);
    }
#endif

#ifdef XSLT_LOCALE_WINAPI
    int wstrlen, xstrlen, r;
    wchar_t *wstr;
    LCID *lcid = vlocale;

    wstrlen = MultiByteToWideChar(CP_UTF8, 0, (char *) string, -1, NULL, 0);
    if (wstrlen == 0) {
        xsltTransformError(NULL, NULL, NULL, "xsltStrxfrm : MultiByteToWideChar check failed\n");
        return(NULL);
    }
    wstr = (wchar_t *) xmlMalloc(wstrlen * sizeof(wchar_t));
    if (wstr == NULL) {
        xsltTransformError(NULL, NULL, NULL, "xsltStrxfrm : out of memory\n");
        return(NULL);
    }
    r = MultiByteToWideChar(CP_UTF8, 0, (char *) string, -1, wstr, wstrlen);
    if (r == 0) {
        xsltTransformError(NULL, NULL, NULL, "xsltStrxfrm : MultiByteToWideChar failed\n");
        xmlFree(wstr);
        return(NULL);
    }
    /* This returns the size in bytes. */
    xstrlen = LCMapStringW(*lcid, LCMAP_SORTKEY, wstr, wstrlen, NULL, 0);
    if (xstrlen == 0) {
        xsltTransformError(NULL, NULL, NULL, "xsltStrxfrm : LCMapStringW failed\n");
        xmlFree(wstr);
        return(NULL);
    }
    xstr = (xmlChar*) xmlMalloc(xstrlen);
    if (xstr == NULL) {
        xsltTransformError(NULL, NULL, NULL, "xsltStrxfrm : out of memory\n");
        xmlFree(wstr);
        return(NULL);
    }
    r = LCMapStringW(*lcid, LCMAP_SORTKEY, wstr, wstrlen, (wchar_t *) xstr,
                     xstrlen);
    xmlFree(wstr);
    if (r == 0) {
        xsltTransformError(NULL, NULL, NULL, "xsltStrxfrm : LCMapStringW failed\n");
        xmlFree(xstr);
        return(NULL);
    }
#endif /* XSLT_LOCALE_WINAPI */

    return(xstr);
#endif /* XSLT_LOCALE_NONE */
}

/**
 * xsltLocaleStrcmp:
 * @locale: unused
 * @str1: a string transformed with xsltStrxfrm
 * @str2: a string transformed with xsltStrxfrm
 *
 * DEPRECATED: Same as xmlStrcmp.
 *
 * Compares two strings transformed with xsltStrxfrm.
 *
 * Returns a value < 0 if str1 sorts before str2,
 *         a value > 0 if str1 sorts after str2,
 *         0 if str1 and str2 are equal wrt sorting
 */
int
xsltLocaleStrcmp(void *locale, const xmlChar *str1, const xmlChar *str2) {
    (void)locale;
    return(xmlStrcmp(str1, str2));
}

#ifdef XSLT_LOCALE_WINAPI
/**
 * xsltCountSupportedLocales:
 * @lcid: not used
 *
 * callback used to count locales
 *
 * Returns TRUE
 */
static BOOL CALLBACK
xsltCountSupportedLocales(LPSTR lcid) {
    (void) lcid;
    ++xsltLocaleListSize;
    return(TRUE);
}

/**
 * xsltIterateSupportedLocales:
 * @lcid: not used
 *
 * callback used to track locales
 *
 * Returns TRUE if not at the end of the array
 */
static BOOL CALLBACK
xsltIterateSupportedLocales(LPSTR lcid) {
    static int count = 0;
    xmlChar    iso639lang [XSLTMAX_ISO639LANGLEN  +1];
    xmlChar    iso3136ctry[XSLTMAX_ISO3166CNTRYLEN+1];
    int        k, l;
    xsltRFC1766Info *p = xsltLocaleList + count;

    k = sscanf(lcid, "%lx", (unsigned long*)&p->lcid);
    if (k < 1) goto end;
    /*don't count terminating null character*/
    k = GetLocaleInfoA(p->lcid, LOCALE_SISO639LANGNAME,
                       (char *) iso639lang, sizeof(iso639lang));
    if (--k < 1) goto end;
    l = GetLocaleInfoA(p->lcid, LOCALE_SISO3166CTRYNAME,
                       (char *) iso3136ctry, sizeof(iso3136ctry));
    if (--l < 1) goto end;

    {  /*fill results*/
	xmlChar    *q = p->tag;
	memcpy(q, iso639lang, k);
	q += k;
	*q++ = '-';
	memcpy(q, iso3136ctry, l);
	q += l;
	*q = '\0';
    }
    ++count;
end:
    return((count < xsltLocaleListSize) ? TRUE : FALSE);
}


static void
xsltEnumSupportedLocales(void) {
    xmlRMutexLock(xsltLocaleMutex);
    if (xsltLocaleListSize <= 0) {
	size_t len;

	EnumSystemLocalesA(xsltCountSupportedLocales, LCID_SUPPORTED);

	len = xsltLocaleListSize * sizeof(xsltRFC1766Info);
	xsltLocaleList = xmlMalloc(len);
	memset(xsltLocaleList, 0, len);
	EnumSystemLocalesA(xsltIterateSupportedLocales, LCID_SUPPORTED);
    }
    xmlRMutexUnlock(xsltLocaleMutex);
}

#endif /*def XSLT_LOCALE_WINAPI*/
