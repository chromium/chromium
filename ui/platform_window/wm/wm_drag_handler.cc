// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/wm/wm_drag_handler.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::WmDragHandler*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(WmDragHandler*, kWmDragHandlerKey, nullptr)

WmDragHandler::Delegate::~Delegate() = default;

void SetWmDragHandler(PlatformWindow* platform_window,
                      WmDragHandler* drag_handler) {
  platform_window->SetProperty(kWmDragHandlerKey, drag_handler);
}

WmDragHandler* GetWmDragHandler(const PlatformWindow& platform_window) {
  return platform_window.GetProperty(kWmDragHandlerKey);
}

}  // namespace ui
