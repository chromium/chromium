// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gced_as_field_or_variable.h"

namespace blink {

void Foo() {
  GCed gced;
  (void)gced;
  Mixin mixin;
  (void)mixin;
  HeapVector<GCed> vector;  // OK
  HeapHashMap<GCed, int> map;
}

}  // namespace blink
