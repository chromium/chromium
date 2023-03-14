/*
 * buf.c: memory buffers for libxml2
 *
 * new buffer structures and entry points to simplify the maintenance
 * of libxml2 and ensure we keep good control over memory allocations
 * and stay 64 bits clean.
 * The new entry point use the xmlBufPtr opaque structure and
 * xmlBuf...() counterparts to the old xmlBuf...() functions
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXML
#include "libxml.h"

#include <string.h> /* for memset() only ! */
#include <limits.h>
#include <ctype.h>
#include <stdlib.h>

#include <libxml/tree.h>
#include <libxml/globals.h>
#include <libxml/tree.h>
#include <libxml/parserInternals.h> /* for XML_MAX_TEXT_LENGTH */

#include "private/buf.h"
#include "private/error.h"

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t) -1)
#endif

#define WITH_BUFFER_COMPAT

/**
 * xmlBuf:
 *
 * A buffer structure. The base of the structure is somehow compatible
 * with struct _xmlBuffer to limit risks on application which accessed
 * directly the input->buf->buffer structures.
 */

struct _xmlBuf {
    xmlChar *content;		/* The buffer content UTF8 */
    unsigned int compat_use;    /* for binary compatibility */
    unsigned int compat_size;   /* for binary compatibility */
    xmlBufferAllocationScheme alloc; /* The realloc method */
    xmlChar *contentIO;		/* in IO mode we may have a different base */
    size_t use;		        /* The buffer size used */
    size_t size;		/* The buffer size */
    xmlBufferPtr buffer;        /* wrapper for an old buffer */
    int error;                  /* an error code if a failure occurred */
};

#ifdef WITH_BUFFER_COMPAT
/*
 * Macro for compatibility with xmlBuffer to be used after an xmlBuf
 * is updated. This makes sure the compat fields are updated too.
 */
#define UPDATE_COMPAT(buf)				    \
     if (buf->size < INT_MAX) buf->compat_size = buf->size; \
     else buf->compat_size = INT_MAX;			    \
     if (buf->use < INT_MAX) buf->compat_use = buf->use; \
     else buf->compat_use = INT_MAX;

/*
 * Macro for compatibility with xmlBuffer to be used in all the xmlBuf
 * entry points, it checks that the compat fields have not been modified
 * by direct call to xmlBuffer function from code compiled before 2.9.0 .
 */
#define CHECK_COMPAT(buf)				    \
     if (buf->size != (size_t) buf->compat_size)	    \
         if (buf->compat_size < INT_MAX)		    \
	     buf->size = buf->compat_size;		    \
     if (buf->use != (size_t) buf->compat_use)		    \
         if (buf->compat_use < INT_MAX)			    \
	     buf->use = buf->compat_use;

#else /* ! WITH_BUFFER_COMPAT */
#define UPDATE_COMPAT(buf)
#define CHECK_COMPAT(buf)
#endif /* WITH_BUFFER_COMPAT */

/**
 * xmlBufMemoryError:
 * @extra:  extra information
 *
 * Handle an out of memory condition
 * To be improved...
 */
static void
xmlBufMemoryError(xmlBufPtr buf, const char *extra)
{
    __xmlSimpleError(XML_FROM_BUFFER, XML_ERR_NO_MEMORY, NULL, NULL, extra);
    if ((buf) && (buf->error == 0))
        buf->error = XML_ERR_NO_MEMORY;
}

/**
 * xmlBufOverflowError:
 * @extra:  extra information
 *
 * Handle a buffer overflow error
 * To be improved...
 */
static void
xmlBufOverflowError(xmlBufPtr buf, const char *extra)
{
    __xmlSimpleError(XML_FROM_BUFFER, XML_BUF_OVERFLOW, NULL, NULL, extra);
    if ((buf) && (buf->error == 0))
        buf->error = XML_BUF_OVERFLOW;
}


/**
 * xmlBufCreate:
 *
 * routine to create an XML buffer.
 * returns the new structure.
 */
