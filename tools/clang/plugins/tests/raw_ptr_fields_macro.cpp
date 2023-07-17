// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define USR_INT int
#define USR_INTP int*
#define USR_CONST const
#define USR_ATTR [[fake_attribute]]
#define USR_INTP_FIELD() int* macro_ptr
#define USR_TYPE_WITH_SUFFIX(TYP) TYP##Suffix
#define USR_SYMBOL(SYM) SYM
#define USR_SYMBOL_WITH_SUFFIX(SYM) SYM##_suffix
class UsrTypSuffix;

#include <raw_ptr_system_test.h>

// These `SYS_***` macro should be defined
// in `//tools/clang/plugins/tests/system/raw_ptr_system_test.h`.
struct UsrStructWithSysMacro {
  // Error.
  int* ptr0;
  // Error: typeLoc is macro but identifier is written here.
  SYS_INT* ptr1;
  // Error: typeLoc is macro but identifier is written here.
  SYS_INTP ptr2;
  // Error: typeLoc is macro but identifier is written here.
  int* SYS_CONST ptr3;
  // Error: attribute is macro but identifier is written here.
  int* SYS_ATTR ptr4;
  // OK: code owner has no control over fieldDecl.
  SYS_INTP_FIELD();
  // Error: typeLoc is macro but identifier is written here.
  SYS_TYPE_WITH_SUFFIX(UsrTyp) * ptr5;
  // Error: identifier is defined with macro but it is written here.
  int* SYS_SYMBOL(ptr6);
  // OK: code owner has no control over fieldDecl.
  int* SYS_SYMBOL_WITH_SUFFIX(ptr7);
};

// These `CMD_***` macro should be defined in command line arguments.
struct UsrStructWithCmdMacro {
  // Error.
  int* ptr0;
  // Error: typeLoc is macro but identifier is written here.
  CMD_INT* ptr1;
  // Error: typeLoc is macro but identifier is written here.
  CMD_INTP ptr2;
  // Error: typeLoc is macro but identifier is written here.
  int* CMD_CONST ptr3;
  // Error: attribute is macro but identifier is written here.
  int* CMD_ATTR ptr4;
  // OK: code owner has no control over fieldDecl.
  CMD_INTP_FIELD();
  // Error: typeLoc is macro but identifier is written here.
  CMD_TYPE_WITH_SUFFIX(UsrTyp) * ptr5;
  // Error: identifier is defined with macro but it is written here.
  int* CMD_SYMBOL(ptr6);
  // OK: code owner has no control over fieldDecl.
  int* CMD_SYMBOL_WITH_SUFFIX(ptr7);
};

struct UsrStructWithUsrMacro {
  // Error.
  int* ptr0;
  // Error: typeLoc is macro but identifier is written here.
  USR_INT* ptr1;
  // Error: typeLoc is macro but identifier is written here.
  USR_INTP ptr2;
  // Error: typeLoc is macro but identifier is written here.
  int* USR_CONST ptr3;
  // Error: attribute is macro but identifier is written here.
  int* USR_ATTR ptr4;
  // Error: user has control over the macro.
  USR_INTP_FIELD();
  // Error: user has control over the macro.
  USR_TYPE_WITH_SUFFIX(UsrTyp) * ptr5;
  // Error: identifier is defined with macro but it is written here.
  int* USR_SYMBOL(ptr6);
  // OK: the source location for this field declaration will be "<scratch
  // space>" and the real file path cannot be detected.
  int* USR_SYMBOL_WITH_SUFFIX(ptr7);
};
