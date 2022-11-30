// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/sysmem_native_pixmap.h"

#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/overlay_plane_data.h"

namespace ui {

namespace {

zx::event GpuFenceToZxEvent(gfx::GpuFence fence) {
  DCHECK(!fence.GetGpuFenceHandle().is_null());
  return fence.GetGpuFenceHandle().Clone().owned_event;
}

}  // namespace

SysmemNativePixmap::SysmemNativePixmap(
    scoped_refptr<SysmemBufferCollection> collection,
    gfx::NativePixmapHandle handle,
    gfx::Size size)
    : collection_(collection), handle_(std::move(handle)), size_(size) {}

SysmemNativePixmap::~SysmemNativePixmap() {
  if (overlay_image_id_)
    collection_->scenic_overlay_view()->RemoveImage(overlay_image_id_);
}

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

bool SysmemNativePixmap::SupportsZeroCopyWebGPUImport() const {
  NOTREACHED();
  return false;
}

uint64_t SysmemNativePixmap::GetBufferFormatModifier() const {
  NOTREACHED();
  return 0;
}

gfx::BufferFormat SysmemNativePixmap::GetBufferFormat() const {
  return collection_->format();
}

gfx::Size SysmemNativePixmap::GetBufferSize() const {
  return size_;
}

uint32_t SysmemNativePixmap::GetUniqueId() const {
  return 0;
}

bool SysmemNativePixmap::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    const gfx::OverlayPlaneData& overlay_plane_data,
    std::vector<gfx::GpuFence> acquire_fences,
    std::vector<gfx::GpuFence> release_fences) {
  DCHECK(collection_->scenic_overlay_view());
  ScenicOverlayView* overlay_view = collection_->scenic_overlay_view();

  // Convert gfx::GpuFence to zx::event for PresentImage call.
  std::vector<zx::event> acquire_events;
  for (auto& fence : acquire_fences)
    acquire_events.push_back(GpuFenceToZxEvent(std::move(fence)));
  std::vector<zx::event> release_events;
  for (auto& fence : release_fences)
    release_events.push_back(GpuFenceToZxEvent(std::move(fence)));

  overlay_view->SetBlendMode(overlay_plane_data.enable_blend);

  if (!overlay_image_id_)
    overlay_image_id_ = overlay_view->AddImage(handle_.buffer_index, size_);

  overlay_view->PresentImage(overlay_image_id_, std::move(acquire_events),
                             std::move(release_events));

  return true;
}

gfx::NativePixmapHandle SysmemNativePixmap::ExportHandle() {
  return gfx::CloneHandleForIPC(handle_);
}

const gfx::NativePixmapHandle& SysmemNativePixmap::PeekHandle() const {
  return handle_;
}

bool SysmemNativePixmap::SupportsOverlayPlane() const {
  // We can display an overlay as long as we have a ScenicOverlayView. Note that
  // ScenicOverlayView can migrate from one surface to another, but it can't
  // be used across multiple surfaces similtaneously. But on Fuchsia each buffer
  // collection is allocated (in FuchsiaVideoDecoder) for a specific web frame,
  // and each frame can be displayed only on one specific surface.
  return !!collection_->scenic_overlay_view();
}

ScenicOverlayView* SysmemNativePixmap::GetScenicOverlayView() {
  return collection_->scenic_overlay_view();
}

}  // namespace ui
