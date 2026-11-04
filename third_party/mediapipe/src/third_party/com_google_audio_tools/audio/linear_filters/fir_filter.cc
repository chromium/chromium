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

#include "audio/linear_filters/fir_filter.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "audio/dsp/decibels.h"
#include "audio/dsp/window_functions.h"
#include "glog/logging.h"
#include "third_party/eigen3/Eigen/Core"

namespace linear_filters {

void FirFilter::Init(int num_channels, const Eigen::ArrayXf& filter) {
  Eigen::ArrayXXf actual_filter(num_channels, filter.size());
  actual_filter.rowwise() = filter.transpose();
  Init(actual_filter);
}

void FirFilter::Init(const Eigen::ArrayXXf& filter) {
  ABSL_CHECK_GE(filter.rows(), 0);
  ABSL_CHECK_GE(filter.cols(), 0);
  num_channels_ = filter.rows();
  kernel_frames_ = filter.cols();
  filter_ = filter;
  state_.resize(num_channels_, kernel_frames_);
  workspace_.resize(num_channels_, kernel_frames_);
  Reset();
}

void FirFilter::Reset() { state_.setZero(); }

namespace {

Eigen::ArrayXf sinc(const Eigen::ArrayXf& x, float width) {
  // Shift the one sample that is exactly at zero slightly off of zero.
  auto x_2_nonzero = (x / 2.0f).abs().max(1e-6f);
  return (x_2_nonzero * width).sin() / x_2_nonzero;
}

}  // namespace

Eigen::ArrayXf FirFromMagnitudeTransferFunction(
    const Eigen::ArrayXf& frequencies_hz,
    const Eigen::ArrayXf& transfer_function_db, float sample_rate_hz,
    int filter_length) {
  LOG_IF(WARNING, filter_length % 2 == 0)
      << "Using an odd length filter will yield better performance, especially "
         "at high frequencies.";
  Eigen::ArrayXf frequencies = frequencies_hz;
  const int N = filter_length;
  std::vector<float> edges_hz;
  edges_hz.reserve(frequencies.size() + 1);  // Might not use all of these.
  // Handle the one-point specification edge case uniformly (this simplifies
  // the logic for generating edges_hz a little bit).
  if (frequencies.size() == 1) {
    frequencies[0] = sample_rate_hz / 4;
  }
  // Get a vector of edge frequencies that includes 0 and Nyquist.
  if (std::abs(frequencies[0]) > 1e-3) {
    edges_hz.push_back(0);
  }
  const int last_index = frequencies.size() - 1;
  for (int i = 0; i < last_index; ++i) {
    edges_hz.push_back((frequencies[i] + frequencies[i + 1]) / 2);
  }
  if (std::abs(frequencies[last_index] - sample_rate_hz / 2) > 1e-3) {
    edges_hz.push_back(sample_rate_hz / 2);
  }
  Eigen::ArrayXf fir = Eigen::ArrayXf::Zero(N);
  // Shift center from 0 to N / 2. We use N-1 instead of N below so that the n
  // vector is symmetric (thereby making the resulting filter symmetric too).
  int N_1 = N - 1;
  Eigen::ArrayXf n = Eigen::ArrayXf::LinSpaced(N, -N_1 / 2.0, N_1 - N_1 / 2.0);
  std::vector<float> workspace(N);
  Eigen::Map<Eigen::ArrayXf> workspace_map(workspace.data(), workspace.size());
  constexpr float k2Pi = 2 * M_PI;
  const float k2Pi_N = k2Pi / N;
  const float k2Pi_fs = k2Pi / sample_rate_hz;
  // Construct a piecewise-rectangular approximation of the frequency response.
  for (int i = 0; i < edges_hz.size() - 1; ++i) {
    float bandwidth_hz = edges_hz[i + 1] - edges_hz[i];
    float center_hz = (edges_hz[i + 1] + edges_hz[i]) / 2;
    float gain = audio_dsp::DecibelsToAmplitudeRatio(transfer_function_db[i]);
    // Handle the edge bands differently, allowing them to be flatter at the
    // edges by using one contiguous band that wraps into the negative
    // frequencies.
    bool is_dc = i == 0;
    bool is_nyquist = i == edges_hz.size() - 1 && edges_hz.size() > 2;
    if (is_dc || is_nyquist) {
      gain *= 0.5;
      if (is_dc) {
        bandwidth_hz = 2.0 * edges_hz[1];
        center_hz = 0;
      } else {
        const float nyquist = sample_rate_hz / 2.0;
        bandwidth_hz = 2.0 * (nyquist - edges_hz[edges_hz.size() - 2]);
        center_hz = nyquist;
      }
    }
    float bandwidth_bins = bandwidth_hz * N / sample_rate_hz;
    // Make a rectangle in the frequency domain scaled by the transfer function
    // value.
    workspace_map = gain * sinc(k2Pi_N * n, bandwidth_bins);
    // Modulate the filter up to the center frequency.
    workspace_map *= (k2Pi_fs * center_hz * n).cos();
    fir += workspace_map;
  }
  fir *= 2.0f / N;
  // Apply a Hann window.
  audio_dsp::HannWindow().GetSymmetricSamples(N, &workspace);
  fir *= workspace_map;
  return fir;
}

}  // namespace linear_filters
