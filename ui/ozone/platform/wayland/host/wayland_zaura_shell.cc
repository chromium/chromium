// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

#include <cstring>

#include <components/exo/wayland/protocol/aura-shell-client-protocol.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
constexpr uint32_t kMaxVersion = 49;
}

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

  // Usually WaylandOutputManager is instantiated first, so any ZAuraOutputs it
  // created wouldn't have been initialized, since the zaura_shell didn't exist
  // yet. So initialize them now.
  if (connection->wayland_output_manager()) {
    connection->wayland_output_manager()->InitializeAllZAuraOutputs();
  }
}

WaylandZAuraShell::WaylandZAuraShell(zaura_shell* aura_shell,
                                     WaylandConnection* connection)
    : obj_(aura_shell), connection_(connection) {
  DCHECK(obj_);
  DCHECK(connection_);

  static constexpr zaura_shell_listener zaura_shell_listener = {
      &OnLayoutMode, &OnBugFix, &OnDesksChanged, &OnDeskActivationChanged,
      &OnActivated,
  };
  zaura_shell_add_listener(obj_.get(), &zaura_shell_listener, this);
  if (IsWaylandSurfaceSubmissionInPixelCoordinatesEnabled() &&
      zaura_shell_get_version(wl_object()) >=
          ZAURA_TOPLEVEL_SURFACE_SUBMISSION_IN_PIXEL_COORDINATES_SINCE_VERSION) {
    connection->set_surface_submission_in_pixel_coordinates(true);
  }
}

WaylandZAuraShell::~WaylandZAuraShell() = default;

bool WaylandZAuraShell::HasBugFix(uint32_t id) {
  return bug_fix_ids_.find(id) != bug_fix_ids_.end();
}

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
      // |screen| is null in some unit test suites or if it's called eariler
      // than screen initialization.
      if (screen)
        screen->OnTabletStateChanged(display::TabletState::kInClamshellMode);
      return;
    case ZAURA_SHELL_LAYOUT_MODE_TABLET:
      connection->set_tablet_layout_state(display::TabletState::kInTabletMode);
      // |screen| is null in some unit test suites or if it's called eariler
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
  auto* self = static_cast<WaylandZAuraShell*>(data);
  self->bug_fix_ids_.insert(id);
}

// static
void WaylandZAuraShell::OnDesksChanged(void* data,
                                       struct zaura_shell* zaura_shell,
                                       struct wl_array* states) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  char* desk_name = reinterpret_cast<char*>(states->data);
  self->desks_.clear();
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
}  // namespace ui
