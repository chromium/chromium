// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"

#include <xdg-decoration-unstable-v1-client-protocol.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/hit_test.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

XDGSurfaceWrapperImpl::XDGSurfaceWrapperImpl(WaylandWindow* wayland_window,
                                             WaylandConnection* connection)
    : wayland_window_(wayland_window), connection_(connection) {}

XDGSurfaceWrapperImpl::~XDGSurfaceWrapperImpl() {}

bool XDGSurfaceWrapperImpl::Initialize(bool with_toplevel) {
  if (connection_->shell())
    return InitializeStable(with_toplevel);
  else if (connection_->shell_v6())
    return InitializeV6(with_toplevel);
  NOTREACHED() << "Wrong shell protocol";
  return false;
}

void XDGSurfaceWrapperImpl::SetMaximized() {
  if (xdg_toplevel_) {
    xdg_toplevel_set_maximized(xdg_toplevel_.get());
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_set_maximized(zxdg_toplevel_v6_.get());
  }
}

void XDGSurfaceWrapperImpl::UnSetMaximized() {
  if (xdg_toplevel_) {
    xdg_toplevel_unset_maximized(xdg_toplevel_.get());
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_unset_maximized(zxdg_toplevel_v6_.get());
  }
}

void XDGSurfaceWrapperImpl::SetFullscreen() {
  if (xdg_toplevel_) {
    xdg_toplevel_set_fullscreen(xdg_toplevel_.get(), nullptr);
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_set_fullscreen(zxdg_toplevel_v6_.get(), nullptr);
  }
}

void XDGSurfaceWrapperImpl::UnSetFullscreen() {
  if (xdg_toplevel_) {
    xdg_toplevel_unset_fullscreen(xdg_toplevel_.get());
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_unset_fullscreen(zxdg_toplevel_v6_.get());
  }
}

void XDGSurfaceWrapperImpl::SetMinimized() {
  if (xdg_toplevel_) {
    xdg_toplevel_set_minimized(xdg_toplevel_.get());
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_set_minimized(zxdg_toplevel_v6_.get());
  }
}

void XDGSurfaceWrapperImpl::SurfaceMove(WaylandConnection* connection) {
  if (xdg_toplevel_) {
    xdg_toplevel_move(xdg_toplevel_.get(), connection_->seat(),
                      connection_->serial());
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_move(zxdg_toplevel_v6_.get(), connection_->seat(),
                          connection_->serial());
  }
}

void XDGSurfaceWrapperImpl::SurfaceResize(WaylandConnection* connection,
                                          uint32_t hittest) {
  if (xdg_toplevel_) {
    xdg_toplevel_resize(xdg_toplevel_.get(), connection_->seat(),
                        connection_->serial(),
                        wl::IdentifyDirection(*connection, hittest));
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_resize(zxdg_toplevel_v6_.get(), connection_->seat(),
                            connection_->serial(),
                            wl::IdentifyDirection(*connection, hittest));
  }
}

void XDGSurfaceWrapperImpl::SetTitle(const base::string16& title) {
  if (xdg_toplevel_) {
    xdg_toplevel_set_title(xdg_toplevel_.get(),
                           base::UTF16ToUTF8(title).c_str());
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_set_title(zxdg_toplevel_v6_.get(),
                               base::UTF16ToUTF8(title).c_str());
  }
}

void XDGSurfaceWrapperImpl::AckConfigure() {
  if (xdg_surface_) {
    xdg_surface_ack_configure(xdg_surface_.get(), pending_configure_serial_);
  } else {
    DCHECK(zxdg_surface_v6_);
    zxdg_surface_v6_ack_configure(zxdg_surface_v6_.get(),
                                  pending_configure_serial_);
  }
  connection_->wayland_window_manager()->NotifyWindowConfigured(
      wayland_window_);
}

void XDGSurfaceWrapperImpl::SetWindowGeometry(const gfx::Rect& bounds) {
  if (xdg_surface_) {
    xdg_surface_set_window_geometry(xdg_surface_.get(), bounds.x(), bounds.y(),
                                    bounds.width(), bounds.height());
  } else {
    DCHECK(zxdg_surface_v6_);
    zxdg_surface_v6_set_window_geometry(zxdg_surface_v6_.get(), bounds.x(),
                                        bounds.y(), bounds.width(),
                                        bounds.height());
  }
}

