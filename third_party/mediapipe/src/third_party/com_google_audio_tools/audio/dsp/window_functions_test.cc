/*
 * Copyright 2026 Google LLC
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

#include "audio/dsp/window_functions.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

#include "audio/dsp/kiss_fft.h"
#include "audio/dsp/signal_vector_util.h"
#include "audio/dsp/testing_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"

namespace audio_dsp {
namespace {

using ::std::complex;
using ::std::cos;
using ::testing::Each;
using ::testing::Eq;

std::vector<std::unique_ptr<WindowFunction>> TestWindows() {
  std::vector<std::unique_ptr<WindowFunction>> windows;
  windows.emplace_back(new RectangularWindow());
  windows.emplace_back(new CosineWindow());
  windows.emplace_back(new HammingWindow());
  windows.emplace_back(new HannWindow());
  windows.emplace_back(new SqrtHannWindow());
  windows.emplace_back(new KaiserWindow(4.0 /* beta */));
  windows.emplace_back(new NuttallWindow());
  windows.emplace_back(new PlanckTaperWindow(0.15 /* epsilon */));
  windows.emplace_back(new QuarticWindow());
  return windows;
}

std::vector<float> ReferenceHannSymmetricWindow(int num_samples) {
  std::vector<float> samples(num_samples);
  for (int n = 0; n < num_samples; ++n) {
    samples[n] = 0.5 * (1 - cos(2 * M_PI * (n + 1) / (num_samples + 1)));
  }
  return samples;
}

std::vector<float> ReferenceHannPeriodicWindow(int num_samples) {
  std::vector<float> samples(num_samples);
  for (int n = 0; n < num_samples; ++n) {
    samples[n] = 0.5 * (1 - cos(2 * M_PI * n / num_samples));
  }
  return samples;
}

// The shifted periodic window is the same as the periodic window, but with the
// samples shifted by half a period.
std::vector<float> ReferenceHannPeriodicShiftedWindow(int num_samples) {
  std::vector<float> samples(num_samples);
  for (int n = 0; n < num_samples; ++n) {
    samples[n] = 0.5 * (1 - cos(2 * M_PI * (n + 0.5) / num_samples));
  }
  return samples;
}

TEST(WindowFunctionsTest, RectangularWindow) {
  std::vector<float> actual;
  for (int num_samples : {5, 6, 7, 8}) {
    SCOPED_TRACE(absl::StrCat("num_samples: ", num_samples));

    RectangularWindow().GetSymmetricSamples(num_samples, &actual);
    ASSERT_THAT(actual, Each(Eq(1.0)));

    RectangularWindow().GetPeriodicSamples(num_samples, &actual);
    ASSERT_THAT(actual, Each(Eq(1.0)));
  }
}

TEST(WindowFunctionsTest, HannWindow) {
  std::vector<float> actual;
  for (int num_samples : {5, 6, 7, 8}) {
    SCOPED_TRACE(absl::StrCat("num_samples: ", num_samples));

    HannWindow().GetSymmetricSamples(num_samples, &actual);
    ASSERT_THAT(actual, FloatArrayNear(
        ReferenceHannSymmetricWindow(num_samples), 1e-6));

    HannWindow().GetPeriodicSamples(num_samples, &actual);
    ASSERT_THAT(actual, FloatArrayNear(
        ReferenceHannPeriodicWindow(num_samples), 1e-6));
  }
}

TEST(WindowFunctionsTest, ShiftedHannWindow) {
  std::vector<float> actual;
  for (int num_samples : {5, 6, 7, 8, 512, 513}) {
    SCOPED_TRACE(absl::StrCat("num_samples: ", num_samples));

    HannWindow().GetSymmetricSamples(num_samples, &actual);
    ASSERT_THAT(actual, FloatArrayNear(
        ReferenceHannSymmetricWindow(num_samples), 1e-6));

    HannWindow().GetPeriodicSamples(num_samples, &actual);
    ASSERT_THAT(actual, FloatArrayNear(
        ReferenceHannPeriodicWindow(num_samples), 1e-6));

    HannWindow().GetPeriodicShiftedSamples(num_samples, &actual);
    ASSERT_THAT(
        actual,
        FloatArrayNear(ReferenceHannPeriodicShiftedWindow(num_samples), 1e-6));
  }
}

