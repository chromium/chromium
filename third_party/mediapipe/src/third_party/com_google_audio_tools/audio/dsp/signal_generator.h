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

// A library for different signal generators. Useful for creating simple signals
// for testing purposes.
#ifndef AUDIO_DSP_SIGNAL_GENERATOR_H_
#define AUDIO_DSP_SIGNAL_GENERATOR_H_

#include <cmath>
#include <complex>
#include <random>
#include <type_traits>
#include <vector>

#include "audio/linear_filters/biquad_filter.h"
#include "audio/linear_filters/biquad_filter_coefficients.h"
#include "glog/logging.h"
#include "third_party/eigen3/Eigen/Core"



namespace audio_dsp {

// Returns an audio frame containing a sinusoidal signal.
template <typename ValueType>
std::vector<ValueType> GenerateSine(int num_samples, float sample_rate,
                                    float frequency, ValueType amplitude,
                                    float phase_begin = 0.0) {
  static_assert(std::is_signed<ValueType>::value,
                "Type ValueType must be a signed arithmetic type");

  const double kPhaseShiftPerSample = (2.0 * M_PI * frequency) / sample_rate;
  // Avoid expensive trig evaluations in the inner loop by using a
  // phasor/rotator.
  const std::complex<double> kRotator =
      std::polar<double>(1, kPhaseShiftPerSample);
  std::complex<double> scaled_phasor =
      std::polar<double>(amplitude, phase_begin);
  std::vector<ValueType> sin_wave(num_samples);
  for (int sample = 0; sample < num_samples; ++sample) {
    sin_wave[sample] = std::imag(scaled_phasor);
    scaled_phasor *= kRotator;
  }

  return sin_wave;
}

// Returns an audio frame containing a sinusoidal signal as an ArrayXf.
Eigen::ArrayXf GenerateSineEigen(int num_samples, float sample_rate,
                                 float frequency, float amplitude,
                                 float phase_begin = 0.0);

// Create a broadside impulse audio signal with the specified number of channels
// and sample block size. Adds an impulse for each channel at the same time.
// Assumes the array is linear. This function will work for ArrayXXf and
// MatrixXf.
template <typename EigenType>
EigenType GenerateLinearArrayBroadsideImpulse(int num_channels,
                                              int block_size_samples,
                                              int impulse_time_samples) {
  static_assert(std::is_base_of<Eigen::MatrixBase<EigenType>, EigenType>::value,
                "Matrix type must be a descendant of Eigen::MatrixBase.");
  ABSL_CHECK_GT(num_channels, 0);
  ABSL_CHECK_GE(block_size_samples, 0);
  ABSL_CHECK_GE(impulse_time_samples, 0);
  ABSL_CHECK_LT(impulse_time_samples, block_size_samples);

  EigenType data = EigenType::Zero(num_channels, block_size_samples);
  for (int i = 0; i < num_channels; ++i) {
    // Add an impulse seen by all mics at the same time.
    data(i, impulse_time_samples) = 1;
  }
  return data;
}

// Returns an array of length n containing samples of white Gaussian noise with
// the specified mean and standard deviation.
template <typename Scalar>
Eigen::Array<Scalar, Eigen::Dynamic, 1> GenerateWhiteGaussianNoise(
    int n, Scalar mean, Scalar stddev) {
  Eigen::Array<Scalar, Eigen::Dynamic, 1> noise(n);
  if (stddev == Scalar(0)) {
    noise.setConstant(mean);
    return noise;
  }
  std::mt19937 gen(0 /* seed */);
  std::normal_distribution<Scalar> d(mean, stddev);
  for (int i = 0; i < n; ++i) {
    noise(i) = d(gen);
  }
  return noise;
}

namespace internal {

// Returns a filter that converts white noise to pink noise. The
// filter_t60_samples value is the number of samples until the impulse
// response of the filter hits t60. This many samples should be removed from
// the resultant filtered audio to remove transient effects.
linear_filters::BiquadFilterCascadeCoefficients PinkNoiseCoeffs(
    int* filter_t60_samples);

}  // namespace internal

// Computes a pink noise signal with a specified RMS value. Pink noise features
// a 3dB/octave rolloff.
template <typename Scalar>
Eigen::Array<Scalar, Eigen::Dynamic, 1> GeneratePinkNoise(
    int length_samples, Scalar noise_rms) {
  int filter_t60_samples;
  auto coeffs = internal::PinkNoiseCoeffs(&filter_t60_samples);
  int total_length = filter_t60_samples + length_samples;
  Eigen::Array<Scalar, Eigen::Dynamic, 1> signal =
      GenerateWhiteGaussianNoise(total_length, 0.0f, 1.0f);
  // Make the signal pink and trim off the transient.
  linear_filters::BiquadFilterCascade<Scalar> neg3db_per_octave;
  Eigen::Array<Scalar, Eigen::Dynamic, 1> pink =
      Eigen::ArrayXf::Zero(total_length);
  neg3db_per_octave.Init(1, coeffs);
  neg3db_per_octave.ProcessBlock(signal, &pink);
  Eigen::Array<Scalar, Eigen::Dynamic, 1> pink_trimmed =
      pink.tail(length_samples);

  Scalar actual_rms = std::sqrt(pink_trimmed.square().mean());
  pink_trimmed *= noise_rms / actual_rms;

  return pink_trimmed;
}

}  // namespace audio_dsp

#endif  // AUDIO_DSP_SIGNAL_GENERATOR_H_
