// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "third_party/opencv/src/emd_wrapper.h"

// Current usage is expected to use at most 512 points in dimension at most 5.
const int kMaxDimension = 10;
const int kMaxPoints = 1000;

opencv::PointDistribution ReadDistributionFromData(const uint8_t** data,
                                                   size_t* size) {
  FuzzedDataProvider data_provider(*data, *size);
  opencv::PointDistribution distribution;

  distribution.dimensions =
      data_provider.ConsumeIntegralInRange(0, kMaxDimension);
  int distribution_size = data_provider.ConsumeIntegralInRange(0, kMaxPoints);
  for (int i = 0; i < distribution_size; i++) {
    distribution.weights.push_back(data_provider.ConsumeFloatingPoint<float>());
    distribution.positions.push_back(std::vector<float>());

    int point_dimension =
        data_provider.ConsumeIntegralInRange(0, kMaxDimension);
    for (int j = 0; j < point_dimension; j++) {
      distribution.positions.back().push_back(
          data_provider.ConsumeFloatingPoint<float>());
    }
  }

  *data += *size - data_provider.remaining_bytes();
  *size = data_provider.remaining_bytes();
  return distribution;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  opencv::PointDistribution distribution1 =
      ReadDistributionFromData(&data, &size);
  opencv::PointDistribution distribution2 =
      ReadDistributionFromData(&data, &size);

  opencv::EMD(distribution1, distribution2);

  return 0;
}
