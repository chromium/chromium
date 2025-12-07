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

void FirFilter::Reset() {
  state_.setZero();
}

}  // namespace linear_filters
