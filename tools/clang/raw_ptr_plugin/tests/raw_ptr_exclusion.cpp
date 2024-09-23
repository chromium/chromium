// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/memory/raw_ptr_exclusion.h"

#include "base/containers/span.h"

class SomeClass {};

class MyClass {
 public:
  // Error expected.
  SomeClass* raw_ptr_field1;
  // No error expected. Fields can be excluded due to performance reasons.
  RAW_PTR_EXCLUSION SomeClass* ignored_ptr_field1;
  // Error expected.
  SomeClass* raw_ptr_field2;
  // No error expected. Fields can be excluded due to performance reasons.
  RAW_PTR_EXCLUSION SomeClass* ignored_ptr_field2;
  // Error expected.
  SomeClass& raw_ref_field1;
  // No error expected. Fields can be excluded due to performance reasons.
  RAW_PTR_EXCLUSION SomeClass& ignored_ref_field1;
  // Error expected.
  SomeClass& raw_ref_field2;
  // No error expected. Fields can be excluded due to performance reasons.
  RAW_PTR_EXCLUSION SomeClass& ignored_ref_field2;
  // Error expected.
  base::span<SomeClass> span_field;
  // No error expected. Fields can be excluded due to performance reasons.
  RAW_PTR_EXCLUSION base::span<SomeClass> ignored_span_field;
};
