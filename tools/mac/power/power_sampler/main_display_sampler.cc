// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/main_display_sampler.h"

#include "base/memory/ptr_util.h"

extern "C" {

// This sampler imitates the open-source "brightness" tool at
// https://github.com/nriley/brightness.
// Since this sampler doesn't care about older MacOSen, multiple displays
// or other complications that tool has to consider, retrieving the brightness
// level boils down to calling this function for the main display.
extern int DisplayServicesGetBrightness(CGDirectDisplayID id,
                                        float* brightness);
}

namespace power_sampler {

MainDisplaySampler::~MainDisplaySampler() = default;

// static
std::unique_ptr<MainDisplaySampler> MainDisplaySampler::Create() {
  return base::WrapUnique(new MainDisplaySampler(CGMainDisplayID()));
}

std::string MainDisplaySampler::GetName() {
  return kSamplerName;
}

Sampler::DatumNameUnits MainDisplaySampler::GetDatumNameUnits() {
  DatumNameUnits ret;
  // Display brightness is in units of 0-100% of max brightness.
  ret.insert(std::make_pair("brightness", "%"));
  ret.insert(std::make_pair("sleeping", "bool"));
  return ret;
}

Sampler::Sample MainDisplaySampler::GetSample(base::TimeTicks sample_time) {
  Sampler::Sample sample;
  auto result = GetDisplayBrightness();
  if (result.has_value())
    sample.emplace("brightness", result.value() * 100.0);
  sample.emplace("sleeping", GetIsDisplaySleeping());

  return sample;
}

bool MainDisplaySampler::GetIsDisplaySleeping() {
  return CGDisplayIsAsleep(main_display_);
}

std::optional<float> MainDisplaySampler::GetDisplayBrightness() {
  float brightness = 0.0;
  int err = DisplayServicesGetBrightness(main_display_, &brightness);
  if (err != 0)
    return std::nullopt;

  return brightness;
}

MainDisplaySampler::MainDisplaySampler(CGDirectDisplayID main_display)
    : main_display_(main_display) {}

}  // namespace power_sampler
