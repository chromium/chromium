// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_CLIENT_NATIVE_PIXMAP_FACTORY_SCENIC_H_
#define UI_OZONE_PLATFORM_SCENIC_CLIENT_NATIVE_PIXMAP_FACTORY_SCENIC_H_

namespace gfx {
class ClientNativePixmapFactory;
}  // namespace gfx

namespace ui {

// Scenic-platform constructor hook, for use by the auto-generated platform
// constructor-list.
gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryScenic();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_CLIENT_NATIVE_PIXMAP_FACTORY_SCENIC_H_
