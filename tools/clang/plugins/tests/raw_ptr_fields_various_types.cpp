// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

namespace my_namespace {

class SomeClass {
 public:
  void Method(char) {}
  // No error expected.
  int data_member;
};

template <typename T>
struct SomeTemplate {
  // No error expected.
  T t;
};

struct MyStruct {
  // Error expected.
  SomeClass** double_ptr;

  // Error expected.
  void* void_ptr;

  // Error expected.
  bool* bool_ptr;
  const bool* const_bool_ptr;

  // Pointers to templates.
  // Error expected.
  std::string* string_ptr;
  std::vector<char>* vector_ptr;
  SomeTemplate<char>* template_ptr;

  //
  // Various integer types.
  //
  // Error expected.
  int* int_spelling1;
  signed int* int_spelling2;
  long int* int_spelling3;
  unsigned* int_spelling4;

  //
  // Various structs and classes.
  //
  // Error expected.
  SomeClass* class_spelling1;
  class SomeClass* class_spelling2;
  my_namespace::SomeClass* class_spelling3;

  // Function pointers.
  // No error expected. Because they won't ever be allocated by PartitionAlloc.
  int (*func_ptr)();
  void (SomeClass::*member_func_ptr)(char);  // ~ pointer to SomeClass::Method
  int SomeClass::*member_data_ptr;  // ~ pointer to SomeClass::data_member
  typedef void (*func_ptr_typedef)(char);
  func_ptr_typedef func_ptr_typedef_field;

  // Typedef-ed or type-aliased pointees.
  typedef SomeClass SomeClassTypedef;
  using SomeClassAlias = SomeClass;
  typedef void (*func_ptr_typedef2)(char);
  // Error expected.
  SomeClassTypedef* typedef_ptr;
  // Error expected.
  SomeClassAlias* alias_ptr;
  // Error expected.
  func_ptr_typedef2* ptr_to_function_ptr;

  // Char pointer fields.
  //
  // Error expected.
  char* char_ptr;
  // No error expected. crbug.com/1381955
  const char* const_char_ptr;
  // Error expected.
  wchar_t* wide_char_ptr;
  // No error expected. crbug.com/1381955
  const wchar_t* const_wide_char_ptr;

  // |array_of_ptrs| is an array 123 of pointer to SomeClass.
  // No error expected. (this is not a pointer - this is an array).
  SomeClass* ptr_array[123];

  // |ptr_to_array| is a pointer to array 123 of const SomeClass.
  //
  // This test is based on EqualsFramesMatcher from
  // //net/websockets/websocket_channel_test.cc
  //
  // No error expected. Because this rewrite was tricky and not supported by the
  // rewriter.
  // crbug.com/1381969
  const SomeClass (*ptr_to_array)[123];
};

}  // namespace my_namespace
