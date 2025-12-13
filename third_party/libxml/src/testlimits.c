/*
 * testlimits.c: C program to run libxml2 regression tests checking various
 *       limits in document size. Will consume a lot of RAM and CPU cycles
 *
 * To compile on Unixes:
 * cc -o testlimits `xml2-config --cflags` testlimits.c `xml2-config --libs` -lpthread
 *
 * See Copyright for the status of this software.
 *
 * Author: Daniel Veillard
 */

#include "libxml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <libxml/catalog.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#ifdef LIBXML_READER_ENABLED
#include <libxml/xmlreader.h>
#endif

static int verbose = 0;
static int tests_quiet = 0;

/************************************************************************
 *									*
 *		time handling                                           *
 *									*
 ************************************************************************/

/* maximum time for one parsing before declaring a timeout */
#define MAX_TIME 2 /* seconds */

static clock_t t0;
static int timeout = 0;

static void reset_timout(void) {
    timeout = 0;
    t0 = clock();
}

static int check_time(void) {
    clock_t tnow = clock();
    if (((tnow - t0) / CLOCKS_PER_SEC) > MAX_TIME) {
        timeout = 1;
        return(0);
    }
    return(1);
}

/************************************************************************
 *									*
 *		Huge document generator					*
 *									*
 ************************************************************************/

#include <libxml/xmlIO.h>

/*
 * Huge documents are built using fixed start and end chunks
 * and filling between the two an unconventional amount of char data
 */
typedef struct hugeTest hugeTest;
typedef hugeTest *hugeTestPtr;
struct hugeTest {
    const char *description;
    const char *name;
    const char *start;
    const char *end;
};

static struct hugeTest hugeTests[] = {
    { "Huge text node", "huge:textNode", "<foo>", "</foo>" },
    { "Huge attribute node", "huge:attrNode", "<foo bar='", "'/>" },
    { "Huge comment node", "huge:commentNode", "<foo><!--", "--></foo>" },
    { "Huge PI node", "huge:piNode", "<foo><?bar ", "?></foo>" },
};

static const char *current;
static int rlen;
static unsigned int currentTest = 0;
static int instate = 0;

/**
 * Check for an huge: query
 *
 * @param URI  an URI to test
 * @returns 1 if yes and 0 if another Input module should be used
 */
static int
hugeMatch(const char * URI) {
    if ((URI != NULL) && (!strncmp(URI, "huge:", 5)))
        return(1);
    return(0);
}

/**
 * Returns a pointer to the huge: query handler, in this example simply
 * the current pointer...
 *
 * @param URI  an URI to test
 * @returns an Input context or NULL in case or error
 */
static void *
hugeOpen(const char * URI) {
    if ((URI == NULL) || (strncmp(URI, "huge:", 5)))
        return(NULL);

    for (currentTest = 0;currentTest < sizeof(hugeTests)/sizeof(hugeTests[0]);
         currentTest++)
         if (!strcmp(hugeTests[currentTest].name, URI))
             goto found;

    return(NULL);

found:
    rlen = strlen(hugeTests[currentTest].start);
    current = hugeTests[currentTest].start;
    instate = 0;
    return((void *) current);
}

/**
 * Close the huge: query handler
 *
 * @param context  the read context
 * @returns 0 or -1 in case of error
 */
static int
hugeClose(void * context) {
    if (context == NULL) return(-1);
    fprintf(stderr, "\n");
    return(0);
}

#define CHUNK 4096

static char filling[CHUNK + 1];

static void fillFilling(void) {
    int i;

    for (i = 0;i < CHUNK;i++) {
        filling[i] = 'a';
    }
    filling[CHUNK] = 0;
}

static size_t maxlen = 64 * 1024 * 1024;
static size_t curlen = 0;
static size_t dotlen;

/**
 * Implement an huge: query read.
 *
 * @param context  the read context
 * @param buffer  where to store data
 * @param len  number of bytes to read
 * @returns the number of bytes read or -1 in case of error
 */
