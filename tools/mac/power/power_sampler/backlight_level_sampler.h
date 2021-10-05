// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_MAC_POWER_POWER_SAMPLER_BACKLIGHT_LEVEL_SAMPLER_H_
#define TOOLS_MAC_POWER_POWER_SAMPLER_BACKLIGHT_LEVEL_SAMPLER_H_

#include <memory>

#include <CoreGraphics/CoreGraphics.h>

#include "tools/mac/power/power_sampler/sampler.h"

namespace power_sampler {

// Samples the backlight level of the main display, if possible.
// Note that this pretty much assumes that the computer under test has a
// single, built-in backlit display.
// Note also that this samples the set level of the backlight, which doesn't
// necessarily mean the display is lit at all. If the display sleeps, the set
// level doesn't change, so it's assumed that this is running under a power
// assertion that prevents display sleep.
class BacklightLevelSampler : public Sampler {
 public:
  ~BacklightLevelSampler() override;

  // Creates and initializes a new sampler, if possible.
  // Returns nullptr on failure.
  static std::unique_ptr<BacklightLevelSampler> Create();

  // Sampler implementation.
  std::string GetName() override;
  DatumNameUnits GetDatumNameUnits() override;
  Sample GetSample(base::TimeTicks sample_time) override;

  using DisplayServicesGetBrightnessFn = int (*)(CGDirectDisplayID, float*);
  static std::unique_ptr<BacklightLevelSampler> CreateForTesting(
      CGDirectDisplayID main_display,
      DisplayServicesGetBrightnessFn brightness_fn);

 private:
  static std::unique_ptr<BacklightLevelSampler> CreateImpl(
      CGDirectDisplayID main_display,
      DisplayServicesGetBrightnessFn brightness_fn);

  BacklightLevelSampler(CGDirectDisplayID main_display,
                        DisplayServicesGetBrightnessFn brightness_fn);

  CGDirectDisplayID main_display_;

  // Test seam.
  const DisplayServicesGetBrightnessFn brightness_fn_;
};

}  // namespace power_sampler

#endif  // TOOLS_MAC_POWER_POWER_SAMPLER_BACKLIGHT_LEVEL_SAMPLER_H_
