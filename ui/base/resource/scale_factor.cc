// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/scale_factor.h"

#include "base/cxx17_backports.h"

namespace ui {

namespace {

const float kResourceScaleFactorScales[] = {1.0f, 1.0f, 2.0f, 3.0f};
static_assert(NUM_SCALE_FACTORS == base::size(kResourceScaleFactorScales),
              "kScaleFactorScales has incorrect size");

}  // namespace

float GetScaleForResourceScaleFactor(ResourceScaleFactor scale_factor) {
  return kResourceScaleFactorScales[scale_factor];
}

}  // namespace ui
