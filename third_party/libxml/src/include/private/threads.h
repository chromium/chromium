#ifndef XML_THREADS_H_PRIVATE__
#define XML_THREADS_H_PRIVATE__

#include <libxml/threads.h>

#ifdef LIBXML_THREAD_ENABLED
  #ifdef HAVE_PTHREAD_H
    #include <pthread.h>
    #define HAVE_POSIX_THREADS
  #elif defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #define HAVE_WIN32_THREADS
  #endif
#endif

/*
 * xmlMutex are a simple mutual exception locks
 */
struct _xmlMutex {
#ifdef HAVE_POSIX_THREADS
    pthread_mutex_t lock;
#elif defined HAVE_WIN32_THREADS
    CRITICAL_SECTION cs;
#else
    int empty;
#endif
};

XML_HIDDEN void
xmlInitMutex(xmlMutexPtr mutex);
XML_HIDDEN void
xmlCleanupMutex(xmlMutexPtr mutex);

#endif /* XML_THREADS_H_PRIVATE__ */
