// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

  // Returns the Wayland compositor version. If the bound zaura_shell is not
  // recent enough (ie: < v58), the returned version is not valid. This can be
  // used in conjunction with ozone platform's RuntimeProperties + (optional)
  // HasBugFix() calls in order to determine if Exo supports a given feature.
  // See https://crbug.com/1457008.
  const base::Version& compositor_version() const {
    return compositor_version_;
  }
  // Due to version skew between Lacros and Ash, there may be certain bug
  // fixes in one but not in the other (crbug.com/1151508). Lacros can use
  // |HasBugFix| to provide a temporary workaround to an exo bug until Ash
  // uprevs and starts reporting that a given bug ID has been fixed.
  bool HasBugFix(uint32_t id);

  // Gets bug fix ids if it's ready. Returns nullopt if AllBugFixesSent event is
  // not yet received.
  absl::optional<std::vector<uint32_t>> MaybeGetBugFixIds() const;

  std::string GetDeskName(int index) const;
  int GetNumberOfDesks();
  int GetActiveDeskIndex() const;
  display::TabletState GetTabletState() const;
  bool SupportsAllBugFixesSent() const;

  // Resets bug_fix_ids cache and all_bug_fixes_sent flag. This is used for
  // testing bug fix ids feature.
  void ResetBugFixesStatusForTesting();

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
  static void OnSetOverviewMode(void* data, struct zaura_shell* zaura_shell);
  static void OnUnsetOverviewMode(void* data, struct zaura_shell* zaura_shell);
  static void OnCompositorVersion(void* data,
                                  struct zaura_shell* zaura_shell,
                                  const char* version_label);
  static void OnAllBugFixesSent(void* data, struct zaura_shell* zaura_shell);

  wl::Object<zaura_shell> obj_;
  const raw_ptr<WaylandConnection> connection_;
  base::Version compositor_version_;
  bool all_bug_fixes_sent_ = false;
  std::vector<uint32_t> bug_fix_ids_;
  std::vector<std::string> desks_;
  int active_desk_index_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SHELL_H_
