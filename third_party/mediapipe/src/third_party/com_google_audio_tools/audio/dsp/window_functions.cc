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

#include "audio/dsp/window_functions.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include "audio/dsp/bessel_functions.h"
#include "glog/logging.h"

namespace audio_dsp {

using ::std::complex;
using ::std::cos;
using ::std::sin;
using ::std::sinh;

WindowFunction::WindowFunction(double radius): radius_(radius) {
  ABSL_CHECK_GT(radius, 0.0);
}

namespace {
template <typename ValueType>
void ComputeSymmetricSamples(const WindowFunction& window,
                             int num_samples, std::vector<ValueType>* samples) {
  ABSL_CHECK_GE(num_samples, 2);
  ABSL_CHECK(samples != nullptr);
  samples->resize(num_samples);
  const int half = num_samples / 2;

  // Exclude samples at both endpoints if the window is zero there.
  const double dx = (2.0 * window.radius()) /
      (num_samples + (window.zero_at_endpoints() ? 1 : -1));
  const double x0 = (num_samples % 2 == 0) ? (dx / 2) : 0.0;
  for (int n = 0; n < num_samples - half; ++n) {
    (*samples)[half + n] = window.Eval(x0 + dx * n);
  }
  // Make the left half of the samples by reverse copy.
  std::reverse_copy(samples->begin() + half, samples->end(),
                    samples->begin());
}

template <typename ValueType>
void ComputePeriodicSamples(const WindowFunction& window,
                            int num_samples, std::vector<ValueType>* samples) {
  ABSL_CHECK_GE(num_samples, 2);
  ABSL_CHECK(samples != nullptr);
  samples->resize(num_samples);
  const int half = (num_samples + 1) / 2;

  const double dx = (2.0 * window.radius()) / num_samples;
  const double x0 = (num_samples % 2 == 1) ? (dx / 2) : 0.0;
  for (int n = 0; n < num_samples - half; ++n) {
    (*samples)[half + n] = window.Eval(x0 + dx * n);
  }
  (*samples)[0] = window.zero_at_endpoints() ? 0 : window.Eval(window.radius());
  // Make the left half of the samples by reverse copy.
  std::reverse_copy(samples->begin() + half, samples->end(),
                    samples->begin() + 1);
}
}  // namespace

void WindowFunction::GetSymmetricSamples(int num_samples,
                                         std::vector<float>* samples) const {
  ComputeSymmetricSamples(*this, num_samples, samples);
}

void WindowFunction::GetSymmetricSamples(int num_samples,
                                         std::vector<double>* samples) const {
  ComputeSymmetricSamples(*this, num_samples, samples);
}

void WindowFunction::GetPeriodicSamples(int num_samples,
                                        std::vector<float>* samples) const {
  ComputePeriodicSamples(*this, num_samples, samples);
}

void WindowFunction::GetPeriodicSamples(int num_samples,
                                        std::vector<double>* samples) const {
  ComputePeriodicSamples(*this, num_samples, samples);
}

void WindowFunction::MemoizeSamples() const {
  // If memoized_samples_ hasn't been computed yet, sample the right half of the
  // window at kNumSamples uniformly spaced points.
  if (memoized_samples_.empty()) {
    constexpr int kNumSamples = 512;
    memoized_samples_.resize(kNumSamples);
    const double dx = radius_ / memoized_samples_.size();
    for (int n = 1; n <= kNumSamples; ++n) {
      memoized_samples_[n - 1] = Eval(dx * n);
    }
  }
}

// This generic implementation of EvalFourierTransform is used if the derived
// class doesn't override it with a closed-form formula.
//
// We perform a direct dot product between a cosine of frequency f and
// uniformly-spaced window samples, using a rotator+phasor to generate the
// cosine values. This dot product is effectively midpoint quadrature to
// numerically approximate the Fourier integral
//          /
//   W(f) = | w(x) cos(2 pi f x) dx.
//          /
//
// Alternatively, the spectrum could be estimated by FFT. To resolve the main
// lobe and sidelobes (sidelobe peaks are often between DFT bins), a 100x
// finer-resolution spectrum than the DFT is needed, which requires zero-padding
// the input to 100x size. Furthermore, the frequency range of interest is
// usually only the first ~10 DFT bins, since the main lobe ends by the 5th DFT
// bin for most practical windows. That is, we want to compute a tiny subset of
// the FFT output for a very sparse input. Such a transform might be efficiently
// done with a pruned FFT algorithm [see e.g. http://www.fftw.org/pruned.html].
//
// For simplicity, we use direct dot products, which is more efficient than
// naively performing large FFTs and simpler than doing pruned FFTs.
double WindowFunction::EvalFourierTransform(double f) const {
  MemoizeSamples();
  const double dx = radius_ / memoized_samples_.size();
  const complex<double> rotator = std::polar(1.0, 2.0 * M_PI * f * dx);
  complex<double> phasor = rotator * 2.0;  // * 2 to take advantage of symmetry.
  double sum = 1.0;  // Add sample at x = 0, where window must be one.
  for (double sample : memoized_samples_) {  // Sum over right half of window.
    sum += std::real(phasor) * sample;
    phasor *= rotator;
  }

  return sum * dx;
}

WindowFunction::SpectralProperties
    WindowFunction::ComputeSpectralProperties() const {
  MemoizeSamples();
  const double dx = radius_ / memoized_samples_.size();

  double total_energy = 0.5;
  for (double sample : memoized_samples_) {
    total_energy += sample * sample;
  }
  total_energy *= 2 * dx;

  const double max_frequency = 3.0;
  const int num_frequencies = 601;
  const double df = max_frequency / (num_frequencies - 1);
  std::vector<double> energy(num_frequencies);
  for (int k = 0; k < num_frequencies; ++k) {
    energy[k] = std::norm(EvalFourierTransform(df * k));
  }

  // Find the half-maximum energy (-3dB) cutoff.
  const double half_max = 0.5 * energy[0];
  auto half_max_cutoff = std::find_if(energy.cbegin(), energy.cend(),
      [half_max](float e) { return e <= half_max; });
  // Find the end of the main lobe as the point where energy stops decreasing.
  auto main_lobe_end = std::is_sorted_until(half_max_cutoff, energy.cend(),
                                            std::greater<float>());
  LOG_IF(FATAL, main_lobe_end == energy.cend())
      << "Failed to find main lobe in " << name() << " spectrum.";

  double main_lobe_energy = 0.5 * energy[0];
  for (auto it = energy.cbegin() + 1; it != main_lobe_end; ++it) {
    main_lobe_energy += *it;
  }
  main_lobe_energy *= 2 * df;

  SpectralProperties results;
  results.main_lobe_fwhm =
      2 * df * std::distance(energy.cbegin(), half_max_cutoff);
  results.main_lobe_energy_ratio = main_lobe_energy / total_energy;
  results.highest_sidelobe_db = (10.0 / M_LN10) * log(
      *std::max_element(main_lobe_end, energy.cend()) / energy[0]);
  return results;
}

namespace {
double Sinc(double x) {
  return (std::abs(x) < 1e-8) ? 1.0 : sin(x) / x;
}

double Sinhc(double x) {
  return (std::abs(x) < 1e-8) ? 1.0 : sinh(x) / x;
}
}  // namespace

double RectangularWindow::Eval(double x) const {
  constexpr double kTol = 1e-12;
  return (std::abs(x) < (1.0 + kTol) * radius()) ? 1.0 : 0.0;
}

double RectangularWindow::EvalFourierTransform(double f) const {
  return 2.0 * radius() * Sinc(2.0 * M_PI * f * radius());
}

double CosineWindow::Eval(double x) const {
  const double y = std::abs(x / radius());
  return (y < 1.0) ? cos((M_PI / 2.0) * y) : 0.0;
}

double CosineWindow::EvalFourierTransform(double f) const {
  constexpr double kTol = 1e-8;
  const double g = 2.0 * M_PI * f * radius();
  const double denom = M_PI * M_PI - 4.0 * g * g;
  if (std::abs(denom) >= kTol) {
    return radius() * 4.0 * M_PI * cos(g) / denom;
  } else {
    return radius();  // Handle removable singularity at g = pi / 2.
  }
}

constexpr double HammingWindow::kA[2];

double HammingWindow::Eval(double x) const {
  constexpr double kTol = 1e-12;
  const double y = std::abs(x / radius());
  // The window has nonzero value of 0.08 at the endpoints |x| = radius.
  // To sample the endpoints reliably, we push the threshold out a small bit.
  if (y < 1.0 + kTol) {
    return kA[0] + kA[1] * cos(M_PI * std::min<double>(1.0, y));
  } else {
    return 0.0;
  }
}

double HammingWindow::EvalFourierTransform(double f) const {
  constexpr double kTol = 1e-8;
  const double g = 2.0 * f * radius();
  const double g2 = g * g;
  const double denom = 1.0 - g2;
  if (std::abs(denom) < kTol) {
    return radius() * kA[1];  // Handle removable singularity at g = 1.
  } else {
    return radius() * 2.0 * Sinc(M_PI * g) * (kA[0] + g2 * kA[1] / denom);
  }
}

double HannWindow::Eval(double x) const {
  const double y = std::abs(x / radius());
  return (y < 1.0) ? 0.5 * (1.0 + cos(M_PI * y)) : 0.0;
}

double HannWindow::EvalFourierTransform(double f) const {
  constexpr double kTol = 1e-8;
  const double g = 2.0 * f * radius();
  const double denom = 1.0 - g * g;
  if (std::abs(denom) >= kTol) {
    return radius() * Sinc(M_PI * g) / denom;
  } else {
    return radius() * 0.5;  // Handle removable singularity at g = 1.
  }
}

KaiserWindow::KaiserWindow(double radius, double beta)
    : WindowFunction(radius),
      beta_(beta),
      denom_(BesselI0(beta)) {
  ABSL_CHECK_GE(beta, 0.0);
}

double KaiserWindow::Eval(double x) const {
  constexpr double kTol = 1e-12;
  const double y = std::abs(x / radius());
  // The window has nonzero value of 1 / I0(beta) at the endpoints |x| = radius.
  if (y < 1.0 + kTol) {
    return BesselI0(beta_ * std::sqrt(std::max<double>(0.0, 1.0 - y * y))) /
        denom_;
  } else {
    return 0.0;
  }
}

double KaiserWindow::EvalFourierTransform(double f) const {
  const double g = 2.0 * M_PI * f * radius();
  const double diff = beta_ * beta_ - g * g;
  return radius() * (2.0 / denom_) *
      ((diff > 0) ? Sinhc(std::sqrt(diff)) : Sinc(std::sqrt(-diff)));
}

constexpr double NuttallWindow::kA[4];

double NuttallWindow::Eval(double x) const {
  constexpr double kTol = 1e-12;
  const double y = std::abs(x / radius());
  // The window has nonzero value of 3.628e-4 at the endpoints |x| = radius.
  if (y < 1.0 + kTol) {
    const double z = M_PI * std::min<double>(1.0, y);
    return kA[0] + kA[1] * cos(z) +
        kA[2] * cos(2 * z) + kA[3] * cos(3 * z);
  } else {
    return 0.0;
  }
}

double NuttallWindow::EvalFourierTransform(double f) const {
  constexpr double kTol = 1e-8;
  const double g = 2.0 * f * radius();
  const double g2 = g * g;
  const double denom1 = 1.0 - g2;
  const double denom2 = 4.0 - g2;
  const double denom3 = 9.0 - g2;
  if (std::abs(denom1) < kTol) {
    return radius() * kA[1];  // Handle removable singularity at g = 1.
  } else if (std::abs(denom2) < kTol) {
    return radius() * kA[2];  // Handle removable singularity at g = 2.
  } else if (std::abs(denom3) < kTol) {
    return radius() * kA[3];  // Handle removable singularity at g = 3.
  } else {
    return radius() * 2.0 * Sinc(M_PI * g) *
        (kA[0] + g2 * (kA[1] / denom1 - kA[2] / denom2 + kA[3] / denom3));
  }
}

PlanckTaperWindow::PlanckTaperWindow(double radius, double epsilon)
    : WindowFunction(radius),
      epsilon_(epsilon) {
  ABSL_CHECK_GE(epsilon, 0.0);
  ABSL_CHECK_LE(epsilon, radius);
}

double PlanckTaperWindow::Eval(double x) const {
  constexpr double kTol = 1e-12;
  const double y = (radius() - std::abs(x)) / epsilon_;
  if (y >= 1.0 - kTol) {
    return 1.0;
  } else if (y > kTol) {
    return 1.0 / (1.0 + std::exp(1.0 / y - 1.0 / (1.0 - y)));
  } else {
    return 0.0;
  }
}

QuarticWindow::QuarticWindow(double radius, double c_2, double c_4)
    : WindowFunction(radius),
      c_2_(c_2),
      c_4_(c_4) {
  ABSL_CHECK_GE(c_2, -2.0);  // Falling faster than this can't be compensated by c_4.
  ABSL_CHECK_LE(c_2, 0.0);  // Keeps peak at center.
  ABSL_CHECK_GE(c_4, -(1 + c_2));  // Keeps window nonnegative at ends.
  ABSL_CHECK_LE(c_4, -c_2 / 2);  // Prevent slope reversal at ends to keep unimodal.
}

double QuarticWindow::Eval(double x) const {
  constexpr double kTol = 1e-12;
  const double y = x / radius();
  const double y2 = y * y;
  // The window is generally nonzero at the endpoints |x| = radius.
  // To sample the endpoints reliably, we push the threshold out a small bit.
  return (y2 < 1.0 + kTol) ? 1.0 + y2 * (c_2_ + y2 * c_4_) : 0.0;
}

double QuarticWindow::EvalFourierTransform(double f) const {
  constexpr double kTol = 1e-8;
  const double g = 2 * M_PI * f * radius();
  if (std::abs(g) >= kTol) {
    const double g2 = g * g;
    return radius() * 2.0 * (
        (2.0 * c_2_ + 4.0 * c_4_ - 24.0 * c_4_ / g2) * cos(g) / g2 +
        (1.0 + c_2_ + c_4_ +
         (-2.0 * c_2_ - 12.0 * c_4_ + 24.0 * c_4_ / g2) / g2) * Sinc(g));
  } else {  // Handle removable singularity at g = 0.
    return radius() * 2.0 * (1.0 + c_2_ / 3.0 + c_4_ / 5.0);
  }
}

}  // namespace audio_dsp
