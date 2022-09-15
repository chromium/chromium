// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zxdg_toplevel_v6_wrapper_impl.h"

#include <xdg-decoration-unstable-v1-client-protocol.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/hit_test.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/shell_surface_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/zxdg_surface_v6_wrapper_impl.h"

namespace ui {

namespace {

absl::optional<wl::Serial> GetSerialForMoveResize(
    WaylandConnection* connection) {
  return connection->serial_tracker().GetSerial({wl::SerialType::kTouchPress,
                                                 wl::SerialType::kMousePress,
                                                 wl::SerialType::kKeyPress});
}

}  // namespace

ZXDGToplevelV6WrapperImpl::ZXDGToplevelV6WrapperImpl(
    std::unique_ptr<ZXDGSurfaceV6WrapperImpl> surface,
    WaylandWindow* wayland_window,
    WaylandConnection* connection)
    : zxdg_surface_v6_wrapper_(std::move(surface)),
      wayland_window_(wayland_window),
      connection_(connection) {}

ZXDGToplevelV6WrapperImpl::~ZXDGToplevelV6WrapperImpl() = default;

bool ZXDGToplevelV6WrapperImpl::Initialize() {
  if (!connection_->shell_v6()) {
    NOTREACHED() << "Wrong shell protocol";
    return false;
  }

  static constexpr zxdg_toplevel_v6_listener zxdg_toplevel_v6_listener = {
      &ConfigureTopLevel,
      &CloseTopLevel,
  };

  if (!zxdg_surface_v6_wrapper_)
    return false;

  zxdg_toplevel_v6_.reset(
      zxdg_surface_v6_get_toplevel(zxdg_surface_v6_wrapper_->zxdg_surface()));
  if (!zxdg_toplevel_v6_) {
    LOG(ERROR) << "Failed to create zxdg_toplevel";
    return false;
  }
  connection_->wayland_window_manager()->NotifyWindowRoleAssigned(
      wayland_window_);
  zxdg_toplevel_v6_add_listener(zxdg_toplevel_v6_.get(),
                                &zxdg_toplevel_v6_listener, this);

  return true;
}

void ZXDGToplevelV6WrapperImpl::SetMaximized() {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_maximized(zxdg_toplevel_v6_.get());
}

void ZXDGToplevelV6WrapperImpl::UnSetMaximized() {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_unset_maximized(zxdg_toplevel_v6_.get());
}

void ZXDGToplevelV6WrapperImpl::SetFullscreen() {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_fullscreen(zxdg_toplevel_v6_.get(), nullptr);
}

void ZXDGToplevelV6WrapperImpl::UnSetFullscreen() {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_unset_fullscreen(zxdg_toplevel_v6_.get());
}

void ZXDGToplevelV6WrapperImpl::SetMinimized() {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_minimized(zxdg_toplevel_v6_.get());
}

void ZXDGToplevelV6WrapperImpl::SurfaceMove(WaylandConnection* connection) {
  DCHECK(zxdg_toplevel_v6_);
  DCHECK(connection_->seat());

  if (auto serial = GetSerialForMoveResize(connection)) {
    zxdg_toplevel_v6_move(zxdg_toplevel_v6_.get(),
                          connection->seat()->wl_object(), serial->value);
  }
}

void ZXDGToplevelV6WrapperImpl::SurfaceResize(WaylandConnection* connection,
                                              uint32_t hittest) {
  DCHECK(zxdg_toplevel_v6_);
  DCHECK(connection_->seat());

  if (auto serial = GetSerialForMoveResize(connection)) {
    zxdg_toplevel_v6_resize(zxdg_toplevel_v6_.get(),
                            connection->seat()->wl_object(), serial->value,
                            wl::IdentifyDirection(*connection, hittest));
  }
}

void ZXDGToplevelV6WrapperImpl::SetTitle(const std::u16string& title) {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_title(zxdg_toplevel_v6_.get(),
                             base::UTF16ToUTF8(title).c_str());
}

void ZXDGToplevelV6WrapperImpl::SetWindowGeometry(const gfx::Rect& bounds) {
  zxdg_surface_v6_wrapper_->SetWindowGeometry(bounds);
}

void ZXDGToplevelV6WrapperImpl::SetMinSize(int32_t width, int32_t height) {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_min_size(zxdg_toplevel_v6_.get(), width, height);
}

void ZXDGToplevelV6WrapperImpl::SetMaxSize(int32_t width, int32_t height) {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_max_size(zxdg_toplevel_v6_.get(), width, height);
}

void ZXDGToplevelV6WrapperImpl::SetAppId(const std::string& app_id) {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_app_id(zxdg_toplevel_v6_.get(), app_id.c_str());
}

void ZXDGToplevelV6WrapperImpl::SetDecoration(DecorationMode decoration) {}

void ZXDGToplevelV6WrapperImpl::AckConfigure(uint32_t serial) {
  DCHECK(zxdg_surface_v6_wrapper_);
  zxdg_surface_v6_wrapper_->AckConfigure(serial);
}

bool ZXDGToplevelV6WrapperImpl::IsConfigured() {
  DCHECK(zxdg_surface_v6_wrapper_);
  return zxdg_surface_v6_wrapper_->IsConfigured();
}

// static
void ZXDGToplevelV6WrapperImpl::ConfigureTopLevel(
    void* data,
    struct zxdg_toplevel_v6* zxdg_toplevel_v6,
    int32_t width,
    int32_t height,
    struct wl_array* states) {
  auto* surface = static_cast<ZXDGToplevelV6WrapperImpl*>(data);
  DCHECK(surface);

