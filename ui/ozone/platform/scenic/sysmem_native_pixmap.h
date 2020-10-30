// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SYSMEM_NATIVE_PIXMAP_H_
#define UI_OZONE_PLATFORM_SCENIC_SYSMEM_NATIVE_PIXMAP_H_

#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"

namespace ui {

class SysmemNativePixmap : public gfx::NativePixmap {
 public:
  SysmemNativePixmap(scoped_refptr<SysmemBufferCollection> collection,
                     gfx::NativePixmapHandle handle);

  SysmemNativePixmap(const SysmemNativePixmap&) = delete;
  SysmemNativePixmap& operator=(const SysmemNativePixmap&) = delete;

  // gfx::NativePixmap implementation.
  bool AreDmaBufFdsValid() const override;
  int GetDmaBufFd(size_t plane) const override;
  uint32_t GetDmaBufPitch(size_t plane) const override;
  size_t GetDmaBufOffset(size_t plane) const override;
  size_t GetDmaBufPlaneSize(size_t plane) const override;
  size_t GetNumberOfPlanes() const override;
  uint64_t GetBufferFormatModifier() const override;
  gfx::BufferFormat GetBufferFormat() const override;
  gfx::Size GetBufferSize() const override;
  uint32_t GetUniqueId() const override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::vector<gfx::GpuFence> acquire_fences,
                            std::vector<gfx::GpuFence> release_fences) override;
  gfx::NativePixmapHandle ExportHandle() override;

  // Returns true if overlay planes are supported and ScheduleOverlayPlane() can
  // be called.
  bool SupportsOverlayPlane(gfx::AcceleratedWidget widget) const;

 private:
  ~SysmemNativePixmap() override;

  // Keep reference to the collection to make sure it outlives the pixmap.
  scoped_refptr<SysmemBufferCollection> collection_;
  gfx::NativePixmapHandle handle_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SYSMEM_NATIVE_PIXMAP_H_
