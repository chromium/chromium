// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354862211): Remove this after removing unsafe code only used
// in Lacros in OnDesksChanged
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

#include <cstring>

#include <components/exo/wayland/protocol/aura-shell-client-protocol.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

namespace {

constexpr uint32_t kMinVersion = 1;
// Version: 65
constexpr uint32_t kMaxVersion =
    ZAURA_TOPLEVEL_CONFIGURE_OCCLUSION_STATE_SINCE_VERSION;

}  // namespace

// static
constexpr char WaylandZAuraShell::kInterfaceName[];

// static
void WaylandZAuraShell::Instantiate(WaylandConnection* connection,
                                    wl_registry* registry,
                                    uint32_t name,
                                    const std::string& interface,
                                    uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->zaura_shell_ ||
      !wl::CanBind(interface, version, kMinVersion, kMaxVersion)) {
    return;
  }

  auto zaura_shell = wl::Bind<struct zaura_shell>(
      registry, name, std::min(version, kMaxVersion));
  if (!zaura_shell) {
    LOG(ERROR) << "Failed to bind zaura_shell";
    return;
  }
  connection->zaura_shell_ =
      std::make_unique<WaylandZAuraShell>(zaura_shell.release(), connection);
  ReportShellUMA(UMALinuxWaylandShell::kZauraShell);
}

WaylandZAuraShell::WaylandZAuraShell(zaura_shell* aura_shell,
                                     WaylandConnection* connection)
    : obj_(aura_shell), connection_(connection) {
  DCHECK(obj_);
  DCHECK(connection_);

  static constexpr zaura_shell_listener kZAuraShellListener = {
      .layout_mode = &OnLayoutMode,
      .bug_fix = &OnBugFix,
      .desks_changed = &OnDesksChanged,
      .desk_activation_changed = &OnDeskActivationChanged,
      .activated = &OnActivated,
      .set_overview_mode = &OnSetOverviewMode,
      .unset_overview_mode = &OnUnsetOverviewMode,
      .compositor_version = &OnCompositorVersion,
      .all_bug_fixes_sent = &OnAllBugFixesSent,
      .window_corners_radii = &OnSetWindowCornersRadii};
  zaura_shell_add_listener(obj_.get(), &kZAuraShellListener, this);

  if (IsWaylandSurfaceSubmissionInPixelCoordinatesEnabled() &&
      zaura_shell_get_version(wl_object()) >=
          ZAURA_TOPLEVEL_SURFACE_SUBMISSION_IN_PIXEL_COORDINATES_SINCE_VERSION) {
    connection->set_surface_submission_in_pixel_coordinates(true);
  }
}

WaylandZAuraShell::~WaylandZAuraShell() = default;

std::string WaylandZAuraShell::GetDeskName(int index) const {
  if (static_cast<size_t>(index) >= desks_.size())
    return std::string();
  return desks_[index];
}

int WaylandZAuraShell::GetNumberOfDesks() {
  return desks_.size();
}

int WaylandZAuraShell::GetActiveDeskIndex() const {
  return active_desk_index_;
}

gfx::RoundedCornersF WaylandZAuraShell::GetWindowCornersRadii() const {
  return window_corners_radii_;
}

// static
void WaylandZAuraShell::OnLayoutMode(void* data,
                                     struct zaura_shell* zaura_shell,
                                     uint32_t layout_mode) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* self = static_cast<WaylandZAuraShell*>(data);
  auto* connection = self->connection_.get();
  auto* screen = connection->wayland_output_manager()->wayland_screen();

  switch (layout_mode) {
    case ZAURA_SHELL_LAYOUT_MODE_WINDOWED:
      connection->set_tablet_layout_state(
          display::TabletState::kInClamshellMode);
      // `screen` is null in some unit test suites or if it's called earlier
      // than screen initialization.
      if (screen)
        screen->OnTabletStateChanged(display::TabletState::kInClamshellMode);
      return;
    case ZAURA_SHELL_LAYOUT_MODE_TABLET:
      connection->set_tablet_layout_state(display::TabletState::kInTabletMode);
      // `screen` is null in some unit test suites or if it's called earlier
      // than screen initialization.
      if (screen)
        screen->OnTabletStateChanged(display::TabletState::kInTabletMode);
      return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

// static
void WaylandZAuraShell::OnBugFix(void* data,
                                 struct zaura_shell* zaura_shell,
                                 uint32_t id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandZAuraShell::OnDesksChanged(void* data,
                                       struct zaura_shell* zaura_shell,
                                       struct wl_array* states) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  char* desk_name = reinterpret_cast<char*>(states->data);
  self->desks_.clear();
  // SAFETY: TODO(crbug.com/354862211): This code is only used by Lacros and
  // will be removed, so we are opting not fix the unsafe pointer
  // arithmetic and -Wunsafe-buffer-usage suppression in this file.
  while (desk_name < reinterpret_cast<char*>(states->data) + states->size) {
    std::string str(desk_name, strlen(desk_name));
    self->desks_.push_back(str);
    desk_name += strlen(desk_name) + 1;
  }
}

// static
void WaylandZAuraShell::OnDeskActivationChanged(void* data,
                                                struct zaura_shell* zaura_shell,
                                                int active_desk_index) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  self->active_desk_index_ = active_desk_index;
}

// static
void WaylandZAuraShell::OnActivated(void* data,
                                    struct zaura_shell* zaura_shell,
                                    wl_surface* gained_active,
                                    wl_surface* lost_active) {}

// static
void WaylandZAuraShell::OnSetOverviewMode(void* data,
                                          struct zaura_shell* zaura_shell) {
}

// static
void WaylandZAuraShell::OnUnsetOverviewMode(void* data,
                                            struct zaura_shell* zaura_shell) {
}

// static
void WaylandZAuraShell::OnCompositorVersion(void* data,
                                            struct zaura_shell* zaura_shell,
                                            const char* version_label) {
  auto* self = static_cast<WaylandZAuraShell*>(data);

  self->server_version_.emplace(version_label);

  if (!self->server_version_->IsValid()) {
    LOG(WARNING) << "Invalid compositor version string received.";
    return;
  }

  DCHECK_EQ(self->server_version_->components().size(), 4u);
  DVLOG(1) << "Wayland compositor version: " << self->server_version_.value();
}

// static
void WaylandZAuraShell::OnAllBugFixesSent(void* data,
                                          struct zaura_shell* zaura_shell) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandZAuraShell::OnSetWindowCornersRadii(void* data,
                                                struct zaura_shell* zaura_shell,
                                                uint32_t upper_left_radius,
                                                uint32_t upper_right_radius,
                                                uint32_t lower_right_radius,
                                                uint32_t lower_left_radius) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  self->window_corners_radii_ =
      gfx::RoundedCornersF(upper_left_radius, upper_right_radius,
                           lower_right_radius, lower_left_radius);
}

}  // namespace ui
