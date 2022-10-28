// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SYSMEM_NATIVE_PIXMAP_H_
#define UI_OZONE_PLATFORM_SCENIC_SYSMEM_NATIVE_PIXMAP_H_

#include <lib/zx/eventpair.h>

#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"

namespace ui {

class ScenicOverlayView;

class SysmemNativePixmap : public gfx::NativePixmap {
 public:
  SysmemNativePixmap(scoped_refptr<SysmemBufferCollection> collection,
                     gfx::NativePixmapHandle handle,
                     gfx::Size size);

  SysmemNativePixmap(const SysmemNativePixmap&) = delete;
  SysmemNativePixmap& operator=(const SysmemNativePixmap&) = delete;

  // gfx::NativePixmap implementation.
  bool AreDmaBufFdsValid() const override;
  int GetDmaBufFd(size_t plane) const override;
  uint32_t GetDmaBufPitch(size_t plane) const override;
  size_t GetDmaBufOffset(size_t plane) const override;
  size_t GetDmaBufPlaneSize(size_t plane) const override;
  size_t GetNumberOfPlanes() const override;
  bool SupportsZeroCopyWebGPUImport() const override;
  uint64_t GetBufferFormatModifier() const override;
  gfx::BufferFormat GetBufferFormat() const override;
  gfx::Size GetBufferSize() const override;
  uint32_t GetUniqueId() const override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            const gfx::OverlayPlaneData& overlay_plane_data,
                            std::vector<gfx::GpuFence> acquire_fences,
                            std::vector<gfx::GpuFence> release_fences) override;
  gfx::NativePixmapHandle ExportHandle() override;

  const gfx::NativePixmapHandle& PeekHandle() const;

  // Returns true if overlay planes are supported and ScheduleOverlayPlane() can
  // be called.
  bool SupportsOverlayPlane() const;

  // Returns true ScenicOverlayView for the pixmap if any.
  ScenicOverlayView* GetScenicOverlayView();

 private:
  ~SysmemNativePixmap() override;

  scoped_refptr<SysmemBufferCollection> collection_;
  gfx::NativePixmapHandle handle_;
  gfx::Size size_;

  // ID of the image registered with the `ImagePipe` owned by the
  // `ScenicOverlayView` that corresponds to the `collection_`.
  uint32_t overlay_image_id_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SYSMEM_NATIVE_PIXMAP_H_
