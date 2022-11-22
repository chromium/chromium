// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

namespace not_blink {

void function(int x) {}

class Class {
 public:
  void method() {}
  virtual void virtualMethod() {}
  template <typename T>
  void methodTemplate(T) {}
  template <typename T>
  static void staticMethodTemplate(T) {}
};

template <typename T>
void functionTemplate(T x) {}

template <typename T = Class>
void functionTemplate2() {
  T::staticMethodTemplate(123);
}

template <typename T = Class>
class TemplatedClass {
 public:
  void anotherMethod() { T::staticMethodTemplate(123); }
};

}  // not_blink

namespace blink {

bool FunctionNotMarkedConstexpr(int a) {
  return a == 4 || a == 10;
}

template <typename T>
bool TemplatedFunctionNotMarkedConstexpr(T t) {
  return !!t;
}

int g_global_number;

template <typename T, int number>
void F() {
  // These are const but hacker_case so we leave them alone.
  const int maybe_a_const = sizeof(T);
  const int is_a_const = number;
  // These are const expressions so they get a k prefix.
  const int kMaybeAConstToo = sizeof(T);
  const int kIsAConstToo = number;
  // These are built from calls to functions which produces inconsistent
  // results so they should not be considered const to be safe.
  const bool from_a_method = FunctionNotMarkedConstexpr(number);
  const bool from_a_templated_method =
      TemplatedFunctionNotMarkedConstexpr(number);
  // A complex statement of const things is const.
  const bool kComplexConst = number || (number + 1);
  // A complex statement with a non-const thing is not const.
  const bool complex_not_const = number || (g_global_number + 1);
  // A const built from other consts is a const.
  const bool kConstFromAConst = kComplexConst || number;
}

template <int number, typename... T>
void F() {
  // These are const but hacker_case so we leave them alone.
  const int maybe_a_const = sizeof...(T);
  const int is_a_const = number;
  // These are const expressions so they get a k prefix.
  const int kMaybeAConstToo = sizeof...(T);
  const int kIsAConstToo = number;
}

namespace test_member_in_template {

template <typename T>
class HasAMember {
 public:
  HasAMember() {}
  HasAMember(const T&) {}

  void UsesMember() { const int not_const = i_; }
  void AlsoUsesMember();

