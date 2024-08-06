// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/toplevel_icon_manager.h"

#include <xdg-toplevel-icon-v1-client-protocol.h>

#include "base/check_op.h"
#include "base/logging.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {

constexpr uint32_t kMaxVersion = 1;

}  // namespace

// static
void ToplevelIconManager::Instantiate(WaylandConnection* connection,
                                      wl_registry* registry,
                                      uint32_t name,
                                      const std::string& interface,
                                      uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->toplevel_icon_manager_v1_) {
    return;
  }

  auto instance = wl::Bind<::xdg_toplevel_icon_manager_v1>(
      registry, name, std::min(version, kMaxVersion));
  if (!instance) {
    LOG(ERROR) << "Failed to bind " << kInterfaceName;
    return;
  }
  connection->toplevel_icon_manager_v1_ = std::move(instance);
}

}  // namespace ui
