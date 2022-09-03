#ifndef XML_TREE_H_PRIVATE__
#define XML_TREE_H_PRIVATE__

/*
 * Internal variable indicating if a callback has been registered for
 * node creation/destruction. It avoids spending a lot of time in locking
 * function while checking if the callback exists.
 */
extern int __xmlRegisterCallbacks;

#endif /* XML_TREE_H_PRIVATE__ */
