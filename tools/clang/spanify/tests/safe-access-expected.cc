// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/containers/span.h"

void test_safe_array_access_basic() {
  int buffer[10];  // Expect no rewrite, because 0 < 10.
  buffer[0] = 0;
}

void test_safe_array_access_basic_pointer() {
  int buffer[10];
  // Here, the plugin fails to detect the access to be safe.
  // Expected rewrite:
  // base::span<int> ptr = buffer;
  base::span<int> ptr = buffer;
  ptr[0] = 0;
}

void test_safe_array_access_computed_1() {
  std::array<int, 10> buffer;  // Expect no rewrite, because 5 + 4 < 10.
  int a = 5;
  int b = 4;
  int index = a + b;
  buffer[index] = 0;
}

void test_safe_array_access_computed_1_pointer() {
  int buffer[10];
  int a = 5;
  int b = 4;
  int index = a + b;
  // Here, the plugin fails to detect the access to be safe.
  // Expected rewrite:
  // base::span<int> ptr = buffer;
  base::span<int> ptr = buffer;
  ptr[index] = 0;
}

void test_safe_array_access_computed_2() {
  std::array<int, 10> buffer;  // Expect no rewrite, because 109 % 10 < 10.
  int index = 109;
  index %= 10;
  buffer[index] = 0;
}

void test_safe_array_access_computed_2_pointer() {
  int buffer[10];
  int index = 109;
  index %= 10;
  // Here, the plugin fails to detect the access to be safe.
  // Expected rewrite:
  // base::span<int> ptr = buffer;
  base::span<int> ptr = buffer;
  ptr[index] = 0;
}
