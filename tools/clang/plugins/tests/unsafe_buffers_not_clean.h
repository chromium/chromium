// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_H_
#define TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_H_

inline int allowed_bad_stuff(int* i, unsigned s) {
  return i[s]     // This is in a known-bad header, so no error is emitted.
         + i[s];  // The second one uses caching and still no error.
}

#endif  // TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_H_
