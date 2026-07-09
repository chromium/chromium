// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JNI_ZERO_COMPILER_SPECIFIC_H_
#define JNI_ZERO_COMPILER_SPECIFIC_H_

#ifndef JNI_ZERO_UNSAFE_TODO
#if defined(__clang__)
// Disabling `clang-format` allows each `_Pragma` to be on its own line, as
// recommended by https://gcc.gnu.org/onlinedocs/cpp/Pragmas.html.
// clang-format off
#define JNI_ZERO_UNSAFE_TODO(...)            \
  _Pragma("clang unsafe_buffer_usage begin") \
  __VA_ARGS__                                \
  _Pragma("clang unsafe_buffer_usage end")
// clang-format on
#else
#define JNI_ZERO_UNSAFE_TODO(...) __VA_ARGS__
#endif
#endif

#endif  // JNI_ZERO_COMPILER_SPECIFIC_H_
