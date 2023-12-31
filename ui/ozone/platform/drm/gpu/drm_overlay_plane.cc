// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"

namespace ui {

namespace {

std::unique_ptr<gfx::GpuFence> CloneGpuFence(
    const std::unique_ptr<gfx::GpuFence>& gpu_fence) {
  if (!gpu_fence)
    return nullptr;
  return std::make_unique<gfx::GpuFence>(
      gpu_fence->GetGpuFenceHandle().Clone());
}

}  // namespace

DrmOverlayPlane DrmOverlayPlane::TestPlane(
    const scoped_refptr<DrmFramebuffer>& buffer,
    gfx::ColorSpace color_space,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  return DrmOverlayPlane(buffer, color_space, 0, gfx::OVERLAY_TRANSFORM_NONE,
                         gfx::Rect(buffer->size()),
                         gfx::Rect(gfx::Point(), buffer->size()),
                         gfx::RectF(0, 0, 1, 1), false, std::move(gpu_fence));
}

DrmOverlayPlane::DrmOverlayPlane(const scoped_refptr<DrmFramebuffer>& buffer,
                                 const gfx::ColorSpace& color_space,
                                 int z_order,
                                 gfx::OverlayTransform plane_transform,
                                 const gfx::Rect& damage_rect,
                                 const gfx::Rect& display_bounds,
                                 const gfx::RectF& crop_rect,
                                 bool enable_blend,
                                 std::unique_ptr<gfx::GpuFence> gpu_fence)
    : buffer(buffer),
      color_space(color_space),
      z_order(z_order),
      plane_transform(plane_transform),
      damage_rect(damage_rect),
      display_bounds(display_bounds),
      crop_rect(crop_rect),
      enable_blend(enable_blend),
      gpu_fence(std::move(gpu_fence)) {}

DrmOverlayPlane::DrmOverlayPlane(
    const scoped_refptr<DrmFramebuffer>& buffer,
    const gfx::OverlayPlaneData& overlay_plane_data,
    std::unique_ptr<gfx::GpuFence> gpu_fence)
    : DrmOverlayPlane(
          buffer,
          overlay_plane_data.color_space,
          overlay_plane_data.z_order,
          absl::get<gfx::OverlayTransform>(overlay_plane_data.plane_transform),
          overlay_plane_data.damage_rect,
          gfx::ToNearestRect(overlay_plane_data.display_bounds),
          overlay_plane_data.crop_rect,
          overlay_plane_data.enable_blend,
          std::move(gpu_fence)) {}

DrmOverlayPlane::DrmOverlayPlane(DrmOverlayPlane&& other) = default;

DrmOverlayPlane& DrmOverlayPlane::operator=(DrmOverlayPlane&& other) = default;

DrmOverlayPlane::~DrmOverlayPlane() = default;

// static
DrmOverlayPlane DrmOverlayPlane::Error() {
  return DrmOverlayPlane(nullptr, gfx::ColorSpace(), 0,
                         gfx::OVERLAY_TRANSFORM_INVALID, gfx::Rect(),
                         gfx::Rect(), gfx::RectF(),
                         /* enable_blend */ true, /* gpu_fence */ nullptr);
}

bool DrmOverlayPlane::operator<(const DrmOverlayPlane& plane) const {
  return std::tie(z_order, color_space, damage_rect, display_bounds, crop_rect,
                  plane_transform) <
         std::tie(plane.z_order, plane.color_space, plane.damage_rect,
                  plane.display_bounds, plane.crop_rect, plane.plane_transform);
}

// static
const DrmOverlayPlane* DrmOverlayPlane::GetPrimaryPlane(
    const DrmOverlayPlaneList& overlays) {
  for (const auto& overlay : overlays) {
    if (overlay.z_order == 0) {
      return &overlay;
    }
  }

  return nullptr;
}

DrmOverlayPlane DrmOverlayPlane::Clone() const {
  return DrmOverlayPlane(buffer, color_space, z_order, plane_transform,
                         damage_rect, display_bounds, crop_rect, enable_blend,
                         CloneGpuFence(gpu_fence));
}

void DrmOverlayPlane::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();

  dict.Add("framebuffer_id", buffer ? buffer->framebuffer_id() : -1);
  dict.Add("color_space", color_space.ToString());
  dict.Add("z_order", z_order);
  dict.Add("plane_transform", plane_transform);
  dict.Add("damage_rect", damage_rect.ToString());
  dict.Add("display_bounds", display_bounds.ToString());
  dict.Add("crop_rect", crop_rect.ToString());
  dict.Add("enable_blend", enable_blend);
  dict.Add("has_fence", !!gpu_fence);
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
