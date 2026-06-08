// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <tuple>

class SomeClass {};
class MyClass {
 public:
  // No error expected. Because of invalid config file
  // the check is disabled by default.
  std::tuple<SomeClass*, int*> raw_ptr_tuple;
};
