/*
 * xmlSeed.c: Generate the XML seed corpus for fuzzing.
 *
 * See Copyright for the status of this software.
 */

#include <stdio.h>
#include <string.h>
#include <glob.h>
#include <libgen.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/HTMLparser.h>
#include <libxml/xinclude.h>
#include <libxml/xmlschemas.h>
#include "fuzz.h"

#define PATH_SIZE 500
#define SEED_BUF_SIZE 16384
#define EXPR_SIZE 4500

typedef int
(*fileFunc)(const char *base, FILE *out);

typedef int
(*mainFunc)(const char *arg);

static struct {
    FILE *out;
    xmlHashTablePtr entities; /* Maps URLs to xmlFuzzEntityInfos */
    xmlExternalEntityLoader oldLoader;
    fileFunc processFile;
    const char *fuzzer;
    int counter;
    char cwd[PATH_SIZE];
} globalData;

/*
 * A custom entity loader that writes all external DTDs or entities to a
 * single file in the format expected by xmlFuzzEntityLoader.
 */
static xmlParserInputPtr
fuzzEntityRecorder(const char *URL, const char *ID,
                      xmlParserCtxtPtr ctxt) {
    xmlParserInputPtr in;
    static const int chunkSize = 16384;
    int len;

    in = xmlNoNetExternalEntityLoader(URL, ID, ctxt);
    if (in == NULL)
        return(NULL);

    if (globalData.entities == NULL) {
        globalData.entities = xmlHashCreate(4);
    } else if (xmlHashLookup(globalData.entities,
                             (const xmlChar *) URL) != NULL) {
        return(in);
    }

    do {
        len = xmlParserInputBufferGrow(in->buf, chunkSize);
        if (len < 0) {
            fprintf(stderr, "Error reading %s\n", URL);
            xmlFreeInputStream(in);
            return(NULL);
        }
    } while (len > 0);

    xmlFuzzWriteString(globalData.out, URL);
    xmlFuzzWriteString(globalData.out,
                       (char *) xmlBufContent(in->buf->buffer));

    xmlFreeInputStream(in);

    xmlHashAddEntry(globalData.entities, (const xmlChar *) URL, NULL);

    return(xmlNoNetExternalEntityLoader(URL, ID, ctxt));
}

static void
fuzzRecorderInit(FILE *out) {
    globalData.out = out;
    globalData.entities = xmlHashCreate(8);
    globalData.oldLoader = xmlGetExternalEntityLoader();
    xmlSetExternalEntityLoader(fuzzEntityRecorder);
}

static void
fuzzRecorderCleanup() {
    xmlSetExternalEntityLoader(globalData.oldLoader);
    xmlHashFree(globalData.entities, xmlHashDefaultDeallocator);
    globalData.out = NULL;
    globalData.entities = NULL;
    globalData.oldLoader = NULL;
}

#ifdef HAVE_XML_FUZZER
static int
processXml(const char *docFile, FILE *out) {
    int opts = XML_PARSE_NOENT | XML_PARSE_DTDLOAD;
    xmlDocPtr doc;

    fwrite(&opts, sizeof(opts), 1, out);

    fuzzRecorderInit(out);

    doc = xmlReadFile(docFile, NULL, opts);
    xmlXIncludeProcessFlags(doc, opts);
    xmlFreeDoc(doc);

    fuzzRecorderCleanup();

    return(0);
}
#endif

#ifdef HAVE_HTML_FUZZER
static int
processHtml(const char *docFile, FILE *out) {
    char buf[SEED_BUF_SIZE];
    FILE *file;
    size_t size;
    int opts = 0;

    fwrite(&opts, sizeof(opts), 1, out);

    /* Copy file */
    file = fopen(docFile, "rb");
    if (file == NULL) {
        fprintf(stderr, "couldn't open %s\n", docFile);
        return(0);
    }
    do {
        size = fread(buf, 1, SEED_BUF_SIZE, file);
        if (size > 0)
            fwrite(buf, 1, size, out);
    } while (size == SEED_BUF_SIZE);
    fclose(file);

    return(0);
}
#endif

