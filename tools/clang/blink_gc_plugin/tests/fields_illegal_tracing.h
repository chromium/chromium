// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIELDS_ILLEGAL_TRACING_H_
#define FIELDS_ILLEGAL_TRACING_H_

#include "heap/stubs.h"

namespace blink {

namespace bar {

// check that (only) std::unique_ptr<> is reported
// as an illegal smart pointer type.
template<typename T> class unique_ptr {
public:
    ~unique_ptr() { }
    operator T*() const { return 0; }
    T* operator->() { return 0; }

    void Trace(Visitor* visitor)
    {
    }
};

}

class HeapObject;
class PartObject;

class PartObject {
    DISALLOW_NEW();
public:
    void Trace(Visitor*);
private:
    RefPtr<HeapObject> m_obj2;
    bar::unique_ptr<HeapObject> m_obj3;
    std::unique_ptr<HeapObject> m_obj4;
    Vector<int>::iterator m_iterator1;
    HeapVector<Member<HeapObject>>::iterator m_iterator2;
    HeapHashSet<PartObject>::const_iterator m_iterator3;
};

class HeapObject : public GarbageCollected<HeapObject> {
 public:
  void Trace(Visitor*);

 private:
  PartObject m_part;
  RefPtr<HeapObject> m_obj2;
  bar::unique_ptr<HeapObject> m_obj3;
  std::unique_ptr<HeapObject> m_obj4;
  HeapHashMap<int, Member<HeapObject>>::reverse_iterator m_iterator3;
  HeapDeque<Member<HeapObject>>::const_reverse_iterator m_iterator4;
  HeapListHashSet<Member<HeapObject>>::const_iterator m_iterator5;
  HeapLinkedHashSet<Member<HeapObject>>::const_iterator m_iterator6;
};
}

#endif
