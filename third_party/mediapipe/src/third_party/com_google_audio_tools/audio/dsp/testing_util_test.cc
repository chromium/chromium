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

#include "audio/dsp/testing_util.h"

#include <cmath>
#include <complex>
#include <deque>
#include <initializer_list>
#include <list>

#include "gmock/gmock.h"
#include "absl/types/span.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {
namespace {

using ::Eigen::ColMajor;
using ::Eigen::Dynamic;
using ::Eigen::RowMajor;
using ::absl::Span;
using ::std::complex;
using ::testing::DoubleEq;
using ::testing::FloatEq;
using ::testing::Matcher;
using ::testing::Not;

TEST(FloatArrayNearTest, TypicalUse) {
  std::vector<double> test_vector({0.998, -1.414, 3.142});
  std::vector<double> reference_vector({1.0, -M_SQRT2, M_PI});
  EXPECT_THAT(test_vector, FloatArrayNear(reference_vector, 1e-2));
  EXPECT_THAT(test_vector, Not(FloatArrayNear(reference_vector, 1e-4)));
}

template <typename ContainerType>
class FloatArrayNearContainerTypeTest : public ::testing::Test {};

typedef ::testing::Types<std::vector<float>, std::deque<float>,
                         std::list<float>, Span<const float>>
    TestContainerTypes;
TYPED_TEST_SUITE(FloatArrayNearContainerTypeTest, TestContainerTypes);

TYPED_TEST(FloatArrayNearContainerTypeTest, MatchesApproximately) {
  typedef TypeParam ContainerType;
  auto test_values = {0.505f, 1.0f, -0.992f, 1.995f};
  ContainerType test_container(test_values);
  auto reference_values = {0.5f, 1.0f, -1.0f, 2.0f};
  ContainerType reference_container(reference_values);

  const Matcher<ContainerType> m1 = FloatArrayNear(reference_container, 1e-2);
  EXPECT_TRUE(m1.Matches(test_container));
  const Matcher<ContainerType> m2 = FloatArrayNear(reference_container, 1e-3);
  EXPECT_FALSE(m2.Matches(test_container));
}

TYPED_TEST(FloatArrayNearContainerTypeTest, DoesNotMatchWrongSize) {
  typedef TypeParam ContainerType;
  EXPECT_THAT(ContainerType({1.0f, 2.0f}),
              Not(FloatArrayNear(ContainerType({1.0f, 2.0f, 3.0f}), 1e-2)));
}

TYPED_TEST(FloatArrayNearContainerTypeTest, DoesNotMatchWrongOrder) {
  typedef TypeParam ContainerType;
  EXPECT_THAT(ContainerType({1.0f, 3.0f, 2.0f}),
              Not(FloatArrayNear(ContainerType({1.0f, 2.0f, 3.0f}), 1e-2)));
}

TYPED_TEST(FloatArrayNearContainerTypeTest, DoesNotMatchNaNs) {
  typedef TypeParam ContainerType;
  auto test_values = {1.0f, MathLimits<float>::kNaN};
  ContainerType test_container(test_values);

  EXPECT_THAT(test_container,
              Not(FloatArrayNear(ContainerType({1.0f, 2.0f}), 1e0)));
  EXPECT_THAT(test_container,
              Not(FloatArrayNear(test_container, 1e0)));
}

TEST(FloatArrayNearTest, WithComplexElements) {
  std::vector<complex<float>> test_vector({{5, 0}, {6, 0}, {6, 1}});
  std::vector<complex<float>> reference_vector({{5, 0}, {5, 0}, {5, 0}});

  const Matcher<std::vector<complex<float>>> m1 =
      FloatArrayNear(reference_vector, 1.5);
  EXPECT_TRUE(m1.Matches(test_vector));
  const Matcher<std::vector<complex<float>>> m2 =
      FloatArrayNear(reference_vector, 1.0);
  EXPECT_FALSE(m2.Matches(test_vector));
}

TEST(FloatArrayNearTest, WithIntegerElements) {
  std::vector<int> test_vector({505, 1000, -992, 1990});
  std::vector<int> reference_vector({500, 1000, -1000, 2000});

  const Matcher<std::vector<int>> m1 = FloatArrayNear(reference_vector, 10);
  EXPECT_TRUE(m1.Matches(test_vector));
  const Matcher<std::vector<int>> m2 = FloatArrayNear(reference_vector, 1);
  EXPECT_FALSE(m2.Matches(test_vector));
}

TEST(FloatArrayEqTest, TypicalUse) {
  std::vector<float> reference_vector({1e6, -M_SQRT2, M_PI});
  // Values are within 4 ULPs.
  std::vector<float> test_vector({1e6 + 0.25, -1.41421323, 3.14159262});
  EXPECT_THAT(test_vector, FloatArrayEq(reference_vector));
  // Create a difference of 5 ULPs in the first element.
  test_vector[0] = 1e6 + 0.3125;
  EXPECT_THAT(test_vector, Not(FloatArrayEq(reference_vector)));
}

template <typename ContainerType>
class FloatArrayEqContainerTypeTest : public ::testing::Test {};

TYPED_TEST_SUITE(FloatArrayEqContainerTypeTest, TestContainerTypes);

TYPED_TEST(FloatArrayEqContainerTypeTest, MatchesApproximately) {
  typedef TypeParam ContainerType;
  auto reference_values = {-1e6f, 0.0f, 1.0f};
  ContainerType reference_container(reference_values);
  const Matcher<ContainerType> m = FloatArrayEq(reference_container);
  EXPECT_TRUE(m.Matches(reference_container));
  EXPECT_TRUE(m.Matches(ContainerType({-1e6 + 0.25, 5e-45, 1.0000002})));
  EXPECT_TRUE(m.Matches(ContainerType({-1e6 - 0.25, -5e-45, 0.9999998})));
  EXPECT_FALSE(m.Matches(ContainerType({-1e6 + 0.3125, 0.0, 1.0})));
  EXPECT_FALSE(m.Matches(ContainerType({-1e6, 1e-44, 1.0})));
  EXPECT_FALSE(m.Matches(ContainerType({-1e6, 0.0, 1.0000006})));
}

TYPED_TEST(FloatArrayEqContainerTypeTest, DoesNotMatchWrongSize) {
  typedef TypeParam ContainerType;
  EXPECT_THAT(ContainerType({1.0f, 2.0f}),
              Not(FloatArrayEq(ContainerType({1.0f, 2.0f, 3.0f}))));
}

TYPED_TEST(FloatArrayEqContainerTypeTest, DoesNotMatchWrongOrder) {
  typedef TypeParam ContainerType;
  EXPECT_THAT(ContainerType({1.0f, 3.0f, 2.0f}),
              Not(FloatArrayEq(ContainerType({1.0f, 2.0f, 3.0f}))));
}

TYPED_TEST(FloatArrayEqContainerTypeTest, DoesNotMatchNaNs) {
  typedef TypeParam ContainerType;
  auto reference_values = {1.0f, MathLimits<float>::kNaN};
  ContainerType reference_container(reference_values);
  const Matcher<ContainerType> m = FloatArrayEq(reference_container);
  EXPECT_FALSE(m.Matches(reference_container));
  EXPECT_FALSE(m.Matches(ContainerType({1.0f, 2.0f})));
}

TYPED_TEST(FloatArrayEqContainerTypeTest, HandlesInfinities) {
  typedef TypeParam ContainerType;
  auto reference_values =
      {1.0f, MathLimits<float>::kPosInf, MathLimits<float>::kNegInf};
  ContainerType reference_container(reference_values);
  const Matcher<ContainerType> m = FloatArrayEq(reference_container);
  EXPECT_TRUE(m.Matches(reference_container));
  EXPECT_FALSE(m.Matches(ContainerType({1.0f, 2.0f, 3.0f})));
}

TEST(FloatArrayEqContainerTypeTest, WithComplexElements) {
  const std::vector<complex<float>> reference_vector(
      {{1e6, -1}, {M_SQRT2, M_PI}});
  const Matcher<std::vector<complex<float>>> m = FloatArrayEq(reference_vector);
  EXPECT_TRUE(m.Matches(reference_vector));
  EXPECT_TRUE(m.Matches(std::vector<complex<float>>(
      {{1e6 + 0.25, -1.0000002}, {1.41421323, 3.14159266}})));
  EXPECT_FALSE(m.Matches(std::vector<complex<float>>(
      {{1e6 + 0.3125, -1}, {M_SQRT2, M_PI}})));
  EXPECT_FALSE(m.Matches(std::vector<complex<float>>(
      {{1e6, -1.0000006}, {M_SQRT2, M_PI}})));
}

static const double kEps = 1e-6;

TEST(EigenArrayNearTest, ArrayXd) {
  const Eigen::ArrayXd expected = Eigen::ArrayXd::Random(4);
  Eigen::ArrayXd actual = expected;
  EXPECT_THAT(actual, EigenArrayNear(expected, kEps));
  EXPECT_THAT(actual, EigenArrayNear(expected, 1e-100));

  actual += 100;
  EXPECT_THAT(actual, Not(EigenArrayNear(expected, kEps)));
  // Wrong shape.
  actual.resize(2);
  EXPECT_THAT(actual, Not(EigenArrayNear(expected, kEps)));
}

TEST(EigenArrayNearTest, ArrayXdInlinedValues) {
  Eigen::ArrayXd actual(3);
  actual << 1.0, 2.0, 3.0;
  EXPECT_THAT(actual, EigenArrayNear<double>({1.0, 2.0, 3.0}, kEps));
  EXPECT_THAT(actual,
              EigenArrayNear<double>({1.0, 2.0 + 0.5 * kEps, 3.0}, kEps));

  EXPECT_THAT(actual, Not(EigenArrayNear<double>({1.0, 2.0, 5.0}, kEps)));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayNear<double>({1.0, 2.0}, kEps)));
}