xmlBufPtr
xmlBufCreate(void) {
    xmlBufPtr ret;

    ret = (xmlBufPtr) xmlMalloc(sizeof(xmlBuf));
    if (ret == NULL) {
	xmlBufMemoryError(NULL, "creating buffer");
        return(NULL);
    }
    ret->use = 0;
    ret->error = 0;
    ret->buffer = NULL;
    ret->size = xmlDefaultBufferSize;
    UPDATE_COMPAT(ret);
    ret->alloc = xmlBufferAllocScheme;
    ret->content = (xmlChar *) xmlMallocAtomic(ret->size);
    if (ret->content == NULL) {
	xmlBufMemoryError(ret, "creating buffer");
	xmlFree(ret);
        return(NULL);
    }
    ret->content[0] = 0;
    ret->contentIO = NULL;
    return(ret);
}

/**
 * xmlBufCreateSize:
 * @size: initial size of buffer
 *
 * routine to create an XML buffer.
 * returns the new structure.
 */
xmlBufPtr
xmlBufCreateSize(size_t size) {
    xmlBufPtr ret;

    if (size == SIZE_MAX)
        return(NULL);
    ret = (xmlBufPtr) xmlMalloc(sizeof(xmlBuf));
    if (ret == NULL) {
	xmlBufMemoryError(NULL, "creating buffer");
        return(NULL);
    }
    ret->use = 0;
    ret->error = 0;
    ret->buffer = NULL;
    ret->alloc = xmlBufferAllocScheme;
    ret->size = (size ? size + 1 : 0);         /* +1 for ending null */
    UPDATE_COMPAT(ret);
    if (ret->size){
        ret->content = (xmlChar *) xmlMallocAtomic(ret->size);
        if (ret->content == NULL) {
	    xmlBufMemoryError(ret, "creating buffer");
            xmlFree(ret);
            return(NULL);
        }
        ret->content[0] = 0;
    } else
	ret->content = NULL;
    ret->contentIO = NULL;
    return(ret);
}

/**
 * xmlBufDetach:
 * @buf:  the buffer
 *
 * Remove the string contained in a buffer and give it back to the
 * caller. The buffer is reset to an empty content.
 * This doesn't work with immutable buffers as they can't be reset.
 *
 * Returns the previous string contained by the buffer.
 */
xmlChar *
xmlBufDetach(xmlBufPtr buf) {
    xmlChar *ret;

    if (buf == NULL)
        return(NULL);
    if (buf->buffer != NULL)
        return(NULL);
    if (buf->error)
        return(NULL);

    ret = buf->content;
    buf->content = NULL;
    buf->size = 0;
    buf->use = 0;
    UPDATE_COMPAT(buf);

    return ret;
}

/**
 * xmlBufGetAllocationScheme:
 * @buf:  the buffer
 *
 * Get the buffer allocation scheme
 *
 * Returns the scheme or -1 in case of error
 */
int
xmlBufGetAllocationScheme(xmlBufPtr buf) {
    if (buf == NULL) {
#ifdef DEBUG_BUFFER
        xmlGenericError(xmlGenericErrorContext,
		"xmlBufGetAllocationScheme: buf == NULL\n");
#endif
        return(-1);
    }
    return(buf->alloc);
}

/**
 * xmlBufSetAllocationScheme:
 * @buf:  the buffer to tune
 * @scheme:  allocation scheme to use
 *
 * Sets the allocation scheme for this buffer
 *
 * returns 0 in case of success and -1 in case of failure
 */
int
xmlBufSetAllocationScheme(xmlBufPtr buf,
                          xmlBufferAllocationScheme scheme) {
    if ((buf == NULL) || (buf->error != 0)) {
#ifdef DEBUG_BUFFER
        xmlGenericError(xmlGenericErrorContext,
		"xmlBufSetAllocationScheme: buf == NULL or in error\n");
#endif
        return(-1);
    }
    if (buf->alloc == XML_BUFFER_ALLOC_IO)
        return(-1);
    if ((scheme == XML_BUFFER_ALLOC_DOUBLEIT) ||
        (scheme == XML_BUFFER_ALLOC_EXACT) ||
        (scheme == XML_BUFFER_ALLOC_HYBRID) ||
	(scheme == XML_BUFFER_ALLOC_BOUNDED)) {
	buf->alloc = scheme;
        if (buf->buffer)
            buf->buffer->alloc = scheme;
        return(0);
    }
    /*
     * Switching a buffer ALLOC_IO has the side effect of initializing
     * the contentIO field with the current content
     */
    if (scheme == XML_BUFFER_ALLOC_IO) {
        buf->alloc = XML_BUFFER_ALLOC_IO;
        buf->contentIO = buf->content;
    }
    return(-1);
}

