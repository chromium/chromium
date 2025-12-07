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
** Author: Mikko Mononen, July 2009.
*/

#include <stdio.h>
#include <stdlib.h>
#include "../Include/tesselator.h"

//#define CHECK_BOUNDS

typedef struct BucketAlloc BucketAlloc;
typedef struct Bucket Bucket;

struct Bucket
{
	Bucket *next;
};

struct BucketAlloc
{
	void *freelist;
	Bucket *buckets;
	unsigned int itemSize;
	unsigned int bucketSize;
	const char *name;
	TESSalloc* alloc;
};

static int CreateBucket( struct BucketAlloc* ba )
{
	size_t size;
	Bucket* bucket;
	void* freelist;
	unsigned char* head;
	unsigned char* it;

	// Allocate memory for the bucket
	size = sizeof(Bucket) + ba->itemSize * ba->bucketSize;
	bucket = (Bucket*)ba->alloc->memalloc( ba->alloc->userData, size );
	if ( !bucket )
		return 0;
	bucket->next = 0;

	// Add the bucket into the list of buckets.
	bucket->next = ba->buckets;
	ba->buckets = bucket;

	// Add new items to the free list.
	freelist = ba->freelist;
	head = (unsigned char*)bucket + sizeof(Bucket);
	it = head + ba->itemSize * ba->bucketSize;
	do
	{
		it -= ba->itemSize;
		// Store pointer to next free item.
		*((void**)it) = freelist;
		// Pointer to next location containing a free item.
		freelist = (void*)it;
	}
	while ( it != head );
	// Update pointer to next location containing a free item.
	ba->freelist = (void*)it;

	return 1;
}

static void *NextFreeItem( struct BucketAlloc *ba )
{
	return *(void**)ba->freelist;
}

struct BucketAlloc* createBucketAlloc( TESSalloc* alloc, const char* name,
									  unsigned int itemSize, unsigned int bucketSize )
{
	BucketAlloc* ba = (BucketAlloc*)alloc->memalloc( alloc->userData, sizeof(BucketAlloc) );

	ba->alloc = alloc;
	ba->name = name;
	ba->itemSize = itemSize;
	if ( ba->itemSize < sizeof(void*) )
		ba->itemSize = sizeof(void*);
	ba->bucketSize = bucketSize;
	ba->freelist = 0;
	ba->buckets = 0;

	if ( !CreateBucket( ba ) )
	{
		alloc->memfree( alloc->userData, ba );
		return 0;
	}

	return ba;
}

void* bucketAlloc( struct BucketAlloc *ba )
{
	void *it;

	// If running out of memory, allocate new bucket and update the freelist.
	if ( !ba->freelist || !NextFreeItem( ba ) )
	{
		if ( !CreateBucket( ba ) )
			return 0;
	}

	// Pop item from in front of the free list.
	it = ba->freelist;
	ba->freelist = NextFreeItem( ba );

	return it;
}

void bucketFree( struct BucketAlloc *ba, void *ptr )
{
#ifdef CHECK_BOUNDS
	int inBounds = 0;
	Bucket *bucket;

	// Check that the pointer is allocated with this allocator.
	bucket = ba->buckets;
	while ( bucket )
	{
		void *bucketMin = (void*)((unsigned char*)bucket + sizeof(Bucket));
		void *bucketMax = (void*)((unsigned char*)bucket + sizeof(Bucket) + ba->itemSize * ba->bucketSize);
		if ( ptr >= bucketMin && ptr < bucketMax )
		{
			inBounds = 1;
			break;
		}
		bucket = bucket->next;			
	}		

	if ( inBounds )
	{
		// Add the node in front of the free list.
		*(void**)ptr = ba->freelist;
		ba->freelist = ptr;
	}
	else
	{
		printf("ERROR! pointer 0x%p does not belong to allocator '%s'\n", ba->name);
	}
#else
	// Add the node in front of the free list.
	*(void**)ptr = ba->freelist;
	ba->freelist = ptr;
#endif
}

void deleteBucketAlloc( struct BucketAlloc *ba )
{
	TESSalloc* alloc = ba->alloc;
	Bucket *bucket = ba->buckets;
	Bucket *next;
	while ( bucket )
	{
		next = bucket->next;
		alloc->memfree( alloc->userData, bucket );
		bucket = next;
	}		
	ba->freelist = 0;
	ba->buckets = 0;
	alloc->memfree( alloc->userData, ba );
}
