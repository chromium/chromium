// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_surface.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandZAuraSurface::WaylandZAuraSurface(zaura_shell* zaura_shell,
                                         wl_surface* wl_surface,
                                         WaylandConnection* connection)
    : zaura_surface_(zaura_shell_get_aura_surface(zaura_shell, wl_surface)),
      connection_(connection) {
  CHECK(zaura_surface_);
}

WaylandZAuraSurface::~WaylandZAuraSurface() = default;

bool WaylandZAuraSurface::SetFrame(zaura_surface_frame_type type) {
  if (IsSupported(ZAURA_SURFACE_SET_FRAME_SINCE_VERSION)) {
    zaura_surface_set_frame(zaura_surface_.get(), type);
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::Activate() {
  if (IsSupported(ZAURA_SURFACE_ACTIVATE_SINCE_VERSION)) {
    zaura_surface_activate(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetFullscreenMode(
    zaura_surface_fullscreen_mode mode) {
  if (IsSupported(ZAURA_SURFACE_SET_FULLSCREEN_MODE_SINCE_VERSION)) {
    zaura_surface_set_fullscreen_mode(zaura_surface_.get(), mode);
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetServerStartResize() {
  if (IsSupported(ZAURA_SURFACE_SET_SERVER_START_RESIZE_SINCE_VERSION)) {
    zaura_surface_set_server_start_resize(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetCanGoBack() {
  if (IsSupported(ZAURA_SURFACE_SET_CAN_GO_BACK_SINCE_VERSION)) {
    zaura_surface_set_can_go_back(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::UnsetCanGoBack() {
  if (IsSupported(ZAURA_SURFACE_UNSET_CAN_GO_BACK_SINCE_VERSION)) {
    zaura_surface_unset_can_go_back(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetPip() {
  if (IsSupported(ZAURA_SURFACE_SET_PIP_SINCE_VERSION)) {
    zaura_surface_set_pip(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetAspectRatio(float width, float height) {
  if (IsSupported(ZAURA_SURFACE_SET_ASPECT_RATIO_SINCE_VERSION)) {
    zaura_surface_set_aspect_ratio(zaura_surface_.get(), width, height);
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetInitialWorkspace(int workspace) {
  if (IsSupported(ZAURA_SURFACE_SET_INITIAL_WORKSPACE_SINCE_VERSION)) {
    zaura_surface_set_initial_workspace(
        zaura_surface_.get(), base::NumberToString(workspace).c_str());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SupportsActivate() const {
  return IsSupported(ZAURA_SURFACE_ACTIVATE_SINCE_VERSION);
}

bool WaylandZAuraSurface::SupportsSetServerStartResize() const {
  return IsSupported(ZAURA_SURFACE_SET_SERVER_START_RESIZE_SINCE_VERSION);
}

bool WaylandZAuraSurface::IsSupported(uint32_t version) const {
  CHECK(zaura_surface_);
  return zaura_surface_get_version(zaura_surface_.get()) >= version;
}

}  // namespace ui
