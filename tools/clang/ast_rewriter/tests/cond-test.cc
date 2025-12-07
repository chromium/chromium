// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma clang diagnostic ignored "-Wunused-variable"

#include <string>

void foo(bool b, int n) {
  auto x = b ? 1 : 2;
  auto y = b ? "true" : "fls";
  auto w = b ? "0" : "1";
  auto a = b ? "tluo" : "dalse";
  auto z = b ? "true" : "false";
  const char* z1 = b ? "true" : "false";
  const char* z2 = (b ? "true" : "false");
  std::string z3 = b ? "true" : "false";
  std::string z4 = b ? "false" : "true";
  std::string z5 = n == 5 ? "false" : "true";
}
