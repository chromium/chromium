// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window.h"

#include <string>

#include "ui/gfx/geometry/rect.h"

namespace ui {

PlatformWindow::PlatformWindow() = default;

PlatformWindow::~PlatformWindow() = default;

bool PlatformWindow::ShouldWindowContentsBeTransparent() const {
  return false;
}

void PlatformWindow::SetZOrderLevel(ZOrderLevel order) {}

ZOrderLevel PlatformWindow::GetZOrderLevel() const {
  return ZOrderLevel::kNormal;
}

void PlatformWindow::StackAbove(gfx::AcceleratedWidget widget) {}

void PlatformWindow::StackAtTop() {}

void PlatformWindow::FlashFrame(bool flash_frame) {}

void PlatformWindow::SetShape(std::unique_ptr<ShapeRects> native_shape,
                              const gfx::Transform& transform) {}

void PlatformWindow::SetAspectRatio(const gfx::SizeF& aspect_ratio) {}

void PlatformWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                    const gfx::ImageSkia& app_icon) {}

bool PlatformWindow::IsAnimatingClosed() const {
  return false;
}

void PlatformWindow::SetOpacity(float opacity) {}

void PlatformWindow::SetVisibilityChangedAnimationsEnabled(bool enabled) {}

std::string PlatformWindow::GetWindowUniqueId() const {
  return std::string();
}

bool PlatformWindow::ShouldUpdateWindowShape() const {
  return false;
}

bool PlatformWindow::CanSetDecorationInsets() const {
  return false;
}

void PlatformWindow::SetVideoCapture() {}

void PlatformWindow::ReleaseVideoCapture() {}

void PlatformWindow::SetOpaqueRegion(
    std::optional<std::vector<gfx::Rect>> region_px) {}

void PlatformWindow::SetInputRegion(
    std::optional<std::vector<gfx::Rect>> region_px) {}

bool PlatformWindow::IsClientControlledWindowMovementSupported() const {
  return true;
}

void PlatformWindow::NotifyStartupComplete(const std::string& startup_id) {}

}  // namespace ui
