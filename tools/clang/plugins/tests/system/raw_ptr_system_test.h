// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_SYSTEM_RAW_PTR_SYSTEM_TEST_H_
#define TOOLS_CLANG_PLUGINS_TESTS_SYSTEM_RAW_PTR_SYSTEM_TEST_H_

#define INT int
#define INTP_FIELD() int* macro_ptr

struct SystemStruct {
  int* ptr;  // OK: In a system header.
};

#endif  // TOOLS_CLANG_PLUGINS_TESTS_SYSTEM_RAW_PTR_SYSTEM_TEST_H_
