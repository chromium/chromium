// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass {};

class MyClass {
 public:
  // No error expected because of raw-ptr-exclude-path arg.
  SomeClass* raw_ptr_field1;
  // No error expected because of exclude-paths file,
  // raw_ptr_exclude_paths.exclude.
  SomeClass& raw_ref_field;
};
