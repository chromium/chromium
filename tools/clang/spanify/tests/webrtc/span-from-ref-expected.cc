// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <span>
#include <tuple>

namespace webrtc {

// Expected rewrite:
// void processBuffer(std::span<int> buf) {
void processBuffer(std::span<int> buf) {
  std::ignore = buf[0];
}

void test() {
  int singleInt = 0;
  // Expected rewrite:
  // processBuffer(SPAN_FROM_REF_NOT_AVAILABLE(singleInt));
  processBuffer(SPAN_FROM_REF_NOT_AVAILABLE(singleInt));
}

}  // namespace webrtc
