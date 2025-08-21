// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_SPACE_CREATOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_SPACE_CREATOR_H_

#include <cstdint>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"

namespace ui {

// WaylandZcrColorSpaceCreator is used to create a zcr_color_space_v1 object
// that can be sent to exo over wayland protocol.
class WaylandZcrColorSpaceCreator {
 public:
  using CreatorResultCallback =
      base::OnceCallback<void(scoped_refptr<WaylandZcrColorSpace>,
                              std::optional<uint32_t>)>;
  WaylandZcrColorSpaceCreator(wl::Object<zcr_color_space_creator_v1> creator,
                              CreatorResultCallback on_creation);
  WaylandZcrColorSpaceCreator(const WaylandZcrColorSpaceCreator&) = delete;
  WaylandZcrColorSpaceCreator& operator=(const WaylandZcrColorSpaceCreator&) =
      delete;
  ~WaylandZcrColorSpaceCreator();

 private:
  // zcr_color_space_creator_v1_listener callbacks:
  static void OnCreated(void* data,
                        zcr_color_space_creator_v1* csc,
                        zcr_color_space_v1* color_space);
  static void OnError(void* data,
                      zcr_color_space_creator_v1* csc,
                      uint32_t error);

  wl::Object<zcr_color_space_creator_v1> zcr_color_space_creator_;
  CreatorResultCallback on_creation_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_SPACE_CREATOR_H_