  surface->wayland_window_->HandleToplevelConfigure(
      width, height,
      {
          .is_maximized =
              CheckIfWlArrayHasValue(states, ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED),
          .is_fullscreen =
              CheckIfWlArrayHasValue(states, ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN),
          .is_activated =
              CheckIfWlArrayHasValue(states, ZXDG_TOPLEVEL_V6_STATE_ACTIVATED),
      });
}

// static
void ZXDGToplevelV6WrapperImpl::CloseTopLevel(
    void* data,
    struct zxdg_toplevel_v6* zxdg_toplevel_v6) {
  auto* surface = static_cast<ZXDGToplevelV6WrapperImpl*>(data);
  DCHECK(surface);
  surface->wayland_window_->OnCloseRequest();
}

ZXDGSurfaceV6WrapperImpl* ZXDGToplevelV6WrapperImpl::zxdg_surface_v6_wrapper()
    const {
  DCHECK(zxdg_surface_v6_wrapper_.get());
  return zxdg_surface_v6_wrapper_.get();
}

void ZXDGToplevelV6WrapperImpl::Lock(WaylandOrientationLockType lock_type) {}

void ZXDGToplevelV6WrapperImpl::Unlock() {}

void ZXDGToplevelV6WrapperImpl::RequestWindowBounds(const gfx::Rect& bounds) {
  NOTREACHED();
}

void ZXDGToplevelV6WrapperImpl::SetRestoreInfo(int32_t, int32_t) {}

void ZXDGToplevelV6WrapperImpl::SetRestoreInfoWithWindowIdSource(
    int32_t,
    const std::string&) {}

void ZXDGToplevelV6WrapperImpl::SetSystemModal(bool modal) {
  NOTREACHED();
}

bool ZXDGToplevelV6WrapperImpl::SupportsScreenCoordinates() const {
  return false;
}

void ZXDGToplevelV6WrapperImpl::EnableScreenCoordinates() {}

void ZXDGToplevelV6WrapperImpl::SetFloat() {}

void ZXDGToplevelV6WrapperImpl::UnSetFloat() {}

void ZXDGToplevelV6WrapperImpl::SetZOrder(ZOrderLevel z_order) {}

}  // namespace ui
