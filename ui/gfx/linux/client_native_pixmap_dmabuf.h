// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_CLIENT_NATIVE_PIXMAP_DMABUF_H_
#define UI_GFX_LINUX_CLIENT_NATIVE_PIXMAP_DMABUF_H_

#include <stdint.h>

#include <array>
#include <memory>

#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
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

  // Note: |handle| is expected to have been validated as in
  // ClientNativePixmapFactoryDmabuf::ImportFromHandle().
  // TODO(andrescj): consider not exposing this class outside of
  // client_native_pixmap_factory_dmabuf.cc.
  static std::unique_ptr<gfx::ClientNativePixmap> ImportFromDmabuf(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format);

  ClientNativePixmapDmaBuf(const ClientNativePixmapDmaBuf&) = delete;
  ClientNativePixmapDmaBuf& operator=(const ClientNativePixmapDmaBuf&) = delete;

  ~ClientNativePixmapDmaBuf() override;

  // Overridden from ClientNativePixmap.
  bool Map() override;
  void Unmap() override;

  size_t GetNumberOfPlanes() const override;
  void* GetMemoryAddress(size_t plane) const override;
  int GetStride(size_t plane) const override;
  NativePixmapHandle CloneHandleForIPC() const override;

 private:
  static constexpr size_t kMaxPlanes = 4;

  struct PlaneInfo {
    PlaneInfo();
    PlaneInfo(PlaneInfo&& plane_info);
    ~PlaneInfo();

    raw_ptr<void> data = nullptr;
    size_t offset = 0;
    size_t size = 0;
  };
  ClientNativePixmapDmaBuf(gfx::NativePixmapHandle handle,
                           const gfx::Size& size,
                           std::array<PlaneInfo, kMaxPlanes> plane_info);

  const gfx::NativePixmapHandle pixmap_handle_;
  const gfx::Size size_;
  std::array<PlaneInfo, kMaxPlanes> plane_info_;
  bool mapped_ = false;
};

}  // namespace gfx

#endif  // UI_GFX_LINUX_CLIENT_NATIVE_PIXMAP_DMABUF_H_
