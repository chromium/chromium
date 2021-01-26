// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_delegate.h"

#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

PlatformWindowDelegate::PlatformWindowDelegate() = default;

PlatformWindowDelegate::~PlatformWindowDelegate() = default;

base::Optional<gfx::Size> PlatformWindowDelegate::GetMinimumSizeForWindow() {
  return base::nullopt;
}

base::Optional<gfx::Size> PlatformWindowDelegate::GetMaximumSizeForWindow() {
  return base::nullopt;
}

SkPath PlatformWindowDelegate::GetWindowMaskForWindowShapeInPixels() {
  return SkPath();
}

}  // namespace ui
