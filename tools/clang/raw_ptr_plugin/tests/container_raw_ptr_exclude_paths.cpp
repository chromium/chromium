// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <list>

class SomeClass {};

class MyClass {
 public:
  // No error expected because of raw-ptr-exclude-path arg.
  std::list<SomeClass*> raw_ptr_ctn_field1;
};
