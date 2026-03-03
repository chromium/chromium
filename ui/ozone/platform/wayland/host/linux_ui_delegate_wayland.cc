// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/linux_ui_delegate_wayland.h"

#include <utility>

#include "base/logging.h"
#include "ui/linux/linux_ui_delegate.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_foreign_wrapper.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

LinuxUiDelegateWayland::LinuxUiDelegateWayland(WaylandConnection* connection)
    : connection_(connection) {
  DCHECK(connection_);
}

LinuxUiDelegateWayland::~LinuxUiDelegateWayland() = default;

LinuxUiBackend LinuxUiDelegateWayland::GetBackend() const {
  return LinuxUiBackend::kWayland;
}

void LinuxUiDelegateWayland::ExportWindowHandle(
    gfx::AcceleratedWidget window_id,
    base::OnceCallback<void(std::string)> callback) {
  auto* parent_window = connection_->window_manager()->GetWindow(window_id);
  auto* toplevel_parent = parent_window;
  while (toplevel_parent && !toplevel_parent->AsWaylandToplevelWindow()) {
    toplevel_parent = toplevel_parent->parent_window();
  }

  auto* foreign = connection_->xdg_foreign();
  if (!toplevel_parent || !foreign) {
    std::move(callback).Run("");
    return;
  }

  DCHECK_EQ(toplevel_parent->type(), PlatformWindowType::kWindow);

  foreign->ExportSurfaceToForeign(
      toplevel_parent,
      base::BindOnce(&LinuxUiDelegateWayland::OnHandleForward,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LinuxUiDelegateWayland::OnHandleForward(
    base::OnceCallback<void(std::string)> callback,
    const std::string& handle) {
  std::move(callback).Run("wayland:" + handle);
}

}  // namespace ui
