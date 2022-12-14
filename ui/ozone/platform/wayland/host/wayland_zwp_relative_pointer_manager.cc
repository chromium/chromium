// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_relative_pointer_manager.h"

#include <relative-pointer-unstable-v1-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
}

// static
constexpr char WaylandZwpRelativePointerManager::kInterfaceName[];

// static
void WaylandZwpRelativePointerManager::Instantiate(
    WaylandConnection* connection,
    wl_registry* registry,
    uint32_t name,
    const std::string& interface,
    uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->zwp_relative_pointer_manager_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto zwp_relative_pointer_manager_v1 =
      wl::Bind<struct zwp_relative_pointer_manager_v1>(registry, name,
                                                       kMinVersion);
  if (!zwp_relative_pointer_manager_v1) {
    LOG(ERROR) << "Failed to bind zwp_relative_pointer_manager_v1";
    return;
  }
  connection->zwp_relative_pointer_manager_ =
      std::make_unique<WaylandZwpRelativePointerManager>(
          zwp_relative_pointer_manager_v1.release(), connection);
}

WaylandZwpRelativePointerManager::WaylandZwpRelativePointerManager(
    zwp_relative_pointer_manager_v1* relative_pointer_manager,
    WaylandConnection* connection)
    : obj_(relative_pointer_manager),
      connection_(connection),
      delegate_(connection_->event_source()) {
  DCHECK(obj_);
  DCHECK(connection_);
  DCHECK(delegate_);
}

WaylandZwpRelativePointerManager::~WaylandZwpRelativePointerManager() = default;

void WaylandZwpRelativePointerManager::EnableRelativePointer() {
  relative_pointer_.reset(zwp_relative_pointer_manager_v1_get_relative_pointer(
      obj_.get(), connection_->seat()->pointer()->wl_object()));

  static constexpr zwp_relative_pointer_v1_listener relative_pointer_listener =
      {
          &WaylandZwpRelativePointerManager::OnHandleMotion,
      };
  zwp_relative_pointer_v1_add_listener(relative_pointer_.get(),
                                       &relative_pointer_listener, this);
  delegate_->SetRelativePointerMotionEnabled(true);
}

void WaylandZwpRelativePointerManager::DisableRelativePointer() {
  relative_pointer_.reset();
  delegate_->SetRelativePointerMotionEnabled(false);
}

// static
void WaylandZwpRelativePointerManager::OnHandleMotion(
    void* data,
    struct zwp_relative_pointer_v1* pointer,
    uint32_t utime_hi,
    uint32_t utime_lo,
    wl_fixed_t dx,
    wl_fixed_t dy,
    wl_fixed_t dx_unaccel,
    wl_fixed_t dy_unaccel) {
  auto* relative_pointer_manager =
      static_cast<WaylandZwpRelativePointerManager*>(data);

  gfx::Vector2dF delta = {static_cast<float>(wl_fixed_to_double(dx)),
                          static_cast<float>(wl_fixed_to_double(dy))};
  gfx::Vector2dF delta_unaccel = {
      static_cast<float>(wl_fixed_to_double(dx_unaccel)),
      static_cast<float>(wl_fixed_to_double(dy_unaccel))};

  relative_pointer_manager->delegate_->OnRelativePointerMotion(delta);
}

}  // namespace ui
