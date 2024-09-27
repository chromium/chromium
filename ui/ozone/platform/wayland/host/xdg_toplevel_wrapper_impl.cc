// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_toplevel_wrapper_impl.h"

#include <aura-shell-client-protocol.h>
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
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
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
      NOTREACHED_IN_MIGRATION();
      return XDGToplevelWrapperImpl::DecorationMode::kClientSide;
  }
}

uint32_t ToInt32(XDGToplevelWrapperImpl::DecorationMode mode) {
  switch (mode) {
    case XDGToplevelWrapperImpl::DecorationMode::kClientSide:
      return ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    case XDGToplevelWrapperImpl::DecorationMode::kServerSide:
      return ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    default:
      NOTREACHED_IN_MIGRATION();
      return ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
  }
}

std::optional<wl::Serial> GetSerialForMoveResize(
    WaylandConnection* connection) {
  return connection->serial_tracker().GetSerial({wl::SerialType::kTouchPress,
                                                 wl::SerialType::kMousePress,
                                                 wl::SerialType::kKeyPress});
}

zaura_toplevel_z_order_level ToZauraToplevelZOrderLevel(
    ZOrderLevel z_order_level) {
  switch (z_order_level) {
    case ZOrderLevel::kNormal:
      return ZAURA_TOPLEVEL_Z_ORDER_LEVEL_NORMAL;
    case ZOrderLevel::kFloatingWindow:
      return ZAURA_TOPLEVEL_Z_ORDER_LEVEL_FLOATING_WINDOW;
    case ZOrderLevel::kFloatingUIElement:
      return ZAURA_TOPLEVEL_Z_ORDER_LEVEL_FLOATING_UI_ELEMENT;
    case ZOrderLevel::kSecuritySurface:
      return ZAURA_TOPLEVEL_Z_ORDER_LEVEL_SECURITY_SURFACE;
  }

  NOTREACHED_IN_MIGRATION();
  return ZAURA_TOPLEVEL_Z_ORDER_LEVEL_NORMAL;
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
    NOTREACHED_IN_MIGRATION() << "Wrong shell protocol";
    return false;
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

  if (connection_->zaura_shell()) {
    uint32_t version =
        zaura_shell_get_version(connection_->zaura_shell()->wl_object());
    if (version >=
        ZAURA_SHELL_GET_AURA_TOPLEVEL_FOR_XDG_TOPLEVEL_SINCE_VERSION) {
      aura_toplevel_.reset(zaura_shell_get_aura_toplevel_for_xdg_toplevel(
          connection_->zaura_shell()->wl_object(), xdg_toplevel_.get()));
      if (ui::IsWaylandSurfaceSubmissionInPixelCoordinatesEnabled() &&
          version >=
              ZAURA_TOPLEVEL_SURFACE_SUBMISSION_IN_PIXEL_COORDINATES_SINCE_VERSION) {
        zaura_toplevel_surface_submission_in_pixel_coordinates(
            aura_toplevel_.get());
      }
    }
  }

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

bool XDGToplevelWrapperImpl::IsSupportedOnAuraToplevel(uint32_t version) const {
  return aura_toplevel_ &&
         zaura_toplevel_get_version(aura_toplevel_.get()) >= version;
}

void XDGToplevelWrapperImpl::SetCanMaximize(bool can_maximize) {
  if (!aura_toplevel_ || zaura_toplevel_get_version(aura_toplevel_.get()) <
                             ZAURA_TOPLEVEL_SET_CAN_MAXIMIZE_SINCE_VERSION) {
    return;
  }

  if (can_maximize) {
    zaura_toplevel_set_can_maximize(aura_toplevel_.get());
  } else {
    zaura_toplevel_unset_can_maximize(aura_toplevel_.get());
  }
}

void XDGToplevelWrapperImpl::SetMaximized() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_maximized(xdg_toplevel_.get());
}

void XDGToplevelWrapperImpl::UnSetMaximized() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_unset_maximized(xdg_toplevel_.get());
}

