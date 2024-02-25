// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_constraints.h"

#include <pointer-constraints-unstable-v1-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_relative_pointer_manager.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
}

// static
constexpr char WaylandZwpPointerConstraints::kInterfaceName[];

// static
void WaylandZwpPointerConstraints::Instantiate(WaylandConnection* connection,
                                               wl_registry* registry,
                                               uint32_t name,
                                               const std::string& interface,
                                               uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->zwp_pointer_constraints_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto zwp_pointer_constraints_v1 =
      wl::Bind<struct zwp_pointer_constraints_v1>(registry, name, kMinVersion);
  if (!zwp_pointer_constraints_v1) {
    LOG(ERROR) << "Failed to bind wp_pointer_constraints_v1";
    return;
  }
  connection->zwp_pointer_constraints_ =
      std::make_unique<WaylandZwpPointerConstraints>(
          zwp_pointer_constraints_v1.release(), connection);
}

WaylandZwpPointerConstraints::WaylandZwpPointerConstraints(
    zwp_pointer_constraints_v1* pointer_constraints,
    WaylandConnection* connection)
    : obj_(pointer_constraints), connection_(connection) {
  DCHECK(obj_);
  DCHECK(connection_);
}

WaylandZwpPointerConstraints::~WaylandZwpPointerConstraints() = default;

void WaylandZwpPointerConstraints::LockPointer(WaylandSurface* surface) {
  locked_pointer_.reset(zwp_pointer_constraints_v1_lock_pointer(
      obj_.get(), surface->surface(),
      connection_->seat()->pointer()->wl_object(), nullptr,
      ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT));

  static constexpr zwp_locked_pointer_v1_listener kLockedPointerListener = {
      .locked = &OnLocked,
      .unlocked = &OnUnlocked,
  };
  zwp_locked_pointer_v1_add_listener(locked_pointer_.get(),
                                     &kLockedPointerListener, this);
}

void WaylandZwpPointerConstraints::UnlockPointer() {
  locked_pointer_.reset();
  connection_->zwp_relative_pointer_manager()->DisableRelativePointer();
}

// static
void WaylandZwpPointerConstraints::OnLocked(
    void* data,
    struct zwp_locked_pointer_v1* locked_pointer) {
  auto* self = static_cast<WaylandZwpPointerConstraints*>(data);
  self->connection_->zwp_relative_pointer_manager()->EnableRelativePointer();
}

// static
void WaylandZwpPointerConstraints::OnUnlocked(
    void* data,
    struct zwp_locked_pointer_v1* locked_pointer) {
  auto* self = static_cast<WaylandZwpPointerConstraints*>(data);
  self->UnlockPointer();
}

}  // namespace ui
