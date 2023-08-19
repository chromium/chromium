// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle_win.h"

#include <windows.h>

#include <memory>

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_data_dll_win.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"

namespace ui {

namespace {

HINSTANCE resources_data_dll;

HINSTANCE GetCurrentResourceDLL() {
  if (resources_data_dll)
    return resources_data_dll;
  return GetModuleHandle(NULL);
}

}  // namespace

void ResourceBundle::LoadCommonResources() {
  // As a convenience, add the current resource module as a data packs.
  resource_handles_.push_back(
      std::make_unique<ResourceDataDLL>(GetCurrentResourceDLL()));

  LoadChromeResources();
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  // Windows only uses SkBitmap for gfx::Image, so this is the same as
  // GetImageNamed.
  return GetImageNamed(resource_id);
}

void SetResourcesDataDLL(HINSTANCE handle) {
  resources_data_dll = handle;
}

HICON LoadThemeIconFromResourcesDataDLL(int icon_id) {
  return ::LoadIcon(GetCurrentResourceDLL(), MAKEINTRESOURCE(icon_id));
}

HCURSOR LoadCursorFromResourcesDataDLL(const wchar_t* cursor_id) {
  return ::LoadCursor(GetCurrentResourceDLL(), cursor_id);
}

}  // namespace ui;
