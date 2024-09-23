// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_sysmem_native_pixmap.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {

FlatlandSysmemNativePixmap::FlatlandSysmemNativePixmap(
    scoped_refptr<FlatlandSysmemBufferCollection> collection,
    gfx::NativePixmapHandle handle,
    gfx::Size size)
    : collection_(collection), handle_(std::move(handle)), size_(size) {}

FlatlandSysmemNativePixmap::~FlatlandSysmemNativePixmap() = default;

bool FlatlandSysmemNativePixmap::AreDmaBufFdsValid() const {
  return false;
}

int FlatlandSysmemNativePixmap::GetDmaBufFd(size_t plane) const {
  NOTREACHED_IN_MIGRATION();
  return -1;
}

uint32_t FlatlandSysmemNativePixmap::GetDmaBufPitch(size_t plane) const {
  NOTREACHED_IN_MIGRATION();
  return 0u;
}

size_t FlatlandSysmemNativePixmap::GetDmaBufOffset(size_t plane) const {
  NOTREACHED_IN_MIGRATION();
  return 0u;
}

size_t FlatlandSysmemNativePixmap::GetDmaBufPlaneSize(size_t plane) const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

size_t FlatlandSysmemNativePixmap::GetNumberOfPlanes() const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

bool FlatlandSysmemNativePixmap::SupportsZeroCopyWebGPUImport() const {
  NOTREACHED_IN_MIGRATION();
  // TODO(crbug.com/40217759): Figure out how to import multi-planar pixmap into
  // WebGPU without copy.
  return false;
}

uint64_t FlatlandSysmemNativePixmap::GetBufferFormatModifier() const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

gfx::BufferFormat FlatlandSysmemNativePixmap::GetBufferFormat() const {
  return collection_->format();
}

gfx::Size FlatlandSysmemNativePixmap::GetBufferSize() const {
  return size_;
}

uint32_t FlatlandSysmemNativePixmap::GetUniqueId() const {
  return 0;
}

bool FlatlandSysmemNativePixmap::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    const gfx::OverlayPlaneData& overlay_plane_data,
    std::vector<gfx::GpuFence> acquire_fences,
    std::vector<gfx::GpuFence> release_fences) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

gfx::NativePixmapHandle FlatlandSysmemNativePixmap::ExportHandle() const {
  return gfx::CloneHandleForIPC(handle_);
}

const gfx::NativePixmapHandle& FlatlandSysmemNativePixmap::PeekHandle() const {
  return handle_;
}

bool FlatlandSysmemNativePixmap::SupportsOverlayPlane() const {
  return collection_->HasFlatlandImportToken();
}

}  // namespace ui
