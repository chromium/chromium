// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>  // for uintptr_t

#include <string>
#include <tuple>    // for std::tie
#include <utility>  // for std::swap

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

class SomeClass {};
class DerivedClass : public SomeClass {};

struct MyStruct {
  MyStruct(SomeClass& ref1, SomeClass& ref2, const SomeClass& ref3)
      : ref1_(ref1), ref2_(ref2), const_ref_(ref3) {}
  raw_ptr<SomeClass> ptr;
  raw_ptr<SomeClass> ptr2;
  raw_ptr<const SomeClass> const_ptr;
  int (*func_ptr_field)();
  const char* const_char_ptr;

  // Expected rewrite: const raw_ref<SomeClass> ref1_;
  const raw_ref<SomeClass> ref1_;
  // Expected rewrite: const raw_ref<SomeClass> ref2_;
  const raw_ref<SomeClass> ref2_;
  // Expected rewrite: const raw_ref<const SomeClass> const_ref_;
  const raw_ref<const SomeClass> const_ref_;
};

namespace auto_tests {

MyStruct* GetMyStruct() {
  return nullptr;
}

SomeClass* GetSomeClass() {
  return nullptr;
}

SomeClass* ConvertSomeClassToSomeClass(SomeClass* some_class) {
  return some_class;
}

void foo() {
  SomeClass s;
  MyStruct my_struct(s, s, s);

  // After the rewrite |my_struct.ptr_field| is no longer a pointer,
  // so |auto*| won't work.  We fix this up, by appending |.get()|.
  // Expected rewrite: auto* ptr_var = my_struct.ptr.get();
  auto* ptr_var = my_struct.ptr.get();

  // Tests for other kinds of initialization.
  // Expected rewrite: |.get()| should be appended in both cases below.
  auto* init_test1(my_struct.ptr.get());
  auto* init_test2{my_struct.ptr.get()};

  // Test for handling of the |const| qualifier.
  // Expected rewrite: const auto* ptr_var = my_struct.ptr.get();
  const auto* const_ptr_var = my_struct.ptr.get();

  // More complicated initialization expression, but the |ptr_field| struct
  // member dereference is still the top/last expression here.
  // Expected rewrite: ...->ptr.get()
  auto* complicated_var = GetMyStruct()->ptr.get();

  // The test below covers:
  // 1. Two variables with single |auto|,
  // 2. Tricky placement of |*| (next to the variable name).
  // Expected rewrite: ...ptr.get()... (twice in the 2nd example).
  auto *ptr_var1 = my_struct.ptr.get(), *ptr_var2 = GetSomeClass();
  auto *ptr_var3 = my_struct.ptr.get(), *ptr_var4 = my_struct.ptr.get();
  auto *ptr_var5 = GetSomeClass(), *ptr_var6 = my_struct.ptr.get();

  // Test for the case where
  // 1. The resulting type is the same as in the |ptr_var| and |complicated_var|
  //    examples
  // 2. Deep in the initialization expression there is a member dereference
  //    of |ptr_field|
  // but
  // 3. The final/top-level initialization expression doesn't dereference
  //    |ptr_field|.
  // No rewrite expected.
  auto* not_affected_field_var = ConvertSomeClassToSomeClass(my_struct.ptr);

  // Test for pointer |auto| assigned from non-raw_ptr-elligible field.
  // No rewrite expected.
  auto* func_ptr_var = my_struct.func_ptr_field;

  // Test for non-pointer |auto| assigned from raw_ptr-elligible field.
  // No rewrite expected.
  auto non_pointer_auto_var = my_struct.ptr;

  // Test for non-auto pointer.
  // No rewrite expected.
  SomeClass* non_auto_ptr_var = my_struct.ptr;

  // raw_ref tests
  {
    SomeClass some_class;
    MyStruct s(some_class, some_class, some_class);

    // After the rewrite |my_struct.ref_1| is no longer a native reference,
    // so |auto&| won't do what's expected.  We fix this up, by injecting *
    // operator. Expected rewrite: auto& ptr_var = *(my_struct.ref1_);
    auto& ref_var = *my_struct.ref1_;

    // Tests for other kinds of initialization.
    // Expected rewrite: operator* should be added in both cases below.
    auto& init_test1(*my_struct.ref1_);
    auto& init_test2{*my_struct.ref2_};

    // Test for handling of the |const| qualifier.
    // Expected rewrite: const auto& ptr_var = *my_struct.const_ref_;
    const auto& const_ref_var = *my_struct.const_ref_;

    // More complicated initialization expression, but the |ref1_| struct
    // member dereference is still the top/last expression here.
    // Expected rewrite: *GetMyStruct()->ref1_
    auto& complicated_var = *GetMyStruct()->ref1_;

    // The test below covers:
    // 1. Two variables with single |auto|,
    // 2. Tricky placement of |&| (next to the variable name).
    // Expected rewrite: *...ref_... (twice in the 2nd example).
    auto &ref_var1 = *my_struct.ref1_, &ref_var2 = *GetSomeClass();
    auto &ref_var3 = *my_struct.ref1_, &ref_var4 = *my_struct.ref1_;
    auto &ref_var5 = *GetSomeClass(), &ref_var6 = *my_struct.ref1_;

    // Expected rewrite: auto* not_affected_field_var =
    // ConvertSomeClassToSomeClass(&*my_struct.ref1_);
    auto* not_affected_field_var =
        ConvertSomeClassToSomeClass(&*my_struct.ref1_);

    // Test for non-pointer |auto| assigned from raw_ref-eligible field.
    // expected rewrite: auto non_pointer_auto_var = *my_struct.ref1_;
    auto non_pointer_auto_var = *my_struct.ref1_;

    // Test for non-auto pointer.
    // No rewrite expected.
    SomeClass& non_auto_ref_var = my_struct.ref1_;
  }
}

}  // namespace auto_tests

