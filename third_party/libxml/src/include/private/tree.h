#ifndef XML_TREE_H_PRIVATE__
#define XML_TREE_H_PRIVATE__

/*
 * Internal variable indicating if a callback has been registered for
 * node creation/destruction. It avoids spending a lot of time in locking
 * function while checking if the callback exists.
 */
XML_HIDDEN extern int
__xmlRegisterCallbacks;

XML_HIDDEN xmlNodePtr
xmlStaticCopyNode(xmlNodePtr node, xmlDocPtr doc, xmlNodePtr parent,
                  int extended);
XML_HIDDEN xmlNodePtr
xmlStaticCopyNodeList(xmlNodePtr node, xmlDocPtr doc, xmlNodePtr parent);

#endif /* XML_TREE_H_PRIVATE__ */
