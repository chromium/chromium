// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/sysmem_native_pixmap.h"

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
  DCHECK(collection_->scenic_overlay_view());
  // TODO(crbug.com/1127984): Present to ScenicOverlayView's ImagePipe. Send
  // ScenicOverlayView's viewholder to the ScenicSurface associated with
  // |widget|.
  NOTIMPLEMENTED();
  return false;
}

gfx::NativePixmapHandle SysmemNativePixmap::ExportHandle() {
  return gfx::CloneHandleForIPC(handle_);
}

bool SysmemNativePixmap::SupportsOverlayPlane(
    gfx::AcceleratedWidget widget) const {
  return collection_->scenic_overlay_view();
}

}  // namespace ui