namespace printf_tests {

int ConvertSomeClassToInt(SomeClass* some_class) {
  return 123;
}

void MyPrintf(const char* fmt, ...) {}

void foo() {
  SomeClass some_class;
  MyStruct s(some_class, some_class, some_class);

  // Expected rewrite: MyPrintf("%p", s.ptr.get());
  MyPrintf("%p", s.ptr.get());

  // Test - all arguments are rewritten.
  // Expected rewrite: MyPrintf("%p, %p", s.ptr.get(), s.ptr2.get());
  MyPrintf("%p, %p", s.ptr.get(), s.ptr2.get());

  // Test - only |s.ptr|-style arguments are rewritten.
  // Expected rewrite: MyPrintf("%d, %p", 123, s.ptr.get());
  MyPrintf("%d, %p", 123, s.ptr.get());

  // Test - |s.ptr| is deeply nested.
  // No rewrite expected.
  MyPrintf("%d", ConvertSomeClassToInt(s.ptr));
}

}  // namespace printf_tests

namespace cast_tests {

void foo() {
  SomeClass s;
  MyStruct my_struct(s, s, s);

  // To get |const_cast<...>(...)| to compile after the rewrite we
  // need to rewrite the casted expression.
  // Expected rewrite: const_cast<SomeClass*>(my_struct.const_ptr.get());
  SomeClass* v = const_cast<SomeClass*>(my_struct.const_ptr.get());
  // Expected rewrite: const_cast<const SomeClass*>(my_struct.ptr.get());
  const SomeClass* v2 = const_cast<const SomeClass*>(my_struct.ptr.get());

  // To get |reinterpret_cast<uintptr_t>(...)| to compile after the rewrite we
  // need to rewrite the casted expression.
  // Expected rewrite: reinterpret_cast<uintptr_t>(my_struct.ptr.get());
  uintptr_t u = reinterpret_cast<uintptr_t>(my_struct.ptr.get());

  // There is no need to append |.get()| inside static_cast - unlike the
  // const_cast and reinterpret_cast examples above, static_cast will compile
  // just fine.
  DerivedClass* d = static_cast<DerivedClass*>(my_struct.ptr);
  void* void_var = static_cast<void*>(my_struct.ptr);
}

void foo2() {
  SomeClass s;
  MyStruct my_struct(s, s, s);

  // To get |const_cast<...>(...)| to compile after the rewrite we
  // need to rewrite the casted expression.
  // Expected rewrite: const_cast<SomeClass&>(*my_struct.const_ref_);
  SomeClass& v = const_cast<SomeClass&>(*my_struct.const_ref_);
  // Expected rewrite: const_cast<const SomeClass&>(*my_struct.ptr);
  const SomeClass& v2 = const_cast<const SomeClass&>(*my_struct.ref1_);

  // There is no need to append |.get()| inside static_cast - unlike the
  // const_cast and reinterpret_cast examples above, static_cast will compile
  // just fine.
  DerivedClass& d = static_cast<DerivedClass&>(*my_struct.ref1_);
}

}  // namespace cast_tests