void XDGToplevelWrapperImpl::SetCanFullscreen(bool can_fullscreen) {
  if (!aura_toplevel_ || zaura_toplevel_get_version(aura_toplevel_.get()) <
                             ZAURA_TOPLEVEL_SET_CAN_FULLSCREEN_SINCE_VERSION) {
    return;
  }

  if (can_fullscreen) {
    zaura_toplevel_set_can_fullscreen(aura_toplevel_.get());
  } else {
    zaura_toplevel_unset_can_fullscreen(aura_toplevel_.get());
  }
}

void XDGToplevelWrapperImpl::SetFullscreen(WaylandOutput* wayland_output) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_fullscreen(
      xdg_toplevel_.get(),
      wayland_output ? wayland_output->get_output() : nullptr);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void XDGToplevelWrapperImpl::SetUseImmersiveMode(bool immersive) {
  auto mode = immersive ? ZAURA_TOPLEVEL_FULLSCREEN_MODE_IMMERSIVE
                        : ZAURA_TOPLEVEL_FULLSCREEN_MODE_PLAIN;
  zaura_toplevel_set_fullscreen_mode(aura_toplevel_.get(), mode);
}

void XDGToplevelWrapperImpl::SetTopInset(int height) {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_TOP_INSET_SINCE_VERSION) {
    zaura_toplevel_set_top_inset(aura_toplevel_.get(), height);
  }
}

