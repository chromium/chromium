// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/wm/wm_move_loop_handler.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::WmMoveLoopHandler*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(WmMoveLoopHandler*, kWmMoveLoopHandlerKey, nullptr)

void SetWmMoveLoopHandler(PlatformWindow* platform_window,
                          WmMoveLoopHandler* drag_handler) {
  platform_window->SetProperty(kWmMoveLoopHandlerKey, drag_handler);
}

WmMoveLoopHandler* GetWmMoveLoopHandler(const PlatformWindow& platform_window) {
  return platform_window.GetProperty(kWmMoveLoopHandlerKey);
}

}  // namespace ui
