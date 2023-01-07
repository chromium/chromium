// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

// This file (and other gen-*-test.cc files) tests generation of output for
// --field-filter-file and therefore the expectations file
// (gen-char-expected.txt) needs to be compared against the raw output of the
// rewriter (rather than against the actual edits result).  This makes the test
// incompatible with other tests, which require passing --apply-edits switch to
// test_tool.py and so to disable the test it is named *-test.cc rather than
// *-original.cc.
//
// To run the test use tools/clang/rewrite_raw_ptr_fields/tests/run_all_tests.py

class ReinterpretedClass1 {
  // The field below should be emitted as candidates for the
  // --field-filter-file, because `ReinterpretedClass1*` is used as the
  // target type of `reinterpret_cast` expressions.  See also
  // https://crbug.com/1165613.
  int* ptr_;

  // All fields in ReinterpretedClass1 should be emitted.
  int* ptr2_;
};
static_assert(std::is_trivial<ReinterpretedClass1>::value,
              "ReinterpretedClass1 is trivial");

class ReinterpretedClass2 {
  // The field below should be emitted as candidates for the
  // --field-filter-file, because `const ReinterpretedClass2*` is used as the
  // target type of `reinterpret_cast` expressions.  See also
  // https://crbug.com/1165613.
  int* ptr_;
};
static_assert(std::is_trivial<ReinterpretedClass2>::value,
              "ReinterpretedClass2 is trivial");

class ReinterpretedNonTrivialClass3 {
  // User-defined constructor means that ReinterpretedNonTrivialClass3 is
  // non-trivial.
  ReinterpretedNonTrivialClass3() : ptr_(nullptr) {}

  // This field should not be emitted as a candidate for --field-filter-file,
  // because we only want to exclude cases where a `reinterpret_cast` is 1)
  // valid before the rewrite and 2) invalid after the rewrite (e.g. because it
  // skips raw_ptr's constructors).  A reinterpret_cast of a pointer to
  // non-trivial type would have been invalid before the rewrite if it skipped
  // the (non-trivial) constructors.  See also the discussion in
  // https://groups.google.com/a/google.com/g/chrome-memory-safety/c/MwnBj_EuILg/m/1cVmcBOMBAAJ
  int* ptr_;
};
static_assert(!std::is_trivial<ReinterpretedNonTrivialClass3>::value,
              "ReinterpretedNonTrivialClass3 is *not* trivial");

class SomeOtherClass {
  // This field should not be emitted as a candidate for --field-filter-file.
  int* ptr_;
};
static_assert(std::is_trivial<SomeOtherClass>::value,
              "SomeOtherClass is trivial");

void foo() {
  void* void_ptr = nullptr;
  auto* p1 = reinterpret_cast<ReinterpretedClass1*>(void_ptr);
  auto* p2 = reinterpret_cast<const ReinterpretedClass2*>(void_ptr);
  auto* p3 = reinterpret_cast<const ReinterpretedNonTrivialClass3*>(void_ptr);
}
