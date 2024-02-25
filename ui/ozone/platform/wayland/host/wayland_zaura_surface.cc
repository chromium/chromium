// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_surface.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

PlatformWindowOcclusionState
WaylandOcclusionStateToPlatformWindowOcclusionState(uint32_t mode) {
  switch (mode) {
    case ZAURA_SURFACE_OCCLUSION_STATE_UNKNOWN:
      return PlatformWindowOcclusionState::kUnknown;
    case ZAURA_SURFACE_OCCLUSION_STATE_VISIBLE:
      return PlatformWindowOcclusionState::kVisible;
    case ZAURA_SURFACE_OCCLUSION_STATE_OCCLUDED:
      return PlatformWindowOcclusionState::kOccluded;
    case ZAURA_SURFACE_OCCLUSION_STATE_HIDDEN:
      return PlatformWindowOcclusionState::kHidden;
  }
  return PlatformWindowOcclusionState::kUnknown;
}

WaylandZAuraSurface::WaylandZAuraSurface(zaura_shell* zaura_shell,
                                         wl_surface* wl_surface,
                                         WaylandConnection* connection)
    : zaura_surface_(zaura_shell_get_aura_surface(zaura_shell, wl_surface)),
      connection_(connection) {
  CHECK(zaura_surface_);
  static constexpr zaura_surface_listener kAuraSurfaceListener = {
      .occlusion_changed = &OnOcclusionChanged,
      .lock_frame_normal = &OnLockFrameNormal,
      .unlock_frame_normal = &OnUnlockFrameNormal,
      .occlusion_state_changed = &OnOcclusionStateChanged,
      .desk_changed = &OnDeskChanged,
      .start_throttle = &OnStartThrottle,
      .end_throttle = &OnEndThrottle,
      .tooltip_shown = &OnTooltipShown,
      .tooltip_hidden = &OnTooltipHidden,
  };
  zaura_surface_add_listener(zaura_surface_.get(), &kAuraSurfaceListener, this);
}

WaylandZAuraSurface::~WaylandZAuraSurface() = default;

