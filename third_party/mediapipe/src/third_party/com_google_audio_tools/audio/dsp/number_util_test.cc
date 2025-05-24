/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/number_util.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "audio/dsp/testing_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Each;
using ::testing::Ge;
using ::testing::Lt;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::std::vector;

typedef std::pair<int, int> Fraction;

// Factorial values for 0 <= n <= 12. The factorial 12! = 479,001,600 is the
// largest factorial that fits in signed 32-bit integer range (13! is
// 6,227,020,800).
int Factorial(int n) {
  ABSL_CHECK_GE(n, 0);
  ABSL_CHECK_LE(n, 12);
  int result = 1;
  for (; n > 1; --n) {
    result *= n;
  }
  return result;
}

// Binomial coefficients for 0 <= k <= n <= 12.
int BinomialCoefficient(int n, int k) {
  ABSL_CHECK_LE(k, n);
  return Factorial(n) / (Factorial(k) * Factorial(n - k));
}

// Find the best rational approximation a/b of value by exhaustively testing
// each possible denominator 0 < b <= max_denominator.
Fraction ExhaustiveFindBestRationalApproximation(
    double value, int max_denominator) {
  Fraction best_rational(std::round(value), 1);
  double best_error = std::abs(value - best_rational.first);
  for (int denominator = 2; denominator <= max_denominator; ++denominator) {
    int numerator = std::round(value * denominator);
    double error = std::abs(value -
        static_cast<double>(numerator) / denominator);
    if (error < best_error) {
      best_rational = {numerator, denominator};
      best_error = error;
    }
  }
  const int gcd = GreatestCommonDivisor(
      std::abs(best_rational.first), best_rational.second);
  best_rational = {best_rational.first / gcd, best_rational.second / gcd};
  return best_rational;
}

TEST(NumberUtilTest, Modulo) {
  for (int modulus : {3, 4, 7, 10}) {
    for (int multiple = -2; multiple <= 2; ++multiple) {
      for (int i = 0; i < modulus; ++i) {
        const int value = modulus * multiple + i;
        EXPECT_EQ(i, Modulo(value, modulus))
            << "with Modulo(" << value << ", " << modulus << ")";
      }
    }
  }
}

TEST(NumberUtilTest, RoundDownToMultiple) {
  for (int factor : {3, 4, 7, 10}) {
    for (int multiple = -2; multiple <= 2; ++multiple) {
      for (int i = 0; i < factor; ++i) {
        const int value = factor * multiple + i;
        EXPECT_EQ(factor * multiple, RoundDownToMultiple(value, factor))
            << "with RoundDownToMultiple(" << value << ", " << factor << ")";
      }
    }
  }
}

TEST(NumberUtilTest, RoundUpToMultiple) {
  for (int factor : {3, 4, 7, 10}) {
    for (int multiple = -2; multiple <= 2; ++multiple) {
      for (int i = 0; i < factor; ++i) {
        const int value = factor * multiple - i;
        EXPECT_EQ(factor * multiple, RoundUpToMultiple(value, factor))
            << "with RoundUpToMultiple(" << value << ", " << factor << ")";
      }
    }
  }
}

TEST(NumberUtilTest, GreatestCommonDivisor) {
  EXPECT_EQ(25, GreatestCommonDivisor(200, 75));
  EXPECT_EQ(25, GreatestCommonDivisor(75, 200));
  EXPECT_EQ(7, GreatestCommonDivisor(7, 0));
  EXPECT_EQ(7, GreatestCommonDivisor(0, 7));
  EXPECT_EQ(1, GreatestCommonDivisor(64, 27));
  EXPECT_EQ(87, GreatestCommonDivisor(13 * 87, 16 * 87));
  EXPECT_EQ(7, GreatestCommonDivisor(7, 7));
  EXPECT_EQ(0, GreatestCommonDivisor(0, 0));
  // Vector signature.
  EXPECT_EQ(7, GreatestCommonDivisor({7, 7, 7}));
  EXPECT_EQ(1, GreatestCommonDivisor({1, 7, 7}));
  EXPECT_EQ(7, GreatestCommonDivisor({49, 14, 21}));
}

