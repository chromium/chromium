// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

class SomeClass;

SomeClass* GetPointer();
SomeClass& GetReference();
base::span<SomeClass> GetSpan();

class MyClass {
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field = GetPointer();
  SomeClass* raw_ptr_field = GetPointer();

  // Expected rewrite: const raw_ref<SomeClass> raw_ref_field = GetReference();
  SomeClass& raw_ref_field = GetReference();

  // Expected rewrite: base::raw_span<SomeClass> span_field = GetSpan();
  base::span<SomeClass> span_field = GetSpan();
};
