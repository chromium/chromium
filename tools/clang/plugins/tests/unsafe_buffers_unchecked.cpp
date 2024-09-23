// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define UNSAFE_FN [[clang::unsafe_buffer_usage]]

// clang-format off
#define UNSAFE_BUFFERS(...)                  \
  _Pragma("clang unsafe_buffer_usage begin") \
  __VA_ARGS__                                \
  _Pragma("clang unsafe_buffer_usage end")
// clang-format on

#include "unsafe_buffers_not_clean_dir/clean_header.h"
#include "unsafe_buffers_not_clean_dir/not_clean_header.h"
#include "unsafe_buffers_not_clean_dir/still_not_clean_dir_1/not_clean_header.h"
#include "unsafe_buffers_not_clean_dir/still_not_clean_dir_2/not_clean_header.h"

// This is in a known-bad cc file, so no error is emitted.
DO_UNSAFE_THING_FROM_CHECKED_HEADER(UncheckedCpp, N, i, s);    // No error.
DO_UNSAFE_THING_FROM_UNCHECKED_HEADER(UncheckedCpp, N, i, s);  // No error.

inline int allowed_bad_stuff_in_cpp(int* i, unsigned s) {
  auto x = [&]() { return i; };
  // This is in a known-bad cc file, so no error is emitted.
  return MACRO_CALL_FUNCTION_FROM_CHECKED_HEADER(x)[s] +    // No error.
         MACRO_CALL_FUNCTION_FROM_UNCHECKED_HEADER(x)[s] +  // No error.
         i[s];                                              // No error.
}

int main() {
  int i;
  allowed_bad_stuff_in_cpp(&i, 1u);

  auto a = UncheckStructThingTryToMakeScratchBufferUncheckedCpp();
  auto b = CheckStructThingTryToMakeScratchBufferUncheckedCpp

      ();
}
