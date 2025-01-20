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

#include "audio/linear_filters/biquad_filter_design.h"

#include <cmath>

#include "audio/linear_filters/discretization.h"

namespace linear_filters {
namespace {

void CheckArguments(double sample_rate_hz, double corner_frequency_hz,
                    double quality_factor) {
  ABSL_CHECK_LT(corner_frequency_hz, sample_rate_hz / 2);
  ABSL_CHECK_GT(corner_frequency_hz, 0.0);
  ABSL_CHECK_GT(quality_factor, 0.0);
}

void CheckArgumentsBandEdges(double sample_rate_hz, double lower_band_edge_hz,
                             double upper_band_edge_hz) {
  ABSL_CHECK_GT(sample_rate_hz / 2, upper_band_edge_hz);
  ABSL_CHECK_GT(upper_band_edge_hz, lower_band_edge_hz);
  ABSL_CHECK_GT(lower_band_edge_hz, 0.0);
}
}  // namespace

BiquadFilterCoefficients LowpassBiquadFilterCoefficients(
    double sample_rate_hz, double corner_frequency_hz, double quality_factor) {
  CheckArguments(sample_rate_hz, corner_frequency_hz, quality_factor);
  // New alternative design approach based on explicit bilinear transform of
  // s-domain numerator and denominator:
  const double omega_n = 2.0 * M_PI * corner_frequency_hz;
  return BilinearTransform(
      {0.0, 0.0, omega_n * omega_n},
      {1.0, omega_n / quality_factor, omega_n * omega_n},
      sample_rate_hz, corner_frequency_hz);
}

BiquadFilterCoefficients HighpassBiquadFilterCoefficients(
    double sample_rate_hz, double corner_frequency_hz, double quality_factor) {
  CheckArguments(sample_rate_hz, corner_frequency_hz, quality_factor);
  // New alternative design approach based on explicit bilinear transform of
  // s-domain numerator and denominator:
  const double omega_n = 2.0 * M_PI * corner_frequency_hz;
  return BilinearTransform(
      {1.0, 0.0, 0.0},
      {1.0, omega_n / quality_factor, omega_n * omega_n},
      sample_rate_hz, corner_frequency_hz);
}

namespace {
  BiquadFilterCoefficients AnalogBandpassBiquadFilterCoefficients(
      double center_frequency_hz, double quality_factor) {
    const double omega_n = 2.0 * M_PI * center_frequency_hz;
    return {{0.0, omega_n / quality_factor, 0.0},
            {1.0, omega_n / quality_factor, omega_n * omega_n}};
  }
}  // namespace

BiquadFilterCoefficients BandpassBiquadFilterCoefficients(
    double sample_rate_hz, double center_frequency_hz, double quality_factor) {
  CheckArguments(sample_rate_hz, center_frequency_hz, quality_factor);
  BiquadFilterCoefficients bandpass = AnalogBandpassBiquadFilterCoefficients(
      center_frequency_hz, quality_factor);
  return BilinearTransform(bandpass.b, bandpass.a,
                           sample_rate_hz, center_frequency_hz);
}

BiquadFilterCoefficients BandstopBiquadFilterCoefficients(
    double sample_rate_hz, double center_frequency_hz, double quality_factor) {
  CheckArguments(sample_rate_hz, center_frequency_hz, quality_factor);
  const double omega_n = 2.0 * M_PI * center_frequency_hz;
  return BilinearTransform(
      {1.0, 0.0, omega_n * omega_n},
      {1.0, omega_n / quality_factor, omega_n * omega_n},
      sample_rate_hz, center_frequency_hz);
}

BiquadFilterCoefficients RangedBandpassBiquadFilterCoefficients(
    double sample_rate_hz, double lower_passband_edge_hz,
    double upper_passband_edge_hz) {
  CheckArgumentsBandEdges(sample_rate_hz, lower_passband_edge_hz,
                          upper_passband_edge_hz);
  // Prewarp the band edges rather than warping the transform to match at
  // one frequeny.
  const double omega_1 = BilinearPrewarp(lower_passband_edge_hz,
                                         sample_rate_hz);
  const double omega_2 = BilinearPrewarp(upper_passband_edge_hz,
                                         sample_rate_hz);
  return BilinearTransform(
      {0.0, omega_2 - omega_1, 0.0},
      {1.0, omega_2 - omega_1, omega_2 * omega_1},
      sample_rate_hz, 0.0);
}

BiquadFilterCoefficients RangedBandstopBiquadFilterCoefficients(
    double sample_rate_hz, double lower_stopband_edge_hz,
    double upper_stopband_edge_hz) {
  CheckArgumentsBandEdges(sample_rate_hz, lower_stopband_edge_hz,
                          upper_stopband_edge_hz);
  // Prewarp the band edges rather than warping the transform to match at
  // one frequeny.
  const double omega_1 = BilinearPrewarp(lower_stopband_edge_hz,
                                         sample_rate_hz);
  const double omega_2 = BilinearPrewarp(upper_stopband_edge_hz,
                                         sample_rate_hz);
  return BilinearTransform(
      {1.0, 0.0, omega_2 * omega_1},
      {1.0, omega_2 - omega_1, omega_2 * omega_1},
      sample_rate_hz, 0.0);
}

BiquadFilterCoefficients LowShelfBiquadFilterCoefficients(
    float sample_rate_hz,
    float corner_frequency_hz,
    float Q,
    float gain) {
  CheckArguments(sample_rate_hz, corner_frequency_hz, Q);
  ABSL_CHECK_GT(gain, 0);
  const double sqrtk = std::sqrt(gain);
  const double omega = 2 * M_PI * corner_frequency_hz / sample_rate_hz;
  const double beta = std::sin(omega) * std::sqrt (sqrtk) / Q;

  const double sqrtk_minus_one_cos_omega = (sqrtk - 1) * std::cos(omega);
  const double sqrtk_plus_one_cos_omega = (sqrtk + 1) * std::cos(omega);

  return {{sqrtk * ((sqrtk + 1) - sqrtk_minus_one_cos_omega + beta),
           sqrtk * 2.0 * ((sqrtk - 1) - sqrtk_plus_one_cos_omega),
           sqrtk * ((sqrtk + 1) - sqrtk_minus_one_cos_omega - beta)},
          {(sqrtk + 1) + sqrtk_minus_one_cos_omega + beta,
           -2.0 * ((sqrtk - 1) + sqrtk_plus_one_cos_omega),
           (sqrtk + 1) + sqrtk_minus_one_cos_omega - beta}};
}

BiquadFilterCoefficients HighShelfBiquadFilterCoefficients(
    float sample_rate_hz,
    float corner_frequency_hz,
    float Q,
    float gain) {
  CheckArguments(sample_rate_hz, corner_frequency_hz, Q);
  ABSL_CHECK_GT(gain, 0);
  const double sqrtk = std::sqrt(gain);
  const double omega = 2 * M_PI * corner_frequency_hz / sample_rate_hz;
  const double beta = std::sin (omega) * std::sqrt(sqrtk) / Q;

  const double sqrtk_minus_one_cos_omega = (sqrtk - 1) * std::cos(omega);
  const double sqrtk_plus_one_cos_omega = (sqrtk + 1) * std::cos(omega);

  return {{sqrtk * ((sqrtk + 1) + sqrtk_minus_one_cos_omega + beta),
           sqrtk * -2.0 * ((sqrtk - 1) + sqrtk_plus_one_cos_omega),
           sqrtk * ((sqrtk + 1) + sqrtk_minus_one_cos_omega - beta)},
          {(sqrtk + 1) - sqrtk_minus_one_cos_omega + beta,
           2.0 * ((sqrtk - 1) - sqrtk_plus_one_cos_omega),
           (sqrtk + 1) - sqrtk_minus_one_cos_omega - beta}};
}

BiquadFilterCoefficients ParametricPeakBiquadFilterCoefficients(
    float sample_rate_hz,
    float center_frequency_hz,
    float Q,
    float gain) {
  CheckArguments(sample_rate_hz, center_frequency_hz, Q);
  ABSL_CHECK_GE(gain, 0);
  BiquadFilterCoefficients resonator = AnalogBandpassBiquadFilterCoefficients(
      center_frequency_hz, Q);
  ABSL_DCHECK_EQ(resonator.b[0], 0);
  ABSL_DCHECK_EQ(resonator.b[1], resonator.a[1]);
  ABSL_DCHECK_EQ(resonator.b[2], 0);
  // This is based on Julius Smith's formulation of the parametric peak
  // filter as 1 + Hs(s), where Hs(s) is an analog resonator.
  // https://ccrma.stanford.edu/~jos/fp/Peaking_Equalizers.html
  // This produces the same filter as in the notes in the header doc, but
  // the computation is simpler and more intuitive.
  resonator.b[0] += resonator.a[0];
  resonator.b[1] = gain * resonator.a[1];
  resonator.b[2] += resonator.a[2];
  return BilinearTransform(resonator.b, resonator.a,
                           sample_rate_hz, center_frequency_hz);
}

BiquadFilterCoefficients ParametricPeakBiquadFilterSymmetricCoefficients(
    float sample_rate_hz,
    float center_frequency_hz,
    float Q,
    float gain) {
  if (gain < 1) { Q *= gain; }
  return ParametricPeakBiquadFilterCoefficients(
      sample_rate_hz, center_frequency_hz, Q, gain);
}

// Uses the notation from:
// http://faculty.tru.ca/rtaylor/publications/allpass2_align.pdf
BiquadFilterCoefficients AllpassBiquadFilterCoefficients(
    float sample_rate_hz,
    float corner_frequency_hz,
    float phase_delay_radians /* at corner_frequency_hz */,
    float group_delay_seconds /* at corner_frequency_hz */) {
  ABSL_CHECK_LT(corner_frequency_hz, sample_rate_hz / 2);
  ABSL_CHECK_GT(std::abs(phase_delay_radians), 1e-5)
      << "You don't need a filter at all!";  // Covers a divide by zero case.
  // TODO: Can we handle the case where phase_delay_radians is a
  // multiple of pi more elegantly?
  ABSL_CHECK_GT(std::abs(std::sin(phase_delay_radians)), 1e-5);
  ABSL_CHECK_GT(corner_frequency_hz, 0.0);
  ABSL_CHECK(CheckAllPassConfiguration(sample_rate_hz, corner_frequency_hz,
                                  phase_delay_radians, group_delay_seconds));
  // Compute the allpass parameters, omega_0 and Q.
  const double omega_hat = BilinearPrewarp(corner_frequency_hz, sample_rate_hz);
  const double omegaT = 2 * M_PI * corner_frequency_hz / sample_rate_hz;
  const double k = group_delay_seconds * sample_rate_hz *
      std::sin(omegaT) / std::sin(phase_delay_radians);
  const double omega_0 = omega_hat * std::sqrt((k - 1) / (k + 1));
  const double cot_phase =
      (1 + std::cos(phase_delay_radians)) / std::sin(phase_delay_radians);
  const double Q = cot_phase * omega_0 * omega_hat /
      (omega_hat * omega_hat - omega_0 * omega_0);
  LOG_IF(WARNING, Q > 20) << "Large value of Q in allpass filter: " << Q;
  // Design the allpass with parameters omega_0, and Q.
  const double p = omega_0 / sample_rate_hz / 2;
  const double q_p_sq_plus_one = Q * (p * p + 1);
  const double b0 = (q_p_sq_plus_one - p) / (q_p_sq_plus_one + p);
  const double b1 = 2 * (Q * (p * p - 1)) / (q_p_sq_plus_one + p);
  return {{b0, b1, 1.0}, {1.0, b1, b0}};
}

BiquadFilterCoefficients AllpassBiquadFilterCoefficients(
    float sample_rate_hz,
    float corner_frequency_hz,
    float phase_delay_radians /* at corner_frequency_hz */) {
  return AllpassBiquadFilterCoefficients(
      sample_rate_hz, corner_frequency_hz, phase_delay_radians,
      MinimumGroupDelayForAllPass(sample_rate_hz, corner_frequency_hz,
                                  phase_delay_radians));
}

namespace {
double InfimalGroupDelayForAllPass(
    float sample_rate_hz,
    float corner_frequency_hz,
    float phase_delay_radians /* at corner_frequency_hz */) {
  const double omegaT = 2 * M_PI * corner_frequency_hz / sample_rate_hz;
  return std::abs(std::sin(phase_delay_radians)) /
      sample_rate_hz / std::sin(omegaT);
}
}  // namespace

bool /* success */ CheckAllPassConfiguration(
    float sample_rate_hz,
    float corner_frequency_hz,
    float phase_delay_radians /* at corner_frequency_hz */,
    float group_delay_seconds /* at corner_frequency_hz */) {
  if (group_delay_seconds < 0) { return false; }
  // We can't design a filter that simply inverts the waveform at the corner
  // frequencies.
  if (std::abs(std::sin(phase_delay_radians)) < 1e-5) { return false; }
  return group_delay_seconds >=
         InfimalGroupDelayForAllPass(sample_rate_hz, corner_frequency_hz,
                                     phase_delay_radians);
}

double MinimumGroupDelayForAllPass(
    float sample_rate_hz,
    float corner_frequency_hz,
    float phase_delay_radians /* at corner_frequency_hz */) {
  // If we are calling this function in the first place, it means we probably
  // aren't extremely picky about the group delay. We increase the minimum
  // group delay by an amount that will be both negligible and far enough from
  // the precise minimum so as to not fail the CheckAllPassConfiguration ABSL_CHECK.
  //
  // This tolerance places sets k = 1.001, ω₀ = 0.022355 * ω, and
  // Q = 0.022366 * (cot φ / 2).
  return 1.001 * InfimalGroupDelayForAllPass(sample_rate_hz,
                                             corner_frequency_hz,
                                             phase_delay_radians);
}

}  // namespace linear_filters