TEST(EigenArrayNearTest, EmptyArrayX) {
  const Eigen::ArrayXi empty;
  EXPECT_THAT(empty, EigenArrayNear(empty, kEps));
  // Can pass in an Eigen expression type.
  EXPECT_THAT(empty, EigenArrayNear(Eigen::ArrayXi(), kEps));

  EXPECT_THAT(empty, Not(EigenArrayNear<int>({1, 2}, kEps)));
  EXPECT_THAT(empty, Not(EigenArrayNear(Eigen::ArrayXi::Zero(3), kEps)));
}

TEST(EigenArrayNearTest, ArrayXXf) {
  const Eigen::ArrayXXf expected = Eigen::ArrayXXf::Random(4, 5);
  Eigen::ArrayXXf actual = expected;
  EXPECT_THAT(actual, EigenArrayNear(expected, kEps));
  EXPECT_THAT(actual, EigenArrayNear(expected, 1e-100));

  actual.row(2) += 100;
  EXPECT_THAT(actual, Not(EigenArrayNear(expected, kEps)));
  // Wrong shape.
  EXPECT_THAT(expected, Not(EigenArrayNear(expected.transpose(), kEps)));
  actual.resize(4, 3);
  EXPECT_THAT(actual, Not(EigenArrayNear(expected, kEps)));

  // Expression type.
  actual.resize(3, 2);
  actual.col(0) << 1.0, 2.0, 3.0;
  actual.col(1) << 4.0, 5.0, 6.0;
  std::vector<float> expected_vector({1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  EXPECT_THAT(actual, EigenArrayNear(Eigen::Map<Eigen::ArrayXXf>(
                                         &expected_vector[0], 3 /* rows */,
                                         2 /* columns */),
                                     kEps));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayNear(Eigen::Map<Eigen::ArrayXXf>(
                                             &expected_vector[0], 3 /* rows */,
                                             1 /* columns */),
                                         kEps)));
}

