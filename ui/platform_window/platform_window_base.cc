// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_base.h"

#include "ui/gfx/geometry/rect.h"

namespace ui {

PlatformWindowBase::PlatformWindowBase() = default;

PlatformWindowBase::~PlatformWindowBase() = default;

bool PlatformWindowBase::ShouldWindowContentsBeTransparent() const {
  return false;
}

void PlatformWindowBase::SetZOrderLevel(ZOrderLevel order) {}

ZOrderLevel PlatformWindowBase::GetZOrderLevel() const {
  return ZOrderLevel::kNormal;
}

void PlatformWindowBase::StackAbove(gfx::AcceleratedWidget widget) {}

void PlatformWindowBase::StackAtTop() {}

void PlatformWindowBase::FlashFrame(bool flash_frame) {}

void PlatformWindowBase::SetShape(std::unique_ptr<ShapeRects> native_shape,
                                  const gfx::Transform& transform) {}

void PlatformWindowBase::SetAspectRatio(const gfx::SizeF& aspect_ratio) {}

void PlatformWindowBase::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                        const gfx::ImageSkia& app_icon) {}

bool PlatformWindowBase::IsAnimatingClosed() const {
  return false;
}

bool PlatformWindowBase::IsTranslucentWindowOpacitySupported() const {
  return false;
}

}  // namespace ui
