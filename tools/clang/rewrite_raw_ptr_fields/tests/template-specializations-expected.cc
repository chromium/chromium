// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for template specializations.
//
// In template specializations template parameters (e.g. |T| or |T2| in
// MyTemplate below) get substituted with an actual class (e.g. |SomeClass| or
// |int|).  In an *implicit* specialization, these substitutions are "overlaid"
// / "overimposed" on top of the template definition and this can lead to
// generating conflicting replacements - for example the same |t_ptr_field|
// definition can get replaced with:
// 1. T* t_ptr_field  ->  raw_ptr<T> t_ptr_field            // expected
// 2. T* t_ptr_field  ->  raw_ptr<SomeClass> t_ptr_field    // undesired
//
// To avoid generating conflicting replacements, the rewriter excludes implicit
// template specializations via |implicit_field_decl_matcher|.
//
// Note that rewrites in *explicit* template specializations are still
// desirable.  For example, see the |T2* t2_ptr_field| in |MyTemplate<int, T2>|
// partial template specialization.
#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"

class SomeClass;
class SomeClass2;

template <typename T, typename T2>
class MyTemplate {
  // Expected rewrite: raw_ptr<T> t_ptr_field;
  raw_ptr<T> t_ptr_field;

  // Expected rewrite: raw_ptr<SomeClass> some_class_ptr_field;
  raw_ptr<SomeClass> some_class_ptr_field;

  // No rewrite expected.
  int int_field;

  // Expected rewrite: base::raw_span<T> span_field1;
  base::raw_span<T> span_field1;
  // Expected rewrite: base::raw_span<SomeClass> span_field2;
  base::raw_span<SomeClass> span_field2;
  // Expected rewrite: std::optional<base::raw_span<T>> optional_span_field1;
  std::optional<base::raw_span<T>> optional_span_field1;
  // Expected rewrite: std::optional<base::raw_span<SomeClass>>
  // optional_span_field2;
  std::optional<base::raw_span<SomeClass>> optional_span_field2;
};

// Partial *explicit* specialization.
template <typename T2>
class MyTemplate<int, T2> {
  // Expected rewrite: raw_ptr<T2> t2_ptr_field;
  raw_ptr<T2> t2_ptr_field;

  // Expected rewrite: raw_ptr<SomeClass> some_class_ptr_field;
  raw_ptr<SomeClass> some_class_ptr_field;

  // Expected rewrite: raw_ptr<int> int_ptr_field;
  raw_ptr<int> int_ptr_field;

  // No rewrite expected.
  int int_field;

  // Expected rewrite: base::raw_span<T2> span_field1;
  base::raw_span<T2> span_field1;
  // Expected rewrite: base::raw_span<SomeClass> span_field2;
  base::raw_span<SomeClass> span_field2;
  // Expected rewrite: std::optional<base::raw_span<int>> optional_span_field1;
  std::optional<base::raw_span<int>> optional_span_field1;
  // Expected rewrite: std::optional<base::raw_span<T2>> optional_span_field2;
  std::optional<base::raw_span<T2>> optional_span_field2;
};

// Full *explicit* specialization.
template <>
class MyTemplate<int, SomeClass2> {
  // Expected rewrite: raw_ptr<int> int_ptr_field;
  raw_ptr<int> int_ptr_field;

  // Expected rewrite: raw_ptr<SomeClass2> some_class2_ptr_field;
  raw_ptr<SomeClass2> some_class2_ptr_field;

  // No rewrite expected.
  int int_field;

  // Expected rewrite: base::raw_span<T2> span_field1;
  base::raw_span<int> span_field1;
  // Expected rewrite: base::raw_span<SomeClass> span_field2;
  base::raw_span<SomeClass2> span_field2;
  // Expected rewrite: std::optional<base::raw_span<int>> optional_span_field1;
  std::optional<base::raw_span<int>> optional_span_field1;
  // Expected rewrite: std::optional<base::raw_span<T2>> optional_span_field2;
  std::optional<base::raw_span<SomeClass2>> optional_span_field2;
};

// The class definitions below trigger an implicit template specialization of
// MyTemplate.
class TemplateDerived : public MyTemplate<SomeClass, int> {};
class TemplateDerived2 : public MyTemplate<SomeClass2, int> {};

