// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_toplevel_wrapper_impl.h"

#include <xdg-decoration-unstable-v1-client-protocol.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/hit_test.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/shell_surface_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"

namespace ui {

XDGToplevelWrapperImpl::XDGToplevelWrapperImpl(
    std::unique_ptr<XDGSurfaceWrapperImpl> surface,
    WaylandWindow* wayland_window,
    WaylandConnection* connection)
    : xdg_surface_wrapper_(std::move(surface)),
      wayland_window_(wayland_window),
      connection_(connection),
      decoration_mode_(DecorationMode::kClientSide) {}

XDGToplevelWrapperImpl::~XDGToplevelWrapperImpl() = default;

bool XDGToplevelWrapperImpl::Initialize() {
  if (!connection_->shell()) {
    NOTREACHED() << "Wrong shell protocol";
    return false;
  }

  static const xdg_toplevel_listener xdg_toplevel_listener = {
      &XDGToplevelWrapperImpl::ConfigureTopLevel,
      &XDGToplevelWrapperImpl::CloseTopLevel,
  };

  if (!xdg_surface_wrapper_)
    return false;

  xdg_toplevel_.reset(
      xdg_surface_get_toplevel(xdg_surface_wrapper_->xdg_surface()));
  if (!xdg_toplevel_) {
    LOG(ERROR) << "Failed to create xdg_toplevel";
    return false;
  }

  xdg_toplevel_add_listener(xdg_toplevel_.get(), &xdg_toplevel_listener, this);

  InitializeXdgDecoration();

  wayland_window_->root_surface()->Commit();
  connection_->ScheduleFlush();
  return true;
}

void XDGToplevelWrapperImpl::SetMaximized() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_maximized(xdg_toplevel_.get());
}

void XDGToplevelWrapperImpl::UnSetMaximized() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_unset_maximized(xdg_toplevel_.get());
}

void XDGToplevelWrapperImpl::SetFullscreen() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_fullscreen(xdg_toplevel_.get(), nullptr);
}

void XDGToplevelWrapperImpl::UnSetFullscreen() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_unset_fullscreen(xdg_toplevel_.get());
}

void XDGToplevelWrapperImpl::SetMinimized() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_minimized(xdg_toplevel_.get());
}

void XDGToplevelWrapperImpl::SurfaceMove(WaylandConnection* connection) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_move(xdg_toplevel_.get(), connection->seat(),
                    connection->serial());
}

void XDGToplevelWrapperImpl::SurfaceResize(WaylandConnection* connection,
                                           uint32_t hittest) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_resize(xdg_toplevel_.get(), connection->seat(),
                      connection->serial(),
                      wl::IdentifyDirection(*connection, hittest));
}

void XDGToplevelWrapperImpl::SetTitle(const base::string16& title) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_title(xdg_toplevel_.get(), base::UTF16ToUTF8(title).c_str());
}

void XDGToplevelWrapperImpl::SetWindowGeometry(const gfx::Rect& bounds) {
  xdg_surface_wrapper_->SetWindowGeometry(bounds);
}

void XDGToplevelWrapperImpl::SetMinSize(int32_t width, int32_t height) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_min_size(xdg_toplevel_.get(), width, height);
}

void XDGToplevelWrapperImpl::SetMaxSize(int32_t width, int32_t height) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_max_size(xdg_toplevel_.get(), width, height);
}

void XDGToplevelWrapperImpl::SetAppId(const std::string& app_id) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_app_id(xdg_toplevel_.get(), app_id.c_str());
}

void XDGToplevelWrapperImpl::SetDecoration(DecorationMode decoration) {
  SetTopLevelDecorationMode(decoration);
}

void XDGToplevelWrapperImpl::AckConfigure(uint32_t serial) {
  DCHECK(xdg_surface_wrapper_);
  xdg_surface_wrapper_->AckConfigure(serial);
}

// static
void XDGToplevelWrapperImpl::ConfigureTopLevel(
    void* data,
    struct xdg_toplevel* xdg_toplevel,
    int32_t width,
    int32_t height,
    struct wl_array* states) {
  auto* surface = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(surface);

  bool is_maximized =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_MAXIMIZED);
  bool is_fullscreen =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_FULLSCREEN);
  bool is_activated =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_ACTIVATED);

  surface->wayland_window_->HandleToplevelConfigure(
      width, height, is_maximized, is_fullscreen, is_activated);
}

// static
void XDGToplevelWrapperImpl::CloseTopLevel(void* data,
                                           struct xdg_toplevel* xdg_toplevel) {
  auto* surface = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(surface);
  surface->wayland_window_->OnCloseRequest();
}

void XDGToplevelWrapperImpl::SetTopLevelDecorationMode(
    DecorationMode requested_mode) {
  if (!zxdg_toplevel_decoration_ || requested_mode == decoration_mode_)
    return;

  decoration_mode_ = requested_mode;
  zxdg_toplevel_decoration_v1_set_mode(zxdg_toplevel_decoration_.get(),
                                       static_cast<uint32_t>(requested_mode));
}

// static
void XDGToplevelWrapperImpl::ConfigureDecoration(
    void* data,
    struct zxdg_toplevel_decoration_v1* decoration,
    uint32_t mode) {
  auto* surface = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(surface);
  surface->SetTopLevelDecorationMode(static_cast<DecorationMode>(mode));
}

void XDGToplevelWrapperImpl::InitializeXdgDecoration() {
  if (connection_->xdg_decoration_manager_v1()) {
    DCHECK(!zxdg_toplevel_decoration_);
    static const zxdg_toplevel_decoration_v1_listener decoration_listener = {
        &XDGToplevelWrapperImpl::ConfigureDecoration,
    };
    zxdg_toplevel_decoration_.reset(
        zxdg_decoration_manager_v1_get_toplevel_decoration(
            connection_->xdg_decoration_manager_v1(), xdg_toplevel_.get()));
    zxdg_toplevel_decoration_v1_add_listener(zxdg_toplevel_decoration_.get(),
                                             &decoration_listener, this);
  }
}

XDGSurfaceWrapperImpl* XDGToplevelWrapperImpl::xdg_surface_wrapper() const {
  DCHECK(xdg_surface_wrapper_.get());
  return xdg_surface_wrapper_.get();
}

}  // namespace ui
