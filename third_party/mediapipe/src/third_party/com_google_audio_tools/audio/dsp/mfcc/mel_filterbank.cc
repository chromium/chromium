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

// This code resamples the FFT bins, and smooths then with triangle-shaped
// weights to create a mel-frequency filter bank. For filter i centered at f_i,
// there is a triangular weighting of the FFT bins that extends from
// filter f_i-1 (with a value of zero at the left edge of the triangle) to f_i
// (where the filter value is 1) to f_i+1 (where the filter values returns to
// zero).

// Note: this code fails if you ask for too many channels.  The algorithm used
// here assumes that each FFT bin contributes to at most two channels: the
// right side of a triangle for channel i, and the left side of the triangle
// for channel i+1.  If you ask for so many channels that some of the
// resulting mel triangle filters are smaller than a single FFT bin, these
// channels may end up with no contributing FFT bins.  The resulting mel
// spectrum output will have some channels that are always zero.

#include "audio/dsp/mfcc/mel_filterbank.h"

#include <math.h>

#include "glog/logging.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {

MelFilterbank::MelFilterbank() : initialized_(false) {}

bool MelFilterbank::Initialize(int fft_length, double sample_rate,
                               int mel_channel_count,
                               double lower_frequency_limit,
                               double upper_frequency_limit) {
  num_mel_channels_ = mel_channel_count;
  sample_rate_ = sample_rate;
  fft_length_ = fft_length;

  if (num_mel_channels_ < 1) {
    LOG(ERROR) << "Number of mel filterbank channels must be positive.";
    return false;
  }

  if (sample_rate_ <= 0) {
    LOG(ERROR) << "Sample rate must be positive.";
    return false;
  }

  if (fft_length_ < 2) {
    LOG(ERROR) << " FFT length must greater than 1.";
    return false;
  }

  if (lower_frequency_limit < 0) {
    LOG(ERROR) << "Lower frequency limit must be nonnegative.";
    return false;
  }

  if (upper_frequency_limit <= lower_frequency_limit) {
    LOG(ERROR) << "Upper frequency limit must be greater than "
               << "lower frequency limit.";
    return false;
  }

  const double mel_low = FreqToMel(lower_frequency_limit);
  const double mel_hi = FreqToMel(upper_frequency_limit);
  const double mel_spacing = (mel_hi - mel_low) / (num_mel_channels_ + 1);

  // Always exclude DC; emulate HTK.
  const double hz_per_sbin = sample_rate_ / (2.0 * (fft_length_ - 1));
  start_index_ = static_cast<int>(1.5 + lower_frequency_limit / hz_per_sbin);
  end_index_ = static_cast<int>(upper_frequency_limit / hz_per_sbin);

  // Maps the FFT bin indices to filter bank channels/indices and creates the
  // weighting functions to taper the band edges. For each FFT bin, band_mapper
  // tells us which channel this bin contributes to on the right side of the
  // triangle. Thus this bin also contributes to the left side of the next
  // channel's triangle response. The contribution of any one FFT bin is
  // complementary to the distance to the center of mel channel it is
  // contributing to. This means it contributes weights_[i] to the channel above
  // and 1-weights_[i] to the next below.
  channel_weights_sum_.assign(num_mel_channels_, 0.0);
  band_mapper_.resize(fft_length_);
  weights_.resize(fft_length_);
  for (int i = 0; i < fft_length_; ++i) {
    if (i < start_index_ || i > end_index_) {
      band_mapper_[i] = -2;  // Indicate an unused Fourier coefficient.
      weights_[i] = 0.0;
    } else {
      const double mel_position =
          (FreqToMel(i * hz_per_sbin) - mel_low) / mel_spacing - 1;
      const int channel = static_cast<int>(std::ceil(mel_position)) - 1;
      band_mapper_[i] = channel;
      ABSL_DCHECK_GT(channel, -2);
      ABSL_DCHECK_LT(channel, num_mel_channels_);
      weights_[i] = 1.0 - (mel_position - channel);

      // Accumulate the sum of all the weights pertaining to a channel.
      if (channel + 1 < num_mel_channels_) {
        // Left side of triangle.
        channel_weights_sum_[channel + 1] += (1.0 - weights_[i]);
      }
      if (channel >= 0) {
        // Right side of the triangle.
        channel_weights_sum_[channel] += weights_[i];
      }
    }
  }

  // Check the sum of FFT bin weights for every mel channel to identify
  // situations where the mel channels are so narrow that they don't get
  // significant weight on enough (or any) FFT bins -- i.e., too many
  // mel channels have been requested for the given FFT size.
  std::vector<int> bad_channels;
  for (int c = 0; c < num_mel_channels_; ++c) {
    // The lowest mel channels have the fewest FFT bins and the lowest
    // weights sum.  But given that the target gain at the center frequency
    // is 1.0, if the total sum of weights is 0.5, we're in bad shape.
    if (channel_weights_sum_[c] < 0.5) {
      bad_channels.push_back(c);
    }
  }
  if (!bad_channels.empty()) {
    LOG(ERROR) << "Missing " << bad_channels.size() << " bands "
               << " starting at " << bad_channels[0]
               << " in mel-frequency design. "
               << "Perhaps too many channels or "
               << "not enough frequency resolution in spectrum. ("
               << "fft_length: " << fft_length
               << " sample_rate: " << sample_rate
               << " mel_channel_count: " << mel_channel_count
               << " lower_frequency_limit: " << lower_frequency_limit
               << " upper_frequency_limit: " << upper_frequency_limit;
  }

  initialized_ = true;
  return true;
}

