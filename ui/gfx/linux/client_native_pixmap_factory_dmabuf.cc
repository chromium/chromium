// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gfx/native_pixmap_handle.h"

// Although, it's compiled for all linux platforms, it does not mean dmabuf
// will work there. Check the comment below in the
// ClientNativePixmapFactoryDmabuf for more details.
#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"

namespace gfx {

namespace {

class ClientNativePixmapOpaque : public ClientNativePixmap {
 public:
  ClientNativePixmapOpaque() {}
  ~ClientNativePixmapOpaque() override {}

  bool Map() override {
    NOTREACHED();
    return false;
  }
  void Unmap() override { NOTREACHED(); }
  void* GetMemoryAddress(size_t plane) const override {
    NOTREACHED();
    return nullptr;
  }
  int GetStride(size_t plane) const override {
    NOTREACHED();
    return 0;
  }
};

}  // namespace

class ClientNativePixmapFactoryDmabuf : public ClientNativePixmapFactory {
 public:
  explicit ClientNativePixmapFactoryDmabuf() {}
  ~ClientNativePixmapFactoryDmabuf() override {}

  std::unique_ptr<ClientNativePixmap> ImportFromHandle(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    DCHECK(!handle.planes.empty());
    switch (usage) {
      case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      case gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE:
        return ClientNativePixmapDmaBuf::ImportFromDmabuf(std::move(handle),
                                                          size, format);
      case gfx::BufferUsage::GPU_READ:
      case gfx::BufferUsage::SCANOUT:
      case gfx::BufferUsage::SCANOUT_VDA_WRITE:
        return base::WrapUnique(new ClientNativePixmapOpaque);
    }
    NOTREACHED();
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFactoryDmabuf);
};

ClientNativePixmapFactory* CreateClientNativePixmapFactoryDmabuf() {
  return new ClientNativePixmapFactoryDmabuf();
}

}  // namespace gfx