TEST(WindowFunctionsTest, SqrtHannWindow) {
  std::vector<float> actual;
  for (int num_samples : {5, 6, 7, 8}) {
    SCOPED_TRACE(absl::StrCat("num_samples: ", num_samples));

    SqrtHannWindow().GetSymmetricSamples(num_samples, &actual);

    std::vector<float> expected = ReferenceHannSymmetricWindow(num_samples);
    for (float& sample : expected) { sample = std::sqrt(sample); }
    ASSERT_THAT(actual, FloatArrayNear(expected, 1e-6));

    SqrtHannWindow().GetPeriodicSamples(num_samples, &actual);

    expected = ReferenceHannPeriodicWindow(num_samples);
    for (float& sample : expected) { sample = std::sqrt(sample); }
    ASSERT_THAT(actual, FloatArrayNear(expected, 1e-6));
  }
}

TEST(WindowFunctionsTest, Eval) {
  for (auto& window : TestWindows()) {
    ASSERT_EQ(window->radius(), 1.0) << "Default radius should be 1.0";

    for (double radius : {1.0, 2.5, 0.2}) {
      SCOPED_TRACE(absl::StrCat(window->name(), " window with radius ",
                                radius));
      window->set_radius(radius);
      ASSERT_EQ(window->radius(), radius);

      ASSERT_DOUBLE_EQ(window->Eval(0.0), 1.0);  // Center should be one.

      for (double x : {0.2 * radius, 0.75 * radius, 0.9 * radius}) {
        SCOPED_TRACE(absl::StrCat("x: ", x));
        // Window function should be positive for |x| < radius.
        ASSERT_GE(window->Eval(x), 1e-4);
        // Should be symmetric about x = 0.
        ASSERT_DOUBLE_EQ(window->Eval(x), window->Eval(-x));
      }

      if (window->zero_at_endpoints()) {
        // If zero_at_endpoints() returns true, check that they really are zero.
        ASSERT_DOUBLE_EQ(window->Eval(window->radius()), 0.0);
        ASSERT_DOUBLE_EQ(window->Eval(-window->radius()), 0.0);
      } else {
        // Otherwise, the window should be positive at the endpoints.
        ASSERT_GE(window->Eval(window->radius()), 1e-4);
        ASSERT_GE(window->Eval(-window->radius()), 1e-4);
      }

      // Should be zero outside of [-radius, radius].
      ASSERT_EQ(window->Eval(window->radius() + 0.01), 0.0);
    }
  }
}

TEST(WindowFunctionsTest, EvalFourierTransform) {
  constexpr int kWindowSamples = 512;
  constexpr int kTransformSize = 4 * kWindowSamples;
  RealFFTTransformer fft(kTransformSize, true);
  std::vector<float> samples;
  std::vector<complex<float>> expected_spectrum;

  for (auto& window : TestWindows()) {
    window->GetPeriodicSamples(kWindowSamples, &samples);
    samples.push_back(samples[0]);  // Add missing right endpoint.
    samples.resize(kTransformSize, 0.0);  // Zero pad to kTransformSize.
    std::rotate(samples.begin(), samples.begin() + kWindowSamples / 2,
                samples.end());

    for (double radius : {1.0, 2.5, 0.2}) {
      SCOPED_TRACE(absl::StrCat(window->name(), " window with radius ",
                                radius));
      window->set_radius(radius);
      const double dx = (2.0 * radius) / kWindowSamples;
      const double df = 1.0 / (kTransformSize * dx);
      // Estimate window spectrum by FFT.
      fft.ForwardTransform(samples, &expected_spectrum);
      MultiplyVectorByScalar(dx, &expected_spectrum);

      std::vector<float> spectrum(expected_spectrum.size());
      for (int k = 0; k < spectrum.size(); ++k) {
        spectrum[k] = window->EvalFourierTransform(df * k);
      }

      if (window->name() == "rectangular") {
        ASSERT_THAT(spectrum, FloatArrayNear(expected_spectrum, 5e-2));
      } else {
        ASSERT_THAT(spectrum, FloatArrayNear(expected_spectrum, 5e-3));
      }
    }
  }
}

