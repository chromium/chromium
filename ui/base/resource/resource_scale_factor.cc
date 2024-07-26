// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/resource/resource_scale_factor.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"

namespace ui {

namespace {

std::vector<ResourceScaleFactor>* g_supported_resource_scale_factors = nullptr;

const float kResourceScaleFactorScales[] = {1.0f, 1.0f, 2.0f, 3.0f};
static_assert(NUM_SCALE_FACTORS == std::size(kResourceScaleFactorScales),
              "kResourceScaleFactorScales has incorrect size");

// The difference to fall back to the smaller scale factor rather than the
// larger one. For example, assume 1.20 is requested but only 1.0 and 2.0 are
// supported. In that case, not fall back to 2.0 but 1.0, and then expand
// the image to 1.20.
const float kFallbackToSmallerScaleDiff = 0.20f;

}  // namespace

float GetScaleForResourceScaleFactor(ResourceScaleFactor scale_factor) {
  return kResourceScaleFactorScales[scale_factor];
}

void SetSupportedResourceScaleFactors(
    const std::vector<ResourceScaleFactor>& scale_factors) {
  CHECK(!scale_factors.empty());

  if (g_supported_resource_scale_factors != nullptr) {
    delete g_supported_resource_scale_factors;
  }

  g_supported_resource_scale_factors =
      new std::vector<ResourceScaleFactor>(scale_factors);
  std::sort(g_supported_resource_scale_factors->begin(),
            g_supported_resource_scale_factors->end(),
            [](ResourceScaleFactor lhs, ResourceScaleFactor rhs) {
              return kResourceScaleFactorScales[lhs] <
                     kResourceScaleFactorScales[rhs];
            });
}

const std::vector<ResourceScaleFactor>& GetSupportedResourceScaleFactors() {
  CHECK_NE(g_supported_resource_scale_factors, nullptr)
      << "ResourceBundle needs to be initialized.";

  return *g_supported_resource_scale_factors;
}

ResourceScaleFactor GetSupportedResourceScaleFactor(float scale) {
  CHECK_NE(g_supported_resource_scale_factors, nullptr)
      << "ResourceBundle needs to be initialized.";

  ResourceScaleFactor closest_match = k100Percent;
  float smallest_diff = std::numeric_limits<float>::max();
  for (const auto scale_factor : *g_supported_resource_scale_factors) {
    const float diff =
        std::abs(kResourceScaleFactorScales[scale_factor] - scale);
    if (diff < smallest_diff) {
      closest_match = scale_factor;
      smallest_diff = diff;
    } else {
      break;
    }
  }

  CHECK_NE(closest_match, kScaleFactorNone);
  return closest_match;
}

ResourceScaleFactor GetSupportedResourceScaleFactorForRescale(float scale) {
  CHECK_NE(g_supported_resource_scale_factors, nullptr)
      << "ResourceBundle needs to be initialized.";

  // Returns an exact match, a smaller scale within
  // `kFallbackToSmallerScaleDiff` units, the nearest larger scale, or the max
  // supported scale.
  for (auto supported_scale : *g_supported_resource_scale_factors) {
    if (kResourceScaleFactorScales[supported_scale] +
            kFallbackToSmallerScaleDiff >=
        scale) {
      return supported_scale;
    }
  }

  return GetMaxSupportedResourceScaleFactor();
}

ResourceScaleFactor GetMaxSupportedResourceScaleFactor() {
  CHECK_NE(g_supported_resource_scale_factors, nullptr)
      << "ResourceBundle needs to be initialized.";

  ResourceScaleFactor max_scale = g_supported_resource_scale_factors->back();
  CHECK_NE(max_scale, kScaleFactorNone);
  return max_scale;
}

float GetScaleForMaxSupportedResourceScaleFactor() {
  return kResourceScaleFactorScales[GetMaxSupportedResourceScaleFactor()];
}

bool IsScaleFactorSupported(ResourceScaleFactor scale_factor) {
  CHECK_NE(g_supported_resource_scale_factors, nullptr)
      << "ResourceBundle needs to be initialized.";

  return base::Contains(*g_supported_resource_scale_factors, scale_factor);
}

namespace test {

ScopedSetSupportedResourceScaleFactors::ScopedSetSupportedResourceScaleFactors(
    const std::vector<ResourceScaleFactor>& new_scale_factors) {
  if (g_supported_resource_scale_factors) {
    original_scale_factors_ =
        std::make_unique<std::vector<ResourceScaleFactor>>(
            *g_supported_resource_scale_factors);
  }
  SetSupportedResourceScaleFactors(new_scale_factors);
}

ScopedSetSupportedResourceScaleFactors::
    ~ScopedSetSupportedResourceScaleFactors() {
  if (original_scale_factors_) {
    SetSupportedResourceScaleFactors(*original_scale_factors_);
  } else {
    delete g_supported_resource_scale_factors;
    g_supported_resource_scale_factors = nullptr;
  }
}

}  // namespace test

}  // namespace ui
