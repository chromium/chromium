// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass;

namespace my_namespace {

struct MyStruct {
  // Blocklisted - no rewrite expected.
  SomeClass* my_field;
  SomeClass* my_field2;

  // Non-blocklisted - expected rewrite: CheckedPtr<SomeClass> my_field3;
  SomeClass* my_field3;
};

template <typename T>
class MyTemplate {
 public:
  // Blocklisted - no rewrite expected.
  SomeClass* my_field;

  // Non-blocklisted - expected rewrite: CheckedPtr<SomeClass> my_field2;
  SomeClass* my_field2;
};

}  // namespace my_namespace

namespace other_namespace {

struct MyStruct {
  // Blocklisted in another namespace, but not here.
  // Expected rewrite: CheckedPtr<SomeClass> my_field;
  SomeClass* my_field;
};

}  // namespace other_namespace