 private:
  int i_;
};

template <typename T>
void HasAMember<T>::AlsoUsesMember() {
  const int not_const = i_;
}

template <typename T>
static void BasedOnSubType(const HasAMember<T>& t) {
  const HasAMember<T> problematic_not_const(t);
}

void Run() {
  HasAMember<int>().UsesMember();

  BasedOnSubType<int>(HasAMember<int>());
  enum E { A };
  BasedOnSubType<E>(HasAMember<E>());
}
}

namespace test_template_arg_is_function {

void F(int x) {}

template <typename T, void g(T)>
void H(T x) {
  g(x);
}

void Test() {
  // f should be rewritten.
  H<int, F>(0);
  // Non-Blink should stay the same.
  H<int, not_blink::function>(1);

  // The int one makes the methods called from F() considered as constexpr, and
  // can be collapsed to not have template arguments before it reaches the AST.
  F<int, 10>();
  // The enum one makes them not constexpr, as it doesn't collapse away the
  // template stuff as much. This can lead to conflicting decisions about
  // the names inside F() vs the above instantiation.
  enum E { A };
  F<E, 11>();
}

}  // namespace test_template_arg_is_function

namespace test_template_arg_is_method {

class Class {
 public:
  void Method() {}
};

template <typename T, void (T::*g)()>
void H(T&& x) {
  (x.*g)();
}

void Test() {
  // method should be rewritten.
  H<Class, &Class::Method>(Class());
  // Non-Blink should stay the same.
  H<not_blink::Class, &not_blink::Class::method>(not_blink::Class());
}

}  // namespace test_template_arg_is_method

namespace test_template_arg_is_function_template {

namespace nested {
template <typename T>
void F(T) {}
}

template <typename T, void g(T)>
void H(T x) {
  g(x);
}

void Test() {
  // f should be rewritten.
  H<int, nested::F>(0);
  // Non-Blink should stay the same.
  H<int, not_blink::functionTemplate>(1);
}

}  // namespace test_template_arg_is_function_template

namespace test_template_arg_is_method_template_in_non_member_context {

struct Class {
  template <typename T>
  static void F(T) {}
};

template <typename T, void g(T)>
void H(T x) {
  g(x);
}

void Test() {
  // f should be rewritten.
  H<int, Class::F>(0);
  // Non-Blink should stay the same.
  H<int, not_blink::Class::staticMethodTemplate>(1);
}

}  // test_template_arg_is_method_template_in_non_member_context

namespace test_inherited_field {

template <typename T>
class BaseClass {
 public:
  unsigned long size_;
};

template <typename T>
class DerivedClass : protected BaseClass<T> {
 private:
  using Base = BaseClass<T>;
  // https://crbug.com/640016: Need to rewrite |m_size| into |size_|.
  using Base::size_;
  void Method() { size_ = 123; }
};

}  // namespace test_inherited_field

namespace test_template_arg_is_method_template_in_member_context {

struct Class {
  template <typename T>
  static void F(T) {}
};

struct Class2 {
  template <typename T>
  void F(T x) {
    // f should be rewritten.
    Class c;
    c.F(x);
    // Non-Blink should stay the same.
    not_blink::Class c2;
    c2.method(x);
  }
};

}  // namespace test_template_arg_is_method_template_in_member_context

namespace test_unnamed_arg {

template <typename T>
class Class {
 public:
  // Test for https://crbug.com/598141 - shouldn't rewrite
  //    ...int);
  // into
  //    ...intdata_size;
  void F(int);
};

template <typename T>
void Class<T>::F(int data_size){};

void Foo() {
  Class<char>().F(123);
};

}  // namespace test_unnamed_arg

namespace cxx_dependent_scope_member_expr_testing {

class PartitionAllocator {
 public:
  static void Method() {}
};

template <typename Allocator = PartitionAllocator>
class Vector {
 public:
  // https://crbug.com/582315: |Allocator::method| is a
  // CXXDependentScopeMemberExpr.
  void AnotherMethod() {
    if (std::is_class<Allocator>::value)  // Shouldn't rename |value|
      Allocator::Method();                // Should rename |method| -> |Method|.
  }
};

template <typename Allocator = PartitionAllocator>
void Test() {
  // https://crbug.com/582315: |Allocator::method| is a
  // DependentScopeDeclRefExpr.
  if (std::is_class<Allocator>::value)  // Shouldn't rename |value|.
    Allocator::Method();                // Should rename |method|.
}

class InterceptingCanvasBase : public ::not_blink::Class {
 public:
  virtual void VirtualMethodInBlink(){};
};

template <typename DerivedCanvas>
class InterceptingCanvas : public InterceptingCanvasBase {
 public:
  void virtualMethod() override {
    this->Class::virtualMethod();  // https://crbug.com/582315#c19
    this->InterceptingCanvasBase::VirtualMethodInBlink();
  }
};

template <typename T>
class ThreadSpecific {
 public:
  T* operator->();
  operator T*();
};

template <typename T>
inline ThreadSpecific<T>::operator T*() {
  return nullptr;
}

template <typename T>
inline T* ThreadSpecific<T>::operator->() {
  return operator T*();
}

class Class {
 public:
  virtual void VirtualMethodInBlink() {}
};

}  // namespace cxx_dependent_scope_member_expr_testing

namespace blocklisting_of_renaming_of_begin_method {

template <typename T>
class IntrusiveHeap {
 public:
  // https://crbug.com/672353: |begin| shouldn't be rewritten to |Begin|.
  const T* begin() const { return nullptr; }
};

}  // namespace blocklisting_of_renaming_of_begin_method

namespace specializations {

template <unsigned long sizeOfValue>
int ToV8SignedIntegerInternal(long value);

template <>
int ToV8SignedIntegerInternal<4>(long value) {
  return 123 + value;
}

template <>
int ToV8SignedIntegerInternal<8>(long value) {
  return 456 + value;
}

int ToV8(int value) {
  return ToV8SignedIntegerInternal<sizeof value>(value);
}

int ToV8(long value) {
  return ToV8SignedIntegerInternal<sizeof value>(value);
}

}  // namespace specializations

}  // namespace blink

namespace not_blink {

namespace cxx_dependent_scope_member_expr_testing {

class Base : public ::blink::cxx_dependent_scope_member_expr_testing::Class {
 public:
  virtual void virtualMethod() {}
};

template <typename T>
class Derived : public Base {
 public:
  void virtualMethod() override {
    this->Class::VirtualMethodInBlink();
    this->Base::virtualMethod();
  }
};

}  // namespace cxx_dependent_scope_member_expr_testing

namespace blink_methods_called_from_mojo_traits_are_not_rewritten {

template <typename V>
struct MapTraits;

template <typename V>
struct MapTraits<blink::test_unnamed_arg::Class<V>> {
  static void SetToEmpty(blink::test_unnamed_arg::Class<V>* output) {
    // Need to rewrite |f| to |F| below (because this method name
    // does get rewritten when processing blink::test_unnamed_arg::Class).
    // See also https://crbug.com/670434.
    output->F(123);
  }
};

}  // namespace blink_methods_called_from_mojo_traits_are_not_rewritten

}  // namespace not_blink
