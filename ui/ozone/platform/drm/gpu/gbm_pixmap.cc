// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"

#include <gbm.h>
#include <memory>
#include <utility>

#include "base/check.h"
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

gfx::NativePixmapHandle GbmPixmap::ExportHandle() const {
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

bool GbmPixmap::SupportsZeroCopyWebGPUImport() const {
  return buffer_->SupportsZeroCopyWebGPUImport();
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

bool GbmPixmap::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    const gfx::OverlayPlaneData& overlay_plane_data,
    std::vector<gfx::GpuFence> acquire_fences,
    std::vector<gfx::GpuFence> release_fences) {
  DCHECK(buffer_->GetFlags() & GBM_BO_USE_SCANOUT);
  // |framebuffer_id| might be 0 if AddFramebuffer2 failed, in that case we
  // already logged the error in GbmBuffer ctor. We avoid logging the error
  // here since this method might be called every pageflip.
  if (framebuffer_) {
    DCHECK(acquire_fences.empty() || acquire_fences.size() == 1u);
    surface_manager_->GetSurface(widget)->QueueOverlayPlane(DrmOverlayPlane(
        framebuffer_, overlay_plane_data,
        acquire_fences.empty()
            ? nullptr
            : std::make_unique<gfx::GpuFence>(std::move(acquire_fences[0]))));
  }

  return true;
}

GbmPixmap::~GbmPixmap() = default;

}  // namespace ui
