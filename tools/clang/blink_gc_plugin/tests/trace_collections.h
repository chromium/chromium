// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRACE_COLLECTIONS_H_
#define TRACE_COLLECTIONS_H_

#include "heap/stubs.h"

namespace blink {

class HeapObject : public GarbageCollected<HeapObject> {
public:
 void Trace(Visitor*) const;

private:
    HeapVector<Member<HeapObject> > m_heapVector;
    Member<GCedHeapVector<Member<HeapObject>>> m_gcedHeapVector;
    Vector<Member<HeapObject>, 0, HeapAllocator> m_wtfVector;

    HeapDeque<Member<HeapObject> > m_heapDeque;
    Member<GCedHeapDeque<Member<HeapObject>>> m_gcedHeapDeque;
    Deque<Member<HeapObject>, 0, HeapAllocator> m_wtfDeque;

    HeapHashSet<Member<HeapObject> > m_heapSet;
    Member<GCedHeapHashSet<Member<HeapObject>>> m_gcedHeapSet;
    HashSet<Member<HeapObject>, void, HeapAllocator> m_wtfSet;

    HeapLinkedHashSet<Member<HeapObject> > m_heapLinkedSet;
    Member<GCedHeapLinkedHashSet<Member<HeapObject>>> m_gcedHeapLinkedSet;
    LinkedHashSet<Member<HeapObject>, void, HeapAllocator> m_wtfLinkedSet;

    HeapHashCountedSet<Member<HeapObject> > m_heapCountedSet;
    Member<GCedHeapHashCountedSet<Member<HeapObject>>> m_gcedHeapCountedSet;
    HashCountedSet<Member<HeapObject>, void, HeapAllocator> m_wtfCountedSet;

    HeapHashMap<int, Member<HeapObject> > m_heapMapKey;
    HeapHashMap<Member<HeapObject>, int > m_heapMapVal;
    Member<GCedHeapHashMap<int, Member<HeapObject>>> m_gcedHeapMapKey;
    Member<GCedHeapHashMap<Member<HeapObject>, int>> m_gcedHeapMapVal;
    HashMap<int, Member<HeapObject>, void, void, HeapAllocator> m_wtfMapKey;
    HashMap<Member<HeapObject>, int, void, void, HeapAllocator> m_wtfMapVal;
};

}

#endif
