// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass;

namespace my_namespace {

struct MyStruct {
  // Blocklisted - no rewrite expected.
  SomeClass* my_ptr_field;
  SomeClass* my_ptr_field2;
  SomeClass& my_ref_field;
  SomeClass& my_ref_field2;

  // Non-blocklisted - expected rewrite: const raw_ref<SomeClass> my_ref_field3;
  SomeClass& my_ref_field3;
  // Non-blocklisted - expected rewrite: raw_ptr<SomeClass> my_ptr_field3;
  SomeClass* my_ptr_field3;
};

template <typename T>
class MyTemplate {
 public:
  // Blocklisted - no rewrite expected.
  SomeClass* my_ptr_field;
  // Blocklisted - no rewrite expected.
  SomeClass& my_ref_field;

  // Non-blocklisted - expected rewrite: const raw_ref<SomeClass> my_ref_field2;
  SomeClass& my_ref_field2;

  // Non-blocklisted - expected rewrite: raw_ptr<SomeClass> my_ptr_field2;
  SomeClass* my_ptr_field2;
};

}  // namespace my_namespace

namespace other_namespace {

struct MyStruct {
  // Blocklisted in another namespace, but not here.
  // Expected rewrite: raw_ptr<SomeClass> my_ptr_field;
  SomeClass* my_ptr_field;

  // Blocklisted in another namespace, but not here.
  // Expected rewrite: const raw_ref<SomeClass> my_ref_field;
  SomeClass& my_ref_field;
};

}  // namespace other_namespace
