// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"

#include <gbm.h>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"

namespace ui {

GbmPixmap::GbmPixmap(GbmSurfaceFactory* surface_manager,
                     std::unique_ptr<GbmBuffer> buffer,
                     scoped_refptr<DrmFramebuffer> framebuffer)
    : surface_manager_(surface_manager),
      buffer_(std::move(buffer)),
      framebuffer_(std::move(framebuffer)) {}

gfx::NativePixmapHandle GbmPixmap::ExportHandle() {
  return buffer_->ExportHandle();
}

bool GbmPixmap::AreDmaBufFdsValid() const {
  return buffer_->AreFdsValid();
}

int GbmPixmap::GetDmaBufFd(size_t plane) const {
  return buffer_->GetPlaneFd(plane);
}

uint32_t GbmPixmap::GetDmaBufPitch(size_t plane) const {
  return buffer_->GetPlaneStride(plane);
}

size_t GbmPixmap::GetDmaBufOffset(size_t plane) const {
  return buffer_->GetPlaneOffset(plane);
}

size_t GbmPixmap::GetDmaBufPlaneSize(size_t plane) const {
  return buffer_->GetPlaneSize(plane);
}

size_t GbmPixmap::GetNumberOfPlanes() const {
  return buffer_->GetNumPlanes();
}

uint64_t GbmPixmap::GetBufferFormatModifier() const {
  return buffer_->GetFormatModifier();
}

gfx::BufferFormat GbmPixmap::GetBufferFormat() const {
  return buffer_->GetBufferFormat();
}

gfx::Size GbmPixmap::GetBufferSize() const {
  return buffer_->GetSize();
}

uint32_t GbmPixmap::GetUniqueId() const {
  return buffer_->GetHandle();
}

bool GbmPixmap::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                     int plane_z_order,
                                     gfx::OverlayTransform plane_transform,
                                     const gfx::Rect& display_bounds,
                                     const gfx::RectF& crop_rect,
                                     bool enable_blend,
                                     std::unique_ptr<gfx::GpuFence> gpu_fence) {
  DCHECK(buffer_->GetFlags() & GBM_BO_USE_SCANOUT);
  // |framebuffer_id| might be 0 if AddFramebuffer2 failed, in that case we
  // already logged the error in GbmBuffer ctor. We avoid logging the error
  // here since this method might be called every pageflip.
  if (framebuffer_) {
    surface_manager_->GetSurface(widget)->QueueOverlayPlane(DrmOverlayPlane(
        framebuffer_, plane_z_order, plane_transform, display_bounds, crop_rect,
        enable_blend, std::move(gpu_fence)));
  }

  return true;
}

GbmPixmap::~GbmPixmap() {}

}  // namespace ui
