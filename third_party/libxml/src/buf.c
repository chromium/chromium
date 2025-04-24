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

#include <string.h>
#include <limits.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "private/buf.h"

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t) -1)
#endif

#define WITH_BUFFER_COMPAT

#define BUF_FLAG_OOM        (1u << 0)
#define BUF_FLAG_OVERFLOW   (1u << 1)
#define BUF_FLAG_STATIC     (1u << 2)

#define BUF_ERROR(buf) ((buf)->flags & (BUF_FLAG_OOM | BUF_FLAG_OVERFLOW))
#define BUF_STATIC(buf) ((buf)->flags & BUF_FLAG_STATIC)

/**
 * xmlBuf:
 *
 * A buffer structure. The base of the structure is somehow compatible
 * with struct _xmlBuffer to limit risks on application which accessed
 * directly the input->buf->buffer structures.
 */

struct _xmlBuf {
    xmlChar *content;		/* The buffer content UTF8 */
#ifdef WITH_BUFFER_COMPAT
    unsigned int compat_use;    /* for binary compatibility */
    unsigned int compat_size;   /* for binary compatibility */
#endif
    xmlChar *mem;		/* Start of the allocation */
    size_t use;		        /* The buffer size used */
    size_t size;		/* The buffer size, excluding terminating 0 */
    size_t maxSize;             /* The maximum buffer size */
    unsigned flags;             /* flags */
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
xmlBufMemoryError(xmlBufPtr buf)
{
    if (!BUF_ERROR(buf))
        buf->flags |= BUF_FLAG_OOM;
}

/**
 * xmlBufOverflowError:
 * @extra:  extra information
 *
 * Handle a buffer overflow error
 * To be improved...
 */
static void
xmlBufOverflowError(xmlBufPtr buf)
{
    if (!BUF_ERROR(buf))
        buf->flags |= BUF_FLAG_OVERFLOW;
}

/**
 * xmlBufCreate:
 * @size: initial size of buffer
 *
 * routine to create an XML buffer.
 * returns the new structure.
 */
xmlBufPtr
xmlBufCreate(size_t size) {
    xmlBufPtr ret;

    if (size == SIZE_MAX)
        return(NULL);

    ret = xmlMalloc(sizeof(*ret));
    if (ret == NULL)
        return(NULL);

    ret->use = 0;
    ret->flags = 0;
    ret->size = size;
    ret->maxSize = SIZE_MAX - 1;

    ret->mem = xmlMalloc(ret->size + 1);
    if (ret->mem == NULL) {
        xmlFree(ret);
        return(NULL);
    }
    ret->content = ret->mem;
    ret->content[0] = 0;

    UPDATE_COMPAT(ret);
    return(ret);
}

/**
 * xmlBufCreateMem:
 * @mem:  a memory area
 * @size:  size of the buffer excluding terminator
 * @isStatic:  whether the memory area is static
 *
 * Create a buffer initialized with memory.
 *
 * If @isStatic is set, uses the memory area directly as backing store.
 * The memory must be zero-terminated and not be modified for the
 * lifetime of the buffer. A static buffer can't be grown, modified or
 * detached, but it can be shrunk.
 *
 * Returns a new buffer.
 */
xmlBufPtr
xmlBufCreateMem(const xmlChar *mem, size_t size, int isStatic) {
    xmlBufPtr ret;

    if (mem == NULL)
        return(NULL);

    ret = xmlMalloc(sizeof(*ret));
    if (ret == NULL)
        return(NULL);

    if (isStatic) {
        /* Check that memory is zero-terminated */
        if (mem[size] != 0) {
            xmlFree(ret);
            return(NULL);
        }
        ret->flags = BUF_FLAG_STATIC;
        ret->mem = (xmlChar *) mem;
    } else {
        ret->flags = 0;
        ret->mem = xmlMalloc(size + 1);
        if (ret->mem == NULL) {
            xmlFree(ret);
            return(NULL);
        }
        memcpy(ret->mem, mem, size);
        ret->mem[size] = 0;
    }

    ret->use = size;
    ret->size = size;
    ret->maxSize = SIZE_MAX - 1;
    ret->content = ret->mem;

    UPDATE_COMPAT(ret);
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

    if ((buf == NULL) || (BUF_ERROR(buf)) || (BUF_STATIC(buf)))
        return(NULL);

    if (buf->content != buf->mem) {
        ret = xmlStrndup(buf->content, buf->use);
        xmlFree(buf->mem);
    } else {
        ret = buf->mem;
    }

    buf->content = NULL;
    buf->mem = NULL;
    buf->size = 0;
    buf->use = 0;

    UPDATE_COMPAT(buf);
    return ret;
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
    if (buf == NULL)
	return;

    if (!BUF_STATIC(buf))
        xmlFree(buf->mem);
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
    if ((buf == NULL) || (BUF_ERROR(buf)) || (BUF_STATIC(buf)))
        return;
    if (buf->mem == NULL)
        return;
    CHECK_COMPAT(buf)

    buf->use = 0;
    buf->size += buf->content - buf->mem;
    buf->content = buf->mem;
    buf->content[0] = 0;

    UPDATE_COMPAT(buf)
}

/**
 * xmlBufShrink:
 * @buf:  the buffer to dump
 * @len:  the number of xmlChar to remove
 *
 * DEPRECATED: Don't use.
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
    if ((buf == NULL) || (BUF_ERROR(buf)))
        return(0);
    if (len == 0)
        return(0);
    CHECK_COMPAT(buf)

    if (len > buf->use)
        return(0);

    buf->use -= len;
    buf->content += len;
    buf->size -= len;

    UPDATE_COMPAT(buf)
    return(len);
}

/**
 * xmlBufGrowInternal:
 * @buf:  the buffer
 * @len:  the minimum free size to allocate
 *
 * Grow the available space of an XML buffer, @len is the target value
 *
 * Returns 0 on success, -1 in case of error
 */
static int
xmlBufGrowInternal(xmlBufPtr buf, size_t len) {
    size_t size;
    size_t start;
    xmlChar *newbuf;

    /*
     * If there's enough space at the start of the buffer,
     * move the contents.
     */
    start = buf->content - buf->mem;
    if (len <= start + buf->size - buf->use) {
        memmove(buf->mem, buf->content, buf->use + 1);
        buf->size += start;
        buf->content = buf->mem;
        return(0);
    }

    if (len > buf->maxSize - buf->use) {
        xmlBufOverflowError(buf);
        return(-1);
    }

    if (buf->size > (size_t) len) {
        if (buf->size <= buf->maxSize / 2)
            size = buf->size * 2;
        else
            size = buf->maxSize;
    } else {
        size = buf->use + len;
        if (size <= buf->maxSize - 100)
            size += 100;
    }

    if (buf->content == buf->mem) {
        newbuf = xmlRealloc(buf->mem, size + 1);
        if (newbuf == NULL) {
            xmlBufMemoryError(buf);
            return(-1);
        }
    } else {
        newbuf = xmlMalloc(size + 1);
        if (newbuf == NULL) {
            xmlBufMemoryError(buf);
            return(-1);
        }
        if (buf->content != NULL)
            memcpy(newbuf, buf->content, buf->use + 1);
        xmlFree(buf->mem);
    }

    buf->mem = newbuf;
    buf->content = newbuf;
    buf->size = size;

    return(0);
}

/**
 * xmlBufGrow:
 * @buf:  the buffer
 * @len:  the minimum free size to allocate
 *
 * Grow the available space of an XML buffer, @len is the target value
 * This is been kept compatible with xmlBufferGrow() as much as possible
 *
 * Returns 0 on succes, -1 in case of error
 */
int
xmlBufGrow(xmlBufPtr buf, size_t len) {
    if ((buf == NULL) || (BUF_ERROR(buf)) || (BUF_STATIC(buf)))
        return(-1);
    CHECK_COMPAT(buf)

    if (len <= buf->size - buf->use)
        return(0);

    if (xmlBufGrowInternal(buf, len) < 0)
        return(-1);

    UPDATE_COMPAT(buf)
    return(0);
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
    if ((!buf) || (BUF_ERROR(buf)))
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
    if ((!buf) || (BUF_ERROR(buf)))
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
    if ((buf == NULL) || (BUF_ERROR(buf)) || (BUF_STATIC(buf)))
        return(-1);
    CHECK_COMPAT(buf)
    if (len > buf->size - buf->use)
        return(-1);
    buf->use += len;
    buf->content[buf->use] = 0;
    UPDATE_COMPAT(buf)
    return(0);
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
    if ((!buf) || (BUF_ERROR(buf)))
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
    if ((!buf) || (BUF_ERROR(buf)))
        return 0;
    CHECK_COMPAT(buf)

    return(buf->size - buf->use);
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
    if ((!buf) || (BUF_ERROR(buf)))
        return(-1);
    CHECK_COMPAT(buf)

    return(buf->use == 0);
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
 * Returns 0 if successful, -1 in case of error.
 */
int
xmlBufAdd(xmlBufPtr buf, const xmlChar *str, size_t len) {
    if ((buf == NULL) || (BUF_ERROR(buf)) || (BUF_STATIC(buf)))
        return(-1);
    if (len == 0)
        return(0);
    if (str == NULL)
	return(-1);
    CHECK_COMPAT(buf)

    if (len > buf->size - buf->use) {
        if (xmlBufGrowInternal(buf, len) < 0)
            return(-1);
    }

    memmove(&buf->content[buf->use], str, len);
    buf->use += len;
    buf->content[buf->use] = 0;

    UPDATE_COMPAT(buf)
    return(0);
}

/**
 * xmlBufCat:
 * @buf:  the buffer to add to
 * @str:  the #xmlChar string (optional)
 *
 * Append a zero terminated string to an XML buffer.
 *
 * Returns 0 successful, a positive error code number otherwise
 *         and -1 in case of internal or API error.
 */
int
xmlBufCat(xmlBufPtr buf, const xmlChar *str) {
    if (str == NULL)
        return(0);
    return(xmlBufAdd(buf, str, strlen((const char *) str)));
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

    ret = xmlMalloc(sizeof(xmlBuf));
    if (ret == NULL)
        return(NULL);

    ret->use = buffer->use;
    ret->flags = 0;
    ret->maxSize = SIZE_MAX - 1;

    if (buffer->content == NULL) {
        ret->size = 50;
        ret->mem = xmlMalloc(ret->size + 1);
        ret->content = ret->mem;
        if (ret->mem == NULL)
            xmlBufMemoryError(ret);
        else
            ret->content[0] = 0;
    } else {
        ret->size = buffer->size - 1;
        ret->content = buffer->content;
        if (buffer->alloc == XML_BUFFER_ALLOC_IO)
            ret->mem = buffer->contentIO;
        else
            ret->mem = buffer->content;
    }

    UPDATE_COMPAT(ret);
    return(ret);
}

/**
 * xmlBufBackToBuffer:
 * @buf: new buffer wrapping the old one
 * @ret: old buffer
 *
 * Function to be called once internal processing had been done to
 * update back the buffer provided by the user. This can lead to
 * a failure in case the size accumulated in the xmlBuf is larger
 * than what an xmlBuffer can support on 64 bits (INT_MAX)
 * The xmlBufPtr @buf wrapper is deallocated by this call in any case.
 *
 * Returns 0 on success, -1 on error.
 */
int
xmlBufBackToBuffer(xmlBufPtr buf, xmlBufferPtr ret) {
    if ((buf == NULL) || (ret == NULL))
        return(-1);

    if ((BUF_ERROR(buf)) || (BUF_STATIC(buf)) ||
        (buf->use >= INT_MAX)) {
        xmlBufFree(buf);
        ret->content = NULL;
        ret->contentIO = NULL;
        ret->use = 0;
        ret->size = 0;
        return(-1);
    }

    ret->use = buf->use;
    if (buf->size >= INT_MAX) {
        /* Keep the buffer but provide a truncated size value. */
        ret->size = INT_MAX;
    } else {
        ret->size = buf->size + 1;
    }
    ret->alloc = XML_BUFFER_ALLOC_IO;
    ret->content = buf->content;
    ret->contentIO = buf->mem;
    xmlFree(buf);
    return(0);
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
    return(xmlBufUpdateInput(buf, input, 0));
}

/**
 * xmlBufUpdateInput:
 * @buf: an xmlBufPtr
 * @input: an xmlParserInputPtr
 * @pos: the cur value relative to the beginning of the buffer
 *
 * Update the input to use the base and cur relative to the buffer
 * after a possible reallocation of its content
 *
 * Returns -1 in case of error, 0 otherwise
 */
int
xmlBufUpdateInput(xmlBufPtr buf, xmlParserInputPtr input, size_t pos) {
    if ((buf == NULL) || (input == NULL))
        return(-1);
    CHECK_COMPAT(buf)
    input->base = buf->content;
    input->cur = input->base + pos;
    input->end = &buf->content[buf->use];
    return(0);
}

/************************************************************************
 *									*
 *			Old buffer implementation			*
 *									*
 ************************************************************************/

/**
 * xmlSetBufferAllocationScheme:
 * @scheme:  allocation method to use
 *
 * DEPRECATED: Use xmlBufferSetAllocationScheme.
 *
 * Set the buffer allocation method.  Types are
 * XML_BUFFER_ALLOC_EXACT - use exact sizes, keeps memory usage down
 * XML_BUFFER_ALLOC_DOUBLEIT - double buffer when extra needed,
 *                             improves performance
 */
void
xmlSetBufferAllocationScheme(xmlBufferAllocationScheme scheme ATTRIBUTE_UNUSED) {
}

/**
 * xmlGetBufferAllocationScheme:
 *
 * DEPRECATED: Use xmlBufferSetAllocationScheme.
 *
 * Types are
 * XML_BUFFER_ALLOC_EXACT - use exact sizes, keeps memory usage down
 * XML_BUFFER_ALLOC_DOUBLEIT - double buffer when extra needed,
 *                             improves performance
 * XML_BUFFER_ALLOC_HYBRID - use exact sizes on small strings to keep memory usage tight
 *                            in normal usage, and doubleit on large strings to avoid
 *                            pathological performance.
 *
 * Returns the current allocation scheme
 */
xmlBufferAllocationScheme
xmlGetBufferAllocationScheme(void) {
    return(XML_BUFFER_ALLOC_EXACT);
}

/**
 * xmlBufferCreate:
 *
 * routine to create an XML buffer.
 * returns the new structure.
 */
xmlBufferPtr
xmlBufferCreate(void) {
    xmlBufferPtr ret;

    ret = xmlMalloc(sizeof(*ret));
    if (ret == NULL)
        return(NULL);

    ret->use = 0;
    ret->size = 256;
    ret->alloc = XML_BUFFER_ALLOC_IO;
    ret->contentIO = xmlMalloc(ret->size);
    if (ret->contentIO == NULL) {
	xmlFree(ret);
        return(NULL);
    }
    ret->content = ret->contentIO;
    ret->content[0] = 0;

    return(ret);
}

/**
 * xmlBufferCreateSize:
 * @size: initial size of buffer
 *
 * routine to create an XML buffer.
 * returns the new structure.
 */
xmlBufferPtr
xmlBufferCreateSize(size_t size) {
    xmlBufferPtr ret;

    if (size >= INT_MAX)
        return(NULL);

    ret = xmlMalloc(sizeof(*ret));
    if (ret == NULL)
        return(NULL);

    ret->use = 0;
    ret->alloc = XML_BUFFER_ALLOC_IO;
    ret->size = (size ? size + 1 : 0);         /* +1 for ending null */

    if (ret->size) {
        ret->contentIO = xmlMalloc(ret->size);
        if (ret->contentIO == NULL) {
            xmlFree(ret);
            return(NULL);
        }
        ret->content = ret->contentIO;
        ret->content[0] = 0;
    } else {
        ret->contentIO = NULL;
	ret->content = NULL;
    }

    return(ret);
}

/**
 * xmlBufferDetach:
 * @buf:  the buffer
 *
 * Remove the string contained in a buffer and gie it back to the
 * caller. The buffer is reset to an empty content.
 * This doesn't work with immutable buffers as they can't be reset.
 *
 * Returns the previous string contained by the buffer.
 */
xmlChar *
xmlBufferDetach(xmlBufferPtr buf) {
    xmlChar *ret;

    if (buf == NULL)
        return(NULL);

    if ((buf->alloc == XML_BUFFER_ALLOC_IO) &&
        (buf->content != buf->contentIO)) {
        ret = xmlStrndup(buf->content, buf->use);
        xmlFree(buf->contentIO);
    } else {
        ret = buf->content;
    }

    buf->contentIO = NULL;
    buf->content = NULL;
    buf->size = 0;
    buf->use = 0;

    return ret;
}

/**
 * xmlBufferCreateStatic:
 * @mem: the memory area
 * @size:  the size in byte
 *
 * Returns an XML buffer initialized with bytes.
 */
xmlBufferPtr
xmlBufferCreateStatic(void *mem, size_t size) {
    xmlBufferPtr buf = xmlBufferCreateSize(size);

    xmlBufferAdd(buf, mem, size);
    return(buf);
}

/**
 * xmlBufferSetAllocationScheme:
 * @buf:  the buffer to tune
 * @scheme:  allocation scheme to use
 *
 * Sets the allocation scheme for this buffer.
 *
 * For libxml2 before 2.14, it is recommended to set this to
 * XML_BUFFER_ALLOC_DOUBLE_IT. Has no effect on 2.14 or later.
 */
void
xmlBufferSetAllocationScheme(xmlBufferPtr buf ATTRIBUTE_UNUSED,
                             xmlBufferAllocationScheme scheme ATTRIBUTE_UNUSED) {
}

/**
 * xmlBufferFree:
 * @buf:  the buffer to free
 *
 * Frees an XML buffer. It frees both the content and the structure which
 * encapsulate it.
 */
void
xmlBufferFree(xmlBufferPtr buf) {
    if (buf == NULL)
	return;

    if (buf->alloc == XML_BUFFER_ALLOC_IO)
        xmlFree(buf->contentIO);
    else
        xmlFree(buf->content);

    xmlFree(buf);
}

/**
 * xmlBufferEmpty:
 * @buf:  the buffer
 *
 * empty a buffer.
 */
void
xmlBufferEmpty(xmlBufferPtr buf) {
    if (buf == NULL)
        return;
    if (buf->content == NULL)
        return;

    buf->use = 0;

    if (buf->alloc == XML_BUFFER_ALLOC_IO) {
	buf->size += buf->content - buf->contentIO;
        buf->content = buf->contentIO;
        buf->content[0] = 0;
    } else {
        buf->content[0] = 0;
    }
}

/**
 * xmlBufferShrink:
 * @buf:  the buffer to dump
 * @len:  the number of xmlChar to remove
 *
 * DEPRECATED: Don't use.
 *
 * Remove the beginning of an XML buffer.
 *
 * Returns the number of #xmlChar removed, or -1 in case of failure.
 */
int
xmlBufferShrink(xmlBufferPtr buf, unsigned int len) {
    if (buf == NULL)
        return(-1);
    if (len == 0)
        return(0);
    if (len > buf->use)
        return(-1);

    buf->use -= len;

    if (buf->alloc == XML_BUFFER_ALLOC_IO) {
        buf->content += len;
	buf->size -= len;
    } else {
	memmove(buf->content, &buf->content[len], buf->use + 1);
    }

    return(len);
}

/**
 * xmlBufferGrow:
 * @buf:  the buffer
 * @len:  the minimum free size to allocate
 *
 * DEPRECATED: Don't use.
 *
 * Grow the available space of an XML buffer.
 *
 * Returns the new available space or -1 in case of error
 */
int
xmlBufferGrow(xmlBufferPtr buf, unsigned int len) {
    unsigned int size;
    xmlChar *newbuf;

    if (buf == NULL)
        return(-1);

    if (len < buf->size - buf->use)
        return(0);
    if (len >= INT_MAX - buf->use)
        return(-1);

    if (buf->size > (size_t) len) {
        if (buf->size <= INT_MAX / 2)
            size = buf->size * 2;
        else
            size = INT_MAX;
    } else {
        size = buf->use + len + 1;
        if (size <= INT_MAX - 100)
            size += 100;
    }

    if ((buf->alloc == XML_BUFFER_ALLOC_IO) &&
        (buf->content != buf->contentIO)) {
        newbuf = xmlMalloc(size);
        if (newbuf == NULL)
            return(-1);
        if (buf->content != NULL)
            memcpy(newbuf, buf->content, buf->use + 1);
        xmlFree(buf->contentIO);
    } else {
        newbuf = xmlRealloc(buf->content, size);
        if (newbuf == NULL)
            return(-1);
    }

    if (buf->alloc == XML_BUFFER_ALLOC_IO)
        buf->contentIO = newbuf;
    buf->content = newbuf;
    buf->size = size;

    return(buf->size - buf->use - 1);
}

/**
 * xmlBufferDump:
 * @file:  the file output
 * @buf:  the buffer to dump
 *
 * Dumps an XML buffer to  a FILE *.
 * Returns the number of #xmlChar written
 */
int
xmlBufferDump(FILE *file, xmlBufferPtr buf) {
    size_t ret;

    if (buf == NULL)
	return(0);
    if (buf->content == NULL)
	return(0);
    if (file == NULL)
	file = stdout;
    ret = fwrite(buf->content, 1, buf->use, file);
    return(ret > INT_MAX ? INT_MAX : ret);
}

/**
 * xmlBufferContent:
 * @buf:  the buffer
 *
 * Function to extract the content of a buffer
 *
 * Returns the internal content
 */

const xmlChar *
xmlBufferContent(const xmlBuffer *buf)
{
    if(!buf)
        return NULL;

    return buf->content;
}

/**
 * xmlBufferLength:
 * @buf:  the buffer
 *
 * Function to get the length of a buffer
 *
 * Returns the length of data in the internal content
 */

int
xmlBufferLength(const xmlBuffer *buf)
{
    if(!buf)
        return 0;

    return buf->use;
}

/**
 * xmlBufferResize:
 * @buf:  the buffer to resize
 * @size:  the desired size
 *
 * DEPRECATED: Don't use.

 * Resize a buffer to accommodate minimum size of @size.
 *
 * Returns  0 in case of problems, 1 otherwise
 */
int
xmlBufferResize(xmlBufferPtr buf, unsigned int size)
{
    int res;

    if (buf == NULL)
        return(0);
    if (size < buf->size)
        return(1);
    res = xmlBufferGrow(buf, size - buf->use);

    return(res < 0 ? 0 : 1);
}

/**
 * xmlBufferAdd:
 * @buf:  the buffer to dump
 * @str:  the #xmlChar string
 * @len:  the number of #xmlChar to add
 *
 * Add a string range to an XML buffer. if len == -1, the length of
 * str is recomputed.
 *
 * Returns a xmlParserErrors code.
 */
int
xmlBufferAdd(xmlBufferPtr buf, const xmlChar *str, int len) {
    if ((buf == NULL) || (str == NULL))
	return(XML_ERR_ARGUMENT);
    if (len < 0)
        len = xmlStrlen(str);
    if (len == 0)
        return(XML_ERR_OK);

    /* Note that both buf->size and buf->use can be zero here. */
    if ((unsigned) len >= buf->size - buf->use) {
        if (xmlBufferGrow(buf, len) < 0)
            return(XML_ERR_NO_MEMORY);
    }

    memmove(&buf->content[buf->use], str, len);
    buf->use += len;
    buf->content[buf->use] = 0;
    return(XML_ERR_OK);
}

/**
 * xmlBufferAddHead:
 * @buf:  the buffer
 * @str:  the #xmlChar string
 * @len:  the number of #xmlChar to add
 *
 * Add a string range to the beginning of an XML buffer.
 * if len == -1, the length of @str is recomputed.
 *
 * Returns a xmlParserErrors code.
 */
int
xmlBufferAddHead(xmlBufferPtr buf, const xmlChar *str, int len) {
    unsigned start = 0;

    if ((buf == NULL) || (str == NULL))
	return(XML_ERR_ARGUMENT);
    if (len < 0)
        len = xmlStrlen(str);
    if (len == 0)
        return(XML_ERR_OK);

    if (buf->alloc == XML_BUFFER_ALLOC_IO) {
        start = buf->content - buf->contentIO;

        /*
         * We can add it in the space previously shrunk
         */
        if ((unsigned) len <= start) {
            buf->content -= len;
            memmove(&buf->content[0], str, len);
            buf->use += len;
            buf->size += len;
            return(0);
        }
        if ((unsigned) len < buf->size + start - buf->use) {
            memmove(&buf->contentIO[len], buf->content, buf->use + 1);
            memmove(buf->contentIO, str, len);
            buf->content = buf->contentIO;
            buf->use += len;
            buf->size += start;
            return(0);
        }
    }

    if ((unsigned) len >= buf->size - buf->use) {
        if (xmlBufferGrow(buf, len) < 0)
            return(-1);
    }

    memmove(&buf->content[len], buf->content, buf->use + 1);
    memmove(buf->content, str, len);
    buf->use += len;
    return (0);
}

/**
 * xmlBufferCat:
 * @buf:  the buffer to add to
 * @str:  the #xmlChar string
 *
 * Append a zero terminated string to an XML buffer.
 *
 * Returns 0 successful, a positive error code number otherwise
 *         and -1 in case of internal or API error.
 */
int
xmlBufferCat(xmlBufferPtr buf, const xmlChar *str) {
    return(xmlBufferAdd(buf, str, -1));
}

/**
 * xmlBufferCCat:
 * @buf:  the buffer to dump
 * @str:  the C char string
 *
 * Append a zero terminated C string to an XML buffer.
 *
 * Returns 0 successful, a positive error code number otherwise
 *         and -1 in case of internal or API error.
 */
int
xmlBufferCCat(xmlBufferPtr buf, const char *str) {
    return(xmlBufferAdd(buf, (const xmlChar *) str, -1));
}

/**
 * xmlBufferWriteCHAR:
 * @buf:  the XML buffer
 * @string:  the string to add
 *
 * routine which manages and grows an output buffer. This one adds
 * xmlChars at the end of the buffer.
 */
void
xmlBufferWriteCHAR(xmlBufferPtr buf, const xmlChar *string) {
    xmlBufferAdd(buf, string, -1);
}

/**
 * xmlBufferWriteChar:
 * @buf:  the XML buffer output
 * @string:  the string to add
 *
 * routine which manage and grows an output buffer. This one add
 * C chars at the end of the array.
 */
void
xmlBufferWriteChar(xmlBufferPtr buf, const char *string) {
    xmlBufferAdd(buf, (const xmlChar *) string, -1);
}


/**
 * xmlBufferWriteQuotedString:
 * @buf:  the XML buffer output
 * @string:  the string to add
 *
 * routine which manage and grows an output buffer. This one writes
 * a quoted or double quoted #xmlChar string, checking first if it holds
 * quote or double-quotes internally
 */
void
xmlBufferWriteQuotedString(xmlBufferPtr buf, const xmlChar *string) {
    const xmlChar *cur, *base;
    if (buf == NULL)
        return;
    if (xmlStrchr(string, '\"')) {
        if (xmlStrchr(string, '\'')) {
	    xmlBufferCCat(buf, "\"");
            base = cur = string;
            while(*cur != 0){
                if(*cur == '"'){
                    if (base != cur)
                        xmlBufferAdd(buf, base, cur - base);
                    xmlBufferAdd(buf, BAD_CAST "&quot;", 6);
                    cur++;
                    base = cur;
                }
                else {
                    cur++;
                }
            }
            if (base != cur)
                xmlBufferAdd(buf, base, cur - base);
	    xmlBufferCCat(buf, "\"");
	}
        else{
	    xmlBufferCCat(buf, "\'");
            xmlBufferCat(buf, string);
	    xmlBufferCCat(buf, "\'");
        }
    } else {
        xmlBufferCCat(buf, "\"");
        xmlBufferCat(buf, string);
        xmlBufferCCat(buf, "\"");
    }
}

