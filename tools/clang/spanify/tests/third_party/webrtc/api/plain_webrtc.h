// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_TESTS_THIRD_PARTY_WEBRTC_API_PLAIN_WEBRTC_H_
#define TOOLS_CLANG_SPANIFY_TESTS_THIRD_PARTY_WEBRTC_API_PLAIN_WEBRTC_H_

// Simulates ordinary WebRTC code as it is laid out in a real checkout, at
// third_party/webrtc/api/. Unlike the other fixtures this one lives outside
// tests/webrtc/, so the predicate cannot take its test-corpus shortcut and
// must classify the file by its submodule prefix instead. This is regular
// submodule code and must therefore be rewritten.
inline void PlainWebRtcFunction(const int* buffer) {
  int x = buffer[0];
}

#endif  // TOOLS_CLANG_SPANIFY_TESTS_THIRD_PARTY_WEBRTC_API_PLAIN_WEBRTC_H_
