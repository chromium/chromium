// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/rust/fend_core/v1/wrapper/fend_core.h"

#include <optional>
#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fend_core {
namespace {

using ::testing::Eq;
using ::testing::Optional;

TEST(FendCoreTest, SimpleMath) {
  std::optional<std::string> result = evaluate("1 + 1", /*timeout_in_ms=*/0);
  EXPECT_THAT(result, Optional(Eq("2")));
}

TEST(FendCoreTest, NoApproxString) {
  std::optional<std::string> result = evaluate("1/3", /*timeout_in_ms=*/0);
  EXPECT_THAT(result, Optional(Eq("0.33")));
}

TEST(FendCoreTest, FiltersTrivialResult) {
  std::optional<std::string> result = evaluate("1", /*timeout_in_ms=*/0);
  EXPECT_THAT(result, std::nullopt);
}

TEST(FendCoreTest, FiltersUnitOnlyQueries) {
  std::optional<std::string> result = evaluate("meter", /*timeout_in_ms=*/0);
  EXPECT_THAT(result, std::nullopt);
}

TEST(FendCoreTest, FiltersLambdaResults) {
  std::optional<std::string> result = evaluate("sqrt", /*timeout_in_ms=*/0);
  EXPECT_THAT(result, std::nullopt);
}

TEST(FendCoreTest, UnitConversion) {
  std::optional<std::string> result =
      evaluate("2 miles in meters", /*timeout_in_ms=*/0);
  EXPECT_THAT(result, Optional(Eq("3218.68 meters")));
}

TEST(FendCoreTest, HandlesInvalidInput) {
  std::optional<std::string> result = evaluate("abc", /*timeout_in_ms=*/0);
  EXPECT_EQ(result, std::nullopt);
}

TEST(FendCoreTest, CanTimeout) {
  std::optional<std::string> result =
      evaluate("10**100000", /*timeout_in_ms=*/500);
  EXPECT_EQ(result, std::nullopt);
}

}  // namespace
}  // namespace fend_core
