// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_TESTS_ANGLE_RELATIVE_INCLUDE_RELATIVE_INCLUDE_HELPER_H_
#define TOOLS_CLANG_SPANIFY_TESTS_ANGLE_RELATIVE_INCLUDE_RELATIVE_INCLUDE_HELPER_H_

// Reachable only through the relative `-I relative_include`, so clang reports
// this header's path in relative form, like a standalone ANGLE build does
// with `-I../../src`. It must still be rewritable.
inline void RelativeIncludeFunction(const int* buffer) {
  int x = buffer[0];
}

#endif  // TOOLS_CLANG_SPANIFY_TESTS_ANGLE_RELATIVE_INCLUDE_RELATIVE_INCLUDE_HELPER_H_
