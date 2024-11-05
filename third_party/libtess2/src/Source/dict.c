/*
** SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008) 
** Copyright (C) [dates of first publication] Silicon Graphics, Inc.
** All Rights Reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
** of the Software, and to permit persons to whom the Software is furnished to do so,
** subject to the following conditions:
** 
** The above copyright notice including the dates of first publication and either this
** permission notice or a reference to http://oss.sgi.com/projects/FreeB/ shall be
** included in all copies or substantial portions of the Software. 
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
** INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
** PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL SILICON GRAPHICS, INC.
** BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
** OR OTHER DEALINGS IN THE SOFTWARE.
** 
** Except as contained in this notice, the name of Silicon Graphics, Inc. shall not
** be used in advertising or otherwise to promote the sale, use or other dealings in
** this Software without prior written authorization from Silicon Graphics, Inc.
*/
/*
** Author: Eric Veach, July 1994.
*/

#include <stddef.h>
#include "../Include/tesselator.h"
#include "bucketalloc.h"
#include "dict.h"

/* really tessDictListNewDict */
Dict *dictNewDict( TESSalloc* alloc, void *frame, int (*leq)(void *frame, DictKey key1, DictKey key2) )
{
	Dict *dict = (Dict *)alloc->memalloc( alloc->userData, sizeof( Dict ));
	DictNode *head;

	if (dict == NULL) return NULL;

	head = &dict->head;

	head->key = NULL;
	head->next = head;
	head->prev = head;

	dict->frame = frame;
	dict->leq = leq;

	if (alloc->dictNodeBucketSize < 16)
		alloc->dictNodeBucketSize = 16;
	if (alloc->dictNodeBucketSize > 4096)
		alloc->dictNodeBucketSize = 4096;
	dict->nodePool = createBucketAlloc( alloc, "Dict", sizeof(DictNode), alloc->dictNodeBucketSize );

	return dict;
}

/* really tessDictListDeleteDict */
void dictDeleteDict( TESSalloc* alloc, Dict *dict )
{
	deleteBucketAlloc( dict->nodePool );
	alloc->memfree( alloc->userData, dict );
}

/* really tessDictListInsertBefore */
DictNode *dictInsertBefore( Dict *dict, DictNode *node, DictKey key )
{
	DictNode *newNode;

	do {
		node = node->prev;
	} while( node->key != NULL && ! (*dict->leq)(dict->frame, node->key, key));

	newNode = (DictNode *)bucketAlloc( dict->nodePool );
	if (newNode == NULL) return NULL;

	newNode->key = key;
	newNode->next = node->next;
	node->next->prev = newNode;
	newNode->prev = node;
	node->next = newNode;

	return newNode;
}

/* really tessDictListDelete */
void dictDelete( Dict *dict, DictNode *node ) /*ARGSUSED*/
{
	node->next->prev = node->prev;
	node->prev->next = node->next;
	bucketFree( dict->nodePool, node );
}

/* really tessDictListSearch */
DictNode *dictSearch( Dict *dict, DictKey key )
{
	DictNode *node = &dict->head;

	do {
		node = node->next;
	} while( node->key != NULL && ! (*dict->leq)(dict->frame, key, node->key));

	return node;
}
