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

Member<HeapObject> ReturnMember();
WeakMember<HeapObject> ReturnWeakMember();
UntracedMember<HeapObject> ReturnUntracedMember();                  // OK
Member<HeapObject>* ReturnMemberPtr();                              // OK
Member<HeapObject>& ReturnMemberRef();                              // OK
GC_PLUGIN_IGNORE("test") Member<HeapObject> ReturnIgnoredMember();  // OK
void MethodWithRefArg(const Member<HeapObject>&) {
  MethodWithRefArg(ReturnMember());
  auto lambda = []() -> Member<HeapObject> { return ReturnMember(); };
  (void)lambda;
}

struct WithOperator {
  Member<HeapObject> operator()();
};

void HeapObject::Trace(Visitor* visitor) const {}

void GCedWithMember::Trace(Visitor* v) const {
  v->Trace(member_);
}

}  // namespace blink
