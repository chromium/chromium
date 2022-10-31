// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/client_native_pixmap_factory_cast.h"

#include <memory>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
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
  explicit ClientNativePixmapCast(gfx::NativePixmapHandle pixmap_handle)
      : pixmap_handle_(std::move(pixmap_handle)) {}
  ~ClientNativePixmapCast() override = default;

  // ClientNativePixmap implementation:
  bool Map() override {
    CHECK(false);
    return false;
  }
  size_t GetNumberOfPlanes() const override {
    CHECK(false);
    return 0u;
  }
  void* GetMemoryAddress(size_t plane) const override {
    CHECK(false);
    return nullptr;
  }
  void Unmap() override { CHECK(false); }
  int GetStride(size_t plane) const override {
    CHECK(false);
    return 0;
  }
  gfx::NativePixmapHandle CloneHandleForIPC() const override {
    CHECK(false);
    return gfx::NativePixmapHandle();
  }

 private:
  gfx::NativePixmapHandle pixmap_handle_;
};

class ClientNativePixmapFactoryCast : public gfx::ClientNativePixmapFactory {
 public:
  // ClientNativePixmapFactoryCast implementation:
  std::unique_ptr<gfx::ClientNativePixmap> ImportFromHandle(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    return std::make_unique<ClientNativePixmapCast>(std::move(handle));
  }
};

}  // namespace

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryCast() {
  return new ClientNativePixmapFactoryCast();
}

}  // namespace ui
