// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass;

class MyClass {
  MyClass() : ptr_field3_(nullptr), ptr_field7_(nullptr) {}

  // Expected rewrite: raw_ptr<const SomeClass> ptr_field1_;
  const SomeClass* ptr_field1_;

  // Expected rewrite: raw_ptr<volatile SomeClass> ptr_field2_;
  volatile SomeClass* ptr_field2_;

  // Expected rewrite: const raw_ptr<SomeClass> ptr_field3_;
  SomeClass* const ptr_field3_;

  // Expected rewrite: mutable raw_ptr<SomeClass> ptr_field4_;
  mutable SomeClass* ptr_field4_;

  // Expected rewrite: raw_ptr<const SomeClass> ptr_field5_;
  SomeClass const* ptr_field5_;

  // Expected rewrite: volatile raw_ptr<const SomeClass> ptr_field6_;
  const SomeClass* volatile ptr_field6_;

  // Expected rewrite: const raw_ptr<const SomeClass> ptr_field7_;
  const SomeClass* const ptr_field7_;
};
