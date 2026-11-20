// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weak_ptr_to_gc_managed_class.h"

namespace blink {

base::WeakPtr<GCed> ReturnWeakPtrGCed();
base::WeakPtr<Mixin> ReturnWeakPtrMixin();
base::WeakPtr<NonGCed> ReturnWeakPtrNonGCed();                        // OK
GC_PLUGIN_IGNORE("test") base::WeakPtr<GCed> ReturnIgnoredWeakPtr();  // OK
void TakeWeakPtrByRef(const base::WeakPtr<GCed>&);                    // OK

void Foo() {
  base::WeakPtr<GCed> gced;
  base::WeakPtr<Mixin> mixin;
  base::WeakPtr<NonGCed> nongced;  // OK
  TakeWeakPtrByRef(ReturnWeakPtrGCed());
}

}  // namespace blink
