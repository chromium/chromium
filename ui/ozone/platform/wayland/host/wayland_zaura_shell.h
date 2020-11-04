// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_

#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;

// Wraps the zaura_shell object.
class WaylandZAuraShell {
 public:
  WaylandZAuraShell(zaura_shell* aura_shell, WaylandConnection* connection);
  WaylandZAuraShell(const WaylandZAuraShell&) = delete;
  WaylandZAuraShell& operator=(const WaylandZAuraShell&) = delete;
  ~WaylandZAuraShell();

  zaura_shell* wl_object() const { return obj_.get(); }

 private:
  // zaura_shell_listener
  static void OnLayoutMode(void* data,
                           struct zaura_shell* zaura_shell,
                           uint32_t layout_mode);

  wl::Object<zaura_shell> obj_;
  WaylandConnection* const connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_
