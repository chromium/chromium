// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "heap/stubs.h"

namespace blink {

class HeapObject;

class PartObject {
  DISALLOW_NEW();
  void Trace(Visitor* v) const { v->Trace(m_obj); }

 private:
  Member<HeapObject> m_obj;
};

class DisallowNewUntraceable {
  DISALLOW_NEW();
  int a [[maybe_unused]] = 0;
};

class OffHeapObjectGood1 {
 private:
  DisallowNewUntraceable m_part;
};

class OffHeapObjectBad1 {
  void Trace(Visitor* v) const { v->Trace(m_part); }

 private:
  PartObject m_part;
};

class OffHeapObjectGood2 {
  DISALLOW_NEW();

  void Trace(Visitor* v) const { v->Trace(m_part); }

 private:
  PartObject m_part;
};

template <typename T>
class TemplatedObject {
 private:
  T m_part;
};

class OffHeapObjectBad2 : public TemplatedObject<PartObject> {};

}  // namespace blink
