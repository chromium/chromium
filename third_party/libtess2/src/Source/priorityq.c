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

//#include "tesos.h"
#include <stddef.h>
#include <assert.h>
#include "../Include/tesselator.h"
#include "priorityq.h"


#define INIT_SIZE	32

#define TRUE 1
#define FALSE 0

#ifdef FOR_TRITE_TEST_PROGRAM
#define LEQ(x,y)	(*pq->leq)(x,y)
#else
/* Violates modularity, but a little faster */
#include "geom.h"
#define LEQ(x,y)	VertLeq((TESSvertex *)x, (TESSvertex *)y)
#endif


/* Include all the code for the regular heap-based queue here. */

/* The basic operations are insertion of a new key (pqInsert),
* and examination/extraction of a key whose value is minimum
* (pqMinimum/pqExtractMin).  Deletion is also allowed (pqDelete);
* for this purpose pqInsert returns a "handle" which is supplied
* as the argument.
*
* An initial heap may be created efficiently by calling pqInsert
* repeatedly, then calling pqInit.  In any case pqInit must be called
* before any operations other than pqInsert are used.
*
* If the heap is empty, pqMinimum/pqExtractMin will return a NULL key.
* This may also be tested with pqIsEmpty.
*/


/* Since we support deletion the data structure is a little more
* complicated than an ordinary heap.  "nodes" is the heap itself;
* active nodes are stored in the range 1..pq->size.  When the
* heap exceeds its allocated size (pq->max), its size doubles.
* The children of node i are nodes 2i and 2i+1.
*
* Each node stores an index into an array "handles".  Each handle
* stores a key, plus a pointer back to the node which currently
* represents that key (ie. nodes[handles[i].node].handle == i).
*/


#define pqHeapMinimum(pq)	((pq)->handles[(pq)->nodes[1].handle].key)
#define pqHeapIsEmpty(pq)	((pq)->size == 0)



/* really pqHeapNewPriorityQHeap */
PriorityQHeap *pqHeapNewPriorityQ( TESSalloc* alloc, int size, int (*leq)(PQkey key1, PQkey key2) )
{
	PriorityQHeap *pq = (PriorityQHeap *)alloc->memalloc( alloc->userData, sizeof( PriorityQHeap ));
	if (pq == NULL) return NULL;

	pq->size = 0;
	pq->max = size;
	pq->nodes = (PQnode *)alloc->memalloc( alloc->userData, (size + 1) * sizeof(pq->nodes[0]) );
	if (pq->nodes == NULL) {
		alloc->memfree( alloc->userData, pq );
		return NULL;
	}

	pq->handles = (PQhandleElem *)alloc->memalloc( alloc->userData, (size + 1) * sizeof(pq->handles[0]) );
	if (pq->handles == NULL) {
		alloc->memfree( alloc->userData, pq->nodes );
		alloc->memfree( alloc->userData, pq );
		return NULL;
	}

	pq->initialized = FALSE;
	pq->freeList = 0;
	pq->leq = leq;

	pq->nodes[1].handle = 1;	/* so that Minimum() returns NULL */
	pq->handles[1].key = NULL;
	return pq;
}

/* really pqHeapDeletePriorityQHeap */
void pqHeapDeletePriorityQ( TESSalloc* alloc, PriorityQHeap *pq )
{
	alloc->memfree( alloc->userData, pq->handles );
	alloc->memfree( alloc->userData, pq->nodes );
	alloc->memfree( alloc->userData, pq );
}


static void FloatDown( PriorityQHeap *pq, int curr )
{
	PQnode *n = pq->nodes;
	PQhandleElem *h = pq->handles;
	PQhandle hCurr, hChild;
	int child;

	hCurr = n[curr].handle;
	for( ;; ) {
		child = curr << 1;
		if( child < pq->size && LEQ( h[n[child+1].handle].key,
			h[n[child].handle].key )) {
				++child;
		}

		assert(child <= pq->max);

		hChild = n[child].handle;
		if( child > pq->size || LEQ( h[hCurr].key, h[hChild].key )) {
			n[curr].handle = hCurr;
			h[hCurr].node = curr;
			break;
		}
		n[curr].handle = hChild;
		h[hChild].node = curr;
		curr = child;
	}
}


