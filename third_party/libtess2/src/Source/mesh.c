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
#include "mesh.h"
#include "geom.h"
#include "bucketalloc.h"

#define TRUE 1
#define FALSE 0

/************************ Utility Routines ************************/

/* Allocate and free half-edges in pairs for efficiency.
* The *only* place that should use this fact is allocation/free.
*/
typedef struct { TESShalfEdge e, eSym; } EdgePair;

/* MakeEdge creates a new pair of half-edges which form their own loop.
* No vertex or face structures are allocated, but these must be assigned
* before the current edge operation is completed.
*/
static TESShalfEdge *MakeEdge( TESSmesh* mesh, TESShalfEdge *eNext )
{
	TESShalfEdge *e;
	TESShalfEdge *eSym;
	TESShalfEdge *ePrev;
	EdgePair *pair = (EdgePair *)bucketAlloc( mesh->edgeBucket );
	if (pair == NULL) return NULL;

	e = &pair->e;
	eSym = &pair->eSym;

	/* Make sure eNext points to the first edge of the edge pair */
	if( eNext->Sym < eNext ) { eNext = eNext->Sym; }

	/* Insert in circular doubly-linked list before eNext.
	* Note that the prev pointer is stored in Sym->next.
	*/
	ePrev = eNext->Sym->next;
	eSym->next = ePrev;
	ePrev->Sym->next = e;
	e->next = eNext;
	eNext->Sym->next = eSym;

	e->Sym = eSym;
	e->Onext = e;
	e->Lnext = eSym;
	e->Org = NULL;
	e->Lface = NULL;
	e->winding = 0;
	e->activeRegion = NULL;
	e->mark = 0;

	eSym->Sym = e;
	eSym->Onext = eSym;
	eSym->Lnext = e;
	eSym->Org = NULL;
	eSym->Lface = NULL;
	eSym->winding = 0;
	eSym->activeRegion = NULL;
	eSym->mark = 0;

	return e;
}

/* Splice( a, b ) is best described by the Guibas/Stolfi paper or the
* CS348a notes (see mesh.h).  Basically it modifies the mesh so that
* a->Onext and b->Onext are exchanged.  This can have various effects
* depending on whether a and b belong to different face or vertex rings.
* For more explanation see tessMeshSplice() below.
*/
static void Splice( TESShalfEdge *a, TESShalfEdge *b )
{
	TESShalfEdge *aOnext = a->Onext;
	TESShalfEdge *bOnext = b->Onext;

	aOnext->Sym->Lnext = b;
	bOnext->Sym->Lnext = a;
	a->Onext = bOnext;
	b->Onext = aOnext;
}

/* MakeVertex( newVertex, eOrig, vNext ) attaches a new vertex and makes it the
* origin of all edges in the vertex loop to which eOrig belongs. "vNext" gives
* a place to insert the new vertex in the global vertex list.  We insert
* the new vertex *before* vNext so that algorithms which walk the vertex
* list will not see the newly created vertices.
*/
static void MakeVertex( TESSvertex *newVertex, 
					   TESShalfEdge *eOrig, TESSvertex *vNext )
{
	TESShalfEdge *e;
	TESSvertex *vPrev;
	TESSvertex *vNew = newVertex;

	assert(vNew != NULL);

	/* insert in circular doubly-linked list before vNext */
	vPrev = vNext->prev;
	vNew->prev = vPrev;
	vPrev->next = vNew;
	vNew->next = vNext;
	vNext->prev = vNew;

	vNew->anEdge = eOrig;
	/* leave coords, s, t undefined */

	/* fix other edges on this vertex loop */
	e = eOrig;
	do {
		e->Org = vNew;
		e = e->Onext;
	} while( e != eOrig );
}

