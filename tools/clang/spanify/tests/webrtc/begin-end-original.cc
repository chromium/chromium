// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

void F() {
  int arr[3] = {};
  // No rewrite expected.
  for (int* it = std::begin(arr); it != std::end(arr); ++it) {
  }
}
