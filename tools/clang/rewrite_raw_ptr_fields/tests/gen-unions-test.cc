// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file (and other gen-*-test.cc files) tests generation of output for
// --field-filter-file and therefore the expectations file
// (gen-return-ref-expected.txt) needs to be compared against the raw output of
// the rewriter (rather than against the actual edits result).  This makes the
// test incompatible with other tests, which require passing --apply-edits
// switch to test_tool.py and so to disable the test it is named *-test.cc
// rather than *-original.cc.
//
// To run the test use tools/clang/rewrite_raw_ptr_fields/tests/run_all_tests.sh

#include <stdint.h>

namespace my_namespace {

class SomeClass;

struct MyStruct {
  SomeClass* ptr_field;
};

class MyClass {
  SomeClass* ptr_field;
};

union MyUnion1 {
  SomeClass* some_class_ptr;
  char* char_ptr;
  // TODO(crbug.com/40245402) |const char| pointer fields are not supported yet.
  const char* const_char_ptr;
};

union MyUnion2 {
  SomeClass* some_class_ptr;
  uintptr_t uintptr;
};

union MyUnion3 {
  SomeClass* some_class_ptr;
  SomeClass* some_class_ptr2;
};

struct MyNestedStruct {
  SomeClass* ptr_field;
};

union MyUnion4 {
  MyNestedStruct nested_struct;
  uintptr_t uintptr;
};

}  // namespace my_namespace