/* MakeFace( newFace, eOrig, fNext ) attaches a new face and makes it the left
* face of all edges in the face loop to which eOrig belongs.  "fNext" gives
* a place to insert the new face in the global face list.  We insert
* the new face *before* fNext so that algorithms which walk the face
* list will not see the newly created faces.
*/
static void MakeFace( TESSface *newFace, TESShalfEdge *eOrig, TESSface *fNext )
{
	TESShalfEdge *e;
	TESSface *fPrev;
	TESSface *fNew = newFace;

	assert(fNew != NULL); 

	/* insert in circular doubly-linked list before fNext */
	fPrev = fNext->prev;
	fNew->prev = fPrev;
	fPrev->next = fNew;
	fNew->next = fNext;
	fNext->prev = fNew;

	fNew->anEdge = eOrig;
	fNew->trail = NULL;
	fNew->marked = FALSE;

	/* The new face is marked "inside" if the old one was.  This is a
	* convenience for the common case where a face has been split in two.
	*/
	fNew->inside = fNext->inside;

	/* fix other edges on this face loop */
	e = eOrig;
	do {
		e->Lface = fNew;
		e = e->Lnext;
	} while( e != eOrig );
}

/* KillEdge( eDel ) destroys an edge (the half-edges eDel and eDel->Sym),
* and removes from the global edge list.
*/
static void KillEdge( TESSmesh *mesh, TESShalfEdge *eDel )
{
	TESShalfEdge *ePrev, *eNext;

	/* Half-edges are allocated in pairs, see EdgePair above */
	if( eDel->Sym < eDel ) { eDel = eDel->Sym; }

	/* delete from circular doubly-linked list */
	eNext = eDel->next;
	ePrev = eDel->Sym->next;
	eNext->Sym->next = ePrev;
	ePrev->Sym->next = eNext;

	bucketFree( mesh->edgeBucket, eDel );
}


/* KillVertex( vDel ) destroys a vertex and removes it from the global
* vertex list.  It updates the vertex loop to point to a given new vertex.
*/
static void KillVertex( TESSmesh *mesh, TESSvertex *vDel, TESSvertex *newOrg )
{
	TESShalfEdge *e, *eStart = vDel->anEdge;
	TESSvertex *vPrev, *vNext;

	/* change the origin of all affected edges */
	e = eStart;
	do {
		e->Org = newOrg;
		e = e->Onext;
	} while( e != eStart );

	/* delete from circular doubly-linked list */
	vPrev = vDel->prev;
	vNext = vDel->next;
	vNext->prev = vPrev;
	vPrev->next = vNext;

	bucketFree( mesh->vertexBucket, vDel );
}

/* KillFace( fDel ) destroys a face and removes it from the global face
* list.  It updates the face loop to point to a given new face.
*/
static void KillFace( TESSmesh *mesh, TESSface *fDel, TESSface *newLface )
{
	TESShalfEdge *e, *eStart = fDel->anEdge;
	TESSface *fPrev, *fNext;

	/* change the left face of all affected edges */
	e = eStart;
	do {
		e->Lface = newLface;
		e = e->Lnext;
	} while( e != eStart );

	/* delete from circular doubly-linked list */
	fPrev = fDel->prev;
	fNext = fDel->next;
	fNext->prev = fPrev;
	fPrev->next = fNext;

	bucketFree( mesh->faceBucket, fDel );
}


/****************** Basic Edge Operations **********************/

/* tessMeshMakeEdge creates one edge, two vertices, and a loop (face).
* The loop consists of the two new half-edges.
*/
TESShalfEdge *tessMeshMakeEdge( TESSmesh *mesh )
{
	TESSvertex *newVertex1 = (TESSvertex*)bucketAlloc(mesh->vertexBucket);
	TESSvertex *newVertex2 = (TESSvertex*)bucketAlloc(mesh->vertexBucket);
	TESSface *newFace = (TESSface*)bucketAlloc(mesh->faceBucket);
	TESShalfEdge *e;

	/* if any one is null then all get freed */
	if (newVertex1 == NULL || newVertex2 == NULL || newFace == NULL) {
		if (newVertex1 != NULL) bucketFree( mesh->vertexBucket, newVertex1 );
		if (newVertex2 != NULL) bucketFree( mesh->vertexBucket, newVertex2 );
		if (newFace != NULL) bucketFree( mesh->faceBucket, newFace );     
		return NULL;
	} 

	e = MakeEdge( mesh, &mesh->eHead );
	if (e == NULL) return NULL;

	MakeVertex( newVertex1, e, &mesh->vHead );
	MakeVertex( newVertex2, e->Sym, &mesh->vHead );
	MakeFace( newFace, e, &mesh->fHead );
	return e;
}


