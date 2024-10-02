// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

void fct() {
  // Expected rewrite:
  // std::array<const char*, 6> buf = {"\\,", "+++",     "%%%2C",
  //                                   "@",   "<empty>", ":::"};
  std::array<const char*, 6> buf = {"\\,", "+++",     "%%%2C",
                                    "@",   "<empty>", ":::"};
  int index = 0;
  buf[index] = nullptr;
}
