// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_

#include <memory>
#include <vector>

#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/common/linux/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"

namespace ui {

class GbmSurfaceFactory;

class GbmPixmap : public gfx::NativePixmap {
 public:
  static constexpr uint32_t kFlagNoModifiers = 1U << 0;

  GbmPixmap(GbmSurfaceFactory* surface_manager,
            std::unique_ptr<GbmBuffer> buffer,
            scoped_refptr<DrmFramebuffer> framebuffer);

  // NativePixmap:
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
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  gfx::NativePixmapHandle ExportHandle() override;

  GbmBuffer* buffer() const { return buffer_.get(); }
  const scoped_refptr<DrmFramebuffer>& framebuffer() const {
    return framebuffer_;
  }

 private:
  ~GbmPixmap() override;

  GbmSurfaceFactory* const surface_manager_;
  const std::unique_ptr<GbmBuffer> buffer_;
  const scoped_refptr<DrmFramebuffer> framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(GbmPixmap);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_