/* tessMeshSplice( eOrg, eDst ) is the basic operation for changing the
* mesh connectivity and topology.  It changes the mesh so that
*	eOrg->Onext <- OLD( eDst->Onext )
*	eDst->Onext <- OLD( eOrg->Onext )
* where OLD(...) means the value before the meshSplice operation.
*
* This can have two effects on the vertex structure:
*  - if eOrg->Org != eDst->Org, the two vertices are merged together
*  - if eOrg->Org == eDst->Org, the origin is split into two vertices
* In both cases, eDst->Org is changed and eOrg->Org is untouched.
*
* Similarly (and independently) for the face structure,
*  - if eOrg->Lface == eDst->Lface, one loop is split into two
*  - if eOrg->Lface != eDst->Lface, two distinct loops are joined into one
* In both cases, eDst->Lface is changed and eOrg->Lface is unaffected.
*
* Some special cases:
* If eDst == eOrg, the operation has no effect.
* If eDst == eOrg->Lnext, the new face will have a single edge.
* If eDst == eOrg->Lprev, the old face will have a single edge.
* If eDst == eOrg->Onext, the new vertex will have a single edge.
* If eDst == eOrg->Oprev, the old vertex will have a single edge.
*/
int tessMeshSplice( TESSmesh* mesh, TESShalfEdge *eOrg, TESShalfEdge *eDst )
{
	int joiningLoops = FALSE;
	int joiningVertices = FALSE;

	if( eOrg == eDst ) return 1;

	if( eDst->Org != eOrg->Org ) {
		/* We are merging two disjoint vertices -- destroy eDst->Org */
		joiningVertices = TRUE;
		KillVertex( mesh, eDst->Org, eOrg->Org );
	}
	if( eDst->Lface != eOrg->Lface ) {
		/* We are connecting two disjoint loops -- destroy eDst->Lface */
		joiningLoops = TRUE;
		KillFace( mesh, eDst->Lface, eOrg->Lface );
	}

	/* Change the edge structure */
	Splice( eDst, eOrg );

	if( ! joiningVertices ) {
		TESSvertex *newVertex = (TESSvertex*)bucketAlloc( mesh->vertexBucket );
		if (newVertex == NULL) return 0;

		/* We split one vertex into two -- the new vertex is eDst->Org.
		* Make sure the old vertex points to a valid half-edge.
		*/
		MakeVertex( newVertex, eDst, eOrg->Org );
		eOrg->Org->anEdge = eOrg;
	}
	if( ! joiningLoops ) {
		TESSface *newFace = (TESSface*)bucketAlloc( mesh->faceBucket );  
		if (newFace == NULL) return 0;

		/* We split one loop into two -- the new loop is eDst->Lface.
		* Make sure the old face points to a valid half-edge.
		*/
		MakeFace( newFace, eDst, eOrg->Lface );
		eOrg->Lface->anEdge = eOrg;
	}

	return 1;
}


