// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/overlay_manager_scenic.h"

#include "base/logging.h"
#include "ui/ozone/platform/scenic/sysmem_native_pixmap.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {
namespace {

class OverlayCandidatesScenic : public OverlayCandidatesOzone {
 public:
  OverlayCandidatesScenic() = default;

  void CheckOverlaySupport(OverlaySurfaceCandidateList* candidates) override {
    for (auto& candidate : *candidates) {
      if (!candidate.native_pixmap)
        continue;
      SysmemNativePixmap* sysmem_native_pixmap =
          reinterpret_cast<SysmemNativePixmap*>(candidate.native_pixmap.get());
      candidate.overlay_handled = sysmem_native_pixmap->SupportsOverlayPlane();
    }
  }
};

}  // namespace

OverlayManagerScenic::OverlayManagerScenic() {
  // Fuchsia overlays rely on ShouldUseRealBuffersForPageFlipTest.
  allow_sync_and_real_buffer_page_flip_testing_ = true;
}

OverlayManagerScenic::~OverlayManagerScenic() = default;

std::unique_ptr<OverlayCandidatesOzone>
OverlayManagerScenic::CreateOverlayCandidates(gfx::AcceleratedWidget widget) {
  return std::make_unique<OverlayCandidatesScenic>();
}

}  // namespace ui
