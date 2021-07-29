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

struct MyStruct {
  // Chromium is built with a warning/error that there are no user-defined
  // constructors invoked when initializing global-scoped values.
  // CheckedPtr<char> conversion might trigger a global constructor call when a
  // pointer field is initialized with a non-null value.  This frequently
  // happens when initializing |const char*| fields with a string literal:
  //     struct MyStruct {
  //       int foo;
  //       CheckedPtr<const char> bar;
  //     }
  //     MyStruct g_foo = {123, "string literal" /* global constr! */};
  //
  // Because of the above, we have a heuristic for all fields that point to a
  // char-like type - the heuristic causes such fields to be emitted as
  // candidates for the --field-filter-file.
  const char* const_char_ptr;
  const wchar_t* const_wide_char_ptr;

  // String literals cannot be used to initialize non-const fields - such usage
  // would trigger the following warning: ISO C++11 does not allow conversion
  // from string literal to 'char *' [-Wwritable-strings].
  //
  // Because of the above, we want to exclude non-const char pointers from the
  // heuristic that excludes such fields from the rewrite.  Avoiding the
  // double-negative: we want to include the fields below in the rewrite.
  char* char_ptr;
  wchar_t* wide_char_ptr;
};
