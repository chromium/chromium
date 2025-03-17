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

#include "audio/linear_filters/filter_poles_and_zeros.h"

#include <cmath>

#include "audio/dsp/testing_util.h"
#include "audio/linear_filters/biquad_filter_coefficients.h"
#include "gtest/gtest.h"

using audio_dsp::FloatArrayEq;
using std::complex;

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {
namespace {

// All of the tested filters are digital (the argument to Eval()
// is in terms of z).
TEST(FilterTypesTest, MakeCoeffsTest) {
  std::vector<double> identity_coeffs = {1.0, 0.0, 0.0};
  {  // No poles and no zeros produces single stage, identity filter.
    FilterPolesAndZeros zpk;
    BiquadFilterCascadeCoefficients coeffs = zpk.GetCoefficients();
    EXPECT_EQ(coeffs.size(), 1);
    EXPECT_THAT(coeffs[0].b, FloatArrayEq(identity_coeffs));
    EXPECT_THAT(coeffs[0].a, FloatArrayEq(identity_coeffs));
    EXPECT_NEAR(std::abs(zpk.Eval(1)), 1.0, 1e-7);
    EXPECT_NEAR(std::abs(zpk.Eval({0, 1})), 1.0, 1e-7);  // Eval at pi/2.
    EXPECT_NEAR(std::abs(zpk.Eval(-1)), 1.0, 1e-7);
  }
  {  // A single zero.
    FilterPolesAndZeros zpk;
    zpk.AddZero(1.0);
    BiquadFilterCascadeCoefficients coeffs = zpk.GetCoefficients();
    EXPECT_EQ(coeffs.size(), 1);
    EXPECT_EQ(zpk.RelativeDegree(), -1);
    std::vector<double> single_root = {1.0, -1.0, 0.0};
    EXPECT_THAT(coeffs[0].b, FloatArrayEq(single_root));
    EXPECT_THAT(coeffs[0].a, FloatArrayEq(identity_coeffs));
    EXPECT_NEAR(std::abs(zpk.Eval(1)), 0.0, 1e-7);
    EXPECT_NEAR(std::abs(zpk.Eval({0, 1})), M_SQRT2, 1e-7);
    EXPECT_NEAR(std::abs(zpk.Eval(-1)), 2.0, 1e-7);
  }
  {  // A single pole and zero.
    FilterPolesAndZeros zpk;
    zpk.AddZero(1.0);
    zpk.AddPole(0.5);
    BiquadFilterCascadeCoefficients coeffs = zpk.GetCoefficients();
    EXPECT_EQ(coeffs.size(), 1);
    EXPECT_EQ(zpk.RelativeDegree(), 0);
    std::vector<double> single_zero = {1.0, -1.0, 0.0};
    std::vector<double> single_pole = {1.0, -0.5, 0.0};
    EXPECT_THAT(coeffs[0].b, FloatArrayEq(single_zero));
    EXPECT_THAT(coeffs[0].a, FloatArrayEq(single_pole));
    EXPECT_NEAR(std::abs(zpk.Eval(1)), 0.0, 1e-7);
    EXPECT_NEAR(std::abs(zpk.Eval({0, 1})), 1.2649, 1e-4);
    EXPECT_NEAR(std::abs(zpk.Eval(-1)), 4 / 3.0, 1e-7);
  }
  {  // Two real poles and real zeros.
    FilterPolesAndZeros zpk;
    zpk.AddZero(1.0);
    zpk.AddZero(2.0);
    zpk.AddPole(0.5);
    zpk.AddPole(-0.5);
    zpk.SetGain(2);
    BiquadFilterCascadeCoefficients coeffs = zpk.GetCoefficients();
    EXPECT_EQ(coeffs.size(), 1);
    EXPECT_EQ(zpk.RelativeDegree(), 0);
    std::vector<double> b = {2.0, -6.0, 4.0};
    std::vector<double> a = {1.0, 0.0, -0.25};
    EXPECT_THAT(coeffs[0].b, FloatArrayEq(b));
    EXPECT_THAT(coeffs[0].a, FloatArrayEq(a));
  }
  {  // Two complex poles and two complex zeros.
    FilterPolesAndZeros zpk;
    zpk.AddConjugateZeroPair({1.0, 1.0});
    zpk.AddConjugatePolePair({0.5, 0.1});
    BiquadFilterCascadeCoefficients coeffs = zpk.GetCoefficients();
    EXPECT_EQ(coeffs.size(), 1);
    EXPECT_EQ(zpk.RelativeDegree(), 0);
    std::vector<double> b = {1.0, -2.0, 2.0};
    std::vector<double> a = {1.0, -1.0, 0.26};
    EXPECT_THAT(coeffs[0].b, FloatArrayEq(b));
    EXPECT_THAT(coeffs[0].a, FloatArrayEq(a));
    EXPECT_NEAR(std::abs(zpk.Eval(1)), 3.8461, 1e-4);
    EXPECT_NEAR(std::abs(zpk.Eval({0, 1})), 1.7974, 1e-4);
    EXPECT_NEAR(std::abs(zpk.Eval(-1)), 2.2124, 1e-4);
  }
  {  // Two complex poles, two real poles, and two complex zeros.
    FilterPolesAndZeros zpk;
    zpk.AddConjugateZeroPair({1.0, 1.0});
    zpk.AddConjugatePolePair({0.5, 0.5});
    zpk.AddPole(-0.5);
    zpk.AddPole(-0.7);
    BiquadFilterCascadeCoefficients coeffs = zpk.GetCoefficients();
    EXPECT_EQ(coeffs.size(), 2);
    EXPECT_EQ(zpk.RelativeDegree(), 2);
    // Stage 1. It groups the conjugate pairs even though they are not
    // adjacent in the vector.
    std::vector<double> b = {1.0, -2.0, 2.0};
    std::vector<double> a_stage1 = {1.0, -1.0, 0.5};
    EXPECT_THAT(coeffs[0].b, FloatArrayEq(b));
    EXPECT_THAT(coeffs[0].a, FloatArrayEq(a_stage1));
    // Stage 2.
    std::vector<double> a_stage2 = {1.0, 1.2, 0.35};
    EXPECT_THAT(coeffs[1].b, FloatArrayEq(identity_coeffs));
    EXPECT_THAT(coeffs[1].a, FloatArrayEq(a_stage2));
  }
  {  // Three pairs of complex conjugate zeros, one pair of conjugate poles.
    FilterPolesAndZeros zpk;
    zpk.AddConjugateZeroPair({0.5, 0.5});
    zpk.AddConjugateZeroPair({0.3, 0.2});
    zpk.AddConjugateZeroPair({0.9, 0.1});
    zpk.AddConjugatePolePair({-0.3, 0.3});
    BiquadFilterCascadeCoefficients coeffs = zpk.GetCoefficients();
    EXPECT_EQ(coeffs.size(), 3);
    EXPECT_EQ(zpk.RelativeDegree(), -4);
    // Stage 1.
    std::vector<double> b_stage1 = {1.0, -1.0, 0.5};
    std::vector<double> a_stage1 = {1.0, 0.6, 0.18};
    EXPECT_THAT(coeffs[0].b, FloatArrayEq(b_stage1));
    EXPECT_THAT(coeffs[0].a, FloatArrayEq(a_stage1));
    // Stage 2.
    std::vector<double> b_stage2 = {1.0, -0.6, 0.13};
    EXPECT_THAT(coeffs[1].b, FloatArrayEq(b_stage2));
    EXPECT_THAT(coeffs[1].a, FloatArrayEq(identity_coeffs));
    // Stage 3.
    std::vector<double> b_stage3 = {1.0, -1.8, 0.82};
    EXPECT_THAT(coeffs[2].b, FloatArrayEq(b_stage3));
    EXPECT_THAT(coeffs[2].a, FloatArrayEq(identity_coeffs));
  }
}

TEST(FilterTypesTest, MakeRealCoeffsTest) {
  std::vector<double> identity_coeffs = {1.0, 0.0, 0.0};
  {  // Three pairs of complex conjugate zeros, one pair of conjugate poles.
    FilterPolesAndZeros zpk;
    zpk.AddConjugateZeroPair({0.5, 0.5});
    zpk.AddConjugateZeroPair({0.3, 0.2});
    zpk.AddConjugateZeroPair({0.9, 0.1});
    zpk.AddConjugatePolePair({-0.3, 0.3});
    BiquadFilterCascadeCoefficients coeffs = zpk.GetCoefficients();
    // Stage 1.
    std::vector<complex<double>> b_stage1 = {1.0, -1.0, 0.5};
    std::vector<complex<double>> a_stage1 = {1.0, 0.6, 0.18};
    EXPECT_THAT(coeffs[0].b, FloatArrayEq(b_stage1));
    EXPECT_THAT(coeffs[0].a, FloatArrayEq(a_stage1));
    // Stage 2.
    std::vector<complex<double>> b_stage2 = {1.0, -0.6, 0.13};
    EXPECT_THAT(coeffs[1].b, FloatArrayEq(b_stage2));
    EXPECT_THAT(coeffs[1].a, FloatArrayEq(identity_coeffs));
    // Stage 3.
    std::vector<complex<double>> b_stage3 = {1.0, -1.8, 0.82};
    EXPECT_THAT(coeffs[2].b, FloatArrayEq(b_stage3));
    EXPECT_THAT(coeffs[2].a, FloatArrayEq(identity_coeffs));
  }
}
}  // namespace
}  // namespace linear_filters