TEST(NumberUtilTest, IsPowerOfTwoOrZero) {
  EXPECT_TRUE(IsPowerOfTwoOrZero(0));
  for (int power = 0; power < 15; ++power) {
    const int value = 1 << power;
    EXPECT_TRUE(IsPowerOfTwoOrZero(value)) << "with value = 2^" << power;
  }
  for (int value = 17; value < 32; ++value) {
    EXPECT_FALSE(IsPowerOfTwoOrZero(value)) << "with value = " << value;
  }
}

TEST(NumberUtilTest, Log2) {
  for (int value = 1; value < 300; ++value) {
    SCOPED_TRACE(absl::StrFormat("value:%d", value));
    EXPECT_EQ(std::floor(std::log2(value)), Log2Floor(value));
    EXPECT_EQ(std::ceil(std::log2(value)), Log2Ceiling(value));
  }
}

TEST(NumberUtilTest, NextPowerOfTwo) {
  for (int value = 1; value < 300; ++value) {
    SCOPED_TRACE(absl::StrFormat("value:%d", value));
    int result = NextPowerOfTwo(value);
    EXPECT_TRUE(IsPowerOfTwoOrZero(result));
    EXPECT_GE(result, value);
    EXPECT_LT(result / 2, value);
  }
}

TEST(NumberUtilTest, ArithmeticSequenceBasicOps) {
  const vector<double> kExpected({11.7, 11.8, 11.9, 12.0});
  ArithmeticSequence seq(11.7, 0.1, 12.0);
  EXPECT_DOUBLE_EQ(seq.base(), 11.7);
  EXPECT_DOUBLE_EQ(seq.step(), 0.1);
  EXPECT_DOUBLE_EQ(seq.limit(), 12.0);
  EXPECT_FALSE(seq.empty());
  ASSERT_EQ(seq.size(), kExpected.size());

  // Check element values obtained by operator[].
  EXPECT_DOUBLE_EQ(seq[0], kExpected[0]);
  EXPECT_DOUBLE_EQ(seq[1], kExpected[1]);
  EXPECT_DOUBLE_EQ(seq[2], kExpected[2]);
  EXPECT_DOUBLE_EQ(seq[3], kExpected[3]);

  // Check element values obtained by iteration and CopyTo.
  EXPECT_THAT(vector<double>(seq.begin(), seq.end()),
              FloatArrayNear(kExpected, 1e-12));
  vector<double> values;
  seq.CopyTo(&values);
  EXPECT_THAT(values, FloatArrayNear(kExpected, 1e-12));

  // Test iterator operations.
  EXPECT_DOUBLE_EQ(*(1 + seq.begin()), kExpected[1]);
  EXPECT_DOUBLE_EQ(*(seq.end() - 2), kExpected[2]);
  EXPECT_DOUBLE_EQ((seq.begin() + 1)[2], kExpected[3]);
  EXPECT_EQ(seq.end() - seq.begin(), kExpected.size());
}

struct ULPs {
  explicit ULPs(int num_in): num(num_in) {}
  int num;
};
double operator+(double x, ULPs ulps) {
  const double target = (ulps.num > 0 ? 1 : -1) *
      std::numeric_limits<double>::infinity();
  for (int i = 0; i < std::abs(ulps.num); ++i) {
    x = std::nextafter(x, target);
  }
  return x;
}
double operator-(double x, ULPs ulps) {
  return x + ULPs(-ulps.num);
}

