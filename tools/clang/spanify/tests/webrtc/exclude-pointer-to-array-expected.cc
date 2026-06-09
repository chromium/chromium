// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>
#include <span>

int UnsafeIndex();

namespace webrtc {

// Regression test for https://crbug.com/501280389.
// Pointers to arrays must be excluded from spanification. Without exclusion,
// the tool used to produce invalid syntax like:
//   void TakesPointerToArray(std::span<uint8_t[16]> ptr)[16], int size);
void TakesPointerToArray(uint8_t (*ptr)[16], int size);

void TestPointerToArrayExclusion() {
  // Expected rewrite:
  // std::array<std::array<uint8_t, 16>, 8> buffer;
  std::array<std::array<uint8_t, 16>, 8> buffer;

  buffer[UnsafeIndex()][0] = 255;

  // Expected rewrite:
  // TakesPointerToArray(std::span<uint8_t[16]>(buffer).subspan(1u).data(), 7);
  TakesPointerToArray(std::span<uint8_t[16]>(buffer).subspan(1u).data(), 7);
}

}  // namespace webrtc