/* tessMeshDelete( eDel ) removes the edge eDel.  There are several cases:
* if (eDel->Lface != eDel->Rface), we join two loops into one; the loop
* eDel->Lface is deleted.  Otherwise, we are splitting one loop into two;
* the newly created loop will contain eDel->Dst.  If the deletion of eDel
* would create isolated vertices, those are deleted as well.
*
* This function could be implemented as two calls to tessMeshSplice
* plus a few calls to memFree, but this would allocate and delete
* unnecessary vertices and faces.
*/
int tessMeshDelete( TESSmesh *mesh, TESShalfEdge *eDel )
{
	TESShalfEdge *eDelSym = eDel->Sym;
	int joiningLoops = FALSE;

	/* First step: disconnect the origin vertex eDel->Org.  We make all
	* changes to get a consistent mesh in this "intermediate" state.
	*/
	if( eDel->Lface != eDel->Rface ) {
		/* We are joining two loops into one -- remove the left face */
		joiningLoops = TRUE;
		KillFace( mesh, eDel->Lface, eDel->Rface );
	}

	if( eDel->Onext == eDel ) {
		KillVertex( mesh, eDel->Org, NULL );
	} else {
		/* Make sure that eDel->Org and eDel->Rface point to valid half-edges */
		eDel->Rface->anEdge = eDel->Oprev;
		eDel->Org->anEdge = eDel->Onext;

		Splice( eDel, eDel->Oprev );
		if( ! joiningLoops ) {
			TESSface *newFace= (TESSface*)bucketAlloc( mesh->faceBucket );
			if (newFace == NULL) return 0; 

			/* We are splitting one loop into two -- create a new loop for eDel. */
			MakeFace( newFace, eDel, eDel->Lface );
		}
	}

	/* Claim: the mesh is now in a consistent state, except that eDel->Org
	* may have been deleted.  Now we disconnect eDel->Dst.
	*/
	if( eDelSym->Onext == eDelSym ) {
		KillVertex( mesh, eDelSym->Org, NULL );
		KillFace( mesh, eDelSym->Lface, NULL );
	} else {
		/* Make sure that eDel->Dst and eDel->Lface point to valid half-edges */
		eDel->Lface->anEdge = eDelSym->Oprev;
		eDelSym->Org->anEdge = eDelSym->Onext;
		Splice( eDelSym, eDelSym->Oprev );
	}

	/* Any isolated vertices or faces have already been freed. */
	KillEdge( mesh, eDel );

	return 1;
}


/******************** Other Edge Operations **********************/

/* All these routines can be implemented with the basic edge
* operations above.  They are provided for convenience and efficiency.
*/


/* tessMeshAddEdgeVertex( eOrg ) creates a new edge eNew such that
* eNew == eOrg->Lnext, and eNew->Dst is a newly created vertex.
* eOrg and eNew will have the same left face.
*/
TESShalfEdge *tessMeshAddEdgeVertex( TESSmesh *mesh, TESShalfEdge *eOrg )
{
	TESShalfEdge *eNewSym;
	TESShalfEdge *eNew = MakeEdge( mesh, eOrg );
	if (eNew == NULL) return NULL;

	eNewSym = eNew->Sym;

	/* Connect the new edge appropriately */
	Splice( eNew, eOrg->Lnext );

	/* Set the vertex and face information */
	eNew->Org = eOrg->Dst;
	{
		TESSvertex *newVertex= (TESSvertex*)bucketAlloc( mesh->vertexBucket );
		if (newVertex == NULL) return NULL;

		MakeVertex( newVertex, eNewSym, eNew->Org );
	}
	eNew->Lface = eNewSym->Lface = eOrg->Lface;

	return eNew;
}


/* tessMeshSplitEdge( eOrg ) splits eOrg into two edges eOrg and eNew,
* such that eNew == eOrg->Lnext.  The new vertex is eOrg->Dst == eNew->Org.
* eOrg and eNew will have the same left face.
*/
TESShalfEdge *tessMeshSplitEdge( TESSmesh *mesh, TESShalfEdge *eOrg )
{
	TESShalfEdge *eNew;
	TESShalfEdge *tempHalfEdge= tessMeshAddEdgeVertex( mesh, eOrg );
	if (tempHalfEdge == NULL) return NULL;

	eNew = tempHalfEdge->Sym;

	/* Disconnect eOrg from eOrg->Dst and connect it to eNew->Org */
	Splice( eOrg->Sym, eOrg->Sym->Oprev );
	Splice( eOrg->Sym, eNew );

	/* Set the vertex and face information */
	eOrg->Dst = eNew->Org;
	eNew->Dst->anEdge = eNew->Sym;	/* may have pointed to eOrg->Sym */
	eNew->Rface = eOrg->Rface;
	eNew->winding = eOrg->winding;	/* copy old winding information */
	eNew->Sym->winding = eOrg->Sym->winding;

	return eNew;
}


