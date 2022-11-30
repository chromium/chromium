// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_CLIENT_NATIVE_PIXMAP_FACTORY_DMABUF_H_
#define UI_GFX_LINUX_CLIENT_NATIVE_PIXMAP_FACTORY_DMABUF_H_

#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

GFX_EXPORT ClientNativePixmapFactory* CreateClientNativePixmapFactoryDmabuf();

}  // namespace gfx

#endif  // UI_GFX_LINUX_CLIENT_NATIVE_PIXMAP_FACTORY_DMABUF_H_