/**
 * xmlBufFree:
 * @buf:  the buffer to free
 *
 * Frees an XML buffer. It frees both the content and the structure which
 * encapsulate it.
 */
void
xmlBufFree(xmlBufPtr buf) {
    if (buf == NULL) {
#ifdef DEBUG_BUFFER
        xmlGenericError(xmlGenericErrorContext,
		"xmlBufFree: buf == NULL\n");
#endif
	return;
    }

    if ((buf->alloc == XML_BUFFER_ALLOC_IO) &&
        (buf->contentIO != NULL)) {
        xmlFree(buf->contentIO);
    } else if (buf->content != NULL) {
        xmlFree(buf->content);
    }
    xmlFree(buf);
}

/**
 * xmlBufEmpty:
 * @buf:  the buffer
 *
 * empty a buffer.
 */
void
xmlBufEmpty(xmlBufPtr buf) {
    if ((buf == NULL) || (buf->error != 0)) return;
    if (buf->content == NULL) return;
    CHECK_COMPAT(buf)
    buf->use = 0;
    if ((buf->alloc == XML_BUFFER_ALLOC_IO) &&
               (buf->contentIO != NULL)) {
        size_t start_buf = buf->content - buf->contentIO;

	buf->size += start_buf;
        buf->content = buf->contentIO;
        buf->content[0] = 0;
    } else {
        buf->content[0] = 0;
    }
    UPDATE_COMPAT(buf)
}

/**
 * xmlBufShrink:
 * @buf:  the buffer to dump
 * @len:  the number of xmlChar to remove
 *
 * Remove the beginning of an XML buffer.
 * NOTE that this routine behaviour differs from xmlBufferShrink()
 * as it will return 0 on error instead of -1 due to size_t being
 * used as the return type.
 *
 * Returns the number of byte removed or 0 in case of failure
 */
size_t
xmlBufShrink(xmlBufPtr buf, size_t len) {
    if ((buf == NULL) || (buf->error != 0)) return(0);
    CHECK_COMPAT(buf)
    if (len == 0) return(0);
    if (len > buf->use) return(0);

    buf->use -= len;
    if ((buf->alloc == XML_BUFFER_ALLOC_IO) && (buf->contentIO != NULL)) {
	/*
	 * we just move the content pointer, but also make sure
	 * the perceived buffer size has shrunk accordingly
	 */
        buf->content += len;
	buf->size -= len;

        /*
	 * sometimes though it maybe be better to really shrink
	 * on IO buffers
	 */
	if ((buf->alloc == XML_BUFFER_ALLOC_IO) && (buf->contentIO != NULL)) {
	    size_t start_buf = buf->content - buf->contentIO;
	    if (start_buf >= buf->size) {
		memmove(buf->contentIO, &buf->content[0], buf->use);
		buf->content = buf->contentIO;
		buf->content[buf->use] = 0;
		buf->size += start_buf;
	    }
	}
    } else {
	memmove(buf->content, &buf->content[len], buf->use);
	buf->content[buf->use] = 0;
    }
    UPDATE_COMPAT(buf)
    return(len);
}

/**
 * xmlBufGrowInternal:
 * @buf:  the buffer
 * @len:  the minimum free size to allocate
 *
 * Grow the available space of an XML buffer, @len is the target value
 * Error checking should be done on buf->error since using the return
 * value doesn't work that well
 *
 * Returns 0 in case of error or the length made available otherwise
 */
