// Copyright 2015 The Chromium Authors
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

  bool allow_sync_and_real_buffer_page_flip_testing() const {
    return allow_sync_and_real_buffer_page_flip_testing_;
  }

  // Tell the manager that the overlay delegation is enabled. This is only
  // useful for Wayland as checking for overlay support depends on
  // features::IsDelegatedCompositingEnabled, which cannot be accessed from
  // //ui/ozone.
  // TODO(msisov, petermcneeley): remove this once Wayland uses only delegated
  // context.
  virtual void SetContextDelegated() {}

 protected:
  // TODO(fangzhoug): Some Chrome OS boards still use the legacy video decoder.
  // Remove this once ChromeOSVideoDecoder is on everywhere.
  bool allow_sync_and_real_buffer_page_flip_testing_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OVERLAY_MANAGER_OZONE_H_
