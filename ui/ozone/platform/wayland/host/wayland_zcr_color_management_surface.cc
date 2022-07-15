// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_color_management_surface.h"

#include <chrome-color-management-client-protocol.h>
#include <memory>

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandZcrColorManagementSurface::WaylandZcrColorManagementSurface(
    zcr_color_management_surface_v1* color_management_surface,
    WaylandConnection* connection)
    : zcr_color_management_surface_(color_management_surface),
      connection_(connection) {
  DCHECK(color_management_surface);
  static const zcr_color_management_surface_v1_listener listener = {
      &WaylandZcrColorManagementSurface::OnPreferredColorSpace,
  };

  zcr_color_management_surface_v1_add_listener(
      zcr_color_management_surface_.get(), &listener, this);
}

WaylandZcrColorManagementSurface::~WaylandZcrColorManagementSurface() = default;

void WaylandZcrColorManagementSurface::SetDefaultColorSpace() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandZcrColorManagementSurface::SetColorSpace(
    gfx::ColorSpace color_space) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandZcrColorManagementSurface::OnPreferredColorSpace(
    void* data,
    struct zcr_color_management_surface_v1* cms,
    struct wl_output* output) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
