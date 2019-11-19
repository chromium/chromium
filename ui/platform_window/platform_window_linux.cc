// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_linux.h"

namespace ui {

PlatformWindowLinux::PlatformWindowLinux() = default;

PlatformWindowLinux::~PlatformWindowLinux() = default;

bool PlatformWindowLinux::IsSyncExtensionAvailable() const {
  return false;
}

void PlatformWindowLinux::OnCompleteSwapAfterResize() {}

base::Optional<int> PlatformWindowLinux::GetWorkspace() const {
  return base::nullopt;
}

void PlatformWindowLinux::SetVisibleOnAllWorkspaces(bool always_visible) {}

bool PlatformWindowLinux::IsVisibleOnAllWorkspaces() const {
  return false;
}

gfx::Rect PlatformWindowLinux::GetXRootWindowOuterBounds() const {
  return {};
}

bool PlatformWindowLinux::ContainsPointInXRegion(
    const gfx::Point& point) const {
  return false;
}

void PlatformWindowLinux::SetOpacityForXWindow(float opacity) {}

void PlatformWindowLinux::LowerXWindow() {}

}  // namespace ui
