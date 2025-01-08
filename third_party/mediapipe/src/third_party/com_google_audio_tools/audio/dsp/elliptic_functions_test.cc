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

#include "audio/dsp/elliptic_functions.h"

#include <cmath>
#include <random>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"

#include "audio/dsp/porting.h"  // auto-added.


using std::complex;

namespace audio_dsp {
namespace {

MATCHER_P2(ComplexDoubleNear, expected, tolerance,
           "complex value is near " + ::testing::PrintToString(expected)) {
  return std::abs(arg - expected) <= tolerance;
}

// Compare EllipticK to known values.
TEST(EllipticFunctionsTest, EllipticK) {
  EXPECT_DOUBLE_EQ(EllipticK(0.0), M_PI / 2);
  EXPECT_TRUE(isinf(EllipticK(1.0)));

  // Compare with values verified with scipy and WolframAlpha (e.g. for m = 0.1,
  // scipy.special.ellipk(0.1) and EllipticK[0.1]). Accuracy degrades as m
  // approaches 1, where the function is singular.
  EXPECT_NEAR(EllipticK(0.1), 1.6124413487202194, 1e-12);
  EXPECT_NEAR(EllipticK(0.5), 1.8540746773013719, 1e-12);
  EXPECT_NEAR(EllipticK(0.99), 3.6956373629898747, 1e-12);
  EXPECT_NEAR(EllipticK(1.0 - 1e-10), 12.8992198263875995, 1e-7);
  EXPECT_NEAR(EllipticK(1.0 - 1e-14), 17.5043900120782517, 1e-3);
}

// Compare EllipticF to known values.
TEST(EllipticFunctionsTest, EllipticF) {
  struct TestValue {
    complex<double> phi;
    double m;
    complex<double> expected;
  };
  // Compare with values verified with scipy and WolframAlpha (e.g for
  // F(0.6|0.1), scipy.special.ellipkinc(0.6, 0.1) and EllipticF[0.6, 0.1]; note
  // that scipy's implementation doesn't support complex phi).
  for (const auto& test_value : std::vector<TestValue>({
    // Test that F(0|m) = 0.
    {{0, 0.0}, 0.7, {0.0, 0.0}},
    // Test a small value where EllipticF uses a Taylor approximation.
    {{5e-4, 1e-4}, 0.7, {0.0005000000128333, 0.0001000000086333}},
    // Values on the real line.
    {{0.6, 0.0}, 0.1, {0.6033995956681880, 0.0}},
    {{0.8, 0.0}, 0.7, {0.8640250261841760, 0.0}},
    // Complex values.
    {{0.0, 1.8317348832729556}, 0.00027225, {0.0, 1.8311969870256281}},
    {{0.8, 0.3}, 0.7, {0.8333618443650643, 0.3700664506506054}},
    // Should have F(-z|m) = -F(z|m) and F(z*|m) = F(z|m)*.
    {{0.8, -0.3}, 0.7, {0.8333618443650643, -0.3700664506506054}},
    {{-0.8, 0.3}, 0.7, {-0.8333618443650643, 0.3700664506506054}},
    {{-0.8, -0.3}, 0.7, {-0.8333618443650643, -0.3700664506506054}}})) {
    const complex<double> phi = test_value.phi;
    const double m = test_value.m;
    const complex<double> expected = test_value.expected;
    SCOPED_TRACE(absl::StrFormat("phi = %g + %gi, m = %g, expected = %g + %gi",
                                 phi.real(), phi.imag(), m, expected.real(),
                                 expected.imag()));

    complex<double> u = EllipticF(phi, m);
    EXPECT_THAT(u, ComplexDoubleNear(expected, 1e-12));
  }
}

// JacobiAmplitude should be the inverse of EllipticF.
TEST(EllipticFunctionsTest, JacobiAmplitude) {
  std::mt19937 rng(0 /* seed */);
  std::uniform_real_distribution<double> m_dist(0.0, 0.9999);
  std::uniform_real_distribution<double> phi_real_dist(-M_PI / 2, M_PI / 2);
  std::uniform_real_distribution<double> phi_imag_dist(-4.0, 4.0);

  for (int i = 0; i < 100; ++i) {
    const double m = m_dist(rng);
    const complex<double> phi(phi_real_dist(rng), phi_imag_dist(rng));
    const complex<double> u = EllipticF(phi, m);
    SCOPED_TRACE(absl::StrFormat("phi = %g + %gi, m = %g, u = %g + %gi",
                                 phi.real(), phi.imag(), m, u.real(),
                                 u.imag()));
    const complex<double> phi_recovered = JacobiAmplitude(u, m);
    ASSERT_THAT(phi_recovered, ComplexDoubleNear(phi, 1e-12));
  }
}

}  // namespace
}  // namespace audio_dsp
