// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_CLIENT_NATIVE_PIXMAP_DMABUF_H_
#define UI_GFX_LINUX_CLIENT_NATIVE_PIXMAP_DMABUF_H_

#include <stdint.h>

#include <array>
#include <memory>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace gfx {

class ClientNativePixmapDmaBuf : public gfx::ClientNativePixmap {
 public:
  static GFX_EXPORT bool IsConfigurationSupported(gfx::BufferFormat format,
                                                  gfx::BufferUsage usage);

  static std::unique_ptr<gfx::ClientNativePixmap> ImportFromDmabuf(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format);

  ~ClientNativePixmapDmaBuf() override;

  // Overridden from ClientNativePixmap.
  bool Map() override;
  void Unmap() override;

  void* GetMemoryAddress(size_t plane) const override;
  int GetStride(size_t plane) const override;

 private:
  static constexpr size_t kMaxPlanes = 4;

  struct PlaneInfo {
    PlaneInfo();
    PlaneInfo(PlaneInfo&& plane_info);
    ~PlaneInfo();

    void* data = nullptr;
    size_t offset = 0;
    size_t size = 0;
  };
  ClientNativePixmapDmaBuf(gfx::NativePixmapHandle handle,
                           const gfx::Size& size,
                           std::array<PlaneInfo, kMaxPlanes> plane_info);

  const gfx::NativePixmapHandle pixmap_handle_;
  const gfx::Size size_;
  const std::array<PlaneInfo, kMaxPlanes> plane_info_;

  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapDmaBuf);
};

}  // namespace gfx

#endif  // UI_GFX_LINUX_CLIENT_NATIVE_PIXMAP_DMABUF_H_
