// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <vector>

int UnsafeIndex();  // This function might return an out-of-bound index.
std::vector<int> buffer = {0, 0, 0, 0};

// Regression test: The result of an external function call, implicitly casted
// into a boolean used to violate assertions.
bool test_with_external_function() {
  if (std::getenv("TEST")) {
    return true;
  } else {
    return false;
  }
}

// Expected rewrite:
// base::span<int> internal_function_rewritten() {
int* internal_function_rewritten() {
  return buffer.data();
}
bool test_with_internal_function_with_rewrite() {
  internal_function_rewritten()[UnsafeIndex()] = 1;  // Force rewrite.

  // Expected rewrite:
  // if (!internal_function_rewritten().empty()) {
  if (internal_function_rewritten()) {
    return true;
  } else {
    return false;
  }
}

int* internal_function_not_rewritten() {
  return buffer.data();
}
bool test_with_internal_function_without_rewrite() {
  if (internal_function_not_rewritten()) {
    return true;
  } else {
    return false;
  }
}