static void FloatUp( PriorityQHeap *pq, int curr )
{
	PQnode *n = pq->nodes;
	PQhandleElem *h = pq->handles;
	PQhandle hCurr, hParent;
	int parent;

	hCurr = n[curr].handle;
	for( ;; ) {
		parent = curr >> 1;
		hParent = n[parent].handle;
		if( parent == 0 || LEQ( h[hParent].key, h[hCurr].key )) {
			n[curr].handle = hCurr;
			h[hCurr].node = curr;
			break;
		}
		n[curr].handle = hParent;
		h[hParent].node = curr;
		curr = parent;
	}
}

/* really pqHeapInit */
void pqHeapInit( PriorityQHeap *pq )
{
	int i;

	/* This method of building a heap is O(n), rather than O(n lg n). */

	for( i = pq->size; i >= 1; --i ) {
		FloatDown( pq, i );
	}
	pq->initialized = TRUE;
}

/* really pqHeapInsert */
/* returns INV_HANDLE iff out of memory */
PQhandle pqHeapInsert( TESSalloc* alloc, PriorityQHeap *pq, PQkey keyNew )
{
	int curr;
	PQhandle free;

	curr = ++ pq->size;
	if( (curr*2) > pq->max ) {
		if (!alloc->memrealloc)
		{
			return INV_HANDLE;
		}
		else
		{
			PQnode *saveNodes= pq->nodes;
			PQhandleElem *saveHandles= pq->handles;

			// If the heap overflows, double its size.
			pq->max <<= 1;
			pq->nodes = (PQnode *)alloc->memrealloc( alloc->userData, pq->nodes, 
				(size_t)((pq->max + 1) * sizeof( pq->nodes[0] )));
			if (pq->nodes == NULL) {
				pq->nodes = saveNodes;	// restore ptr to free upon return 
				return INV_HANDLE;
			}
			pq->handles = (PQhandleElem *)alloc->memrealloc( alloc->userData, pq->handles,
				(size_t) ((pq->max + 1) * sizeof( pq->handles[0] )));
			if (pq->handles == NULL) {
				pq->handles = saveHandles; // restore ptr to free upon return 
				return INV_HANDLE;
			}
		}
	}

	if( pq->freeList == 0 ) {
		free = curr;
	} else {
		free = pq->freeList;
		pq->freeList = pq->handles[free].node;
	}

	pq->nodes[curr].handle = free;
	pq->handles[free].node = curr;
	pq->handles[free].key = keyNew;

	if( pq->initialized ) {
		FloatUp( pq, curr );
	}
	assert(free != INV_HANDLE);
	return free;
}

/* really pqHeapExtractMin */
PQkey pqHeapExtractMin( PriorityQHeap *pq )
{
	PQnode *n = pq->nodes;
	PQhandleElem *h = pq->handles;
	PQhandle hMin = n[1].handle;
	PQkey min = h[hMin].key;

	if( pq->size > 0 ) {
		n[1].handle = n[pq->size].handle;
		h[n[1].handle].node = 1;

		h[hMin].key = NULL;
		h[hMin].node = pq->freeList;
		pq->freeList = hMin;

		if( -- pq->size > 0 ) {
			FloatDown( pq, 1 );
		}
	}
	return min;
}

/* really pqHeapDelete */
void pqHeapDelete( PriorityQHeap *pq, PQhandle hCurr )
{
	PQnode *n = pq->nodes;
	PQhandleElem *h = pq->handles;
	int curr;

	assert( hCurr >= 1 && hCurr <= pq->max && h[hCurr].key != NULL );

	curr = h[hCurr].node;
	n[curr].handle = n[pq->size].handle;
	h[n[curr].handle].node = curr;

	if( curr <= -- pq->size ) {
		if( curr <= 1 || LEQ( h[n[curr>>1].handle].key, h[n[curr].handle].key )) {
			FloatDown( pq, curr );
		} else {
			FloatUp( pq, curr );
		}
	}
	h[hCurr].key = NULL;
	h[hCurr].node = pq->freeList;
	pq->freeList = hCurr;
}



