// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_popup.h"

#include <aura-shell-client-protocol.h>

#include <optional>

#include "base/auto_reset.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/shell_object_factory.h"
#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

namespace ui {

WaylandPopup::WaylandPopup(PlatformWindowDelegate* delegate,
                           WaylandConnection* connection,
                           WaylandWindow* parent)
    : WaylandWindow(delegate, connection) {
  set_parent_window(parent);
#if BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/330384470): Whether the popup appear depends on whether
  // anchor point is outside of the parent xdg_surface. On Mutter the popup will
  // not show when outside.
  LOG_IF(WARNING,
         !parent->AsWaylandToplevelWindow() && !parent->AsWaylandPopup())
      << "Popup's parent is a bubble. Wayland shell popup is not guaranteed to "
         "show up.";
#endif
}

WaylandPopup::~WaylandPopup() = default;

void WaylandPopup::TooltipShown(const char* text,
                                int32_t x,
                                int32_t y,
                                int32_t width,
                                int32_t height) {
  delegate()->OnTooltipShownOnServer(base::UTF8ToUTF16(text),
                                     gfx::Rect(x, y, width, height));
}

void WaylandPopup::TooltipHidden() {
  delegate()->OnTooltipHiddenOnServer();
}

bool WaylandPopup::CreateShellPopup() {
  DCHECK(parent_window() && !shell_popup_);

  // Use `xdg_parent_window` and do appropriate origin transformations when
  // sending requests through Wayland.
  // Use `parent_window()` in all other cases.
  auto* xdg_parent_window = GetXdgParentWindow();
  if (!xdg_parent_window) {
    return false;
  }

  if (applied_state().window_scale !=
      parent_window()->applied_state().window_scale) {
    // If scale changed while this was hidden (when WaylandPopup hides, parent
    // window's child is reset), update buffer scale accordingly.
    UpdateWindowScale(true);
  }

  auto bounds_dip =
      wl::TranslateWindowBoundsToParentDIP(this, xdg_parent_window);
  bounds_dip.Inset(delegate()->CalculateInsetsInDIP(GetPlatformWindowState()));

  // At this point, both `bounds` and `anchor_rect` parameters here are in
  // UI coordinates space (i.e ui_scale'd), as they have just been provided by
  // upper UI layers. As they are going to be used to issue Wayland requests,
  // eg: xdg_positioner, they must be reverse-transformed to Wayland DIP
  // coordinates space.
  const float ui_scale = applied_state().ui_scale;

  ShellPopupParams params;
  params.bounds = gfx::ScaleToEnclosingRectIgnoringError(bounds_dip, ui_scale);
  params.anchor = delegate()->GetOwnedWindowAnchorAndRectInDIP();
  if (params.anchor.has_value()) {
    // The anchor rectangle must be relative to the window geometry, rather
    // than the root surface origin. See https://crbug.com/1292486.
    gfx::Rect anchor_rect(
        wl::TranslateBoundsToParentCoordinates(
            params.anchor->anchor_rect, xdg_parent_window->GetBoundsInDIP()) -
        xdg_parent_window->GetWindowGeometryOffsetInDIP());

    // Convert `anchor_rect` to Wayland coordinates space.
    anchor_rect = gfx::ScaleToEnclosingRectIgnoringError(anchor_rect, ui_scale);

    // If size is empty, set 1x1.
    if (anchor_rect.size().IsEmpty()) {
      anchor_rect.set_size({1, 1});
    }

    params.anchor->anchor_rect = anchor_rect;
  }

  // Certain Wayland compositors (E.g. Mutter) expects wl_surface to have no
  // buffer attached when xdg-surface role is created.
  wl_surface_attach(root_surface()->surface(), nullptr, 0, 0);
  root_surface()->Commit(false);

  ShellObjectFactory factory;
  shell_popup_ = factory.CreateShellPopupWrapper(connection(), this, params);
  if (!shell_popup_) {
    LOG(ERROR) << "Failed to create Wayland shell popup";
    return false;
  }

  if (auto* zaura_surface = root_surface()->CreateZAuraSurface()) {
    zaura_surface->set_delegate(AsWeakPtr());
  }

  parent_window()->set_child_popup(this);
  UpdateDecoration();
  return true;
}

void WaylandPopup::UpdateDecoration() {
  DCHECK(shell_popup_);

  // If the surface is already decorated early return.
  if (!connection()->zaura_shell() || decorated_via_aura_popup_)
    return;

  // Decorate the surface using the newer protocol. Relies on Ash >= M105.
  if (shell_popup_->SupportsDecoration()) {
    decorated_via_aura_popup_ = true;
    shell_popup_->Decorate(shadow_type_);
    return;
  }
}

void WaylandPopup::Show(bool inactive) {
  if (shell_popup_)
    return;

  // Map parent window as WaylandPopup cannot become a visible child of a
  // window that is not mapped.
  DCHECK(parent_window());
  if (!parent_window()->IsVisible())
    parent_window()->Show(false);

  if (!CreateShellPopup()) {
    Close();
    return;
  }

  connection()->Flush();
  WaylandWindow::Show(inactive);
}

void WaylandPopup::Hide() {
  if (!shell_popup_)
    return;

  if (child_popup()) {
    child_popup()->Hide();
  }
  WaylandWindow::Hide();
  // Mutter compositor crashes if we don't reset subsurfaces when hiding.
  if (WaylandWindow::primary_subsurface()) {
    WaylandWindow::primary_subsurface()->ResetSubsurface();
  }

  if (root_surface()) {
    root_surface()->ResetZAuraSurface();
  }

  if (shell_popup_) {
    parent_window()->set_child_popup(nullptr);
    shell_popup_.reset();
    decorated_via_aura_popup_ = false;
  }

  connection()->Flush();
}

bool WaylandPopup::IsVisible() const {
  return !!shell_popup_;
}

void WaylandPopup::SetBoundsInDIP(const gfx::Rect& bounds_dip) {
  auto* xdg_parent_window = GetXdgParentWindow();
  if (!xdg_parent_window) {
    return;
  }

  auto old_bounds_dip = GetBoundsInDIP();
  WaylandWindow::SetBoundsInDIP(bounds_dip);

  // The shell popup can be null if bounds are being fixed during
  // the initialization. See WaylandPopup::CreateShellPopup.
  if (shell_popup_ && old_bounds_dip != bounds_dip) {
    auto bounds_dip_in_parent =
        wl::TranslateWindowBoundsToParentDIP(this, xdg_parent_window);
    bounds_dip_in_parent.Inset(
        delegate()->CalculateInsetsInDIP(GetPlatformWindowState()));

    // If Wayland moved the popup (for example, a dnd arrow icon), schedule
    // redraw as Aura doesn't do that for moved surfaces. If redraw has not been
    // scheduled and a new buffer is not attached, some compositors may not
    // apply a new state. And committing the surface without attaching a buffer
    // won't make Wayland compositor apply these new bounds.
    schedule_redraw_ = old_bounds_dip.origin() != GetBoundsInDIP().origin();

    // If ShellPopup doesn't support repositioning, the popup will be recreated
    // with new geometry applied. Availability of methods to move/resize popup
    // surfaces purely depends on a protocol. See implementations of ShellPopup
    // for more details.
    if (!shell_popup_->SetBounds(bounds_dip_in_parent)) {
      // Always force redraw for recreated objects.
      schedule_redraw_ = true;
      // This will also close all the children windows...
      Hide();
      // ... and will result in showing them again starting with their parents.
      GetTopMostChildWindow()->Show(false);
    }
  }
}

void WaylandPopup::HandlePopupConfigure(const gfx::Rect& bounds_dip) {
  auto* xdg_parent_window = GetXdgParentWindow();
  if (!xdg_parent_window) {
    return;
  }

  // Use UI scale to scale the bounds received from the Wayland compositor (ie:
  // non-empty `bounds_dip`) as it is an internal scaling factor, which the
  // compositor is not aware of.
  gfx::Rect pending_bounds_dip(
      bounds_dip.IsEmpty() ? GetBoundsInDIP()
                           : gfx::ScaleToEnclosingRectIgnoringError(
                                 bounds_dip, 1.0f / applied_state().ui_scale));

  // The origin is relative to parent's window geometry.
  // See https://crbug.com/1292486.
  pending_configure_state_.bounds_dip =
      wl::TranslateBoundsToTopLevelCoordinates(
          pending_bounds_dip, xdg_parent_window->GetBoundsInDIP()) +
      xdg_parent_window->GetWindowGeometryOffsetInDIP();

  // Bounds are in the geometry space. Need to add decoration insets backs. Note
  // that the window state for WaylandPopup is always `kNormal` now, but we
  // check `pending_configure_state_.window_state` to make it consistent.
  const auto insets = delegate()->CalculateInsetsInDIP(
      pending_configure_state_.window_state.value_or(
          PlatformWindowState::kNormal));
  pending_configure_state_.bounds_dip->Inset(-insets);
  pending_configure_state_.size_px =
      delegate()->ConvertRectToPixels(pending_bounds_dip).size();
}

void WaylandPopup::HandleSurfaceConfigure(uint32_t serial) {
  if (schedule_redraw_) {
    delegate()->OnDamageRect(gfx::Rect{applied_state().size_px});
    schedule_redraw_ = false;
  }
  ProcessPendingConfigureState(serial);
}

void WaylandPopup::OnSequencePoint(int64_t seq) {
  if (!shell_popup())
    return;

  ProcessSequencePoint(seq);
  MaybeApplyLatestStateRequest(/*force=*/false);
}

void WaylandPopup::UpdateWindowMask() {
  // Popup doesn't have a shape. Update the opaqueness.
  auto region = IsOpaqueWindow() ? std::optional<std::vector<gfx::Rect>>(
                                       {gfx::Rect(latched_state().size_px)})
                                 : std::nullopt;
  root_surface()->set_opaque_region(region);
}

void WaylandPopup::PropagateBufferScale(float new_scale) {
  if (!IsSurfaceConfigured())
    return;

  if (!last_sent_buffer_scale_ ||
      last_sent_buffer_scale_.value() != new_scale) {
    shell_popup()->SetScaleFactor(new_scale);
    last_sent_buffer_scale_ = new_scale;
  }
}

void WaylandPopup::ShowTooltip(const std::u16string& text,
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

void WaylandPopup::HideTooltip() {
  auto* zaura_surface = GetZAuraSurface();
  if (zaura_surface && zaura_surface->HideTooltip()) {
    connection()->Flush();
  }
}

bool WaylandPopup::IsScreenCoordinatesEnabled() const {
  return parent_window()->IsScreenCoordinatesEnabled();
}

base::WeakPtr<WaylandWindow> WaylandPopup::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WaylandPopup::OnCloseRequest() {
  // Before calling OnCloseRequest, the |shell_popup_| must become hidden and
  // only then call OnCloseRequest().
  DCHECK(!shell_popup_);
  WaylandWindow::OnCloseRequest();
}

bool WaylandPopup::OnInitialize(PlatformWindowInitProperties properties,
                                PlatformWindowDelegate::State* state) {
  DCHECK(parent_window());

  // `window_state` is always `kNormal` on WaylandPopup.
  state->window_state = PlatformWindowState::kNormal;

  state->window_scale = parent_window()->applied_state().window_scale;
  shadow_type_ = properties.shadow_type;
  return true;
}

WaylandPopup* WaylandPopup::AsWaylandPopup() {
  return this;
}

bool WaylandPopup::IsSurfaceConfigured() {
  return shell_popup() ? shell_popup()->IsConfigured() : false;
}

void WaylandPopup::SetWindowGeometry(
    const PlatformWindowDelegate::State& state) {
  if (!shell_popup_) {
    return;
  }

  // State's `bounds_dip` is in UI coordinates space (ie: ui_scale'd), thus
  // before sending it through Wayland, it must be reverse-transformed to
  // Wayland coordinates space.
  gfx::Rect geometry_dip(gfx::ScaleToEnclosingRectIgnoringError(
      gfx::Rect(state.bounds_dip.size()), state.ui_scale));

  geometry_dip.Inset(delegate()->CalculateInsetsInDIP(state.window_state));
  shell_popup_->SetWindowGeometry(geometry_dip);
}

void WaylandPopup::AckConfigure(uint32_t serial) {
  DCHECK(shell_popup_);
  shell_popup_->AckConfigure(serial);
}
}  // namespace ui