void CheckArithmeticSequenceWithPerturbedArgs(
    const std::vector<double>& args, int size) {
  SCOPED_TRACE(absl::StrCat("Args: (", absl::StrJoin(args, ", "), ")"));
  const double base = args[0];
  const double step = args[1];
  const double limit = args[2];
  // Check first without adding perturbations.
  ASSERT_THAT(ArithmeticSequence(base, step, limit), SizeIs(size));

  // Small perturbations to the args should not change the computed size.
  ASSERT_THAT(ArithmeticSequence(base + ULPs(3), step, limit), SizeIs(size));
  ASSERT_THAT(ArithmeticSequence(base - ULPs(3), step, limit), SizeIs(size));
  ASSERT_THAT(ArithmeticSequence(base, step + ULPs(3), limit), SizeIs(size));
  ASSERT_THAT(ArithmeticSequence(base, step - ULPs(3), limit), SizeIs(size));
  ASSERT_THAT(ArithmeticSequence(base, step, limit + ULPs(3)), SizeIs(size));
  ASSERT_THAT(ArithmeticSequence(base, step, limit - ULPs(3)), SizeIs(size));
  ASSERT_THAT(ArithmeticSequence(
      base - ULPs(3), step - ULPs(3), limit + ULPs(3)), SizeIs(size));
  ASSERT_THAT(ArithmeticSequence(
      base + ULPs(3), step + ULPs(3), limit - ULPs(3)), SizeIs(size));

  // But the size should differ with a large perturbation.
  const double large_perturbation = (step > 0 ? 1 : -1) *
      50 * std::numeric_limits<double>::epsilon() *
      std::max(std::abs(base), std::abs(limit));
  ASSERT_THAT(ArithmeticSequence(
      base + large_perturbation, step, limit), SizeIs(size - 1));
  ASSERT_THAT(ArithmeticSequence(
      base, step, limit - large_perturbation), SizeIs(size - 1));
}

// Check that size is correctly computed for various arguments.
TEST(NumberUtilTest, ArithmeticSequenceHasCorrectSize) {
  EXPECT_THAT(ArithmeticSequence(2.1, 1.0, 2.1), SizeIs(1));
  EXPECT_THAT(ArithmeticSequence(2.1, 1.0, 3.1 - 1e-8), SizeIs(1));
  EXPECT_THAT(ArithmeticSequence(2.1, 1.0, 1.0), IsEmpty());
  const double epsilon = std::numeric_limits<double>::epsilon();
  EXPECT_THAT(ArithmeticSequence(1.0, epsilon, 1.0 + 3 * epsilon), SizeIs(4));

  CheckArithmeticSequenceWithPerturbedArgs({10, 1, 20}, 11);
  CheckArithmeticSequenceWithPerturbedArgs({1.0, 0.1, 2.0}, 11);
  CheckArithmeticSequenceWithPerturbedArgs({3.1, 0.1, 3.3}, 3);
  CheckArithmeticSequenceWithPerturbedArgs({3.3, -0.1, 3.1}, 3);
  CheckArithmeticSequenceWithPerturbedArgs({-3.1, -0.1, -3.3}, 3);
  CheckArithmeticSequenceWithPerturbedArgs({3.1, 0.01, 3.3}, 21);
  CheckArithmeticSequenceWithPerturbedArgs({3.1, 0.001, 3.3}, 201);
  CheckArithmeticSequenceWithPerturbedArgs({1.8, 0.05, 1.9}, 3);
  CheckArithmeticSequenceWithPerturbedArgs({-5.6, -0.7, -9.1}, 6);

  std::mt19937 rng(0 /* seed */);
  std::normal_distribution<double> endpoint_dist(0.0, 1e3);
  std::uniform_int_distribution<int> size_dist(100, 10000);
  for (int trial = 0; trial < 100; ++trial) {
    const int desired_size = size_dist(rng);
    const double base = endpoint_dist(rng);
    const double limit = endpoint_dist(rng);
    const double step = (limit - base) / (desired_size - 1);
    CheckArithmeticSequenceWithPerturbedArgs({base, step, limit}, desired_size);
  }
}

