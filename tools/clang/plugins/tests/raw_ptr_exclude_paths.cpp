// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass;

class MyClass {
  // No error expected because of exclude-paths file,
  // raw_ptr_exclude_paths.exclude.
  SomeClass* raw_ptr_field1;
};
