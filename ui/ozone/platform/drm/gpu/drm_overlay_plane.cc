// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "ui/gfx/gpu_fence.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"

namespace ui {

namespace {

std::unique_ptr<gfx::GpuFence> CloneGpuFence(
    const std::unique_ptr<gfx::GpuFence>& gpu_fence) {
  if (!gpu_fence)
    return nullptr;
  return std::make_unique<gfx::GpuFence>(
      gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle()));
}

}  // namespace

DrmOverlayPlane::DrmOverlayPlane(const scoped_refptr<DrmFramebuffer>& buffer,
                                 std::unique_ptr<gfx::GpuFence> gpu_fence)
    : buffer(buffer),
      plane_transform(gfx::OVERLAY_TRANSFORM_NONE),
      display_bounds(gfx::Point(), buffer->size()),
      crop_rect(0, 0, 1, 1),
      enable_blend(false),
      gpu_fence(std::move(gpu_fence)) {}

DrmOverlayPlane::DrmOverlayPlane(const scoped_refptr<DrmFramebuffer>& buffer,
                                 int z_order,
                                 gfx::OverlayTransform plane_transform,
                                 const gfx::Rect& display_bounds,
                                 const gfx::RectF& crop_rect,
                                 bool enable_blend,
                                 std::unique_ptr<gfx::GpuFence> gpu_fence)
    : buffer(buffer),
      z_order(z_order),
      plane_transform(plane_transform),
      display_bounds(display_bounds),
      crop_rect(crop_rect),
      enable_blend(enable_blend),
      gpu_fence(std::move(gpu_fence)) {}

DrmOverlayPlane::DrmOverlayPlane(DrmOverlayPlane&& other) = default;

DrmOverlayPlane& DrmOverlayPlane::operator=(DrmOverlayPlane&& other) = default;

DrmOverlayPlane::~DrmOverlayPlane() {}

// static
DrmOverlayPlane DrmOverlayPlane::Error() {
  return DrmOverlayPlane(nullptr, 0, gfx::OVERLAY_TRANSFORM_INVALID,
                         gfx::Rect(), gfx::RectF(), /* enable_blend */ true,
                         /* gpu_fence */ nullptr);
}

bool DrmOverlayPlane::operator<(const DrmOverlayPlane& plane) const {
  return std::tie(z_order, display_bounds, crop_rect, plane_transform) <
         std::tie(plane.z_order, plane.display_bounds, plane.crop_rect,
                  plane.plane_transform);
}

// static
const DrmOverlayPlane* DrmOverlayPlane::GetPrimaryPlane(
    const DrmOverlayPlaneList& overlays) {
  for (size_t i = 0; i < overlays.size(); ++i) {
    if (overlays[i].z_order == 0)
      return &overlays[i];
  }

  return nullptr;
}

DrmOverlayPlane DrmOverlayPlane::Clone() const {
  return DrmOverlayPlane(buffer, z_order, plane_transform, display_bounds,
                         crop_rect, enable_blend, CloneGpuFence(gpu_fence));
}

// static
std::vector<DrmOverlayPlane> DrmOverlayPlane::Clone(
    const std::vector<DrmOverlayPlane>& planes) {
  std::vector<DrmOverlayPlane> cloned_planes;
  cloned_planes.reserve(planes.size());
  for (auto& plane : planes)
    cloned_planes.push_back(plane.Clone());
  return cloned_planes;
}

}  // namespace ui