static size_t
xmlBufGrowInternal(xmlBufPtr buf, size_t len) {
    size_t size;
    xmlChar *newbuf;

    if ((buf == NULL) || (buf->error != 0)) return(0);
    CHECK_COMPAT(buf)

    if (len < buf->size - buf->use)
        return(buf->size - buf->use - 1);
    if (len >= SIZE_MAX - buf->use) {
        xmlBufMemoryError(buf, "growing buffer past SIZE_MAX");
        return(0);
    }

    if (buf->size > (size_t) len) {
        size = buf->size > SIZE_MAX / 2 ? SIZE_MAX : buf->size * 2;
    } else {
        size = buf->use + len;
        size = size > SIZE_MAX - 100 ? SIZE_MAX : size + 100;
    }

    if (buf->alloc == XML_BUFFER_ALLOC_BOUNDED) {
        /*
	 * Used to provide parsing limits
	 */
        if ((buf->use + len + 1 >= XML_MAX_TEXT_LENGTH) ||
	    (buf->size >= XML_MAX_TEXT_LENGTH)) {
	    xmlBufMemoryError(buf, "buffer error: text too long\n");
	    return(0);
	}
	if (size >= XML_MAX_TEXT_LENGTH)
	    size = XML_MAX_TEXT_LENGTH;
    }
    if ((buf->alloc == XML_BUFFER_ALLOC_IO) && (buf->contentIO != NULL)) {
        size_t start_buf = buf->content - buf->contentIO;

	newbuf = (xmlChar *) xmlRealloc(buf->contentIO, start_buf + size);
	if (newbuf == NULL) {
	    xmlBufMemoryError(buf, "growing buffer");
	    return(0);
	}
	buf->contentIO = newbuf;
	buf->content = newbuf + start_buf;
    } else {
	newbuf = (xmlChar *) xmlRealloc(buf->content, size);
	if (newbuf == NULL) {
	    xmlBufMemoryError(buf, "growing buffer");
	    return(0);
	}
	buf->content = newbuf;
    }
    buf->size = size;
    UPDATE_COMPAT(buf)
    return(buf->size - buf->use - 1);
}

/**
 * xmlBufGrow:
 * @buf:  the buffer
 * @len:  the minimum free size to allocate
 *
 * Grow the available space of an XML buffer, @len is the target value
 * This is been kept compatible with xmlBufferGrow() as much as possible
 *
 * Returns -1 in case of error or the length made available otherwise
 */
int
xmlBufGrow(xmlBufPtr buf, int len) {
    size_t ret;

    if ((buf == NULL) || (len < 0)) return(-1);
    if (len == 0)
        return(0);
    ret = xmlBufGrowInternal(buf, len);
    if (buf->error != 0)
        return(-1);
    return(ret > INT_MAX ? INT_MAX : ret);
}

/**
 * xmlBufDump:
 * @file:  the file output
 * @buf:  the buffer to dump
 *
 * Dumps an XML buffer to  a FILE *.
 * Returns the number of #xmlChar written
 */
size_t
xmlBufDump(FILE *file, xmlBufPtr buf) {
    size_t ret;

    if ((buf == NULL) || (buf->error != 0)) {
#ifdef DEBUG_BUFFER
        xmlGenericError(xmlGenericErrorContext,
		"xmlBufDump: buf == NULL or in error\n");
#endif
	return(0);
    }
    if (buf->content == NULL) {
#ifdef DEBUG_BUFFER
        xmlGenericError(xmlGenericErrorContext,
		"xmlBufDump: buf->content == NULL\n");
#endif
	return(0);
    }
    CHECK_COMPAT(buf)
    if (file == NULL)
	file = stdout;
    ret = fwrite(buf->content, 1, buf->use, file);
    return(ret);
}

/**
 * xmlBufContent:
 * @buf:  the buffer
 *
 * Function to extract the content of a buffer
 *
 * Returns the internal content
 */

xmlChar *
xmlBufContent(const xmlBuf *buf)
{
    if ((!buf) || (buf->error))
        return NULL;

    return(buf->content);
}

/**
 * xmlBufEnd:
 * @buf:  the buffer
 *
 * Function to extract the end of the content of a buffer
 *
 * Returns the end of the internal content or NULL in case of error
 */

xmlChar *
xmlBufEnd(xmlBufPtr buf)
{
    if ((!buf) || (buf->error))
        return NULL;
    CHECK_COMPAT(buf)

    return(&buf->content[buf->use]);
}

/**
 * xmlBufAddLen:
 * @buf:  the buffer
 * @len:  the size which were added at the end
 *
 * Sometime data may be added at the end of the buffer without
 * using the xmlBuf APIs that is used to expand the used space
 * and set the zero terminating at the end of the buffer
 *
 * Returns -1 in case of error and 0 otherwise
 */
