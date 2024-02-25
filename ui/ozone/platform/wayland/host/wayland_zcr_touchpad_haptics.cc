// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_touchpad_haptics.h"

#include <touchpad-haptics-unstable-v1-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
}

// static
constexpr char WaylandZcrTouchpadHaptics::kInterfaceName[];

// static
void WaylandZcrTouchpadHaptics::Instantiate(WaylandConnection* connection,
                                            wl_registry* registry,
                                            uint32_t name,
                                            const std::string& interface,
                                            uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->zcr_touchpad_haptics_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto zcr_touchpad_haptics =
      wl::Bind<zcr_touchpad_haptics_v1>(registry, name, kMinVersion);
  if (!zcr_touchpad_haptics) {
    LOG(ERROR) << "Failed to bind zcr_touchpad_haptics_v1";
    return;
  }
  connection->zcr_touchpad_haptics_ =
      std::make_unique<WaylandZcrTouchpadHaptics>(
          zcr_touchpad_haptics.release(), connection);
}

WaylandZcrTouchpadHaptics::WaylandZcrTouchpadHaptics(
    zcr_touchpad_haptics_v1* zcr_touchpad_haptics,
    WaylandConnection* connection)
    : obj_(zcr_touchpad_haptics), connection_(connection) {
  DCHECK(obj_);
  DCHECK(connection_);
  static constexpr zcr_touchpad_haptics_v1_listener kTouchpadHapticsListener = {
      .activated = &OnActivated,
      .deactivated = &OnDeactivated,
  };
  zcr_touchpad_haptics_v1_add_listener(obj_.get(), &kTouchpadHapticsListener,
                                       this);
}

WaylandZcrTouchpadHaptics::~WaylandZcrTouchpadHaptics() = default;

// static
void WaylandZcrTouchpadHaptics::OnActivated(
    void* data,
    zcr_touchpad_haptics_v1* touchpad_haptics) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandZcrTouchpadHaptics::OnDeactivated(
    void* data,
    zcr_touchpad_haptics_v1* touchpad_haptics) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandZcrTouchpadHaptics::Play(int32_t effect, int32_t strength) {
  zcr_touchpad_haptics_v1_play(obj_.get(), effect, strength);
}

}  // namespace ui
