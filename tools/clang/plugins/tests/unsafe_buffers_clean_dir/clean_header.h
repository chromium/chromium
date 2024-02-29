// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_CLEAN_DIR_CLEAN_HEADER_H_
#define TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_CLEAN_DIR_CLEAN_HEADER_H_

int in_a_dir_clean_bad_stuff(int* i, unsigned s) {
  return i[s];  // This is in a "clean" file, so it should make a warning.
}

int in_a_dir_clean_guarded_bad_stuff(int* i, unsigned s) {
  return UNSAFE_BUFFERS(i[s]);  // Guarded so no warning.
}

UNSAFE_FN void in_a_dir_unsafe_fn() {}

inline void in_a_dir_call_unsafe_stuff() {
  in_a_dir_unsafe_fn();  // Unannotated call causes error.
  in_a_dir_unsafe_fn();  // Second one uses caching and still makes an error.

  // Annotated call is okay.
  UNSAFE_BUFFERS(in_a_dir_unsafe_fn());
}

#endif  // TOOLS_CLANG_PLUGINS_TESTS_UNSAFE_BUFFERS_CLEAN_DIR_CLEAN_HEADER_H_
