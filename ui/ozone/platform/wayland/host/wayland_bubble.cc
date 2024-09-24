// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_bubble.h"

#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

WaylandBubble::WaylandBubble(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection,
                             WaylandWindow* parent)
    : WaylandWindow(delegate, connection) {
  set_parent_window(parent);
  parent->AddBubble(this);
}

WaylandBubble::~WaylandBubble() {
  if (parent_window()) {
    parent_window()->RemoveBubble(this);
  }
}

void WaylandBubble::Show(bool inactive) {
  if (subsurface_) {
    return;
  }

  UpdateWindowScale(false);
  AddToParentAsSubsurface();
  WaylandWindow::Show(inactive);
}

void WaylandBubble::Hide() {
  if (!subsurface_) {
    return;
  }

  WaylandWindow::Hide();
  if (root_surface()) {
    root_surface()->ResetZAuraSurface();
  }

  Deactivate();

  subsurface_.reset();
  connection()->Flush();
}

bool WaylandBubble::IsVisible() const {
  return !!subsurface_;
}

void WaylandBubble::SetBoundsInDIP(const gfx::Rect& bounds_dip) {
  // TODO(crbug.com/329145822): There will be occasional visual inconsistencies
  // when scale factor changes due to async, either the size, or the offset.
  // The toplevel parent and bubble are submitted in separate compositor frames.
  // There is currently no guarantee that the 2 compositor frames arrive
  // together atomically.
  auto old_bounds_dip = GetBoundsInDIP();
  WaylandWindow::SetBoundsInDIP(bounds_dip);

  // TODO(crbug.com/329145822): Don't apply position immediately here, wait for
  // ackconfigure, otherwise it might jitter if offset changes.
  if (subsurface_ && old_bounds_dip != bounds_dip) {
    SetSubsurfacePosition();
  }
}

void WaylandBubble::SetInputRegion(
    std::optional<std::vector<gfx::Rect>> region_px) {
  if (accept_events_) {
    root_surface()->set_input_region(region_px);
  }
}

void WaylandBubble::Activate() {
  if (subsurface_ && activatable_ && parent_window()) {
    parent_window()->ActivateBubble(this);
  }
  WaylandWindow::Activate();
}

void WaylandBubble::Deactivate() {
  if (IsActive()) {
    parent_window()->ActivateBubble(nullptr);
  }
  WaylandWindow::Deactivate();
}

void WaylandBubble::ShowTooltip(const std::u16string& text,
                                const gfx::Point& position,
                                const PlatformWindowTooltipTrigger trigger,
                                const base::TimeDelta show_delay,
                                const base::TimeDelta hide_delay) {
  auto* zaura_surface = GetZAuraSurface();
  const auto zaura_shell_trigger =
      trigger == PlatformWindowTooltipTrigger::kCursor
          ? ZAURA_SURFACE_TOOLTIP_TRIGGER_CURSOR
          : ZAURA_SURFACE_TOOLTIP_TRIGGER_KEYBOARD;
  if (zaura_surface &&
      zaura_surface->ShowTooltip(text, position, zaura_shell_trigger,
                                 show_delay, hide_delay)) {
    connection()->Flush();
  }
}

void WaylandBubble::HideTooltip() {
  auto* zaura_surface = GetZAuraSurface();
  if (zaura_surface && zaura_surface->HideTooltip()) {
    connection()->Flush();
  }
}

void WaylandBubble::UpdateWindowScale(bool update_bounds) {
  WaylandWindow::UpdateWindowScale(update_bounds);
  if (subsurface_) {
    SetSubsurfacePosition();
  }
}

void WaylandBubble::OnSequencePoint(int64_t seq) {
  if (!subsurface_) {
    return;
  }

  ProcessSequencePoint(seq);
  MaybeApplyLatestStateRequest(/*force=*/false);
}

base::WeakPtr<WaylandWindow> WaylandBubble::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool WaylandBubble::IsScreenCoordinatesEnabled() const {
  return parent_window()->IsScreenCoordinatesEnabled();
}

