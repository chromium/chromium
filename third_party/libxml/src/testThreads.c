#include "libxml.h"

#include <stdlib.h>
#include <stdio.h>

#if defined(LIBXML_THREAD_ENABLED) && defined(LIBXML_CATALOG_ENABLED)
#include <libxml/globals.h>
#include <libxml/threads.h>
#include <libxml/parser.h>
#include <libxml/catalog.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#elif defined(_WIN32)
#include <windows.h>
#endif
#include <string.h>
#include <assert.h>

#define	MAX_ARGC	20
#define TEST_REPEAT_COUNT 500
#ifdef HAVE_PTHREAD_H
static pthread_t tid[MAX_ARGC];
#elif defined(_WIN32)
static HANDLE tid[MAX_ARGC];
#endif

typedef struct {
    const char *filename;
    int okay;
} xmlThreadParams;

static const char *catalog = "test/threads/complex.xml";
static xmlThreadParams threadParams[] = {
    { "test/threads/abc.xml", 0 },
    { "test/threads/acb.xml", 0 },
    { "test/threads/bac.xml", 0 },
    { "test/threads/bca.xml", 0 },
    { "test/threads/cab.xml", 0 },
    { "test/threads/cba.xml", 0 },
    { "test/threads/invalid.xml", 0 }
};
static const unsigned int num_threads = sizeof(threadParams) /
                                        sizeof(threadParams[0]);

#ifndef xmlDoValidityCheckingDefaultValue
#error xmlDoValidityCheckingDefaultValue is not a macro
#endif
#ifndef xmlGenericErrorContext
#error xmlGenericErrorContext is not a macro
#endif

static void *
thread_specific_data(void *private_data)
{
    xmlDocPtr myDoc;
    xmlThreadParams *params = (xmlThreadParams *) private_data;
    const char *filename = params->filename;
    int okay = 1;

    if (!strcmp(filename, "test/threads/invalid.xml")) {
        xmlDoValidityCheckingDefaultValue = 0;
        xmlGenericErrorContext = stdout;
    } else {
        xmlDoValidityCheckingDefaultValue = 1;
        xmlGenericErrorContext = stderr;
    }
#ifdef LIBXML_SAX1_ENABLED
    myDoc = xmlParseFile(filename);
#else
    myDoc = xmlReadFile(filename, NULL, XML_WITH_CATALOG);
#endif
    if (myDoc) {
        xmlFreeDoc(myDoc);
    } else {
        printf("parse failed\n");
	okay = 0;
    }
    if (!strcmp(filename, "test/threads/invalid.xml")) {
        if (xmlDoValidityCheckingDefaultValue != 0) {
	    printf("ValidityCheckingDefaultValue override failed\n");
	    okay = 0;
	}
        if (xmlGenericErrorContext != stdout) {
	    printf("xmlGenericErrorContext override failed\n");
	    okay = 0;
	}
    } else {
        if (xmlDoValidityCheckingDefaultValue != 1) {
	    printf("ValidityCheckingDefaultValue override failed\n");
	    okay = 0;
	}
        if (xmlGenericErrorContext != stderr) {
	    printf("xmlGenericErrorContext override failed\n");
	    okay = 0;
	}
    }
    params->okay = okay;
    return(NULL);
}

#ifdef HAVE_PTHREAD_H
int
main(void)
{
    unsigned int i, repeat;
    int ret;

    xmlInitParser();
    for (repeat = 0;repeat < TEST_REPEAT_COUNT;repeat++) {
	xmlLoadCatalog(catalog);

        memset(tid, 0xff, sizeof(*tid)*num_threads);

	for (i = 0; i < num_threads; i++) {
	    ret = pthread_create(&tid[i], NULL, thread_specific_data,
				 (void *) &threadParams[i]);
	    if (ret != 0) {
		perror("pthread_create");
		exit(1);
	    }
	}
	for (i = 0; i < num_threads; i++) {
            void *result;
	    ret = pthread_join(tid[i], &result);
	    if (ret != 0) {
		perror("pthread_join");
		exit(1);
	    }
	}

	xmlCatalogCleanup();
	for (i = 0; i < num_threads; i++)
	    if (threadParams[i].okay == 0)
		printf("Thread %d handling %s failed\n", i,
                       threadParams[i].filename);
    }
    xmlCleanupParser();
    xmlMemoryDump();
    return (0);
}
#elif defined(_WIN32)
static DWORD WINAPI
win32_thread_specific_data(void *private_data)
{
    thread_specific_data(private_data);
    return(0);
}

int
main(void)
{
    unsigned int i, repeat;
    BOOL ret;

    xmlInitParser();
    for (repeat = 0;repeat < TEST_REPEAT_COUNT;repeat++)
    {
        xmlLoadCatalog(catalog);

        for (i = 0; i < num_threads; i++)
        {
            tid[i] = (HANDLE) -1;
        }

        for (i = 0; i < num_threads; i++)
        {
            DWORD useless;
            tid[i] = CreateThread(NULL, 0,
                win32_thread_specific_data, &threadParams[i], 0, &useless);
            if (tid[i] == NULL)
            {
                perror("CreateThread");
                exit(1);
            }
        }

        if (WaitForMultipleObjects (num_threads, tid, TRUE, INFINITE) == WAIT_FAILED)
            perror ("WaitForMultipleObjects failed");

        for (i = 0; i < num_threads; i++)
        {
            DWORD exitCode;
            ret = GetExitCodeThread (tid[i], &exitCode);
            if (ret == 0)
            {
                perror("GetExitCodeThread");
                exit(1);
            }
            CloseHandle (tid[i]);
        }

        xmlCatalogCleanup();
        for (i = 0; i < num_threads; i++) {
            if (threadParams[i].okay == 0)
            printf("Thread %d handling %s failed\n", i,
                   threadParams[i].filename);
        }
    }

    xmlCleanupParser();
    xmlMemoryDump();

    return (0);
}
#endif /* pthreads */

#else /* !LIBXML_THREADS_ENABLED */
int
main(void)
{
    fprintf(stderr, "libxml was not compiled with thread or catalog support\n");
    return (0);
}
#endif
