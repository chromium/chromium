// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_OVERLAY_MANAGER_CAST_H_
#define UI_OZONE_PLATFORM_CAST_OVERLAY_MANAGER_CAST_H_

#include <memory>

#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {

class OverlayManagerCast : public OverlayManagerOzone {
 public:
  OverlayManagerCast();

  OverlayManagerCast(const OverlayManagerCast&) = delete;
  OverlayManagerCast& operator=(const OverlayManagerCast&) = delete;

  ~OverlayManagerCast() override;

  // OverlayManagerOzone:
  std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_OVERLAY_MANAGER_CAST_H_
