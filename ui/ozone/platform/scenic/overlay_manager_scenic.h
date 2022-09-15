// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_OVERLAY_MANAGER_SCENIC_H_
#define UI_OZONE_PLATFORM_SCENIC_OVERLAY_MANAGER_SCENIC_H_

#include <memory>

#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {

class OverlayManagerScenic : public OverlayManagerOzone {
 public:
  OverlayManagerScenic();
  ~OverlayManagerScenic() override;

  OverlayManagerScenic(const OverlayManagerScenic&) = delete;
  OverlayManagerScenic& operator=(const OverlayManagerScenic&) = delete;

  // OverlayManagerOzone implementation
  std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget widget) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_OVERLAY_MANAGER_SCENIC_H_
