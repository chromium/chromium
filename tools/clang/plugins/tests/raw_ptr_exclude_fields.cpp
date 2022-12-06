// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass;

class MyClass {
  // Error expected.
  SomeClass* raw_ptr_field;
  // No error expected because of exclude-fields file,
  // raw_ptr_exclude_fields.exclude.
  SomeClass* excluded_field;
};
