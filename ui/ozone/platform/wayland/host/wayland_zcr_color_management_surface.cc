// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_color_management_surface.h"

#include <chrome-color-management-client-protocol.h>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/base/wayland/color_manager_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space_creator.h"
#include "ui/ozone/platform/wayland/wayland_utils.h"

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
  zcr_color_management_surface_v1_set_default_color_space(
      zcr_color_management_surface_.get());
}

void WaylandZcrColorManagementSurface::SetColorSpace(
    scoped_refptr<WaylandZcrColorSpace> wayland_zcr_color_space) {
  zcr_color_management_surface_v1_set_color_space(
      zcr_color_management_surface_.get(),
      wayland_zcr_color_space->zcr_color_space(),
      ZCR_COLOR_MANAGEMENT_SURFACE_V1_RENDER_INTENT_RELATIVE);
}

// static
void WaylandZcrColorManagementSurface::OnPreferredColorSpace(
    void* data,
    struct zcr_color_management_surface_v1* cms,
    struct wl_output* output) {
  WaylandZcrColorManagementSurface* zcr_color_management_surface =
      static_cast<WaylandZcrColorManagementSurface*>(data);
  DCHECK(zcr_color_management_surface);
  // TODO(b/229646816): Determine what should happen upon receiving this event.
}

}  // namespace ui
