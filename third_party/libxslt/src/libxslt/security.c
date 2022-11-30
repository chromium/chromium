/*
 * security.c: Implementation of the XSLT security framework
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXSLT
#include "libxslt.h"

#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif
#endif

#ifndef HAVE_STAT
#  ifdef HAVE__STAT
     /* MS C library seems to define stat and _stat. The definition
      *         is identical. Still, mapping them to each other causes a warning. */
#    ifndef _MSC_VER
#      define stat(x,y) _stat(x,y)
#    endif
#    define HAVE_STAT
#  endif
#endif

#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "extensions.h"
#include "security.h"


struct _xsltSecurityPrefs {
    xsltSecurityCheck readFile;
    xsltSecurityCheck createFile;
    xsltSecurityCheck createDir;
    xsltSecurityCheck readNet;
    xsltSecurityCheck writeNet;
};

static xsltSecurityPrefsPtr xsltDefaultSecurityPrefs = NULL;

/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/

/**
 * xsltNewSecurityPrefs:
 *
 * Create a new security preference block
 *
 * Returns a pointer to the new block or NULL in case of error
 */
xsltSecurityPrefsPtr
xsltNewSecurityPrefs(void) {
    xsltSecurityPrefsPtr ret;

    xsltInitGlobals();

    ret = (xsltSecurityPrefsPtr) xmlMalloc(sizeof(xsltSecurityPrefs));
    if (ret == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltNewSecurityPrefs : malloc failed\n");
	return(NULL);
    }
    memset(ret, 0, sizeof(xsltSecurityPrefs));
    return(ret);
}

/**
 * xsltFreeSecurityPrefs:
 * @sec:  the security block to free
 *
 * Free up a security preference block
 */
void
xsltFreeSecurityPrefs(xsltSecurityPrefsPtr sec) {
    if (sec == NULL)
	return;
    xmlFree(sec);
}

/**
 * xsltSetSecurityPrefs:
 * @sec:  the security block to update
 * @option:  the option to update
 * @func:  the user callback to use for this option
 *
 * Update the security option to use the new callback checking function
 *
 * Returns -1 in case of error, 0 otherwise
 */
int
xsltSetSecurityPrefs(xsltSecurityPrefsPtr sec, xsltSecurityOption option,
                     xsltSecurityCheck func) {
    xsltInitGlobals();
    if (sec == NULL)
	return(-1);
    switch (option) {
        case XSLT_SECPREF_READ_FILE:
            sec->readFile = func; return(0);
        case XSLT_SECPREF_WRITE_FILE:
            sec->createFile = func; return(0);
        case XSLT_SECPREF_CREATE_DIRECTORY:
            sec->createDir = func; return(0);
        case XSLT_SECPREF_READ_NETWORK:
            sec->readNet = func; return(0);
        case XSLT_SECPREF_WRITE_NETWORK:
            sec->writeNet = func; return(0);
    }
    return(-1);
}

/**
 * xsltGetSecurityPrefs:
 * @sec:  the security block to update
 * @option:  the option to lookup
 *
 * Lookup the security option to get the callback checking function
 *
 * Returns NULL if not found, the function otherwise
 */
xsltSecurityCheck
xsltGetSecurityPrefs(xsltSecurityPrefsPtr sec, xsltSecurityOption option) {
    if (sec == NULL)
	return(NULL);
    switch (option) {
        case XSLT_SECPREF_READ_FILE:
            return(sec->readFile);
        case XSLT_SECPREF_WRITE_FILE:
            return(sec->createFile);
        case XSLT_SECPREF_CREATE_DIRECTORY:
            return(sec->createDir);
        case XSLT_SECPREF_READ_NETWORK:
            return(sec->readNet);
        case XSLT_SECPREF_WRITE_NETWORK:
            return(sec->writeNet);
    }
    return(NULL);
}

/**
 * xsltSetDefaultSecurityPrefs:
 * @sec:  the security block to use
 *
 * Set the default security preference application-wide
 */
void
xsltSetDefaultSecurityPrefs(xsltSecurityPrefsPtr sec) {

    xsltDefaultSecurityPrefs = sec;
}

/**
 * xsltGetDefaultSecurityPrefs:
 *
 * Get the default security preference application-wide
 *
 * Returns the current xsltSecurityPrefsPtr in use or NULL if none
 */
