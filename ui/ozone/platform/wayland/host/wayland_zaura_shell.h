// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_

#include <string>

#include "base/containers/flat_set.h"
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
  // Due to version skew between Lacros and Ash, there may be certain bug
  // fixes in one but not in the other (crbug.com/1151508). Lacros can use
  // |HasBugFix| to provide a temporary workaround to an exo bug until Ash
  // uprevs and starts reporting that a given bug ID has been fixed.
  bool HasBugFix(uint32_t id);

 private:
  // zaura_shell_listeners
  static void OnLayoutMode(void* data,
                           struct zaura_shell* zaura_shell,
                           uint32_t layout_mode);
  static void OnBugFix(void* data,
                       struct zaura_shell* zaura_shell,
                       uint32_t id);

  wl::Object<zaura_shell> obj_;
  WaylandConnection* const connection_;
  base::flat_set<uint32_t> bug_fix_ids_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_
