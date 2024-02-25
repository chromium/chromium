/*
 * date.c: Implementation of the EXSLT -- Dates and Times module
 *
 * References:
 *   http://www.exslt.org/date/date.html
 *
 * See Copyright for the status of this software.
 *
 * Authors:
 *   Charlie Bozeman <cbozeman@HiWAAY.net>
 *   Thomas Broyer <tbroyer@ltgt.net>
 *
 * TODO:
 * elements:
 *   date-format
 * functions:
 *   format-date
 *   parse-date
 *   sum
 */

#define IN_LIBEXSLT
#include "libexslt/libexslt.h"

#if defined(HAVE_LOCALTIME_R) && defined(__GLIBC__)	/* _POSIX_SOURCE required by gnu libc */
#ifndef _AIX51		/* but on AIX we're not using gnu libc */
#define _POSIX_SOURCE
#endif
#endif

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <libxslt/xsltutils.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/extensions.h>

#include "exslt.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <math.h>

/* needed to get localtime_r on Solaris */
#ifdef __sun
#ifndef __EXTENSIONS__
#define __EXTENSIONS__
#endif
#endif

#include <time.h>

#if defined(_MSC_VER) && _MSC_VER >= 1400 || \
    defined(_WIN32) && \
    defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR >= 4
  #define HAVE_MSVCRT
#endif

/*
 * types of date and/or time (from schema datatypes)
 *   somewhat ordered from least specific to most specific (i.e.
 *   most truncated to least truncated).
 */
typedef enum {
    EXSLT_UNKNOWN  =    0,
    XS_TIME        =    1,       /* time is left-truncated */
    XS_GDAY        = (XS_TIME   << 1),
    XS_GMONTH      = (XS_GDAY   << 1),
    XS_GMONTHDAY   = (XS_GMONTH | XS_GDAY),
    XS_GYEAR       = (XS_GMONTH << 1),
    XS_GYEARMONTH  = (XS_GYEAR  | XS_GMONTH),
    XS_DATE        = (XS_GYEAR  | XS_GMONTH | XS_GDAY),
    XS_DATETIME    = (XS_DATE   | XS_TIME)
} exsltDateType;

/* Date value */
typedef struct _exsltDateVal exsltDateVal;
typedef exsltDateVal *exsltDateValPtr;
struct _exsltDateVal {
    exsltDateType	type;
    long		year;
    unsigned int	mon	:4;	/* 1 <=  mon    <= 12   */
    unsigned int	day	:5;	/* 1 <=  day    <= 31   */
    unsigned int	hour	:5;	/* 0 <=  hour   <= 23   */
    unsigned int	min	:6;	/* 0 <=  min    <= 59	*/
    double		sec;
    unsigned int	tz_flag	:1;	/* is tzo explicitely set? */
    signed int		tzo	:12;	/* -1440 <= tzo <= 1440 currently only -840 to +840 are needed */
};

/* Duration value */
typedef struct _exsltDateDurVal exsltDateDurVal;
typedef exsltDateDurVal *exsltDateDurValPtr;
struct _exsltDateDurVal {
    long	mon;	/* mon stores years also */
    long	day;
    double	sec;	/* sec stores min and hour also
			   0 <= sec < SECS_PER_DAY */
};

/****************************************************************
 *								*
 *		Convenience macros and functions		*
 *								*
 ****************************************************************/

#define IS_TZO_CHAR(c)						\
	((c == 0) || (c == 'Z') || (c == '+') || (c == '-'))

#define VALID_ALWAYS(num)	(num >= 0)
#define VALID_MONTH(mon)        ((mon >= 1) && (mon <= 12))
/* VALID_DAY should only be used when month is unknown */
#define VALID_DAY(day)          ((day >= 1) && (day <= 31))
#define VALID_HOUR(hr)          ((hr >= 0) && (hr <= 23))
#define VALID_MIN(min)          ((min >= 0) && (min <= 59))
#define VALID_SEC(sec)          ((sec >= 0) && (sec < 60))
#define VALID_TZO(tzo)          ((tzo > -1440) && (tzo < 1440))
#define IS_LEAP(y)						\
	(((y & 3) == 0) && ((y % 25 != 0) || ((y & 15) == 0)))

static const long daysInMonth[12] =
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const long daysInMonthLeap[12] =
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

#define MAX_DAYINMONTH(yr,mon)                                  \
        (IS_LEAP(yr) ? daysInMonthLeap[mon - 1] : daysInMonth[mon - 1])

#define VALID_MDAY(dt)						\
	(IS_LEAP(dt->year) ?				        \
	    (dt->day <= daysInMonthLeap[dt->mon - 1]) :	        \
	    (dt->day <= daysInMonth[dt->mon - 1]))

#define VALID_DATE(dt)						\
	(VALID_MONTH(dt->mon) && VALID_MDAY(dt))

/*
    hour and min structure vals are unsigned, so normal macros give
    warnings on some compilers.
*/
#define VALID_TIME(dt)						\
	((dt->hour <=23 ) && (dt->min <= 59) &&			\
	 VALID_SEC(dt->sec) && VALID_TZO(dt->tzo))

#define VALID_DATETIME(dt)					\
	(VALID_DATE(dt) && VALID_TIME(dt))

#define SECS_PER_MIN            60
#define MINS_PER_HOUR           60
#define HOURS_PER_DAY           24
#define SECS_PER_HOUR           (MINS_PER_HOUR * SECS_PER_MIN)
#define SECS_PER_DAY            (HOURS_PER_DAY * SECS_PER_HOUR)
#define MINS_PER_DAY            (HOURS_PER_DAY * MINS_PER_HOUR)
#define DAYS_PER_EPOCH          (400 * 365 + 100 - 4 + 1)
#define YEARS_PER_EPOCH         400

static const long dayInYearByMonth[12] =
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
static const long dayInLeapYearByMonth[12] =
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };

#define DAY_IN_YEAR(day, month, year)				\
        ((IS_LEAP(year) ?					\
                dayInLeapYearByMonth[month - 1] :		\
                dayInYearByMonth[month - 1]) + day)

#define YEAR_MAX LONG_MAX
#define YEAR_MIN (-LONG_MAX + 1)

/**
 * _exsltDateParseGYear:
 * @dt:  pointer to a date structure
 * @str: pointer to the string to analyze
 *
 * Parses a xs:gYear without time zone and fills in the appropriate
 * field of the @dt structure. @str is updated to point just after the
 * xs:gYear. It is supposed that @dt->year is big enough to contain
 * the year.
 *
 * According to XML Schema Part 2, the year "0000" is an illegal year value
 * which probably means that the year preceding AD 1 is BC 1. Internally,
 * we allow a year 0 and adjust the value when parsing and formatting.
 *
 * Returns 0 or the error code
 */
static int
_exsltDateParseGYear (exsltDateValPtr dt, const xmlChar **str)
{
    const xmlChar *cur = *str, *firstChar;
    int isneg = 0, digcnt = 0;

    if (((*cur < '0') || (*cur > '9')) &&
	(*cur != '-') && (*cur != '+'))
	return -1;

    if (*cur == '-') {
	isneg = 1;
	cur++;
    }

    firstChar = cur;

    while ((*cur >= '0') && (*cur <= '9')) {
        if (dt->year >= YEAR_MAX / 10) /* Not really exact */
            return -1;
	dt->year = dt->year * 10 + (*cur - '0');
	cur++;
	digcnt++;
    }

    /* year must be at least 4 digits (CCYY); over 4
     * digits cannot have a leading zero. */
    if ((digcnt < 4) || ((digcnt > 4) && (*firstChar == '0')))
	return 1;

    if (dt->year == 0)
	return 2;

    /* The internal representation of negative years is continuous. */
    if (isneg)
	dt->year = -dt->year + 1;

    *str = cur;

#ifdef DEBUG_EXSLT_DATE
    xsltGenericDebug(xsltGenericDebugContext,
		     "Parsed year %04ld\n", dt->year);
#endif

    return 0;
}

/**
 * exsltFormatGYear:
 * @cur: a pointer to a pointer to an allocated buffer
 * @end: a pointer to the end of @cur buffer
 * @yr:  the year to format
 *
 * Formats @yr in xsl:gYear format. Result is appended to @cur and
 * @cur is updated to point after the xsl:gYear.
 */
static void
exsltFormatGYear(xmlChar **cur, xmlChar *end, long yr)
{
    long year;
    xmlChar tmp_buf[100], *tmp = tmp_buf, *tmp_end = tmp_buf + 99;

    if (yr <= 0 && *cur < end) {
        *(*cur)++ = '-';
    }

    year = (yr <= 0) ? -yr + 1 : yr;
    /* result is in reverse-order */
    while (year > 0 && tmp < tmp_end) {
        *tmp++ = '0' + (xmlChar)(year % 10);
        year /= 10;
    }

    /* virtually adds leading zeros */
    while ((tmp - tmp_buf) < 4)
        *tmp++ = '0';

    /* restore the correct order */
    while (tmp > tmp_buf && *cur < end) {
        tmp--;
        *(*cur)++ = *tmp;
    }
}

/**
 * PARSE_2_DIGITS:
 * @num:  the integer to fill in
 * @cur:  an #xmlChar *
 * @func: validation function for the number
 * @invalid: an integer
 *
 * Parses a 2-digits integer and updates @num with the value. @cur is
 * updated to point just after the integer.
 * In case of error, @invalid is set to %TRUE, values of @num and
 * @cur are undefined.
 */
#define PARSE_2_DIGITS(num, cur, func, invalid)			\
	if ((cur[0] < '0') || (cur[0] > '9') ||			\
	    (cur[1] < '0') || (cur[1] > '9'))			\
	    invalid = 1;					\
	else {							\
	    int val;						\
	    val = (cur[0] - '0') * 10 + (cur[1] - '0');		\
	    if (!func(val))					\
	        invalid = 2;					\
	    else						\
	        num = val;					\
	}							\
	cur += 2;

/**
 * exsltFormat2Digits:
 * @cur: a pointer to a pointer to an allocated buffer
 * @end: a pointer to the end of @cur buffer
 * @num: the integer to format
 *
 * Formats a 2-digits integer. Result is appended to @cur and
 * @cur is updated to point after the integer.
 */
static void
exsltFormat2Digits(xmlChar **cur, xmlChar *end, unsigned int num)
{
    if (*cur < end)
        *(*cur)++ = '0' + ((num / 10) % 10);
    if (*cur < end)
        *(*cur)++ = '0' + (num % 10);
}

/**
 * PARSE_FLOAT:
 * @num:  the double to fill in
 * @cur:  an #xmlChar *
 * @invalid: an integer
 *
 * Parses a float and updates @num with the value. @cur is
 * updated to point just after the float. The float must have a
 * 2-digits integer part and may or may not have a decimal part.
 * In case of error, @invalid is set to %TRUE, values of @num and
 * @cur are undefined.
 */
#define PARSE_FLOAT(num, cur, invalid)				\
	PARSE_2_DIGITS(num, cur, VALID_ALWAYS, invalid);	\
	if (!invalid && (*cur == '.')) {			\
	    double mult = 1;				        \
	    cur++;						\
	    if ((*cur < '0') || (*cur > '9'))			\
		invalid = 1;					\
	    while ((*cur >= '0') && (*cur <= '9')) {		\
		mult /= 10;					\
		num += (*cur - '0') * mult;			\
		cur++;						\
	    }							\
	}

/**
 * _exsltDateParseGMonth:
 * @dt:  pointer to a date structure
 * @str: pointer to the string to analyze
 *
 * Parses a xs:gMonth without time zone and fills in the appropriate
 * field of the @dt structure. @str is updated to point just after the
 * xs:gMonth.
 *
 * Returns 0 or the error code
 */
static int
_exsltDateParseGMonth (exsltDateValPtr dt, const xmlChar **str)
{
    const xmlChar *cur = *str;
    int ret = 0;

    PARSE_2_DIGITS(dt->mon, cur, VALID_MONTH, ret);
    if (ret != 0)
	return ret;

    *str = cur;

#ifdef DEBUG_EXSLT_DATE
    xsltGenericDebug(xsltGenericDebugContext,
		     "Parsed month %02i\n", dt->mon);
#endif

    return 0;
}