int
xmlBufAddLen(xmlBufPtr buf, size_t len) {
    if ((buf == NULL) || (buf->error))
        return(-1);
    CHECK_COMPAT(buf)
    if (len >= (buf->size - buf->use))
        return(-1);
    buf->use += len;
    buf->content[buf->use] = 0;
    UPDATE_COMPAT(buf)
    return(0);
}

/**
 * xmlBufLength:
 * @buf:  the buffer
 *
 * Function to get the length of a buffer
 *
 * Returns the length of data in the internal content
 */

size_t
xmlBufLength(const xmlBufPtr buf)
{
    if ((!buf) || (buf->error))
        return 0;
    CHECK_COMPAT(buf)

    return(buf->use);
}

/**
 * xmlBufUse:
 * @buf:  the buffer
 *
 * Function to get the length of a buffer
 *
 * Returns the length of data in the internal content
 */

size_t
xmlBufUse(const xmlBufPtr buf)
{
    if ((!buf) || (buf->error))
        return 0;
    CHECK_COMPAT(buf)

    return(buf->use);
}

/**
 * xmlBufAvail:
 * @buf:  the buffer
 *
 * Function to find how much free space is allocated but not
 * used in the buffer. It reserves one byte for the NUL
 * terminator character that is usually needed, so there is
 * no need to subtract 1 from the result anymore.
 *
 * Returns the amount, or 0 if none or if an error occurred.
 */

size_t
xmlBufAvail(const xmlBufPtr buf)
{
    if ((!buf) || (buf->error))
        return 0;
    CHECK_COMPAT(buf)

    return((buf->size > buf->use) ? (buf->size - buf->use - 1) : 0);
}

/**
 * xmlBufIsEmpty:
 * @buf:  the buffer
 *
 * Tell if a buffer is empty
 *
 * Returns 0 if no, 1 if yes and -1 in case of error
 */
int
xmlBufIsEmpty(const xmlBufPtr buf)
{
    if ((!buf) || (buf->error))
        return(-1);
    CHECK_COMPAT(buf)

    return(buf->use == 0);
}

/**
 * xmlBufResize:
 * @buf:  the buffer to resize
 * @size:  the desired size
 *
 * Resize a buffer to accommodate minimum size of @size.
 *
 * Returns  0 in case of problems, 1 otherwise
 */
int
xmlBufResize(xmlBufPtr buf, size_t size)
{
    size_t newSize;
    xmlChar* rebuf = NULL;
    size_t start_buf;

    if ((buf == NULL) || (buf->error))
        return(0);
    CHECK_COMPAT(buf)

    if (buf->alloc == XML_BUFFER_ALLOC_BOUNDED) {
        /*
	 * Used to provide parsing limits
	 */
        if (size >= XML_MAX_TEXT_LENGTH) {
	    xmlBufMemoryError(buf, "buffer error: text too long\n");
	    return(0);
	}
    }

    /* Don't resize if we don't have to */
    if (size < buf->size)
        return 1;

    /* figure out new size */
    switch (buf->alloc){
	case XML_BUFFER_ALLOC_IO:
	case XML_BUFFER_ALLOC_DOUBLEIT:
	    /*take care of empty case*/
            if (buf->size == 0) {
                newSize = (size > SIZE_MAX - 10 ? SIZE_MAX : size + 10);
            } else {
                newSize = buf->size;
            }
	    while (size > newSize) {
	        if (newSize > SIZE_MAX / 2) {
	            xmlBufMemoryError(buf, "growing buffer");
	            return 0;
	        }
	        newSize *= 2;
	    }
	    break;
	case XML_BUFFER_ALLOC_EXACT:
            newSize = (size > SIZE_MAX - 10 ? SIZE_MAX : size + 10);
	    break;
        case XML_BUFFER_ALLOC_HYBRID:
            if (buf->use < BASE_BUFFER_SIZE)
                newSize = size;
            else {
                newSize = buf->size;
                while (size > newSize) {
                    if (newSize > SIZE_MAX / 2) {
                        xmlBufMemoryError(buf, "growing buffer");
                        return 0;
                    }
                    newSize *= 2;
                }
            }
            break;

	default:
            newSize = (size > SIZE_MAX - 10 ? SIZE_MAX : size + 10);
	    break;
    }

    if ((buf->alloc == XML_BUFFER_ALLOC_IO) && (buf->contentIO != NULL)) {
        start_buf = buf->content - buf->contentIO;

        if (start_buf > newSize) {
	    /* move data back to start */
	    memmove(buf->contentIO, buf->content, buf->use);
	    buf->content = buf->contentIO;
	    buf->content[buf->use] = 0;
	    buf->size += start_buf;
	} else {
	    rebuf = (xmlChar *) xmlRealloc(buf->contentIO, start_buf + newSize);
	    if (rebuf == NULL) {
		xmlBufMemoryError(buf, "growing buffer");
		return 0;
	    }
	    buf->contentIO = rebuf;
	    buf->content = rebuf + start_buf;
	}
    } else {
	if (buf->content == NULL) {
	    rebuf = (xmlChar *) xmlMallocAtomic(newSize);
	    buf->use = 0;
            if (rebuf != NULL)
	        rebuf[buf->use] = 0;
	} else if (buf->size - buf->use < 100) {
	    rebuf = (xmlChar *) xmlRealloc(buf->content, newSize);
        } else {
	    /*
	     * if we are reallocating a buffer far from being full, it's
	     * better to make a new allocation and copy only the used range
	     * and free the old one.
	     */
	    rebuf = (xmlChar *) xmlMallocAtomic(newSize);
	    if (rebuf != NULL) {
		memcpy(rebuf, buf->content, buf->use);
		xmlFree(buf->content);
		rebuf[buf->use] = 0;
	    }
	}
	if (rebuf == NULL) {
	    xmlBufMemoryError(buf, "growing buffer");
	    return 0;
	}
	buf->content = rebuf;
    }
    buf->size = newSize;
    UPDATE_COMPAT(buf)

    return 1;
}

