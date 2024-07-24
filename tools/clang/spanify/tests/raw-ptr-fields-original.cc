// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

struct S {
  S(int* ptr) : ptr_(ptr) {}
  int* get();
  // TODO: Currently return type is rewritten to: const base::span<int>
  // Expected rewrite:
  // static inline base::span<const int>
  // SetWrapperInInlineStorage(base::span<int> isolate, int* object);
  template <bool entered_context = true>
  static inline const int* SetWrapperInInlineStorage(int* isolate, int* object);
  raw_ptr<int> ptr_;
};

// TODO: Currently return type is rewritten to: const base::span<int>
// Expected rewrite:
// base::span<const int> SetWrapperInInlineStorage(base::span<int> isolate, int*
// object);
template <bool entered_context>
const int* S::SetWrapperInInlineStorage(int* isolate, int* object) {
  (void)isolate[0];
  return isolate;
}

int* S::get() {
  // Expected rewrite:
  // const int* temp = ptr_.data();
  const int* temp = ptr_.get();
  // Expected rewrite:
  // temp = SetWrapperInInlineStorage({}, nullptr).data();
  temp = SetWrapperInInlineStorage(nullptr, nullptr);

  // Expected rewrite:
  // base::span<const int> ptr = SetWrapperInInlineStorage({}, nullptr);
  const int* ptr = SetWrapperInInlineStorage({}, nullptr);
  // Leads ptr to be rewritten, and thus SetWrapperInInlineStorage return type.
  (void)ptr[0];

  // Expected rewrite:
  // base::span<const int> temp2 = ptr_;
  const int* temp2 = ptr_.get();

  // Expected rewrite:
  // const base::span<const int> temp3 = temp2;
  const int* const temp3 = temp2;

  // Expected rewrite:
  // base::span<const int> temp4 = temp3;
  const auto* temp4 = temp3;

  // Expected rewrite:
  // const base::span<const int> temp5 = temp4;
  const auto* const temp5 = temp4;
  // Leads temp5 to be rewritten.
  (void)temp5[0];

  return ptr_;
}

char fct() {
  // Expected rewrite:
  // base::span<int> var = {};
  int* var = nullptr;
  S obj(var);
  // Expected rewrite:
  // base::span<int> v2 = obj.get();
  int* v2 = obj.get();
  return v2[0];
}

struct S2 {
  // Expected rewrite:
  // S2(base::span<int> ptr) : ptr_(ptr) {}
  S2(int* ptr) : ptr_(ptr) {}

  int* get_and_advance() {
    // Expected rewrite:
    // return ptr_++.data();
    return ptr_++;
  }

  // Expected rewrite:
  // base::raw_span<int> ptr_;
  raw_ptr<int> ptr_;
};

void fct2() {
  // Expected rewrite:
  // S2 obj({});
  S2 obj(nullptr);
}