/**
 * _exsltDateParseGDay:
 * @dt:  pointer to a date structure
 * @str: pointer to the string to analyze
 *
 * Parses a xs:gDay without time zone and fills in the appropriate
 * field of the @dt structure. @str is updated to point just after the
 * xs:gDay.
 *
 * Returns 0 or the error code
 */
static int
_exsltDateParseGDay (exsltDateValPtr dt, const xmlChar **str)
{
    const xmlChar *cur = *str;
    int ret = 0;

    PARSE_2_DIGITS(dt->day, cur, VALID_DAY, ret);
    if (ret != 0)
	return ret;

    *str = cur;

#ifdef DEBUG_EXSLT_DATE
    xsltGenericDebug(xsltGenericDebugContext,
		     "Parsed day %02i\n", dt->day);
#endif

    return 0;
}

/**
 * exsltFormatYearMonthDay:
 * @cur: a pointer to a pointer to an allocated buffer
 * @end: a pointer to the end of @cur buffer
 * @dt:  the #exsltDateVal to format
 *
 * Formats @dt in xsl:date format. Result is appended to @cur and
 * @cur is updated to point after the xsl:date.
 */
static void
exsltFormatYearMonthDay(xmlChar **cur, xmlChar *end, const exsltDateValPtr dt)
{
    exsltFormatGYear(cur, end, dt->year);
    if (*cur < end)
        *(*cur)++ = '-';
    exsltFormat2Digits(cur, end, dt->mon);
    if (*cur < end)
        *(*cur)++ = '-';
    exsltFormat2Digits(cur, end, dt->day);
}

/**
 * _exsltDateParseTime:
 * @dt:  pointer to a date structure
 * @str: pointer to the string to analyze
 *
 * Parses a xs:time without time zone and fills in the appropriate
 * fields of the @dt structure. @str is updated to point just after the
 * xs:time.
 * In case of error, values of @dt fields are undefined.
 *
 * Returns 0 or the error code
 */
static int
_exsltDateParseTime (exsltDateValPtr dt, const xmlChar **str)
{
    const xmlChar *cur = *str;
    unsigned int hour = 0; /* use temp var in case str is not xs:time */
    int ret = 0;

    PARSE_2_DIGITS(hour, cur, VALID_HOUR, ret);
    if (ret != 0)
	return ret;

    if (*cur != ':')
	return 1;
    cur++;

    /* the ':' insures this string is xs:time */
    dt->hour = hour;

    PARSE_2_DIGITS(dt->min, cur, VALID_MIN, ret);
    if (ret != 0)
	return ret;

    if (*cur != ':')
	return 1;
    cur++;

    PARSE_FLOAT(dt->sec, cur, ret);
    if (ret != 0)
	return ret;

    if (!VALID_TIME(dt))
	return 2;

    *str = cur;

#ifdef DEBUG_EXSLT_DATE
    xsltGenericDebug(xsltGenericDebugContext,
		     "Parsed time %02i:%02i:%02.f\n",
		     dt->hour, dt->min, dt->sec);
#endif

    return 0;
}

/**
 * _exsltDateParseTimeZone:
 * @dt:  pointer to a date structure
 * @str: pointer to the string to analyze
 *
 * Parses a time zone without time zone and fills in the appropriate
 * field of the @dt structure. @str is updated to point just after the
 * time zone.
 *
 * Returns 0 or the error code
 */
static int
_exsltDateParseTimeZone (exsltDateValPtr dt, const xmlChar **str)
{
    const xmlChar *cur;
    int ret = 0;

    if (str == NULL)
	return -1;
    cur = *str;
    switch (*cur) {
    case 0:
	dt->tz_flag = 0;
	dt->tzo = 0;
	break;

    case 'Z':
	dt->tz_flag = 1;
	dt->tzo = 0;
	cur++;
	break;

    case '+':
    case '-': {
	int isneg = 0, tmp = 0;
	isneg = (*cur == '-');

	cur++;

	PARSE_2_DIGITS(tmp, cur, VALID_HOUR, ret);
	if (ret != 0)
	    return ret;

	if (*cur != ':')
	    return 1;
	cur++;

	dt->tzo = tmp * 60;

	PARSE_2_DIGITS(tmp, cur, VALID_MIN, ret);
	if (ret != 0)
	    return ret;

	dt->tzo += tmp;
	if (isneg)
	    dt->tzo = - dt->tzo;

	if (!VALID_TZO(dt->tzo))
	    return 2;

	break;
      }
    default:
	return 1;
    }

    *str = cur;

#ifdef DEBUG_EXSLT_DATE
    xsltGenericDebug(xsltGenericDebugContext,
		     "Parsed time zone offset (%s) %i\n",
		     dt->tz_flag ? "explicit" : "implicit", dt->tzo);
#endif

    return 0;
}

/**
 * exsltFormatTimeZone:
 * @cur: a pointer to a pointer to an allocated buffer
 * @end: a pointer to the end of @cur buffer
 * @tzo: the timezone offset to format
 *
 * Formats @tzo timezone. Result is appended to @cur and
 * @cur is updated to point after the timezone.
 */
static void
exsltFormatTimeZone(xmlChar **cur, xmlChar *end, int tzo)
{
    if (tzo == 0) {
        if (*cur < end)
            *(*cur)++ = 'Z';
    } else {
        unsigned int aTzo = (tzo < 0) ? -tzo : tzo;
        unsigned int tzHh = aTzo / 60, tzMm = aTzo % 60;
        if (*cur < end)
            *(*cur)++ = (tzo < 0) ? '-' : '+';
        exsltFormat2Digits(cur, end, tzHh);
        if (*cur < end)
            *(*cur)++ = ':';
        exsltFormat2Digits(cur, end, tzMm);
    }
}

/****************************************************************
 *								*
 *	XML Schema Dates/Times Datatypes Handling		*
 *								*
 ****************************************************************/

/**
 * exsltDateCreateDate:
 * @type:       type to create
 *
 * Creates a new #exsltDateVal, uninitialized.
 *
 * Returns the #exsltDateValPtr
 */
static exsltDateValPtr
exsltDateCreateDate (exsltDateType type)
{
    exsltDateValPtr ret;

    ret = (exsltDateValPtr) xmlMalloc(sizeof(exsltDateVal));
    if (ret == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltDateCreateDate: out of memory\n");
	return (NULL);
    }
    memset (ret, 0, sizeof(exsltDateVal));

    ret->mon = 1;
    ret->day = 1;

    if (type != EXSLT_UNKNOWN)
        ret->type = type;

    return ret;
}

/**
 * exsltDateFreeDate:
 * @date: an #exsltDateValPtr
 *
 * Frees up the @date
 */
static void
exsltDateFreeDate (exsltDateValPtr date) {
    if (date == NULL)
	return;

    xmlFree(date);
}

/**
 * exsltDateCreateDuration:
 *
 * Creates a new #exsltDateDurVal, uninitialized.
 *
 * Returns the #exsltDateDurValPtr
 */
static exsltDateDurValPtr
exsltDateCreateDuration (void)
{
    exsltDateDurValPtr ret;

    ret = (exsltDateDurValPtr) xmlMalloc(sizeof(exsltDateDurVal));
    if (ret == NULL) {
	xsltGenericError(xsltGenericErrorContext,
			 "exsltDateCreateDuration: out of memory\n");
	return (NULL);
    }
    memset (ret, 0, sizeof(exsltDateDurVal));

    return ret;
}

/**
 * exsltDateFreeDuration:
 * @date: an #exsltDateDurValPtr
 *
 * Frees up the @duration
 */
static void
exsltDateFreeDuration (exsltDateDurValPtr duration) {
    if (duration == NULL)
	return;

    xmlFree(duration);
}

/**
 * exsltDateCurrent:
 *
 * Returns the current date and time.
 */
static exsltDateValPtr
exsltDateCurrent (void)
{
    struct tm localTm, gmTm;
#if !defined(HAVE_GMTIME_R) && !defined(HAVE_MSVCRT)
    struct tm *tb = NULL;
#endif
    time_t secs;
    int local_s, gm_s;
    exsltDateValPtr ret;
    char *source_date_epoch;
    int override = 0;

    ret = exsltDateCreateDate(XS_DATETIME);
    if (ret == NULL)
        return NULL;

    /*
     * Allow the date and time to be set externally by an exported
     * environment variable to enable reproducible builds.
     */
    source_date_epoch = getenv("SOURCE_DATE_EPOCH");
    if (source_date_epoch) {
        errno = 0;
	secs = (time_t) strtol (source_date_epoch, NULL, 10);
	if (errno == 0) {
#ifdef HAVE_MSVCRT
	    struct tm *gm = gmtime_s(&localTm, &secs) ? NULL : &localTm;
	    if (gm != NULL)
	        override = 1;
#elif HAVE_GMTIME_R
	    if (gmtime_r(&secs, &localTm) != NULL)
	        override = 1;
#else
	    tb = gmtime(&secs);
	    if (tb != NULL) {
	        localTm = *tb;
		override = 1;
	    }
#endif
        }
    }

    if (override == 0) {
    /* get current time */
	secs    = time(NULL);

#ifdef HAVE_MSVCRT
	localtime_s(&localTm, &secs);
#elif HAVE_LOCALTIME_R
	localtime_r(&secs, &localTm);
#else
	localTm = *localtime(&secs);
#endif
    }

    /* get real year, not years since 1900 */
    ret->year = localTm.tm_year + 1900;

    ret->mon  = localTm.tm_mon + 1;
    ret->day  = localTm.tm_mday;
    ret->hour = localTm.tm_hour;
    ret->min  = localTm.tm_min;

    /* floating point seconds */
    ret->sec  = (double) localTm.tm_sec;

    /* determine the time zone offset from local to gm time */
#ifdef HAVE_MSVCRT
    gmtime_s(&gmTm, &secs);
#elif HAVE_GMTIME_R
    gmtime_r(&secs, &gmTm);
#else
    tb = gmtime(&secs);
    if (tb == NULL)
        return NULL;
    gmTm = *tb;
#endif
    ret->tz_flag = 0;
#if 0
    ret->tzo = (((ret->day * 1440) +
                 (ret->hour * 60) +
                  ret->min) -
                ((gmTm.tm_mday * 1440) + (gmTm.tm_hour * 60) +
                  gmTm.tm_min));
#endif
    local_s = localTm.tm_hour * SECS_PER_HOUR +
        localTm.tm_min * SECS_PER_MIN +
        localTm.tm_sec;

    gm_s = gmTm.tm_hour * SECS_PER_HOUR +
        gmTm.tm_min * SECS_PER_MIN +
        gmTm.tm_sec;

    if (localTm.tm_year < gmTm.tm_year) {
	ret->tzo = -((SECS_PER_DAY - local_s) + gm_s)/60;
    } else if (localTm.tm_year > gmTm.tm_year) {
	ret->tzo = ((SECS_PER_DAY - gm_s) + local_s)/60;
    } else if (localTm.tm_mon < gmTm.tm_mon) {
	ret->tzo = -((SECS_PER_DAY - local_s) + gm_s)/60;
    } else if (localTm.tm_mon > gmTm.tm_mon) {
	ret->tzo = ((SECS_PER_DAY - gm_s) + local_s)/60;
    } else if (localTm.tm_mday < gmTm.tm_mday) {
	ret->tzo = -((SECS_PER_DAY - local_s) + gm_s)/60;
    } else if (localTm.tm_mday > gmTm.tm_mday) {
	ret->tzo = ((SECS_PER_DAY - gm_s) + local_s)/60;
    } else  {
	ret->tzo = (local_s - gm_s)/60;
    }

    return ret;
}

/**
 * exsltDateParse:
 * @dateTime:  string to analyze
 *
 * Parses a date/time string
 *
 * Returns a newly built #exsltDateValPtr of NULL in case of error
 */