xsltSecurityPrefsPtr
xsltGetDefaultSecurityPrefs(void) {
    return(xsltDefaultSecurityPrefs);
}

/**
 * xsltSetCtxtSecurityPrefs:
 * @sec:  the security block to use
 * @ctxt:  an XSLT transformation context
 *
 * Set the security preference for a specific transformation
 *
 * Returns -1 in case of error, 0 otherwise
 */
int
xsltSetCtxtSecurityPrefs(xsltSecurityPrefsPtr sec,
	                 xsltTransformContextPtr ctxt) {
    if (ctxt == NULL)
	return(-1);
    ctxt->sec = (void *) sec;
    return(0);
}


/**
 * xsltSecurityAllow:
 * @sec:  the security block to use
 * @ctxt:  an XSLT transformation context
 * @value:  unused
 *
 * Function used to always allow an operation
 *
 * Returns 1 always
 */
int
xsltSecurityAllow(xsltSecurityPrefsPtr sec ATTRIBUTE_UNUSED,
	          xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED,
		  const char *value ATTRIBUTE_UNUSED) {
    return(1);
}

/**
 * xsltSecurityForbid:
 * @sec:  the security block to use
 * @ctxt:  an XSLT transformation context
 * @value:  unused
 *
 * Function used to always forbid an operation
 *
 * Returns 0 always
 */
int
xsltSecurityForbid(xsltSecurityPrefsPtr sec ATTRIBUTE_UNUSED,
	          xsltTransformContextPtr ctxt ATTRIBUTE_UNUSED,
		  const char *value ATTRIBUTE_UNUSED) {
    return(0);
}

/************************************************************************
 *									*
 *			Internal interfaces				*
 *									*
 ************************************************************************/

/**
 * xsltCheckFilename
 * @path:  the path to check
 *
 * function checks to see if @path is a valid source
 * (file, socket...) for XML.
 *
 * TODO: remove at some point !!!
 * Local copy of xmlCheckFilename to avoid a hard dependency on
 * a new version of libxml2
 *
 * if stat is not available on the target machine,
 * returns 1.  if stat fails, returns 0 (if calling
 * stat on the filename fails, it can't be right).
 * if stat succeeds and the file is a directory,
 * returns 2.  otherwise returns 1.
 */

static int
xsltCheckFilename (const char *path)
{
#ifdef HAVE_STAT
    struct stat stat_buffer;
#if defined(_WIN32)
    DWORD dwAttrs;

    dwAttrs = GetFileAttributesA(path);
    if (dwAttrs != INVALID_FILE_ATTRIBUTES) {
        if (dwAttrs & FILE_ATTRIBUTE_DIRECTORY) {
            return 2;
		}
    }
#endif

    if (stat(path, &stat_buffer) == -1)
        return 0;

#ifdef S_ISDIR
    if (S_ISDIR(stat_buffer.st_mode)) {
        return 2;
    }
#endif
#endif
    return 1;
}

static int
xsltCheckWritePath(xsltSecurityPrefsPtr sec,
		   xsltTransformContextPtr ctxt,
		   const char *path)
{
    int ret;
    xsltSecurityCheck check;
    char *directory;

    check = xsltGetSecurityPrefs(sec, XSLT_SECPREF_WRITE_FILE);
    if (check != NULL) {
	ret = check(sec, ctxt, path);
	if (ret == 0) {
	    xsltTransformError(ctxt, NULL, NULL,
			       "File write for %s refused\n", path);
	    return(0);
	}
    }

    directory = xmlParserGetDirectory (path);

    if (directory != NULL) {
	ret = xsltCheckFilename(directory);
	if (ret == 0) {
	    /*
	     * The directory doesn't exist check for creation
	     */
	    check = xsltGetSecurityPrefs(sec,
					 XSLT_SECPREF_CREATE_DIRECTORY);
	    if (check != NULL) {
		ret = check(sec, ctxt, directory);
		if (ret == 0) {
		    xsltTransformError(ctxt, NULL, NULL,
				       "Directory creation for %s refused\n",
				       path);
		    xmlFree(directory);
		    return(0);
		}
	    }
	    ret = xsltCheckWritePath(sec, ctxt, directory);
	    if (ret == 1)
		ret = mkdir(directory, 0755);
	}
	xmlFree(directory);
	if (ret < 0)
	    return(ret);
    }

    return(1);
}

