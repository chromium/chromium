// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <algorithm>
#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  // Limit width and height for performance.
  int width = provider.ConsumeIntegralInRange<int>(1, 100);
  int height = provider.ConsumeIntegralInRange<int>(1, 100);
  SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
  size_t expected_size = info.computeMinByteSize();

  const double lower_bound_hue = provider.ConsumeFloatingPointInRange(0.0, 1.0);
  const double upper_bound_hue = provider.ConsumeFloatingPointInRange(
      lower_bound_hue, lower_bound_hue + 1);
  const double s1 = provider.ConsumeFloatingPointInRange(0.0, 1.0);
  const double s2 = provider.ConsumeFloatingPointInRange(0.0, 1.0);
  const double l1 = provider.ConsumeFloatingPointInRange(0.0, 1.0);
  const double l2 = provider.ConsumeFloatingPointInRange(0.0, 1.0);
  color_utils::HSL upper_bound = {upper_bound_hue, std::max(s1, s2),
                                  std::max(l1, l2)};
  color_utils::HSL lower_bound = {lower_bound_hue, std::min(s1, s2),
                                  std::min(l1, l2)};

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