static exsltDateValPtr
exsltDateParse (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    int ret;
    const xmlChar *cur = dateTime;

#define RETURN_TYPE_IF_VALID(t)					\
    if (IS_TZO_CHAR(*cur)) {					\
	ret = _exsltDateParseTimeZone(dt, &cur);		\
	if (ret == 0) {						\
	    if (*cur != 0)					\
		goto error;					\
	    dt->type = t;					\
	    return dt;						\
	}							\
    }

    if (dateTime == NULL)
	return NULL;

    if ((*cur != '-') && (*cur < '0') && (*cur > '9'))
	return NULL;

    dt = exsltDateCreateDate(EXSLT_UNKNOWN);
    if (dt == NULL)
	return NULL;

    if ((cur[0] == '-') && (cur[1] == '-')) {
	/*
	 * It's an incomplete date (xs:gMonthDay, xs:gMonth or
	 * xs:gDay)
	 */
	cur += 2;

	/* is it an xs:gDay? */
	if (*cur == '-') {
	  ++cur;
	    ret = _exsltDateParseGDay(dt, &cur);
	    if (ret != 0)
		goto error;

	    RETURN_TYPE_IF_VALID(XS_GDAY);

	    goto error;
	}

	/*
	 * it should be an xs:gMonthDay or xs:gMonth
	 */
	ret = _exsltDateParseGMonth(dt, &cur);
	if (ret != 0)
	    goto error;

	if (*cur != '-')
	    goto error;
	cur++;

	/* is it an xs:gMonth? */
	if (*cur == '-') {
	    cur++;
	    RETURN_TYPE_IF_VALID(XS_GMONTH);
	    goto error;
	}

	/* it should be an xs:gMonthDay */
	ret = _exsltDateParseGDay(dt, &cur);
	if (ret != 0)
	    goto error;

	RETURN_TYPE_IF_VALID(XS_GMONTHDAY);

	goto error;
    }

    /*
     * It's a right-truncated date or an xs:time.
     * Try to parse an xs:time then fallback on right-truncated dates.
     */
    if ((*cur >= '0') && (*cur <= '9')) {
	ret = _exsltDateParseTime(dt, &cur);
	if (ret == 0) {
	    /* it's an xs:time */
	    RETURN_TYPE_IF_VALID(XS_TIME);
	}
    }

    /* fallback on date parsing */
    cur = dateTime;

    ret = _exsltDateParseGYear(dt, &cur);
    if (ret != 0)
	goto error;

    /* is it an xs:gYear? */
    RETURN_TYPE_IF_VALID(XS_GYEAR);

    if (*cur != '-')
	goto error;
    cur++;

    ret = _exsltDateParseGMonth(dt, &cur);
    if (ret != 0)
	goto error;

    /* is it an xs:gYearMonth? */
    RETURN_TYPE_IF_VALID(XS_GYEARMONTH);

    if (*cur != '-')
	goto error;
    cur++;

    ret = _exsltDateParseGDay(dt, &cur);
    if ((ret != 0) || !VALID_DATE(dt))
	goto error;

    /* is it an xs:date? */
    RETURN_TYPE_IF_VALID(XS_DATE);

    if (*cur != 'T')
	goto error;
    cur++;

    /* it should be an xs:dateTime */
    ret = _exsltDateParseTime(dt, &cur);
    if (ret != 0)
	goto error;

    ret = _exsltDateParseTimeZone(dt, &cur);
    if ((ret != 0) || (*cur != 0) || !VALID_DATETIME(dt))
	goto error;

    dt->type = XS_DATETIME;

    return dt;

error:
    if (dt != NULL)
	exsltDateFreeDate(dt);
    return NULL;
}

/**
 * exsltDateParseDuration:
 * @duration:  string to analyze
 *
 * Parses a duration string
 *
 * Returns a newly built #exsltDateDurValPtr of NULL in case of error
 */
static exsltDateDurValPtr
exsltDateParseDuration (const xmlChar *duration)
{
    const xmlChar  *cur = duration;
    exsltDateDurValPtr dur;
    int isneg = 0;
    unsigned int seq = 0;
    long days, secs = 0;
    double sec_frac = 0.0;

    if (duration == NULL)
	return NULL;

    if (*cur == '-') {
        isneg = 1;
        cur++;
    }

    /* duration must start with 'P' (after sign) */
    if (*cur++ != 'P')
	return NULL;

    if (*cur == 0)
	return NULL;

    dur = exsltDateCreateDuration();
    if (dur == NULL)
	return NULL;

    while (*cur != 0) {
        long           num = 0;
        size_t         has_digits = 0;
        int            has_frac = 0;
        const xmlChar  desig[] = {'Y', 'M', 'D', 'H', 'M', 'S'};

        /* input string should be empty or invalid date/time item */
        if (seq >= sizeof(desig))
            goto error;

        /* T designator must be present for time items */
        if (*cur == 'T') {
            if (seq > 3)
                goto error;
            cur++;
            seq = 3;
        } else if (seq == 3)
            goto error;

        /* Parse integral part. */
        while (*cur >= '0' && *cur <= '9') {
            long digit = *cur - '0';

            if (num > LONG_MAX / 10)
                goto error;
            num *= 10;
            if (num > LONG_MAX - digit)
                goto error;
            num += digit;

            has_digits = 1;
            cur++;
        }

        if (*cur == '.') {
            /* Parse fractional part. */
            double mult = 1.0;
            cur++;
            has_frac = 1;
            while (*cur >= '0' && *cur <= '9') {
                mult /= 10.0;
                sec_frac += (*cur - '0') * mult;
                has_digits = 1;
                cur++;
            }
        }

        while (*cur != desig[seq]) {
            seq++;
            /* No T designator or invalid char. */
            if (seq == 3 || seq == sizeof(desig))
                goto error;
        }
        cur++;

        if (!has_digits || (has_frac && (seq != 5)))
            goto error;

        switch (seq) {
            case 0:
                /* Year */
                if (num > LONG_MAX / 12)
                    goto error;
                dur->mon = num * 12;
                break;
            case 1:
                /* Month */
                if (dur->mon > LONG_MAX - num)
                    goto error;
                dur->mon += num;
                break;
            case 2:
                /* Day */
                dur->day = num;
                break;
            case 3:
                /* Hour */
                days = num / HOURS_PER_DAY;
                if (dur->day > LONG_MAX - days)
                    goto error;
                dur->day += days;
                secs = (num % HOURS_PER_DAY) * SECS_PER_HOUR;
                break;
            case 4:
                /* Minute */
                days = num / MINS_PER_DAY;
                if (dur->day > LONG_MAX - days)
                    goto error;
                dur->day += days;
                secs += (num % MINS_PER_DAY) * SECS_PER_MIN;
                break;
            case 5:
                /* Second */
                days = num / SECS_PER_DAY;
                if (dur->day > LONG_MAX - days)
                    goto error;
                dur->day += days;
                secs += num % SECS_PER_DAY;
                break;
        }

        seq++;
    }

    days = secs / SECS_PER_DAY;
    if (dur->day > LONG_MAX - days)
        goto error;
    dur->day += days;
    dur->sec = (secs % SECS_PER_DAY) + sec_frac;

    if (isneg) {
        dur->mon = -dur->mon;
        dur->day = -dur->day;
        if (dur->sec != 0.0) {
            dur->sec = SECS_PER_DAY - dur->sec;
            dur->day -= 1;
        }
    }

#ifdef DEBUG_EXSLT_DATE
    xsltGenericDebug(xsltGenericDebugContext,
		     "Parsed duration %f\n", dur->sec);
#endif

    return dur;

error:
    if (dur != NULL)
	exsltDateFreeDuration(dur);
    return NULL;
}

static void
exsltFormatLong(xmlChar **cur, xmlChar *end, long num) {
    xmlChar buf[20];
    int i = 0;

    while (i < 20) {
        buf[i++] = '0' + num % 10;
        num /= 10;
        if (num == 0)
            break;
    }

    while (i > 0) {
        if (*cur < end)
            *(*cur)++ = buf[--i];
    }
}

static void
exsltFormatNanoseconds(xmlChar **cur, xmlChar *end, long nsecs) {
    long p10, digit;

    if (nsecs > 0) {
        if (*cur < end)
            *(*cur)++ = '.';
        p10 = 100000000;
        while (nsecs > 0) {
            digit = nsecs / p10;
            if (*cur < end)
                *(*cur)++ = '0' + digit;
            nsecs -= digit * p10;
            p10 /= 10;
        }
    }
}

/**
 * exsltDateFormatDuration:
 * @dur: an #exsltDateDurValPtr
 *
 * Formats the duration.
 *
 * Returns a newly allocated string, or NULL in case of error
 */
static xmlChar *
exsltDateFormatDuration (const exsltDateDurValPtr dur)
{
    xmlChar buf[100], *cur = buf, *end = buf + 99;
    double secs, tmp;
    long days, months, intSecs, nsecs;

    if (dur == NULL)
	return NULL;

    /* quick and dirty check */
    if ((dur->sec == 0.0) && (dur->day == 0) && (dur->mon == 0))
        return xmlStrdup((xmlChar*)"P0D");

    secs   = dur->sec;
    days   = dur->day;
    months = dur->mon;

    *cur = '\0';
    if (days < 0) {
        if (secs != 0.0) {
            secs = SECS_PER_DAY - secs;
            days += 1;
        }
        days = -days;
        *cur = '-';
    }
    if (months < 0) {
        months = -months;
        *cur = '-';
    }
    if (*cur == '-')
	cur++;

    *cur++ = 'P';

    if (months >= 12) {
        long years = months / 12;

        months -= years * 12;
        exsltFormatLong(&cur, end, years);
        if (cur < end)
            *cur++ = 'Y';
    }

    if (months != 0) {
        exsltFormatLong(&cur, end, months);
        if (cur < end)
            *cur++ = 'M';
    }

    if (days != 0) {
        exsltFormatLong(&cur, end, days);
        if (cur < end)
            *cur++ = 'D';
    }

    tmp = floor(secs);
    intSecs = (long) tmp;
    /* Round to nearest to avoid issues with floating point precision */
    nsecs = (long) floor((secs - tmp) * 1000000000 + 0.5);
    if (nsecs >= 1000000000) {
        nsecs -= 1000000000;
        intSecs += 1;
    }

    if ((intSecs > 0) || (nsecs > 0)) {
        if (cur < end)
            *cur++ = 'T';

        if (intSecs >= SECS_PER_HOUR) {
            long hours = intSecs / SECS_PER_HOUR;

            intSecs -= hours * SECS_PER_HOUR;
            exsltFormatLong(&cur, end, hours);
            if (cur < end)
                *cur++ = 'H';
        }

        if (intSecs >= SECS_PER_MIN) {
            long mins = intSecs / SECS_PER_MIN;

            intSecs -= mins * SECS_PER_MIN;
            exsltFormatLong(&cur, end, mins);
            if (cur < end)
                *cur++ = 'M';
        }

        if ((intSecs > 0) || (nsecs > 0)) {
            exsltFormatLong(&cur, end, intSecs);
            exsltFormatNanoseconds(&cur, end, nsecs);
            if (cur < end)
                *cur++ = 'S';
        }
    }

    *cur = 0;

    return xmlStrdup(buf);
}

static void
exsltFormatTwoDigits(xmlChar **cur, xmlChar *end, int num) {
    if (num < 0 || num >= 100)
        return;
    if (*cur < end)
        *(*cur)++ = '0' + num / 10;
    if (*cur < end)
        *(*cur)++ = '0' + num % 10;
}

static void
exsltFormatTime(xmlChar **cur, xmlChar *end, exsltDateValPtr dt) {
    double tmp;
    long intSecs, nsecs;

    exsltFormatTwoDigits(cur, end, dt->hour);
    if (*cur < end)
        *(*cur)++ = ':';

    exsltFormatTwoDigits(cur, end, dt->min);
    if (*cur < end)
        *(*cur)++ = ':';

    tmp = floor(dt->sec);
    intSecs = (long) tmp;
    /*
     * Round to nearest to avoid issues with floating point precision,
     * but don't carry over so seconds stay below 60.
     */
    nsecs = (long) floor((dt->sec - tmp) * 1000000000 + 0.5);
    if (nsecs > 999999999)
        nsecs = 999999999;
    exsltFormatTwoDigits(cur, end, intSecs);
    exsltFormatNanoseconds(cur, end, nsecs);
}

/**
 * exsltDateFormatDateTime:
 * @dt: an #exsltDateValPtr
 *
 * Formats @dt in xs:dateTime format.
 *
 * Returns a newly allocated string, or NULL in case of error
 */
static xmlChar *
exsltDateFormatDateTime (const exsltDateValPtr dt)
{
    xmlChar buf[100], *cur = buf, *end = buf + 99;

    if ((dt == NULL) ||	!VALID_DATETIME(dt))
	return NULL;

    exsltFormatYearMonthDay(&cur, end, dt);
    if (cur < end)
        *cur++ = 'T';
    exsltFormatTime(&cur, end, dt);
    exsltFormatTimeZone(&cur, end, dt->tzo);
    *cur = 0;

    return xmlStrdup(buf);
}