void XDGToplevelWrapperImpl::SetShadowCornersRadii(
    const gfx::RoundedCornersF& radii) {
  if (aura_toplevel_ &&
      zaura_toplevel_get_version(aura_toplevel_.get()) >=
          ZAURA_TOPLEVEL_SET_SHADOW_CORNER_RADII_SINCE_VERSION) {
    zaura_toplevel_set_shadow_corner_radii(
        aura_toplevel_.get(), radii.upper_left(), radii.upper_right(),
        radii.lower_right(), radii.lower_left());
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

#if BUILDFLAG(IS_LINUX)
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
#endif  // IS_LINUX || IS_CHROMEOS_LACROS

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

// static
void XDGToplevelWrapperImpl::OnAuraToplevelConfigure(
    void* data,
    zaura_toplevel* aura_toplevel,
    int32_t x,
    int32_t y,
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  window_states.is_immersive_fullscreen =
      CheckIfWlArrayHasValue(states, ZAURA_TOPLEVEL_STATE_IMMERSIVE);
  window_states.is_pinned_fullscreen =
      CheckIfWlArrayHasValue(states, ZAURA_TOPLEVEL_STATE_PINNED);
  window_states.is_trusted_pinned_fullscreen =
      CheckIfWlArrayHasValue(states, ZAURA_TOPLEVEL_STATE_TRUSTED_PINNED);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  window_states.is_activated =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_ACTIVATED);
  window_states.is_minimized =
      CheckIfWlArrayHasValue(states, ZAURA_TOPLEVEL_STATE_MINIMIZED);
  window_states.is_snapped_primary =
      CheckIfWlArrayHasValue(states, ZAURA_TOPLEVEL_STATE_SNAPPED_PRIMARY);
  window_states.is_snapped_secondary =
      CheckIfWlArrayHasValue(states, ZAURA_TOPLEVEL_STATE_SNAPPED_SECONDARY);
  window_states.is_floated =
      CheckIfWlArrayHasValue(states, ZAURA_TOPLEVEL_STATE_FLOATED);
  window_states.is_pip =
      CheckIfWlArrayHasValue(states, ZAURA_TOPLEVEL_STATE_PIP);

  self->wayland_window_->HandleToplevelConfigureWithOrigin(x, y, width, height,
                                                           window_states);
}

// static
void XDGToplevelWrapperImpl::OnOriginChange(void* data,
                                            zaura_toplevel* aura_toplevel,
                                            int32_t x,
                                            int32_t y) {
  auto* self = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(self);
  auto* toplevel_window =
      static_cast<WaylandToplevelWindow*>(self->wayland_window_);
  toplevel_window->SetOrigin(gfx::Point(x, y));
}

// static
void XDGToplevelWrapperImpl::OnConfigureRasterScale(
    void* data,
    zaura_toplevel* aura_toplevel,
    uint32_t scale_as_uint) {
  auto* self = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(self);
  auto* wayland_window = static_cast<WaylandWindow*>(self->wayland_window_);
  float scale = base::bit_cast<float>(scale_as_uint);
  wayland_window->SetPendingRasterScale(scale);
}

// static
void XDGToplevelWrapperImpl::OnRotateFocus(void* data,
                                           zaura_toplevel* aura_toplevel,
                                           uint32_t serial,
                                           uint32_t direction,
                                           uint32_t restart) {
  auto* self = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(self);
  auto* toplevel_window =
      static_cast<WaylandToplevelWindow*>(self->wayland_window_);
  toplevel_window->OnRotateFocus(serial, direction, restart);
}

// static
void XDGToplevelWrapperImpl::OnOverviewChange(void* data,
                                              zaura_toplevel* aura_toplevel,
                                              uint32_t in_overview_as_uint) {
  auto* self = static_cast<XDGToplevelWrapperImpl*>(data);
  CHECK(self);
  self->wayland_window_->AsWaylandToplevelWindow()->OnOverviewChange(
      in_overview_as_uint);
}

// static
void XDGToplevelWrapperImpl::OnConfigureOcclusionState(
    void* data,
    struct zaura_toplevel* zaura_toplevel,
    uint32_t mode) {
  auto* surface = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(surface);
  auto* wayland_window = static_cast<WaylandWindow*>(surface->wayland_window_);
  auto occlusion_state =
      WaylandOcclusionStateToPlatformWindowOcclusionState(mode);
  wayland_window->SetPendingOcclusionState(occlusion_state);
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

zaura_toplevel_orientation_lock ToZauraSurfaceOrientationLock(
    WaylandOrientationLockType lock_type) {
  switch (lock_type) {
    case WaylandOrientationLockType::kLandscape:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_LANDSCAPE;
    case WaylandOrientationLockType::kLandscapePrimary:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_LANDSCAPE_PRIMARY;
    case WaylandOrientationLockType::kLandscapeSecondary:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_LANDSCAPE_SECONDARY;
    case WaylandOrientationLockType::kPortrait:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_PORTRAIT;
    case WaylandOrientationLockType::kPortraitPrimary:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_PORTRAIT_PRIMARY;
    case WaylandOrientationLockType::kPortraitSecondary:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_PORTRAIT_SECONDARY;
    case WaylandOrientationLockType::kAny:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_NONE;
    case WaylandOrientationLockType::kNatural:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_CURRENT;
  }
  return ZAURA_TOPLEVEL_ORIENTATION_LOCK_NONE;
}

void XDGToplevelWrapperImpl::Lock(WaylandOrientationLockType lock_type) {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_ORIENTATION_LOCK_SINCE_VERSION) {
    zaura_toplevel_set_orientation_lock(
        aura_toplevel_.get(), ToZauraSurfaceOrientationLock(lock_type));
  }
}

void XDGToplevelWrapperImpl::Unlock() {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_ORIENTATION_LOCK_SINCE_VERSION) {
    zaura_toplevel_set_orientation_lock(aura_toplevel_.get(),
                                        ZAURA_TOPLEVEL_ORIENTATION_LOCK_NONE);
  }
}

void XDGToplevelWrapperImpl::RequestWindowBounds(const gfx::Rect& bounds,
                                                 int64_t display_id) {
  const WaylandOutputManager* manager = connection_->wayland_output_manager();

  WaylandOutput* target_output = nullptr;
  if (display_id != display::kInvalidDisplayId) {
    auto output_id_for_display_id =
        manager->wayland_screen()->GetOutputIdForDisplayId(display_id);
    // the output for the valid display_id should exist.
    LOG_IF(WARNING, !output_id_for_display_id)
        << "No output found for display id:" << display_id;

    target_output = manager->GetOutput(output_id_for_display_id);
  }

  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_WINDOW_BOUNDS_SINCE_VERSION) {
    zaura_toplevel_set_window_bounds(
        aura_toplevel_.get(), bounds.x(), bounds.y(), bounds.width(),
        bounds.height(), target_output ? target_output->get_output() : nullptr);
  }
}

