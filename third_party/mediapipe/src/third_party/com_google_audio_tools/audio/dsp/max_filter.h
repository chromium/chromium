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

// Max filtering with constant extrapolation boundary handling.
// These functions implement efficient max filtering where for an input x(i) and
// window radius r, the output y(i) is
//
//   y(i) = max {x(i - r), x(i - r + 1), ..., x(i + r)}.
//
// For samples beyond the boundaries, the input signal is extended by constant
// extrapolation, x(i) := x(0) for i < 0 and x(i) := x(num_samples - 1) for
// i >= num_samples.
//
// The response is symmetric for an odd window width w = 2 * r + 1. With an even
// widow width, the trailing (left) side has one more sample,
//
//   y(i) = max {x(i - ceil((w - 1)/2.0), ..., x(i + floor((w - 1)/2.0))}.
//
// The running max is efficiently tracked using a deque, as described in
//   Daniel Lemire, Streaming Maximum-Minimum Filter Using No More than Three
//   Comparisons per Element, Nordic Journal of Computing, Volume 13, Number 4,
//   pages 328-339, 2006. [http://arxiv.org/pdf/cs/0610046v5.pdf]
// The algorithm traverses the input x(i) one sample at a time from left to
// right. The order of recent input samples is tracked in a "monotonic wedge," a
// deque of indices. On the ith iteration, the window is {i - w + 1, ..., i},
// and the wedge is updated such that
//   i - w < wedge[0] < wedge[1] < ... < wedge.back() = i
// and
//   x(wedge[0]) > x(wedge[1]) > ... > x(wedge.back()).
// The maximum over the window is x(wedge[0]), which determines output sample
// y(i - floor((w - 1)/2.0)).
//
// The computational complexity is linear in the number of input samples. The
// window size only affects the complexity for initialization on the boundary.
// The cost for interior samples is constant with respect to window size.

#ifndef AUDIO_DSP_MAX_FILTER_H_
#define AUDIO_DSP_MAX_FILTER_H_

#include <algorithm>
#include <deque>
#include <vector>

#include "glog/logging.h"

namespace audio_dsp {

template <typename ValueType>
void MaxFilter(const ValueType* input, int num_samples, int window_size,
               ValueType* output);

// Apply max filter of size window_size to input. The output has the same size
// as the input and the ith output sample is
//   max {input[i - ceil((w - 1)/2.0)], ..., input[i + floor((w - 1)/2.0)]},
// where w := window_size. For samples beyond the boundaries, the input signal
// is extrapolated with constant extension.
template <typename ValueType>
std::vector<ValueType> MaxFilter(const std::vector<ValueType>& input,
                                 int window_size) {
  std::vector<ValueType> output(input.size());
  MaxFilter(input.data(),
            static_cast<int>(input.size()), window_size, output.data());
  return output;
}

// Raw pointer interface. It is the caller's responsibility to ensure that input
// has size num_samples and to allocate output with space for num_samples
// results.
template <typename ValueType>
void MaxFilter(const ValueType* input, int num_samples, int window_size,
               ValueType* output) {
  ABSL_CHECK_GE(num_samples, 0);
  ABSL_CHECK_GE(window_size, 1);
  if (num_samples == 0) {
    return;
  }
  ABSL_CHECK(input);
  ABSL_CHECK(output);
  // The max filter window of size window_size is centered so that output[i] is
  //   max {input[i - trailing_radius], ..., input[i + leading_radius]},
  // where the leading_radius = floor((window_size - 1)/2.0) and trailing_radius
  // = ceil((window_size - 1)/2.0). The leading_radius is the number of input
  // samples that the filter looks ahead of the current output sample.
  const int leading_radius = (window_size - 1) / 2;  // Integer division.
  // The wedge is a queue of indices within the current window. The queue is
  // maintained such that i - window_size < wedge[0] < ... < wedge.back() = i
  // and input[wedge[0]] > ... > input[wedge.back()].
  std::deque<int> wedge;
  for (int i = 0; i < num_samples + leading_radius; ++i) {
    // The current max filter window is {i - window_size + 1, ..., i}. Remove
    // old wedge indices that have fallen behind the window.
    if (!wedge.empty() && wedge.front() <= i - window_size) {
      wedge.pop_front();
    }
    if (i < num_samples) {
      // Remove all wedge indices where the input is <= input[i].
      while (!wedge.empty() && input[wedge.back()] <= input[i]) {
        wedge.pop_back();
      }
      wedge.push_back(i);
    }
    if (i >= leading_radius) {
      output[i - leading_radius] = input[wedge.front()];
    }
  }
}

// Raw pointer interface. It is the caller's responsibility to ensure that input
// has size num_samples and to allocate output with space for num_samples
// results. Additionally for each input sample, returns the index of maximum
// value for precision.

template <typename ValueType>
void MaxFilter(const ValueType* input, int num_samples, int window_size,
               ValueType* output, std::vector<int>* max_index) {
  ABSL_CHECK_GE(num_samples, 0);
  ABSL_CHECK_GE(window_size, 1);
  if (num_samples == 0) {
    return;
  }
  ABSL_CHECK(input);
  ABSL_CHECK(output);
  // The max filter window of size window_size is centered so that output[i] is
  //   max {input[i - trailing_radius], ..., input[i + leading_radius]},
  // where the leading_radius = floor((window_size - 1)/2.0) and trailing_radius
  // = ceil((window_size - 1)/2.0). The leading_radius is the number of input
  // samples that the filter looks ahead of the current output sample.
  const int leading_radius = (window_size - 1) / 2;  // Integer division.
  // The wedge is a queue of indices within the current window. The queue is
  // maintained such that i - window_size < wedge[0] < ... < wedge.back() = i
  // and input[wedge[0]] > ... > input[wedge.back()].
  std::deque<int> wedge;
  for (int i = 0; i < num_samples + leading_radius; ++i) {
    // The current max filter window is {i - window_size + 1, ..., i}. Remove
    // old wedge indices that have fallen behind the window.
    if (!wedge.empty() && wedge.front() <= i - window_size) {
      wedge.pop_front();
    }
    if (i < num_samples) {
      // Remove all wedge indices where the input is <= input[i].
      while (!wedge.empty() && input[wedge.back()] <= input[i]) {
        wedge.pop_back();
      }
      wedge.push_back(i);
    }
    if (i >= leading_radius) {
      output[i - leading_radius] = input[wedge.front()];
      (*max_index)[i - leading_radius] = wedge.front();
    }
  }
}

}  // namespace audio_dsp

#endif  // AUDIO_DSP_MAX_FILTER_H_
