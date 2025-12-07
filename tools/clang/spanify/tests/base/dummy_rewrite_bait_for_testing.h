// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_TESTS_BASE_DUMMY_REWRITE_BAIT_FOR_TESTING_H_
#define TOOLS_CLANG_SPANIFY_TESTS_BASE_DUMMY_REWRITE_BAIT_FOR_TESTING_H_

unsigned UnsafeIndex();

// This function uses buffers unsafely, but represents a file that we don't wish
// to spanify (i.e. included in `SpanifyManualPathsToIgnore.h`).
//
// Note: the test harness doesn't appear to attempt to rewrite this anyway.
// The main regression we guard against is `arg` being spanified and passed
// into `GetUnsafeChar()` under the assumption that this function also gets
// spanified.
unsigned char GetUnsafeChar(const unsigned char* arg) {
  return arg[UnsafeIndex()];
}

#endif  // TOOLS_CLANG_SPANIFY_TESTS_BASE_DUMMY_REWRITE_BAIT_FOR_TESTING_H_
