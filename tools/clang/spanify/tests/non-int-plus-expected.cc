// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/span.h"

int UnsafeIndex();  // This function might return an out-of-bound index.

// Test that plus operator with non int type expression for the rhs is not
// rewritten to subspan.

void fct() {
  // Expected rewrite:
  // std::array<char, 4> prefix{"foo"};
  std::array<char, 4> prefix{"foo"};

  std::string bar = "bar";
  // This should not be rewritten to subspan.
  // Expected rewrite:
  // std::ignore = prefix.data() + bar;
  std::ignore = prefix.data() + bar;

  std::ignore = prefix[UnsafeIndex()];
}

void fct2() {
  auto buf = std::vector<char>(1, 1);
  // Expected rewrite:
  // base::span<char> expected_data = buf;
  base::span<char> expected_data = buf;

  std::string bar = "bar";
  // This should not be rewritten to subspan.
  // Expected rewrite:
  // std::ignore = expected_data.data() + bar;
  std::ignore = expected_data.data() + bar;

  std::ignore = expected_data[UnsafeIndex()];
}
