// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_SPACE_CREATOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_SPACE_CREATOR_H_

#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"

namespace ui {

// WaylandZcrColorSpaceCreator is used to create a zcr_color_space_v1 object
// that can be sent to exo over wayland protocol.
class WaylandZcrColorSpaceCreator {
 public:
  WaylandZcrColorSpaceCreator(
      struct zcr_color_space_creator_v1* creator,
      struct zcr_color_management_surface_v1* management_surface);
  WaylandZcrColorSpaceCreator(const WaylandZcrColorSpaceCreator&) = delete;
  WaylandZcrColorSpaceCreator& operator=(const WaylandZcrColorSpaceCreator&) =
      delete;
  ~WaylandZcrColorSpaceCreator();

 private:
  // zcr_color_space_creator_v1_listener
  static void OnCreated(void* data,
                        struct zcr_color_space_creator_v1* css,
                        struct zcr_color_space_v1* color_space);
  static void OnError(void* data,
                      struct zcr_color_space_creator_v1* css,
                      uint32_t error);

  wl::Object<zcr_color_space_creator_v1> zcr_color_space_creator_;
  zcr_color_management_surface_v1* zcr_color_management_surface_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_SPACE_CREATOR_H_
