// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_PIXMAP_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_PIXMAP_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"

namespace ui {

class GbmSurfaceFactory;

class GbmPixmap : public gfx::NativePixmap {
 public:
  static constexpr uint32_t kFlagNoModifiers = 1U << 0;

  GbmPixmap(GbmSurfaceFactory* surface_manager,
            std::unique_ptr<GbmBuffer> buffer,
            scoped_refptr<DrmFramebuffer> framebuffer);

  GbmPixmap(const GbmPixmap&) = delete;
  GbmPixmap& operator=(const GbmPixmap&) = delete;

  // NativePixmap:
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

  GbmBuffer* buffer() const { return buffer_.get(); }
  const scoped_refptr<DrmFramebuffer>& framebuffer() const {
    return framebuffer_;
  }

 private:
  ~GbmPixmap() override;

  const raw_ptr<GbmSurfaceFactory> surface_manager_;
  const std::unique_ptr<GbmBuffer> buffer_;
  const scoped_refptr<DrmFramebuffer> framebuffer_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_PIXMAP_H_