// Test where excluding SubstTemplateTypeParmType pointees is not sufficient,
// because the pointee is not |T|, but |TemplateSelfPointerTest<T>| like in
// the fields below.
//
// This test forces using
//     classTemplateSpecializationDecl(isImplicitSpecialization())
// in the definition of |implicit_field_decl_matcher|.
// Note that no |hasAncestor| matcher is necessary - compare with
// nested_iterator_test below.
namespace self_pointer_test {

template <typename T>
class TemplateSelfPointerTest {
  // Early versions of the rewriter used to rewrite the type below to three
  // conflicting replacements:
  // 1. raw_ptr<TemplateSelfPointerTest<bool>>
  // 2. raw_ptr<TemplateSelfPointerTest<SomeClass2>>
  // 3. raw_ptr<TemplateSelfPointerTest<T>>
  //
  // Something similar would have happened in //base/scoped_generic.h (in the
  // nested Receiver class):
  //   ScopedGeneric* scoped_generic_;
  //
  // Expected rewrite: raw_ptr<TemplateSelfPointerTest<T>>
  raw_ptr<TemplateSelfPointerTest<T>> ptr_field_;

  // Similar test to the above.  Something similar would have happened in
  // //base/id_map.h (in the nested Iterator class):
  //   IDMap<V, K>* map_;
  //
  // Expected rewrite: raw_ptr<TemplateSelfPointerTest<T>>
  raw_ptr<TemplateSelfPointerTest<T>> ptr_field2_;

  // Expected rewrite: base::raw_span<TemplateSelfPointerTest> span_field1;
  base::raw_span<TemplateSelfPointerTest> span_field1;
  // Expected rewrite: base::raw_span<TemplateSelfPointerTest<T>> span_field2;
  base::raw_span<TemplateSelfPointerTest<T>> span_field2;
  // Expected rewrite: std::optional<base::raw_span<TemplateSelfPointerTest>>
  // optional_span_field1;
  std::optional<base::raw_span<TemplateSelfPointerTest>> optional_span_field1;
  // Expected rewrite: std::optional<base::raw_span<TemplateSelfPointerTest<T>>>
  // optional_span_field2;
  std::optional<base::raw_span<TemplateSelfPointerTest<T>>>
      optional_span_field2;
};

void foo() {
  // Variable declarations below trigger an implicit template specialization of
  // TemplateSelfPointerTest.
  TemplateSelfPointerTest<bool> foo;
  TemplateSelfPointerTest<SomeClass2> bar;
}

}  // namespace self_pointer_test

// Test against overlapping replacement that occurred in Chromium in places
// like:
// - //components/url_pattern_index/string_splitter.h
//   |const StringSplitter* splitter_| in nested Iterator class
// - //base/callback_list.h
//   |CallbackListBase<CallbackType>* list_| in nested Iterator class
// - //mojo/public/cpp/bindings/receiver_set.h
//   |ReceiverSetBase* const receiver_set_| in nested Entry class
//
// This test forces using
//     hasAncestor(classTemplateSpecializationDecl(isImplicitSpecialization()))
// in the definition of |implicit_field_decl_matcher|.
namespace nested_iterator_test {

template <typename T>
class StringSplitter {
 public:
  class Iterator {
   public:
    Iterator(const StringSplitter& splitter) : splitter_(&splitter) {}

   private:
    // Danger of an overlapping replacement (when substituting
    // |StringSplitter<T>| for |StringSplitter<int>| in an implicit template
    // specialization triggered by the |foo2| function below.
    //
    // Expected rewrite: raw_ptr<const StringSplitter<T>> splitter_
    raw_ptr<const StringSplitter<T>> splitter_;

    // Expected rewrite: base::raw_span<const StringSplitter> span_field
    base::raw_span<const StringSplitter> span_field;
  };

  Iterator begin() const { return Iterator(*this); }
};

void foo2() {
  StringSplitter<int> splitter;
  auto iterator = splitter.begin();
}

}  // namespace nested_iterator_test

// Example based on base/trace_event/memory_usage_estimator.h where a function
// template |EstimateMemoryUsage| had a nested struct |SharedPointer| definition
// with a pointer field |value| that was leading to conflicting replacements.
namespace template_function {

template <typename T>
void foo(T* arg) {
  struct NestedStruct {
    // Expected rewrite: raw_ptr<T> ptr_field;
    raw_ptr<T> ptr_field;

    // Expected rewrite: raw_ptr<MyTemplate<T, T>> ptr_field2;
    raw_ptr<MyTemplate<T, T>> ptr_field2;

    // Expected rewrite: base::raw_span<T> span_field;
    base::raw_span<T> span_field;
  } var;

  var.ptr_field = nullptr;
}

void bar() {
  // Triggering implicit specializations of foo that in the past led the
  // rewriter to generate conflicting replacements.
  int i = 123;
  SomeClass* p = nullptr;
  foo(p);
  foo(&i);
}

}  // namespace template_function
