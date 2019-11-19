// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

double ConsumeDouble(FuzzedDataProvider* provider) {
  std::vector<uint8_t> v = provider->ConsumeBytes<uint8_t>(sizeof(double));
  if (v.size() == sizeof(double))
    return reinterpret_cast<double*>(v.data())[0];

  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  // Limit width and height for performance.
  int width = provider.ConsumeIntegralInRange<int>(1, 100);
  int height = provider.ConsumeIntegralInRange<int>(1, 100);
  SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
  size_t expected_size = info.computeMinByteSize();

  color_utils::HSL upper_bound = {ConsumeDouble(&provider),
                                  ConsumeDouble(&provider),
                                  ConsumeDouble(&provider)};
  color_utils::HSL lower_bound = {ConsumeDouble(&provider),
                                  ConsumeDouble(&provider),
                                  ConsumeDouble(&provider)};

  bool find_closest = provider.ConsumeBool();

  // Ensure that we have enough data for this image.
  std::vector<uint8_t> image_data =
      provider.ConsumeBytes<uint8_t>(expected_size);
  if (image_data.size() < expected_size)
    return 0;

  SkBitmap bitmap;
  bitmap.installPixels(info, image_data.data(), info.minRowBytes());

  color_utils::CalculateKMeanColorOfBitmap(
      bitmap, provider.ConsumeIntegralInRange<int>(-1, height + 2), lower_bound,
      upper_bound, find_closest);

  return 0;
}
