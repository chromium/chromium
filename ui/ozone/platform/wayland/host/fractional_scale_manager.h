// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_FRACTIONAL_SCALE_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_FRACTIONAL_SCALE_MANAGER_H_

#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

// Sets up the global wp_fractional_scale_manager_v1 instance.
class FractionalScaleManager
    : public wl::GlobalObjectRegistrar<FractionalScaleManager> {
 public:
  static constexpr char kInterfaceName[] = "wp_fractional_scale_manager_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  FractionalScaleManager() = delete;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_FRACTIONAL_SCALE_MANAGER_H_