namespace ternary_operator_tests {

void foo(int x) {
  SomeClass s;
  MyStruct my_struct(s, s, s);
  SomeClass* other_ptr = nullptr;

  // To avoid the following error type:
  //     conditional expression is ambiguous; 'const raw_ptr<SomeClass>'
  //     can be converted to 'SomeClass *' and vice versa
  // we need to append |.get()| to |my_struct.ptr| below.
  //
  // Expected rewrite: ... my_struct.ptr.get() ...
  SomeClass* v = (x > 123) ? my_struct.ptr.get() : other_ptr;

  // Rewrite in the other position.
  // Expected rewrite: ... my_struct.ptr.get() ...
  SomeClass* v2 = (x > 456) ? other_ptr : my_struct.ptr.get();

  // No rewrite is needed for the first, conditional argument.
  // No rewrite expected.
  int v3 = my_struct.ptr ? 123 : 456;

  // Test for 1st and 2nd arg.  Only 2nd arg should be rewritten.
  SomeClass* v4 = my_struct.ptr ? my_struct.ptr.get() : other_ptr;
}

void foo2(int x) {
  SomeClass s;
  MyStruct my_struct(s, s, s);
  SomeClass* other_ptr = nullptr;

  // Expected rewrite: SomeClass* v = (x > 123) ? &*my_struct.ref1_ :
  // other_ptr;
  SomeClass* v = (x > 123) ? &*my_struct.ref1_ : other_ptr;

  // Rewrite in the other position.
  // Expected rewrite: SomeClass* v2 = (x > 456) ? other_ptr :
  // &*my_struct.ref1_;
  SomeClass* v2 = (x > 456) ? other_ptr : &*my_struct.ref1_;
}

}  // namespace ternary_operator_tests

namespace string_comparison_operator_tests {

void foo(int x) {
  SomeClass s;
  MyStruct my_struct(s, s, s);
  std::string other_str = "other";

  // No rewrite expected. (for now)
  // TODO(crbug.com/40245402) |const char| pointer fields are not supported yet.
  bool v1 = my_struct.const_char_ptr == other_str;
  bool v2 = other_str == my_struct.const_char_ptr;
  bool v3 = my_struct.const_char_ptr > other_str;
  bool v4 = other_str > my_struct.const_char_ptr;
  bool v5 = my_struct.const_char_ptr >= other_str;
  bool v6 = other_str >= my_struct.const_char_ptr;
  bool v7 = my_struct.const_char_ptr < other_str;
  bool v8 = other_str < my_struct.const_char_ptr;
  bool v9 = my_struct.const_char_ptr <= other_str;
  bool v10 = other_str <= my_struct.const_char_ptr;
  std::string v11 = my_struct.const_char_ptr + other_str;
  std::string v12 = other_str + my_struct.const_char_ptr;
}

}  // namespace string_comparison_operator_tests

namespace templated_functions {

template <typename T>
void AffectedFunction(T* t) {}

template <typename T>
void TemplatedFunction_NonTemplatedParam(SomeClass* arg, T t) {}

template <typename T>
class MyTemplate {
 public:
  template <typename U>
  MyTemplate(U* u) {}