#ifdef HAVE_SCHEMA_FUZZER
static int
processSchema(const char *docFile, FILE *out) {
    xmlSchemaPtr schema;
    xmlSchemaParserCtxtPtr pctxt;

    fuzzRecorderInit(out);

    pctxt = xmlSchemaNewParserCtxt(docFile);
    xmlSchemaSetParserErrors(pctxt, xmlFuzzErrorFunc, xmlFuzzErrorFunc, NULL);
    schema = xmlSchemaParse(pctxt);
    xmlSchemaFreeParserCtxt(pctxt);
    xmlSchemaFree(schema);

    fuzzRecorderCleanup();

    return(0);
}
#endif

static int
processPattern(const char *pattern) {
    glob_t globbuf;
    int ret = 0;
    int res, i;

    res = glob(pattern, 0, NULL, &globbuf);
    if (res == GLOB_NOMATCH)
        return(0);
    if (res != 0) {
        fprintf(stderr, "couldn't match pattern %s\n", pattern);
        return(-1);
    }

    for (i = 0; i < globbuf.gl_pathc; i++) {
        struct stat statbuf;
        char outPath[PATH_SIZE];
        char *dirBuf = NULL;
        char *baseBuf = NULL;
        const char *path, *dir, *base;
        FILE *out = NULL;
        int dirChanged = 0;
        size_t size;

        path = globbuf.gl_pathv[i];

        if ((stat(path, &statbuf) != 0) || (!S_ISREG(statbuf.st_mode)))
            continue;

        dirBuf = (char *) xmlCharStrdup(path);
        baseBuf = (char *) xmlCharStrdup(path);
        if ((dirBuf == NULL) || (baseBuf == NULL)) {
            fprintf(stderr, "memory allocation failed\n");
            ret = -1;
            goto error;
        }
        dir = dirname(dirBuf);
        base = basename(baseBuf);

        size = snprintf(outPath, sizeof(outPath), "seed/%s/%s",
                        globalData.fuzzer, base);
        if (size >= PATH_SIZE) {
            fprintf(stderr, "creating path failed\n");
            ret = -1;
            goto error;
        }
        out = fopen(outPath, "wb");
        if (out == NULL) {
            fprintf(stderr, "couldn't open %s for writing\n", outPath);
            ret = -1;
            goto error;
        }
        if (chdir(dir) != 0) {
            fprintf(stderr, "couldn't chdir to %s\n", dir);
            ret = -1;
            goto error;
        }
        dirChanged = 1;
        if (globalData.processFile(base, out) != 0)
            ret = -1;

error:
        if (out != NULL)
            fclose(out);
        xmlFree(dirBuf);
        xmlFree(baseBuf);
        if ((dirChanged) && (chdir(globalData.cwd) != 0)) {
            fprintf(stderr, "couldn't chdir to %s\n", globalData.cwd);
            ret = -1;
            break;
        }
    }

    globfree(&globbuf);
    return(ret);
}

