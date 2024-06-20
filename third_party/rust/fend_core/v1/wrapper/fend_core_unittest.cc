// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/rust/fend_core/v1/wrapper/fend_core.h"

#include <optional>
#include <string>
#include <string_view>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fend_core {
namespace {

using ::testing::Eq;
using ::testing::Optional;

struct FendCoreTestCase {
  std::string name;
  std::string_view query;
  std::optional<std::string> expected;
};

class FendCoreParamTest : public testing::TestWithParam<FendCoreTestCase> {};

TEST_P(FendCoreParamTest, Test) {
  std::optional<std::string> result =
      evaluate(GetParam().query, /*timeout_in_ms=*/0);
  EXPECT_EQ(result, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(
    , FendCoreParamTest,
    testing::ValuesIn<FendCoreTestCase>({
        {
            .name = "SimpleMath",
            .query = "1 + 1",
            .expected = "2",
        },
        {
            .name = "NoApproxString",
            .query = "1/3",
            .expected = "0.33",
        },
        {
            .name = "FiltersTrivialResult",
            .query = "1",
            .expected = std::nullopt,
        },
        {
            .name = "FiltersUnitOnlyQueries",
            .query = "meter",
            .expected = std::nullopt,
        },
        {
            .name = "FiltersLambdaResults",
            .query = "sqrt",
            .expected = std::nullopt,
        },
        {
            .name = "UnitConversion",
            .query = "2 miles in meters",
            .expected = "3218.68 meters",
        },
        {
            .name = "HandlesInvalidInput",
            .query = "abc",
            .expected = std::nullopt,
        },
    }),
    [](const testing::TestParamInfo<FendCoreTestCase> &info) {
      return info.param.name;
    });

TEST(FendCoreTest, CanTimeout) {
  std::optional<std::string> result =
      evaluate("10**100000", /*timeout_in_ms=*/500);
  EXPECT_EQ(result, std::nullopt);
}

} // namespace
} // namespace fend_core
