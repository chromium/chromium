// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_DIR_OPT_IN_HEADER_H_
#define TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_DIR_OPT_IN_HEADER_H_

inline int opt_in_bad_stuff(int* i, unsigned s) {
  return i[s];  // This file would not be checked, but the pragma opts in.
}

#pragma check_unsafe_buffers

#endif  // TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_DIR_OPT_IN_HEADER_H_
