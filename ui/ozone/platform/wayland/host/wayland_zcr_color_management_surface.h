// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_MANAGEMENT_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_MANAGEMENT_SURFACE_H_

#include "base/memory/scoped_refptr.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"

namespace ui {

class WaylandConnection;

// TODO(b/237094484): merge into wayland_surface.h along with
// color_space_creator zcr_color_mangement_surface_v1
class WaylandZcrColorManagementSurface {
 public:
  explicit WaylandZcrColorManagementSurface(
      zcr_color_management_surface_v1* management_surface,
      WaylandConnection* connection);
  WaylandZcrColorManagementSurface(const WaylandZcrColorManagementSurface&) =
      delete;
  WaylandZcrColorManagementSurface& operator=(
      const WaylandZcrColorManagementSurface&) = delete;
  ~WaylandZcrColorManagementSurface();

  void SetDefaultColorSpace();
  void SetColorSpace(
      scoped_refptr<WaylandZcrColorSpace> wayland_zcr_color_space);

 private:
  // zcr_color_management_surface_v1_listener callbacks:
  static void OnPreferredColorSpace(void* data,
                                    zcr_color_management_surface_v1* cms,
                                    wl_output* output);

  wl::Object<zcr_color_management_surface_v1> zcr_color_management_surface_;
  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_MANAGEMENT_SURFACE_H_
