// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/notreached.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace ui {

namespace {

class StubClientNativePixmapFactory : public gfx::ClientNativePixmapFactory {
 public:
  StubClientNativePixmapFactory() {}

  StubClientNativePixmapFactory(const StubClientNativePixmapFactory&) = delete;
  StubClientNativePixmapFactory& operator=(
      const StubClientNativePixmapFactory&) = delete;

  ~StubClientNativePixmapFactory() override {}

  // ClientNativePixmapFactory:
  std::unique_ptr<gfx::ClientNativePixmap> ImportFromHandle(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
};

}  // namespace

gfx::ClientNativePixmapFactory* CreateStubClientNativePixmapFactory() {
  return new StubClientNativePixmapFactory;
}

}  // namespace ui