TEST(EigenArrayNearTest, DifferentMajor) {
  Eigen::Array<float, Dynamic, Dynamic, ColMajor> col_major(2, 3);
  col_major << 1.0, 2.0, 3.0,
               4.0, 5.0, 6.0;
  Eigen::Array<float, Dynamic, Dynamic, RowMajor> row_major(2, 3);
  row_major << 1.0, 2.0, 3.0,
               4.0, 5.0, 6.0;
  ABSL_CHECK_EQ(col_major(1, 0), row_major(1, 0));

  EXPECT_THAT(row_major, EigenArrayNear(col_major, 0.0));
  EXPECT_THAT(row_major, EigenArrayNear<float>({{1.0, 2.0, 3.0},
                                                {4.0, 5.0, 6.0}}, 0.0));
  EXPECT_THAT(col_major, EigenArrayNear(row_major, 0.0));
  EXPECT_THAT(col_major, EigenArrayNear<float>({{1.0, 2.0, 3.0},
                                                {4.0, 5.0, 6.0}}, 0.0));
}

TEST(EigenArrayNearTest, ArrayXXfInlinedValues) {
  Eigen::ArrayXXf actual(2, 3);
  actual.row(0) << 1.0, 2.0, 3.0;
  actual.row(1) << 4.0, -5.0, -6.0;

  EXPECT_THAT(actual, EigenArrayNear<float>({{1.0, 2.0, 3.0},  //
                                             {4.0, -5.0, -6.0}},
                                            kEps));
  EXPECT_THAT(actual, EigenArrayNear<float>(
                          {{1.0, 2.0, 3.0},
                           {4.0, -5.0, static_cast<float>(-6.0 - 0.9 * kEps)}},
                          kEps));
  EXPECT_THAT(actual, Not(EigenArrayNear<float>({{1.0, 2.0, 3.0},  //
                                                 {4.0, -5.0, -8.0}},
                                                kEps)));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayNear<float>({{1.0, 2.0, 3.0}}, kEps)));
}