/**
 * exsltDateFormatDate:
 * @dt: an #exsltDateValPtr
 *
 * Formats @dt in xs:date format.
 *
 * Returns a newly allocated string, or NULL in case of error
 */
static xmlChar *
exsltDateFormatDate (const exsltDateValPtr dt)
{
    xmlChar buf[100], *cur = buf, *end = buf + 99;

    if ((dt == NULL) || !VALID_DATETIME(dt))
	return NULL;

    exsltFormatYearMonthDay(&cur, end, dt);
    if (dt->tz_flag || (dt->tzo != 0)) {
        exsltFormatTimeZone(&cur, end, dt->tzo);
    }
    *cur = 0;

    return xmlStrdup(buf);
}

/**
 * exsltDateFormatTime:
 * @dt: an #exsltDateValPtr
 *
 * Formats @dt in xs:time format.
 *
 * Returns a newly allocated string, or NULL in case of error
 */
static xmlChar *
exsltDateFormatTime (const exsltDateValPtr dt)
{
    xmlChar buf[100], *cur = buf, *end = buf + 99;

    if ((dt == NULL) || !VALID_TIME(dt))
	return NULL;

    exsltFormatTime(&cur, end, dt);
    if (dt->tz_flag || (dt->tzo != 0)) {
        exsltFormatTimeZone(&cur, end, dt->tzo);
    }
    *cur = 0;

    return xmlStrdup(buf);
}

/**
 * exsltDateFormat:
 * @dt: an #exsltDateValPtr
 *
 * Formats @dt in the proper format.
 * Note: xs:gmonth and xs:gday are not formatted as there are no
 * routines that output them.
 *
 * Returns a newly allocated string, or NULL in case of error
 */
static xmlChar *
exsltDateFormat (const exsltDateValPtr dt)
{
    if (dt == NULL)
	return NULL;

    switch (dt->type) {
    case XS_DATETIME:
        return exsltDateFormatDateTime(dt);
    case XS_DATE:
        return exsltDateFormatDate(dt);
    case XS_TIME:
        return exsltDateFormatTime(dt);
    default:
        break;
    }

    if (dt->type & XS_GYEAR) {
        xmlChar buf[100], *cur = buf, *end = buf + 99;

        exsltFormatGYear(&cur, end, dt->year);
        if (dt->type == XS_GYEARMONTH) {
            if (cur < end)
	        *cur++ = '-';
            exsltFormat2Digits(&cur, end, dt->mon);
        }

        if (dt->tz_flag || (dt->tzo != 0)) {
            exsltFormatTimeZone(&cur, end, dt->tzo);
        }
        *cur = 0;
        return xmlStrdup(buf);
    }

    return NULL;
}

/**
 * _exsltDateCastYMToDays:
 * @dt: an #exsltDateValPtr
 *
 * Convert mon and year of @dt to total number of days. Take the
 * number of years since (or before) 1 AD and add the number of leap
 * years. This is a function  because negative
 * years must be handled a little differently.
 *
 * Returns number of days.
 */
static long
_exsltDateCastYMToDays (const exsltDateValPtr dt)
{
    long ret;

    if (dt->year <= 0)
        ret = ((dt->year-1) * 365) +
              (((dt->year)/4)-((dt->year)/100)+
               ((dt->year)/400)) +
              DAY_IN_YEAR(0, dt->mon, dt->year) - 1;
    else
        ret = ((dt->year-1) * 365) +
              (((dt->year-1)/4)-((dt->year-1)/100)+
               ((dt->year-1)/400)) +
              DAY_IN_YEAR(0, dt->mon, dt->year);

    return ret;
}

/**
 * TIME_TO_NUMBER:
 * @dt:  an #exsltDateValPtr
 *
 * Calculates the number of seconds in the time portion of @dt.
 *
 * Returns seconds.
 */
#define TIME_TO_NUMBER(dt)                              \
    ((double)((dt->hour * SECS_PER_HOUR) +   \
              (dt->min * SECS_PER_MIN)) + dt->sec)

/**
 * _exsltDateTruncateDate:
 * @dt: an #exsltDateValPtr
 * @type: dateTime type to set to
 *
 * Set @dt to truncated @type.
 *
 * Returns 0 success, non-zero otherwise.
 */
static int
_exsltDateTruncateDate (exsltDateValPtr dt, exsltDateType type)
{
    if (dt == NULL)
        return 1;

    if ((type & XS_TIME) != XS_TIME) {
        dt->hour = 0;
        dt->min  = 0;
        dt->sec  = 0.0;
    }

    if ((type & XS_GDAY) != XS_GDAY)
        dt->day = 1;

    if ((type & XS_GMONTH) != XS_GMONTH)
        dt->mon = 1;

    if ((type & XS_GYEAR) != XS_GYEAR)
        dt->year = 0;

    dt->type = type;

    return 0;
}

/**
 * _exsltDayInWeek:
 * @yday: year day (1-366)
 * @yr: year
 *
 * Determine the day-in-week from @yday and @yr. 0001-01-01 was
 * a Monday so all other days are calculated from there. Take the
 * number of years since (or before) add the number of leap years and
 * the day-in-year and mod by 7. This is a function  because negative
 * years must be handled a little differently.
 *
 * Returns day in week (Sunday = 0).
 */
static long
_exsltDateDayInWeek(long yday, long yr)
{
    long ret;

    if (yr <= 0) {
        /* Compute modulus twice to avoid integer overflow */
        ret = ((yr%7-2 + ((yr/4)-(yr/100)+(yr/400)) + yday) % 7);
        if (ret < 0)
            ret += 7;
    } else
        ret = (((yr%7-1) + (((yr-1)/4)-((yr-1)/100)+((yr-1)/400)) + yday) % 7);

    return ret;
}

/**
 * _exsltDateAdd:
 * @dt: an #exsltDateValPtr
 * @dur: an #exsltDateDurValPtr
 *
 * Compute a new date/time from @dt and @dur. This function assumes @dt
 * is either #XS_DATETIME, #XS_DATE, #XS_GYEARMONTH, or #XS_GYEAR.
 *
 * Returns date/time pointer or NULL.
 */
static exsltDateValPtr
_exsltDateAdd (exsltDateValPtr dt, exsltDateDurValPtr dur)
{
    exsltDateValPtr ret;
    long carry, temp;
    double sum;

    if ((dt == NULL) || (dur == NULL))
        return NULL;

    ret = exsltDateCreateDate(dt->type);
    if (ret == NULL)
        return NULL;

    /*
     * Note that temporary values may need more bits than the values in
     * bit field.
     */

    /* month */
    temp  = dt->mon + dur->mon % 12;
    carry = dur->mon / 12;
    if (temp < 1) {
        temp  += 12;
        carry -= 1;
    }
    else if (temp > 12) {
        temp  -= 12;
        carry += 1;
    }
    ret->mon = temp;

    /*
     * year (may be modified later)
     *
     * Add epochs from dur->day now to avoid overflow later and to speed up
     * pathological cases.
     */
    carry += (dur->day / DAYS_PER_EPOCH) * YEARS_PER_EPOCH;
    if ((carry > 0 && dt->year > YEAR_MAX - carry) ||
        (carry < 0 && dt->year < YEAR_MIN - carry)) {
        /* Overflow */
        exsltDateFreeDate(ret);
        return NULL;
    }
    ret->year = dt->year + carry;

    /* time zone */
    ret->tzo     = dt->tzo;
    ret->tz_flag = dt->tz_flag;

    /* seconds */
    sum    = dt->sec + dur->sec;
    ret->sec = fmod(sum, 60.0);
    carry  = (long)(sum / 60.0);

    /* minute */
    temp  = dt->min + carry % 60;
    carry = carry / 60;
    if (temp >= 60) {
        temp  -= 60;
        carry += 1;
    }
    ret->min = temp;

    /* hours */
    temp  = dt->hour + carry % 24;
    carry = carry / 24;
    if (temp >= 24) {
        temp  -= 24;
        carry += 1;
    }
    ret->hour = temp;

    /* days */
    if (dt->day > MAX_DAYINMONTH(ret->year, ret->mon))
        temp = MAX_DAYINMONTH(ret->year, ret->mon);
    else if (dt->day < 1)
        temp = 1;
    else
        temp = dt->day;

    temp += dur->day % DAYS_PER_EPOCH + carry;

    while (1) {
        if (temp < 1) {
            if (ret->mon > 1) {
                ret->mon -= 1;
            }
            else {
                if (ret->year == YEAR_MIN) {
                    exsltDateFreeDate(ret);
                    return NULL;
                }
                ret->mon   = 12;
                ret->year -= 1;
            }
            temp += MAX_DAYINMONTH(ret->year, ret->mon);
        } else if (temp > (long)MAX_DAYINMONTH(ret->year, ret->mon)) {
            temp -= MAX_DAYINMONTH(ret->year, ret->mon);
            if (ret->mon < 12) {
                ret->mon += 1;
            }
            else {
                if (ret->year == YEAR_MAX) {
                    exsltDateFreeDate(ret);
                    return NULL;
                }
                ret->mon   = 1;
                ret->year += 1;
            }
        } else
            break;
    }

    ret->day = temp;

    /*
     * adjust the date/time type to the date values
     */
    if (ret->type != XS_DATETIME) {
        if ((ret->hour) || (ret->min) || (ret->sec))
            ret->type = XS_DATETIME;
        else if (ret->type != XS_DATE) {
            if (ret->day != 1)
                ret->type = XS_DATE;
            else if ((ret->type != XS_GYEARMONTH) && (ret->mon != 1))
                ret->type = XS_GYEARMONTH;
        }
    }

    return ret;
}

/**
 * _exsltDateDifference:
 * @x: an #exsltDateValPtr
 * @y: an #exsltDateValPtr
 * @flag: force difference in days
 *
 * Calculate the difference between @x and @y as a duration
 * (i.e. y - x). If the @flag is set then even if the least specific
 * format of @x or @y is xs:gYear or xs:gYearMonth.
 *
 * Returns a duration pointer or NULL.
 */
static exsltDateDurValPtr
_exsltDateDifference (exsltDateValPtr x, exsltDateValPtr y, int flag)
{
    exsltDateDurValPtr ret;

    if ((x == NULL) || (y == NULL))
        return NULL;

    if (((x->type < XS_GYEAR) || (x->type > XS_DATETIME)) ||
        ((y->type < XS_GYEAR) || (y->type > XS_DATETIME)))
        return NULL;

    /*
     * the operand with the most specific format must be converted to
     * the same type as the operand with the least specific format.
     */
    if (x->type != y->type) {
        if (x->type < y->type) {
            _exsltDateTruncateDate(y, x->type);
        } else {
            _exsltDateTruncateDate(x, y->type);
        }
    }

    ret = exsltDateCreateDuration();
    if (ret == NULL)
        return NULL;

    if (((x->type == XS_GYEAR) || (x->type == XS_GYEARMONTH)) && (!flag)) {
        /* compute the difference in months */
        if ((x->year >= LONG_MAX / 24) || (x->year <= LONG_MIN / 24) ||
            (y->year >= LONG_MAX / 24) || (y->year <= LONG_MIN / 24)) {
            /* Possible overflow. */
            exsltDateFreeDuration(ret);
            return NULL;
        }
        ret->mon = (y->year - x->year) * 12 + (y->mon - x->mon);
    } else {
        long carry;

        if ((x->year > LONG_MAX / 731) || (x->year < LONG_MIN / 731) ||
            (y->year > LONG_MAX / 731) || (y->year < LONG_MIN / 731)) {
            /* Possible overflow. */
            exsltDateFreeDuration(ret);
            return NULL;
        }

        ret->sec  = TIME_TO_NUMBER(y) - TIME_TO_NUMBER(x);
        ret->sec += (x->tzo - y->tzo) * SECS_PER_MIN;
        carry    = (long)floor(ret->sec / SECS_PER_DAY);
        ret->sec  = ret->sec - carry * SECS_PER_DAY;

        ret->day  = _exsltDateCastYMToDays(y) - _exsltDateCastYMToDays(x);
        ret->day += y->day - x->day;
        ret->day += carry;
    }

    return ret;
}