static int
hugeRead(void *context, char *buffer, int len)
{
    if ((context == NULL) || (buffer == NULL) || (len < 0))
        return (-1);

    if (instate == 0) {
        if (len >= rlen) {
            len = rlen;
            rlen = 0;
            memcpy(buffer, current, len);
            instate = 1;
            curlen = 0;
            dotlen = maxlen / 10;
        } else {
            memcpy(buffer, current, len);
            rlen -= len;
            current += len;
        }
    } else if (instate == 2) {
        if (len >= rlen) {
            len = rlen;
            rlen = 0;
            memcpy(buffer, current, len);
            instate = 3;
            curlen = 0;
        } else {
            memcpy(buffer, current, len);
            rlen -= len;
            current += len;
        }
    } else if (instate == 1) {
        if (len > CHUNK) len = CHUNK;
        memcpy(buffer, &filling[0], len);
        curlen += len;
        if (curlen >= maxlen) {
            rlen = strlen(hugeTests[currentTest].end);
            current = hugeTests[currentTest].end;
            instate = 2;
	} else {
            if (curlen > dotlen) {
                fprintf(stderr, ".");
                dotlen += maxlen / 10;
            }
        }
    } else
      len = 0;
    return (len);
}

/************************************************************************
 *									*
 *		Crazy document generator				*
 *									*
 ************************************************************************/

static unsigned int crazy_indx = 0;

static const char *const crazy = "<?xml version='1.0' encoding='UTF-8'?>\
<?tst ?>\
<!-- tst -->\
<!DOCTYPE foo [\
<?tst ?>\
<!-- tst -->\
<!ELEMENT foo (#PCDATA)>\
<!ELEMENT p (#PCDATA|emph)* >\
]>\
<?tst ?>\
<!-- tst -->\
<foo bar='foo'>\
<?tst ?>\
<!-- tst -->\
foo\
<![CDATA[ ]]>\
</foo>\
<?tst ?>\
<!-- tst -->";

/**
 * Check for a crazy: query
 *
 * @param URI  an URI to test
 * @returns 1 if yes and 0 if another Input module should be used
 */
static int
crazyMatch(const char * URI) {
    if ((URI != NULL) && (!strncmp(URI, "crazy:", 6)))
        return(1);
    return(0);
}

/**
 * Returns a pointer to the crazy: query handler, in this example simply
 * the current pointer...
 *
 * @param URI  an URI to test
 * @returns an Input context or NULL in case or error
 */
static void *
crazyOpen(const char * URI) {
    if ((URI == NULL) || (strncmp(URI, "crazy:", 6)))
        return(NULL);

    if (crazy_indx > strlen(crazy))
        return(NULL);
    reset_timout();
    rlen = crazy_indx;
    current = &crazy[0];
    instate = 0;
    return((void *) current);
}

/**
 * Close the crazy: query handler
 *
 * @param context  the read context
 * @returns 0 or -1 in case of error
 */
static int
crazyClose(void * context) {
    if (context == NULL) return(-1);
    return(0);
}


/**
 * Implement an crazy: query read.
 *
 * @param context  the read context
 * @param buffer  where to store data
 * @param len  number of bytes to read
 * @returns the number of bytes read or -1 in case of error
 */
static int
crazyRead(void *context, char *buffer, int len)
{
    if ((context == NULL) || (buffer == NULL) || (len < 0))
        return (-1);

    if ((check_time() <= 0) && (instate == 1)) {
        fprintf(stderr, "\ntimeout in crazy(%d)\n", crazy_indx);
        rlen = strlen(crazy) - crazy_indx;
        current = &crazy[crazy_indx];
        instate = 2;
    }
    if (instate == 0) {
        if (len >= rlen) {
            len = rlen;
            rlen = 0;
            memcpy(buffer, current, len);
            instate = 1;
            curlen = 0;
        } else {
            memcpy(buffer, current, len);
            rlen -= len;
            current += len;
        }
    } else if (instate == 2) {
        if (len >= rlen) {
            len = rlen;
            rlen = 0;
            memcpy(buffer, current, len);
            instate = 3;
            curlen = 0;
        } else {
            memcpy(buffer, current, len);
            rlen -= len;
            current += len;
        }
    } else if (instate == 1) {
        if (len > CHUNK) len = CHUNK;
        memcpy(buffer, &filling[0], len);
        curlen += len;
        if (curlen >= maxlen) {
            rlen = strlen(crazy) - crazy_indx;
            current = &crazy[crazy_indx];
            instate = 2;
        }
    } else
      len = 0;
    return (len);
}
/************************************************************************
 *									*
 *		Libxml2 specific routines				*
 *									*
 ************************************************************************/

static int nb_tests = 0;
static int nb_errors = 0;
static int nb_leaks = 0;

