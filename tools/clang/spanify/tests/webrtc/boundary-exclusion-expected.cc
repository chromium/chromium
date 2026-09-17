// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "tools/clang/spanify/tests/third_party/webrtc/api/plain_webrtc.h"
#include "tools/clang/spanify/tests/third_party/webrtc/common_audio/third_party/vendored_audio.h"

namespace webrtc {

void TestWebRtcBoundaries(int index) {
  // The cases below deliberately live outside tests/webrtc/, under a simulated
  // checkout root at tests/third_party/webrtc/. Files inside tests/webrtc/ are
  // classified by the test-corpus prefix, which short circuits before the
  // submodule prefix is ever consulted; this layout is the only way to
  // exercise that second classification path of IsExcludedFromSubmodule.

  // Regular submodule code, at third_party/webrtc/api/. Nothing follows the
  // submodule root, so it is not excluded: its signature is spanified and the
  // call site needs no .data(). This guards the regression where every file
  // in the submodule was excluded and the rewrite produced zero patches.
  // Expected rewrite:
  // std::array<int, 4> plain_arr = {1, 2, 3, 4};
  std::array<int, 4> plain_arr = {1, 2, 3, 4};
  plain_arr[index] = 10;
  // No rewrite expected at this call site: the callee's own parameter is
  // spanified, so no .data() frontier is needed.
  PlainWebRtcFunction(plain_arr);

  // Vendored library, at third_party/webrtc/common_audio/third_party/. A
  // third_party/ directory follows the submodule root, so it is excluded and
  // the call becomes a frontier.
  // Expected rewrite:
  // std::array<int, 4> vendored_arr = {1, 2, 3, 4};
  std::array<int, 4> vendored_arr = {1, 2, 3, 4};
  vendored_arr[index] = 10;
  // Expected rewrite:
  // VendoredAudioFunction(vendored_arr.data());
  VendoredAudioFunction(vendored_arr.data());
}

}  // namespace webrtc