#ifdef HAVE_XPATH_FUZZER
static int
processXPath(const char *testDir, const char *prefix, const char *name,
             const char *data, const char *subdir, int xptr) {
    char pattern[PATH_SIZE];
    glob_t globbuf;
    size_t i, size;
    int ret = 0, res;

    size = snprintf(pattern, sizeof(pattern), "%s/%s/%s*",
                    testDir, subdir, prefix);
    if (size >= PATH_SIZE)
        return(-1);
    res = glob(pattern, 0, NULL, &globbuf);
    if (res == GLOB_NOMATCH)
        return(0);
    if (res != 0) {
        fprintf(stderr, "couldn't match pattern %s\n", pattern);
        return(-1);
    }

    for (i = 0; i < globbuf.gl_pathc; i++) {
        char *path = globbuf.gl_pathv[i];
        struct stat statbuf;
        FILE *in;
        char expr[EXPR_SIZE];

        if ((stat(path, &statbuf) != 0) || (!S_ISREG(statbuf.st_mode)))
            continue;

        in = fopen(path, "rb");
        if (in == NULL) {
            ret = -1;
            continue;
        }

        while (fgets(expr, EXPR_SIZE, in) > 0) {
            char outPath[PATH_SIZE];
            FILE *out;
            int j;

            for (j = 0; expr[j] != 0; j++)
                if (expr[j] == '\r' || expr[j] == '\n')
                    break;
            expr[j] = 0;

            size = snprintf(outPath, sizeof(outPath), "seed/xpath/%s-%d",
                            name, globalData.counter);
            if (size >= PATH_SIZE) {
                ret = -1;
                continue;
            }
            out = fopen(outPath, "wb");
            if (out == NULL) {
                ret = -1;
                continue;
            }

            if (xptr) {
                xmlFuzzWriteString(out, expr);
            } else {
                char xptrExpr[EXPR_SIZE+100];

                /* Wrap XPath expressions as XPointer */
                snprintf(xptrExpr, sizeof(xptrExpr), "xpointer(%s)", expr);
                xmlFuzzWriteString(out, xptrExpr);
            }

            xmlFuzzWriteString(out, data);

            fclose(out);
            globalData.counter++;
        }

        fclose(in);
    }

    globfree(&globbuf);

    return(ret);
}

int
processXPathDir(const char *testDir) {
    char pattern[PATH_SIZE];
    glob_t globbuf;
    size_t i, size;
    int ret = 0;

    globalData.counter = 1;
    if (processXPath(testDir, "", "expr", "<d></d>", "expr", 0) != 0)
        ret = -1;

    size = snprintf(pattern, sizeof(pattern), "%s/docs/*", testDir);
    if (size >= PATH_SIZE)
        return(1);
    if (glob(pattern, 0, NULL, &globbuf) != 0)
        return(1);

    for (i = 0; i < globbuf.gl_pathc; i++) {
        char *path = globbuf.gl_pathv[i];
        char *data;
        const char *docFile;

        data = xmlSlurpFile(path, NULL);
        if (data == NULL) {
            ret = -1;
            continue;
        }
        docFile = basename(path);

        globalData.counter = 1;
        if (processXPath(testDir, docFile, docFile, data, "tests", 0) != 0)
            ret = -1;
        if (processXPath(testDir, docFile, docFile, data, "xptr", 1) != 0)
            ret = -1;

        xmlFree(data);
    }

    globfree(&globbuf);

    return(ret);
}
#endif

int
main(int argc, const char **argv) {
    mainFunc processArg = NULL;
    const char *fuzzer;
    int ret = 0;
    int xpath = 0;
    int i;

    if (argc < 3) {
        fprintf(stderr, "usage: seed [FUZZER] [PATTERN...]\n");
        return(1);
    }

    xmlSetGenericErrorFunc(NULL, xmlFuzzErrorFunc);

    fuzzer = argv[1];
    if (strcmp(fuzzer, "html") == 0) {
#ifdef HAVE_HTML_FUZZER
        processArg = processPattern;
        globalData.processFile = processHtml;
#endif
    } else if (strcmp(fuzzer, "schema") == 0) {
#ifdef HAVE_SCHEMA_FUZZER
        processArg = processPattern;
        globalData.processFile = processSchema;
#endif
    } else if (strcmp(fuzzer, "xml") == 0) {
#ifdef HAVE_XML_FUZZER
        processArg = processPattern;
        globalData.processFile = processXml;
#endif
    } else if (strcmp(fuzzer, "xpath") == 0) {
#ifdef HAVE_XPATH_FUZZER
        processArg = processXPathDir;
#endif
    } else {
        fprintf(stderr, "unknown fuzzer %s\n", fuzzer);
        return(1);
    }
    globalData.fuzzer = fuzzer;

    if (getcwd(globalData.cwd, PATH_SIZE) == NULL) {
        fprintf(stderr, "couldn't get current directory\n");
        return(1);
    }

    if (processArg != NULL)
        for (i = 2; i < argc; i++)
            processArg(argv[i]);

    return(ret);
}

