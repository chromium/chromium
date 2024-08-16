// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "<array>"

// No rewrite expected.
extern const int kPropertyVisitedIDs[];

void fct() {
  // Expected rewrite:
  // std::array<int, 4> buf = {1, 2, 3, 4};
  std::array<int, 4> buf = {1, 2, 3, 4};
  int index = 0;
  buf[index] = 11;

  // Expected rewrite:
  // std::array<int, 5> buf2 = {1, 1, 1, 1, 1};
  std::array<int, 5> buf2 = {1, 1, 1, 1, 1};
  buf2[index] = 11;

  constexpr int size = 5;
  // Expected rewrite:
  // constexpr std::array<int, size> buf3 = {1, 1, 1, 1, 1};
  constexpr std::array<int, size> buf3 = {1, 1, 1, 1, 1};
  (void)buf3[index];

  // Expected rewrite:
  // std::array<int, buf3[0]> buf4;
  std::array<int, buf3[0]> buf4;
  buf4[index] = 11;

  index = kPropertyVisitedIDs[index];
}