void XDGToplevelWrapperImpl::SetSystemModal(bool modal) {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_SYSTEM_MODAL_SINCE_VERSION) {
    if (modal) {
      zaura_toplevel_set_system_modal(aura_toplevel_.get());
    } else {
      zaura_toplevel_unset_system_modal(aura_toplevel_.get());
    }
  }
}

bool XDGToplevelWrapperImpl::SupportsScreenCoordinates() const {
  return aura_toplevel_ &&
         zaura_toplevel_get_version(aura_toplevel_.get()) >=
             ZAURA_TOPLEVEL_SET_SUPPORTS_SCREEN_COORDINATES_SINCE_VERSION;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void XDGToplevelWrapperImpl::EnableScreenCoordinates() {
  if (!SupportsScreenCoordinates()) {
    LOG(WARNING) << "Server implementation of wayland is incompatible, "
                    "WaylandScreenCoordinatesEnabled has no effect.";
    return;
  }
  zaura_toplevel_set_supports_screen_coordinates(aura_toplevel_.get());

  static constexpr zaura_toplevel_listener kAuraToplevelListener = {
      .configure = &OnAuraToplevelConfigure,
      .origin_change = &OnOriginChange,
      .configure_raster_scale = &OnConfigureRasterScale,
      .rotate_focus = &OnRotateFocus,
      .overview_change = &OnOverviewChange,
      .configure_occlusion_state = &OnConfigureOcclusionState};
  zaura_toplevel_add_listener(aura_toplevel_.get(), &kAuraToplevelListener,
                              this);
}
#endif

void XDGToplevelWrapperImpl::SetZOrder(ZOrderLevel z_order) {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_Z_ORDER_SINCE_VERSION) {
    zaura_toplevel_set_z_order(aura_toplevel_.get(),
                               ToZauraToplevelZOrderLevel(z_order));
  }
}

bool XDGToplevelWrapperImpl::SupportsActivation() {
  static_assert(
      ZAURA_TOPLEVEL_ACTIVATE_SINCE_VERSION ==
          ZAURA_TOPLEVEL_DEACTIVATE_SINCE_VERSION,
      "Support for activation and deactivation was added in the same version.");
  return aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                               ZAURA_TOPLEVEL_ACTIVATE_SINCE_VERSION;
}

void XDGToplevelWrapperImpl::Activate() {
  if (aura_toplevel_ && SupportsActivation()) {
    zaura_toplevel_activate(aura_toplevel_.get());
  }
}

void XDGToplevelWrapperImpl::Deactivate() {
  if (aura_toplevel_ && SupportsActivation()) {
    zaura_toplevel_deactivate(aura_toplevel_.get());
  }
}

void XDGToplevelWrapperImpl::SetScaleFactor(float scale_factor) {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_SCALE_FACTOR_SINCE_VERSION) {
    uint32_t value = base::bit_cast<uint32_t>(scale_factor);
    zaura_toplevel_set_scale_factor(aura_toplevel_.get(), value);
  }
}

void XDGToplevelWrapperImpl::SetRestoreInfo(int32_t restore_session_id,
                                            int32_t restore_window_id) {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_RESTORE_INFO_SINCE_VERSION) {
    zaura_toplevel_set_restore_info(aura_toplevel_.get(), restore_session_id,
                                    restore_window_id);
  }
}

void XDGToplevelWrapperImpl::SetRestoreInfoWithWindowIdSource(
    int32_t restore_session_id,
    const std::string& restore_window_id_source) {
  if (aura_toplevel_ &&
      zaura_toplevel_get_version(aura_toplevel_.get()) >=
          ZAURA_TOPLEVEL_SET_RESTORE_INFO_WITH_WINDOW_ID_SOURCE_SINCE_VERSION) {
    zaura_toplevel_set_restore_info_with_window_id_source(
        aura_toplevel_.get(), restore_session_id,
        restore_window_id_source.c_str());
  }
}

