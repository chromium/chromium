// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/overlay_manager_flatland.h"

#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_native_pixmap.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {
namespace {

class OverlayCandidatesFlatland : public OverlayCandidatesOzone {
 public:
  OverlayCandidatesFlatland() = default;
  ~OverlayCandidatesFlatland() override = default;

  // OverlayCandidatesOzone implementation.
  void CheckOverlaySupport(
      std::vector<OverlaySurfaceCandidate>* candidates) override {
    for (auto& candidate : *candidates) {
      if (!candidate.native_pixmap)
        continue;
      FlatlandSysmemNativePixmap* sysmem_native_pixmap =
          static_cast<FlatlandSysmemNativePixmap*>(
              candidate.native_pixmap.get());
      candidate.overlay_handled = sysmem_native_pixmap->SupportsOverlayPlane();
    }
  }
};

}  // namespace

OverlayManagerFlatland::OverlayManagerFlatland() {
  // Fuchsia overlays rely on ShouldUseRealBuffersForPageFlipTest.
  allow_sync_and_real_buffer_page_flip_testing_ = true;
}

OverlayManagerFlatland::~OverlayManagerFlatland() = default;

std::unique_ptr<OverlayCandidatesOzone>
OverlayManagerFlatland::CreateOverlayCandidates(gfx::AcceleratedWidget widget) {
  return std::make_unique<OverlayCandidatesFlatland>();
}

}  // namespace ui
