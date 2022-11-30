// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_WINDOW_MANAGER_H_

#include <stdint.h>
#include <memory>

#include "base/component_export.h"
#include "base/containers/id_map.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/flatland/flatland_screen.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class FlatlandWindow;

// Window manager is responsible for mapping window IDs to FlatlandWindow
// instances. Window IDs are integer values that are passed around as
// gpu::AcceleratedWidget. The manager is created and owned by
// OzonePlatformFlatland.
//
// TODO(sergeyu): Consider updating AcceleratedWidget to store FlatlandWindow*
// which would remove the need for the IDMap.
class COMPONENT_EXPORT(OZONE) FlatlandWindowManager {
 public:
  FlatlandWindowManager();
  ~FlatlandWindowManager();
  FlatlandWindowManager(const FlatlandWindowManager&) = delete;
  FlatlandWindowManager& operator=(const FlatlandWindowManager&) = delete;

  // Shuts down the window manager.
  void Shutdown();

  std::unique_ptr<PlatformScreen> CreateScreen();

  // Called by FlatlandWindow when a new window instance is created. Returns
  // window ID for the |window|.
  int32_t AddWindow(FlatlandWindow* window);

  // Called by FlatlandWindow destructor to unregister |window|.
  void RemoveWindow(int32_t window_id, FlatlandWindow* window);

  FlatlandWindow* GetWindow(int32_t window_id);

 private:
  base::IDMap<FlatlandWindow*> windows_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_WINDOW_MANAGER_H_