/* tessMeshConnect( eOrg, eDst ) creates a new edge from eOrg->Dst
* to eDst->Org, and returns the corresponding half-edge eNew.
* If eOrg->Lface == eDst->Lface, this splits one loop into two,
* and the newly created loop is eNew->Lface.  Otherwise, two disjoint
* loops are merged into one, and the loop eDst->Lface is destroyed.
*
* If (eOrg == eDst), the new face will have only two edges.
* If (eOrg->Lnext == eDst), the old face is reduced to a single edge.
* If (eOrg->Lnext->Lnext == eDst), the old face is reduced to two edges.
*/
TESShalfEdge *tessMeshConnect( TESSmesh *mesh, TESShalfEdge *eOrg, TESShalfEdge *eDst )
{
	TESShalfEdge *eNewSym;
	int joiningLoops = FALSE;  
	TESShalfEdge *eNew = MakeEdge( mesh, eOrg );
	if (eNew == NULL) return NULL;

	eNewSym = eNew->Sym;

	if( eDst->Lface != eOrg->Lface ) {
		/* We are connecting two disjoint loops -- destroy eDst->Lface */
		joiningLoops = TRUE;
		KillFace( mesh, eDst->Lface, eOrg->Lface );
	}

	/* Connect the new edge appropriately */
	Splice( eNew, eOrg->Lnext );
	Splice( eNewSym, eDst );

	/* Set the vertex and face information */
	eNew->Org = eOrg->Dst;
	eNewSym->Org = eDst->Org;
	eNew->Lface = eNewSym->Lface = eOrg->Lface;

	/* Make sure the old face points to a valid half-edge */
	eOrg->Lface->anEdge = eNewSym;

	if( ! joiningLoops ) {
		TESSface *newFace= (TESSface*)bucketAlloc( mesh->faceBucket );
		if (newFace == NULL) return NULL;

		/* We split one loop into two -- the new loop is eNew->Lface */
		MakeFace( newFace, eNew, eOrg->Lface );
	}
	return eNew;
}


/******************** Other Operations **********************/

/* tessMeshZapFace( fZap ) destroys a face and removes it from the
* global face list.  All edges of fZap will have a NULL pointer as their
* left face.  Any edges which also have a NULL pointer as their right face
* are deleted entirely (along with any isolated vertices this produces).
* An entire mesh can be deleted by zapping its faces, one at a time,
* in any order.  Zapped faces cannot be used in further mesh operations!
*/
void tessMeshZapFace( TESSmesh *mesh, TESSface *fZap )
{
	TESShalfEdge *eStart = fZap->anEdge;
	TESShalfEdge *e, *eNext, *eSym;
	TESSface *fPrev, *fNext;

	/* walk around face, deleting edges whose right face is also NULL */
	eNext = eStart->Lnext;
	do {
		e = eNext;
		eNext = e->Lnext;

		e->Lface = NULL;
		if( e->Rface == NULL ) {
			/* delete the edge -- see TESSmeshDelete above */

			if( e->Onext == e ) {
				KillVertex( mesh, e->Org, NULL );
			} else {
				/* Make sure that e->Org points to a valid half-edge */
				e->Org->anEdge = e->Onext;
				Splice( e, e->Oprev );
			}
			eSym = e->Sym;
			if( eSym->Onext == eSym ) {
				KillVertex( mesh, eSym->Org, NULL );
			} else {
				/* Make sure that eSym->Org points to a valid half-edge */
				eSym->Org->anEdge = eSym->Onext;
				Splice( eSym, eSym->Oprev );
			}
			KillEdge( mesh, e );
		}
	} while( e != eStart );

	/* delete from circular doubly-linked list */
	fPrev = fZap->prev;
	fNext = fZap->next;
	fNext->prev = fPrev;
	fPrev->next = fNext;

	bucketFree( mesh->faceBucket, fZap );
}


