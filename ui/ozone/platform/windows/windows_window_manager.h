// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WINDOWS_WINDOWS_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_WINDOWS_WINDOWS_WINDOW_MANAGER_H_

#include <stdint.h>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class WindowsWindow;

class WindowsWindowManager {
 public:
  WindowsWindowManager();
  ~WindowsWindowManager();

  // Register a new window. Returns the window id.
  int32_t AddWindow(WindowsWindow* window);

  // Remove a window.
  void RemoveWindow(int32_t window_id, WindowsWindow* window);

  // Find a window object by id;
  WindowsWindow* GetWindow(int32_t window_id);

 private:
  base::IDMap<WindowsWindow*> windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowsWindowManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WINDOWS_WINDOWS_WINDOW_MANAGER_H_
