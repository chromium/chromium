// Copyright 2020 The Chromium Authors
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
  SomeClass *overlapping_ptr_1a, *overlapping_ptr_1b;

  // It is sufficient to emit pointer fields (e.g. no need to emit
  // overlapping_ptr_2b or overlapping_ptr_3a).
  SomeClass *overlapping_ptr_2a, overlapping_ptr_2b;
  SomeClass overlapping_ptr_3a, *overlapping_ptr_3b;

  // The fields below have an overlapping |replacement_range| and therefore
  // should be emitted as candidates for --field-filter-file.
  SomeClass &overlapping_ref_1a, &overlapping_ref_1b;

  // It is sufficient to emit pointer fields (e.g. no need to emit
  // overlapping_ref_2b or overlapping_ref_3a).
  SomeClass &overlapping_ref_2a, overlapping_ref_2b;
  SomeClass overlapping_ref_3a, &overlapping_ref_3b;

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
    SomeClass& inner_ref;
  } * ptr_to_non_free_standing_struct2;
};

}  // namespace my_namespace
