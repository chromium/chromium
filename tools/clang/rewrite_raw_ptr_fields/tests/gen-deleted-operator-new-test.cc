// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

// This file (and other gen-*-test.cc files) tests generation of output for
// --field-filter-file and therefore the expectations file
// (gen-char-expected.txt) needs to be compared against the raw output of the
// rewriter (rather than against the actual edits result).  This makes the test
// incompatible with other tests, which require passing --apply-edits switch to
// test_tool.py and so to disable the test it is named *-test.cc rather than
// *-original.cc.
//
// To run the test use tools/clang/rewrite_raw_ptr_fields/tests/run_all_tests.py

// The class below deletes the |operator new| - this simulate's Blink's
// STACK_ALLOCATED macro and/or OilPan / GarbageCollected<T> classes.
//
// We assume that NoNewOperator classes are never allocated on the heap (i.e.
// are never managed by PartitionAlloc) and therefore would not benefit from
// the protection of CheckedPtr.  This assumption/heuristic is generally true,
// but not always - for details how WTF::Vector can allocate elements without
// operator new, see (Google-internal)
// https://groups.google.com/a/google.com/g/chrome-memory-safety/c/GybWkNGqSyk/m/pUOjMvK5CQAJ
class NoNewOperator {
  void* operator new(size_t) = delete;

  // Based on the deleted-oparator-new assumption/heuristic above, we assume
  // that NoNewOperator will always be allocated on the stack or in OilPan and
  // therefore doesn't need CheckedPtr protection (since it will be protected
  // either by StackScanning or by OilPan's tracing and GC).  Therefore,
  // |no_operator_new_struct| should be emitted as candidates for the
  // --field-filter-file with "embedder-has-no-operator-new" tag.
  int* field_in_struct_with_no_operator_new;
};

struct MyStruct {
  // Based on the deleted-oparator-new assumption/heuristic above, we assume
  // that NoNewOperator is never allocated on the PartitionAlloc-managed heap.
  // This means that CheckedPtr would always be disabled for the field below and
  // therefore the test below checks that |no_operator_new_pointee| is emitted
  // as a candidate for the --field-filter-file with
  // "pointee-has-no-operator-new" tag.
  NoNewOperator* pointer_to_pointee_with_no_operator_new;
};
