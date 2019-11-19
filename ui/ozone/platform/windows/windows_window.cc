// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/windows/windows_window.h"

#include <string>

#include "build/build_config.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/windows/windows_window_manager.h"

namespace ui {

WindowsWindow::WindowsWindow(PlatformWindowDelegate* delegate,
                             const gfx::Rect& bounds)
    : WinWindow(delegate, bounds) {}

WindowsWindow::~WindowsWindow() {}

}  // namespace ui
