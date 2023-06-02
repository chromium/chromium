// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/tablet_state.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;

constexpr bool kDefaultScreenCoordinateEnabled =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    true;
#else
    false;
#endif

// Wraps the zaura_shell object.
class WaylandZAuraShell : public wl::GlobalObjectRegistrar<WaylandZAuraShell> {
 public:
  static constexpr char kInterfaceName[] = "zaura_shell";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

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
  std::string GetDeskName(int index) const;
  int GetNumberOfDesks();
  int GetActiveDeskIndex() const;
  display::TabletState GetTabletState() const;

 private:
  // zaura_shell_listeners
  static void OnLayoutMode(void* data,
                           struct zaura_shell* zaura_shell,
                           uint32_t layout_mode);
  static void OnBugFix(void* data,
                       struct zaura_shell* zaura_shell,
                       uint32_t id);
  static void OnDesksChanged(void* data,
                             struct zaura_shell* zaura_shell,
                             struct wl_array* states);
  static void OnDeskActivationChanged(void* data,
                                      struct zaura_shell* zaura_shell,
                                      int active_desk_index);
  static void OnActivated(void* data,
                          struct zaura_shell* zaura_shell,
                          struct wl_surface* gained_active,
                          struct wl_surface* lost_active);

  wl::Object<zaura_shell> obj_;
  const raw_ptr<WaylandConnection> connection_;
  base::flat_set<uint32_t> bug_fix_ids_;
  std::vector<std::string> desks_;
  int active_desk_index_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_