/* tessMeshNewMesh() creates a new mesh with no edges, no vertices,
* and no loops (what we usually call a "face").
*/
TESSmesh *tessMeshNewMesh( TESSalloc* alloc )
{
	TESSvertex *v;
	TESSface *f;
	TESShalfEdge *e;
	TESShalfEdge *eSym;
	TESSmesh *mesh = (TESSmesh *)alloc->memalloc( alloc->userData, sizeof( TESSmesh ));
	if (mesh == NULL) {
		return NULL;
	}
	
	if (alloc->meshEdgeBucketSize < 16)
		alloc->meshEdgeBucketSize = 16;
	if (alloc->meshEdgeBucketSize > 4096)
		alloc->meshEdgeBucketSize = 4096;
	
	if (alloc->meshVertexBucketSize < 16)
		alloc->meshVertexBucketSize = 16;
	if (alloc->meshVertexBucketSize > 4096)
		alloc->meshVertexBucketSize = 4096;
	
	if (alloc->meshFaceBucketSize < 16)
		alloc->meshFaceBucketSize = 16;
	if (alloc->meshFaceBucketSize > 4096)
		alloc->meshFaceBucketSize = 4096;

	mesh->edgeBucket = createBucketAlloc( alloc, "Mesh Edges", sizeof(EdgePair), alloc->meshEdgeBucketSize );
	mesh->vertexBucket = createBucketAlloc( alloc, "Mesh Vertices", sizeof(TESSvertex), alloc->meshVertexBucketSize );
	mesh->faceBucket = createBucketAlloc( alloc, "Mesh Faces", sizeof(TESSface), alloc->meshFaceBucketSize );

	v = &mesh->vHead;
	f = &mesh->fHead;
	e = &mesh->eHead;
	eSym = &mesh->eHeadSym;

	v->next = v->prev = v;
	v->anEdge = NULL;

	f->next = f->prev = f;
	f->anEdge = NULL;
	f->trail = NULL;
	f->marked = FALSE;
	f->inside = FALSE;

	e->next = e;
	e->Sym = eSym;
	e->Onext = NULL;
	e->Lnext = NULL;
	e->Org = NULL;
	e->Lface = NULL;
	e->winding = 0;
	e->activeRegion = NULL;

	eSym->next = eSym;
	eSym->Sym = e;
	eSym->Onext = NULL;
	eSym->Lnext = NULL;
	eSym->Org = NULL;
	eSym->Lface = NULL;
	eSym->winding = 0;
	eSym->activeRegion = NULL;

	return mesh;
}


/* tessMeshUnion( mesh1, mesh2 ) forms the union of all structures in
* both meshes, and returns the new mesh (the old meshes are destroyed).
*/
TESSmesh *tessMeshUnion( TESSalloc* alloc, TESSmesh *mesh1, TESSmesh *mesh2 )
{
	TESSface *f1 = &mesh1->fHead;
	TESSvertex *v1 = &mesh1->vHead;
	TESShalfEdge *e1 = &mesh1->eHead;
	TESSface *f2 = &mesh2->fHead;
	TESSvertex *v2 = &mesh2->vHead;
	TESShalfEdge *e2 = &mesh2->eHead;

	/* Add the faces, vertices, and edges of mesh2 to those of mesh1 */
	if( f2->next != f2 ) {
		f1->prev->next = f2->next;
		f2->next->prev = f1->prev;
		f2->prev->next = f1;
		f1->prev = f2->prev;
	}

	if( v2->next != v2 ) {
		v1->prev->next = v2->next;
		v2->next->prev = v1->prev;
		v2->prev->next = v1;
		v1->prev = v2->prev;
	}

	if( e2->next != e2 ) {
		e1->Sym->next->Sym->next = e2->next;
		e2->next->Sym->next = e1->Sym->next;
		e2->Sym->next->Sym->next = e1;
		e1->Sym->next = e2->Sym->next;
	}

	alloc->memfree( alloc->userData, mesh2 );
	return mesh1;
}


