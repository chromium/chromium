// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_GBM_SUPPORT_X11_H_
#define UI_GFX_LINUX_GBM_SUPPORT_X11_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace gfx {
class Size;
}

namespace ui {

class GbmBuffer;
class GbmDevice;

// Obtains and holds a GbmDevice for creating GbmBuffers.
class COMPONENT_EXPORT(GBM_SUPPORT_X11) GBMSupportX11 {
 public:
  static GBMSupportX11* GetInstance();

  std::unique_ptr<GbmBuffer> CreateBuffer(viz::SharedImageFormat format,
                                          const gfx::Size& size,
                                          gfx::BufferUsage usage);

  bool CanCreateBufferForFormat(viz::SharedImageFormat format);
  std::unique_ptr<GbmBuffer> CreateBufferFromHandle(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::NativePixmapHandle handle);

  ~GBMSupportX11();

  GBMSupportX11(const GBMSupportX11&) = delete;
  GBMSupportX11& operator=(const GBMSupportX11&) = delete;

  bool has_gbm_device() const { return device_ != nullptr; }

 private:
  friend class base::NoDestructor<GBMSupportX11>;

  GBMSupportX11();

  const std::unique_ptr<GbmDevice> device_;
  const std::vector<gfx::BufferUsageAndFormat> supported_configs_;
};

}  // namespace ui

#endif  // UI_GFX_LINUX_GBM_SUPPORT_X11_H_
