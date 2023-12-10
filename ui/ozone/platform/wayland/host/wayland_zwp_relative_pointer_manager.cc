// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_relative_pointer_manager.h"

#include <relative-pointer-unstable-v1-client-protocol.h>

#include "base/logging.h"
#include "build/buildflag.h"
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

  auto new_pointer_manager =
      wl::Bind<zwp_relative_pointer_manager_v1>(registry, name, kMinVersion);
  if (!new_pointer_manager) {
    LOG(ERROR) << "Failed to bind zwp_relative_pointer_manager_v1";
    return;
  }
  connection->zwp_relative_pointer_manager_ =
      std::make_unique<WaylandZwpRelativePointerManager>(
          new_pointer_manager.release(), connection);
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

  static constexpr zwp_relative_pointer_v1_listener kRelativePointerListener = {
      .relative_motion = &OnRelativeMotion,
  };
  zwp_relative_pointer_v1_add_listener(relative_pointer_.get(),
                                       &kRelativePointerListener, this);

  delegate_->SetRelativePointerMotionEnabled(true);
}

void WaylandZwpRelativePointerManager::DisableRelativePointer() {
  relative_pointer_.reset();
  delegate_->SetRelativePointerMotionEnabled(false);
}

// static
void WaylandZwpRelativePointerManager::OnRelativeMotion(
    void* data,
    zwp_relative_pointer_v1* pointer,
    uint32_t utime_hi,
    uint32_t utime_lo,
    wl_fixed_t dx,
    wl_fixed_t dy,
    wl_fixed_t dx_unaccel,
    wl_fixed_t dy_unaccel) {
  auto* self = static_cast<WaylandZwpRelativePointerManager*>(data);

  gfx::Vector2dF delta = {static_cast<float>(wl_fixed_to_double(dx)),
                          static_cast<float>(wl_fixed_to_double(dy))};
  gfx::Vector2dF delta_unaccel = {
      static_cast<float>(wl_fixed_to_double(dx_unaccel)),
      static_cast<float>(wl_fixed_to_double(dy_unaccel))};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The unit of timestamp should be same as unit used in event events, which is
  // one uint32_t (it is CPU clock time in milliseconds). ChromeOS is using only
  // using utime_lo, and utime_hi should always be zero.
  CHECK(!utime_hi);
#endif
  self->delegate_->OnRelativePointerMotion(
      delta, wl::EventMillisecondsToTimeTicks(utime_lo));
}

}  // namespace ui
