// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_OPT_OUT_H_
#define TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_OPT_OUT_H_

#pragma allow_unsafe_buffers

inline int opt_out_bad_stuff(int* i, unsigned s) {
  return i[s];  // This header would be checked but the pragma disables it.
}

#endif  // TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_OPT_OUT_H_
