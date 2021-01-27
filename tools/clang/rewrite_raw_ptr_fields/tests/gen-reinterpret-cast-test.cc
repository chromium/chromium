// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

class ReinterpretedClass2 {
  // The field below should be emitted as candidates for the
  // --field-filter-file, because `const ReinterpretedClass2*` is used as the
  // target type of `reinterpret_cast` expressions.  See also
  // https://crbug.com/1165613.
  int* ptr_;
};

class SomeOtherClass {
  // This field should not be emitted as a candidate for --field-filter-file.
  int* ptr_;
};

void foo() {
  void* void_ptr = nullptr;
  auto* p1 = reinterpret_cast<ReinterpretedClass1*>(void_ptr);
  auto* p2 = reinterpret_cast<const ReinterpretedClass2*>(void_ptr);
}
