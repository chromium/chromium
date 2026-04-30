// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

namespace webrtc {

// Expected rewrite:
// void processBuffer(std::span<int> buf) {
void processBuffer(int* buf) {
  std::ignore = buf[0];
}

void test() {
  int singleInt = 0;
  // Expected rewrite:
  // processBuffer(SPAN_FROM_REF_NOT_AVAILABLE(singleInt));
  processBuffer(&singleInt);
}

}  // namespace webrtc
