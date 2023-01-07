// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_OVERLAY_PRIORITIZER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_OVERLAY_PRIORITIZER_H_

#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;

// Wraps the overlay-prioritizer, which is provided via
// overlay_prioritizer interface.
class OverlayPrioritizer
    : public wl::GlobalObjectRegistrar<OverlayPrioritizer> {
 public:
  static constexpr char kInterfaceName[] = "overlay_prioritizer";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  explicit OverlayPrioritizer(overlay_prioritizer* prioritizer,
                              WaylandConnection* connection);
  OverlayPrioritizer(const OverlayPrioritizer&) = delete;
  OverlayPrioritizer& operator=(const OverlayPrioritizer&) = delete;
  ~OverlayPrioritizer();

  wl::Object<overlay_prioritized_surface> CreateOverlayPrioritizedSurface(
      wl_surface* surface);

 private:
  // Wayland object wrapped by this class.
  wl::Object<overlay_prioritizer> prioritizer_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_OVERLAY_PRIORITIZER_H_