/* Now redefine all the function names to map to their "Sort" versions. */

/* really tessPqSortNewPriorityQ */
PriorityQ *pqNewPriorityQ( TESSalloc* alloc, int size, int (*leq)(PQkey key1, PQkey key2) )
{
	PriorityQ *pq = (PriorityQ *)alloc->memalloc( alloc->userData, sizeof( PriorityQ ));
	if (pq == NULL) return NULL;

	pq->heap = pqHeapNewPriorityQ( alloc, size, leq );
	if (pq->heap == NULL) {
		alloc->memfree( alloc->userData, pq );
		return NULL;
	}

//	pq->keys = (PQkey *)memAlloc( INIT_SIZE * sizeof(pq->keys[0]) );
	pq->keys = (PQkey *)alloc->memalloc( alloc->userData, size * sizeof(pq->keys[0]) );
	if (pq->keys == NULL) {
		pqHeapDeletePriorityQ( alloc, pq->heap );
		alloc->memfree( alloc->userData, pq );
		return NULL;
	}

	pq->size = 0;
	pq->max = size; //INIT_SIZE;
	pq->initialized = FALSE;
	pq->leq = leq;
	
	return pq;
}

/* really tessPqSortDeletePriorityQ */
void pqDeletePriorityQ( TESSalloc* alloc, PriorityQ *pq )
{
	assert(pq != NULL); 
	if (pq->heap != NULL) pqHeapDeletePriorityQ( alloc, pq->heap );
	if (pq->order != NULL) alloc->memfree( alloc->userData, pq->order );
	if (pq->keys != NULL) alloc->memfree( alloc->userData, pq->keys );
	alloc->memfree( alloc->userData, pq );
}


#define LT(x,y)     (! LEQ(y,x))
#define GT(x,y)     (! LEQ(x,y))
#define Swap(a,b)   if(1){PQkey *tmp = *a; *a = *b; *b = tmp;}else

/* really tessPqSortInit */
int pqInit( TESSalloc* alloc, PriorityQ *pq )
{
	PQkey **p, **r, **i, **j, *piv;
	struct { PQkey **p, **r; } Stack[50], *top = Stack;
	unsigned int seed = 2016473283;

	/* Create an array of indirect pointers to the keys, so that we
	* the handles we have returned are still valid.
	*/
	/*
	pq->order = (PQkey **)memAlloc( (size_t)
	(pq->size * sizeof(pq->order[0])) );
	*/
	pq->order = (PQkey **)alloc->memalloc( alloc->userData,
										  (size_t)((pq->size+1) * sizeof(pq->order[0])) );
	/* the previous line is a patch to compensate for the fact that IBM */
	/* machines return a null on a malloc of zero bytes (unlike SGI),   */
	/* so we have to put in this defense to guard against a memory      */
	/* fault four lines down. from fossum@austin.ibm.com.               */
	if (pq->order == NULL) return 0;

	p = pq->order;
	r = p + pq->size - 1;
	for( piv = pq->keys, i = p; i <= r; ++piv, ++i ) {
		*i = piv;
	}

	/* Sort the indirect pointers in descending order,
	* using randomized Quicksort
	*/
	top->p = p; top->r = r; ++top;
	while( --top >= Stack ) {
		p = top->p;
		r = top->r;
		while( r > p + 10 ) {
			seed = seed * 1539415821 + 1;
			i = p + seed % (r - p + 1);
			piv = *i;
			*i = *p;
			*p = piv;
			i = p - 1;
			j = r + 1;
			do {
				do { ++i; } while( GT( **i, *piv ));
				do { --j; } while( LT( **j, *piv ));
				Swap( i, j );
			} while( i < j );
			Swap( i, j ); /* Undo last swap */
			if( i - p < r - j ) {
				top->p = j+1; top->r = r; ++top;
				r = i-1;
			} else {
				top->p = p; top->r = i-1; ++top;
				p = j+1;
			}
		}
		/* Insertion sort small lists */
		for( i = p+1; i <= r; ++i ) {
			piv = *i;
			for( j = i; j > p && LT( **(j-1), *piv ); --j ) {
				*j = *(j-1);
			}
			*j = piv;
		}
	}
	pq->max = pq->size;
	pq->initialized = TRUE;
	pqHeapInit( pq->heap );  /* always succeeds */

#ifndef NDEBUG
	p = pq->order;
	r = p + pq->size - 1;
	for( i = p; i < r; ++i ) {
		assert( LEQ( **(i+1), **i ));
	}
#endif

	return 1;
}