/**
 * xmlBufAdd:
 * @buf:  the buffer to dump
 * @str:  the #xmlChar string
 * @len:  the number of #xmlChar to add
 *
 * Add a string range to an XML buffer. if len == -1, the length of
 * str is recomputed.
 *
 * Returns 0 successful, a positive error code number otherwise
 *         and -1 in case of internal or API error.
 */
int
xmlBufAdd(xmlBufPtr buf, const xmlChar *str, int len) {
    size_t needSize;

    if ((str == NULL) || (buf == NULL) || (buf->error))
	return -1;
    CHECK_COMPAT(buf)

    if (len < -1) {
#ifdef DEBUG_BUFFER
        xmlGenericError(xmlGenericErrorContext,
		"xmlBufAdd: len < 0\n");
#endif
	return -1;
    }
    if (len == 0) return 0;

    if (len < 0)
        len = xmlStrlen(str);

    if (len < 0) return -1;
    if (len == 0) return 0;

    /* Note that both buf->size and buf->use can be zero here. */
    if ((size_t) len >= buf->size - buf->use) {
        if ((size_t) len >= SIZE_MAX - buf->use) {
            xmlBufMemoryError(buf, "growing buffer past SIZE_MAX");
            return(-1);
        }
        needSize = buf->use + len + 1;
	if (buf->alloc == XML_BUFFER_ALLOC_BOUNDED) {
	    /*
	     * Used to provide parsing limits
	     */
	    if (needSize >= XML_MAX_TEXT_LENGTH) {
		xmlBufMemoryError(buf, "buffer error: text too long\n");
		return(-1);
	    }
	}
        if (!xmlBufResize(buf, needSize)){
	    xmlBufMemoryError(buf, "growing buffer");
            return XML_ERR_NO_MEMORY;
        }
    }

    memmove(&buf->content[buf->use], str, len);
    buf->use += len;
    buf->content[buf->use] = 0;
    UPDATE_COMPAT(buf)
    return 0;
}

/**
 * xmlBufCat:
 * @buf:  the buffer to add to
 * @str:  the #xmlChar string
 *
 * Append a zero terminated string to an XML buffer.
 *
 * Returns 0 successful, a positive error code number otherwise
 *         and -1 in case of internal or API error.
 */
