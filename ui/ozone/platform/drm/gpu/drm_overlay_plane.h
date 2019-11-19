// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_PLANE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_PLANE_H_

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {
class GpuFence;
}

namespace ui {

class DrmFramebuffer;

struct DrmOverlayPlane;
typedef std::vector<DrmOverlayPlane> DrmOverlayPlaneList;

struct DrmOverlayPlane {
  // Simpler constructor for the primary plane.
  explicit DrmOverlayPlane(const scoped_refptr<DrmFramebuffer>& buffer,
                           std::unique_ptr<gfx::GpuFence> gpu_fence);

  DrmOverlayPlane(const scoped_refptr<DrmFramebuffer>& buffer,
                  int z_order,
                  gfx::OverlayTransform plane_transform,
                  const gfx::Rect& display_bounds,
                  const gfx::RectF& crop_rect,
                  bool enable_blend,
                  std::unique_ptr<gfx::GpuFence> gpu_fence);
  DrmOverlayPlane(DrmOverlayPlane&& other);
  DrmOverlayPlane& operator=(DrmOverlayPlane&& other);

  // Returns DrmOverlayPlane will null |buffer| for use as error.
  static DrmOverlayPlane Error();

  bool operator<(const DrmOverlayPlane& plane) const;

  ~DrmOverlayPlane();

  // Returns the primary plane in |overlays|.
  static const DrmOverlayPlane* GetPrimaryPlane(
      const DrmOverlayPlaneList& overlays);

  DrmOverlayPlane Clone() const;

  static std::vector<DrmOverlayPlane> Clone(
      const std::vector<DrmOverlayPlane>& planes);

  scoped_refptr<DrmFramebuffer> buffer;
  int z_order = 0;
  gfx::OverlayTransform plane_transform;
  gfx::Rect display_bounds;
  gfx::RectF crop_rect;
  bool enable_blend;
  std::unique_ptr<gfx::GpuFence> gpu_fence;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_PLANE_H_
