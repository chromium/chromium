// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OVERLAY_PLANE_H_
#define UI_OZONE_PUBLIC_OVERLAY_PLANE_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gfx/overlay_transform.h"

namespace ui {

// Configuration for a hardware overlay plane.
//
// Modern display hardware is capable of transforming and composing multiple
// images into a final fullscreen image. An OverlayPlane represents one such
// image as well as any transformations that must be applied.
struct COMPONENT_EXPORT(OZONE_BASE) OverlayPlane {
  OverlayPlane();
  OverlayPlane(scoped_refptr<gfx::NativePixmap> pixmap,
               std::unique_ptr<gfx::GpuFence> gpu_fence,
               const gfx::OverlayPlaneData& overlay_plane_data);
  OverlayPlane(OverlayPlane&& other);
  OverlayPlane& operator=(OverlayPlane&& other);
  ~OverlayPlane();

  // Image to be presented by the overlay.
  scoped_refptr<gfx::NativePixmap> pixmap;

  // Fence which when signaled marks that writes to |pixmap| have completed.
  std::unique_ptr<gfx::GpuFence> gpu_fence;

  // Specifies necessary data about this overlay plane.
  gfx::OverlayPlaneData overlay_plane_data;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OVERLAY_PLANE_H_
