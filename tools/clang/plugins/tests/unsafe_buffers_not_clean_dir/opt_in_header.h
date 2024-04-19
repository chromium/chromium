// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_DIR_OPT_IN_HEADER_H_
#define TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_DIR_OPT_IN_HEADER_H_

#include "clean_header.h"
#include "not_clean_header.h"

// This file would not be checked, but the pragma opts in.
DO_UNSAFE_THING_FROM_CHECKED_HEADER(OptIn, N, i, s);    // Should error.
DO_UNSAFE_THING_FROM_UNCHECKED_HEADER(OptIn, N, i, s);  // Should error.

inline int opt_in_bad_stuff(int* i, unsigned s) {
  auto a = UncheckStructThingTryToMakeScratchBufferOptIn();
  auto b = CheckStructThingTryToMakeScratchBufferOptIn();

  auto x = [&]() { return i; };
  // This file would not be checked, but the pragma opts in.
  return MACRO_CALL_FUNCTION_FROM_CHECKED_HEADER(x)[s] +    // Should error.
         MACRO_CALL_FUNCTION_FROM_UNCHECKED_HEADER(x)[s] +  // Should error.
         i[s];                                              // Should error.
}

#pragma check_unsafe_buffers

#endif  // TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_DIR_OPT_IN_HEADER_H_
