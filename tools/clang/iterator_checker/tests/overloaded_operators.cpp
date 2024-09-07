// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

void OverloadedOperatorPlus(std::vector<int>& v) {
  auto begin = std::begin(v);

  // Fine comparison because we are using the returned iterator.
  if (begin != std::end(v)) {
    auto next = begin + 1;

    // Valid because it was checked against `end`.
    *begin;

    // Invalid because it was not checked against `end`.
    *next;
  }
}

void OverloadedOperatorPlusPlus(std::vector<int>& v) {
  auto begin = std::begin(v);

  // Fine comparison because we are using the returned iterator.
  if (begin != std::end(v)) {
    auto next = begin++;

    // Both invalid because they were not checked against `end`.
    *begin;
    *next;

    if (begin != std::end(v)) {
      // Valid because it was checked against `end`.
      *begin;

      // Invalid because it was not checked against `end`.
      *next;
    }
  }
}

void OverloadedOperatorPlusEqual(std::vector<int>& v) {
  auto begin = std::begin(v);

  // Fine comparison because we are using the returned iterator.
  if (begin != std::end(v)) {
    auto next = begin += 1;

    // Both invalid because they were not checked against `end`.
    *begin;
    *next;

    if (begin != std::end(v)) {
      // Valid because it was checked against `end`.
      *begin;

      // Invalid because it was not checked against `end`.
      *next;
    }
  }
}
