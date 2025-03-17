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

#include "audio/dsp/bessel_functions.h"

#include <random>

#include "audio/dsp/signal_vector_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/substitute.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {
namespace {

// gMock matcher to test approximate equality with a relative error tolerance,
//   | actual - expected | / |expected| <= relative_tolerance.
MATCHER_P2(IsApprox, expected, relative_tolerance,
           absl::Substitute("is approximately $0 with relative tolerance $1",
                            expected, relative_tolerance)) {
  const double actual = arg;  // arg is passed by gMock.
  const double relative_error =
      std::abs(actual - expected) / std::abs(expected);
  if (relative_error > relative_tolerance) {
    *result_listener << absl::Substitute(
        "where actual = $0 with relative error = $1", actual, relative_error);
    return false;
  }
  return true;
}

// Compare BesselI0() to values computed with high precision using the Python
// mpmath library. Values should match with relative error less than kRelTol.
TEST(BesselFunctionsTest, BesselI0Values) {
  EXPECT_DOUBLE_EQ(1.0, BesselI0(0.0));

  constexpr double kRelTol = 1e-15;
  EXPECT_THAT(BesselI0(0.1), IsApprox(1.0025015629340956469, kRelTol));
  EXPECT_THAT(BesselI0(1.0), IsApprox(1.2660658777520084062, kRelTol));
  EXPECT_THAT(BesselI0(2.5), IsApprox(3.2898391440501231209, kRelTol));
  EXPECT_THAT(BesselI0(3.4), IsApprox(6.7848131604315859988, kRelTol));
  EXPECT_THAT(BesselI0(6.8), IsApprox(140.13615971568955842, kRelTol));
  EXPECT_THAT(BesselI0(9.7), IsApprox(2118.8645036757889102, kRelTol));
  EXPECT_THAT(BesselI0(14.0), IsApprox(129418.56270064855926, kRelTol));
  EXPECT_THAT(BesselI0(18.3), IsApprox(8323886.3621920654550, kRelTol));
  // BesselI0() switches from the power series to the asymptotic expansion at
  // |x| = 21, so test several x values around this point.
  EXPECT_THAT(BesselI0(20.9), IsApprox(104774245.97086331248, kRelTol));
  EXPECT_THAT(BesselI0(21.0), IsApprox(115513961.92215806246, kRelTol));
  EXPECT_THAT(BesselI0(21.1), IsApprox(127356016.29188391566, kRelTol));
  EXPECT_THAT(BesselI0(36.3), IsApprox(386690202093606.25, kRelTol));
  EXPECT_THAT(BesselI0(40.5), IsApprox(24404306884276696.0, kRelTol));
  EXPECT_THAT(BesselI0(55.9), IsApprox(1.0121519001264093712e+23, kRelTol));
  EXPECT_THAT(BesselI0(100.0), IsApprox(1.0737517071310737999e+42, kRelTol));
  EXPECT_THAT(BesselI0(217.0), IsApprox(4.7296991939904055441e+92, kRelTol));
  EXPECT_THAT(BesselI0(450.0), IsApprox(5.092621944327146067e+193, kRelTol));
  // The result is close to exceeding DBL_MAX, which is approximately 1.8e+308.
  EXPECT_THAT(BesselI0(700.0), IsApprox(1.5295933476718736755e+302, kRelTol));
}

// Same as BesselI0Values for BesselI1().
TEST(BesselFunctionsTest, BesselI1Values) {
  EXPECT_DOUBLE_EQ(0.0, BesselI1(0.0));

  constexpr double kRelTol = 1e-15;
  EXPECT_THAT(BesselI1(0.1), IsApprox(0.050062526047092693882, kRelTol));
  EXPECT_THAT(BesselI1(1.0), IsApprox(0.5651591039924850346, kRelTol));
  EXPECT_THAT(BesselI1(2.5), IsApprox(2.516716245288698417, kRelTol));
  EXPECT_THAT(BesselI1(3.4), IsApprox(5.6701021926352188629, kRelTol));
  EXPECT_THAT(BesselI1(6.8), IsApprox(129.37763914530361831, kRelTol));
  EXPECT_THAT(BesselI1(9.7), IsApprox(2006.478672341535912, kRelTol));
  EXPECT_THAT(BesselI1(14.0), IsApprox(124707.25914906985417, kRelTol));
  EXPECT_THAT(BesselI1(18.3), IsApprox(8093164.6217115828767, kRelTol));
  // BesselI1() switches from the power series to the asymptotic expansion at
  // |x| = 21, so test several x values around this point.
  EXPECT_THAT(BesselI1(20.9), IsApprox(102236148.21702210605, kRelTol));
  EXPECT_THAT(BesselI1(21.0), IsApprox(112729199.13777552545, kRelTol));
  EXPECT_THAT(BesselI1(21.1), IsApprox(124300509.62534716725, kRelTol));
  EXPECT_THAT(BesselI1(36.3), IsApprox(381326151461679.9375, kRelTol));
  EXPECT_THAT(BesselI1(40.5), IsApprox(24101111554186504.0, kRelTol));
  EXPECT_THAT(BesselI1(55.9), IsApprox(1.0030574292185372846e+23, kRelTol));
  EXPECT_THAT(BesselI1(100.0), IsApprox(1.0683693903381625005e+42, kRelTol));
  EXPECT_THAT(BesselI1(217.0), IsApprox(4.7187886560890665448e+92, kRelTol));
  EXPECT_THAT(BesselI1(450.0), IsApprox(5.0869603248961896097e+193, kRelTol));
  EXPECT_THAT(BesselI1(700.0), IsApprox(1.5285003902339005987e+302, kRelTol));
}

// I0 should be symmetric, I0(x) = I0(-x).
TEST(BesselFunctionsTest, BesselI0IsSymmetric) {
  constexpr int kNumTrials = 20;
  std::mt19937 rng(0 /* seed */);
  for (int trial = 0; trial < kNumTrials; ++trial) {
    double x = std::uniform_real_distribution<double>(-50, 50)(rng);
    EXPECT_DOUBLE_EQ(BesselI0(x), BesselI0(-x)) << "where x = " << x;
  }
}

// I1 should be antisymmetric, I1(x) = -I1(-x).
TEST(BesselFunctionsTest, BesselI1IsAntisymmetric) {
  constexpr int kNumTrials = 20;
  std::mt19937 rng(0 /* seed */);
  for (int trial = 0; trial < kNumTrials; ++trial) {
    double x = std::uniform_real_distribution<double>(-50, 50)(rng);
    EXPECT_DOUBLE_EQ(BesselI1(x), -BesselI1(-x)) << "where x = " << x;
  }
}

double FirstDerivative(double (*f)(double), double x) {
  constexpr double dx = 1e-4;
  // A fourth-order-accurate approximation of the first derivative, equation
  // (10) of http://www.geometrictools.com/Documentation/FiniteDifferences.pdf
  return (8 * (f(x + dx) - f(x - dx)) -
          (f(x + 2 * dx) - f(x - 2 * dx))) / (12 * dx);
}

double SecondDerivative(double (*f)(double), double x) {
  constexpr double dx = 1e-4;
  // A fourth-order-accurate approximation of the second derivative, equation
  // (13) of http://www.geometrictools.com/Documentation/FiniteDifferences.pdf
  return (-30 * f(x) + 16 * (f(x + dx) + f(x - dx)) -
          (f(x + 2 * dx) + f(x - 2 * dx))) / (12 * Square(dx));
}

// The derivative of BesselI0(x) should be BesselI1(x).
TEST(BesselFunctionsTest, BesselI0Derivative) {
  constexpr double kRelTol = 1e-6;
  for (int i = 0; i <= 500; ++i) {
    const double x = i * 0.1;
    EXPECT_THAT(FirstDerivative(BesselI0, x), IsApprox(BesselI1(x), kRelTol))
        << "where x = " << x;
  }
}

// The modified Bessel functions I_n(x) and K_n(x) are solutions of the modified
// Bessel equation [http://dlmf.nist.gov/10.25],
//   x^2 f"(x) + x f'(x) = (x^2 + n^2) f(x).
// This test verifies that BesselI0() satisfies this equation using numerically
// approximated derivatives over 0 <= x <= 50.
TEST(BesselFunctionsTest, BesselI0SatisfiesDifferentialEquation) {
  constexpr double kRelTol = 1e-6;
  for (int i = 0; i <= 500; ++i) {
    const double x = i * 0.1;
    double lhs = Square(x) * SecondDerivative(BesselI0, x) +
                 x * FirstDerivative(BesselI0, x);
    double rhs = Square(x) * BesselI0(x);
    // Test that x^2 f"(x) + x f'(x) = x^2 f(x).
    ASSERT_THAT(lhs, IsApprox(rhs, kRelTol))
        << "where x = " << x << ", BesselI0(x) = " << BesselI0(x)
        << ", BesselI0'(x) = " << FirstDerivative(BesselI0, x)
        << ", BesselI0\"(x) = " << SecondDerivative(BesselI0, x);
  }
}

// Test that BesselI1() satisfies the modified Bessel equation using numerically
// approximated derivatives over 0 <= x <= 50.
TEST(BesselFunctionsTest, BesselI1SatisfiesDifferentialEquation) {
  constexpr double kRelTol = 1e-6;
  for (int i = 0; i <= 500; ++i) {
    const double x = i * 0.1;
    double lhs = Square(x) * SecondDerivative(BesselI1, x) +
                 x * FirstDerivative(BesselI1, x);
    double rhs = (Square(x) + 1) * BesselI1(x);
    // Test that x^2 f"(x) + x f'(x) = (x^2 + 1) f(x).
    ASSERT_THAT(lhs, IsApprox(rhs, kRelTol))
        << "where x = " << x << ", BesselI1(x) = " << BesselI1(x)
        << ", BesselI1'(x) = " << FirstDerivative(BesselI1, x)
        << ", BesselI1\"(x) = " << SecondDerivative(BesselI1, x);
  }
}

}  // namespace
}  // namespace audio_dsp
