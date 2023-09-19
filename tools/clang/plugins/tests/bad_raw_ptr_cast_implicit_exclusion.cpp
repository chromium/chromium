// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

struct RawPtrWrapper {
  raw_ptr<int> ptr;
};

void OverloadedFunction(void* p) {}
void OverloadedFunction(int v) {}

namespace test {
void NormalFunc(void* p) {}
void AllowlistedFunc(void* p) {}
struct AllowlistedConstructor {
  explicit AllowlistedConstructor(void* p) {}
};
struct NormalConstructor {
  explicit NormalConstructor(void* p) {}
};
}  // namespace test

// 'unsafe_raw_ptr_*_cast' should not emit errors.
void CastToCastingUnsafeExclusion() {
  void* p = nullptr;
  RawPtrWrapper* q = nullptr;

  // Base case: should error.
  (void)reinterpret_cast<RawPtrWrapper*>(p);
  (void)reinterpret_cast<void*>(q);

  // Casts to const built-in typed pointers should be excluded.
  (void)reinterpret_cast<const void*>(q);
  (void)reinterpret_cast<const char*>(q);

  // Casts in allowlisted invocation context should be excluded.
  test::NormalFunc(q);                    // Not allowlisted
  test::AllowlistedFunc(q);               // Allowlisted
  (void)test::NormalConstructor(q);       // Not allowlisted
  (void)test::AllowlistedConstructor(q);  // Allowlisted

  // Casts in comparison context should be excluded.
  (void)(p == q);
  //          ^ implicit cast from |RawPtrWrapper*| to |void*| here.

  // Implicit casts in invocation inside template context should be excluded.
  auto f = [](auto* x) { OverloadedFunction(x); };
  f(p);
  f(q);

  // Casts that |isNotSpelledInSource()| should be excluded.
#define ANY_CAST(type) type##_cast
  (void)ANY_CAST(reinterpret)<RawPtrWrapper*>(p);
  //    ^~~~~~~~ token "reinterpret_cast" is in <scratch space>.

  // Casts that |isInThirdPartyLocation()| should be excluded.
#line 1 "../../third_party/fake_loc/bad_raw_ptr_cast_implicit_exclusion.cpp"
  (void)reinterpret_cast<RawPtrWrapper*>(p);

  // Casts that |isInLocationListedInFilterFile(...)| should be excluded.
#line 1 "../../internal/fake_loc/bad_raw_ptr_cast_implicit_exclusion.cpp"
  (void)reinterpret_cast<RawPtrWrapper*>(p);

  // Casts in allowlisted paths should be excluded.
#line 1 "../../ppapi/fake_loc/bad_raw_ptr_cast_implicit_exclusion.cpp"
  (void)reinterpret_cast<RawPtrWrapper*>(p);
}
