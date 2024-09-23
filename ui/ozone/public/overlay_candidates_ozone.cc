// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/overlay_candidates_ozone.h"

#include "base/notreached.h"

namespace ui {

void OverlayCandidatesOzone::CheckOverlaySupport(
    OverlaySurfaceCandidateList* surfaces) {
  NOTREACHED_IN_MIGRATION();
}

void OverlayCandidatesOzone::ObserveHardwareCapabilities(
    ui::HardwareCapabilitiesCallback receive_callback) {}

OverlayCandidatesOzone::~OverlayCandidatesOzone() = default;

}  // namespace ui
