// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_NATIVE_PIXMAP_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_NATIVE_PIXMAP_H_

#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"

namespace ui {

class FlatlandSysmemNativePixmap : public gfx::NativePixmap {
 public:
  FlatlandSysmemNativePixmap(
      scoped_refptr<FlatlandSysmemBufferCollection> collection,
      gfx::NativePixmapHandle handle,
      gfx::Size size);

  FlatlandSysmemNativePixmap(const FlatlandSysmemNativePixmap&) = delete;
  FlatlandSysmemNativePixmap& operator=(const FlatlandSysmemNativePixmap&) =
      delete;

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
  gfx::NativePixmapHandle ExportHandle() const override;

  FlatlandSysmemBufferCollection* sysmem_buffer_collection() const {
    return collection_.get();
  }
  const gfx::NativePixmapHandle& PeekHandle() const;

  // Returns true if overlay planes are supported.
  bool SupportsOverlayPlane() const;

 private:
  ~FlatlandSysmemNativePixmap() override;

  // Keep reference to the collection to make sure it outlives the pixmap.
  scoped_refptr<FlatlandSysmemBufferCollection> collection_;
  gfx::NativePixmapHandle handle_;

  gfx::Size size_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_NATIVE_PIXMAP_H_
