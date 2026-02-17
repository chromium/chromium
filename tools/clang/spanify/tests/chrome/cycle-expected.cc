// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/containers/span.h"
#include "third_party/do_not_rewrite/third_party_api.h"

int UnsafeIndex();  // Return out-of-bounds index.
bool Bool();        // Unknown boolean value.

// Expected rewrite:
// base::span<int> GetFirstPartyBuffer() {
base::span<int> GetFirstPartyBuffer() {
  return new int[10];
}

void test_cycle_isolated() {
  // An isolated cycle with no dependencies.
  //
  // As of today, it gets rewritten, because nothing prevents it from being
  // rewritten, and some of its dependents would like to be spanified.

  // Expected rewrite:
  // base::span<int> cycle_1;
  base::span<int> cycle_1;
  // Expected rewrite:
  // base::span<int> cycle_2;
  base::span<int> cycle_2;
  cycle_1 = cycle_2;
  cycle_2 = cycle_1;

  // Expected rewrite:
  // base::span<int> output;
  base::span<int> output;
  if (Bool()) {
    output = GetFirstPartyBuffer();
  } else {
    output = cycle_1;
  }
  std::ignore = output[UnsafeIndex()];
}

void test_cycle_depending_on_first_party_buffer() {
  // A cycle that can be rewritten, because it depends on something that can be
  // rewritten, and dependants would like to be spanified.

  // Expected rewrite:
  // base::span<int> cycle_1 = GetFirstPartyBuffer();
  base::span<int> cycle_1 = GetFirstPartyBuffer();
  // Expected rewrite:
  // base::span<int> cycle_2;
  base::span<int> cycle_2;
  cycle_1 = cycle_2;
  cycle_2 = cycle_1;

  // Expected rewrite:
  // base::span<int> output;
  base::span<int> output;
  if (Bool()) {
    output = GetFirstPartyBuffer();
  } else {
    output = cycle_1;
  }
  std::ignore = output[UnsafeIndex()];
}

void test_cycle_depending_on_third_party_buffer() {
  // A cycle that can't be rewritten, because it depends on something that can't
  // be rewritten.

  // The current implementation of the spanifier tool assumes that all third-
  // party functions tell the size someway. The implementation is below.
  // https://source.chromium.org/chromium/chromium/src/+/main:tools/clang/spanify/Spanifier.cpp;drc=f240549b481969cf1442aac67a9086bfd639787c;l=2190-2191
  //
  // This test case was originally introduced to test that a buffer returned by
  // a third-party function shouldn't be rewritten to base::span, but it's not
  // been the intended behavior. A bug caused that the buffer doesn't get
  // rewritten, but the bug was fixed in https://crrev.com/c/6348037.
  //
  // TODO: Revisit this point and determine whether buffers returned by third-
  // party functions must be spanified unconditionally or in a certain
  // condition.
  //
  // Expected rewrite:
  // base::span<int> cycle_1 = GetThirdPartyBuffer();
  base::span<int> cycle_1 = GetThirdPartyBuffer();
  // Expected rewrite:
  // base::span<int> cycle_2;
  base::span<int> cycle_2;
  cycle_1 = cycle_2;
  cycle_2 = cycle_1;

  // Expected rewrite:
  // base::span<int> output;
  base::span<int> output;
  if (Bool()) {
    output = GetFirstPartyBuffer();
  } else {
    output = cycle_1;
  }
  std::ignore = output[UnsafeIndex()];
}