/* really tessPqSortInsert */
/* returns INV_HANDLE iff out of memory */ 
PQhandle pqInsert( TESSalloc* alloc, PriorityQ *pq, PQkey keyNew )
{
	int curr;

	if( pq->initialized ) {
		return pqHeapInsert( alloc, pq->heap, keyNew );
	}
	curr = pq->size;
	if( ++ pq->size >= pq->max ) {
		if (!alloc->memrealloc)
		{
			return INV_HANDLE;
		}
		else
		{
			PQkey *saveKey= pq->keys;
			// If the heap overflows, double its size.
			pq->max <<= 1;
			pq->keys = (PQkey *)alloc->memrealloc( alloc->userData, pq->keys, 
				(size_t)(pq->max * sizeof( pq->keys[0] )));
			if (pq->keys == NULL) { 
				pq->keys = saveKey;  // restore ptr to free upon return 
				return INV_HANDLE;
			}
		}
	}
	assert(curr != INV_HANDLE); 
	pq->keys[curr] = keyNew;

	/* Negative handles index the sorted array. */
	return -(curr+1);
}

/* really tessPqSortExtractMin */
PQkey pqExtractMin( PriorityQ *pq )
{
	PQkey sortMin, heapMin;

	if( pq->size == 0 ) {
		return pqHeapExtractMin( pq->heap );
	}
	sortMin = *(pq->order[pq->size-1]);
	if( ! pqHeapIsEmpty( pq->heap )) {
		heapMin = pqHeapMinimum( pq->heap );
		if( LEQ( heapMin, sortMin )) {
			return pqHeapExtractMin( pq->heap );
		}
	}
	do {
		-- pq->size;
	} while( pq->size > 0 && *(pq->order[pq->size-1]) == NULL );
	return sortMin;
}

/* really tessPqSortMinimum */
PQkey pqMinimum( PriorityQ *pq )
{
	PQkey sortMin, heapMin;

	if( pq->size == 0 ) {
		return pqHeapMinimum( pq->heap );
	}
	sortMin = *(pq->order[pq->size-1]);
	if( ! pqHeapIsEmpty( pq->heap )) {
		heapMin = pqHeapMinimum( pq->heap );
		if( LEQ( heapMin, sortMin )) {
			return heapMin;
		}
	}
	return sortMin;
}

/* really tessPqSortIsEmpty */
int pqIsEmpty( PriorityQ *pq )
{
	return (pq->size == 0) && pqHeapIsEmpty( pq->heap );
}

/* really tessPqSortDelete */
void pqDelete( PriorityQ *pq, PQhandle curr )
{
	if( curr >= 0 ) {
		pqHeapDelete( pq->heap, curr );
		return;
	}
	curr = -(curr+1);
	assert( curr < pq->max && pq->keys[curr] != NULL );

	pq->keys[curr] = NULL;
	while( pq->size > 0 && *(pq->order[pq->size-1]) == NULL ) {
		-- pq->size;
	}
}
