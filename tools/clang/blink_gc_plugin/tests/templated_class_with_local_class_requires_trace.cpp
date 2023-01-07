// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "templated_class_with_local_class_requires_trace.h"

namespace blink {

template <typename T>
void TemplatedObject<T>::Trace(Visitor* visitor) const {
  visitor->Trace(m_local);
  visitor->Trace(m_memberRef);
}

class Test {
 public:
  static void test() {
    HeapObject* obj = MakeGarbageCollected<HeapObject>();
    TemplatedObject<HeapObject>* instance =
        MakeGarbageCollected<TemplatedObject<HeapObject>>(obj);
  }
};

} // namespace blink
