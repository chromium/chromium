// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/overlay_plane.h"

#include <memory>

namespace ui {

OverlayPlane::OverlayPlane() {}

OverlayPlane::OverlayPlane(scoped_refptr<gfx::NativePixmap> pixmap,
                           std::unique_ptr<gfx::GpuFence> gpu_fence,
                           const gfx::OverlayPlaneData& overlay_plane_data)
    : pixmap(std::move(pixmap)),
      gpu_fence(std::move(gpu_fence)),
      overlay_plane_data(overlay_plane_data) {}

OverlayPlane::OverlayPlane(OverlayPlane&& other) = default;

OverlayPlane& OverlayPlane::operator=(OverlayPlane&& other) = default;

OverlayPlane::~OverlayPlane() {}

}  // namespace ui
