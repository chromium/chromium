// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "member_on_stack.h"

namespace blink {

namespace {

void FreeMethod() {
  Member<HeapObject> strong;
  WeakMember<HeapObject> weak;
  UntracedMember<HeapObject> untraced;
  Member<HeapObject>* ptr;
  Member<HeapObject>& ref = strong;
}

void MethodWithArg(Member<HeapObject>) {}

void MethodWithConstArg(const Member<HeapObject>) {}

}  // namespace

void HeapObject::Trace(Visitor* visitor) const {}

void GCedWithMember::Trace(Visitor* v) const {
  v->Trace(member_);
}

}  // namespace blink