  void AffectedMethod(T* t) {}
};

// We also want to append |.get()| for |T| parameters (i.e. not just for |T*|
// parameters).
//
// One motivating example is the following pattern from
// //components/variations/service/ui_string_overrider.cc where the type of the
// 2 arguments needs to be kept consistent:
//     const uint32_t* end = ptr_field_ + num_resources_;
//     const uint32_t* element = std::lower_bound(ptr_field_, end, hash);
template <typename T>
void AffectedNonPointerFunction(T t) {}

// base::Unretained has a template specialization that accepts `const
// raw_ptr<T>&` as an argument (since https://crrev.com/c/3283196).  Therefore
// we expect that `.get()` is *not* used when calling base::Unretained.
//
// Originally, ActivityLogDatabasePolicy::ScheduleAndForget was used as a
// motivating example - passes a raw_ptr to base::Unretained.
template <typename T>
void Unretained(T* t) {}

// AffectedFunctionWithDeepT mimics ConvertPPResourceArrayToObjects from
// //ppapi/cpp/array_output.h
template <typename T>
void AffectedFunctionWithDeepT(MyTemplate<T>* blah) {}

// StructWithPointerToTemplate is used to test AffectedFunctionWithDeepT.
// StructWithPointerToTemplate mimics ResourceArrayOutputAdapter<T>
// (and its |output_| field that will be converted to a raw_ptr)
// from //ppapi/cpp/array_output.h
template <typename T>
struct StructWithPointerToTemplate {
  raw_ptr<MyTemplate<T>> ptr_to_template;
};

void foo() {
  SomeClass s;
  MyStruct my_struct(s, s, s);

  // Expected rewrite - appending: .get()
  AffectedFunction(my_struct.ptr.get());

  // Expected rewrite - appending: .get()
  MyTemplate<SomeClass> mt(my_struct.ptr.get());
  // Expected rewrite - appending: .get()
  mt.AffectedMethod(my_struct.ptr.get());

  // No rewrite expected.
  TemplatedFunction_NonTemplatedParam(my_struct.ptr, 123);

  // Expected rewrite - appending: .get()
  AffectedNonPointerFunction(my_struct.ptr.get());

  // Expected rewrite - appending: .get()
  StructWithPointerToTemplate<SomeClass> swptt;
  AffectedFunctionWithDeepT(swptt.ptr_to_template.get());

  // No rewrite expected - T& parameter.
  std::swap(my_struct.ptr, my_struct.ptr2);
  std::tie(my_struct.ptr, my_struct.ptr2) = std::make_pair(nullptr, nullptr);

  // No rewrite expected - functions named "Unretained" are excluded (they have
  // been manually modified to also provide a template specialization that
  // accepts `const raw_ptr<T>&` as an argument).
  Unretained(my_struct.ptr);
}

}  // namespace templated_functions

namespace templated_functions_raw_ref_tests {

template <typename T>
void AffectedFunction(T& t) {}

template <typename T>
void TemplatedFunction_NonTemplatedParam(SomeClass& arg, T t) {}

template <typename T>
class MyTemplate {
 public:
  template <typename U>
  MyTemplate(U& u) {}

  void AffectedMethod(T& t) {}
};

template <typename T>
void AffectedNonPointerFunction(T t) {}

// AffectedFunctionWithDeepT mimics ConvertPPResourceArrayToObjects from
// //ppapi/cpp/array_output.h
template <typename T>
void AffectedFunctionWithDeepT(MyTemplate<T>& blah) {}

// StructWithPointerToTemplate is used to test AffectedFunctionWithDeepT.
// StructWithPointerToTemplate mimics ResourceArrayOutputAdapter<T>
// (and its |output_| field that will be converted to a raw_ref)
// from //ppapi/cpp/array_output.h
template <typename T>
struct StructWithPointerToTemplate {
  StructWithPointerToTemplate(MyTemplate<T>& ref) : ref_to_template(ref) {}
  const raw_ref<MyTemplate<T>> ref_to_template;
};

void foo() {
  SomeClass s;
  MyStruct my_struct(s, s, s);

  // Expected rewrite: AffectedFunction(*my_struct.ref1_);
  AffectedFunction(*my_struct.ref1_);

  // Expected rewrite: MyTemplate<SomeClass> mt(*my_struct.ref1_);
  MyTemplate<SomeClass> mt(*my_struct.ref1_);
  // Expected rewrite: mt.AffectedMethod(*my_struct.ref1_);
  mt.AffectedMethod(*my_struct.ref1_);

  // Expected rewrite: TemplatedFunction_NonTemplatedParam(*my_struct.ref1_,
  // 123)
  TemplatedFunction_NonTemplatedParam(*my_struct.ref1_, 123);

  // Expected rewrite: AffectedNonPointerFunction(*my_struct.ref1_);
  AffectedNonPointerFunction(*my_struct.ref1_);

  MyTemplate<SomeClass> my_template(s);
  StructWithPointerToTemplate<SomeClass> swptt(my_template);
  // Expected rewrite: AffectedFunctionWithDeepT(*swptt.ref_to_template);
  AffectedFunctionWithDeepT(*swptt.ref_to_template);

  // Expected rewrite: std::swap(*my_struct.ref1_, *my_struct.ref2_)
  std::swap(*my_struct.ref1_, *my_struct.ref2_);
  std::tie(*my_struct.ref1_, *my_struct.ref2_) = std::make_pair(s, s);
}

}  // namespace templated_functions_raw_ref_tests