/**
 * _exsltDateAddDurCalc
 * @ret: an exsltDateDurValPtr for the return value:
 * @x: an exsltDateDurValPtr for the first operand
 * @y: an exsltDateDurValPtr for the second operand
 *
 * Add two durations, catering for possible negative values.
 * The sum is placed in @ret.
 *
 * Returns 1 for success, 0 if error detected.
 */
static int
_exsltDateAddDurCalc (exsltDateDurValPtr ret, exsltDateDurValPtr x,
		      exsltDateDurValPtr y)
{
    /* months */
    if ((x->mon > 0 && y->mon >  LONG_MAX - x->mon) ||
        (x->mon < 0 && y->mon <= LONG_MIN - x->mon)) {
        /* Overflow */
        return 0;
    }
    ret->mon = x->mon + y->mon;

    /* days */
    if ((x->day > 0 && y->day >  LONG_MAX - x->day) ||
        (x->day < 0 && y->day <= LONG_MIN - x->day)) {
        /* Overflow */
        return 0;
    }
    ret->day = x->day + y->day;

    /* seconds */
    ret->sec = x->sec + y->sec;
    if (ret->sec >= SECS_PER_DAY) {
        if (ret->day == LONG_MAX) {
            /* Overflow */
            return 0;
        }
        ret->sec -= SECS_PER_DAY;
        ret->day += 1;
    }

    /*
     * are the results indeterminate? i.e. how do you subtract days from
     * months or years?
     */
    if (ret->day >= 0) {
        if (((ret->day > 0) || (ret->sec > 0)) && (ret->mon < 0))
            return 0;
    }
    else {
        if (ret->mon > 0)
            return 0;
    }
    return 1;
}

/**
 * _exsltDateAddDuration:
 * @x: an #exsltDateDurValPtr
 * @y: an #exsltDateDurValPtr
 *
 * Compute a new duration from @x and @y.
 *
 * Returns a duration pointer or NULL.
 */
static exsltDateDurValPtr
_exsltDateAddDuration (exsltDateDurValPtr x, exsltDateDurValPtr y)
{
    exsltDateDurValPtr ret;

    if ((x == NULL) || (y == NULL))
        return NULL;

    ret = exsltDateCreateDuration();
    if (ret == NULL)
        return NULL;

    if (_exsltDateAddDurCalc(ret, x, y))
        return ret;

    exsltDateFreeDuration(ret);
    return NULL;
}

/****************************************************************
 *								*
 *		EXSLT - Dates and Times functions		*
 *								*
 ****************************************************************/

/**
 * exsltDateDateTime:
 *
 * Implements the EXSLT - Dates and Times date-time() function:
 *     string date:date-time()
 *
 * Returns the current date and time as a date/time string.
 */
static xmlChar *
exsltDateDateTime (void)
{
    xmlChar *ret = NULL;
    exsltDateValPtr cur;

    cur = exsltDateCurrent();
    if (cur != NULL) {
	ret = exsltDateFormatDateTime(cur);
	exsltDateFreeDate(cur);
    }

    return ret;
}

/**
 * exsltDateDate:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times date() function:
 *     string date:date (string?)
 *
 * Returns the date specified in the date/time string given as the
 * argument.  If no argument is given, then the current local
 * date/time, as returned by date:date-time is used as a default
 * argument.
 * The date/time string specified as an argument must be a string in
 * the format defined as the lexical representation of either
 * xs:dateTime or xs:date.  If the argument is not in either of these
 * formats, returns NULL.
 */
static xmlChar *
exsltDateDate (const xmlChar *dateTime)
{
    exsltDateValPtr dt = NULL;
    xmlChar *ret = NULL;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return NULL;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return NULL;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_DATE)) {
	    exsltDateFreeDate(dt);
	    return NULL;
	}
    }

    ret = exsltDateFormatDate(dt);
    exsltDateFreeDate(dt);

    return ret;
}

/**
 * exsltDateTime:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times time() function:
 *     string date:time (string?)
 *
 * Returns the time specified in the date/time string given as the
 * argument.  If no argument is given, then the current local
 * date/time, as returned by date:date-time is used as a default
 * argument.
 * The date/time string specified as an argument must be a string in
 * the format defined as the lexical representation of either
 * xs:dateTime or xs:time.  If the argument is not in either of these
 * formats, returns NULL.
 */
static xmlChar *
exsltDateTime (const xmlChar *dateTime)
{
    exsltDateValPtr dt = NULL;
    xmlChar *ret = NULL;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return NULL;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return NULL;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_TIME)) {
	    exsltDateFreeDate(dt);
	    return NULL;
	}
    }

    ret = exsltDateFormatTime(dt);
    exsltDateFreeDate(dt);

    return ret;
}

/**
 * exsltDateYear:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times year() function
 *    number date:year (string?)
 * Returns the year of a date as a number.  If no argument is given,
 * then the current local date/time, as returned by date:date-time is
 * used as a default argument.
 * The date/time string specified as the first argument must be a
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 *  - xs:gYearMonth (CCYY-MM)
 *  - xs:gYear (CCYY)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 */
static double
exsltDateYear (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    long year;
    double ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_DATE) &&
	    (dt->type != XS_GYEARMONTH) && (dt->type != XS_GYEAR)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    year = dt->year;
    if (year <= 0) year -= 1; /* Adjust for missing year 0. */
    ret = (double) year;
    exsltDateFreeDate(dt);

    return ret;
}

/**
 * exsltDateLeapYear:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times leap-year() function:
 *    boolean date:leap-yea (string?)
 * Returns true if the year given in a date is a leap year.  If no
 * argument is given, then the current local date/time, as returned by
 * date:date-time is used as a default argument.
 * The date/time string specified as the first argument must be a
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 *  - xs:gYearMonth (CCYY-MM)
 *  - xs:gYear (CCYY)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 */
static xmlXPathObjectPtr
exsltDateLeapYear (const xmlChar *dateTime)
{
    exsltDateValPtr dt = NULL;
    xmlXPathObjectPtr ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
    } else {
	dt = exsltDateParse(dateTime);
	if ((dt != NULL) &&
            (dt->type != XS_DATETIME) && (dt->type != XS_DATE) &&
	    (dt->type != XS_GYEARMONTH) && (dt->type != XS_GYEAR)) {
	    exsltDateFreeDate(dt);
	    dt = NULL;
	}
    }

    if (dt == NULL) {
        ret = xmlXPathNewFloat(xmlXPathNAN);
    }
    else {
        ret = xmlXPathNewBoolean(IS_LEAP(dt->year));
        exsltDateFreeDate(dt);
    }

    return ret;
}

/**
 * exsltDateMonthInYear:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times month-in-year() function:
 *    number date:month-in-year (string?)
 * Returns the month of a date as a number.  If no argument is given,
 * then the current local date/time, as returned by date:date-time is
 * used the default argument.
 * The date/time string specified as the argument is a left or
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 *  - xs:gYearMonth (CCYY-MM)
 *  - xs:gMonth (--MM--)
 *  - xs:gMonthDay (--MM-DD)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 */
static double
exsltDateMonthInYear (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    double ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_DATE) &&
	    (dt->type != XS_GYEARMONTH) && (dt->type != XS_GMONTH) &&
	    (dt->type != XS_GMONTHDAY)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    ret = (double) dt->mon;
    exsltDateFreeDate(dt);

    return ret;
}

/**
 * exsltDateMonthName:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Time month-name() function
 *    string date:month-name (string?)
 * Returns the full name of the month of a date.  If no argument is
 * given, then the current local date/time, as returned by
 * date:date-time is used the default argument.
 * The date/time string specified as the argument is a left or
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 *  - xs:gYearMonth (CCYY-MM)
 *  - xs:gMonth (--MM--)
 * If the date/time string is not in one of these formats, then an
 * empty string ('') is returned.
 * The result is an English month name: one of 'January', 'February',
 * 'March', 'April', 'May', 'June', 'July', 'August', 'September',
 * 'October', 'November' or 'December'.
 */
static const xmlChar *
exsltDateMonthName (const xmlChar *dateTime)
{
    static const xmlChar monthNames[13][10] = {
        { 0 },
	{ 'J', 'a', 'n', 'u', 'a', 'r', 'y', 0 },
	{ 'F', 'e', 'b', 'r', 'u', 'a', 'r', 'y', 0 },
	{ 'M', 'a', 'r', 'c', 'h', 0 },
	{ 'A', 'p', 'r', 'i', 'l', 0 },
	{ 'M', 'a', 'y', 0 },
	{ 'J', 'u', 'n', 'e', 0 },
	{ 'J', 'u', 'l', 'y', 0 },
	{ 'A', 'u', 'g', 'u', 's', 't', 0 },
	{ 'S', 'e', 'p', 't', 'e', 'm', 'b', 'e', 'r', 0 },
	{ 'O', 'c', 't', 'o', 'b', 'e', 'r', 0 },
	{ 'N', 'o', 'v', 'e', 'm', 'b', 'e', 'r', 0 },
	{ 'D', 'e', 'c', 'e', 'm', 'b', 'e', 'r', 0 }
    };
    double month;
    int index = 0;
    month = exsltDateMonthInYear(dateTime);
    if (!xmlXPathIsNaN(month) && (month >= 1.0) && (month <= 12.0))
      index = (int) month;
    return monthNames[index];
}

/**
 * exsltDateMonthAbbreviation:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Time month-abbreviation() function
 *    string date:month-abbreviation (string?)
 * Returns the abbreviation of the month of a date.  If no argument is
 * given, then the current local date/time, as returned by
 * date:date-time is used the default argument.
 * The date/time string specified as the argument is a left or
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 *  - xs:gYearMonth (CCYY-MM)
 *  - xs:gMonth (--MM--)
 * If the date/time string is not in one of these formats, then an
 * empty string ('') is returned.
 * The result is an English month abbreviation: one of 'Jan', 'Feb',
 * 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov' or
 * 'Dec'.
 */
static const xmlChar *
exsltDateMonthAbbreviation (const xmlChar *dateTime)
{
    static const xmlChar monthAbbreviations[13][4] = {
        { 0 },
	{ 'J', 'a', 'n', 0 },
	{ 'F', 'e', 'b', 0 },
	{ 'M', 'a', 'r', 0 },
	{ 'A', 'p', 'r', 0 },
	{ 'M', 'a', 'y', 0 },
	{ 'J', 'u', 'n', 0 },
	{ 'J', 'u', 'l', 0 },
	{ 'A', 'u', 'g', 0 },
	{ 'S', 'e', 'p', 0 },
	{ 'O', 'c', 't', 0 },
	{ 'N', 'o', 'v', 0 },
	{ 'D', 'e', 'c', 0 }
    };
    double month;
    int index = 0;
    month = exsltDateMonthInYear(dateTime);
    if (!xmlXPathIsNaN(month) && (month >= 1.0) && (month <= 12.0))
      index = (int) month;
    return monthAbbreviations[index];
}

/**
 * exsltDateWeekInYear:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times week-in-year() function
 *    number date:week-in-year (string?)
 * Returns the week of the year as a number.  If no argument is given,
 * then the current local date/time, as returned by date:date-time is
 * used as the default argument.  For the purposes of numbering,
 * counting follows ISO 8601: week 1 in a year is the week containing
 * the first Thursday of the year, with new weeks beginning on a
 * Monday.
 * The date/time string specified as the argument is a right-truncated
 * string in the format defined as the lexical representation of
 * xs:dateTime in one of the formats defined in [XML Schema Part 2:
 * Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 */
static double
exsltDateWeekInYear (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    long diy, diw, year, ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_DATE)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    diy = DAY_IN_YEAR(dt->day, dt->mon, dt->year);

    /*
     * Determine day-in-week (0=Sun, 1=Mon, etc.) then adjust so Monday
     * is the first day-in-week
     */
    diw = (_exsltDateDayInWeek(diy, dt->year) + 6) % 7;

    /* ISO 8601 adjustment, 3 is Thu */
    diy += (3 - diw);
    if(diy < 1) {
	year = dt->year - 1;
	if(year == 0) year--;
	diy = DAY_IN_YEAR(31, 12, year) + diy;
    } else if (diy > (long)DAY_IN_YEAR(31, 12, dt->year)) {
	diy -= DAY_IN_YEAR(31, 12, dt->year);
    }

    ret = ((diy - 1) / 7) + 1;

    exsltDateFreeDate(dt);

    return (double) ret;
}

