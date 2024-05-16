// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass {};

class MyClass {
 public:
  // Error expected. A raw_ptr should be used instead of a raw pointer.
  SomeClass* raw_ptr_field;
  // No error expected because of exclude-fields file,
  // raw_ptr_exclude_fields.exclude.
  SomeClass* excluded_ptr_field;
  // Error expected. A raw_ref should be used instead of a native reference.
  SomeClass& raw_ref_field;
  // No error expected because of exclude-fields file,
  // raw_ref_exclude_fields.exclude.
  SomeClass& excluded_ref_field;
};