// Compute the mel spectrum from the squared-magnitude FFT input by taking the
// square root, then summing FFT magnitudes under triangular integration windows
// whose widths increase with frequency.
void MelFilterbank::Compute(const std::vector<double> &squared_magnitude_fft,
                            std::vector<double> *mel) const {
  if (!initialized_) {
    LOG(ERROR) << "Mel Filterbank not initialized.";
    return;
  }

  if (squared_magnitude_fft.size() <= end_index_) {
    LOG(ERROR) << "FFT too short to compute filterbank";
    return;
  }

  // Ensure mel is right length and reset all values.
  mel->assign(num_mel_channels_, 0.0);

  for (int i = start_index_; i <= end_index_; i++) {  // For each FFT bin
    double spec_val = sqrt(squared_magnitude_fft[i]);
    double weighted = spec_val * weights_[i];
    int channel = band_mapper_[i];
    if (channel >= 0)
      (*mel)[channel] += weighted;  // Right side of triangle, downward slope
    channel++;
    if (channel < num_mel_channels_)
      (*mel)[channel] += spec_val - weighted;  // Left side of triangle
  }
}

void MelFilterbank::EstimateInverse(
    const std::vector<double> &mel,
    std::vector<double> *squared_magnitude_fft) const {
  if (!initialized_) {
    LOG(ERROR) << "Mel Filterbank not initialized.";
    return;
  }

  if (mel.size() != num_mel_channels_) {
    LOG(ERROR) << "Mel size does not match number of mel channels.";
    return;
  }

  // Ensure squared-magnitude FFT is right length and reset all values.
  squared_magnitude_fft->assign(fft_length_, 0.0);

  // Estimates the squared-magnitude FFT bin value in the following way:
  //
  // fft[i] ~ (weights_[i] * mel[c] / channel_weights_sum_[c] +
  //          (1 - weights_[i]) * mel[c + 1] / channel_weights_sum_[c + 1]) ^ 2
  //
  // where i represents the squared-magnitude FFT bin and c represents the mel
  // channel.
  for (int i = start_index_; i <= end_index_; ++i) {
    const int channel = band_mapper_[i];
    if (channel + 1 < num_mel_channels_) {
      // Left side of triangle.
      (*squared_magnitude_fft)[i] += (1 - weights_[i]) * mel[channel + 1] /
                                     channel_weights_sum_[channel + 1];
    }
    if (channel >= 0) {
      // Right side of the triangle.
      (*squared_magnitude_fft)[i] +=
          weights_[i] * mel[channel] / channel_weights_sum_[channel];
    }
    // Squared magnitude.
    (*squared_magnitude_fft)[i] *= (*squared_magnitude_fft)[i];
  }
}

double MelFilterbank::FreqToMel(double freq) const {
  return 1127.0 * log(1.0 + (freq / 700.0));
}

}  // namespace audio_dsp
