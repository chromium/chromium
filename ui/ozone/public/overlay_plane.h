// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OVERLAY_PLANE_H_
#define UI_OZONE_PUBLIC_OVERLAY_PLANE_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"
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
               int z_order,
               gfx::OverlayTransform plane_transform,
               const gfx::Rect& display_bounds,
               const gfx::RectF& crop_rect,
               bool enable_blend);
  OverlayPlane(OverlayPlane&& other);
  OverlayPlane& operator=(OverlayPlane&& other);
  ~OverlayPlane();

  // Image to be presented by the overlay.
  scoped_refptr<gfx::NativePixmap> pixmap;

  // Fence which when signaled marks that writes to |pixmap| have completed.
  std::unique_ptr<gfx::GpuFence> gpu_fence;

  // Specifies the stacking order of the plane relative to the main framebuffer
  // located at index 0.
  int z_order = 0;

  // Specifies how the buffer is to be transformed during composition.
  gfx::OverlayTransform plane_transform =
      gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;

  // Pixel bounds within the display to position the image.
  //
  // A fullscreen buffer would use gfx::Rect(display_size_pixels).
  gfx::Rect display_bounds;

  // Normalized bounds of the image to be displayed in |display_bounds|.
  gfx::RectF crop_rect = gfx::RectF(1.f, 1.f);

  // Whether alpha blending should be enabled.
  bool enable_blend = false;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OVERLAY_PLANE_H_
