// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERSISTENT_FIELD_IN_GC_MANAGED_CLASS_H_
#define PERSISTENT_FIELD_IN_GC_MANAGED_CLASS_H_

#include "heap/stubs.h"

namespace blink {

class HeapObject;

class PartObject {
    DISALLOW_NEW();
private:
    Persistent<HeapObject> m_obj;
};

class HeapObject : public GarbageCollected<HeapObject> {
public:
 void Trace(Visitor*) const;

private:
    PartObject m_part;
    HeapVector<PartObject> m_parts;
    Persistent<HeapVector<Member<HeapObject>>> m_objs;
    WeakPersistent<HeapObject> m_weakPersistent;
};

}

#endif
