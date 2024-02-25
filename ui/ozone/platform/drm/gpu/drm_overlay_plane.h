// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_PLANE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_PLANE_H_

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace ui {

class DrmFramebuffer;

struct DrmOverlayPlane;
typedef std::vector<DrmOverlayPlane> DrmOverlayPlaneList;

struct DrmOverlayPlane {
  // Simpler constructor for tests.
  static DrmOverlayPlane TestPlane(
      const scoped_refptr<DrmFramebuffer>& buffer,
      gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB(),
      std::unique_ptr<gfx::GpuFence> gpu_fence = nullptr);

  DrmOverlayPlane(const scoped_refptr<DrmFramebuffer>& buffer,
                  const gfx::ColorSpace& color_space,
                  int z_order,
                  gfx::OverlayTransform plane_transform,
                  const gfx::Rect& damage_rect,
                  const gfx::Rect& display_bounds,
                  const gfx::RectF& crop_rect,
                  bool enable_blend,
                  std::unique_ptr<gfx::GpuFence> gpu_fence);
  DrmOverlayPlane(const scoped_refptr<DrmFramebuffer>& buffer,
                  const gfx::OverlayPlaneData& overlay_plane_data,
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

  // Adds trace records to |context|.
  void WriteIntoTrace(perfetto::TracedValue context) const;

  static std::vector<DrmOverlayPlane> Clone(
      const std::vector<DrmOverlayPlane>& planes);

  scoped_refptr<DrmFramebuffer> buffer;
  gfx::ColorSpace color_space;
  int z_order = 0;
  gfx::OverlayTransform plane_transform;
  gfx::Rect damage_rect;
  gfx::Rect display_bounds;
  gfx::RectF crop_rect;
  bool enable_blend;
  std::unique_ptr<gfx::GpuFence> gpu_fence;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_PLANE_H_
