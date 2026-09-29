// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_TESTS_ANGLE_RELATIVE_INCLUDE_THIRD_PARTY_RELATIVE_INCLUDE_VENDORED_H_
#define TOOLS_CLANG_SPANIFY_TESTS_ANGLE_RELATIVE_INCLUDE_THIRD_PARTY_RELATIVE_INCLUDE_VENDORED_H_

// Simulates a library vendored inside ANGLE (e.g.
// src/common/third_party/xxhash/). Must stay excluded after the path is
// resolved.
inline void RelativeIncludeVendoredFunction(const int* buffer) {
  int x = buffer[0];
}

#endif  // TOOLS_CLANG_SPANIFY_TESTS_ANGLE_RELATIVE_INCLUDE_THIRD_PARTY_RELATIVE_INCLUDE_VENDORED_H_
