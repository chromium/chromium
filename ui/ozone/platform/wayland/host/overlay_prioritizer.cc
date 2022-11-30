// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/overlay_prioritizer.h"

#include <overlay-prioritizer-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
}

// static
constexpr char OverlayPrioritizer::kInterfaceName[];

// static
void OverlayPrioritizer::Instantiate(WaylandConnection* connection,
                                     wl_registry* registry,
                                     uint32_t name,
                                     const std::string& interface,
                                     uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->overlay_prioritizer_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto prioritizer = wl::Bind<overlay_prioritizer>(registry, name, kMinVersion);
  if (!prioritizer) {
    LOG(ERROR) << "Failed to bind overlay_prioritizer";
    return;
  }
  connection->overlay_prioritizer_ =
      std::make_unique<OverlayPrioritizer>(prioritizer.release(), connection);
}

OverlayPrioritizer::OverlayPrioritizer(overlay_prioritizer* prioritizer,
                                       WaylandConnection* connection)
    : prioritizer_(prioritizer) {}

OverlayPrioritizer::~OverlayPrioritizer() = default;

wl::Object<overlay_prioritized_surface>
OverlayPrioritizer::CreateOverlayPrioritizedSurface(wl_surface* surface) {
  return wl::Object<overlay_prioritized_surface>(
      overlay_prioritizer_get_overlay_prioritized_surface(prioritizer_.get(),
                                                          surface));
}

}  // namespace ui
