// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/rust/fend_core/v1/wrapper/fend_core.h"

#include <optional>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace fend_core {
namespace {

using ::testing::Eq;
using ::testing::Optional;

// TODO: crbug.com/40240570 - Re-enable once MSan stops failing on Rust-side
// allocations.
#if defined(MEMORY_SANITIZER)
#define MAYBE_FendCoreTest DISABLED_FendCoreTest
#else
#define MAYBE_FendCoreTest FendCoreTest
#endif

TEST(MAYBE_FendCoreTest, SimpleMath) {
  std::optional<std::string> result = evaluate("1 + 1");
  EXPECT_THAT(result, Optional(Eq("2")));
}

TEST(MAYBE_FendCoreTest, UnitConversion) {
  std::optional<std::string> result = evaluate("2 miles in meters");
  EXPECT_THAT(result, Optional(Eq("3218.688 meters")));
}

// This test passes MSan as it does not allocate on the Rust side. However, we
// should still disable this in case `fend_core` starts allocating on this test.
TEST(MAYBE_FendCoreTest, HandlesInvalidInput) {
  std::optional<std::string> result = evaluate("abc");
  EXPECT_EQ(result, std::nullopt);
}

TEST(MAYBE_FendCoreTest, CanTimeout) {
  std::optional<std::string> result = evaluate("10**100000");
  EXPECT_EQ(result, std::nullopt);
}

}  // namespace
}  // namespace fend_core
