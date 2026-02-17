// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "./third_party/do_not_rewrite/header-original.h"
#include "base/containers/span.h"
#include "header-original.h"

// Declared from first party.
//
// Expected rewrite:
// void ProcessBuffer1(base::span<int> buffer, int size) {
void ProcessBuffer1(base::span<int> buffer, int size) {
  for (int i = 0; i < size; i++) {
    buffer[i] = buffer[i] + 1;
  }
}

// Declared from third party.
//
// No expected rewrite: Because ProcessBuffer4 has a declaration in
// third_party/.
void ProcessBuffer4(int* buffer, int size) {
  for (int i = 0; i < size; i++) {
    buffer[i] = buffer[i] * 2;
  }
}

void AllocateAndProcess() {
  std::vector<int> buffer(10);
  // Expected rewrite:
  // ProcessBuffer1(buffer, buffer.size());
  ProcessBuffer1(buffer, buffer.size());

  // Expected rewrite:
  // ProcessBuffer2(buffer.data(), buffer.size());
  ProcessBuffer2(buffer.data(), buffer.size());

  // Expected rewrite:
  // ProcessBuffer3(buffer.data(), buffer.size());
  ProcessBuffer3(buffer.data(), buffer.size());

  // No expected rewrite: Because ProcessBuffer4 has a declaration in
  // third_party/.
  ProcessBuffer4(buffer.data(), buffer.size());
}
