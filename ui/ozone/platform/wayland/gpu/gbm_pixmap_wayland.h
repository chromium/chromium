// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_PIXMAP_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_PIXMAP_WAYLAND_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class WaylandBufferManagerGpu;

class GbmPixmapWayland : public gfx::NativePixmap {
 public:
  explicit GbmPixmapWayland(WaylandBufferManagerGpu* buffer_manager);

  GbmPixmapWayland(const GbmPixmapWayland&) = delete;
  GbmPixmapWayland& operator=(const GbmPixmapWayland&) = delete;

  // Creates a buffer object and initializes the pixmap buffer.
  // |visible_area_size| represents a 'visible size', i.e., a buffer
  // of size |size| may actually contain visible data only in the
  // subregion of size |visible_area_size|. If |visible_area_size| is
  // not provided, |size| is used. If |widget| is provided, browser
  // side wl_buffer is also created. Otherwise, this pixmap
  // behaves as a staging pixmap and mustn't be scheduled as an overlay.
  bool InitializeBuffer(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      std::optional<gfx::Size> visible_area_size = std::nullopt);

  // Creates a buffer object from native pixmap handle and initializes the
  // pixmap buffer. If |widget| is provided, browser side wl_buffer is also
  // created. Otherwise, this pixmap behaves as a staging pixmap and mustn't be
  // scheduled as an overlay.
  bool InitializeBufferFromHandle(gfx::AcceleratedWidget widget,
                                  gfx::Size size,
                                  gfx::BufferFormat format,
                                  gfx::NativePixmapHandle handle);

  // gfx::NativePixmap overrides:
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

 private:
  ~GbmPixmapWayland() override;

  // Asks Wayland to create a dmabuf based wl_buffer.
  void CreateDmabufBasedWlBuffer();

  // gbm_bo wrapper for struct gbm_bo.
  std::unique_ptr<GbmBuffer> gbm_bo_;

  // Represents a connection to Wayland.
  const raw_ptr<WaylandBufferManagerGpu> buffer_manager_;

  // Represents widget this pixmap backs.
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;

  // A unique ID to identify the buffer for this pixmap.
  const uint32_t buffer_id_;

  // Size of the visible area of the buffer.
  gfx::Size visible_area_size_;

  // Says a wl_buffer has been created and must removed.
  bool created_wl_buffer_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_PIXMAP_WAYLAND_H_
