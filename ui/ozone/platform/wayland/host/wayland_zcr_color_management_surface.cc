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
  static constexpr zcr_color_management_surface_v1_listener
      kColorManagementSurfaceListener = {
          .preferred_color_space = &OnPreferredColorSpace,
      };
  zcr_color_management_surface_v1_add_listener(
      zcr_color_management_surface_.get(), &kColorManagementSurfaceListener,
      this);
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
    zcr_color_management_surface_v1* cms,
    wl_output* output) {
  auto* self = static_cast<WaylandZcrColorManagementSurface*>(data);
  DCHECK(self);
  // TODO(b/229646816): Determine what should happen upon receiving this event.
}

}  // namespace ui
