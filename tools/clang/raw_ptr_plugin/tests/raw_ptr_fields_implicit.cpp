// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fields that aren't spelled in the source code shouldn't get errors: fields
// of implicit template instantiations (the error is reported for the
// template), of classes nested in them, and of lambdas.

class SomeClass;

template <typename T>
struct MyTemplate {
  // Error expected, but only once and not again for every instantiation.
  SomeClass* ptr_field;
  // Error expected, but only once and not again for every instantiation.
  SomeClass& ref_field;

  struct Nested {
    // Error expected, but only once and not again for every instantiation.
    SomeClass* nested_ptr_field;
    // Error expected, but only once and not again for every instantiation.
    SomeClass& nested_ref_field;

    struct NestedMore {
      // Error expected, but only once and not again for every instantiation.
      T* nested_more_ptr_field;
    };
  };
};

// An explicit specialization is spelled in the source code.
template <>
struct MyTemplate<bool> {
  // Error expected.
  SomeClass* ptr_field;
  // Error expected.
  SomeClass& ref_field;

  struct Nested {
    // Error expected.
    SomeClass* nested_ptr_field;
  };
};

template <typename T>
void FunctionTemplate(T* ptr, SomeClass& ref) {
  struct Local {
    // Error expected, but only once and not again for every instantiation.
    T* local_ptr_field;
    // Error expected, but only once and not again for every instantiation.
    SomeClass& local_ref_field;
  };
  Local local = {ptr, ref};
  (void)local;

  // No error expected for the captures.
  auto lambda = [ptr, &ref]() { return ptr != nullptr && &ref != nullptr; };
  lambda();
}

void Function(SomeClass* ptr, SomeClass& ref) {
  struct Local {
    // Error expected.
    SomeClass* local_ptr_field;
  };

  // No error expected for the captures.
  auto lambda = [ptr, &ref]() { return ptr != nullptr && &ref != nullptr; };
  lambda();
}

void Instantiate(SomeClass& some_class, int* int_ptr, char* char_ptr) {
  MyTemplate<int>::Nested::NestedMore a = {int_ptr};
  MyTemplate<char>::Nested::NestedMore b = {char_ptr};
  MyTemplate<int>::Nested c = {&some_class, some_class};
  MyTemplate<int> d = {&some_class, some_class};
  MyTemplate<char> e = {&some_class, some_class};
  MyTemplate<bool> f = {&some_class, some_class};
  MyTemplate<bool>::Nested g = {&some_class};
  FunctionTemplate(int_ptr, some_class);
  FunctionTemplate(char_ptr, some_class);
}
