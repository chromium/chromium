// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_BUNDLE_WIN_H_
#define UI_BASE_RESOURCE_RESOURCE_BUNDLE_WIN_H_

#include "build/build_config.h"

#include "base/component_export.h"
#include "base/win/windows_types.h"

namespace ui {

// NOTE: This needs to be called before initializing ResourceBundle if your
// resources are not stored in the executable.
COMPONENT_EXPORT(UI_BASE) void SetResourcesDataDLL(HINSTANCE handle);

// Loads and returns an icon from the app module.
COMPONENT_EXPORT(UI_BASE) HICON LoadThemeIconFromResourcesDataDLL(int icon_id);

// Loads and returns a cursor from the app module.
COMPONENT_EXPORT(UI_BASE)
HCURSOR LoadCursorFromResourcesDataDLL(const wchar_t* cursor_id);

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_BUNDLE_WIN_H_
