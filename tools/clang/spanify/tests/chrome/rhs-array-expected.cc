// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/containers/span.h"

void Test() {
  int16_t arr[10] = {1, 2, 3};
  // Expected rewrite:
  // base::span<const int16_t> p = arr;
  base::span<const int16_t> p = arr;
  // Expected rewrite:
  // p = p.subspan(1u);
  p = p.subspan(1u);
}
