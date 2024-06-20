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
            .name = "LowercaseXAsMultiplication",
            .query = "4 x 5",
            .expected = std::nullopt,
        },
        {
            .name = "UppercaseXAsMultiplication",
            .query = "4 X 5",
            .expected = std::nullopt,
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
            .name = "HandlesInvalidInput",
            .query = "abc",
            .expected = std::nullopt,
        },

        {
            .name = "UnitConversion",
            .query = "2 miles in meters",
            .expected = "3218.68 meters",
        },

        {
            .name = "UnitAddition",
            .query = "10cm + 2m",
            .expected = "210 cm",
        },
        {
            .name = "UnitSubtraction",
            .query = "10cm - 2m",
            .expected = "-190 cm",
        },
        {
            .name = "UnitMultiplication",
            .query = "10cm * 2m",
            .expected = "2000 cm^2",
        },
        {
            .name = "UnitDivision",
            .query = "10cm / 2m",
            .expected = "500 cm^2",
        },

        {
            .name = "ImplicitAddUnitAddition",
            .query = "2m 20cm + 1m 10cm",
            .expected = "3.3 m",
        },
        {
            .name = "ImplicitAddUnitSubtraction",
            .query = "2m 20cm - 1m 10cm",
            .expected = "1.1 m",
        },
        {
            .name = "ImplicitAddUnitMultiplication",
            .query = "2m 20cm * 1m 10cm",
            .expected = std::nullopt,
        },
        {
            .name = "ImplicitAddUnitDivision",
            .query = "2m 20cm / 1m 10cm",
            .expected = std::nullopt,
        },

        {
            .name = "ImplicitMulUnitAddition",
            .query = "4 m s + 2 m s",
            .expected = "6 m s",
        },
        {
            .name = "ImplicitMulUnitSubtraction",
            .query = "4 m s - 2 m s",
            .expected = "2 m s",
        },
        {
            .name = "ImplicitMulUnitMultiplication",
            .query = "4 m s * 2 m s",
            .expected = "8 m^2 s^2",
        },
        {
            .name = "ImplicitMulUnitDivision",
            .query = "4 m s / 2 m s",
            .expected = "2 m^2 s^2",
        },

        {
            .name = "MixedFractionUnitAddition",
            .query = "3 1/3 cm + 6 2/3 mm",
            .expected = "4 cm",
        },
        {
            .name = "MixedFractionUnitSubtraction",
            .query = "3 1/3 cm - 6 2/3 mm",
            .expected = "2.66 cm",
        },
        {
            .name = "MixedFractionUnitMultiplication",
            .query = "3 1/3 cm * 6 2/3 mm",
            .expected = "2.22 cm^2",
        },
        {
            .name = "MixedFractionUnitDivision",
            .query = "3 1/3 cm / 6 2/3 mm",
            .expected = std::nullopt,
        },

        {
            .name = "UnitFraction",
            .query = "3 / 2 m",
            .expected = "1.5 m",
        },
        {
            .name = "UnitHalfFraction",
            .query = "3 half / 2 m",
            .expected = "0.75 m",
        },

        {
            .name = "PercentSymbol",
            .query = "11/12*100%",
            .expected = "0.91",
        },
        {
            .name = "MultiplePercent",
            .query = "50% * 50%",
            .expected = "0.25",
        },
        {
            .name = "DivideByPercent",
            .query = "1 / (80%)",
            .expected = "1.25",
        },
        {
            .name = "PercentTimesUnit",
            .query = "50% * 2cm",
            .expected = "1 cm",
        },

        {
            .name = "UppercaseLog",
            .query = "LOG 10",
            .expected = std::nullopt,
        },
        {
            .name = "UppercaseExp",
            .query = "EXP 0",
            .expected = std::nullopt,
        },
        {
            .name = "UppercaseSin",
            .query = "SIN pi",
            .expected = std::nullopt,
        },
        // Ensures that we don't accidentally break case-sensitive queries.
        {
            .name = "CasedUnits",
            .query = "10 kB to kb",
            .expected = "80 kb",
        },

        // Use `+ 0` to ensure a result isn't filtered out.
        {
            .name = "PositiveRoundingBelowHalf",
            .query = "0.123 + 0",
            .expected = "0.12",
        },
        {
            .name = "PositiveRoundingHalfEven",
            .query = "0.125 + 0",
            .expected = "0.12",
        },
        {
            .name = "PositiveRoundingHalfOdd",
            .query = "0.135 + 0",
            .expected = "0.13",
        },
        {
            .name = "PositiveRoundingAboveHalf",
            .query = "0.127 + 0",
            .expected = "0.12",
        },
        {
            .name = "NegativeRoundingBelowHalf",
            .query = "-0.123 + 0",
            .expected = "-0.12",
        },
        {
            .name = "NegativeRoundingHalfEven",
            .query = "-0.125 + 0",
            .expected = "-0.12",
        },
        {
            .name = "NegativeRoundingHalfOdd",
            .query = "-0.135 + 0",
            .expected = "-0.13",
        },
        {
            .name = "NegativeRoundingAboveHalf",
            .query = "-0.127 + 0",
            .expected = "-0.12",
        },

        {
            .name = "MorePrecision",
            .query = "0.127 to 3dp",
            .expected = "0.12",
        },
        {
            .name = "ToExact",
            .query = "1/3 + 1/2 to exact",
            .expected = "0.83",
        },

        {
            .name = "DegreesC",
            .query = "100 degrees C to F",
            .expected = "35.14 °F",
        },
        {
            .name = "DegreesCelsius",
            .query = "100 degrees Celsius to F",
            .expected = "35.14 °F",
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