/**
 * exsltDateWeekInMonth:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times week-in-month() function
 *    number date:week-in-month (string?)
 * The date:week-in-month function returns the week in a month of a
 * date as a number. If no argument is given, then the current local
 * date/time, as returned by date:date-time is used the default
 * argument. For the purposes of numbering, the first day of the month
 * is in week 1 and new weeks begin on a Monday (so the first and last
 * weeks in a month will often have less than 7 days in them).
 * The date/time string specified as the argument is a right-truncated
 * string in the format defined as the lexical representation of
 * xs:dateTime in one of the formats defined in [XML Schema Part 2:
 * Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 */
static double
exsltDateWeekInMonth (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    long fdiy, fdiw, ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_DATE)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    fdiy = DAY_IN_YEAR(1, dt->mon, dt->year);
    /*
     * Determine day-in-week (0=Sun, 1=Mon, etc.) then adjust so Monday
     * is the first day-in-week
     */
    fdiw = (_exsltDateDayInWeek(fdiy, dt->year) + 6) % 7;

    ret = ((dt->day + fdiw - 1) / 7) + 1;

    exsltDateFreeDate(dt);

    return (double) ret;
}

/**
 * exsltDateDayInYear:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times day-in-year() function
 *    number date:day-in-year (string?)
 * Returns the day of a date in a year as a number.  If no argument is
 * given, then the current local date/time, as returned by
 * date:date-time is used the default argument.
 * The date/time string specified as the argument is a right-truncated
 * string in the format defined as the lexical representation of
 * xs:dateTime in one of the formats defined in [XML Schema Part 2:
 * Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 */
static double
exsltDateDayInYear (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    long ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_DATE)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    ret = DAY_IN_YEAR(dt->day, dt->mon, dt->year);

    exsltDateFreeDate(dt);

    return (double) ret;
}

/**
 * exsltDateDayInMonth:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times day-in-month() function:
 *    number date:day-in-month (string?)
 * Returns the day of a date as a number.  If no argument is given,
 * then the current local date/time, as returned by date:date-time is
 * used the default argument.
 * The date/time string specified as the argument is a left or
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 *  - xs:gMonthDay (--MM-DD)
 *  - xs:gDay (---DD)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 */
static double
exsltDateDayInMonth (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    double ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_DATE) &&
	    (dt->type != XS_GMONTHDAY) && (dt->type != XS_GDAY)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    ret = (double) dt->day;
    exsltDateFreeDate(dt);

    return ret;
}

/**
 * exsltDateDayOfWeekInMonth:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times day-of-week-in-month() function:
 *    number date:day-of-week-in-month (string?)
 * Returns the day-of-the-week in a month of a date as a number
 * (e.g. 3 for the 3rd Tuesday in May).  If no argument is
 * given, then the current local date/time, as returned by
 * date:date-time is used the default argument.
 * The date/time string specified as the argument is a right-truncated
 * string in the format defined as the lexical representation of
 * xs:dateTime in one of the formats defined in [XML Schema Part 2:
 * Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 */
static double
exsltDateDayOfWeekInMonth (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    long ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_DATE)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    ret = ((dt->day -1) / 7) + 1;

    exsltDateFreeDate(dt);

    return (double) ret;
}

/**
 * exsltDateDayInWeek:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times day-in-week() function:
 *    number date:day-in-week (string?)
 * Returns the day of the week given in a date as a number.  If no
 * argument is given, then the current local date/time, as returned by
 * date:date-time is used the default argument.
 * The date/time string specified as the argument is a left or
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 * The numbering of days of the week starts at 1 for Sunday, 2 for
 * Monday and so on up to 7 for Saturday.
 */
static double
exsltDateDayInWeek (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    long diy, ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_DATE)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    diy = DAY_IN_YEAR(dt->day, dt->mon, dt->year);

    ret = _exsltDateDayInWeek(diy, dt->year) + 1;

    exsltDateFreeDate(dt);

    return (double) ret;
}

/**
 * exsltDateDayName:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Time day-name() function
 *    string date:day-name (string?)
 * Returns the full name of the day of the week of a date.  If no
 * argument is given, then the current local date/time, as returned by
 * date:date-time is used the default argument.
 * The date/time string specified as the argument is a left or
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 * If the date/time string is not in one of these formats, then an
 * empty string ('') is returned.
 * The result is an English day name: one of 'Sunday', 'Monday',
 * 'Tuesday', 'Wednesday', 'Thursday' or 'Friday'.
 */
static const xmlChar *
exsltDateDayName (const xmlChar *dateTime)
{
    static const xmlChar dayNames[8][10] = {
        { 0 },
	{ 'S', 'u', 'n', 'd', 'a', 'y', 0 },
	{ 'M', 'o', 'n', 'd', 'a', 'y', 0 },
	{ 'T', 'u', 'e', 's', 'd', 'a', 'y', 0 },
	{ 'W', 'e', 'd', 'n', 'e', 's', 'd', 'a', 'y', 0 },
	{ 'T', 'h', 'u', 'r', 's', 'd', 'a', 'y', 0 },
	{ 'F', 'r', 'i', 'd', 'a', 'y', 0 },
	{ 'S', 'a', 't', 'u', 'r', 'd', 'a', 'y', 0 }
    };
    double day;
    int index = 0;
    day = exsltDateDayInWeek(dateTime);
    if(!xmlXPathIsNaN(day) && (day >= 1.0) && (day <= 7.0))
      index = (int) day;
    return dayNames[index];
}

/**
 * exsltDateDayAbbreviation:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Time day-abbreviation() function
 *    string date:day-abbreviation (string?)
 * Returns the abbreviation of the day of the week of a date.  If no
 * argument is given, then the current local date/time, as returned by
 * date:date-time is used the default argument.
 * The date/time string specified as the argument is a left or
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 * If the date/time string is not in one of these formats, then an
 * empty string ('') is returned.
 * The result is a three-letter English day abbreviation: one of
 * 'Sun', 'Mon', 'Tue', 'Wed', 'Thu' or 'Fri'.
 */
static const xmlChar *
exsltDateDayAbbreviation (const xmlChar *dateTime)
{
    static const xmlChar dayAbbreviations[8][4] = {
        { 0 },
	{ 'S', 'u', 'n', 0 },
	{ 'M', 'o', 'n', 0 },
	{ 'T', 'u', 'e', 0 },
	{ 'W', 'e', 'd', 0 },
	{ 'T', 'h', 'u', 0 },
	{ 'F', 'r', 'i', 0 },
	{ 'S', 'a', 't', 0 }
    };
    double day;
    int index = 0;
    day = exsltDateDayInWeek(dateTime);
    if(!xmlXPathIsNaN(day) && (day >= 1.0) && (day <= 7.0))
      index = (int) day;
    return dayAbbreviations[index];
}

/**
 * exsltDateHourInDay:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times day-in-month() function:
 *    number date:day-in-month (string?)
 * Returns the hour of the day as a number.  If no argument is given,
 * then the current local date/time, as returned by date:date-time is
 * used the default argument.
 * The date/time string specified as the argument is a left or
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:time (hh:mm:ss)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 */
static double
exsltDateHourInDay (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    double ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_TIME)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    ret = (double) dt->hour;
    exsltDateFreeDate(dt);

    return ret;
}

/**
 * exsltDateMinuteInHour:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times day-in-month() function:
 *    number date:day-in-month (string?)
 * Returns the minute of the hour as a number.  If no argument is
 * given, then the current local date/time, as returned by
 * date:date-time is used the default argument.
 * The date/time string specified as the argument is a left or
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:time (hh:mm:ss)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 */
static double
exsltDateMinuteInHour (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    double ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_TIME)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    ret = (double) dt->min;
    exsltDateFreeDate(dt);

    return ret;
}

/**
 * exsltDateSecondInMinute:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times second-in-minute() function:
 *    number date:day-in-month (string?)
 * Returns the second of the minute as a number.  If no argument is
 * given, then the current local date/time, as returned by
 * date:date-time is used the default argument.
 * The date/time string specified as the argument is a left or
 * right-truncated string in the format defined as the lexical
 * representation of xs:dateTime in one of the formats defined in [XML
 * Schema Part 2: Datatypes].  The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:time (hh:mm:ss)
 * If the date/time string is not in one of these formats, then NaN is
 * returned.
 *
 * Returns the second or NaN.
 */
static double
exsltDateSecondInMinute (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    double ret;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
	dt = exsltDateParse(dateTime);
	if (dt == NULL)
	    return xmlXPathNAN;
	if ((dt->type != XS_DATETIME) && (dt->type != XS_TIME)) {
	    exsltDateFreeDate(dt);
	    return xmlXPathNAN;
	}
    }

    ret = dt->sec;
    exsltDateFreeDate(dt);

    return ret;
}

/**
 * exsltDateAdd:
 * @xstr: date/time string
 * @ystr: date/time string
 *
 * Implements the date:add (string,string) function which returns the
 * date/time * resulting from adding a duration to a date/time.
 * The first argument (@xstr) must be right-truncated date/time
 * strings in one of the formats defined in [XML Schema Part 2:
 * Datatypes]. The permitted formats are as follows:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 *  - xs:gYearMonth (CCYY-MM)
 *  - xs:gYear (CCYY)
 * The second argument (@ystr) is a string in the format defined for
 * xs:duration in [3.2.6 duration] of [XML Schema Part 2: Datatypes].
 * The return value is a right-truncated date/time strings in one of
 * the formats defined in [XML Schema Part 2: Datatypes] and listed
 * above. This value is calculated using the algorithm described in
 * [Appendix E Adding durations to dateTimes] of [XML Schema Part 2:
 * Datatypes].

 * Returns date/time string or NULL.
 */
static xmlChar *
exsltDateAdd (const xmlChar *xstr, const xmlChar *ystr)
{
    exsltDateValPtr dt, res;
    exsltDateDurValPtr dur;
    xmlChar     *ret;

    if ((xstr == NULL) || (ystr == NULL))
        return NULL;

    dt = exsltDateParse(xstr);
    if (dt == NULL)
        return NULL;
    else if ((dt->type < XS_GYEAR) || (dt->type > XS_DATETIME)) {
        exsltDateFreeDate(dt);
        return NULL;
    }

    dur = exsltDateParseDuration(ystr);
    if (dur == NULL) {
        exsltDateFreeDate(dt);
        return NULL;
    }

    res = _exsltDateAdd(dt, dur);

    exsltDateFreeDate(dt);
    exsltDateFreeDuration(dur);

    if (res == NULL)
        return NULL;

    ret = exsltDateFormat(res);
    exsltDateFreeDate(res);

    return ret;
}

/**
 * exsltDateAddDuration:
 * @xstr:      first duration string
 * @ystr:      second duration string
 *
 * Implements the date:add-duration (string,string) function which returns
 * the duration resulting from adding two durations together.
 * Both arguments are strings in the format defined for xs:duration
 * in [3.2.6 duration] of [XML Schema Part 2: Datatypes]. If either
 * argument is not in this format, the function returns an empty string
 * ('').
 * The return value is a string in the format defined for xs:duration
 * in [3.2.6 duration] of [XML Schema Part 2: Datatypes].
 * The durations can usually be added by summing the numbers given for
 * each of the components in the durations. However, if the durations
 * are differently signed, then this sometimes results in durations
 * that are impossible to express in this syntax (e.g. 'P1M' + '-P1D').
 * In these cases, the function returns an empty string ('').
 *
 * Returns duration string or NULL.
 */
static xmlChar *
exsltDateAddDuration (const xmlChar *xstr, const xmlChar *ystr)
{
    exsltDateDurValPtr x, y, res;
    xmlChar     *ret;

    if ((xstr == NULL) || (ystr == NULL))
        return NULL;

    x = exsltDateParseDuration(xstr);
    if (x == NULL)
        return NULL;

    y = exsltDateParseDuration(ystr);
    if (y == NULL) {
        exsltDateFreeDuration(x);
        return NULL;
    }

    res = _exsltDateAddDuration(x, y);

    exsltDateFreeDuration(x);
    exsltDateFreeDuration(y);

    if (res == NULL)
        return NULL;

    ret = exsltDateFormatDuration(res);
    exsltDateFreeDuration(res);

    return ret;
}