bool WaylandZAuraSurface::SetFrame(zaura_surface_frame_type type) {
  if (IsSupported(ZAURA_SURFACE_SET_FRAME_SINCE_VERSION)) {
    zaura_surface_set_frame(zaura_surface_.get(), type);
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetOcclusionTracking() {
  if (IsSupported(ZAURA_SURFACE_SET_OCCLUSION_TRACKING_SINCE_VERSION)) {
    zaura_surface_set_occlusion_tracking(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::Activate() {
  if (IsSupported(ZAURA_SURFACE_ACTIVATE_SINCE_VERSION)) {
    zaura_surface_activate(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetFullscreenMode(
    zaura_surface_fullscreen_mode mode) {
  if (IsSupported(ZAURA_SURFACE_SET_FULLSCREEN_MODE_SINCE_VERSION)) {
    zaura_surface_set_fullscreen_mode(zaura_surface_.get(), mode);
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetServerStartResize() {
  if (IsSupported(ZAURA_SURFACE_SET_SERVER_START_RESIZE_SINCE_VERSION)) {
    zaura_surface_set_server_start_resize(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::IntentToSnap(
    zaura_surface_snap_direction snap_direction) {
  if (IsSupported(ZAURA_SURFACE_INTENT_TO_SNAP_SINCE_VERSION)) {
    zaura_surface_intent_to_snap(zaura_surface_.get(), snap_direction);
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetSnapLeft() {
  if (IsSupported(ZAURA_SURFACE_SET_SNAP_LEFT_SINCE_VERSION)) {
    zaura_surface_set_snap_left(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetSnapRight() {
  if (IsSupported(ZAURA_SURFACE_SET_SNAP_RIGHT_SINCE_VERSION)) {
    zaura_surface_set_snap_right(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::UnsetSnap() {
  if (IsSupported(ZAURA_SURFACE_UNSET_SNAP_SINCE_VERSION)) {
    zaura_surface_unset_snap(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetCanGoBack() {
  if (IsSupported(ZAURA_SURFACE_SET_CAN_GO_BACK_SINCE_VERSION)) {
    zaura_surface_set_can_go_back(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::UnsetCanGoBack() {
  if (IsSupported(ZAURA_SURFACE_UNSET_CAN_GO_BACK_SINCE_VERSION)) {
    zaura_surface_unset_can_go_back(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetPip() {
  if (IsSupported(ZAURA_SURFACE_SET_PIP_SINCE_VERSION)) {
    zaura_surface_set_pip(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetAspectRatio(float width, float height) {
  if (IsSupported(ZAURA_SURFACE_SET_ASPECT_RATIO_SINCE_VERSION)) {
    zaura_surface_set_aspect_ratio(zaura_surface_.get(), width, height);
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::MoveToDesk(int index) {
  if (IsSupported(ZAURA_SURFACE_MOVE_TO_DESK_SINCE_VERSION)) {
    zaura_surface_move_to_desk(zaura_surface_.get(), index);
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetInitialWorkspace(int workspace) {
  if (IsSupported(ZAURA_SURFACE_SET_INITIAL_WORKSPACE_SINCE_VERSION)) {
    zaura_surface_set_initial_workspace(
        zaura_surface_.get(), base::NumberToString(workspace).c_str());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SetPin(bool trusted) {
  if (IsSupported(ZAURA_SURFACE_SET_PIN_SINCE_VERSION)) {
    zaura_surface_set_pin(zaura_surface_.get(), trusted);
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::UnsetPin() {
  if (IsSupported(ZAURA_SURFACE_UNSET_PIN_SINCE_VERSION)) {
    zaura_surface_unset_pin(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::ShowTooltip(const std::u16string& text,
                                      const gfx::Point& position,
                                      zaura_surface_tooltip_trigger trigger,
                                      const base::TimeDelta& show_delay,
                                      const base::TimeDelta& hide_delay) {
  if (IsSupported(ZAURA_SURFACE_SHOW_TOOLTIP_SINCE_VERSION)) {
    zaura_surface_show_tooltip(
        zaura_surface_.get(), base::UTF16ToUTF8(text).c_str(), position.x(),
        position.y(), trigger,
        // Cast `show_delay` and `hide_delay` into int32_t as TimeDelta should
        // not be larger than what can be handled in int32_t
        base::saturated_cast<uint32_t>(show_delay.InMilliseconds()),
        base::saturated_cast<uint32_t>(hide_delay.InMilliseconds()));
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::HideTooltip() {
  if (IsSupported(ZAURA_SURFACE_HIDE_TOOLTIP_SINCE_VERSION)) {
    zaura_surface_hide_tooltip(zaura_surface_.get());
    return true;
  }
  return false;
}

bool WaylandZAuraSurface::SupportsActivate() const {
  return IsSupported(ZAURA_SURFACE_ACTIVATE_SINCE_VERSION);
}

bool WaylandZAuraSurface::SupportsSetServerStartResize() const {
  return IsSupported(ZAURA_SURFACE_SET_SERVER_START_RESIZE_SINCE_VERSION);
}

bool WaylandZAuraSurface::SupportsUnsetSnap() const {
  return IsSupported(ZAURA_SURFACE_UNSET_SNAP_SINCE_VERSION);
}

bool WaylandZAuraSurface::IsSupported(uint32_t version) const {
  CHECK(zaura_surface_);
  return zaura_surface_get_version(zaura_surface_.get()) >= version;
}

// static.
void WaylandZAuraSurface::OnOcclusionChanged(void* data,
                                             zaura_surface* surface,
                                             wl_fixed_t occlusion_fraction,
                                             uint32_t occlusion_reason) {
  auto* self = static_cast<WaylandZAuraSurface*>(data);
  if (self && self->delegate_) {
    self->delegate_->OcclusionChanged(occlusion_fraction, occlusion_reason);
  }
}

// static.
void WaylandZAuraSurface::OnLockFrameNormal(void* data,
                                            zaura_surface* surface) {
  auto* self = static_cast<WaylandZAuraSurface*>(data);
  if (self && self->delegate_) {
    self->delegate_->LockFrame();
  }
}

// static.
void WaylandZAuraSurface::OnUnlockFrameNormal(void* data,
                                              zaura_surface* surface) {
  auto* self = static_cast<WaylandZAuraSurface*>(data);
  if (self && self->delegate_) {
    self->delegate_->UnlockFrame();
  }
}

// static.
void WaylandZAuraSurface::OnOcclusionStateChanged(void* data,
                                                  zaura_surface* surface,
                                                  uint32_t mode) {
  auto* self = static_cast<WaylandZAuraSurface*>(data);
  if (self && self->delegate_) {
    self->delegate_->OcclusionStateChanged(
        WaylandOcclusionStateToPlatformWindowOcclusionState(mode));
  }
}

// static.
void WaylandZAuraSurface::OnDeskChanged(void* data,
                                        zaura_surface* surface,
                                        int state) {
  auto* self = static_cast<WaylandZAuraSurface*>(data);
  if (self && self->delegate_) {
    self->delegate_->DeskChanged(state);
  }
}

// static.
void WaylandZAuraSurface::OnStartThrottle(void* data, zaura_surface* surface) {
  auto* self = static_cast<WaylandZAuraSurface*>(data);
  if (self && self->delegate_) {
    self->delegate_->StartThrottle();
  }
}

// static.
void WaylandZAuraSurface::OnEndThrottle(void* data, zaura_surface* surface) {
  auto* self = static_cast<WaylandZAuraSurface*>(data);
  if (self && self->delegate_) {
    self->delegate_->EndThrottle();
  }
}

// static.
void WaylandZAuraSurface::OnTooltipShown(void* data,
                                         zaura_surface* surface,
                                         const char* text,
                                         int32_t x,
                                         int32_t y,
                                         int32_t width,
                                         int32_t height) {
  auto* self = static_cast<WaylandZAuraSurface*>(data);
  if (self && self->delegate_) {
    self->delegate_->TooltipShown(text, x, y, width, height);
  }
}

// static.
void WaylandZAuraSurface::OnTooltipHidden(void* data, zaura_surface* surface) {
  auto* self = static_cast<WaylandZAuraSurface*>(data);
  if (self && self->delegate_) {
    self->delegate_->TooltipHidden();
  }
}

}  // namespace ui
