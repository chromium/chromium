// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"

class SomeClass;

class MyClass {
  MyClass() : ptr_field3_(nullptr), ptr_field7_(nullptr), span_field2_({}) {}

  // Expected rewrite: raw_ptr<const SomeClass> ptr_field1_;
  raw_ptr<const SomeClass> ptr_field1_;

  // Expected rewrite: raw_ptr<volatile SomeClass> ptr_field2_;
  raw_ptr<volatile SomeClass> ptr_field2_;

  // Expected rewrite: const raw_ptr<SomeClass> ptr_field3_;
  const raw_ptr<SomeClass> ptr_field3_;

  // Expected rewrite: mutable raw_ptr<SomeClass> ptr_field4_;
  mutable raw_ptr<SomeClass> ptr_field4_;

  // Expected rewrite: raw_ptr<const SomeClass> ptr_field5_;
  raw_ptr<const SomeClass> ptr_field5_;

  // Expected rewrite: volatile raw_ptr<const SomeClass> ptr_field6_;
  volatile raw_ptr<const SomeClass> ptr_field6_;

  // Expected rewrite: const raw_ptr<const SomeClass> ptr_field7_;
  const raw_ptr<const SomeClass> ptr_field7_;

  // Expected rewrite: base::raw_span<const SomeClass> span_field1_;
  base::raw_span<const SomeClass> span_field1_;

  // Expected rewrite: const base::raw_span<const SomeClass> span_field2_;
  const base::raw_span<const SomeClass> span_field2_;

  // Expected rewrite: base::raw_span<volatile SomeClass> span_field3_;
  base::raw_span<volatile SomeClass> span_field3_;
};
