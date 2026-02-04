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

// Tests which should NOT break from a fend-core roll.
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
        // Division is not tested, see below.

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
        // Division is not tested, see below.

        // This should never have additional percents in the output.
        {
            .name = "PercentTimesUnit",
            .query = "50% * 2cm",
            .expected = "1 cm",
        },

        // Ensures that we don't accidentally break case-sensitive queries.
        {
            .name = "CasedUnits",
            .query = "10 kB to kb",
            .expected = "80 kb",
        },

        {
            .name = "PercentSymbol",
            .query = "11/12*100%",
            .expected = "91.66%",
        },
        {
            .name = "MultiplePercent",
            .query = "50% * 50%",
            .expected = "25%",
        },
        {
            .name = "DivideByPercent",
            .query = "1 / (80%)",
            .expected = "1.25",
        },

        {
            .name = "UppercaseLog",
            .query = "LOG 10",
            .expected = "1",
        },
        {
            .name = "UppercaseExp",
            .query = "EXP 0",
            .expected = "1",
        },
        {
            .name = "UppercaseSin",
            .query = "SIN pi",
            .expected = "0",
        },

        // Queries which could be interpreted as a partial non-math query.
        {
            .name = "PartialC",
            .query = "c",
            .expected = std::nullopt,
        },
        {
            .name = "PartialM",
            .query = "m",
            .expected = std::nullopt,
        },
        {
            .name = "PartialPi",
            .query = "pi",
            .expected = std::nullopt,
        },

        {
            .name = "LowercaseTemperature",
            .query = "50f to c",
            .expected = "10 °C",
        },
    }),
    [](const testing::TestParamInfo<FendCoreTestCase> &info) {
      return info.param.name;
    });

// Tests which COULD break from a fend-core roll due to behaviour which could
// change between Fend versions.
// These tests can be run with `--gtest_also_run_disabled_tests`. Once these
// tests are fixed or stabilised, they should be moved above.
INSTANTIATE_TEST_SUITE_P(
    DISABLED_KnownUnexpectedBehavior, FendCoreParamTest,
    testing::ValuesIn<FendCoreTestCase>({
        // TODO: b/336690740 - X should be able to be used for multiplication.
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

        // TODO: b/346472873 - Handle division more intuitively with units.
        // https://github.com/printfn/fend/issues/76
        {
            .name = "UnitDivision",
            .query = "10cm / 2m",
            .expected = "500 cm^2",
        },

        // Implicit addition with units is uncommon. We do not expect this
        // behaviour to change (except for fixing multiplication and division),
        // but we do not mind if this behaviour does change.
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

        // Unit division (see above).
        {
            .name = "ImplicitMulUnitDivision",
            .query = "4 m s / 2 m s",
            .expected = "2 m^2 s^2",
        },

        // Implicit addition with mixed fractions is uncommon. We do not expect
        // this behaviour to change (except for fixing division), but we do not
        // mind if this behaviour does change.
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

        // These queries are arguably ambiguous due to unit division (see above)
        // and could change meaning in a new Fend version.
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

        // Use `+ 0` to ensure a result isn't filtered out.
        // TODO: b/348067495 - Fend always rounds down.
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

        // Issues with our Fend wrapper.
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

        // TODO: b/347804567 - "degrees C" gives incorrect results.
        // https://github.com/printfn/fend/issues/62
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
