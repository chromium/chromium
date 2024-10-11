// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_NATIVE_PIXMAP_DMABUF_H_
#define UI_GFX_LINUX_NATIVE_PIXMAP_DMABUF_H_

#include <stdint.h>

#include <memory>

#include "base/files/scoped_file.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"

namespace gfx {

// This class converts a gfx::NativePixmapHandle to a gfx::NativePixmap.
class GFX_EXPORT NativePixmapDmaBuf : public gfx::NativePixmap {
 public:
  NativePixmapDmaBuf(const gfx::Size& size,
                     gfx::BufferFormat format,
                     gfx::NativePixmapHandle handle);

  NativePixmapDmaBuf(const NativePixmapDmaBuf&) = delete;
  NativePixmapDmaBuf& operator=(const NativePixmapDmaBuf&) = delete;

  // NativePixmap:
  bool AreDmaBufFdsValid() const override;
  int GetDmaBufFd(size_t plane) const override;
  uint32_t GetDmaBufPitch(size_t plane) const override;
  size_t GetDmaBufOffset(size_t plane) const override;
  size_t GetDmaBufPlaneSize(size_t plane) const override;
  uint64_t GetBufferFormatModifier() const override;
  gfx::BufferFormat GetBufferFormat() const override;
  size_t GetNumberOfPlanes() const override;
  bool SupportsZeroCopyWebGPUImport() const override;
  gfx::Size GetBufferSize() const override;
  uint32_t GetUniqueId() const override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            const gfx::OverlayPlaneData& overlay_plane_data,
                            std::vector<gfx::GpuFence> acquire_fences,
                            std::vector<gfx::GpuFence> release_fences) override;
  gfx::NativePixmapHandle ExportHandle() const override;

 protected:
  ~NativePixmapDmaBuf() override;

 private:
  gfx::Size size_;
  gfx::BufferFormat format_;
  gfx::NativePixmapHandle handle_;
};

}  // namespace gfx

#endif  // UI_GFX_LINUX_NATIVE_PIXMAP_DMABUF_H_