namespace implicit_constructors {

template <typename CharT>
class BasicStringPiece;
typedef BasicStringPiece<char> StringPiece;
template <typename CharT>
class BasicStringPiece {
 public:
  constexpr BasicStringPiece(const char* str) {}
};
// Test case:
void FunctionTakingBasicStringPiece(StringPiece arg) {}
void FunctionTakingBasicStringPieceRef(const StringPiece& arg) {}

class ClassWithImplicitConstructor {
 public:
  ClassWithImplicitConstructor(SomeClass* blah) {}
};
void FunctionTakingArgWithImplicitConstructor(
    ClassWithImplicitConstructor arg) {}

void foo() {
  SomeClass s;
  MyStruct my_struct(s, s, s);

  // No rewrite expected. (for now)
  // TODO(crbug.com/40245402) |const char| pointer fields are not supported yet.
  FunctionTakingBasicStringPiece(my_struct.const_char_ptr);
  FunctionTakingBasicStringPieceRef(my_struct.const_char_ptr);

  // No rewrite expected.
  FunctionTakingBasicStringPiece(StringPiece(my_struct.const_char_ptr));
  FunctionTakingBasicStringPieceRef(StringPiece(my_struct.const_char_ptr));

  // Expected rewrite - appending: .get().  This is the same scenario as with
  // StringPiece above (except that no templates are present here).
  FunctionTakingArgWithImplicitConstructor(my_struct.ptr.get());
}

}  // namespace implicit_constructors

namespace implicit_constructors_raw_ref_tests {

template <typename CharT>
class BasicStringPiece;
typedef BasicStringPiece<char> StringPiece;
template <typename CharT>
class BasicStringPiece {
 public:
  constexpr BasicStringPiece(const char* str) {}
};
// Test case:
void FunctionTakingBasicStringPiece(StringPiece arg) {}
void FunctionTakingBasicStringPieceRef(const StringPiece& arg) {}

class ClassWithImplicitConstructor {
 public:
  ClassWithImplicitConstructor(SomeClass& blah) {}
};
void FunctionTakingArgWithImplicitConstructor(
    ClassWithImplicitConstructor arg) {}

void foo() {
  SomeClass s;
  MyStruct my_struct(s, s, s);
  // Expected rewrite:
  // FunctionTakingArgWithImplicitConstructor(*my_struct.ref1_);
  FunctionTakingArgWithImplicitConstructor(*my_struct.ref1_);
}

}  // namespace implicit_constructors_raw_ref_tests

namespace affected_implicit_template_specialization {

template <typename T, typename T2>
struct MyTemplate {
  raw_ptr<T> t_ptr;
  raw_ptr<T2> t2_ptr;

  struct NestedStruct {
    raw_ptr<SomeClass> nested_ptr_field;
    raw_ptr<T> nested_t_ptr_field;
  };
  NestedStruct nested_struct_field;
};

template <typename T3>
struct MyTemplate<SomeClass, T3> {
  raw_ptr<SomeClass> some_ptr;
  raw_ptr<T3> t3_ptr;
};

// The example that forces explicit |isAnonymousStructOrUnion| checks in
// the implementation of GetExplicitDecl.  The example is based on
// buildtools/third_party/libc++/trunk/include/string.
template <typename T>
struct MyStringTemplate {
  struct NestedStruct {
    union {
      long l;
      short s;
      raw_ptr<T> t_ptr;
      raw_ptr<int> i_ptr;
    };  // Unnamed / anonymous union *field*.

