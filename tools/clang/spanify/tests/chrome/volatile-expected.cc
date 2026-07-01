// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/containers/span.h"

namespace function_params_and_return {

// Expected rewrite
// base::span<int const> get_buf_east();
base::span<int const> get_buf_east();

// Expected rewrite
// base::span<int const> get_buf_east() {
base::span<int const> get_buf_east() {
  static std::vector<int> buf;
  return buf;
}

void f_east() {
  // Expected rewrite
  // base::span<const int> buf = get_buf_east();
  base::span<const int> buf = get_buf_east();
  (void)buf[0];
}

// Test volatile const
// Expected rewrite
// base::span<const volatile int> get_buf_volatile();
base::span<const volatile int> get_buf_volatile();
// Expected rewrite
// base::span<const volatile int> get_buf_volatile() {
base::span<const volatile int> get_buf_volatile() {
  static std::vector<int> buf;
  // Expected rewrite
  // return buf;
  return buf;
}
void f_volatile() {
  // Expected rewrite
  // base::span<const volatile int> buf = get_buf_volatile();
  base::span<const volatile int> buf = get_buf_volatile();
  (void)buf[0];
}

}  // namespace function_params_and_return
