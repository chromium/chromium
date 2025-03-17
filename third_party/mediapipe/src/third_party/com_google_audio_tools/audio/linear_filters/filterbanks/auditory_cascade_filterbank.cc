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

#include "audio/linear_filters/filterbanks/auditory_cascade_filterbank.h"

#include <cmath>
#include <functional>

#include "audio/linear_filters/biquad_filter_design.h"
#include "audio/linear_filters/discretization.h"

namespace linear_filters {

using ::Eigen::ArrayXXf;
using ::std::complex;
using ::std::vector;

namespace {

// Generate the filter coefficients for an s-plane complex pole pair.
vector<double> LaplaceCoeffsFromNaturalFrequencyAndDampings(
    double natural_frequency, double zeta) {
  const double omega = 2.0f * M_PI * natural_frequency;
  return {1.0f / (omega * omega), 2.0f * zeta / omega, 1.0f};
}

// Discretize pair of conjugate poles and a pair of conjugate zeros using
// the bilinear transform warped around pole_frequency.
BiquadFilterCoefficients DesignDiscretizedResonator(
    const AuditoryCascadeFilterbankParams& params, float sample_rate,
    float pole_frequency, float pole_zeta) {
  // This half-octave spacing from the pole to the zero is baked in.
  constexpr double kZeroRatio = M_SQRT2;
  double zero_zeta = params.pole_zeta() / kZeroRatio;

  const double zero_frequency = pole_frequency * kZeroRatio;
  vector<double> numerator = LaplaceCoeffsFromNaturalFrequencyAndDampings(
      zero_frequency, zero_zeta);
  vector<double> denominator = LaplaceCoeffsFromNaturalFrequencyAndDampings(
      pole_frequency, params.pole_zeta());
  return BilinearTransform(numerator, denominator, sample_rate, pole_frequency);
}

void CheckParameterValues(const AuditoryCascadeFilterbankParams& params,
                          float full_sample_rate_hz,
                          bool is_decimation_allowed) {
  ABSL_CHECK_GT(params.highest_pole_frequency(), 0.0);
  ABSL_CHECK_GE(params.max_pole_nyquist_fraction(), 0.0);
  ABSL_CHECK_LE(params.max_pole_nyquist_fraction(), 1.0);
  ABSL_CHECK_GT(params.step_erbs(), 0.0);
  ABSL_CHECK_LT(params.step_erbs(), 2.0);
  ABSL_CHECK_GT(params.pole_zeta(), 0.0);
  ABSL_CHECK_LT(params.pole_zeta(), 0.5);
  ABSL_CHECK_GT(params.min_pole_frequency(), 0.0);
  if (!params.channel_selection_options().use_all_channels()) {
    ABSL_CHECK_GE(params.channel_selection_options().first_channel(), 0);
    ABSL_CHECK_GT(params.channel_selection_options().skip_every_n(), 0);
    ABSL_CHECK_GT(params.channel_selection_options().max_channels(), 0);
  }
  if (is_decimation_allowed) {
    ABSL_CHECK_GT(params.max_samples_per_cycle(), 6.0);
    ABSL_CHECK_GT(params.min_sample_rate(), 0.0);
  }
}

void HandleChannelSelection(const AuditoryCascadeFilterbankParams& params,
                            int num_filterbank_stages,
                            vector<int>* exposed_filter_indices) {
if (params.channel_selection_options().use_all_channels()) {
    exposed_filter_indices->resize(num_filterbank_stages);
    for (int i = 0; i < num_filterbank_stages; ++i) {
      (*exposed_filter_indices)[i] = i;
    }
  } else {
    const auto& options = params.channel_selection_options();
    // i is the index of the exposed channel, j is the actual index of the
    // filter.
    int num_selected = 0;
    for (int i = options.first_channel();
         i < num_filterbank_stages && num_selected < options.max_channels();
         i += options.skip_every_n(), ++num_selected) {
      exposed_filter_indices->push_back(i);
    }
    // If you require a fixed number of channels and you get this warning,
    // you may need to increase the number of total channels so that
    // num_selected equals max_channels. This can be accomplished by decreasing
    // params.step_erbs().
    LOG_IF(WARNING, num_selected < options.max_channels()) <<
        "Only " << num_selected << " filterbank channels were selected.";
    ABSL_CHECK_GT(num_selected, 0) <<
        "No filters were selected. Check the channel selection params!";
  }
}

void ComputeFilterbankCentersAndBandwidths(
    const AuditoryCascadeFilterbankParams& params,
    float sample_rate,
    vector<float>* pole_frequencies,
    vector<float>* bandwidths) {
  // Ref: Glasberg and Moore: Hearing Research, 47 (1990), 103-138
  // ERB = 24.7 * (1 + 4.37 * center_frequency / 1000).
  // The limiting bandwidth on the low end is break_frequency / ear_q = 24.7 Hz.
  constexpr double kBreakFrequencyHz = 1000.0f / 4.37f;
  constexpr double kEarQualityFactor = kBreakFrequencyHz / 24.7f;

  double frequency_hz =
      std::min(params.highest_pole_frequency(),
               params.max_pole_nyquist_fraction() * sample_rate / 2);

  while (frequency_hz > params.min_pole_frequency()) {
    pole_frequencies->push_back(frequency_hz);
    const double auditory_bandwidth_hz =
        (kBreakFrequencyHz + frequency_hz) / kEarQualityFactor;
    bandwidths->push_back(auditory_bandwidth_hz);
    frequency_hz -= params.step_erbs() * auditory_bandwidth_hz;
  }
}

}  // namespace

void AuditoryCascadeFilterbank::Init(int num_mics, float sample_rate) {
  ABSL_DCHECK_GT(num_mics, 0);
  num_mics_ = num_mics;

  DesignFilterbank(sample_rate);
  filtered_output_.clear();

  filtered_output_.resize(filters_.size());
  initialized_ = true;
}

void AuditoryCascadeFilterbank::Reset() {
  for (auto& filter : filters_) {
    filter.Reset();
  }
  for (auto& decimator : decimators_) {
    decimator.Reset();
  }
  for (auto& filter : diff_filters_) {
    filter.Reset();
  }
}

// TODO: The calls to resize aren't problematic for fixed,
// power of two input block sizes, but they could be if the input size is
// variable (or if there are more decimate stages than there are factors of 2
// in input.size()).
void AuditoryCascadeFilterbank::ProcessBlock(const ArrayXXf& input) {
  // Process the first stage.
  filtered_output_[0].resize(input.rows(), input.cols());
  filters_[0].ProcessBlock(input, &filtered_output_[0]);
  // Process all remaining stages, decimating when the sample rate of the
  // current filterbank channel is not equal to that of the previous filterbank
  // channel.
  int decimator_index = 0;
  for (int stage = 1; stage < filters_.size(); ++stage) {
    if (sample_rates_[stage] != sample_rates_[stage - 1]) {
      auto last_stage_decimated =
          decimators_[decimator_index].Decimate(filtered_output_[stage - 1]);
      ++decimator_index;
      filtered_output_[stage].resize(last_stage_decimated.rows(),
                                     last_stage_decimated.cols());
      filters_[stage].ProcessBlock(last_stage_decimated,
                                   &filtered_output_[stage]);
    } else {
      filtered_output_[stage].resize(filtered_output_[stage - 1].rows(),
                                    filtered_output_[stage - 1].cols());
      filters_[stage].ProcessBlock(filtered_output_[stage - 1],
                                   &filtered_output_[stage]);
    }
  }
  for (int stage = 0; stage < filters_.size(); ++stage) {
    diff_filters_[stage].ProcessBlock(filtered_output_[stage],
                                      &filtered_output_[stage]);
  }
}

// Each stage is first designed in the s-plane,
//          s^2 + 2*zero_zeta*zero_omega*s + zero_omega^2
//   H(s) = ---------------------------------------------
//          s^2 + 2*pole_zeta*pole_omega*s + pole_omega^2
// where pole_omega is the nominal stage frequency in radians per second,
// and pole_zeta determines the relative bandwidth.  Similarly for the
// numerator, whose roots are the zeros.
//
// ERB is "equivalent rectangular bandwidth", a commonly used way of
// characterizing auditory filters.  This function implements an ERB scale,
// each frequency being lower than the previous by some number of ERBs
// (typically step_erbs = 0.5, so each frequency is a half ERB lower, which is
// good for making a filterbank of overlapping channels).
void AuditoryCascadeFilterbank::DesignFilterbank(float full_sample_rate_hz) {
  ABSL_CHECK_GT(full_sample_rate_hz, 0.0);
  CheckParameterValues(params_, full_sample_rate_hz, allow_decimation_);


  // In case initialize is called again, start with empty vectors.
  peak_frequencies_.clear();
  bandwidth_hz_.clear();
  exposed_filter_indices_.clear();
  decimators_.clear();
  diff_filters_.clear();
  filters_.clear();
  sample_rates_.clear();

  // Compute all of the pole frequencies.
  vector<float> pole_frequencies;
  ComputeFilterbankCentersAndBandwidths(
      params_, full_sample_rate_hz, &pole_frequencies, &bandwidth_hz_);

  const int num_filterbank_stages = pole_frequencies.size();

  float stage_sample_rate = full_sample_rate_hz;
  for (int stage = 0; stage < num_filterbank_stages; ++stage) {
    double pole_frequency = pole_frequencies[stage];

    // biquad_coefficients_, decimators_, diff_filters_, filters_, and
    // sample_rates_ get filled in one by one in this function.
    stage_sample_rate = DesignFilterbankStage(
        stage, stage_sample_rate, pole_frequency);
  }
  // Finally, if only a subset of the channels are to be used, compute the
  // mapping from requested filter channel number to filter index.
  HandleChannelSelection(params_, num_filterbank_stages,
                         &exposed_filter_indices_);
}

float AuditoryCascadeFilterbank::DesignFilterbankStage(
    int stage, float stage_sample_rate, float pole_frequency) {
  // Computes the gain from all stages and accounts for the differentiator.
  // This function will be maximized via FindPeakByBisection.
  auto FilterbankGainMagnitude = [this](float frequency_hz) {
    complex<double> response = 1.0;
    for (int j = 0; j < biquad_coefficients_.size(); ++j) {
      complex<double> z =
          std::polar(1.0, 2 * M_PI * frequency_hz / sample_rates_[j]);
      response *= biquad_coefficients_[j].EvalTransferFunction(z);
    }
    // Account for the gain of the differentiator.
    float cycles_per_sample = frequency_hz / sample_rates_.back();
    complex<double> z = std::polar(1.0, 2.0 * M_PI * cycles_per_sample);
    return std::abs(response) * std::abs(1.0 - z);
  };

  // Decide if we're going to decimate or not.
  bool decimate_first =
      stage > 0 &&  // Don't decimate on the first stage.
      stage_sample_rate / pole_frequency > params_.max_samples_per_cycle() &&
      stage_sample_rate >= 2 * params_.min_sample_rate() &&
      allow_decimation_;
  if (decimate_first) {
    // Halve the sample rate to start a new subbank.
    stage_sample_rate /= 2.0;
    decimators_.emplace_back();
    decimators_.back().Init(num_mics_);
  }
  diff_filters_.emplace_back();
  diff_filters_.back().Init(num_mics_);
  sample_rates_.push_back(stage_sample_rate);

  // Design the continuous time transfer function.
  biquad_coefficients_.AppendBiquad(
      DesignDiscretizedResonator(params_, stage_sample_rate,
                                 pole_frequency, params_.pole_zeta()));

  // Compute the gain at the peak frequency and readjust to set the gains to
  // one.
  auto peak_freq_and_gain = FindPeakByBisection(FilterbankGainMagnitude,
                                                0, stage_sample_rate / 2);
  peak_frequencies_.push_back(peak_freq_and_gain.first);
  double gain_adjustment = 1.0 / peak_freq_and_gain.second;
  biquad_coefficients_[biquad_coefficients_.size() - 1]
      .AdjustGain(gain_adjustment);

  filters_.emplace_back();
  filters_.back().Init(num_mics_,
                       biquad_coefficients_[biquad_coefficients_.size() - 1]);
  // Tell the caller the rate of the new stage.
  return stage_sample_rate;
}

}  // namespace linear_filters
