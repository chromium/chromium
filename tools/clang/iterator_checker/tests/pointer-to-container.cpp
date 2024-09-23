// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

void PointerToContainerTest(std::vector<int>* v) {
  auto it = v->begin();

  // Invalid because it was not checked against `end`.
  *it = 0;

  if (it == std::end(*v)) {
    return;
  }

  // Valid because it was checked against `end`.
  *it;
}
