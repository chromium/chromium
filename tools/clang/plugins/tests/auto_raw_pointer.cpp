// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Iterable {
 public:
  using const_iterator = int* const*;

  const_iterator begin() { return nullptr; }
  const_iterator end() { return nullptr; }
};

using AliasWithPtr = int*;
AliasWithPtr return_alias_with_ptr() {
  return nullptr;
}
AliasWithPtr* return_ptr_to_alias_with_ptr() {
  return nullptr;
}

typedef int* TypedefWithPtr;
TypedefWithPtr return_typedef_with_ptr() {
  return nullptr;
}

class Foo {
 public:
  void foo() {}
};

void f();

int main() {
  int integer;
  Foo foo;

  auto int_copy = integer;
  const auto const_int_copy = integer;
  const auto& const_int_ref = integer;

  auto raw_int_ptr = &integer;
  const auto const_raw_int_ptr = &integer;
  const auto& const_raw_int_ptr_ref = &integer;

  auto* raw_int_ptr_valid = &integer;
  const auto* const_raw_int_ptr_valid = &integer;

  auto raw_foo_ptr = &foo;
  const auto const_raw_foo_ptr = &foo;
  const auto& const_raw_foo_ptr_ref = &foo;

  auto* raw_foo_ptr_valid = &foo;
  const auto* const_raw_foo_ptr_valid = &foo;

  int* int_ptr;

  auto double_ptr_auto = &int_ptr;
  auto* double_ptr_auto_ptr = &int_ptr;
  auto** double_ptr_auto_double_ptr = &int_ptr;

  auto function_ptr = &f;
  auto method_ptr = &Foo::foo;

  int* const* const volatile** const* pointer_awesomeness;
  auto auto_awesome = pointer_awesomeness;

  auto& int_ptr_ref = int_ptr;
  const auto& const_int_ptr_ref = int_ptr;
  auto&& int_ptr_rref = static_cast<int*&&>(int_ptr);
  const auto&& const_int_ptr_rref = static_cast<int*&&>(int_ptr);

  static auto static_ptr = new int;

  Iterable iterable;
  for (auto& it : iterable) {
    (void)it;
  }

  // The alias itself contains a pointer, which is an implementation detail, so
  // `auto` is allowed.
  auto alias = return_alias_with_ptr();

  // A pointer to an alias (of a pointer) still requires `auto*`. This will
  // succeed.
  auto* good_ptr_to_alias = return_ptr_to_alias_with_ptr();
  // This will fail.
  auto bad_ptr_to_alias = return_ptr_to_alias_with_ptr();

  // `typedef` and `using` type aliases both work the same.
  auto tdef = return_typedef_with_ptr();

  // This is a valid usecase of deducing a type to be a raw pointer and should
  // not trigger a warning / error.
  auto lambda = [foo_ptr = &foo] { return *foo_ptr; };
}

template <class T>
struct WithDependentType {
  void func() {
    // The deduced type here is not known and `isNull()` will be true when
    // parsing the template (but not instantiations).
    auto x = T::foo();
  }
};

void use_template() {
  struct S {
    static int* foo() { return nullptr; };
  };
  // The dependent type is instantiated, but no errors are produced.
  WithDependentType<S>().func();
}

template <class T>
concept Concept = true;

void use_concept() {
  int x = 0;
  // No warning, because this is a constrained auto. Being a pointer or not is
  // an implementation detail of the matching type.
  Concept auto c = &x;
}

template <class T>
T auto_function_return_alias_with_ptr() {
  return return_alias_with_ptr();
}

void template_function() {
  // The auto is a type alias that was provided as a template parameter. Simple
  // repro of things like std::find().
  auto auto_alias = auto_function_return_alias_with_ptr<AliasWithPtr>();
}

template <class T>
using AliasOfT = T;

template <class T>
AliasOfT<T> auto_function_return_elaborated_alias_with_ptr() {
  return return_alias_with_ptr();
}

void alias_template_specialization_function() {
  // The auto is a type alias that was provided as a template parameter, but
  // returned as another alias (an "ElaboratedType" in this case).
  auto auto_alias =
      auto_function_return_elaborated_alias_with_ptr<AliasWithPtr>();
}

struct auto_type_level_three {
  template <class T>
  inline auto foo() const {
    return T();
  }
};

constexpr auto auto_type_level_two = auto_type_level_three{};

template <typename T>
constexpr auto auto_type_level_one() {
  return auto_type_level_two.foo<T>();
}

void nested_auto_function() {
  // This test function returns a type that has `AutoType` nested multiple
  // times. Something like:
  // AutoType 0x157941d90 'int *' sugar
  // `-AutoType 0x157941d90 'int *' sugar
  //   `-AutoType 0x157940b30 'int *' sugar
  //     `-SubstTemplateTypeParmType 0x157940a20 'int *' sugar
  //       `-PointerType 0x15790b7c0 'int *'
  auto x = auto_type_level_one<AliasWithPtr>();
}
