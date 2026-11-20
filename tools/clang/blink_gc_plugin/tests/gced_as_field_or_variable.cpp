// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gced_as_field_or_variable.h"

namespace blink {

GCed ReturnGCed();                              // Not OK
Mixin ReturnMixin();                            // Not OK
GCedHeapVector<GCed> ReturnGCedVector();        // Not OK
HeapVector<GCed> ReturnVector();                // Not OK
HeapVector<Member<GCed>> ReturnMemberVector();  // OK
GCed* ReturnGCedPtr();                          // OK
GCed& ReturnGCedRef();                          // OK
GC_PLUGIN_IGNORE("test") GCed ReturnIgnored();  // OK
GCed ReturnDeletedGCed() = delete;              // OK
void TakeGCedByRef(const GCed&);                // OK
void TakeDeletedGCedByVal(GCed) = delete;       // OK

void Foo() {
  GCed gced;
  (void)gced;
  Mixin mixin;
  (void)mixin;
  HeapVector<GCed> vector;              // Not OK
  HeapHashMap<GCed, int> map;           // Not OK
  GCedHeapVector<GCed> gced_vector;     // Not OK
  GCedHeapHashMap<GCed, int> gced_map;  // Not OK
  HeapVector<Member<GCed>> member_vector;     // OK
  HeapHashMap<Member<GCed>, int> member_map;  // OK
  TakeGCedByRef(ReturnGCed());
}

template <typename T>
T UninstantiatedTemplateFunction();  // OK when not defined/called

template <typename T>
class TemplateWithUninstantiatedMethod {
 public:
  T UninstantiatedMethod(T x) { return x; }  // OK when not called
};

using UnusedTemplateFunctionReturn =
    decltype(UninstantiatedTemplateFunction<GCed>());
TemplateWithUninstantiatedMethod<GCed> g_unused_template_method;

}  // namespace blink
