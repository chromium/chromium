// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

#include <components/exo/wayland/protocol/aura-shell-client-protocol.h>

#include "base/check.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"

namespace ui {

WaylandZAuraShell::WaylandZAuraShell(zaura_shell* aura_shell,
                                     WaylandConnection* connection)
    : obj_(aura_shell), connection_(connection) {
  DCHECK(obj_);
  DCHECK(connection_);

  static const zaura_shell_listener zaura_shell_listener = {
      &WaylandZAuraShell::OnLayoutMode,
      &WaylandZAuraShell::OnBugFix,
  };
  zaura_shell_add_listener(obj_.get(), &zaura_shell_listener, this);
}

WaylandZAuraShell::~WaylandZAuraShell() = default;

bool WaylandZAuraShell::HasBugFix(uint32_t id) {
  return bug_fix_ids_.find(id) != bug_fix_ids_.end();
}

// static
void WaylandZAuraShell::OnLayoutMode(void* data,
                                     struct zaura_shell* zaura_shell,
                                     uint32_t layout_mode) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  auto* screen = self->connection_->wayland_output_manager()->wayland_screen();
  // |screen| is null in some unit test suites.
  if (!screen)
    return;

  switch (layout_mode) {
    case ZAURA_SHELL_LAYOUT_MODE_WINDOWED:
      screen->OnTabletStateChanged(display::TabletState::kInClamshellMode);
      return;
    case ZAURA_SHELL_LAYOUT_MODE_TABLET:
      screen->OnTabletStateChanged(display::TabletState::kInTabletMode);
      return;
  }
}

// static
void WaylandZAuraShell::OnBugFix(void* data,
                                 struct zaura_shell* zaura_shell,
                                 uint32_t id) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  self->bug_fix_ids_.insert(id);
}

}  // namespace ui
