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

#include "audio/dsp/max_filter.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <type_traits>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"

namespace audio_dsp {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

template <typename ValueType>
std::vector<ValueType> NaiveMaxFilter(const std::vector<ValueType>& input,
                                      int window_size) {
  const int leading_radius = (window_size - 1) / 2;
  const int trailing_radius = window_size - leading_radius - 1;
  std::vector<ValueType> output;
  output.reserve(input.size());
  for (int i = 0; i < input.size(); ++i) {
    const int j_start = std::max<int>(0, i - trailing_radius);
    const int j_end = std::min<int>(input.size() - 1, i + leading_radius);
    // Compute the max of {input[j_start], ..., input[j_end]}.
    ValueType max_value = input[j_start];
    for (int j = j_start + 1; j <= j_end; ++j) {
      max_value = std::max<ValueType>(max_value, input[j]);
    }
    output.push_back(max_value);
  }
  return output;
}

template <typename ValueType>
class MaxFilterValueTypeTest : public ::testing::Test {};

typedef ::testing::Types<int, float, double> TestValueTypes;
TYPED_TEST_SUITE(MaxFilterValueTypeTest, TestValueTypes);

TYPED_TEST(MaxFilterValueTypeTest, EmptyInput) {
  typedef TypeParam ValueType;
  std::vector<ValueType> input;
  EXPECT_THAT(MaxFilter(input, 5), IsEmpty());
}

TYPED_TEST(MaxFilterValueTypeTest, RadiusZero) {
  typedef TypeParam ValueType;
  std::vector<ValueType> input({4, -1, 0, 2, 5});
  EXPECT_THAT(MaxFilter(input, 1), ElementsAreArray(input));
}

TYPED_TEST(MaxFilterValueTypeTest, MonotonicInput) {
  typedef TypeParam ValueType;
  constexpr int kNumSamples = 1000;
  std::vector<ValueType> increasing_input;
  std::vector<ValueType> decreasing_input;
  for (int i = 0; i < kNumSamples; ++i) {
    increasing_input.push_back(i);
    decreasing_input.push_back(kNumSamples - 1 - i);
  }
  for (int window_size : {2, 3, 11, 21, 101}) {
    SCOPED_TRACE(absl::StrFormat("window_size: %d", window_size));
    EXPECT_THAT(MaxFilter(increasing_input, window_size),
        ElementsAreArray(NaiveMaxFilter(increasing_input, window_size)));
    EXPECT_THAT(MaxFilter(decreasing_input, window_size),
        ElementsAreArray(NaiveMaxFilter(decreasing_input, window_size)));
  }
}

TYPED_TEST(MaxFilterValueTypeTest, SineWaveInput) {
  typedef TypeParam ValueType;
  constexpr int kNumSamples = 1000;
  std::vector<ValueType> input;
  for (int i = 0; i < kNumSamples; ++i) {
    input.push_back(100 * sin(0.125 * i));
  }
  for (int window_size : {2, 3, 11, 21, 101}) {
    SCOPED_TRACE(absl::StrFormat("window_size: %d", window_size));
    EXPECT_THAT(MaxFilter(input, window_size),
        ElementsAreArray(NaiveMaxFilter(input, window_size)));
  }
}

TYPED_TEST(MaxFilterValueTypeTest, RandomInput) {
  typedef TypeParam ValueType;
  constexpr int kNumSamples = 1000;
  std::mt19937 rng(0);
  typename std::conditional<std::is_integral<ValueType>::value,
      std::uniform_int_distribution<ValueType>,
      std::uniform_real_distribution<ValueType>>::type distribution(-100, 100);
  std::vector<ValueType> input;
  for (int i = 0; i < kNumSamples; ++i) {
    input.push_back(distribution(rng));
  }
  for (int window_size : {2, 3, 11, 21, 101}) {
    SCOPED_TRACE(absl::StrFormat("window_size: %d", window_size));
    EXPECT_THAT(MaxFilter(input, window_size),
        ElementsAreArray(NaiveMaxFilter(input, window_size)));
  }
}

TYPED_TEST(MaxFilterValueTypeTest, RandomInputWithDuplicates) {
  typedef TypeParam ValueType;
  constexpr int kNumSamples = 1000;
  std::mt19937 rng(0);
  typename std::conditional<std::is_integral<ValueType>::value,
      std::uniform_int_distribution<ValueType>,
      std::uniform_real_distribution<ValueType>>::type distribution(-100, 100);
  std::vector<ValueType> input;
  for (int i = 0; i < kNumSamples / 5; ++i) {
    const ValueType sample = distribution(rng);
    for (int repeat = 0; repeat < 5; ++repeat) {
      input.push_back(sample);
    }
  }
  for (int window_size : {2, 3, 11, 21, 101}) {
    SCOPED_TRACE(absl::StrFormat("window_size: %d", window_size));
    EXPECT_THAT(MaxFilter(input, window_size),
        ElementsAreArray(NaiveMaxFilter(input, window_size)));
  }
}

}  // namespace
}  // namespace audio_dsp
