// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents code that's excluded - i.e. not desirable to rewrite, even
// if it uses buffers unsafely.
#include "base/dummy_rewrite_bait_for_testing.h"

// Expected rewrite:
// ...(base::span<const unsigned char> arg) {
unsigned char UseBufferUnsafely(const unsigned char* arg) {
  // We disallow rewrites in the file in which this function is defined.
  //
  // TODO(crbug.com/419598098): figure out why this doesn't work.
  //
  // Expected rewrite:
  // return GetUnsafeChar(arg.subspan(1u).data());
  return GetUnsafeChar(arg + 1);
}
