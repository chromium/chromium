// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_TOPLEVEL_ICON_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_TOPLEVEL_ICON_MANAGER_H_

#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

// Sets up the global xdg_toplevel_icon_manager_v1 instance.
class ToplevelIconManager
    : public wl::GlobalObjectRegistrar<ToplevelIconManager> {
 public:
  static constexpr char kInterfaceName[] = "xdg_toplevel_icon_manager_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  ToplevelIconManager() = delete;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_TOPLEVEL_ICON_MANAGER_H_