TEST(EigenArrayEqTest, ArrayXd) {
  const Eigen::ArrayXd expected = Eigen::ArrayXd::Random(4);
  Eigen::ArrayXd actual = expected;
  EXPECT_THAT(actual, EigenArrayEq(expected));

  actual += 100;
  EXPECT_THAT(actual, Not(EigenArrayEq(expected)));
  // Wrong shape.
  actual.resize(2);
  EXPECT_THAT(actual, Not(EigenArrayEq(expected)));
}

TEST(EigenArrayEqTest, ArrayXdInlinedValues) {
  Eigen::ArrayXd actual(3);
  actual << 1.0, 2.0, 3.0;
  EXPECT_THAT(actual, EigenArrayEq<double>({1.0, 2.0, 3.0}));
  EXPECT_THAT(actual, EigenArrayEq<double>({1.0, 2.0 + 5e-7, 3.0}));

  EXPECT_THAT(actual, Not(EigenArrayEq<double>({1.0, 2.0, 5.0})));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayEq<double>({1.0, 2.0})));
}

TEST(EigenArrayEqTest, EmptyArrayX) {
  const Eigen::ArrayXi empty;
  EXPECT_THAT(empty, EigenArrayEq(empty));
  // Can pass in an Eigen expression type.
  EXPECT_THAT(empty, EigenArrayEq(Eigen::ArrayXi()));

  EXPECT_THAT(empty, Not(EigenArrayEq<int>({1, 2})));
  EXPECT_THAT(empty, Not(EigenArrayEq(Eigen::ArrayXi::Zero(3))));
}

TEST(EigenArrayEqTest, ArrayXXf) {
  const Eigen::ArrayXXf expected = Eigen::ArrayXXf::Random(4, 5);
  Eigen::ArrayXXf actual = expected;
  EXPECT_THAT(actual, EigenArrayEq(expected));

  actual.row(2) += 100;
  EXPECT_THAT(actual, Not(EigenArrayEq(expected)));
  // Wrong shape.
  EXPECT_THAT(expected, Not(EigenArrayEq(expected.transpose())));
  actual.resize(4, 3);
  EXPECT_THAT(actual, Not(EigenArrayEq(expected)));

  // Expression type.
  actual.resize(3, 2);
  actual.col(0) << 1.0, 2.0, 3.0;
  actual.col(1) << 4.0, 5.0, 6.0;
  std::vector<float> expected_vector({1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  EXPECT_THAT(actual, EigenArrayEq(Eigen::Map<Eigen::ArrayXXf>(
                                         &expected_vector[0], 3 /* rows */,
                                         2 /* columns */)));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayEq(Eigen::Map<Eigen::ArrayXXf>(
                                             &expected_vector[0], 3 /* rows */,
                                             1 /* columns */))));
}