void XDGToplevelWrapperImpl::SetFloatToLocation(
    WaylandFloatStartLocation float_start_location) {
  if (!aura_toplevel_) {
    return;
  }

  uint32_t version = zaura_toplevel_get_version(aura_toplevel_.get());
  if (version >= ZAURA_TOPLEVEL_SET_FLOAT_TO_LOCATION_SINCE_VERSION) {
    uint32_t value = base::bit_cast<uint32_t>(float_start_location);
    zaura_toplevel_set_float_to_location(aura_toplevel_.get(), value);
  } else if (version >= ZAURA_TOPLEVEL_SET_FLOAT_SINCE_VERSION) {
    zaura_toplevel_set_float(aura_toplevel_.get());
  }
}

void XDGToplevelWrapperImpl::UnSetFloat() {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_UNSET_FLOAT_SINCE_VERSION) {
    zaura_toplevel_unset_float(aura_toplevel_.get());
  }
}

void XDGToplevelWrapperImpl::CommitSnap(
    WaylandWindowSnapDirection snap_direction,
    float snap_ratio) {
  if (!aura_toplevel_) {
    return;
  }

  if (zaura_toplevel_get_version(aura_toplevel_.get()) >=
          ZAURA_TOPLEVEL_UNSET_SNAP_SINCE_VERSION &&
      snap_direction == WaylandWindowSnapDirection::kNone) {
    zaura_toplevel_unset_snap(aura_toplevel_.get());
    return;
  }

  if (zaura_toplevel_get_version(aura_toplevel_.get()) >=
      ZAURA_TOPLEVEL_SET_SNAP_PRIMARY_SINCE_VERSION) {
    uint32_t value = base::bit_cast<uint32_t>(snap_ratio);
    switch (snap_direction) {
      case WaylandWindowSnapDirection::kPrimary:
        zaura_toplevel_set_snap_primary(aura_toplevel_.get(), value);
        return;
      case WaylandWindowSnapDirection::kSecondary:
        zaura_toplevel_set_snap_secondary(aura_toplevel_.get(), value);
        return;
      case WaylandWindowSnapDirection::kNone:
        NOTREACHED_IN_MIGRATION() << "Toplevel does not support UnsetSnap yet";
        return;
    }
  }
}

void XDGToplevelWrapperImpl::SetPersistable(bool persistable) const {
  auto persistable_enum = persistable
                              ? ZAURA_TOPLEVEL_PERSISTABLE_PERSISTABLE
                              : ZAURA_TOPLEVEL_PERSISTABLE_NOT_PERSISTABLE;

  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_PERSISTABLE_SINCE_VERSION) {
    zaura_toplevel_set_persistable(aura_toplevel_.get(), persistable_enum);
  }
}

void XDGToplevelWrapperImpl::SetShape(std::unique_ptr<ShapeRects> shape_rects) {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_SHAPE_SINCE_VERSION) {
    zaura_toplevel_set_shape(
        aura_toplevel_.get(),
        shape_rects ? CreateAndAddRegion(*shape_rects).get() : nullptr);
  }
}

void XDGToplevelWrapperImpl::ShowSnapPreview(
    WaylandWindowSnapDirection snap_direction,
    bool allow_haptic_feedback) {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_INTENT_TO_SNAP_SINCE_VERSION) {
    uint32_t zaura_shell_snap_direction = ZAURA_TOPLEVEL_SNAP_DIRECTION_NONE;
    switch (snap_direction) {
      case WaylandWindowSnapDirection::kPrimary:
        zaura_shell_snap_direction = ZAURA_TOPLEVEL_SNAP_DIRECTION_PRIMARY;
        break;
      case WaylandWindowSnapDirection::kSecondary:
        zaura_shell_snap_direction = ZAURA_TOPLEVEL_SNAP_DIRECTION_SECONDARY;
        break;
      case WaylandWindowSnapDirection::kNone:
        break;
    }
    zaura_toplevel_intent_to_snap(aura_toplevel_.get(),
                                  zaura_shell_snap_direction);
    return;
  }
}

void XDGToplevelWrapperImpl::AckRotateFocus(uint32_t serial, uint32_t handled) {
  zaura_toplevel_ack_rotate_focus(aura_toplevel_.get(), serial, handled);
  connection_->Flush();
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