bool WaylandBubble::IsActive() const {
  bool is_active = activatable_ && parent_window() &&
                   parent_window()->IsActive() &&
                   parent_window()->active_bubble() == this;
  CHECK(activatable_ || !is_active);
  return is_active;
}

WaylandBubble* WaylandBubble::AsWaylandBubble() {
  return this;
}

void WaylandBubble::AddToParentAsSubsurface() {
  CHECK(parent_window());

  // We need to make sure that window scale matches the parent window.
  UpdateWindowScale(true);

  subsurface_ =
      root_surface()->CreateSubsurface(parent_window()->root_surface());
  DCHECK(subsurface_);

  if (auto* zaura_surface = root_surface()->CreateZAuraSurface()) {
    zaura_surface->set_delegate(AsWeakPtr());
  }

  SetSubsurfacePosition();

  // The associaded widget of this platform_window has a ui::Compositor and
  // receives BeginFrames separately from its parent widget. When this window
  // commits a frame, it should not require a parent wl_surface commit for the
  // frame to be visible.
  wl_subsurface_set_desync(subsurface_.get());

  // wl_subsurface will not receive configure events from server. As soon as the
  // wl_subusrface handle is created, it is considered as configured. Need to
  // manually set `received_configure_event_` so `frame_manager_` can start
  // processing frames.
  OnSurfaceConfigureEvent();

  // Notify the observers the window has been configured. Please note that
  // subsurface doesn't send ack configure events. Thus, notify the observers as
  // soon as the subsurface is created.
  connection()->window_manager()->NotifyWindowConfigured(this);
}

void WaylandBubble::SetSubsurfacePosition() {
  if (connection()->surface_submission_in_pixel_coordinates()) {
    const auto bounds_px_in_parent = wl::TranslateBoundsToParentCoordinates(
        GetBoundsInPixels(), parent_window()->GetBoundsInPixels());
    wl_subsurface_set_position(subsurface_.get(), bounds_px_in_parent.x(),
                               bounds_px_in_parent.y());
  } else {
    // TODO(crbug.com/369213517): Handle ui scale before enabling this on Linux.
    const auto bounds_dip_in_parent =
        wl::TranslateWindowBoundsToParentDIP(this, parent_window());
    wl_subsurface_set_position(subsurface_.get(), bounds_dip_in_parent.x(),
                               bounds_dip_in_parent.y());
  }

  parent_window()->root_surface()->Commit();
}

bool WaylandBubble::OnInitialize(PlatformWindowInitProperties properties,
                                 PlatformWindowDelegate::State* state) {
  DCHECK(parent_window());

  // `window_state` is always `kNormal` on WaylandBubble.
  state->window_state = PlatformWindowState::kNormal;

  state->window_scale = parent_window()->applied_state().window_scale;
  activatable_ = properties.activatable;
  accept_events_ = properties.accept_events;

  // For now kBubbles draw their own shadow, not setting input_region means
  // shadow trap input as well.
  if (!accept_events_) {
    root_surface()->set_input_region(
        std::optional<std::vector<gfx::Rect>>({gfx::Rect()}));
  }

  return true;
}

bool WaylandBubble::IsSurfaceConfigured() {
  // Server will generate unconfigured_buffer error if we attach a buffer to an
  // unconfigured surface for toplevel/popup window. WaylandBubble uses
  // wl_subsurface role, so it is treated as configured as long as it has a
  // wl_subsurface handle.
  return parent_window() && parent_window()->IsSurfaceConfigured() &&
         subsurface_;
}

void WaylandBubble::UpdateWindowMask() {
  // Bubble window doesn't have a shape. Update the opaqueness.
  auto region = IsOpaqueWindow() ? std::optional<std::vector<gfx::Rect>>(
                                       {gfx::Rect(latched_state().size_px)})
                                 : std::nullopt;
  root_surface()->set_opaque_region(region);
}

}  // namespace ui