TEST(EigenArrayEqTest, ArrayXXfInlinedValues) {
  Eigen::ArrayXXf actual(2, 3);
  actual.row(0) << 1.0, 2.0, 3.0;
  actual.row(1) << 4.0, -5.0, -6.0;

  EXPECT_THAT(actual, EigenArrayEq<float>({{1.0, 2.0, 3.0},
                                           {4.0, -5.0, -6.0}}));
  EXPECT_THAT(actual, EigenArrayEq<float>({{1.0, 2.0, 3.0},
                                           {4.0, -5.0, -6.0 - 1e-6}}));
  EXPECT_THAT(actual, Not(EigenArrayEq<float>({{1.0, 2.0, 3.0},
                                               {4.0, -5.0, -8.0}})));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayEq<float>({{1.0, 2.0, 3.0}})));
}

TEST(EigenEach, ArrayX) {
  {  // EigenEach matches empty array.
    EXPECT_THAT(Eigen::ArrayXf(), EigenEach(FloatEq(-1234)));
  }
  {  // With ArrayXf as the LHS type.
    Eigen::ArrayXf actual = Eigen::ArrayXf::Constant(4, 33.3);
    EXPECT_THAT(actual, EigenEach(FloatEq(33.3)));
    actual[2] += 0.1;
    EXPECT_THAT(actual, Not(EigenEach(FloatEq(33.3))));
  }
  {  // With complex-valued ArrayXcf, and using ComplexFloatEq.
    Eigen::ArrayXcf actual = Eigen::ArrayXcf::Constant(4, {-8.1, 1.6});
    EXPECT_THAT(actual, EigenEach(ComplexFloatEq(
        complex<float>(-8.1, 1.6))));
    actual[2] += 0.1;
    EXPECT_THAT(actual, Not(EigenEach(ComplexFloatEq(
        complex<float>(-8.1, 1.6)))));
  }
  {  // With ArrayXd.
    Eigen::ArrayXd actual = Eigen::ArrayXd::Constant(4, 33.3);
    EXPECT_THAT(actual, EigenEach(DoubleEq(33.3)));
    actual[2] += 0.1;
    EXPECT_THAT(actual, Not(EigenEach(DoubleEq(33.3))));
  }
  {  // With ArrayXcd.
    Eigen::ArrayXcd actual = Eigen::ArrayXcd::Constant(4, {-8.1, 1.6});
    EXPECT_THAT(actual, EigenEach(ComplexDoubleEq(
        complex<double>(-8.1, 1.6))));
    actual[2] += 0.1;
    EXPECT_THAT(actual, Not(EigenEach(ComplexDoubleEq(
        complex<double>(-8.1, 1.6)))));
  }
}

TEST(EigenEach, ArrayXX) {
  {
    Eigen::ArrayXXf actual = Eigen::ArrayXXf::Constant(2, 3, 42.0);
    EXPECT_THAT(actual, EigenEach(testing::FloatEq(42.0)));
    actual(1, 2) += 0.1;
    EXPECT_THAT(actual, Not(EigenEach(testing::FloatEq(42.0))));
  }
  {
    Eigen::ArrayXXcf actual = Eigen::ArrayXXcf::Constant(2, 3, {42.0, -5.7});
    EXPECT_THAT(actual, EigenEach(ComplexFloatEq(
        complex<float>(42.0, -5.7))));
    actual(1, 2) += 0.1;
    EXPECT_THAT(actual, Not(EigenEach(ComplexFloatEq(
        complex<float>(42.0, -5.7)))));
  }
}

}  // namespace
}  // namespace audio_dsp
