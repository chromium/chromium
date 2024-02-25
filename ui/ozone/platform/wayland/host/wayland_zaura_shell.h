// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
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

  // Returns the Wayland server version. If the bound zaura_shell is not
  // recent enough (ie: < v58), this returns std::nullopt. This can be used in
  // conjunction with ozone platform's RuntimeProperties in order to determine
  // if Exo supports a given feature.
  // See https://crbug.com/1457008.
  const std::optional<base::Version>& server_version() const {
    return server_version_;
  }

  std::string GetDeskName(int index) const;
  int GetNumberOfDesks();
  int GetActiveDeskIndex() const;
  display::TabletState GetTabletState() const;
  gfx::RoundedCornersF GetWindowCornersRadii() const;

 private:
  // zaura_shell_listener callbacks:
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

  // TODO(sammiequon): Remove these two deprecated functions.
  static void OnSetOverviewMode(void* data, struct zaura_shell* zaura_shell);
  static void OnUnsetOverviewMode(void* data, struct zaura_shell* zaura_shell);

  static void OnCompositorVersion(void* data,
                                  struct zaura_shell* zaura_shell,
                                  const char* version_label);
  static void OnAllBugFixesSent(void* data, struct zaura_shell* zaura_shell);
  static void OnSetWindowCornersRadii(void* data,
                                      struct zaura_shell* zaura_shell,
                                      uint32_t upper_left_radius,
                                      uint32_t upper_right_radius,
                                      uint32_t lower_right_radius,
                                      uint32_t lower_left_radius);

  wl::Object<zaura_shell> obj_;
  const raw_ptr<WaylandConnection> connection_;
  std::optional<base::Version> server_version_;
  std::vector<std::string> desks_;
  int active_desk_index_ = 0;
  gfx::RoundedCornersF window_corners_radii_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_
