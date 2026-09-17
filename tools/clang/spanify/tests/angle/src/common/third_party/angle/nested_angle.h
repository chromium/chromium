// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_TESTS_ANGLE_SRC_COMMON_THIRD_PARTY_ANGLE_NESTED_ANGLE_H_
#define TOOLS_CLANG_SPANIFY_TESTS_ANGLE_SRC_COMMON_THIRD_PARTY_ANGLE_NESTED_ANGLE_H_

// Simulates a third-party library vendored inside ANGLE; the real-world case
// is src/common/third_party/xxhash/. This fixture additionally repeats
// "third_party/angle/" in its path, the hardest shape for a substring
// predicate.
// It uses buffer indexing, which triggers spanification if not excluded.
inline void NestedAngleThirdPartyFunction(const int* buffer) {
  int x = buffer[0];
}

#endif  // TOOLS_CLANG_SPANIFY_TESTS_ANGLE_SRC_COMMON_THIRD_PARTY_ANGLE_NESTED_ANGLE_H_
