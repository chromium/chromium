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

#include <system_unsafe_buffers.h>

#include "unsafe_buffers_clean.h"
#include "unsafe_buffers_not_clean.h"
#include "unsafe_buffers_not_clean_dir/clean_header.h"
#include "unsafe_buffers_not_clean_dir/not_checked_header.h"
#include "unsafe_buffers_not_clean_dir/not_clean_header.h"
#include "unsafe_buffers_not_clean_dir/opt_in_header.h"
#include "unsafe_buffers_opt_out.h"

int main() {
  call_unsafe_stuff();
  in_a_dir_call_unsafe_stuff();
}
