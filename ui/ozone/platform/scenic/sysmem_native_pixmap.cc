// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/sysmem_native_pixmap.h"

#include "base/logging.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/ozone/platform/scenic/scenic_surface.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

namespace ui {

SysmemNativePixmap::SysmemNativePixmap(
    scoped_refptr<SysmemBufferCollection> collection,
    gfx::NativePixmapHandle handle)
    : collection_(collection), handle_(std::move(handle)) {}

SysmemNativePixmap::~SysmemNativePixmap() = default;

bool SysmemNativePixmap::AreDmaBufFdsValid() const {
  return false;
}

int SysmemNativePixmap::GetDmaBufFd(size_t plane) const {
  NOTREACHED();
  return -1;
}

uint32_t SysmemNativePixmap::GetDmaBufPitch(size_t plane) const {
  NOTREACHED();
  return 0u;
}

size_t SysmemNativePixmap::GetDmaBufOffset(size_t plane) const {
  NOTREACHED();
  return 0u;
}

size_t SysmemNativePixmap::GetDmaBufPlaneSize(size_t plane) const {
  NOTREACHED();
  return 0;
}

size_t SysmemNativePixmap::GetNumberOfPlanes() const {
  NOTREACHED();
  return 0;
}

uint64_t SysmemNativePixmap::GetBufferFormatModifier() const {
  NOTREACHED();
  return 0;
}

gfx::BufferFormat SysmemNativePixmap::GetBufferFormat() const {
  return collection_->format();
}

gfx::Size SysmemNativePixmap::GetBufferSize() const {
  return collection_->size();
}

uint32_t SysmemNativePixmap::GetUniqueId() const {
  return 0;
}

bool SysmemNativePixmap::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  DCHECK(collection_->surface_factory());
  ScenicSurface* surface = collection_->surface_factory()->GetSurface(widget);
  if (!surface) {
    DLOG(ERROR) << "Failed to find surface.";
    return false;
  }

  DCHECK(collection_->scenic_overlay_view());
  ScenicOverlayView* overlay_view = collection_->scenic_overlay_view();
  const auto& buffer_collection_id = handle_.buffer_collection_id.value();
  if (!overlay_view->AttachToScenicSurface(surface, widget,
                                           buffer_collection_id)) {
    DLOG(ERROR) << "Failed to attach to surface.";
    return false;
  }
  surface->UpdateOverlayViewPosition(buffer_collection_id, plane_z_order,
                                     display_bounds);
  overlay_view->SetBlendMode(enable_blend);
  overlay_view->PresentImage(handle_.buffer_index);
  return true;
}

gfx::NativePixmapHandle SysmemNativePixmap::ExportHandle() {
  return gfx::CloneHandleForIPC(handle_);
}

bool SysmemNativePixmap::SupportsOverlayPlane(
    gfx::AcceleratedWidget widget) const {
  if (!collection_->scenic_overlay_view())
    return false;

  return collection_->scenic_overlay_view()->CanAttachToAcceleratedWidget(
      widget);
}

}  // namespace ui