static void
initializeLibxml2(void) {
    xmlMemSetup(xmlMemFree, xmlMemMalloc, xmlMemRealloc, xmlMemoryStrdup);
    xmlInitParser();
#ifdef LIBXML_CATALOG_ENABLED
    xmlInitializeCatalog();
    xmlCatalogSetDefaults(XML_CATA_ALLOW_NONE);
#endif
    /*
     * register the new I/O handlers
     */
    if (xmlRegisterInputCallbacks(hugeMatch, hugeOpen,
                                  hugeRead, hugeClose) < 0) {
        fprintf(stderr, "failed to register Huge handlers\n");
	exit(1);
    }
    if (xmlRegisterInputCallbacks(crazyMatch, crazyOpen,
                                  crazyRead, crazyClose) < 0) {
        fprintf(stderr, "failed to register Crazy handlers\n");
	exit(1);
    }
}

/************************************************************************
 *									*
 *		SAX empty callbacks                                     *
 *									*
 ************************************************************************/

static unsigned long callbacks = 0;

/**
 * Is this document tagged standalone ?
 *
 * @param ctxt  An XML parser context
 * @returns 1 if true
 */
static int
isStandaloneCallback(void *ctx ATTRIBUTE_UNUSED)
{
    callbacks++;
    return (0);
}

/**
 * Does this document has an internal subset
 *
 * @param ctxt  An XML parser context
 * @returns 1 if true
 */
static int
hasInternalSubsetCallback(void *ctx ATTRIBUTE_UNUSED)
{
    callbacks++;
    return (0);
}

/**
 * Does this document has an external subset
 *
 * @param ctxt  An XML parser context
 * @returns 1 if true
 */
static int
hasExternalSubsetCallback(void *ctx ATTRIBUTE_UNUSED)
{
    callbacks++;
    return (0);
}

/**
 * Does this document has an internal subset
 *
 * @param ctxt  An XML parser context
 */
