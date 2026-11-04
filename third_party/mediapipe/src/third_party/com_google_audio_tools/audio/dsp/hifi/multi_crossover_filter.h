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

// A filterbank whose bands have the property that their magnitude responses
// sum to unity.

#ifndef AUDIO_DSP_HIFI_MULTI_CROSSOVER_FILTER_H_
#define AUDIO_DSP_HIFI_MULTI_CROSSOVER_FILTER_H_

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "audio/linear_filters/biquad_filter_coefficients.h"
#include "audio/linear_filters/biquad_filter_design.h"
#include "audio/linear_filters/crossover.h"
#include "audio/linear_filters/ladder_filter.h"
#include "glog/logging.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.

namespace audio_dsp {

// A filterbank with the property that preserves energy across frequency, i.e.
// the sweep response is flat.
//
// This condition is only true for the default setting of using the
// Linkwitz-Riley crossover type, however, Butterworth crossovers are supported
// for flexibility.
//
// NOTE: At least with this architecture, it is impossible to guarantee perfect
// flatness at all frequencies. Internally, this class ensures that, at
// crossover frequencies, the output between different stages of the filterbank
// will be in phase. As soon as you deviate from those frequencies, your mileage
// may vary (depends on the all pass design). However, for numbers of bands less
// than 5 or so, the amount of deviation in the sweep response's linear
// amplitude is typically less than 1%. As you move to more bands, you may
// see larger deviations. It is challenging to characterize the amount of
// deviation based only on input parameters, so be sure to test carefully if
// the perfect reconstruction property is important for your application.
//
// There are several examples of multiband filters and their associated
// tolerances in the test file.
//
// The number of biquad filters, processed during multi-crossover filter is:
//   order * ((bands-1)*3/4 + (bands-1)*(bands-2) / 4)
// It can be reduced in a two-step split and merge process where samples should
// be split, processed, and remerged. For example:
//   filter.SplitBands(input);
//   for (int i = 0; i < filter.num_bands(); ++i) {
//     per_band_drc_[i].ProcessBlock(filter.FilteredOutput(i),
//                                  &filtered_output);
//   }
//   filter.MergeBands(filtered_output, merged_output);
// The number of processed biquads in this case is:
//   order * ((bands-1)*3/4 + (bands-2) / 4)
// If you do not need to re-merge the bands, you can use the
// simpler ProcessBlock interface:
//   filter.ProcessBlock(input);
//   for (int i = 0; i < filter.num_bands(); ++i) {
//     per_band_drc_[i].ProcessBlock(filter.FilteredOutput(i),
//                                   &filtered_output[i]);
//   }
// ProcessBlock approach is simpler, but Split/Merge is faster, especially when
// you have more frequency bands:
// Bands Num  ProcessBlock  Split/Merge
//  2          3             3
//  3          8             7
//  4         15            11
//  5         24            15
//  6         35            19
//  7         48            23
//  8         63            27
//  9         80            31
// 10         99            35

template <typename ScalarType /* float/double */>
class MultiCrossoverFilter {
 public:
  using ArrayBlockType =
      Eigen::Array<ScalarType, Eigen::Dynamic, Eigen::Dynamic>;
  using SampleBlockType = Eigen::Array<ScalarType, Eigen::Dynamic, 1>;

  MultiCrossoverFilter(
      int num_bands, int order,
      linear_filters::CrossoverType type = linear_filters::kLinkwitzRiley)
      : type_(type),
        order_(order),
        num_bands_(num_bands),
        num_stages_(num_bands_ - 1),
        num_channels_(0 /* uninitialized */),
        sample_rate_hz_(0 /* uninitialized */),
        highpass_filters_(num_stages_),
        // AP filter of squared LP/HP has two times less biquads.
        // We use allpass filters for faster LP filtering:
        // LP = AP - HP
        allpass_filters_(num_stages_),
        phase_correction_filters_(num_stages_),
        filtered_output_(num_bands_) {
    ABSL_CHECK_GT(num_bands, 1);
    ABSL_CHECK_EQ(order_ % 2, 0);
  }

  // crossover_frequencies_hz.size() must equal num_bands - 1 and have
  // monotonically increasing elements.
  void Init(int num_channels, float sample_rate_hz,
            const std::vector<ScalarType>& crossover_frequencies_hz) {
    num_channels_ = num_channels;
    sample_rate_hz_ = sample_rate_hz;

    SetCrossoverFrequenciesInternal(crossover_frequencies_hz, true);
    Reset();
  }

  void Reset() {
    for (auto& stage : highpass_filters_) {
      for (auto& filter : stage) {
        filter.Reset();
      }
    }
    for (auto& stage : phase_correction_filters_) {
      for (auto& filter : stage) {
        filter.Reset();
      }
    }
    for (auto& stage : allpass_filters_) {
      for (auto& filter : stage) {
        filter.Reset();
      }
    }
    for (auto& output : filtered_output_) {
      output.setZero();
    }
  }

  // crossover_frequencies_hz.size() must equal num_bands - 1 and have
  // monotonically increasing elements.
  void SetCrossoverFrequencies(
      const std::vector<ScalarType>& crossover_frequencies_hz) {
    SetCrossoverFrequenciesInternal(crossover_frequencies_hz, false);
  }

