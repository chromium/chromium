// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "common_audio/third_party/nested_audio.h"
namespace webrtc {

void TestWebRtcBoundaries(int index) {
  // Array passed to a deeply nested 2P third-party dependency.
  // Located in common_audio/third_party/, so it contains "third_party"
  // and must be treated as a frontier and emit .data().
  // Expected rewrite:
  // std::array<int, 4> nested_arr = {1, 2, 3, 4};
  std::array<int, 4> nested_arr = {1, 2, 3, 4};
  nested_arr[index] = 10;
  // Expected rewrite:
  // NestedAudioThirdPartyFunction(nested_arr.data());
  NestedAudioThirdPartyFunction(nested_arr.data());
}

}  // namespace webrtc
