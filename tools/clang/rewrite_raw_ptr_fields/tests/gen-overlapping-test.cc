// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file (and other gen-*-test.cc files) tests generation of output for
// --field-filter-file and therefore the expectations file
// (gen-overlapping-expected.txt) needs to be compared against the raw output of
// the rewriter (rather than against the actual edits result).  This makes the
// test incompatible with other tests, which require passing --apply-edits
// switch to test_tool.py and so to disable the test it is named *-test.cc
// rather than *-original.cc.
//
// To run the test use tools/clang/rewrite_raw_ptr_fields/tests/run_all_tests.py

namespace my_namespace {

class SomeClass {
  int x;
};

struct MyStruct {
  // The fields below have an overlapping |replacement_range| and therefore
  // should be emitted as candidates for --field-filter-file.
  SomeClass *overlapping_1a, *overlapping_1b;

  // It is sufficient to emit pointer fields (e.g. no need to emit
  // overlapping_2b or overlapping_3a).
  SomeClass *overlapping_2a, overlapping_2b;
  SomeClass overlapping_3a, *overlapping_3b;

  // Definition of the struct overlaps with the |replacement_range| of the
  // |ptr_to_non_free_standing_struct|.  Therefore the field should be emitted
  // as a candidate for --field-filter-file.
  struct NonFreeStandingStruct {
    int non_ptr;
  } * ptr_to_non_free_standing_struct;

  // Similarly to the above, definition of the struct overlaps with the
  // |replacement_range| of |ptr_to_non_free_standing_struct2|.  OTOH, it is
  // okay to proceed with rewriting |inner_ptr| - it should not be emitted as a
  // candidate for --field-filter-file.
  struct NonFreeStandingStruct2 {
    SomeClass* inner_ptr;
  } * ptr_to_non_free_standing_struct2;
};

}  // namespace my_namespace
