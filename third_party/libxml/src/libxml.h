/*
 * libxml.h: internal header only used during the compilation of libxml
 *
 * See COPYRIGHT for the status of this software
 *
 * Author: breese@users.sourceforge.net
 */

#ifndef __XML_LIBXML_H__
#define __XML_LIBXML_H__

#include <libxml/xmlstring.h>

#ifndef NO_LARGEFILE_SOURCE
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#endif

#if defined(macintosh)
#include "config-mac.h"
#elif defined(_WIN32_WCE)
/*
 * Windows CE compatibility definitions and functions
 * This is needed to compile libxml2 for Windows CE.
 * At least I tested it with WinCE 5.0 for Emulator and WinCE 4.2/SH4 target
 */
#include <win32config.h>
#include <libxml/xmlversion.h>
#else
/*
 * Currently supported platforms use either autoconf or
 * copy to config.h own "preset" configuration file.
 * As result ifdef HAVE_CONFIG_H is omitted here.
 */
#include "config.h"
#include <libxml/xmlversion.h>
#endif

#if defined(__Lynx__)
#include <stdio.h> /* pull definition of size_t */
#include <varargs.h>
int snprintf(char *, size_t, const char *, ...);
int vfprintf(FILE *, const char *, va_list);
#endif

#ifndef WITH_TRIO
#include <stdio.h>
#else
/**
 * TRIO_REPLACE_STDIO:
 *
 * This macro is defined if the trio string formatting functions are to
 * be used instead of the default stdio ones.
 */
#define TRIO_REPLACE_STDIO
#include "trio.h"
#endif

#if defined(__clang__) || \
    (defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 406))
#define XML_IGNORE_PEDANTIC_WARNINGS \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wpedantic\"")
#define XML_POP_WARNINGS \
    _Pragma("GCC diagnostic pop")
#else
#define XML_IGNORE_PEDANTIC_WARNINGS
#define XML_POP_WARNINGS
#endif

#if defined(__clang__) || \
    (defined(__GNUC__) && (__GNUC__ >= 8))
#define ATTRIBUTE_NO_SANITIZE(arg) __attribute__((no_sanitize(arg)))
#else
#define ATTRIBUTE_NO_SANITIZE(arg)
#endif

/*
 * Internal variable indicating if a callback has been registered for
 * node creation/destruction. It avoids spending a lot of time in locking
 * function while checking if the callback exists.
 */
extern int __xmlRegisterCallbacks;
/*
 * internal error reporting routines, shared but not part of the API.
 */
void __xmlIOErr(int domain, int code, const char *extra);
void __xmlLoaderErr(void *ctx, const char *msg, const char *filename) LIBXML_ATTR_FORMAT(2,0);
#ifdef LIBXML_HTML_ENABLED
/*
 * internal function of HTML parser needed for xmlParseInNodeContext
 * but not part of the API
 */
void __htmlParseContent(void *ctx);
#endif

/*
 * internal global initialization critical section routines.
 */
void __xmlGlobalInitMutexLock(void);
void __xmlGlobalInitMutexUnlock(void);
void __xmlGlobalInitMutexDestroy(void);

int __xmlInitializeDict(void);

#if defined(HAVE_RAND) && defined(HAVE_SRAND) && defined(HAVE_TIME)
/*
 * internal thread safe random function
 */
int __xmlRandom(void);
#endif

XMLPUBFUN xmlChar * XMLCALL xmlEscapeFormatString(xmlChar **msg);
int xmlInputReadCallbackNop(void *context, char *buffer, int len);

#ifdef IN_LIBXML
#ifdef __GNUC__
#ifdef PIC
#ifdef __linux__
#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || (__GNUC__ > 3)
#include "elfgcchack.h"
#endif
#endif
#endif
#endif
#endif
#if !defined(PIC) && !defined(NOLIBTOOL) && !defined(LIBXML_STATIC)
#  define LIBXML_STATIC
#endif
#endif /* ! __XML_LIBXML_H__ */
