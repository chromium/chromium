// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/native_display_delegate.h"

namespace display {

NativeDisplayDelegate::~NativeDisplayDelegate() {}

void NativeDisplayDelegate::SetColorCalibration(
    int64_t display_id,
    const ColorCalibration& calibration) {}

bool NativeDisplayDelegate::SetColorMatrix(
    int64_t display_id,
    const std::vector<float>& color_matrix) {
  return false;
}

bool NativeDisplayDelegate::SetGammaCorrection(int64_t display_id,
                                               const GammaCurve& degamma,
                                               const GammaCurve& gamma) {
  return false;
}

}  // namespace display