TEST(WindowFunctionsTest, GetSymmetricSamples) {
  std::vector<double> samples;
  for (const auto& window : TestWindows()) {
    for (int num_samples : {5, 6, 7, 8}) {
      SCOPED_TRACE(
          absl::StrCat(window->name(), " window of length ", num_samples));
      window->GetSymmetricSamples(num_samples, &samples);
      ASSERT_THAT(samples, testing::SizeIs(num_samples));

      if (num_samples % 2 == 1) {
        // For an odd-length window, the center sample should be one.
        ASSERT_DOUBLE_EQ(samples[(num_samples - 1) / 2], 1.0);
      }

      // Samples should be symmetric.
      std::vector<double> reversed(samples.size());
      std::reverse_copy(samples.cbegin(), samples.cend(), reversed.begin());
      ASSERT_EQ(samples, reversed);
    }
  }
}

TEST(WindowFunctionsTest, GetPeriodicSamples) {
  std::vector<double> samples;
  for (const auto& window : TestWindows()) {
    for (int num_samples : {5, 6, 7, 8}) {
      SCOPED_TRACE(
          absl::StrCat(window->name(), " window of length ", num_samples));
      window->GetPeriodicSamples(num_samples, &samples);
      ASSERT_THAT(samples, testing::SizeIs(num_samples));

      // Periodic sampling should be the same as for a symmetric window +/- one
      // sample, depending on whether the window is zero at the endpoints.
      std::vector<double> expected;
      if (window->zero_at_endpoints()) {
        window->GetSymmetricSamples(num_samples - 1, &expected);
        expected.insert(expected.begin(), 0.0);  // Insert zero left endpoint.
      } else {
        window->GetSymmetricSamples(num_samples + 1, &expected);
        expected.pop_back();  // Discard right endpoint.
      }
      ASSERT_THAT(samples, FloatArrayNear(expected, 1e-9));
    }
  }
}

void CheckSpectralProperties(
    const WindowFunction& window,
    const WindowFunction::SpectralProperties& expected_properties) {
  SCOPED_TRACE(absl::StrCat(window.name(), " window with radius ",
                            window.radius()));
  auto properties = window.ComputeSpectralProperties();
  ASSERT_NEAR(properties.main_lobe_fwhm,
              expected_properties.main_lobe_fwhm, 0.02);
  ASSERT_NEAR(properties.main_lobe_energy_ratio,
              expected_properties.main_lobe_energy_ratio, 1e-4);
  ASSERT_NEAR(properties.highest_sidelobe_db,
              expected_properties.highest_sidelobe_db, 0.1);
}

TEST(WindowFunctionsTest, SpectralProperties) {
  CheckSpectralProperties(CosineWindow(), {0.59, 0.99495, -23.0});
  CheckSpectralProperties(CosineWindow(5.0), {0.59 / 5.0, 0.99495, -23.0});
  CheckSpectralProperties(SqrtHannWindow(), {0.59, 0.99495, -23.0});
  CheckSpectralProperties(HammingWindow(), {0.65, 0.99963, -42.7});
  CheckSpectralProperties(HannWindow(), {0.72, 0.99949, -31.5});
  CheckSpectralProperties(KaiserWindow(4.0), {0.60, 0.99858, -30.0});
  CheckSpectralProperties(NuttallWindow(), {0.94, 0.999999997, -98.2});
  CheckSpectralProperties(PlanckTaperWindow(0.15), {0.48, 0.916125, -13.3});
  CheckSpectralProperties(QuarticWindow(), {0.55, 0.99369, -24.0});

  // ComputeSpectralProperties() is less accurate for RectangularWindow.
  auto properties = RectangularWindow().ComputeSpectralProperties();
  ASSERT_NEAR(properties.main_lobe_fwhm, 0.44, 0.02);
  ASSERT_NEAR(properties.main_lobe_energy_ratio, 0.9028, 5e-3);
  ASSERT_NEAR(properties.highest_sidelobe_db, -13.26, 0.1);
}

}  // namespace
}  // namespace audio_dsp
