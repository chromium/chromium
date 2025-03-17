// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <tuple>

int UnsafeIndex();      // Return out-of-bounds index.
bool Bool();            // Unknown boolean value.
int* ExternalBuffer();  // Return a buffer from an unknown source.

// Expect spanified.
int* InternalBuffer() {
  return new int[10];
}

void test_cycle_isolated() {
  // An isolated cycle with no dependencies.
  //
  // As of today, it gets rewritten, because nothing prevents it from being
  // rewritten, and some of its dependents would like to be spanified.
  int* cycle_1;  // Expect spanified.
  int* cycle_2;  // Expect spanified.
  cycle_1 = cycle_2;
  cycle_2 = cycle_1;

  int* output;
  if (Bool()) {
    output = InternalBuffer();
  } else {
    output = cycle_1;
  }
  std::ignore = output[UnsafeIndex()];
}

void test_cycle_depending_on_internal_buffer() {
  // A cycle that can be rewritten, because it depends on something that can be
  // rewritten, and dependants would like to be spanified.
  int* cycle_1 = InternalBuffer();  // Expect spanified.
  int* cycle_2;                     // Expect spanified.
  cycle_1 = cycle_2;
  cycle_2 = cycle_1;

  int* output;
  if (Bool()) {
    output = InternalBuffer();
  } else {
    output = cycle_1;
  }
  std::ignore = output[UnsafeIndex()];
}

void test_cycle_depending_on_external_buffer() {
  // A cycle that can't be rewritten, because it depends on something that can't
  // be rewritten.
  int* cycle_1 = ExternalBuffer();
  int* cycle_2;
  cycle_1 = cycle_2;  // Expect not spanified.
  cycle_2 = cycle_1;  // Expect not spanified.

  int* output;
  if (Bool()) {
    output = InternalBuffer();  // Expect frontier change (e.g. .data())
  } else {
    output = cycle_1;
  }
  std::ignore = output[UnsafeIndex()];
}
