// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/overlay_plane.h"

#include <memory>

namespace ui {

OverlayPlane::OverlayPlane() {}

OverlayPlane::OverlayPlane(scoped_refptr<gfx::NativePixmap> pixmap,
                           std::unique_ptr<gfx::GpuFence> gpu_fence,
                           int z_order,
                           gfx::OverlayTransform plane_transform,
                           const gfx::Rect& display_bounds,
                           const gfx::RectF& crop_rect,
                           bool enable_blend)
    : pixmap(std::move(pixmap)),
      gpu_fence(std::move(gpu_fence)),
      z_order(z_order),
      plane_transform(plane_transform),
      display_bounds(display_bounds),
      crop_rect(crop_rect),
      enable_blend(enable_blend) {}

OverlayPlane::OverlayPlane(OverlayPlane&& other) = default;

OverlayPlane& OverlayPlane::operator=(OverlayPlane&& other) = default;

OverlayPlane::~OverlayPlane() {}

}  // namespace ui
