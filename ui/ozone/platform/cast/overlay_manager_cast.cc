// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/overlay_manager_cast.h"

#include <memory>

#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {
namespace {

class OverlayCandidatesCast : public OverlayCandidatesOzone {
 public:
  OverlayCandidatesCast() {}

  void CheckOverlaySupport(OverlaySurfaceCandidateList* surfaces) override {}
};

}  // namespace

OverlayManagerCast::OverlayManagerCast() {
}

OverlayManagerCast::~OverlayManagerCast() {
}

std::unique_ptr<OverlayCandidatesOzone>
OverlayManagerCast::CreateOverlayCandidates(gfx::AcceleratedWidget w) {
  return std::make_unique<OverlayCandidatesCast>();
}

}  // namespace ui
