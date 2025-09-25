// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PTRS_TO_TRACEABLE_CLASS_H_
#define PTRS_TO_TRACEABLE_CLASS_H_

#include "heap/stubs.h"

namespace blink {

class HeapObject : public GarbageCollected<HeapObject> {
 public:
  void Trace(Visitor*) const {}
};

class Traceable {
  DISALLOW_NEW();

 public:
  void Trace(Visitor* v) const { v->Trace(member_); }

 private:
  Member<HeapObject> member_;
};

class OffGCedHeap {
 public:
  explicit OffGCedHeap(Traceable* traceable)
      : raw_ptr(traceable), const_raw_ptr(traceable), ref_ptr(*traceable) {
    (void)raw_ptr;
    (void)const_raw_ptr;
    (void)ref_ptr;
    (void)vector;
  }

 private:
  Traceable* raw_ptr;
  const Traceable* const_raw_ptr;
  Traceable& ref_ptr;
  std::unique_ptr<Traceable> unique;
  std::vector<Traceable*> vector;
};

class OnGCedHeap : public GarbageCollected<OnGCedHeap> {
 public:
  explicit OnGCedHeap(Traceable* traceable)
      : raw_ptr(traceable), const_raw_ptr(traceable), ref_ptr(*traceable) {
    (void)raw_ptr;
    (void)const_raw_ptr;
    (void)ref_ptr;
    (void)vector;
  }

 private:
  Traceable* raw_ptr;
  const Traceable* const_raw_ptr;
  Traceable& ref_ptr;
  std::unique_ptr<Traceable> unique;
  std::vector<Traceable*> vector;
};

}  // namespace blink

#endif  // PTRS_TO_TRACEABLE_CLASS_H_