    struct {
      long l2;
      short s2;
      raw_ptr<T> t_ptr2;
      raw_ptr<int> i_ptr2;
    };  // Unnamed / anonymous struct *field*.
  };
  NestedStruct s;
};

void MyPrintf(const char* fmt, ...) {}

void foo() {
  // |s.t_ptr| comes from implicit template specialization (which needs to be
  // skipped for rewriting, but should be included for appending |.get()|).
  //
  // Expected rewrite: MyPrintf("%p", s.t_ptr.get());
  MyTemplate<int, int> s;
  MyPrintf("%p", s.t_ptr.get());

  // |s.some_ptr| and |s.t2_ptr| come from implicit template specialization or a
  // partial template specialization.
  //
  // Expected rewrite: MyPrintf("%p", s.some_ptr.get(), s.t3_ptr.get());
  MyTemplate<SomeClass, int> s2;
  MyPrintf("%p %p", s2.some_ptr.get(), s2.t3_ptr.get());

  // Nested structs require extra care when trying to look up the non-implicit
  // field definition.  Expected rewrite: adding |.get()| suffix.
  MyPrintf("%p", s.nested_struct_field.nested_ptr_field.get());
  MyPrintf("%p", s.nested_struct_field.nested_t_ptr_field.get());

  // Lines below are added mainly to Force implicit specialization of
  // MyStringTemplate (to force explicit |isAnonymousStructOrUnion| checks in
  // the rewriter).  Still, the expected rewrite is: appending |.get()| to the
  // printf arg.
  MyStringTemplate<void> mst;
  MyPrintf("%p %p", mst.s.t_ptr.get(), mst.s.t_ptr2.get());
}

}  // namespace affected_implicit_template_specialization

namespace affected_implicit_template_specialization_raw_ref_tests {

template <typename T, typename T2>
struct MyTemplate {
  const raw_ref<T> t_ref;
  const raw_ref<T2> t2_ref;

  struct NestedStruct {
    const raw_ref<SomeClass> nested_ref_field;
    const raw_ref<T> nested_t_ref_field;
  };
  NestedStruct nested_struct_field;
};

template <typename T3>
struct MyTemplate<SomeClass, T3> {
  const raw_ref<SomeClass> some_ptr;
  const raw_ref<T3> t3_ptr;
};
}  // namespace affected_implicit_template_specialization_raw_ref_tests

// The test scenario below is based on an example encountered in
// //cc/layers/picture_layer_impl_unittest.cc:
//   auto* shared_quad_state = render_pass->quad_list.begin()->shared_quad_state
// In this example, the AST looks like this:
//  `-DeclStmt
//    `-VarDecl shared_quad_state 'const SharedQuadState *' cinit
//      `-ExprWithCleanups 'const SharedQuadState *'
//        `-ImplicitCastExpr 'const SharedQuadState *' <LValueToRValue>
//          `-MemberExpr 'const SharedQuadState *const' lvalue ->shared...state
//            `-.....
// The rewriter needs to ignore the implicit ExprWithCleanups and
// ImplicitCastExpr nodes in order to find the MemberExpr.  If this is
// implemented incorrectly, then the rewriter won't append |.get()| to fix the
// |auto*| initialization.
namespace more_implicit_ast_nodes_trouble {

template <class BaseElementType>
struct ListContainer {
  struct ConstIterator {
    const BaseElementType* operator->() const { return nullptr; }
  };

  ConstIterator begin() const { return ConstIterator(); }
};

class SharedQuadState;

struct DrawQuad {
  raw_ptr<const SharedQuadState> shared_quad_state;
};

struct RenderPass {
  using QuadList = ListContainer<DrawQuad>;
  QuadList quad_list;
};

void foo() {
  RenderPass* render_pass = nullptr;
  auto* shared_quad_state =
      render_pass->quad_list.begin()->shared_quad_state.get();
}

}  // namespace more_implicit_ast_nodes_trouble