/**
 * xsltCheckWrite:
 * @sec:  the security options
 * @ctxt:  an XSLT transformation context
 * @URL:  the resource to be written
 *
 * Check if the resource is allowed to be written, if necessary makes
 * some preliminary work like creating directories
 *
 * Return 1 if write is allowed, 0 if not and -1 in case or error.
 */
int
xsltCheckWrite(xsltSecurityPrefsPtr sec,
	       xsltTransformContextPtr ctxt, const xmlChar *URL) {
    int ret;
    xmlURIPtr uri;
    xsltSecurityCheck check;

    uri = xmlParseURI((const char *)URL);
    if (uri == NULL) {
        uri = xmlCreateURI();
	if (uri == NULL) {
	    xsltTransformError(ctxt, NULL, NULL,
	     "xsltCheckWrite: out of memory for %s\n", URL);
	    return(-1);
	}
	uri->path = (char *)xmlStrdup(URL);
    }
    if ((uri->scheme == NULL) ||
	(xmlStrEqual(BAD_CAST uri->scheme, BAD_CAST "file"))) {

#if defined(_WIN32)
        if ((uri->path)&&(uri->path[0]=='/')&&
            (uri->path[1]!='\0')&&(uri->path[2]==':'))
            ret = xsltCheckWritePath(sec, ctxt, uri->path+1);
        else
#endif
        {
            /*
             * Check if we are allowed to write this file
             */
	    ret = xsltCheckWritePath(sec, ctxt, uri->path);
        }

	if (ret <= 0) {
	    xmlFreeURI(uri);
	    return(ret);
	}
    } else {
	/*
	 * Check if we are allowed to write this network resource
	 */
	check = xsltGetSecurityPrefs(sec, XSLT_SECPREF_WRITE_NETWORK);
	if (check != NULL) {
	    ret = check(sec, ctxt, (const char *)URL);
	    if (ret == 0) {
		xsltTransformError(ctxt, NULL, NULL,
			     "File write for %s refused\n", URL);
		xmlFreeURI(uri);
		return(0);
	    }
	}
    }
    xmlFreeURI(uri);
    return(1);
}


/**
 * xsltCheckRead:
 * @sec:  the security options
 * @ctxt: an XSLT transformation context
 * @URL:  the resource to be read
 *
 * Check if the resource is allowed to be read
 *
 * Return 1 if read is allowed, 0 if not and -1 in case or error.
 */
int
xsltCheckRead(xsltSecurityPrefsPtr sec,
	      xsltTransformContextPtr ctxt, const xmlChar *URL) {
    int ret;
    xmlURIPtr uri;
    xsltSecurityCheck check;

    uri = xmlParseURI((const char *)URL);
    if (uri == NULL) {
	xsltTransformError(ctxt, NULL, NULL,
	 "xsltCheckRead: URL parsing failed for %s\n",
			 URL);
	return(-1);
    }
    if ((uri->scheme == NULL) ||
	(xmlStrEqual(BAD_CAST uri->scheme, BAD_CAST "file"))) {

	/*
	 * Check if we are allowed to read this file
	 */
	check = xsltGetSecurityPrefs(sec, XSLT_SECPREF_READ_FILE);
	if (check != NULL) {
	    ret = check(sec, ctxt, uri->path);
	    if (ret == 0) {
		xsltTransformError(ctxt, NULL, NULL,
			     "Local file read for %s refused\n", URL);
		xmlFreeURI(uri);
		return(0);
	    }
	}
    } else {
	/*
	 * Check if we are allowed to write this network resource
	 */
	check = xsltGetSecurityPrefs(sec, XSLT_SECPREF_READ_NETWORK);
	if (check != NULL) {
	    ret = check(sec, ctxt, (const char *)URL);
	    if (ret == 0) {
		xsltTransformError(ctxt, NULL, NULL,
			     "Network file read for %s refused\n", URL);
		xmlFreeURI(uri);
		return(0);
	    }
	}
    }
    xmlFreeURI(uri);
    return(1);
}