int
xmlBufCat(xmlBufPtr buf, const xmlChar *str) {
    if ((buf == NULL) || (buf->error))
        return(-1);
    CHECK_COMPAT(buf)
    if (str == NULL) return -1;
    return xmlBufAdd(buf, str, -1);
}

/**
 * xmlBufCCat:
 * @buf:  the buffer to dump
 * @str:  the C char string
 *
 * Append a zero terminated C string to an XML buffer.
 *
 * Returns 0 successful, a positive error code number otherwise
 *         and -1 in case of internal or API error.
 */
int
xmlBufCCat(xmlBufPtr buf, const char *str) {
    return xmlBufCat(buf, (const xmlChar *) str);
}

/**
 * xmlBufWriteQuotedString:
 * @buf:  the XML buffer output
 * @string:  the string to add
 *
 * routine which manage and grows an output buffer. This one writes
 * a quoted or double quoted #xmlChar string, checking first if it holds
 * quote or double-quotes internally
 *
 * Returns 0 if successful, a positive error code number otherwise
 *         and -1 in case of internal or API error.
 */
int
xmlBufWriteQuotedString(xmlBufPtr buf, const xmlChar *string) {
    const xmlChar *cur, *base;
    if ((buf == NULL) || (buf->error))
        return(-1);
    CHECK_COMPAT(buf)
    if (xmlStrchr(string, '\"')) {
        if (xmlStrchr(string, '\'')) {
#ifdef DEBUG_BUFFER
	    xmlGenericError(xmlGenericErrorContext,
 "xmlBufWriteQuotedString: string contains quote and double-quotes !\n");
#endif
	    xmlBufCCat(buf, "\"");
            base = cur = string;
            while(*cur != 0){
                if(*cur == '"'){
                    if (base != cur)
                        xmlBufAdd(buf, base, cur - base);
                    xmlBufAdd(buf, BAD_CAST "&quot;", 6);
                    cur++;
                    base = cur;
                }
                else {
                    cur++;
                }
            }
            if (base != cur)
                xmlBufAdd(buf, base, cur - base);
	    xmlBufCCat(buf, "\"");
	}
        else{
	    xmlBufCCat(buf, "\'");
            xmlBufCat(buf, string);
	    xmlBufCCat(buf, "\'");
        }
    } else {
        xmlBufCCat(buf, "\"");
        xmlBufCat(buf, string);
        xmlBufCCat(buf, "\"");
    }
    return(0);
}

/**
 * xmlBufFromBuffer:
 * @buffer: incoming old buffer to convert to a new one
 *
 * Helper routine to switch from the old buffer structures in use
 * in various APIs. It creates a wrapper xmlBufPtr which will be
 * used for internal processing until the xmlBufBackToBuffer() is
 * issued.
 *
 * Returns a new xmlBufPtr unless the call failed and NULL is returned
 */
xmlBufPtr
xmlBufFromBuffer(xmlBufferPtr buffer) {
    xmlBufPtr ret;

    if (buffer == NULL)
        return(NULL);

    ret = (xmlBufPtr) xmlMalloc(sizeof(xmlBuf));
    if (ret == NULL) {
	xmlBufMemoryError(NULL, "creating buffer");
        return(NULL);
    }
    ret->use = buffer->use;
    ret->size = buffer->size;
    UPDATE_COMPAT(ret);
    ret->error = 0;
    ret->buffer = buffer;
    ret->alloc = buffer->alloc;
    ret->content = buffer->content;
    ret->contentIO = buffer->contentIO;

    return(ret);
}

/**
 * xmlBufBackToBuffer:
 * @buf: new buffer wrapping the old one
 *
 * Function to be called once internal processing had been done to
 * update back the buffer provided by the user. This can lead to
 * a failure in case the size accumulated in the xmlBuf is larger
 * than what an xmlBuffer can support on 64 bits (INT_MAX)
 * The xmlBufPtr @buf wrapper is deallocated by this call in any case.
 *
 * Returns the old xmlBufferPtr unless the call failed and NULL is returned
 */
