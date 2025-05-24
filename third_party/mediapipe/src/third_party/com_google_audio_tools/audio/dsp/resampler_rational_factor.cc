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

#include "audio/dsp/resampler_rational_factor.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "audio/dsp/bessel_functions.h"

namespace audio_dsp {

namespace {
const double kEpsilon = 1e-12;
double Sinc(double x) {
  return std::abs(x) > kEpsilon ? std::sin(x) / x : 1.0;
}
}  // namespace

ResamplingKernel::ResamplingKernel(
    double input_sample_rate, double output_sample_rate, double radius)
    : input_sample_rate_(input_sample_rate),
      output_sample_rate_(output_sample_rate),
      radius_(radius) {}

DefaultResamplingKernel::DefaultResamplingKernel(
    double input_sample_rate, double output_sample_rate)
    : ResamplingKernel(
        input_sample_rate,
        output_sample_rate,
        // Radius is 5.0 in units of input samples for upsampling, or 5.0 in
        // units of *output* samples for downsampling.
        input_sample_rate <= output_sample_rate
        ? 5.0 : 5.0 * (input_sample_rate / output_sample_rate)) {
  // Cutoff is 90% of the input or output Nyquist rate, whichever is smaller.
  const double cutoff = 0.45 * std::min(input_sample_rate, output_sample_rate);
  // Set the Kaiser window parameter beta for about -60dB of stopband ripple.
  constexpr double kDefaultKaiserBeta = 6.0;

  Init(cutoff, kDefaultKaiserBeta);
}

DefaultResamplingKernel::DefaultResamplingKernel(
    double input_sample_rate, double output_sample_rate,
    double radius, double cutoff, double kaiser_beta)
    : ResamplingKernel(input_sample_rate, output_sample_rate, radius) {
  Init(cutoff, kaiser_beta);
}

void DefaultResamplingKernel::Init(double cutoff, double kaiser_beta) {
  // Skip initialization on invalid parameters.
  if (input_sample_rate() <= 0.0 || output_sample_rate() <= 0.0 ||
      radius() <= 0.0 || cutoff <= 0.0 ||
      cutoff > 0.5 * input_sample_rate() || kaiser_beta <= 0.0) {
    return;
  }

  // Cutoff frequency as a propotion of the input Nyquist rate.
  const double cutoff_proportion = 2.0 * cutoff / input_sample_rate();
  normalization_ = cutoff_proportion;
  radians_per_sample_ = M_PI * cutoff_proportion;
  kaiser_beta_ = kaiser_beta;
  kaiser_denominator_ = BesselI0(kaiser_beta);
  valid_ = true;
}

double DefaultResamplingKernel::Eval(double x) const {
  return normalization_ * Sinc(radians_per_sample_ * x) * KaiserWindow(x);
}

double DefaultResamplingKernel::KaiserWindow(double x) const {
  double y = std::abs(x) / radius();
  if (y < kEpsilon) {
    return 1.0;
  // The window has nonzero value of 1 / I0(beta) at the endpoints |x| = radius.
  // To sample the endpoints reliably, we push the threshold out a small bit.
  } else if (y > 1.0 + kEpsilon) {
    return 0.0;
  } else {
    return BesselI0(kaiser_beta_ *
                    std::sqrt(std::max<double>(0.0, 1.0 - y * y))) /
           kaiser_denominator_;
  }
}

}  // namespace audio_dsp