TEST(NumberUtilTest, CombinationsIterator) {
  vector<vector<int>> combinations;
  for (CombinationsIterator it(5, 3); !it.Done(); it.Next()) {
    combinations.push_back(it.GetCurrentCombination());
  }
  EXPECT_THAT(combinations, ElementsAreArray(
      vector<vector<int>>({{0, 1, 2},
                           {0, 1, 3},
                           {0, 1, 4},
                           {0, 2, 3},
                           {0, 2, 4},
                           {0, 3, 4},
                           {1, 2, 3},
                           {1, 2, 4},
                           {1, 3, 4},
                           {2, 3, 4}})));

  for (int n = 1; n <= 7; ++n) {
    for (int k = 1; k <= n; ++k) {
      SCOPED_TRACE(absl::StrFormat("n:%d, k:%d", n, k));
      vector<vector<int>> combinations;
      for (CombinationsIterator it(n, k); !it.Done(); it.Next()) {
        const vector<int>& combination = it.GetCurrentCombination();
        EXPECT_EQ(combination.size(), k);
        EXPECT_THAT(combination, Each(Ge(0)));
        EXPECT_THAT(combination, Each(Lt(n)));
        if (!combinations.empty()) {
          EXPECT_TRUE(std::lexicographical_compare(
              combinations.back().cbegin(), combinations.back().cend(),
              combination.cbegin(), combination.cend()));
        }
        combinations.push_back(combination);
      }
      EXPECT_EQ(combinations.size(), BinomialCoefficient(n, k));
    }
  }
}

TEST(NumberUtilTest, BuildCombinationsTable) {
  vector<vector<std::string>> combinations =
      BuildCombinationsTable(vector<std::string>({"A", "B", "C", "D", "E"}), 3);
  vector<std::string> joined_combinations;
  for (const auto& combination : combinations) {
    joined_combinations.push_back(absl::StrJoin(combination, ""));
  }
  EXPECT_THAT(joined_combinations, ElementsAreArray(
      {"ABC", "ABD", "ABE", "ACD", "ACE", "ADE", "BCD", "BCE", "BDE", "CDE"}));
}

TEST(NumberUtilTest, CrossProductRange) {
  {
    vector<vector<int>> iterates;
    for (const vector<int>& i : CrossProductRange({2, 1, 3})) {
      iterates.push_back(i);
    }
    EXPECT_THAT(iterates, ElementsAreArray(vector<vector<int>>({
        {0, 0, 0}, {1, 0, 0},
        {0, 0, 1}, {1, 0, 1},
        {0, 0, 2}, {1, 0, 2}})));

    CrossProductRange range({2, 1, 3});
    EXPECT_FALSE(range.empty());
    EXPECT_THAT(*range.FlatIndex(2), ElementsAre(0, 0, 1));
    EXPECT_THAT(*range.FlatIndex(5), ElementsAre(1, 0, 2));
  }
  {
    constexpr int kNumDims = 20;
    CrossProductRange range(vector<int>(kNumDims, 1));
    vector<vector<int>> iterates;
    for (const vector<int>& i : range) {
      iterates.push_back(i);
    }
    EXPECT_THAT(iterates, ElementsAre(vector<int>(kNumDims, 0)));
    EXPECT_FALSE(range.empty());
  }
  {
    CrossProductRange range({2, 0, 3});
    vector<vector<int>> iterates;
    for (const vector<int>& i : range) {
      iterates.push_back(i);
    }
    EXPECT_THAT(iterates, IsEmpty());
    EXPECT_TRUE(range.empty());
  }
  {
    CrossProductRange range({});
    vector<vector<int>> iterates;
    for (const vector<int>& i : range) {
      iterates.push_back(i);
    }
    EXPECT_THAT(iterates, IsEmpty());
    EXPECT_TRUE(range.empty());
  }
}

}  // namespace
}  // namespace audio_dsp