static int CountFaceVerts( TESSface *f )
{
	TESShalfEdge *eCur = f->anEdge;
	int n = 0;
	do
	{
		n++;
		eCur = eCur->Lnext;
	}
	while (eCur != f->anEdge);
	return n;
}

int tessMeshMergeConvexFaces( TESSmesh *mesh, int maxVertsPerFace )
{
	TESShalfEdge *e, *eNext, *eSym;
	TESShalfEdge *eHead = &mesh->eHead;
	TESSvertex *va, *vb, *vc, *vd, *ve, *vf;
	int leftNv, rightNv;
	
	for( e = eHead->next; e != eHead; e = eNext )
	{
		eNext = e->next;
		eSym = e->Sym;
		if( !eSym )
			continue;
		
		// Both faces must be inside
		if( !e->Lface || !e->Lface->inside )
			continue;
		if( !eSym->Lface || !eSym->Lface->inside )
			continue;

		leftNv = CountFaceVerts( e->Lface );
		rightNv = CountFaceVerts( eSym->Lface );
		if( (leftNv+rightNv-2) > maxVertsPerFace )
			continue;

		// Merge if the resulting poly is convex.
		//
		//      vf--ve--vd
		//          ^|
		// left   e ||   right
		//          |v
		//      va--vb--vc

		va = e->Lprev->Org;
		vb = e->Org;
		vc = e->Sym->Lnext->Dst;

		vd = e->Sym->Lprev->Org;
		ve = e->Sym->Org;
		vf = e->Lnext->Dst;

		if( VertCCW( va, vb, vc ) && VertCCW( vd, ve, vf ) ) {
			if( e == eNext || e == eNext->Sym ) { eNext = eNext->next; }
			if( !tessMeshDelete( mesh, e ) )
				return 0;
		}
	}

	return 1;
}

void tessMeshFlipEdge( TESSmesh *mesh, TESShalfEdge *edge )
{
	TESShalfEdge *a0 = edge;
	TESShalfEdge *a1 = a0->Lnext;
	TESShalfEdge *a2 = a1->Lnext;
	TESShalfEdge *b0 = edge->Sym;
	TESShalfEdge *b1 = b0->Lnext;
	TESShalfEdge *b2 = b1->Lnext;

	TESSvertex *aOrg = a0->Org;
	TESSvertex *aOpp = a2->Org;
	TESSvertex *bOrg = b0->Org;
	TESSvertex *bOpp = b2->Org;

	TESSface *fa = a0->Lface;
	TESSface *fb = b0->Lface;

	assert(EdgeIsInternal(edge));
	assert(a2->Lnext == a0);
	assert(b2->Lnext == b0);

	a0->Org = bOpp;
	a0->Onext = b1->Sym;
	b0->Org = aOpp;
	b0->Onext = a1->Sym;
	a2->Onext = b0;
	b2->Onext = a0;
	b1->Onext = a2->Sym;
	a1->Onext = b2->Sym;

	a0->Lnext = a2;
	a2->Lnext = b1;
	b1->Lnext = a0;

	b0->Lnext = b2;
	b2->Lnext = a1;
	a1->Lnext = b0;

	a1->Lface = fb;
	b1->Lface = fa;

	fa->anEdge = a0;
	fb->anEdge = b0;

	if (aOrg->anEdge == a0) aOrg->anEdge = b1;
	if (bOrg->anEdge == b0) bOrg->anEdge = a1;

	assert( a0->Lnext->Onext->Sym == a0 );
	assert( a0->Onext->Sym->Lnext == a0 );
	assert( a0->Org->anEdge->Org == a0->Org );


	assert( a1->Lnext->Onext->Sym == a1 );
	assert( a1->Onext->Sym->Lnext == a1 );
	assert( a1->Org->anEdge->Org == a1->Org );

	assert( a2->Lnext->Onext->Sym == a2 );
	assert( a2->Onext->Sym->Lnext == a2 );
	assert( a2->Org->anEdge->Org == a2->Org );

	assert( b0->Lnext->Onext->Sym == b0 );
	assert( b0->Onext->Sym->Lnext == b0 );
	assert( b0->Org->anEdge->Org == b0->Org );

	assert( b1->Lnext->Onext->Sym == b1 );
	assert( b1->Onext->Sym->Lnext == b1 );
	assert( b1->Org->anEdge->Org == b1->Org );

	assert( b2->Lnext->Onext->Sym == b2 );
	assert( b2->Onext->Sym->Lnext == b2 );
	assert( b2->Org->anEdge->Org == b2->Org );

	assert(aOrg->anEdge->Org == aOrg);
	assert(bOrg->anEdge->Org == bOrg);

	assert(a0->Oprev->Onext->Org == a0->Org);
}

