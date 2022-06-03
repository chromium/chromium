// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file (and other gen-*-test.cc files) tests generation of output for
// --field-filter-file and therefore the expectations file
// (gen-global-destructor-expected.txt) needs to be compared against the raw
// output of the rewriter (rather than against the actual edits result).  This
// makes the test incompatible with other tests, which require passing
// --apply-edits switch to test_tool.py and so to disable the test it is named
// *-test.cc rather than *-original.cc.
//
// To run the test use tools/clang/rewrite_raw_ptr_fields/tests/run_all_tests.py

// Chromium is built with a warning/error that global and static variables
// may only have trivial destructors.  See also:
// https://google.github.io/styleguide/cppguide.html#Static_and_Global_Variables
// go/totw/110#destruction
//
// If raw_ptr has a non-trivial destructor (e.g. if it is implemented via
// BackupRefPtr) then raw_ptr cannot be used as the type of fields in structs
// that are (recursively/transitively) the type of a global variable:
//     struct MyStruct {       //    Presence of raw_ptr might mean that
//       raw_ptr<int> ptr;  // <- MyStruct has a non-trivial destructor.
//     };
//     MyStruct g_struct;  // <- Error if MyStruct has a non-trivial destructor.
//
// To account for the constraints described above, the rewriter tool should
// avoid rewriting some of the fields below.

namespace global_variables_test {

struct MyStruct {
  // Expected to be emitted in automated-fields-to-ignore.txt, because
  // of |g_struct| below.
  int* ptr;

  // Verification that *all* fields of a struct are covered (e.g. that the
  // |forEach| matcher is used instead of the |has| matcher).
  int* ptr2;
};

MyStruct g_struct;

}  // namespace global_variables_test

namespace static_variables_test {

struct MyStruct {
  // Expected to be emitted in automated-fields-to-ignore.txt, because
  // of |s_struct| below.
  int* ptr;
};

void foo() {
  static MyStruct s_struct;
}

}  // namespace static_variables_test

namespace nested_struct_test {

struct MyStruct {
  // Expected to be emitted in automated-fields-to-ignore.txt, because
  // of |g_outer_struct| below.
  int* ptr;
};

struct MyOuterStruct {
  MyStruct inner_struct;
};

static MyOuterStruct g_outer_struct;

}  // namespace nested_struct_test

namespace nested_in_array_test {

struct MyStruct {
  // Expected to be emitted in automated-fields-to-ignore.txt, because
  // of |g_outer_array| below.
  int* ptr;
};

static MyStruct g_outer_struct[] = {nullptr, nullptr, nullptr};

}  // namespace nested_in_array_test

namespace nested_template_test {

template <typename T>
struct MyStruct {
  // Expected to be emitted in automated-fields-to-ignore.txt, because
  // of |g_outer_struct| below.
  T* ptr;
};

struct MyOuterStruct {
  MyStruct<int> inner_struct;
};

static MyOuterStruct g_outer_struct;

}  // namespace nested_template_test

namespace pointer_nesting_test {

struct MyStruct {
  // Should not be emitted in automated-fields-to-ignore.txt, because
  // |inner_struct| field below is a pointer.  (i.e. this is a test that
  // |hasNestedFieldDecl| matcher doesn't recurse/traverse over pointers)
  int* ptr;
};

struct MyOuterStruct {
  // Expected to be emitted in automated-fields-to-ignore.txt, because
  // of |g_outer_struct| below.
  MyStruct* inner_struct;
};

static MyOuterStruct g_outer_struct;

}  // namespace pointer_nesting_test
