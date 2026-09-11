// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_TESTS_WEBRTC_COMMON_AUDIO_THIRD_PARTY_NESTED_AUDIO_H_
#define TOOLS_CLANG_SPANIFY_TESTS_WEBRTC_COMMON_AUDIO_THIRD_PARTY_NESTED_AUDIO_H_

// Simulates a deeply nested third-party library in WebRTC
// (e.g. common_audio/third_party/spl_sqrt_floor/).
// It uses buffer indexing, which triggers spanification if not excluded.
inline void NestedAudioThirdPartyFunction(const int* buffer) {
  int x = buffer[0];
}

#endif  // TOOLS_CLANG_SPANIFY_TESTS_WEBRTC_COMMON_AUDIO_THIRD_PARTY_NESTED_AUDIO_H_
