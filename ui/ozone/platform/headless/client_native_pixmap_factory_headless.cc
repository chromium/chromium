// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/client_native_pixmap_factory_headless.h"

#include "build/build_config.h"
#include "ui/ozone/common/stub_client_native_pixmap_factory.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"
#endif

namespace ui {

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryHeadless() {
#if BUILDFLAG(IS_LINUX)
  return gfx::CreateClientNativePixmapFactoryDmabuf();
#else
  return CreateStubClientNativePixmapFactory();
#endif
}

}  // namespace ui
