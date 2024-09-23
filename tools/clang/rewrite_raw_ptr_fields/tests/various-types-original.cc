// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <string>
#include <vector>

namespace my_namespace {

class SomeClass {
 public:
  void Method(char) {}
  int data_member;
};

template <typename T>
struct SomeTemplate {
  T t;
};

struct MyStruct {
  // Expected rewrite: raw_ptr<raw_ptr<SomeClass>> double_ptr;
  // TODO(lukasza): Handle recursion/nesting.
  SomeClass** double_ptr;

  // Expected rewrite: raw_ptr<void> void_ptr;
  void* void_ptr;

  // |bool*| used to be rewritten as |raw_ptr<_Bool>| which doesn't compile:
  // use of undeclared identifier '_Bool'.
  //
  // Expected rewrite: raw_ptr<bool> bool_ptr;
  bool* bool_ptr;
  // Expected rewrite: raw_ptr<const bool> bool_ptr;
  const bool* const_bool_ptr;

  // Pointers to templates.
  // Expected rewrite: raw_ptr<std::string> string_ptr;
  std::string* string_ptr;
  // Expected rewrite: raw_ptr<std::vector<char>> vector_ptr;
  std::vector<char>* vector_ptr;
  // Expected rewrite: raw_ptr<SomeTemplate<char>> template_ptr;
  SomeTemplate<char>* template_ptr;

  // Some types may be spelled in various, alternative ways.  If possible, the
  // rewriter should preserve the original spelling.
  //
  // Spelling of integer types.
  //
  // Expected rewrite: raw_ptr<int> ...
  int* int_spelling1;
  // Expected rewrite: raw_ptr<signed int> ...
  // TODO(lukasza): Fix?  Today this is rewritten into: raw_ptr<int> ...
  signed int* int_spelling2;
  // Expected rewrite: raw_ptr<long int> ...
  // TODO(lukasza): Fix?  Today this is rewritten into: raw_ptr<long> ...
  long int* int_spelling3;
  // Expected rewrite: raw_ptr<unsigned> ...
  // TODO(lukasza): Fix?  Today this is rewritten into: raw_ptr<unsigned int>
  unsigned* int_spelling4;
  // Expected rewrite: raw_ptr<int32_t> ...
  int32_t* int_spelling5;
  // Expected rewrite: raw_ptr<int64_t> ...
  int64_t* int_spelling6;
  // Expected rewrite: raw_ptr<int_fast32_t> ...
  int_fast32_t* int_spelling7;
  //
  // Spelling of structs and classes.
  //
  // Expected rewrite: raw_ptr<SomeClass> ...
  SomeClass* class_spelling1;
  // Expected rewrite: raw_ptr<class SomeClass> ...
  class SomeClass* class_spelling2;
  // Expected rewrite: raw_ptr<my_namespace::SomeClass> ...
  my_namespace::SomeClass* class_spelling3;

  // No rewrite of function pointers expected, because they won't ever be either
  // A) allocated by PartitionAlloc or B) derived from RawPtrSupport.  In
  // theory |member_data_ptr| below can be A or B, but it can't be expressed as
  // non-pointer T used as a template argument of raw_ptr<>.
  int (*func_ptr)();
  void (SomeClass::*member_func_ptr)(char);  // ~ pointer to SomeClass::Method
  int SomeClass::*member_data_ptr;  // ~ pointer to SomeClass::data_member
  typedef void (*func_ptr_typedef)(char);
  func_ptr_typedef func_ptr_typedef_field;

  // Typedef-ed or type-aliased pointees should participate in the rewriting. No
  // desugaring of the aliases is expected.
  typedef SomeClass SomeClassTypedef;
  using SomeClassAlias = SomeClass;
  typedef void (*func_ptr_typedef2)(char);
  // Expected rewrite: raw_ptr<SomeClassTypedef> ...
  SomeClassTypedef* typedef_ptr;
  // Expected rewrite: raw_ptr<SomeClassAlias> ...
  SomeClassAlias* alias_ptr;
  // Expected rewrite: raw_ptr<func_ptr_typedef2> ...
  func_ptr_typedef2* ptr_to_function_ptr;

  // Typedefs and type alias definitions should not be rewritten.
  //
  // No rewrite expected (for now - in V1 we only rewrite field decls).
  typedef SomeClass* SomeClassPtrTypedef;
  // No rewrite expected (for now - in V1 we only rewrite field decls).
  using SomeClassPtrAlias = SomeClass*;

  // Char pointer fields should be rewritten, unless they are on the
  // --field-filter-file blocklist.
  //
  // Expected rewrite: raw_ptr<char>, etc.
  char* char_ptr;
  char16_t* char16_ptr;
  wchar_t* wide_char_ptr;
  uint8_t* unsigned_char_ptr;
  int8_t* signed_char_ptr;

  // TODO(crbug.com/40245402) |const char| pointer fields are not supported yet.
  //
  // No rewrite expected (for now).
  const char* const_char_ptr;
  const char16_t* const_char16_ptr;
  const wchar_t* const_wide_char_ptr;

