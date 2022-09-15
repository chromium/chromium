// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_CLIENT_NATIVE_PIXMAP_FACTORY_FLATLAND_H_
#define UI_OZONE_PLATFORM_FLATLAND_CLIENT_NATIVE_PIXMAP_FACTORY_FLATLAND_H_

namespace gfx {
class ClientNativePixmapFactory;
}  // namespace gfx

namespace ui {

// Flatland-platform constructor hook, for use by the auto-generated platform
// constructor-list.
gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryFlatland();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_CLIENT_NATIVE_PIXMAP_FACTORY_FLATLAND_H_
