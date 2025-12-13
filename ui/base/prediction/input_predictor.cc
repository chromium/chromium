// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/prediction/input_predictor.h"

namespace ui {

base::TimeDelta InputPredictor::ResampleLatency(
    base::TimeDelta frame_interval) const {
  // Default implementation returns zero, indicating no resampling offset.
  return base::TimeDelta();
}

}  // namespace ui
