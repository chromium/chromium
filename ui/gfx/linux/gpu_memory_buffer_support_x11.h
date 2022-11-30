// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_GPU_MEMORY_BUFFER_SUPPORT_X11_H_
#define UI_GFX_LINUX_GPU_MEMORY_BUFFER_SUPPORT_X11_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace gfx {
class Size;
}

namespace ui {

class GbmBuffer;
class GbmDevice;

// Obtains and holds a GbmDevice for creating GbmBuffers.  Maintains a list of
// supported buffer configurations.
class COMPONENT_EXPORT(GBM_SUPPORT_X11) GpuMemoryBufferSupportX11 {
 public:
  static GpuMemoryBufferSupportX11* GetInstance();

  std::unique_ptr<GbmBuffer> CreateBuffer(gfx::BufferFormat format,
                                          const gfx::Size& size,
                                          gfx::BufferUsage usage);

  bool CanCreateNativePixmapForFormat(gfx::BufferFormat format);
  std::unique_ptr<GbmBuffer> CreateBufferFromHandle(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::NativePixmapHandle handle);

  ~GpuMemoryBufferSupportX11();

  GpuMemoryBufferSupportX11(const GpuMemoryBufferSupportX11&) = delete;
  GpuMemoryBufferSupportX11& operator=(const GpuMemoryBufferSupportX11&) =
      delete;

  const std::vector<gfx::BufferUsageAndFormat>& supported_configs() const {
    return supported_configs_;
  }

  bool has_gbm_device() const { return device_ != nullptr; }

 private:
  friend class base::NoDestructor<GpuMemoryBufferSupportX11>;

  GpuMemoryBufferSupportX11();

  const std::unique_ptr<GbmDevice> device_;
  const std::vector<gfx::BufferUsageAndFormat> supported_configs_;
};

}  // namespace ui

#endif  // UI_GFX_LINUX_GPU_MEMORY_BUFFER_SUPPORT_X11_H_
