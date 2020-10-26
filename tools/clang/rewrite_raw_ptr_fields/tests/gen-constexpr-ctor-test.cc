// Copyright 2020 The Chromium Authors. All rights reserved.
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

class Foo {
 public:
  constexpr explicit Foo(int* ptr) : ptr_(ptr), ptr2_(ptr), null_(nullptr) {}

 private:
  // CheckedPtr(T*) constructor is non-constexpr and therefore CheckedPtr fields
  // cannot be initialized in constexpr constructors - such fields should be
  // emitted as candidates for the --field-filter-file.
  int* ptr_;

  // Testing that all initializers and fields are covered (i.e. not just the
  // first one).
  int* ptr2_;

  // CheckedPtr(nullptr_t) is constexpr and therefore the field below doesn't
  // need to be skipped.
  int* null_;
};
