// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_scale_factor.h"

#include <iterator>

namespace ui {

namespace {

const float kResourceScaleFactorScales[] = {1.0f, 1.0f, 2.0f, 3.0f};
static_assert(NUM_SCALE_FACTORS == std::size(kResourceScaleFactorScales),
              "kScaleFactorScales has incorrect size");

}  // namespace

float GetScaleForResourceScaleFactor(ResourceScaleFactor scale_factor) {
  return kResourceScaleFactorScales[scale_factor];
}

}  // namespace ui
