// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_toplevel_wrapper_impl.h"

#include <xdg-decoration-unstable-v1-client-protocol.h>
#include <xdg-toplevel-icon-v1-client-protocol.h>

#include <optional>

#include "base/bit_cast.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/shell_surface_wrapper.h"
#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"

namespace ui {

namespace {

static_assert(sizeof(uint32_t) == sizeof(float),
              "Sizes much match for reinterpret cast to be meaningful");

XDGToplevelWrapperImpl::DecorationMode ToDecorationMode(uint32_t mode) {
  switch (mode) {
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
      return XDGToplevelWrapperImpl::DecorationMode::kClientSide;
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
      return XDGToplevelWrapperImpl::DecorationMode::kServerSide;
    default:
      NOTREACHED();
  }
}

uint32_t ToInt32(XDGToplevelWrapperImpl::DecorationMode mode) {
  switch (mode) {
    case XDGToplevelWrapperImpl::DecorationMode::kClientSide:
      return ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    case XDGToplevelWrapperImpl::DecorationMode::kServerSide:
      return ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    default:
      NOTREACHED();
  }
}

std::optional<wl::Serial> GetSerialForMoveResize(
    WaylandConnection* connection) {
  return connection->serial_tracker().GetSerial({wl::SerialType::kTouchPress,
                                                 wl::SerialType::kMousePress,
                                                 wl::SerialType::kKeyPress});
}

}  // namespace

XDGToplevelWrapperImpl::XDGToplevelWrapperImpl(
    std::unique_ptr<XDGSurfaceWrapperImpl> surface,
    WaylandWindow* wayland_window,
    WaylandConnection* connection)
    : xdg_surface_wrapper_(std::move(surface)),
      wayland_window_(wayland_window),
      connection_(connection),
      decoration_mode_(DecorationMode::kNone) {}

XDGToplevelWrapperImpl::~XDGToplevelWrapperImpl() = default;

bool XDGToplevelWrapperImpl::Initialize() {
  if (!connection_->shell()) {
    NOTREACHED() << "Wrong shell protocol";
  }

  if (!xdg_surface_wrapper_) {
    return false;
  }

  xdg_toplevel_.reset(
      xdg_surface_get_toplevel(xdg_surface_wrapper_->xdg_surface()));
  if (!xdg_toplevel_) {
    LOG(ERROR) << "Failed to create xdg_toplevel";
    return false;
  }
  connection_->window_manager()->NotifyWindowRoleAssigned(wayland_window_);

  static constexpr xdg_toplevel_listener kXdgToplevelListener = {
      .configure = &OnToplevelConfigure,
      .close = &OnToplevelClose,
      // Since v4
      .configure_bounds = &OnConfigureBounds,
      // Since v5
      .wm_capabilities = &OnWmCapabilities,
  };
  xdg_toplevel_add_listener(xdg_toplevel_.get(), &kXdgToplevelListener, this);

  InitializeXdgDecoration();

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

void XDGToplevelWrapperImpl::SetFullscreen(WaylandOutput* wayland_output) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_fullscreen(
      xdg_toplevel_.get(),
      wayland_output ? wayland_output->get_output() : nullptr);
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
  if (auto serial = GetSerialForMoveResize(connection)) {
    xdg_toplevel_move(xdg_toplevel_.get(), connection->seat()->wl_object(),
                      serial->value);
  }
}

void XDGToplevelWrapperImpl::SurfaceResize(WaylandConnection* connection,
                                           uint32_t hittest) {
  DCHECK(xdg_toplevel_);
  if (auto serial = GetSerialForMoveResize(connection)) {
    xdg_toplevel_resize(xdg_toplevel_.get(), connection->seat()->wl_object(),
                        serial->value, wl::IdentifyDirection(hittest));
  }
}

void XDGToplevelWrapperImpl::SetTitle(const std::u16string& title) {
  DCHECK(xdg_toplevel_);

  // TODO(crbug.com/40785817): find a better way to handle long titles, or
  // change this logic completely (and at the platform-agnostic level) because a
  // title that long does not make any sense.
  //
  // A long title may exceed the maximum size of the Wayland event sent below
  // upon calling xdg_toplevel_set_title(), which results in a fatal Wayland
  // communication error and termination of the process.  4096 bytes is the
  // limit for the size of the entire message; here we set 4000 as the maximum
  // length of the string so it would fit the message with some margin.
  const size_t kMaxLengh = 4000;
  auto short_title = base::UTF16ToUTF8(title);
  if (short_title.size() > kMaxLengh) {
    short_title.resize(kMaxLengh);
  }
  xdg_toplevel_set_title(xdg_toplevel_.get(), short_title.c_str());
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

bool XDGToplevelWrapperImpl::IsConfigured() {
  DCHECK(xdg_surface_wrapper_);
  return xdg_surface_wrapper_->IsConfigured();
}

// static
void XDGToplevelWrapperImpl::OnToplevelConfigure(void* data,
                                                 xdg_toplevel* toplevel,
                                                 int32_t width,
                                                 int32_t height,
                                                 wl_array* states) {
  auto* self = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(self);

  WaylandWindow::WindowStates window_states;
  window_states.is_maximized =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_MAXIMIZED);
  window_states.is_fullscreen =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_FULLSCREEN);
  window_states.is_activated =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_ACTIVATED);

  if (xdg_toplevel_get_version(toplevel) >=
      XDG_TOPLEVEL_STATE_SUSPENDED_SINCE_VERSION) {
    window_states.is_suspended =
        CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_SUSPENDED);
  }
  if (xdg_toplevel_get_version(toplevel) >=
      XDG_TOPLEVEL_STATE_TILED_LEFT_SINCE_VERSION) {
    // All four tiled states have the same since version, so it is enough to
    // check only one.
    window_states.tiled_edges = {
        .left = CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_TILED_LEFT),
        .right = CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_TILED_RIGHT),
        .top = CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_TILED_TOP),
        .bottom =
            CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_TILED_BOTTOM)};
  }

  self->wayland_window_->HandleToplevelConfigure(width, height, window_states);
}

