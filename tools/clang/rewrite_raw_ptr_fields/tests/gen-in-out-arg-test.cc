// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file (and other gen-*-test.cc files) tests generation of output for
// --field-filter-file and therefore the expectations file
// (gen-in-out-arg-expected.txt) needs to be compared against the raw output of
// the rewriter (rather than against the actual edits result).  This makes the
// test incompatible with other tests, which require passing --apply-edits
// switch to test_tool.py and so to disable the test it is named *-test.cc
// rather than *-original.cc.
//
// To run the test use tools/clang/rewrite_raw_ptr_fields/tests/run_all_tests.py

namespace my_namespace {

class SomeClass;

struct MyStruct {
  SomeClass* ptr_field;
  SomeClass* in_out_via_ptr;
  SomeClass* in_out_via_ref;
  SomeClass* in_out_via_auto_reset;
  SomeClass* not_in_out;
};

template <typename T>
class AutoReset {
 public:
  AutoReset(T* ptr, T value) : ptr_(ptr), value_(value) {}
  ~AutoReset() { *ptr_ = value_; }

 private:
  T* ptr_;
  T value_;
};

void GetViaPtr(SomeClass** out_ptr) {
  *out_ptr = nullptr;
}

// Based on a real-world example (Blink uses references more often than the rest
// of Chromium):
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/layout/layout_table.cc;drc=a3524fd6d1a4f4ff7e97893f6c6375dd1684e132;l=130
void GetViaRef(SomeClass*& out_ptr) {
  out_ptr = nullptr;
}

// Based on trace_event_internal::AddTraceEvent.  This test verifies that
// regular references are covered, but RValue references are excluded.
template <typename T>
void GetViaRValue(T&& param) {}

// Based on base::Bind.  Verifies that RValue references are excluded when used
// as a template parameter pack.
template <typename... TArgs>
void GetViaRValuePack(TArgs&&... param_pack) {}

// Based on std::sort.  Verifies that undecorated, plain |T| is not matched
// (e.g. when it is hypothetically instantiated as something like
// |SomeClass*&|).
template <typename T>
void GetViaPlainT(T t) {}

void foo() {
  MyStruct my_struct;
  GetViaPtr(&my_struct.in_out_via_ptr);
  GetViaRef(my_struct.in_out_via_ref);
  AutoReset<SomeClass*> auto_reset1(&my_struct.in_out_via_auto_reset, nullptr);

  // RValue references should *not* appear in the "FIELD FILTERS" section of the
  // output, with "in-out-param-ref" tag (this requires special care in the
  // rewriter, because a RValueReferenceType is derived from ReferenceType).
  GetViaRValue(my_struct.not_in_out);
  GetViaRValuePack(my_struct.not_in_out);
  GetViaRValuePack(1, 2, 3, my_struct.not_in_out);

  // Plain T template parameters should *not* appear in the "FIELD FILETS"
  // section of the output.
  GetViaRValuePack(my_struct.not_in_out);
}

template <typename T>
class MyTemplateBase {
 protected:
  T* ptr_;
};

class MyTemplateDerived : public MyTemplateBase<SomeClass> {
 public:
  void foo() {
    // This should emit
    //     my_namespace MyTemplateBase<T>::ptr_
    // rather than
    //     my_namespace MyTemplateBase<SomeClass>::ptr_
    GetViaPtr(&ptr_);
  }
};

}  // namespace my_namespace
