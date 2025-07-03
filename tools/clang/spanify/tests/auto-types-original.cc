// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/memory/raw_ptr.h"

// Expected rewrite:
// base::span<int> get_int_ptr();
int* get_int_ptr();

void test_basics() {
  int buf[] = {1, 2, 3};

  // Expected rewrite:
  // base::span<int> ptr1 = buf;
  auto* ptr1 = buf;
  std::ignore = ptr1[0];

  // TODO(yukishiino): Support this case.
  auto ptr2 = buf;
  std::ignore = ptr2[0];

  // TODO(yukishiino): Support this case.
  auto ptr3 = &buf;
  std::ignore = ptr3[0];

  // Expected rewrite:
  // base::span<int> ptr4 = static_cast<int*>(buf);
  auto ptr4 = static_cast<int*>(buf);
  std::ignore = ptr4[0];

  // Expected rewrite:
  // base::span<int> ptr5 = get_int_ptr();
  auto ptr5 = get_int_ptr();
  std::ignore = ptr5[0];

  // TODO(yukishiino): Support this case.
  auto ptr6 = base::raw_ptr<int>(new int[10]);
  std::ignore = ptr6[0];
}
