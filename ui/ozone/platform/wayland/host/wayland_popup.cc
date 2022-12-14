// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_popup.h"

#include <aura-shell-client-protocol.h>

#include "base/auto_reset.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

namespace ui {

WaylandPopup::WaylandPopup(PlatformWindowDelegate* delegate,
                           WaylandConnection* connection,
                           WaylandWindow* parent)
    : WaylandWindow(delegate, connection) {
  set_parent_window(parent);
}

WaylandPopup::~WaylandPopup() = default;

bool WaylandPopup::CreateShellPopup() {
  DCHECK(parent_window() && !shell_popup_);

  if (window_scale() != parent_window()->window_scale()) {
    // If scale changed while this was hidden (when WaylandPopup hides, parent
    // window's child is reset), update buffer scale accordingly.
    UpdateWindowScale(true);
  }

  const auto bounds_dip =
      wl::TranslateWindowBoundsToParentDIP(this, parent_window());

  ShellPopupParams params;
  params.bounds = bounds_dip;
  params.menu_type =
      delegate()->GetMenuType().value_or(MenuType::kRootContextMenu);
  params.anchor = delegate()->GetOwnedWindowAnchorAndRectInDIP();
  if (params.anchor.has_value()) {
    // The anchor should originate from the window geometry, not from the
    // surface.  See https://crbug.com/1292486.
    params.anchor->anchor_rect =
        wl::TranslateBoundsToParentCoordinates(
            params.anchor->anchor_rect, parent_window()->GetBoundsInDIP()) -
        parent_window()->GetWindowGeometryOffsetInDIP();

    // If size is empty, set 1x1.
    if (params.anchor->anchor_rect.size().IsEmpty())
      params.anchor->anchor_rect.set_size({1, 1});
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

  if (connection()->zaura_shell() && !aura_surface()) {
    static constexpr zaura_surface_listener zaura_surface_listener = {
        nullptr, nullptr, nullptr,       nullptr,       &DeskChanged,
        nullptr, nullptr, &TooltipShown, &TooltipHidden};
    SetAuraSurface(zaura_shell_get_aura_surface(
        connection()->zaura_shell()->wl_object(), root_surface()->surface()));
    zaura_surface_add_listener(aura_surface(), &zaura_surface_listener, this);
  }

  parent_window()->set_child_window(this);
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
    shell_popup_->Decorate();
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

  if (child_window())
    child_window()->Hide();
  WaylandWindow::Hide();

  if (IsSupportedOnAuraSurface(ZAURA_SURFACE_RELEASE_SINCE_VERSION))
    SetAuraSurface(nullptr);

  if (shell_popup_) {
    parent_window()->set_child_window(nullptr);
    shell_popup_.reset();
    decorated_via_aura_popup_ = false;
  }

  connection()->Flush();
}

bool WaylandPopup::IsVisible() const {
  return !!shell_popup_;
}

void WaylandPopup::SetBoundsInDIP(const gfx::Rect& bounds_dip) {
  auto old_bounds_dip = GetBoundsInDIP();
  WaylandWindow::SetBoundsInDIP(bounds_dip);

  // The shell popup can be null if bounds are being fixed during
  // the initialization. See WaylandPopup::CreateShellPopup.
  if (shell_popup_ && old_bounds_dip != bounds_dip && !wayland_sets_bounds_) {
    const auto bounds_dip_in_parent =
        wl::TranslateWindowBoundsToParentDIP(this, parent_window());

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
  gfx::Rect pending_bounds_dip(bounds_dip);
  if (pending_bounds_dip.IsEmpty())
    pending_bounds_dip.set_size(GetBoundsInDIP().size());
  pending_configure_state_.bounds_dip =
      wl::TranslateBoundsToTopLevelCoordinates(
          pending_bounds_dip, parent_window()->GetBoundsInDIP());
  pending_configure_state_.size_px =
      delegate()->ConvertRectToPixels(pending_bounds_dip).size();
}

void WaylandPopup::HandleSurfaceConfigure(uint32_t serial) {
  if (schedule_redraw_) {
    delegate()->OnDamageRect(gfx::Rect{size_px()});
    schedule_redraw_ = false;
  }
  ProcessPendingBoundsDip(serial);
}

void WaylandPopup::UpdateVisualSize(const gfx::Size& size_px) {
  WaylandWindow::UpdateVisualSize(size_px);

  if (!shell_popup())
    return;

  ProcessVisualSizeUpdate(size_px);
  ApplyPendingBounds();
}

void WaylandPopup::ApplyPendingBounds() {
  if (has_pending_configures()) {
    base::AutoReset<bool> auto_reset(&wayland_sets_bounds_, true);
    WaylandWindow::ApplyPendingBounds();
  }
}

void WaylandPopup::UpdateWindowMask() {
  // Popup doesn't have a shape. Update the opaqueness.
  std::vector<gfx::Rect> region{gfx::Rect{visual_size_px()}};
  root_surface()->set_opaque_region(IsOpaqueWindow() ? &region : nullptr);
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
  if (IsSupportedOnAuraSurface(ZAURA_SURFACE_SHOW_TOOLTIP_SINCE_VERSION)) {
    uint32_t zaura_shell_trigger =
        trigger == PlatformWindowTooltipTrigger::kCursor
            ? ZAURA_SURFACE_TOOLTIP_TRIGGER_CURSOR
            : ZAURA_SURFACE_TOOLTIP_TRIGGER_KEYBOARD;
    zaura_surface_show_tooltip(
        aura_surface(), base::UTF16ToUTF8(text).c_str(), position.x(),
        position.y(), zaura_shell_trigger,
        // Cast `show_delay` and `hide_delay` into int32_t as TimeDelta should
        // not be larger than what can be handled in int32_t
        base::saturated_cast<uint32_t>(show_delay.InMilliseconds()),
        base::saturated_cast<uint32_t>(hide_delay.InMilliseconds()));
  }
}

void WaylandPopup::HideTooltip() {
  if (IsSupportedOnAuraSurface(ZAURA_SURFACE_SHOW_TOOLTIP_SINCE_VERSION)) {
    zaura_surface_hide_tooltip(aura_surface());
  }
}

void WaylandPopup::TooltipShown(void* data,
                                zaura_surface* surface,
                                const char* text,
                                int32_t x,
                                int32_t y,
                                int32_t width,
                                int32_t height) {
  WaylandPopup* self = static_cast<WaylandPopup*>(data);
  DCHECK(self);
  self->delegate()->OnTooltipShownOnServer(base::UTF8ToUTF16(text),
                                           gfx::Rect(x, y, width, height));
}

void WaylandPopup::TooltipHidden(void* data, zaura_surface* surface) {
  WaylandPopup* self = static_cast<WaylandPopup*>(data);
  DCHECK(self);
  self->delegate()->OnTooltipHiddenOnServer();
}

void WaylandPopup::OnCloseRequest() {
  // Before calling OnCloseRequest, the |shell_popup_| must become hidden and
  // only then call OnCloseRequest().
  DCHECK(!shell_popup_);
  WaylandWindow::OnCloseRequest();
}

bool WaylandPopup::OnInitialize(PlatformWindowInitProperties properties) {
  DCHECK(parent_window());
  SetWindowScale(parent_window()->window_scale());
  set_ui_scale(parent_window()->ui_scale());
  shadow_type_ = properties.shadow_type;
  return true;
}

WaylandPopup* WaylandPopup::AsWaylandPopup() {
  return this;
}

bool WaylandPopup::IsSurfaceConfigured() {
  return shell_popup() ? shell_popup()->IsConfigured() : false;
}

void WaylandPopup::SetWindowGeometry(gfx::Size size_dip) {
  DCHECK(shell_popup_);
  const auto insets = GetDecorationInsetsInDIP();
  shell_popup_->SetWindowGeometry({{insets.left(), insets.top()}, size_dip});
}

void WaylandPopup::AckConfigure(uint32_t serial) {
  shell_popup()->AckConfigure(serial);
}
}  // namespace ui