xmlBufferPtr
xmlBufBackToBuffer(xmlBufPtr buf) {
    xmlBufferPtr ret;

    if (buf == NULL)
        return(NULL);
    CHECK_COMPAT(buf)
    if ((buf->error) || (buf->buffer == NULL)) {
        xmlBufFree(buf);
        return(NULL);
    }

    ret = buf->buffer;
    /*
     * What to do in case of error in the buffer ???
     */
    if (buf->use > INT_MAX) {
        /*
         * Worse case, we really allocated and used more than the
         * maximum allowed memory for an xmlBuffer on this architecture.
         * Keep the buffer but provide a truncated size value.
         */
        xmlBufOverflowError(buf, "Used size too big for xmlBuffer");
        ret->use = INT_MAX;
        ret->size = INT_MAX;
    } else if (buf->size > INT_MAX) {
        /*
         * milder case, we allocated more than the maximum allowed memory
         * for an xmlBuffer on this architecture, but used less than the
         * limit.
         * Keep the buffer but provide a truncated size value.
         */
        xmlBufOverflowError(buf, "Allocated size too big for xmlBuffer");
        ret->use = buf->use;
        ret->size = INT_MAX;
    } else {
        ret->use = buf->use;
        ret->size = buf->size;
    }
    ret->alloc = buf->alloc;
    ret->content = buf->content;
    ret->contentIO = buf->contentIO;
    xmlFree(buf);
    return(ret);
}

/**
 * xmlBufMergeBuffer:
 * @buf: an xmlBufPtr
 * @buffer: the buffer to consume into @buf
 *
 * The content of @buffer is appended to @buf and @buffer is freed
 *
 * Returns -1 in case of error, 0 otherwise, in any case @buffer is freed
 */
int
xmlBufMergeBuffer(xmlBufPtr buf, xmlBufferPtr buffer) {
    int ret = 0;

    if ((buf == NULL) || (buf->error)) {
	xmlBufferFree(buffer);
        return(-1);
    }
    CHECK_COMPAT(buf)
    if ((buffer != NULL) && (buffer->content != NULL) &&
             (buffer->use > 0)) {
        ret = xmlBufAdd(buf, buffer->content, buffer->use);
    }
    xmlBufferFree(buffer);
    return(ret);
}

/**
 * xmlBufResetInput:
 * @buf: an xmlBufPtr
 * @input: an xmlParserInputPtr
 *
 * Update the input to use the current set of pointers from the buffer.
 *
 * Returns -1 in case of error, 0 otherwise
 */
int
xmlBufResetInput(xmlBufPtr buf, xmlParserInputPtr input) {
    if ((input == NULL) || (buf == NULL) || (buf->error))
        return(-1);
    CHECK_COMPAT(buf)
    input->base = input->cur = buf->content;
    input->end = &buf->content[buf->use];
    return(0);
}

/**
 * xmlBufGetInputBase:
 * @buf: an xmlBufPtr
 * @input: an xmlParserInputPtr
 *
 * Get the base of the @input relative to the beginning of the buffer
 *
 * Returns the size_t corresponding to the displacement
 */
size_t
xmlBufGetInputBase(xmlBufPtr buf, xmlParserInputPtr input) {
    size_t base;

    if ((input == NULL) || (buf == NULL) || (buf->error))
        return(0);
    CHECK_COMPAT(buf)
    base = input->base - buf->content;
    /*
     * We could do some pointer arithmetic checks but that's probably
     * sufficient.
     */
    if (base > buf->size) {
        xmlBufOverflowError(buf, "Input reference outside of the buffer");
        base = 0;
    }
    return(base);
}

/**
 * xmlBufSetInputBaseCur:
 * @buf: an xmlBufPtr
 * @input: an xmlParserInputPtr
 * @base: the base value relative to the beginning of the buffer
 * @cur: the cur value relative to the beginning of the buffer
 *
 * Update the input to use the base and cur relative to the buffer
 * after a possible reallocation of its content
 *
 * Returns -1 in case of error, 0 otherwise
 */
int
xmlBufSetInputBaseCur(xmlBufPtr buf, xmlParserInputPtr input,
                      size_t base, size_t cur) {
    if (input == NULL)
        return(-1);
    if ((buf == NULL) || (buf->error)) {
        input->base = input->cur = input->end = BAD_CAST "";
        return(-1);
    }
    CHECK_COMPAT(buf)
    input->base = &buf->content[base];
    input->cur = input->base + cur;
    input->end = &buf->content[buf->use];
    return(0);
}

