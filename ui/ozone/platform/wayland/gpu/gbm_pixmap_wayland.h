// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_PIXMAP_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_PIXMAP_WAYLAND_H_

#include <memory>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/common/linux/gbm_buffer.h"

namespace ui {

class WaylandBufferManagerGpu;

class GbmPixmapWayland : public gfx::NativePixmap {
 public:
  explicit GbmPixmapWayland(WaylandBufferManagerGpu* buffer_manager);

  // Creates a buffer object and initializes the pixmap buffer.
  bool InitializeBuffer(gfx::Size size,
                        gfx::BufferFormat format,
                        gfx::BufferUsage usage);

  // The widget that this pixmap backs can be assigned later. Can be assigned
  // only once.
  void SetAcceleratedWiget(gfx::AcceleratedWidget widget);

  // gfx::NativePixmap overrides:
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

 private:
  ~GbmPixmapWayland() override;

  // Asks Wayland to create a dmabuf based wl_buffer.
  void CreateDmabufBasedBuffer();

  // gbm_bo wrapper for struct gbm_bo.
  std::unique_ptr<GbmBuffer> gbm_bo_;

  // Represents a connection to Wayland.
  WaylandBufferManagerGpu* const buffer_manager_;

  // Represents widget this pixmap backs.
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;

  DISALLOW_COPY_AND_ASSIGN(GbmPixmapWayland);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_PIXMAP_WAYLAND_H_
