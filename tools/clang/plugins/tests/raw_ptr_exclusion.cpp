// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr_exclusion.h"

class SomeClass;

class MyClass {
  // Error expected.
  SomeClass* raw_ptr_field1;
  // No error expected.
  RAW_PTR_EXCLUSION SomeClass* ignored_field1;
  // Error expected.
  SomeClass* raw_ptr_field2;
  // No error expected.
  RAW_PTR_EXCLUSION SomeClass* ignored_field2;
};