/**
 * exsltDateSumFunction:
 * @ns:      a node set of duration strings
 *
 * The date:sum function adds a set of durations together.
 * The string values of the nodes in the node set passed as an argument
 * are interpreted as durations and added together as if using the
 * date:add-duration function. (from exslt.org)
 *
 * The return value is a string in the format defined for xs:duration
 * in [3.2.6 duration] of [XML Schema Part 2: Datatypes].
 * The durations can usually be added by summing the numbers given for
 * each of the components in the durations. However, if the durations
 * are differently signed, then this sometimes results in durations
 * that are impossible to express in this syntax (e.g. 'P1M' + '-P1D').
 * In these cases, the function returns an empty string ('').
 *
 * Returns duration string or NULL.
 */
static void
exsltDateSumFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlNodeSetPtr ns;
    void *user = NULL;
    xmlChar *tmp;
    exsltDateDurValPtr x, total;
    xmlChar *ret;
    int i;

    if (nargs != 1) {
	xmlXPathSetArityError (ctxt);
	return;
    }

    /* We need to delay the freeing of value->user */
    if ((ctxt->value != NULL) && ctxt->value->boolval != 0) {
	user = ctxt->value->user;
	ctxt->value->boolval = 0;
	ctxt->value->user = NULL;
    }

    ns = xmlXPathPopNodeSet (ctxt);
    if (xmlXPathCheckError (ctxt))
	return;

    if ((ns == NULL) || (ns->nodeNr == 0)) {
	xmlXPathReturnEmptyString (ctxt);
	if (ns != NULL)
	    xmlXPathFreeNodeSet (ns);
	return;
    }

    total = exsltDateCreateDuration ();
    if (total == NULL) {
        xmlXPathFreeNodeSet (ns);
        return;
    }

    for (i = 0; i < ns->nodeNr; i++) {
	int result;
	tmp = xmlXPathCastNodeToString (ns->nodeTab[i]);
	if (tmp == NULL) {
	    xmlXPathFreeNodeSet (ns);
	    exsltDateFreeDuration (total);
	    return;
	}

	x = exsltDateParseDuration (tmp);
	if (x == NULL) {
	    xmlFree (tmp);
	    exsltDateFreeDuration (total);
	    xmlXPathFreeNodeSet (ns);
	    xmlXPathReturnEmptyString (ctxt);
	    return;
	}

	result = _exsltDateAddDurCalc(total, total, x);

	exsltDateFreeDuration (x);
	xmlFree (tmp);
	if (!result) {
	    exsltDateFreeDuration (total);
	    xmlXPathFreeNodeSet (ns);
	    xmlXPathReturnEmptyString (ctxt);
	    return;
	}
    }

    ret = exsltDateFormatDuration (total);
    exsltDateFreeDuration (total);

    xmlXPathFreeNodeSet (ns);
    if (user != NULL)
	xmlFreeNodeList ((xmlNodePtr) user);

    if (ret == NULL)
	xmlXPathReturnEmptyString (ctxt);
    else
	xmlXPathReturnString (ctxt, ret);
}

/**
 * exsltDateSeconds:
 * @dateTime: a date/time string
 *
 * Implements the EXSLT - Dates and Times seconds() function:
 *    number date:seconds(string?)
 * The date:seconds function returns the number of seconds specified
 * by the argument string. If no argument is given, then the current
 * local date/time, as returned by exsltDateCurrent() is used as the
 * default argument. If the date/time string is a xs:duration, then the
 * years and months must be zero (or not present). Parsing a duration
 * converts the fields to seconds. If the date/time string is not a
 * duration (and not null), then the legal formats are:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date     (CCYY-MM-DD)
 *  - xs:gYearMonth (CCYY-MM)
 *  - xs:gYear      (CCYY)
 * In these cases the difference between the @dateTime and
 * 1970-01-01T00:00:00Z is calculated and converted to seconds.
 *
 * Note that there was some confusion over whether "difference" meant
 * that a dateTime of 1970-01-01T00:00:01Z should be a positive one or
 * a negative one.  After correspondence with exslt.org, it was determined
 * that the intent of the specification was to have it positive.  The
 * coding was modified in July 2003 to reflect this.
 *
 * Returns seconds or Nan.
 */
static double
exsltDateSeconds (const xmlChar *dateTime)
{
    exsltDateValPtr dt;
    exsltDateDurValPtr dur = NULL;
    double ret = xmlXPathNAN;

    if (dateTime == NULL) {
	dt = exsltDateCurrent();
	if (dt == NULL)
	    return xmlXPathNAN;
    } else {
        dt = exsltDateParse(dateTime);
        if (dt == NULL)
            dur = exsltDateParseDuration(dateTime);
    }

    if ((dt != NULL) && (dt->type >= XS_GYEAR)) {
        exsltDateValPtr y;
        exsltDateDurValPtr diff;

        /*
         * compute the difference between the given (or current) date
         * and epoch date
         */
        y = exsltDateCreateDate(XS_DATETIME);
        if (y != NULL) {
            y->year = 1970;
            y->mon  = 1;
            y->day  = 1;
            y->tz_flag = 1;

            diff = _exsltDateDifference(y, dt, 1);
            if (diff != NULL) {
                ret = (double)diff->day * SECS_PER_DAY + diff->sec;
                exsltDateFreeDuration(diff);
            }
            exsltDateFreeDate(y);
        }

    } else if ((dur != NULL) && (dur->mon == 0)) {
        ret = (double)dur->day * SECS_PER_DAY + dur->sec;
    }

    if (dt != NULL)
        exsltDateFreeDate(dt);
    if (dur != NULL)
        exsltDateFreeDuration(dur);

    return ret;
}

/**
 * exsltDateDifference:
 * @xstr: date/time string
 * @ystr: date/time string
 *
 * Implements the date:difference (string,string) function which returns
 * the duration between the first date and the second date. If the first
 * date occurs before the second date, then the result is a positive
 * duration; if it occurs after the second date, the result is a
 * negative duration.  The two dates must both be right-truncated
 * date/time strings in one of the formats defined in [XML Schema Part
 * 2: Datatypes]. The date/time with the most specific format (i.e. the
 * least truncation) is converted into the same format as the date with
 * the least specific format (i.e. the most truncation). The permitted
 * formats are as follows, from most specific to least specific:
 *  - xs:dateTime (CCYY-MM-DDThh:mm:ss)
 *  - xs:date (CCYY-MM-DD)
 *  - xs:gYearMonth (CCYY-MM)
 *  - xs:gYear (CCYY)
 * If either of the arguments is not in one of these formats,
 * date:difference returns the empty string ('').
 * The difference between the date/times is returned as a string in the
 * format defined for xs:duration in [3.2.6 duration] of [XML Schema
 * Part 2: Datatypes].
 * If the date/time string with the least specific format is in either
 * xs:gYearMonth or xs:gYear format, then the number of days, hours,
 * minutes and seconds in the duration string must be equal to zero.
 * (The format of the string will be PnYnM.) The number of months
 * specified in the duration must be less than 12.
 * Otherwise, the number of years and months in the duration string
 * must be equal to zero. (The format of the string will be
 * PnDTnHnMnS.) The number of seconds specified in the duration string
 * must be less than 60; the number of minutes must be less than 60;
 * the number of hours must be less than 24.
 *
 * Returns duration string or NULL.
 */
static xmlChar *
exsltDateDifference (const xmlChar *xstr, const xmlChar *ystr)
{
    exsltDateValPtr x, y;
    exsltDateDurValPtr dur;
    xmlChar *ret = NULL;

    if ((xstr == NULL) || (ystr == NULL))
        return NULL;

    x = exsltDateParse(xstr);
    if (x == NULL)
        return NULL;

    y = exsltDateParse(ystr);
    if (y == NULL) {
        exsltDateFreeDate(x);
        return NULL;
    }

    if (((x->type < XS_GYEAR) || (x->type > XS_DATETIME)) ||
        ((y->type < XS_GYEAR) || (y->type > XS_DATETIME)))  {
	exsltDateFreeDate(x);
	exsltDateFreeDate(y);
        return NULL;
    }

    dur = _exsltDateDifference(x, y, 0);

    exsltDateFreeDate(x);
    exsltDateFreeDate(y);

    if (dur == NULL)
        return NULL;

    ret = exsltDateFormatDuration(dur);
    exsltDateFreeDuration(dur);

    return ret;
}

/**
 * exsltDateDuration:
 * @number: a xmlChar string
 *
 * Implements the The date:duration function returns a duration string
 * representing the number of seconds specified by the argument string.
 * If no argument is given, then the result of calling date:seconds
 * without any arguments is used as a default argument.
 * The duration is returned as a string in the format defined for
 * xs:duration in [3.2.6 duration] of [XML Schema Part 2: Datatypes].
 * The number of years and months in the duration string must be equal
 * to zero. (The format of the string will be PnDTnHnMnS.) The number
 * of seconds specified in the duration string must be less than 60;
 * the number of minutes must be less than 60; the number of hours must
 * be less than 24.
 * If the argument is Infinity, -Infinity or NaN, then date:duration
 * returns an empty string ('').
 *
 * Returns duration string or NULL.
 */
static xmlChar *
exsltDateDuration (const xmlChar *number)
{
    exsltDateDurValPtr dur;
    double       secs, days;
    xmlChar     *ret;

    if (number == NULL)
        secs = exsltDateSeconds(number);
    else
        secs = xmlXPathCastStringToNumber(number);

    if (xmlXPathIsNaN(secs))
        return NULL;

    days = floor(secs / SECS_PER_DAY);
    if ((days <= (double)LONG_MIN) || (days >= (double)LONG_MAX))
        return NULL;

    dur = exsltDateCreateDuration();
    if (dur == NULL)
        return NULL;

    dur->day = (long)days;
    dur->sec = secs - days * SECS_PER_DAY;

    ret = exsltDateFormatDuration(dur);
    exsltDateFreeDuration(dur);

    return ret;
}

/****************************************************************
 *								*
 *		Wrappers for use by the XPath engine		*
 *								*
 ****************************************************************/

/**
 * exsltDateDateTimeFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateDateTime() for use by the XPath engine.
 */
static void
exsltDateDateTimeFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *ret;

    if (nargs != 0) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    ret = exsltDateDateTime();
    if (ret == NULL)
        xmlXPathReturnEmptyString(ctxt);
    else
        xmlXPathReturnString(ctxt, ret);
}

/**
 * exsltDateDateFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateDate() for use by the XPath engine.
 */
static void
exsltDateDateFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *ret, *dt = NULL;

    if ((nargs < 0) || (nargs > 1)) {
	xmlXPathSetArityError(ctxt);
	return;
    }
    if (nargs == 1) {
	dt = xmlXPathPopString(ctxt);
	if (xmlXPathCheckError(ctxt)) {
	    xmlXPathSetTypeError(ctxt);
	    return;
	}
    }

    ret = exsltDateDate(dt);

    if (ret == NULL) {
	xsltGenericDebug(xsltGenericDebugContext,
			 "{http://exslt.org/dates-and-times}date: "
			 "invalid date or format %s\n", dt);
	xmlXPathReturnEmptyString(ctxt);
    } else {
	xmlXPathReturnString(ctxt, ret);
    }

    if (dt != NULL)
	xmlFree(dt);
}

/**
 * exsltDateTimeFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateTime() for use by the XPath engine.
 */
static void
exsltDateTimeFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *ret, *dt = NULL;

    if ((nargs < 0) || (nargs > 1)) {
	xmlXPathSetArityError(ctxt);
	return;
    }
    if (nargs == 1) {
	dt = xmlXPathPopString(ctxt);
	if (xmlXPathCheckError(ctxt)) {
	    xmlXPathSetTypeError(ctxt);
	    return;
	}
    }

    ret = exsltDateTime(dt);

    if (ret == NULL) {
	xsltGenericDebug(xsltGenericDebugContext,
			 "{http://exslt.org/dates-and-times}time: "
			 "invalid date or format %s\n", dt);
	xmlXPathReturnEmptyString(ctxt);
    } else {
	xmlXPathReturnString(ctxt, ret);
    }

    if (dt != NULL)
	xmlFree(dt);
}

/**
 * exsltDateYearFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateYear() for use by the XPath engine.
 */
static void
exsltDateYearFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *dt = NULL;
    double ret;

    if ((nargs < 0) || (nargs > 1)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs == 1) {
	dt = xmlXPathPopString(ctxt);
	if (xmlXPathCheckError(ctxt)) {
	    xmlXPathSetTypeError(ctxt);
	    return;
	}
    }

    ret = exsltDateYear(dt);

    if (dt != NULL)
	xmlFree(dt);

    xmlXPathReturnNumber(ctxt, ret);
}