void XDGSurfaceWrapperImpl::SetMinSize(int32_t width, int32_t height) {
  if (xdg_toplevel_) {
    xdg_toplevel_set_min_size(xdg_toplevel_.get(), width, height);
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_set_min_size(zxdg_toplevel_v6_.get(), width, height);
  }
}

void XDGSurfaceWrapperImpl::SetMaxSize(int32_t width, int32_t height) {
  if (xdg_toplevel_) {
    xdg_toplevel_set_max_size(xdg_toplevel_.get(), width, height);
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_set_max_size(zxdg_toplevel_v6_.get(), width, height);
  }
}

void XDGSurfaceWrapperImpl::SetAppId(const std::string& app_id) {
  if (xdg_toplevel_) {
    xdg_toplevel_set_app_id(xdg_toplevel_.get(), app_id.c_str());
  } else {
    DCHECK(zxdg_toplevel_v6_);
    zxdg_toplevel_v6_set_app_id(zxdg_toplevel_v6_.get(), app_id.c_str());
  }
}

// static
void XDGSurfaceWrapperImpl::ConfigureStable(void* data,
                                            struct xdg_surface* xdg_surface,
                                            uint32_t serial) {
  auto* surface = static_cast<XDGSurfaceWrapperImpl*>(data);
  DCHECK(surface);
  surface->pending_configure_serial_ = serial;

  surface->AckConfigure();
}

// static
void XDGSurfaceWrapperImpl::ConfigureTopLevelStable(
    void* data,
    struct xdg_toplevel* xdg_toplevel,
    int32_t width,
    int32_t height,
    struct wl_array* states) {
  auto* surface = static_cast<XDGSurfaceWrapperImpl*>(data);
  DCHECK(surface);

  bool is_maximized =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_MAXIMIZED);
  bool is_fullscreen =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_FULLSCREEN);
  bool is_activated =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_ACTIVATED);

  surface->wayland_window_->HandleSurfaceConfigure(width, height, is_maximized,
                                                   is_fullscreen, is_activated);
}

// static
void XDGSurfaceWrapperImpl::CloseTopLevelStable(
    void* data,
    struct xdg_toplevel* xdg_toplevel) {
  auto* surface = static_cast<XDGSurfaceWrapperImpl*>(data);
  DCHECK(surface);
  surface->wayland_window_->OnCloseRequest();
}

void XDGSurfaceWrapperImpl::SetTopLevelDecorationMode(
    zxdg_toplevel_decoration_v1_mode requested_mode) {
  if (requested_mode == decoration_mode_)
    return;

  decoration_mode_ = requested_mode;
  zxdg_toplevel_decoration_v1_set_mode(zxdg_toplevel_decoration_.get(),
                                       requested_mode);
}

// static
void XDGSurfaceWrapperImpl::ConfigureV6(void* data,
                                        struct zxdg_surface_v6* zxdg_surface_v6,
                                        uint32_t serial) {
  auto* surface = static_cast<XDGSurfaceWrapperImpl*>(data);
  DCHECK(surface);
  surface->pending_configure_serial_ = serial;

  surface->AckConfigure();
}

// static
void XDGSurfaceWrapperImpl::ConfigureTopLevelV6(
    void* data,
    struct zxdg_toplevel_v6* zxdg_toplevel_v6,
    int32_t width,
    int32_t height,
    struct wl_array* states) {
  auto* surface = static_cast<XDGSurfaceWrapperImpl*>(data);
  DCHECK(surface);

  bool is_maximized =
      CheckIfWlArrayHasValue(states, ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED);
  bool is_fullscreen =
      CheckIfWlArrayHasValue(states, ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN);
  bool is_activated =
      CheckIfWlArrayHasValue(states, ZXDG_TOPLEVEL_V6_STATE_ACTIVATED);

  surface->wayland_window_->HandleSurfaceConfigure(width, height, is_maximized,
                                                   is_fullscreen, is_activated);
}

// static
void XDGSurfaceWrapperImpl::CloseTopLevelV6(
    void* data,
    struct zxdg_toplevel_v6* zxdg_toplevel_v6) {
  auto* surface = static_cast<XDGSurfaceWrapperImpl*>(data);
  DCHECK(surface);
  surface->wayland_window_->OnCloseRequest();
}

zxdg_surface_v6* XDGSurfaceWrapperImpl::zxdg_surface() const {
  DCHECK(zxdg_surface_v6_);
  return zxdg_surface_v6_.get();
}