  // Process a block of samples. input is a 2D Eigen array with contiguous
  // column-major data, where the number of rows equals GetNumChannels().
  // At this point we do not apply phase correction. Corresponding phase
  // correction is applied in MergeBands.
  // Instead of LP we use AP-HP, that saves us 25% of CPU clocks.
  // A four-way crossover would look like this:
  // Stage:     0        1        2
  // input --> LP_0 ------------------> Output band 0
  //       \-> HP_0 --> LP_1----------> Output band 1
  //                \-> HP_1 --> LP_2-> Output band 2
  //                         \-> HP_2-> Output band 3
  // Do not use ProcessBlock method if you use Split/Merge.
  void SplitBands(const ArrayBlockType& input) {
    filtered_output_[0] = input;
    for (int stage = 0; stage < num_stages_; ++stage) {
      filtered_output_[stage + 1] = filtered_output_[stage];
      // The highpass filter processes.
      for (auto& filter : highpass_filters_[stage]) {
        filter.ProcessBlock(filtered_output_[stage + 1],
                            &filtered_output_[stage + 1]);
      }
      // The lowpass (allpass-highpass) filters processes.
      for (auto& filter : allpass_filters_[stage]) {
        filter.ProcessBlock(filtered_output_[stage], &filtered_output_[stage]);
      }
      filtered_output_[stage] -= filtered_output_[stage + 1];
    }
  }

  // Filtered output from the filter_stage-th of the filterbank. Channels are
  // ordered by increasing passband frequency.
  const ArrayBlockType& FilteredOutput(int band_number) const {
    ABSL_DCHECK_LT(band_number, num_bands_);
    return filtered_output_[band_number];
  }

  // Applies phase correction to filtered output bands and merge it into
  // |output|.
  // A four-way merge would look like this:
  // Stage:     0        1        2
  // Output band 0 --> AP_1->\
  // Output band 1 ------------>\ AP_2 ->\
  // Output band 2 ------------------------>\
  // Output band 3 ---------------------------> output
  void MergeBands(std::vector<ArrayBlockType>& filtered_bands,
                  Eigen::Map<ArrayBlockType> output) {
    ABSL_CHECK_EQ(filtered_bands.size(), num_bands_);
    output = filtered_bands[0];
    for (int stage = 1; stage < num_stages_; ++stage) {
      for (auto& filter : phase_correction_filters_[stage]) {
        filter.ProcessBlock(output, &output);
      }
      output += filtered_bands[stage];
    }
    output += filtered_bands[num_stages_];
  }

  // Process a block of samples. input is a 2D Eigen array with contiguous
  // column-major data, where the number of rows equals GetNumChannels().
  // After spliting input into separate bands we apply phase correction.
  // A four-way crossover would look like this:
  // Stage:     0        1        2
  // input --> LP_0 ----AP_1-----AP_2-> Output band 0
  //       \-> HP_0 --> LP_1-----AP_2-> Output band 1
  //                \-> HP_1 --> LP_2-> Output band 2
  //                         \-> HP_2-> Output band 3
  // Do not use Split/Merge method if you use ProcessBlock.
  void ProcessBlock(const ArrayBlockType& input) {
    SplitBands(input);
    for (int band = 0; band < num_bands_ - 2; ++band) {
      for (int stage = band + 1; stage < num_stages_; ++stage) {
        for (auto& filter : phase_correction_filters_[stage]) {
          filter.ProcessBlock(filtered_output_[band], &filtered_output_[band]);
        }
      }
    }
  }

  int num_bands() const { return num_bands_; }

  double GetPhaseResponseAt(int band, double frequency_hz) const {
    // NOTE: When GetAllCoefficients() is being called during
    // SetCrossoverFrequenciesInternal(), it being used to fill in the
    // allpass_coeffs_ array, which starts as bypassed filters and eventually
    // becomes full of all pass filters. It is working as intended for this
    // to return different values as this array is filled in.
    return GetCoefficientsForBand(band).PhaseResponseAtFrequency(
          frequency_hz, sample_rate_hz_);
  }

  linear_filters::BiquadFilterCascadeCoefficients GetCoefficientsForBand(
      int band) const {
    ABSL_DCHECK_LT(band, num_bands_);
    linear_filters::BiquadFilterCascadeCoefficients this_band;

    for (int s = 0; s < band; ++s) {
      for (int i = 0; i < highpass_coeffs_[s].size(); ++i) {
        this_band.AppendBiquad(highpass_coeffs_[s][i]);
      }
    }

    for (int s = band; s < num_stages_; ++s) {
      for (int i = 0; i < allpass_coeffs_[s].size(); ++i) {
        this_band.AppendBiquad(allpass_coeffs_[s][i]);
      }
    }
    return this_band;
  }

  // The returned angle will be between [-pi and pi).
  template <typename T>
  static T WrapAngle(T x) {
    x = std::fmod(x + M_PI, 2 * M_PI);
    if (x < 0) {
      x += 2 * M_PI;
    }
    return x - M_PI;
  }

