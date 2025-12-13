// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_STANDALONE_BASE_UNSAFE_BUFFERS_H_
#define IPCZ_SRC_STANDALONE_BASE_UNSAFE_BUFFERS_H_

// Standlone version of safe buffer macros for ipcz.

// Annotates a function or class data member indicating it can lead to
// out-of-bounds accesses (OOB) if given incorrect inputs.
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::unsafe_buffer_usage)
#define IPCZ_UNSAFE_BUFFER_USAGE [[clang::unsafe_buffer_usage]]
#endif
#endif

#ifndef IPCZ_UNSAFE_BUFFER_USAGE
#define IPCZ_UNSAFE_BUFFER_USAGE
#endif

// Annotates code indicating that it should be permanently exempted from
// `-Wunsafe-buffer-usage`.
#if defined(__clang__)
// clang-format off
#define IPCZ_UNSAFE_BUFFERS(...)                  \
  _Pragma("clang unsafe_buffer_usage begin") \
  __VA_ARGS__                                \
  _Pragma("clang unsafe_buffer_usage end")
// clang-format on
#endif

#ifndef IPCZ_UNSAFE_BUFFERS
#define IPCZ_UNSAFE_BUFFERS(...) __VA_ARGS__
#endif

// Annotates code indicating that it should be temporarily exempted from
// `-Wunsafe-buffer-usage`.
#define IPCZ_UNSAFE_TODO(...) IPCZ_UNSAFE_BUFFERS(__VA_ARGS__)

#endif  // IPCZ_SRC_STANDALONE_BASE_UNSAFE_BUFFERS_H_
