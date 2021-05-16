// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_EMD_WRAPPER_H_
#define THIRD_PARTY_EMD_WRAPPER_H_

#include <vector>
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace opencv {

struct PointDistribution {
  // The weight of each point.
  std::vector<float> weights;

  // The number of dimensions.
  int dimensions;

  // The positions of each point. Must have the same size as |weights|, and each
  // element must have size |dimensions|.
  std::vector<std::vector<float>> positions;
};

absl::optional<double> EMD(const PointDistribution& distribution1,
                           const PointDistribution& distribution2);
}  // namespace opencv

#endif  // THIRD_PARTY_EMD_WRAPPER_H_
