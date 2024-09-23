// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "stubs/check.h"

void IteratorValidAfterCheck(std::vector<int>& v) {
  auto it = v.begin();

  CHECK(it != v.end());

  // Valid because it was checked against `end`.
  *it;
}
