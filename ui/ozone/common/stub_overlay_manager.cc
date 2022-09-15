// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/stub_overlay_manager.h"

#include <memory>

#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {

StubOverlayManager::StubOverlayManager() {
}

StubOverlayManager::~StubOverlayManager() {
}

std::unique_ptr<OverlayCandidatesOzone>
StubOverlayManager::CreateOverlayCandidates(gfx::AcceleratedWidget w) {
  return nullptr;
}

}  // namespace ui