/**
 * exsltDateLeapYearFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateLeapYear() for use by the XPath engine.
 */
static void
exsltDateLeapYearFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *dt = NULL;
    xmlXPathObjectPtr ret;

    if ((nargs < 0) || (nargs > 1)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs == 1) {
	dt = xmlXPathPopString(ctxt);
	if (xmlXPathCheckError(ctxt)) {
	    xmlXPathSetTypeError(ctxt);
	    return;
	}
    }

    ret = exsltDateLeapYear(dt);

    if (dt != NULL)
	xmlFree(dt);

    valuePush(ctxt, ret);
}

#define X_IN_Y(x, y)						\
static void							\
exsltDate##x##In##y##Function (xmlXPathParserContextPtr ctxt,	\
			      int nargs) {			\
    xmlChar *dt = NULL;						\
    double ret;							\
								\
    if ((nargs < 0) || (nargs > 1)) {				\
	xmlXPathSetArityError(ctxt);				\
	return;							\
    }								\
								\
    if (nargs == 1) {						\
	dt = xmlXPathPopString(ctxt);				\
	if (xmlXPathCheckError(ctxt)) {				\
	    xmlXPathSetTypeError(ctxt);				\
	    return;						\
	}							\
    }								\
								\
    ret = exsltDate##x##In##y(dt);				\
								\
    if (dt != NULL)						\
	xmlFree(dt);						\
								\
    xmlXPathReturnNumber(ctxt, ret);				\
}

/**
 * exsltDateMonthInYearFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateMonthInYear() for use by the XPath engine.
 */
X_IN_Y(Month,Year)

/**
 * exsltDateMonthNameFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateMonthName() for use by the XPath engine.
 */
static void
exsltDateMonthNameFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *dt = NULL;
    const xmlChar *ret;

    if ((nargs < 0) || (nargs > 1)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs == 1) {
	dt = xmlXPathPopString(ctxt);
	if (xmlXPathCheckError(ctxt)) {
	    xmlXPathSetTypeError(ctxt);
	    return;
	}
    }

    ret = exsltDateMonthName(dt);

    if (dt != NULL)
	xmlFree(dt);

    if (ret == NULL)
	xmlXPathReturnEmptyString(ctxt);
    else
	xmlXPathReturnString(ctxt, xmlStrdup(ret));
}

/**
 * exsltDateMonthAbbreviationFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateMonthAbbreviation() for use by the XPath engine.
 */
static void
exsltDateMonthAbbreviationFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *dt = NULL;
    const xmlChar *ret;

    if ((nargs < 0) || (nargs > 1)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs == 1) {
	dt = xmlXPathPopString(ctxt);
	if (xmlXPathCheckError(ctxt)) {
	    xmlXPathSetTypeError(ctxt);
	    return;
	}
    }

    ret = exsltDateMonthAbbreviation(dt);

    if (dt != NULL)
	xmlFree(dt);

    if (ret == NULL)
	xmlXPathReturnEmptyString(ctxt);
    else
	xmlXPathReturnString(ctxt, xmlStrdup(ret));
}

/**
 * exsltDateWeekInYearFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateWeekInYear() for use by the XPath engine.
 */
X_IN_Y(Week,Year)

/**
 * exsltDateWeekInMonthFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateWeekInMonthYear() for use by the XPath engine.
 */
X_IN_Y(Week,Month)

/**
 * exsltDateDayInYearFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateDayInYear() for use by the XPath engine.
 */
X_IN_Y(Day,Year)

/**
 * exsltDateDayInMonthFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateDayInMonth() for use by the XPath engine.
 */
X_IN_Y(Day,Month)

/**
 * exsltDateDayOfWeekInMonthFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDayOfWeekInMonth() for use by the XPath engine.
 */
X_IN_Y(DayOfWeek,Month)

/**
 * exsltDateDayInWeekFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateDayInWeek() for use by the XPath engine.
 */
X_IN_Y(Day,Week)

/**
 * exsltDateDayNameFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateDayName() for use by the XPath engine.
 */
static void
exsltDateDayNameFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *dt = NULL;
    const xmlChar *ret;

    if ((nargs < 0) || (nargs > 1)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs == 1) {
	dt = xmlXPathPopString(ctxt);
	if (xmlXPathCheckError(ctxt)) {
	    xmlXPathSetTypeError(ctxt);
	    return;
	}
    }

    ret = exsltDateDayName(dt);

    if (dt != NULL)
	xmlFree(dt);

    if (ret == NULL)
	xmlXPathReturnEmptyString(ctxt);
    else
	xmlXPathReturnString(ctxt, xmlStrdup(ret));
}

/**
 * exsltDateMonthDayFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateDayAbbreviation() for use by the XPath engine.
 */
static void
exsltDateDayAbbreviationFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *dt = NULL;
    const xmlChar *ret;

    if ((nargs < 0) || (nargs > 1)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs == 1) {
	dt = xmlXPathPopString(ctxt);
	if (xmlXPathCheckError(ctxt)) {
	    xmlXPathSetTypeError(ctxt);
	    return;
	}
    }

    ret = exsltDateDayAbbreviation(dt);

    if (dt != NULL)
	xmlFree(dt);

    if (ret == NULL)
	xmlXPathReturnEmptyString(ctxt);
    else
	xmlXPathReturnString(ctxt, xmlStrdup(ret));
}


/**
 * exsltDateHourInDayFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateHourInDay() for use by the XPath engine.
 */
X_IN_Y(Hour,Day)

/**
 * exsltDateMinuteInHourFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateMinuteInHour() for use by the XPath engine.
 */
X_IN_Y(Minute,Hour)

/**
 * exsltDateSecondInMinuteFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateSecondInMinute() for use by the XPath engine.
 */
X_IN_Y(Second,Minute)

/**
 * exsltDateSecondsFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateSeconds() for use by the XPath engine.
 */
static void
exsltDateSecondsFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *str = NULL;
    double   ret;

    if (nargs > 1) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs == 1) {
	str = xmlXPathPopString(ctxt);
	if (xmlXPathCheckError(ctxt)) {
	    xmlXPathSetTypeError(ctxt);
	    return;
	}
    }

    ret = exsltDateSeconds(str);
    if (str != NULL)
	xmlFree(str);

    xmlXPathReturnNumber(ctxt, ret);
}

/**
 * exsltDateAddFunction:
 * @ctxt:  an XPath parser context
 * @nargs:  the number of arguments
 *
 * Wraps exsltDateAdd() for use by the XPath processor.
 */
static void
exsltDateAddFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *ret, *xstr, *ystr;

    if (nargs != 2) {
	xmlXPathSetArityError(ctxt);
	return;
    }
    ystr = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt))
	return;

    xstr = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt)) {
        xmlFree(ystr);
	return;
    }

    ret = exsltDateAdd(xstr, ystr);

    xmlFree(ystr);
    xmlFree(xstr);

    if (ret == NULL)
        xmlXPathReturnEmptyString(ctxt);
    else
	xmlXPathReturnString(ctxt, ret);
}

/**
 * exsltDateAddDurationFunction:
 * @ctxt:  an XPath parser context
 * @nargs:  the number of arguments
 *
 * Wraps exsltDateAddDuration() for use by the XPath processor.
 */
static void
exsltDateAddDurationFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *ret, *xstr, *ystr;

    if (nargs != 2) {
	xmlXPathSetArityError(ctxt);
	return;
    }
    ystr = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt))
	return;

    xstr = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt)) {
        xmlFree(ystr);
	return;
    }

    ret = exsltDateAddDuration(xstr, ystr);

    xmlFree(ystr);
    xmlFree(xstr);

    if (ret == NULL)
        xmlXPathReturnEmptyString(ctxt);
    else
	xmlXPathReturnString(ctxt, ret);
}

/**
 * exsltDateDifferenceFunction:
 * @ctxt:  an XPath parser context
 * @nargs:  the number of arguments
 *
 * Wraps exsltDateDifference() for use by the XPath processor.
 */
static void
exsltDateDifferenceFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *ret, *xstr, *ystr;

    if (nargs != 2) {
	xmlXPathSetArityError(ctxt);
	return;
    }
    ystr = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt))
	return;

    xstr = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt)) {
        xmlFree(ystr);
	return;
    }

    ret = exsltDateDifference(xstr, ystr);

    xmlFree(ystr);
    xmlFree(xstr);

    if (ret == NULL)
        xmlXPathReturnEmptyString(ctxt);
    else
	xmlXPathReturnString(ctxt, ret);
}

/**
 * exsltDateDurationFunction:
 * @ctxt: an XPath parser context
 * @nargs : the number of arguments
 *
 * Wraps exsltDateDuration() for use by the XPath engine
 */
static void
exsltDateDurationFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlChar *ret;
    xmlChar *number = NULL;

    if ((nargs < 0) || (nargs > 1)) {
	xmlXPathSetArityError(ctxt);
	return;
    }

    if (nargs == 1) {
	number = xmlXPathPopString(ctxt);
	if (xmlXPathCheckError(ctxt)) {
	    xmlXPathSetTypeError(ctxt);
	    return;
	}
    }

    ret = exsltDateDuration(number);

    if (number != NULL)
	xmlFree(number);

    if (ret == NULL)
	xmlXPathReturnEmptyString(ctxt);
    else
	xmlXPathReturnString(ctxt, ret);
}

/**
 * exsltDateRegister:
 *
 * Registers the EXSLT - Dates and Times module
 */
void
exsltDateRegister (void)
{
    xsltRegisterExtModuleFunction ((const xmlChar *) "add",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateAddFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "add-duration",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateAddDurationFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "date",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateDateFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "date-time",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateDateTimeFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "day-abbreviation",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateDayAbbreviationFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "day-in-month",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateDayInMonthFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "day-in-week",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateDayInWeekFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "day-in-year",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateDayInYearFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "day-name",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateDayNameFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "day-of-week-in-month",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateDayOfWeekInMonthFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "difference",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateDifferenceFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "duration",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateDurationFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "hour-in-day",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateHourInDayFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "leap-year",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateLeapYearFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "minute-in-hour",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateMinuteInHourFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "month-abbreviation",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateMonthAbbreviationFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "month-in-year",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateMonthInYearFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "month-name",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateMonthNameFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "second-in-minute",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateSecondInMinuteFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "seconds",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateSecondsFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "sum",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateSumFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "time",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateTimeFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "week-in-month",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateWeekInMonthFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "week-in-year",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateWeekInYearFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "year",
				   (const xmlChar *) EXSLT_DATE_NAMESPACE,
				   exsltDateYearFunction);
}

/**
 * exsltDateXpathCtxtRegister:
 *
 * Registers the EXSLT - Dates and Times module for use outside XSLT
 */
int
exsltDateXpathCtxtRegister (xmlXPathContextPtr ctxt, const xmlChar *prefix)
{
    if (ctxt
        && prefix
        && !xmlXPathRegisterNs(ctxt,
                               prefix,
                               (const xmlChar *) EXSLT_DATE_NAMESPACE)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "add",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateAddFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "add-duration",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateAddDurationFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "date",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateDateFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "date-time",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateDateTimeFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "day-abbreviation",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateDayAbbreviationFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "day-in-month",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateDayInMonthFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "day-in-week",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateDayInWeekFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "day-in-year",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateDayInYearFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "day-name",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateDayNameFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "day-of-week-in-month",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateDayOfWeekInMonthFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "difference",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateDifferenceFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "duration",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateDurationFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "hour-in-day",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateHourInDayFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "leap-year",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateLeapYearFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "minute-in-hour",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateMinuteInHourFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "month-abbreviation",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateMonthAbbreviationFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "month-in-year",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateMonthInYearFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "month-name",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateMonthNameFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "second-in-minute",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateSecondInMinuteFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "seconds",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateSecondsFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "sum",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateSumFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "time",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateTimeFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "week-in-month",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateWeekInMonthFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "week-in-year",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateWeekInYearFunction)
        && !xmlXPathRegisterFuncNS(ctxt,
                                   (const xmlChar *) "year",
                                   (const xmlChar *) EXSLT_DATE_NAMESPACE,
                                   exsltDateYearFunction)) {
        return 0;
    }
    return -1;
}
