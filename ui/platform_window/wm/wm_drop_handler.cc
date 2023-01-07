// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/wm/wm_drop_handler.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::WmDropHandler*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(WmDropHandler*, kWmDropHandlerKey, nullptr)

void SetWmDropHandler(PlatformWindow* platform_window,
                      WmDropHandler* drop_handler) {
  platform_window->SetProperty(kWmDropHandlerKey, drop_handler);
}

WmDropHandler* GetWmDropHandler(const PlatformWindow& platform_window) {
  return platform_window.GetProperty(kWmDropHandlerKey);
}

}  // namespace ui