  // Expected rewrite: raw_ptr<const uint8_t> const_unsigned_char_ptr;
  const uint8_t* const_unsigned_char_ptr;
  // Expected rewrite: raw_ptr<const int8_t> const_signed_char_ptr;
  const int8_t* const_signed_char_ptr;

  // Expected rewrite: raw_ptr<const unsigned char> const_unsigned_char_ptr2;
  const unsigned char* const_unsigned_char_ptr2;
  // Expected rewrite: raw_ptr<const signed char> const_signed_char_ptr2;
  const signed char* const_signed_char_ptr2;

  // Expected rewrite: raw_ptr<const char*> double_char_ptr1;
  const char** double_char_ptr1;
  // Expected rewrite: raw_ptr<const char16_t*> double_char_ptr2;
  const char16_t** double_char_ptr2;
  // Expected rewrite: raw_ptr<const wchar_t*> double_char_ptr3;
  const wchar_t** double_char_ptr3;
  // Expected rewrite: raw_ptr<const unsigned char*> double_char_ptr4;
  const unsigned char** double_char_ptr4;
  // Expected rewrite: raw_ptr<const signed char*> double_char_ptr5;
  const signed char** double_char_ptr5;

  // |array_of_ptrs| is an array 123 of pointer to SomeClass.
  // No rewrite expected (this is not a pointer - this is an array).
  SomeClass* ptr_array[123];

  // |ptr_to_array| is a pointer to array 123 of const SomeClass.
  //
  // This test is based on EqualsFramesMatcher from
  // //net/websockets/websocket_channel_test.cc
  //
  // No rewrite expected (this *is* a pointer, but generating a correct
  // replacement is tricky, because the |replacement_range| needs to cover
  // "[123]" that comes *after* the field name).
  const SomeClass (*ptr_to_array)[123];
};

struct MyStruct2 {
  // Expected rewrite: const raw_ref<bool> bool_ref;
  bool& bool_ref;
  // Expected rewrite: const raw_ref<const bool> bool_ref;
  const bool& const_bool_ref;

  // Expected rewrite: const raw_ref<std::string> string_ref;
  std::string& string_ref;
  // Expected rewrite: const raw_ref<std::vector<char>> vector_ref;
  std::vector<char>& vector_ref;
  // Expected rewrite: const raw_ref<SomeTemplate<char>> template_ref;
  SomeTemplate<char>& template_ref;

  // Some types may be spelled in various, alternative ways.  If possible, the
  // rewriter should preserve the original spelling.
  //
  // Spelling of integer types.
  //
  // Expected rewrite: const raw_ref<int> ...
  int& int_spelling1;
  // Expected rewrite: const raw_ref<signed int> ...
  // Today this is rewritten into: const raw_ref<int> ...
  signed int& int_spelling2;
  // Expected rewrite: const raw_ref<long int> ...
  // Today this is rewritten into: const raw_ref<long> ...
  long int& int_spelling3;
  // Expected rewrite: const raw_ref<unsigned> ...
  // Today this is rewritten into: const raw_ref<unsigned int>
  unsigned& int_spelling4;
  // Expected rewrite: const raw_ref<int32_t> ...
  int32_t& int_spelling5;
  // Expected rewrite: const raw_ref<int64_t> ...
  int64_t& int_spelling6;
  // Expected rewrite: const raw_ref<int_fast32_t> ...
  int_fast32_t& int_spelling7;
  //
  // Spelling of structs and classes.
  //
  // Expected rewrite: const raw_ref<SomeClass> ...
  SomeClass& class_spelling1;
  // Expected rewrite: const raw_ref<class SomeClass> ...
  class SomeClass& class_spelling2;
  // Expected rewrite: const raw_ref<my_namespace::SomeClass> ...
  my_namespace::SomeClass& class_spelling3;

  // Typedef-ed or type-aliased pointees should participate in the rewriting. No
  // desugaring of the aliases is expected.
  typedef SomeClass SomeClassTypedef;
  using SomeClassAlias = SomeClass;
  typedef void (*func_ptr_typedef2)(char);
  // Expected rewrite: const raw_ref<SomeClassTypedef> ...
  SomeClassTypedef& typedef_ref;
  // Expected rewrite: const raw_ref<SomeClassAlias> ...
  SomeClassAlias& alias_ref;
  // Expected rewrite: const raw_ref<func_ptr_typedef2> ...
  func_ptr_typedef2& ref_to_function_ptr;

  // Typedefs and type alias definitions should not be rewritten.
  //
  // No rewrite expected (for now - in V1 we only rewrite field decls).
  typedef SomeClass& SomeClassRefTypedef;
  // No rewrite expected (for now - in V1 we only rewrite field decls).
  using SomeClassRefAlias = SomeClass&;

  // Char pointer fields should be rewritten, unless they are on the
  // --field-filter-file blocklist.
  //
  // Expected rewrite: const raw_ref<char>, etc.
  char& char_ref;
  const char& const_char_ref;
  wchar_t& wide_char_ref;
  const wchar_t& const_wide_char_ref;
};

extern "C" {
struct OtherForeignStruct;
struct ForeignStruct {
  // We should not rewrite foreign, extern "C" structs.
  OtherForeignStruct* ptr;
  OtherForeignStruct& ref;
};
}

}  // namespace my_namespace
