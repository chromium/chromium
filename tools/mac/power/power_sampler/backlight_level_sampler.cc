// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/backlight_level_sampler.h"

#include "base/memory/ptr_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

namespace {
constexpr char kSamplerName[] = "BacklightLevel";

absl::optional<float> GetBrightnessForDisplay(
    int(brightness_fn)(CGDirectDisplayID id, float* brightness),
    CGDirectDisplayID display) {
  float result = 0.0;

  int err = brightness_fn(display, &result);
  if (err != 0)
    return absl::nullopt;

  return result;
}

}  // namespace

BacklightLevelSampler::~BacklightLevelSampler() = default;

// static
std::unique_ptr<BacklightLevelSampler> BacklightLevelSampler::Create() {
  return CreateImpl(CGMainDisplayID(), &DisplayServicesGetBrightness);
}

std::string BacklightLevelSampler::GetName() {
  return kSamplerName;
}

Sampler::DatumNameUnits BacklightLevelSampler::GetDatumNameUnits() {
  DatumNameUnits ret;
  // Display brightness is in units of 0-100% of max brightness.
  ret.insert(std::make_pair("display_brightness", "%"));
  return ret;
}

Sampler::Sample BacklightLevelSampler::GetSample(base::TimeTicks sample_time) {
  Sampler::Sample sample;
  auto brightness = GetBrightnessForDisplay(brightness_fn_, main_display_);
  if (brightness.has_value())
    sample.emplace("display_brightness", brightness.value() * 100.0);

  return sample;
}

// static
std::unique_ptr<BacklightLevelSampler> BacklightLevelSampler::CreateForTesting(
    CGDirectDisplayID main_display,
    DisplayServicesGetBrightnessFn brightness_fn) {
  return CreateImpl(main_display, brightness_fn);
}

// static
std::unique_ptr<BacklightLevelSampler> BacklightLevelSampler::CreateImpl(
    CGDirectDisplayID main_display,
    DisplayServicesGetBrightnessFn brightness_fn) {
  auto brightness = GetBrightnessForDisplay(brightness_fn, main_display);
  if (!brightness.has_value())
    return nullptr;

  return base::WrapUnique(
      new BacklightLevelSampler(main_display, brightness_fn));
}

BacklightLevelSampler::BacklightLevelSampler(
    CGDirectDisplayID main_display,
    DisplayServicesGetBrightnessFn brightness_fn)
    : main_display_(main_display), brightness_fn_(brightness_fn) {}

}  // namespace power_sampler
