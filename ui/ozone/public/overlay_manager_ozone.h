// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OVERLAY_MANAGER_OZONE_H_
#define UI_OZONE_PUBLIC_OVERLAY_MANAGER_OZONE_H_

#include <memory>

#include "ui/gfx/native_widget_types.h"

namespace ui {

class OverlayCandidatesOzone;

// Responsible for providing the oracles used to decide when overlays can be
// used.
class OverlayManagerOzone {
 public:
  virtual ~OverlayManagerOzone() {}

  // Get the hal struct to check for overlay support.
  virtual std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OVERLAY_MANAGER_OZONE_H_
