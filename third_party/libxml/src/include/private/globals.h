#ifndef XML_GLOBALS_H_PRIVATE__
#define XML_GLOBALS_H_PRIVATE__

XML_HIDDEN void
xmlInitGlobalsInternal(void);
XML_HIDDEN void
xmlCleanupGlobalsInternal(void);

#ifdef LIBXML_THREAD_ENABLED
XML_HIDDEN unsigned *
xmlGetLocalRngState(void);
#endif

#endif /* XML_GLOBALS_H_PRIVATE__ */
