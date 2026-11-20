// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GCED_AS_FIELD_OR_VARIABLE_H_
#define GCED_AS_FIELD_OR_VARIABLE_H_

#include "heap/stubs.h"

namespace blink {

class GCed : public GarbageCollected<GCed> {
 public:
  void Trace(Visitor*) const {}
};

class Mixin : public GarbageCollectedMixin {
 public:
  void Trace(Visitor*) const override {}
};

class OtherGCed : public GarbageCollected<OtherGCed> {
 public:
  OtherGCed operator=(const OtherGCed&) = delete;  // OK (deleted)

  void Trace(Visitor* v) const {
    v->Trace(gced_);
    v->Trace(mixin_);
    v->Trace(vector_);
    v->Trace(map_);
    v->Trace(gced_vector_);
    v->Trace(gced_map_);
    v->Trace(member_vector_);
    v->Trace(member_map_);
    v->Trace(member_gced_vector_);
    v->Trace(member_gced_map_);
  }

 private:
  GCed gced_;
  Mixin mixin_;
  HeapVector<GCed> vector_;              // Not OK
  HeapHashMap<GCed, int> map_;           // Not OK
  GCedHeapVector<GCed> gced_vector_;     // Not OK
  GCedHeapHashMap<GCed, int> gced_map_;  // Not OK
  HeapVector<Member<GCed>> member_vector_;                      // OK
  HeapHashMap<Member<GCed>, int> member_map_;                   // OK
  Member<GCedHeapVector<Member<GCed>>> member_gced_vector_;     // OK
  Member<GCedHeapHashMap<Member<GCed>, int>> member_gced_map_;  // OK
};

}  // namespace blink

#endif  // GCED_AS_FIELD_OR_VARIABLE_H_