// static
void XDGToplevelWrapperImpl::OnToplevelClose(void* data,
                                             xdg_toplevel* toplevel) {
  auto* self = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(self);
  self->wayland_window_->OnCloseRequest();
}

// static
void XDGToplevelWrapperImpl::OnConfigureBounds(void* data,
                                               xdg_toplevel* toplevel,
                                               int32_t width,
                                               int32_t height) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void XDGToplevelWrapperImpl::OnWmCapabilities(void* data,
                                              xdg_toplevel* toplevel,
                                              wl_array* capabilities) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void XDGToplevelWrapperImpl::OnDecorationConfigure(
    void* data,
    zxdg_toplevel_decoration_v1* decoration,
    uint32_t mode) {
  auto* self = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(self);
  self->decoration_mode_ = ToDecorationMode(mode);
}

void XDGToplevelWrapperImpl::SetTopLevelDecorationMode(
    DecorationMode requested_mode) {
  if (!zxdg_toplevel_decoration_ || requested_mode == decoration_mode_) {
    return;
  }

  zxdg_toplevel_decoration_v1_set_mode(zxdg_toplevel_decoration_.get(),
                                       ToInt32(requested_mode));
}

void XDGToplevelWrapperImpl::InitializeXdgDecoration() {
  if (connection_->xdg_decoration_manager_v1()) {
    DCHECK(!zxdg_toplevel_decoration_);
    zxdg_toplevel_decoration_.reset(
        zxdg_decoration_manager_v1_get_toplevel_decoration(
            connection_->xdg_decoration_manager_v1(), xdg_toplevel_.get()));

    static constexpr zxdg_toplevel_decoration_v1_listener
        kToplevelDecorationListener = {
            .configure = &OnDecorationConfigure,
        };
    zxdg_toplevel_decoration_v1_add_listener(
        zxdg_toplevel_decoration_.get(), &kToplevelDecorationListener, this);
  }
}

wl::Object<wl_region> XDGToplevelWrapperImpl::CreateAndAddRegion(
    const std::vector<gfx::Rect>& shape) {
  wl::Object<wl_region> region(
      wl_compositor_create_region(connection_->compositor()));

  for (const auto& rect : shape) {
    wl_region_add(region.get(), rect.x(), rect.y(), rect.width(),
                  rect.height());
  }

  return region;
}

XDGSurfaceWrapperImpl* XDGToplevelWrapperImpl::xdg_surface_wrapper() const {
  DCHECK(xdg_surface_wrapper_.get());
  return xdg_surface_wrapper_.get();
}

void XDGToplevelWrapperImpl::SetSystemModal(bool modal) {
  // TODO(crbug.com/378465003): Linux/Wayland can set a window to be modal via
  // xdg-dialog-v1 protocol. Consider support for that.
  // See https://wayland.app/protocols/xdg-dialog-v1
}

void XDGToplevelWrapperImpl::SetIcon(const gfx::ImageSkia& icon) {
  auto* manager = connection_->toplevel_icon_manager_v1();
  if (!manager) {
    return;
  }

  if (icon.isNull()) {
    xdg_toplevel_icon_manager_v1_set_icon(manager, xdg_toplevel_.get(),
                                          nullptr);
    return;
  }

  std::vector<std::pair<WaylandShmBuffer, float>> buffers;
  auto* xdg_icon = xdg_toplevel_icon_manager_v1_create_icon(manager);
  for (const auto& rep : icon.image_reps()) {
    const auto& bitmap = rep.GetBitmap();
    gfx::Size image_size = gfx::SkISizeToSize(bitmap.dimensions());
    if (image_size.IsEmpty() || image_size.width() != image_size.height()) {
      // The toplevel icon protocol requires square icons.
      continue;
    }

    WaylandShmBuffer buffer(connection_->buffer_factory(), image_size);
    if (!buffer.IsValid()) {
      LOG(ERROR) << "Failed to create SHM buffer for icon Bitmap.";
      return;
    }

    wl::DrawBitmap(bitmap, &buffer);
    buffers.emplace_back(std::move(buffer), rep.scale());
  }
  for (const auto& [buffer, scale] : buffers) {
    xdg_toplevel_icon_v1_add_buffer(xdg_icon, buffer.get(), scale);
  }
  xdg_toplevel_icon_manager_v1_set_icon(manager, xdg_toplevel_.get(), xdg_icon);
  xdg_toplevel_icon_v1_destroy(xdg_icon);
}

XDGToplevelWrapperImpl* XDGToplevelWrapperImpl::AsXDGToplevelWrapper() {
  return this;
}

}  // namespace ui
