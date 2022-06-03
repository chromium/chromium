// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEMBER_IN_OFFHEAP_CLASS_H_
#define MEMBER_IN_OFFHEAP_CLASS_H_

#include "heap/stubs.h"

namespace blink {

class HeapObject : public GarbageCollected<HeapObject> { };

class OffHeapObject {
public:
 void Trace(Visitor*) const;

private:
    Member<HeapObject> m_obj; // Must not contain Member.
    WeakMember<HeapObject> m_weak;  // Must not contain WeakMember.
    Persistent<HeapVector<Member<HeapObject> > > m_objs; // OK
};

class StackObject {
    STACK_ALLOCATED();
private:
    HeapObject* m_obj; // OK
    HeapVector<Member<OffHeapObject>> m_heapVectorMemberOff; // NOT OK
};

class PartObject {
    DISALLOW_NEW();
public:
 void Trace(Visitor*) const;

private:
    Member<HeapObject> m_obj; // OK
};

class InlineObject {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
 void Trace(Visitor*) const;

private:
    Member<HeapObject> m_obj; // OK
};

}

#endif
