// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"

namespace ui {

PalmDetectionFilter::PalmDetectionFilter(
    SharedPalmDetectionFilterState* shared_palm_state)
    : shared_palm_state_(shared_palm_state) {
  DCHECK(shared_palm_state != nullptr);
}

PalmDetectionFilter::~PalmDetectionFilter() {}

}  // namespace ui