 private:
  void SetCrossoverFrequenciesInternal(
      const std::vector<ScalarType>& crossover_frequencies_hz, bool initial) {
    ABSL_CHECK_EQ(crossover_frequencies_hz.size(), num_stages_);
    ABSL_CHECK(std::is_sorted(crossover_frequencies_hz.begin(),
                         crossover_frequencies_hz.end()));
    crossover_frequencies_hz_ = crossover_frequencies_hz;
    // Compute the lowpass/highpass filter coefficients.
    allpass_coeffs_.clear();
    allpass_coeffs_.resize(num_stages_);
    highpass_coeffs_.clear();
    highpass_coeffs_.resize(num_stages_);

    int num_channels = num_channels_;  // Avoid passing entire "this" to lambda.
    auto InitFilter = [initial, num_channels](
                          linear_filters::BiquadFilterCoefficients& coeffs,
                          LadderFilterType* filter) {
      std::vector<double> k;
      std::vector<double> v;
      coeffs.AsLadderFilterCoefficients(&k, &v);
      if (initial) {
        filter->InitFromLadderCoeffs(num_channels, k, v);
      } else {
        filter->ChangeLadderCoeffs(k, v);
      }
    };
    for (int stage = 0; stage < num_stages_; ++stage) {
      ScalarType frequency_hz = crossover_frequencies_hz_[stage];
      linear_filters::ButterworthFilterDesign filter_design =
          linear_filters::ButterworthFilterDesign(order_ / 2);
      allpass_coeffs_[stage] =
          filter_design.HighpassCoefficients(sample_rate_hz_, frequency_hz);

      for (int i = 0; i < allpass_coeffs_[stage].size(); ++i) {
        // Square highpass.
        highpass_coeffs_[stage].AppendBiquad(allpass_coeffs_[stage][i]);
        highpass_coeffs_[stage].AppendBiquad(allpass_coeffs_[stage][i]);
        // Convert highpass to allpass.
        // We are getting allpass filter from corresponding highpass by
        // substituting coefficient numerators with flipped denomenators.
        // In case a cascade is first order segment, that is a[2] <= eps,
        // we flip just zero and first order coefficients.
        if (std::abs(allpass_coeffs_[stage][i].a[2]) <= 1e-10) {
          allpass_coeffs_[stage][i].b[0] = -allpass_coeffs_[stage][i].a[1];
          allpass_coeffs_[stage][i].b[1] = -allpass_coeffs_[stage][i].a[0];
          allpass_coeffs_[stage][i].b[2] = 0;
          allpass_coeffs_[stage][i].a[2] = 0;
        } else {
          allpass_coeffs_[stage][i].b[0] = allpass_coeffs_[stage][i].a[2];
          allpass_coeffs_[stage][i].b[1] = allpass_coeffs_[stage][i].a[1];
          allpass_coeffs_[stage][i].b[2] = allpass_coeffs_[stage][i].a[0];
        }
      }
      allpass_coeffs_[stage].Simplify();
      highpass_coeffs_[stage].Simplify();

      const int highpass_size = highpass_coeffs_[stage].size();
      const int allpass_size = allpass_coeffs_[stage].size();
      highpass_filters_[stage].resize(highpass_size);
      allpass_filters_[stage].resize(allpass_size);
      phase_correction_filters_[stage].resize(allpass_size);

      for (int i = 0; i < highpass_size; ++i) {
        InitFilter(highpass_coeffs_[stage][i], &highpass_filters_[stage][i]);
      }

      for (int i = 0; i < allpass_size; ++i) {
        InitFilter(allpass_coeffs_[stage][i], &allpass_filters_[stage][i]);
        InitFilter(allpass_coeffs_[stage][i],
                   &phase_correction_filters_[stage][i]);
      }
    }
  }

  const linear_filters::CrossoverType type_;
  const int order_;
  const int num_bands_;
  // The number of crossovers required to implement the filterbank. See
  // diagram above ProcessBlock().
  const int num_stages_;
  int num_channels_;
  float sample_rate_hz_;
  std::vector<ScalarType> crossover_frequencies_hz_;
  // The successive crossover stages, processed in order, starting with the
  // first.
  using LadderFilterType = linear_filters::LadderFilter<SampleBlockType>;
  // High/Allpass filters coefficients are indexed by stage.
  std::vector<linear_filters::BiquadFilterCascadeCoefficients> highpass_coeffs_;
  std::vector<linear_filters::BiquadFilterCascadeCoefficients> allpass_coeffs_;
  // High/Allpass filters are indexed by stage (each band may see many stages
  // of filters).
  std::vector<std::vector<LadderFilterType>> highpass_filters_;
  std::vector<std::vector<LadderFilterType>> allpass_filters_;
  std::vector<std::vector<LadderFilterType>> phase_correction_filters_;

  // filtered_output_[i] is the filtered output for the ith stage of the
  // cascade.
  std::vector<ArrayBlockType> filtered_output_;
};

}  // namespace audio_dsp

#endif  // AUDIO_DSP_HIFI_MULTI_CROSSOVER_FILTER_H_