xdg_surface* XDGSurfaceWrapperImpl::xdg_surface() const {
  DCHECK(xdg_surface_);
  return xdg_surface_.get();
}

// static
void XDGSurfaceWrapperImpl::ConfigureDecoration(
    void* data,
    struct zxdg_toplevel_decoration_v1* decoration,
    uint32_t mode) {
  auto* surface = static_cast<XDGSurfaceWrapperImpl*>(data);
  DCHECK(surface);
  surface->SetTopLevelDecorationMode(
      static_cast<zxdg_toplevel_decoration_v1_mode>(mode));
}

bool XDGSurfaceWrapperImpl::InitializeStable(bool with_toplevel) {
  static const xdg_surface_listener xdg_surface_listener = {
      &XDGSurfaceWrapperImpl::ConfigureStable,
  };
  static const xdg_toplevel_listener xdg_toplevel_listener = {
      &XDGSurfaceWrapperImpl::ConfigureTopLevelStable,
      &XDGSurfaceWrapperImpl::CloseTopLevelStable,
  };

  // if this surface is created for the popup role, mark that it requires
  // configuration acknowledgement on each configure event.
  surface_for_popup_ = !with_toplevel;

  xdg_surface_.reset(xdg_wm_base_get_xdg_surface(
      connection_->shell(), wayland_window_->root_surface()->surface()));
  if (!xdg_surface_) {
    LOG(ERROR) << "Failed to create xdg_surface";
    return false;
  }
  xdg_surface_add_listener(xdg_surface_.get(), &xdg_surface_listener, this);
  // XDGPopup requires a separate surface to be created, so this is just a
  // request to get an xdg_surface for it.
  if (surface_for_popup_) {
    connection_->ScheduleFlush();
    return true;
  }

  xdg_toplevel_.reset(xdg_surface_get_toplevel(xdg_surface_.get()));
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

bool XDGSurfaceWrapperImpl::InitializeV6(bool with_toplevel) {
  static const zxdg_surface_v6_listener zxdg_surface_v6_listener = {
      &XDGSurfaceWrapperImpl::ConfigureV6,
  };
  static const zxdg_toplevel_v6_listener zxdg_toplevel_v6_listener = {
      &XDGSurfaceWrapperImpl::ConfigureTopLevelV6,
      &XDGSurfaceWrapperImpl::CloseTopLevelV6,
  };

  // if this surface is created for the popup role, mark that it requires
  // configuration acknowledgement on each configure event.
  surface_for_popup_ = !with_toplevel;

  zxdg_surface_v6_.reset(zxdg_shell_v6_get_xdg_surface(
      connection_->shell_v6(), wayland_window_->root_surface()->surface()));
  if (!zxdg_surface_v6_) {
    LOG(ERROR) << "Failed to create zxdg_surface";
    return false;
  }
  zxdg_surface_v6_add_listener(zxdg_surface_v6_.get(),
                               &zxdg_surface_v6_listener, this);
  // XDGPopupV6 requires a separate surface to be created, so this is just a
  // request to get an xdg_surface for it.
  if (surface_for_popup_) {
    connection_->ScheduleFlush();
    return true;
  }

  zxdg_toplevel_v6_.reset(zxdg_surface_v6_get_toplevel(zxdg_surface_v6_.get()));
  if (!zxdg_toplevel_v6_) {
    LOG(ERROR) << "Failed to create zxdg_toplevel";
    return false;
  }
  zxdg_toplevel_v6_add_listener(zxdg_toplevel_v6_.get(),
                                &zxdg_toplevel_v6_listener, this);

  wayland_window_->root_surface()->Commit();
  connection_->ScheduleFlush();
  return true;
}

void XDGSurfaceWrapperImpl::InitializeXdgDecoration() {
  if (connection_->xdg_decoration_manager_v1()) {
    DCHECK(!zxdg_toplevel_decoration_);
    static const zxdg_toplevel_decoration_v1_listener decoration_listener = {
        &XDGSurfaceWrapperImpl::ConfigureDecoration,
    };
    zxdg_toplevel_decoration_.reset(
        zxdg_decoration_manager_v1_get_toplevel_decoration(
            connection_->xdg_decoration_manager_v1(), xdg_toplevel_.get()));
    zxdg_toplevel_decoration_v1_add_listener(zxdg_toplevel_decoration_.get(),
                                             &decoration_listener, this);
  }
}

}  // namespace ui
