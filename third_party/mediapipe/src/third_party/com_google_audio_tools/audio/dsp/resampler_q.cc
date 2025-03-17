/*
 * Copyright 2021 Google LLC
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

#include "audio/dsp/resampler_q.h"

#include "audio/dsp/number_util.h"
#include "audio/dsp/portable/rational_factor_resampler_kernel.h"

// #include "third_party/audio_to_tactile/src/dsp/number_util.h"
// #include "third_party/audio_to_tactile/src/dsp/q_resampler_kernel.h"

namespace audio_dsp::qresampler_internal {

template <typename CoeffType>
QResamplerFilters<CoeffType>::QResamplerFilters()
    : factor_numerator_(1), factor_denominator_(1), radius_(0), num_taps_(1) {}

template <typename CoeffType>
QResamplerFilters<CoeffType>::QResamplerFilters(
    float input_sample_rate, float output_sample_rate,
    const QResamplerParams& params) {
  Init(input_sample_rate, output_sample_rate, params);
}

template <typename CoeffType>
bool QResamplerFilters<CoeffType>::Init(float input_sample_rate,
                                        float output_sample_rate,
                                        const QResamplerParams& params) {
  ::RationalFactorResamplerKernel kernel;
  if (!::RationalFactorResamplerKernelInit(
          &kernel, input_sample_rate, output_sample_rate,
          /*filter_radius_factor=*/params.filter_radius_factor,
          /*cutoff_proportion=*/params.cutoff_proportion,
          /*kaiser_beta=*/params.kaiser_beta) ||
      params.max_denominator <= 0) {
    coeffs_.clear();
    factor_numerator_ = 1;
    factor_denominator_ = 1;
    radius_ = 0;
    num_taps_ = 1;
    return false;
  }

  radius_ = static_cast<int>(std::ceil(kernel.radius));
  // We create the polyphase filters h_p by sampling the kernel h(x) as
  //
  //   h_p[k] := h(p/b + k),  p = 0, 1, ..., b - 1,
  //
  // as described in the .h file. Since h(x) is nonzero for |x| <= radius,
  // h_p[k] is nonzero when |p/b + k| <= radius, or
  //
  //   -radius - p/b <= k <= radius - p/b.
  //
  // Independently of p, the nonzero support of h_p[k] is within
  //
  //   -radius - (b - 1)/b <= k <= radius.
  //
  // Since k and radius are integers, we can round the lower bound up to
  // conclude the nonzero support is within |k| <= radius. Therefore, we sample
  // h(p/b + k) for |k| <= radius, and the number of taps is 2 * radius + 1.
  num_taps_ = 2 * radius_ + 1;
  // Approximate resampling factor as a rational number, > 1 if downsampling.
  const std::pair<int, int> rational =
      RationalApproximation(kernel.factor, params.max_denominator);
  factor_numerator_ = rational.first;
  factor_denominator_ = rational.second;
  factor_floor_ = factor_numerator_ / factor_denominator_;  // Integer divide.
  phase_step_ = factor_numerator_ % factor_denominator_;

  // Compute polyphase resampling filter coefficients.
  coeffs_.resize(factor_denominator_);
  for (int phase = 0; phase < factor_denominator_; ++phase) {
    auto& filter = coeffs_[phase];
    filter.resize(num_taps_);
    const double offset = static_cast<double>(phase) / factor_denominator_;
    for (int k = -radius_; k <= radius_; ++k) {
      // Store filter backwards so that convolution becomes a dot product.
      filter[radius_ - k] = static_cast<CoeffType>(
          ::RationalFactorResamplerKernelEval(&kernel, offset + k));
    }
  }

  return true;
}

template class QResamplerFilters<float>;
template class QResamplerFilters<double>;

}  // namespace audio_dsp::qresampler_internal
