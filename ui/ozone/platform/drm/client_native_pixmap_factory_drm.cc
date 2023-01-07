// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/client_native_pixmap_factory_drm.h"

#include <utility>

#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace ui {

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryDrm() {
  return gfx::CreateClientNativePixmapFactoryDmabuf();
}

}  // namespace ui