#ifdef DELETE_BY_ZAPPING

/* tessMeshDeleteMesh( mesh ) will free all storage for any valid mesh.
*/
void tessMeshDeleteMesh( TESSalloc* alloc, TESSmesh *mesh )
{
	TESSface *fHead = &mesh->fHead;

	while( fHead->next != fHead ) {
		tessMeshZapFace( fHead->next );
	}
	assert( mesh->vHead.next == &mesh->vHead );

	alloc->memfree( alloc->userData, mesh );
}

#else

/* tessMeshDeleteMesh( mesh ) will free all storage for any valid mesh.
*/
void tessMeshDeleteMesh( TESSalloc* alloc, TESSmesh *mesh )
{
	deleteBucketAlloc(mesh->edgeBucket);
	deleteBucketAlloc(mesh->vertexBucket);
	deleteBucketAlloc(mesh->faceBucket);

	alloc->memfree( alloc->userData, mesh );
}

#endif

#ifndef NDEBUG

/* tessMeshCheckMesh( mesh ) checks a mesh for self-consistency.
*/
void tessMeshCheckMesh( TESSmesh *mesh )
{
	TESSface *fHead = &mesh->fHead;
	TESSvertex *vHead = &mesh->vHead;
	TESShalfEdge *eHead = &mesh->eHead;
	TESSface *f, *fPrev;
	TESSvertex *v, *vPrev;
	TESShalfEdge *e, *ePrev;

	for( fPrev = fHead ; (f = fPrev->next) != fHead; fPrev = f) {
		assert( f->prev == fPrev );
		e = f->anEdge;
		do {
			assert( e->Sym != e );
			assert( e->Sym->Sym == e );
			assert( e->Lnext->Onext->Sym == e );
			assert( e->Onext->Sym->Lnext == e );
			assert( e->Lface == f );
			e = e->Lnext;
		} while( e != f->anEdge );
	}
	assert( f->prev == fPrev && f->anEdge == NULL );

	for( vPrev = vHead ; (v = vPrev->next) != vHead; vPrev = v) {
		assert( v->prev == vPrev );
		e = v->anEdge;
		do {
			assert( e->Sym != e );
			assert( e->Sym->Sym == e );
			assert( e->Lnext->Onext->Sym == e );
			assert( e->Onext->Sym->Lnext == e );
			assert( e->Org == v );
			e = e->Onext;
		} while( e != v->anEdge );
	}
	assert( v->prev == vPrev && v->anEdge == NULL );

	for( ePrev = eHead ; (e = ePrev->next) != eHead; ePrev = e) {
		assert( e->Sym->next == ePrev->Sym );
		assert( e->Sym != e );
		assert( e->Sym->Sym == e );
		assert( e->Org != NULL );
		assert( e->Dst != NULL );
		assert( e->Lnext->Onext->Sym == e );
		assert( e->Onext->Sym->Lnext == e );
	}
	assert( e->Sym->next == ePrev->Sym
		&& e->Sym == &mesh->eHeadSym
		&& e->Sym->Sym == e
		&& e->Org == NULL && e->Dst == NULL
		&& e->Lface == NULL && e->Rface == NULL );
}

#endif
