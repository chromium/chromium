// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/client_native_pixmap_factory_cast.h"

#include <memory>

#include "base/logging.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace ui {
namespace {

// Dummy ClientNativePixmap implementation for Cast ozone.
// Our NativePixmaps are just used to plumb an overlay frame through,
// so they get instantiated, but not used.
class ClientNativePixmapCast : public gfx::ClientNativePixmap {
 public:
  // ClientNativePixmap implementation:
  bool Map() override {
    NOTREACHED();
    return false;
  }
  void* GetMemoryAddress(size_t plane) const override {
    NOTREACHED();
    return nullptr;
  }
  void Unmap() override { NOTREACHED(); }
  int GetStride(size_t plane) const override {
    NOTREACHED();
    return 0;
  }
};

class ClientNativePixmapFactoryCast : public gfx::ClientNativePixmapFactory {
 public:
  // ClientNativePixmapFactoryCast implementation:
  std::unique_ptr<gfx::ClientNativePixmap> ImportFromHandle(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    return std::make_unique<ClientNativePixmapCast>();
  }
};

}  // namespace

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryCast() {
  return new ClientNativePixmapFactoryCast();
}

}  // namespace ui