static void
internalSubsetCallback(void *ctx ATTRIBUTE_UNUSED,
                       const xmlChar * name ATTRIBUTE_UNUSED,
                       const xmlChar * ExternalID ATTRIBUTE_UNUSED,
                       const xmlChar * SystemID ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * Does this document has an external subset
 *
 * @param ctxt  An XML parser context
 */
static void
externalSubsetCallback(void *ctx ATTRIBUTE_UNUSED,
                       const xmlChar * name ATTRIBUTE_UNUSED,
                       const xmlChar * ExternalID ATTRIBUTE_UNUSED,
                       const xmlChar * SystemID ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * Special entity resolver, better left to the parser, it has
 * more context than the application layer.
 * The default behaviour is to NOT resolve the entities, in that case
 * the ENTITY_REF nodes are built in the structure (and the parameter
 * values).
 *
 * @param ctxt  An XML parser context
 * @param publicId  The public ID of the entity
 * @param systemId  The system ID of the entity
 * @returns the xmlParserInput if inlined or NULL for DOM behaviour.
 */
static xmlParserInputPtr
resolveEntityCallback(void *ctx ATTRIBUTE_UNUSED,
                      const xmlChar * publicId ATTRIBUTE_UNUSED,
                      const xmlChar * systemId ATTRIBUTE_UNUSED)
{
    callbacks++;
    return (NULL);
}

/**
 * Get an entity by name
 *
 * @param ctxt  An XML parser context
 * @param name  The entity name
 * @returns the xmlParserInput if inlined or NULL for DOM behaviour.
 */
static xmlEntityPtr
getEntityCallback(void *ctx ATTRIBUTE_UNUSED,
                  const xmlChar * name ATTRIBUTE_UNUSED)
{
    callbacks++;
    return (NULL);
}

/**
 * Get a parameter entity by name
 *
 * @param ctxt  An XML parser context
 * @param name  The entity name
 * @returns the xmlParserInput
 */
static xmlEntityPtr
getParameterEntityCallback(void *ctx ATTRIBUTE_UNUSED,
                           const xmlChar * name ATTRIBUTE_UNUSED)
{
    callbacks++;
    return (NULL);
}


/**
 * An entity definition has been parsed
 *
 * @param ctxt  An XML parser context
 * @param name  the entity name
 * @param type  the entity type
 * @param publicId  The public ID of the entity
 * @param systemId  The system ID of the entity
 * @param content  the entity value (without processing).
 */
static void
entityDeclCallback(void *ctx ATTRIBUTE_UNUSED,
                   const xmlChar * name ATTRIBUTE_UNUSED,
                   int type ATTRIBUTE_UNUSED,
                   const xmlChar * publicId ATTRIBUTE_UNUSED,
                   const xmlChar * systemId ATTRIBUTE_UNUSED,
                   xmlChar * content ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * An attribute definition has been parsed
 *
 * @param ctxt  An XML parser context
 * @param name  the attribute name
 * @param type  the attribute type
 */
static void
attributeDeclCallback(void *ctx ATTRIBUTE_UNUSED,
                      const xmlChar * elem ATTRIBUTE_UNUSED,
                      const xmlChar * name ATTRIBUTE_UNUSED,
                      int type ATTRIBUTE_UNUSED, int def ATTRIBUTE_UNUSED,
                      const xmlChar * defaultValue ATTRIBUTE_UNUSED,
                      xmlEnumerationPtr tree ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * An element definition has been parsed
 *
 * @param ctxt  An XML parser context
 * @param name  the element name
 * @param type  the element type
 * @param content  the element value (without processing).
 */
static void
elementDeclCallback(void *ctx ATTRIBUTE_UNUSED,
                    const xmlChar * name ATTRIBUTE_UNUSED,
                    int type ATTRIBUTE_UNUSED,
                    xmlElementContentPtr content ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * What to do when a notation declaration has been parsed.
 *
 * @param ctxt  An XML parser context
 * @param name  The name of the notation
 * @param publicId  The public ID of the entity
 * @param systemId  The system ID of the entity
 */
static void
notationDeclCallback(void *ctx ATTRIBUTE_UNUSED,
                     const xmlChar * name ATTRIBUTE_UNUSED,
                     const xmlChar * publicId ATTRIBUTE_UNUSED,
                     const xmlChar * systemId ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * What to do when an unparsed entity declaration is parsed
 *
 * @param ctxt  An XML parser context
 * @param name  The name of the entity
 * @param publicId  The public ID of the entity
 * @param systemId  The system ID of the entity
 * @param notationName  the name of the notation
 */
static void
unparsedEntityDeclCallback(void *ctx ATTRIBUTE_UNUSED,
                           const xmlChar * name ATTRIBUTE_UNUSED,
                           const xmlChar * publicId ATTRIBUTE_UNUSED,
                           const xmlChar * systemId ATTRIBUTE_UNUSED,
                           const xmlChar * notationName ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * Receive the document locator at startup, actually xmlDefaultSAXLocator
 * Everything is available on the context, so this is useless in our case.
 *
 * @param ctxt  An XML parser context
 * @param loc  A SAX Locator
 */
static void
setDocumentLocatorCallback(void *ctx ATTRIBUTE_UNUSED,
                           xmlSAXLocatorPtr loc ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * called when the document start being processed.
 *
 * @param ctxt  An XML parser context
 */
static void
startDocumentCallback(void *ctx ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * called when the document end has been detected.
 *
 * @param ctxt  An XML parser context
 */
static void
endDocumentCallback(void *ctx ATTRIBUTE_UNUSED)
{
    callbacks++;
}

#if 0
/**
 * called when an opening tag has been processed.
 *
 * @param ctxt  An XML parser context
 * @param name  The element name
 */
static void
startElementCallback(void *ctx ATTRIBUTE_UNUSED,
                     const xmlChar * name ATTRIBUTE_UNUSED,
                     const xmlChar ** atts ATTRIBUTE_UNUSED)
{
    callbacks++;
    return;
}

/**
 * called when the end of an element has been detected.
 *
 * @param ctxt  An XML parser context
 * @param name  The element name
 */
static void
endElementCallback(void *ctx ATTRIBUTE_UNUSED,
                   const xmlChar * name ATTRIBUTE_UNUSED)
{
    callbacks++;
    return;
}
#endif

/**
 * receiving some chars from the parser.
 * Question: how much at a time ???
 *
 * @param ctxt  An XML parser context
 * @param ch  a xmlChar string
 * @param len  the number of xmlChar
 */
static void
charactersCallback(void *ctx ATTRIBUTE_UNUSED,
                   const xmlChar * ch ATTRIBUTE_UNUSED,
                   int len ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * called when an entity reference is detected.
 *
 * @param ctxt  An XML parser context
 * @param name  The entity name
 */
static void
referenceCallback(void *ctx ATTRIBUTE_UNUSED,
                  const xmlChar * name ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * receiving some ignorable whitespaces from the parser.
 * Question: how much at a time ???
 *
 * @param ctxt  An XML parser context
 * @param ch  a xmlChar string
 * @param start  the first char in the string
 * @param len  the number of xmlChar
 */
static void
ignorableWhitespaceCallback(void *ctx ATTRIBUTE_UNUSED,
                            const xmlChar * ch ATTRIBUTE_UNUSED,
                            int len ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * A processing instruction has been parsed.
 *
 * @param ctxt  An XML parser context
 * @param target  the target name
 * @param data  the PI data's
 * @param len  the number of xmlChar
 */
static void
processingInstructionCallback(void *ctx ATTRIBUTE_UNUSED,
                              const xmlChar * target ATTRIBUTE_UNUSED,
                              const xmlChar * data ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * called when a pcdata block has been parsed
 *
 * @param ctx  the user data (XML parser context)
 * @param value  The pcdata content
 * @param len  the block length
 */
static void
cdataBlockCallback(void *ctx ATTRIBUTE_UNUSED,
                   const xmlChar * value ATTRIBUTE_UNUSED,
                   int len ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * A comment has been parsed.
 *
 * @param ctxt  An XML parser context
 * @param value  the comment content
 */
static void
commentCallback(void *ctx ATTRIBUTE_UNUSED,
                const xmlChar * value ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * Display and format a warning messages, gives file, line, position and
 * extra parameters.
 *
 * @param ctxt  An XML parser context
 * @param msg  the message to display/transmit
 * @param ...  extra parameters for the message display
 */
static void
warningCallback(void *ctx ATTRIBUTE_UNUSED,
                const char *msg ATTRIBUTE_UNUSED, ...)
{
    callbacks++;
}

/**
 * Display and format a error messages, gives file, line, position and
 * extra parameters.
 *
 * @param ctxt  An XML parser context
 * @param msg  the message to display/transmit
 * @param ...  extra parameters for the message display
 */
static void
errorCallback(void *ctx ATTRIBUTE_UNUSED, const char *msg ATTRIBUTE_UNUSED,
              ...)
{
    callbacks++;
}

/**
 * Display and format a fatalError messages, gives file, line, position and
 * extra parameters.
 *
 * @param ctxt  An XML parser context
 * @param msg  the message to display/transmit
 * @param ...  extra parameters for the message display
 */
static void
fatalErrorCallback(void *ctx ATTRIBUTE_UNUSED,
                   const char *msg ATTRIBUTE_UNUSED, ...)
{
}


/*
 * SAX2 specific callbacks
 */

/**
 * called when an opening tag has been processed.
 *
 * @param ctxt  An XML parser context
 * @param name  The element name
 */
static void
startElementNsCallback(void *ctx ATTRIBUTE_UNUSED,
                       const xmlChar * localname ATTRIBUTE_UNUSED,
                       const xmlChar * prefix ATTRIBUTE_UNUSED,
                       const xmlChar * URI ATTRIBUTE_UNUSED,
                       int nb_namespaces ATTRIBUTE_UNUSED,
                       const xmlChar ** namespaces ATTRIBUTE_UNUSED,
                       int nb_attributes ATTRIBUTE_UNUSED,
                       int nb_defaulted ATTRIBUTE_UNUSED,
                       const xmlChar ** attributes ATTRIBUTE_UNUSED)
{
    callbacks++;
}

/**
 * called when the end of an element has been detected.
 *
 * @param ctxt  An XML parser context
 * @param name  The element name
 */
static void
endElementNsCallback(void *ctx ATTRIBUTE_UNUSED,
                     const xmlChar * localname ATTRIBUTE_UNUSED,
                     const xmlChar * prefix ATTRIBUTE_UNUSED,
                     const xmlChar * URI ATTRIBUTE_UNUSED)
{
    callbacks++;
}

static xmlSAXHandler callbackSAX2HandlerStruct = {
    internalSubsetCallback,
    isStandaloneCallback,
    hasInternalSubsetCallback,
    hasExternalSubsetCallback,
    resolveEntityCallback,
    getEntityCallback,
    entityDeclCallback,
    notationDeclCallback,
    attributeDeclCallback,
    elementDeclCallback,
    unparsedEntityDeclCallback,
    setDocumentLocatorCallback,
    startDocumentCallback,
    endDocumentCallback,
    NULL,
    NULL,
    referenceCallback,
    charactersCallback,
    ignorableWhitespaceCallback,
    processingInstructionCallback,
    commentCallback,
    warningCallback,
    errorCallback,
    fatalErrorCallback,
    getParameterEntityCallback,
    cdataBlockCallback,
    externalSubsetCallback,
    XML_SAX2_MAGIC,
    NULL,
    startElementNsCallback,
    endElementNsCallback,
    NULL
};

static xmlSAXHandlerPtr callbackSAX2Handler = &callbackSAX2HandlerStruct;

/************************************************************************
 *									*
 *		The tests front-ends                                     *
 *									*
 ************************************************************************/

/**
 * Parse a memory generated file using SAX
 *
 * @param filename  the file to parse
 * @param max_size  size of the limit to test
 * @param options  parsing options
 * @param fail  should a failure be reported
 * @returns 0 in case of success, an error code otherwise
 */
static int
saxTest(const char *filename, size_t limit, int options, int fail) {
    int res = 0;
    xmlParserCtxtPtr ctxt;
    xmlDocPtr doc;

    nb_tests++;

    maxlen = limit;
    ctxt = xmlNewSAXParserCtxt(callbackSAX2Handler, NULL);
    if (ctxt == NULL) {
        fprintf(stderr, "Failed to create parser context\n");
	return(1);
    }
    doc = xmlCtxtReadFile(ctxt, filename, NULL, options | XML_PARSE_NOERROR);

    if (doc != NULL) {
        fprintf(stderr, "SAX parsing generated a document !\n");
        xmlFreeDoc(doc);
        res = 0;
    } else if (ctxt->wellFormed == 0) {
        if (fail)
            res = 0;
        else {
            fprintf(stderr, "Failed to parse '%s' %lu\n", filename,
                    (unsigned long) limit);
            res = 1;
        }
    } else {
        if (fail) {
            fprintf(stderr, "Failed to get failure for '%s' %lu\n",
                    filename, (unsigned long) limit);
            res = 1;
        } else
            res = 0;
    }
    xmlFreeParserCtxt(ctxt);

    return(res);
}
#ifdef LIBXML_READER_ENABLED
/**
 * Parse a memory generated file using the xmlReader
 *
 * @param filename  the file to parse
 * @param max_size  size of the limit to test
 * @param options  parsing options
 * @param fail  should a failure be reported
 * @returns 0 in case of success, an error code otherwise
 */
static int
readerTest(const char *filename, size_t limit, int options, int fail) {
    xmlTextReaderPtr reader;
    int res = 0;
    int ret;

    nb_tests++;

    maxlen = limit;
    reader = xmlReaderForFile(filename , NULL, options | XML_PARSE_NOERROR);
    if (reader == NULL) {
        fprintf(stderr, "Failed to open '%s' test\n", filename);
	return(1);
    }
    ret = xmlTextReaderRead(reader);
    while (ret == 1) {
        ret = xmlTextReaderRead(reader);
    }
    if (ret != 0) {
        if (fail)
            res = 0;
        else {
            if (strncmp(filename, "crazy:", 6) == 0)
                fprintf(stderr, "Failed to parse '%s' %u\n",
                        filename, crazy_indx);
            else
                fprintf(stderr, "Failed to parse '%s' %lu\n",
                        filename, (unsigned long) limit);
            res = 1;
        }
    } else {
        if (fail) {
            if (strncmp(filename, "crazy:", 6) == 0)
                fprintf(stderr, "Failed to get failure for '%s' %u\n",
                        filename, crazy_indx);
            else
                fprintf(stderr, "Failed to get failure for '%s' %lu\n",
                        filename, (unsigned long) limit);
            res = 1;
        } else
            res = 0;
    }
    if (timeout)
        res = 1;
    xmlFreeTextReader(reader);

    return(res);
}
#endif

/************************************************************************
 *									*
 *			Tests descriptions				*
 *									*
 ************************************************************************/

typedef int (*functest) (const char *filename, size_t limit, int options,
                         int fail);

typedef struct limitDesc limitDesc;
typedef limitDesc *limitDescPtr;
struct limitDesc {
    const char *name; /* the huge generator name */
    size_t limit;     /* the limit to test */
    int options;      /* extra parser options */
    int fail;         /* whether the test should fail */
};

static limitDesc limitDescriptions[] = {
    /* max length of a text node in content */
    {"huge:textNode", XML_MAX_TEXT_LENGTH - CHUNK, 0, 0},
    {"huge:textNode", XML_MAX_TEXT_LENGTH + CHUNK, 0, 1},
    {"huge:textNode", XML_MAX_TEXT_LENGTH + CHUNK, XML_PARSE_HUGE, 0},
    /* max length of a text node in content */
    {"huge:attrNode", XML_MAX_TEXT_LENGTH - CHUNK, 0, 0},
    {"huge:attrNode", XML_MAX_TEXT_LENGTH + CHUNK, 0, 1},
    {"huge:attrNode", XML_MAX_TEXT_LENGTH + CHUNK, XML_PARSE_HUGE, 0},
    /* max length of a comment node */
    {"huge:commentNode", XML_MAX_TEXT_LENGTH - CHUNK, 0, 0},
    {"huge:commentNode", XML_MAX_TEXT_LENGTH + CHUNK, 0, 1},
    {"huge:commentNode", XML_MAX_TEXT_LENGTH + CHUNK, XML_PARSE_HUGE, 0},
    /* max length of a PI node */
    {"huge:piNode", XML_MAX_TEXT_LENGTH - CHUNK, 0, 0},
    {"huge:piNode", XML_MAX_TEXT_LENGTH + CHUNK, 0, 1},
    {"huge:piNode", XML_MAX_TEXT_LENGTH + CHUNK, XML_PARSE_HUGE, 0},
};

typedef struct testDesc testDesc;
typedef testDesc *testDescPtr;
struct testDesc {
    const char *desc; /* description of the test */
    functest    func; /* function implementing the test */
};

static
testDesc testDescriptions[] = {
    { "Parsing of huge files with the sax parser", saxTest},
/*    { "Parsing of huge files with the tree parser", treeTest}, */
#ifdef LIBXML_READER_ENABLED
    { "Parsing of huge files with the reader", readerTest},
#endif
    {NULL, NULL}
};

typedef struct testException testException;
typedef testException *testExceptionPtr;
struct testException {
    unsigned int test;  /* the parser test number */
    unsigned int limit; /* the limit test number */
    int fail;           /* new fail value or -1*/
    size_t size;        /* new limit value or 0 */
};

static
testException testExceptions[] = {
    /* the SAX parser doesn't hit a limit of XML_MAX_TEXT_LENGTH text nodes */
    { 0, 1, 0, 0},
};

static int
launchTests(testDescPtr tst, unsigned int test) {
    int res = 0, err = 0;
    unsigned int i, j;
    size_t limit;
    int fail;

    if (tst == NULL) return(-1);

    for (i = 0;i < sizeof(limitDescriptions)/sizeof(limitDescriptions[0]);i++) {
        limit = limitDescriptions[i].limit;
        fail = limitDescriptions[i].fail;
        /*
         * Handle exceptions if any
         */
        for (j = 0;j < sizeof(testExceptions)/sizeof(testExceptions[0]);j++) {
            if ((testExceptions[j].test == test) &&
                (testExceptions[j].limit == i)) {
                if (testExceptions[j].fail != -1)
                    fail = testExceptions[j].fail;
                if (testExceptions[j].size != 0)
                    limit = testExceptions[j].size;
                break;
            }
        }
        res = tst->func(limitDescriptions[i].name, limit,
                        limitDescriptions[i].options, fail);
        if (res != 0) {
            nb_errors++;
            err++;
        }
    }
    return(err);
}


static int
runtest(unsigned int i) {
    int ret = 0, res;
    int old_errors, old_tests, old_leaks;

    old_errors = nb_errors;
    old_tests = nb_tests;
    old_leaks = nb_leaks;
    if ((tests_quiet == 0) && (testDescriptions[i].desc != NULL))
	printf("## %s\n", testDescriptions[i].desc);
    res = launchTests(&testDescriptions[i], i);
    if (res != 0)
	ret++;
    if (verbose) {
	if ((nb_errors == old_errors) && (nb_leaks == old_leaks))
	    printf("Ran %d tests, no errors\n", nb_tests - old_tests);
	else
	    printf("Ran %d tests, %d errors, %d leaks\n",
		   nb_tests - old_tests,
		   nb_errors - old_errors,
		   nb_leaks - old_leaks);
    }
    return(ret);
}

static int
launchCrazySAX(unsigned int test, int fail) {
    int res = 0, err = 0;

    crazy_indx = test;

    res = saxTest("crazy::test", XML_MAX_LOOKUP_LIMIT - CHUNK, 0, fail);
    if (res != 0) {
        nb_errors++;
        err++;
    }
    if (tests_quiet == 0)
        fprintf(stderr, "%c", crazy[test]);

    return(err);
}

#ifdef LIBXML_READER_ENABLED
static int
launchCrazy(unsigned int test, int fail) {
    int res = 0, err = 0;

    crazy_indx = test;

    res = readerTest("crazy::test", XML_MAX_LOOKUP_LIMIT - CHUNK, 0, fail);
    if (res != 0) {
        nb_errors++;
        err++;
    }
    if (tests_quiet == 0)
        fprintf(stderr, "%c", crazy[test]);

    return(err);
}
#endif

static int get_crazy_fail(int test) {
    /*
     * adding 1000000 of character 'a' leads to parser failure mostly
     * everywhere except in those special spots. Need to be updated
     * each time crazy is updated
     */
    int fail = 1;
    if ((test == 44) || /* PI in Misc */
        ((test >= 50) && (test <= 55)) || /* Comment in Misc */
        (test == 79) || /* PI in DTD */
        ((test >= 85) && (test <= 90)) || /* Comment in DTD */
        (test == 154) || /* PI in Misc */
        ((test >= 160) && (test <= 165)) || /* Comment in Misc */
        ((test >= 178) && (test <= 181)) || /* attribute value */
        (test == 183) || /* Text */
        (test == 189) || /* PI in Content */
        (test == 191) || /* Text */
        ((test >= 195) && (test <= 200)) || /* Comment in Content */
        ((test >= 203) && (test <= 206)) || /* Text */
        (test == 215) || (test == 216) || /* in CDATA */
        (test == 219) || /* Text */
        (test == 231) || /* PI in Misc */
        ((test >= 237) && (test <= 242))) /* Comment in Misc */
        fail = 0;
    return(fail);
}

static int
runcrazy(void) {
    int ret = 0, res = 0;
    int old_errors, old_tests, old_leaks;
    unsigned int i;

    old_errors = nb_errors;
    old_tests = nb_tests;
    old_leaks = nb_leaks;

#ifdef LIBXML_READER_ENABLED
    if (tests_quiet == 0) {
	printf("## Crazy tests on reader\n");
    }
    for (i = 0;i < strlen(crazy);i++) {
        res += launchCrazy(i, get_crazy_fail(i));
        if (res != 0)
            ret++;
    }
#endif

    if (tests_quiet == 0) {
	printf("\n## Crazy tests on SAX\n");
    }
    for (i = 0;i < strlen(crazy);i++) {
        res += launchCrazySAX(i, get_crazy_fail(i));
        if (res != 0)
            ret++;
    }
    if (tests_quiet == 0)
        fprintf(stderr, "\n");
    if (verbose) {
	if ((nb_errors == old_errors) && (nb_leaks == old_leaks))
	    printf("Ran %d tests, no errors\n", nb_tests - old_tests);
	else
	    printf("Ran %d tests, %d errors, %d leaks\n",
		   nb_tests - old_tests,
		   nb_errors - old_errors,
		   nb_leaks - old_leaks);
    }
    return(ret);
}


int
main(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED) {
    int i, a, ret = 0;
    int subset = 0;

    fillFilling();
    initializeLibxml2();

    for (a = 1; a < argc;a++) {
        if (!strcmp(argv[a], "-v"))
	    verbose = 1;
        else if (!strcmp(argv[a], "-quiet"))
	    tests_quiet = 1;
        else if (!strcmp(argv[a], "-crazy"))
	    subset = 1;
    }
    if (subset == 0) {
	for (i = 0; testDescriptions[i].func != NULL; i++) {
	    ret += runtest(i);
	}
    }
    ret += runcrazy();
    if ((nb_errors == 0) && (nb_leaks == 0)) {
        ret = 0;
	printf("Total %d tests, no errors\n",
	       nb_tests);
    } else {
        ret = 1;
	printf("Total %d tests, %d errors, %d leaks\n",
	       nb_tests, nb_errors, nb_leaks);
    }
    xmlCleanupParser();

    return(ret);
}
