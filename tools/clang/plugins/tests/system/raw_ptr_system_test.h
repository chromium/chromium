// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_SYSTEM_RAW_PTR_SYSTEM_TEST_H_
#define TOOLS_CLANG_PLUGINS_TESTS_SYSTEM_RAW_PTR_SYSTEM_TEST_H_
#line 7 "/src/tools/clang/plugins/tests/system/raw_ptr_system_test.h"

#define SYS_INT int
#define SYS_INTP int*
#define SYS_CONST const
#define SYS_ATTR [[fake_attribute]]
#define SYS_INTP_FIELD() int* macro_ptr
#define SYS_TYPE_WITH_SUFFIX(TYP) TYP##Suffix
#define SYS_SYMBOL(SYM) SYM
#define SYS_SYMBOL_WITH_SUFFIX(SYM) SYM##_Suffix
class SysTypSuffix;

// OK: code owner has no control over system header.
struct SysStructWithSysMacro {
  int* ptr0;
  SYS_INT* ptr1;
  SYS_INTP ptr2;
  int* SYS_CONST ptr3;
  int* SYS_ATTR ptr4;
  SYS_INTP_FIELD();
  SYS_TYPE_WITH_SUFFIX(SysTyp) * ptr5;
  int* SYS_SYMBOL(ptr6);
  int* SYS_SYMBOL_WITH_SUFFIX(ptr7);
};

// These `CMD_***` macro should be defined before including this header,
// in command line arguments.
// OK: code owner has no control over system header.
struct SysStructWithCmdMacro {
  int* ptr0;
  CMD_INT* ptr1;
  CMD_INTP ptr2;
  int* CMD_CONST ptr3;
  int* CMD_ATTR ptr4;
  CMD_INTP_FIELD();
  CMD_TYPE_WITH_SUFFIX(SysTyp) * ptr5;
  int* CMD_SYMBOL(ptr6);
  int* CMD_SYMBOL_WITH_SUFFIX(ptr7);
};

// These `USR_***` macro should be defined before including this header,
// in `//tools/clang/plugins/tests/raw_ptr_fields_macro.cpp`.
struct SysStructWithUsrMacro {
  // OK: code owner has no control over system header.
  int* ptr0;
  // OK: code owner has no control over system header.
  USR_INT* ptr1;
  // OK: code owner has no control over system header.
  USR_INTP ptr2;
  // OK: code owner has no control over system header.
  int* USR_CONST ptr3;
  // OK: code owner has no control over system header.
  int* USR_ATTR ptr4;
  // Error: user has control over the macro.
  USR_INTP_FIELD();
  // OK: code owner has no control over system header.
  USR_TYPE_WITH_SUFFIX(SysTyp) * ptr5;
  // OK: code owner has no control over system header.
  int* USR_SYMBOL(ptr6);
  // OK: the source location for this field declaration will be "<scratch
  // space>" and the real file path cannot be detected.
  int* USR_SYMBOL_WITH_SUFFIX(ptr7);
};

#endif  // TOOLS_CLANG_PLUGINS_TESTS_SYSTEM_RAW_PTR_SYSTEM_TEST_H_
