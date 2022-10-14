// Copyright 2020 The Chromium Authors
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

namespace field_initializer_in_constexpr_ctor {

class Foo {
 public:
  constexpr explicit Foo(int* ptr, int& ref)
      : ptr_(ptr), ptr2_(ptr), null_(nullptr), ref_(ref), ref2_(ref) {}

 private:
  // raw_ptr(T*) constructor is non-constexpr and therefore raw_ptr fields
  // cannot be initialized in constexpr constructors - such fields should be
  // emitted as candidates for the --field-filter-file.
  int* ptr_;

  // Testing that all initializers and fields are covered (i.e. not just the
  // first one).
  int* ptr2_;

  // raw_ptr(nullptr_t) is constexpr and therefore the field below doesn't
  // need to be skipped.
  int* null_;

  // raw_ref(T&) constructor is non-constexpr and therefore raw_ref fields
  // cannot be initialized in constexpr constructors - such fields should be
  // emitted as candidates for the --field-filter-file.
  int& ref_;

  // Testing that all initializers and fields are covered (i.e. not just the
  // first one).
  int& ref2_;
};

}  // namespace field_initializer_in_constexpr_ctor

namespace constexpr_variable_initializer {

void foo() {
  // The |str| field below should be emitted as a candidate for the
  // --field-filter-file using the "constexpr-var-initializer" rule.
  //
  // This example is based on UtfOffsetTest.Utf8OffsetFromUtf16Offset in
  // //ui/base/ime/utf_offset_unittest.cc
  //
  // Note that in this example, kTestCases does not have a global scope and
  // therefore won't be covered by the "global-scope" heuristic.  Similarly,
  // there is no explicit constexpr constructor here, so the example won't be
  // covered by the "constexpr-ctor-field-initializer" heuristic.
  constexpr struct {
    const char16_t* str;
    int offset;
  } kTestCases[] = {
      {u"ab", 0},
      {u"ab", 1},
      {u"ab", 2},
  };
}

}  // namespace constexpr_variable_initializer

namespace constexpr_variable_uninitialized_field {

void foo() {
  // The |str| field is not covered by the initializers below and therefore
  // should not be emitted as a --field-filter-file candidate.
  constexpr struct {
    int i1;
    const char16_t* str;
  } kTestCases[] = {
      {0},
      {1},
      {2},
  };
}

}  // namespace constexpr_variable_uninitialized_field

namespace constexpr_variable_designated_initializers {

void foo() {
  // The |str2| and |str3| fields below (but not |str_uncovered|) are
  // initialized by a designated initializer and should be emitted as a
  // --field-filter-file candidate.
  constexpr struct {
    int i1;
    const char* str_uncovered;
    const char* str_nullptr;
    const char* str2;
    const char* str3;
  } kTestCases[] = {
      // Test to verify that all designated initializers are covered.
      {.str2 = "blah", .str3 = "foo"},
      // Tests to verify that nullptr initialization doesn't exclude a field
      // (since BackupRefPtr has a constexpr ctor for nullptr_t).
      {.str_nullptr = nullptr},
      // Tests to verify that we don't accidentally cover |str_uncovered|.
      {1},
      {.i1 = 2},
  };
}

}  // namespace constexpr_variable_designated_initializers

namespace implicit_constexpr_ctor {

struct Foo {
  void* expect_rewrite = nullptr;
};

class Bar {
  // The compiler will implicitly generate a constexpr constructor. But we want
  // to rewrite Foo::expect_rewrite_.
  Bar(){};
  Foo info_;
};

struct Baz {
  constexpr Baz(const void* ptr) : no_rewrite(ptr) {}
  const void* no_rewrite;
};

}  // namespace implicit_constexpr_ctor
