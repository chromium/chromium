// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_DIR_NOT_CHECKED_HEADER_H_
#define TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_DIR_NOT_CHECKED_HEADER_H_

#include <system_unsafe_buffers.h>

inline int allowed_bad_stuff(int* i, unsigned s) {
  return i[s]     // This is in a known-bad header, so no error is emitted.
         + i[s];  // The second one uses caching and still no error.
}

// Lie about what file this is. The plugin should not care since it wants to
// apply paths to the file where the text is written, which is this file's path.
#line 38 "some_other_file.h"

int misdirected_bad_stuff(int* i, unsigned s) {
  return i[s];  // This should not make an error since it's in a known-bad
                // header.
}

#endif  // TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_NOT_CLEAN_DIR_NOT_CHECKED_HEADER_H_
