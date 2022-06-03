// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/client_native_pixmap_factory_x11.h"

#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"

namespace ui {

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryX11() {
  return gfx::CreateClientNativePixmapFactoryDmabuf();
}

}  // namespace ui
