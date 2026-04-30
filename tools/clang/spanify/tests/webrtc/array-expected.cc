// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <tuple>

namespace webrtc {

void array_fct() {
  // Expected rewrite:
  // std::array<int, 5> arr = {1, 2, 3, 4, 5};
  std::array<int, 5> arr = {1, 2, 3, 4, 5};
  int index = 2;
  // Trigger spanification.
  std::ignore = arr[index];
}

}  // namespace webrtc
