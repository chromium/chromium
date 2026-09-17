// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_TESTS_THIRD_PARTY_WEBRTC_COMMON_AUDIO_THIRD_PARTY_VENDORED_AUDIO_H_
#define TOOLS_CLANG_SPANIFY_TESTS_THIRD_PARTY_WEBRTC_COMMON_AUDIO_THIRD_PARTY_VENDORED_AUDIO_H_

// Counterpart to plain_webrtc.h: a third-party library vendored inside the
// simulated checkout, at third_party/webrtc/common_audio/third_party/. Also
// classified by submodule prefix, but excluded because a third_party/
// directory follows the submodule root.
inline void VendoredAudioFunction(const int* buffer) {
  int x = buffer[0];
}

#endif  // TOOLS_CLANG_SPANIFY_TESTS_THIRD_PARTY_WEBRTC_COMMON_AUDIO_THIRD_PARTY_VENDORED_AUDIO_H_
