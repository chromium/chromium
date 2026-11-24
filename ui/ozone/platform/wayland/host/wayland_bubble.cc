// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_bubble.h"

#include <utility>

#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"
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
  set_shutting_down();
  WaylandToplevelWindow* root =
      GetRootParentWindow()->AsWaylandToplevelWindow();
  base::WeakPtr<WaylandWindow> weak_root = root ? root->AsWeakPtr() : nullptr;
  const bool was_bubble_ancestor = root && root->IsInBubbleAncestry(this);
  if (parent_window()) {
    if (root_surface()) {
      parent_window()->RemoveChildSubsurface(GetWidget(), TakeWaylandSurface(),
                                             std::move(subsurface_));
    }
    parent_window()->RemoveBubble(this);
    set_parent_window(nullptr);
  }
  if (was_bubble_ancestor && weak_root) {
    root->SetActiveBubble(nullptr);
  }
}

void WaylandBubble::Show(bool inactive) {
  if (subsurface_) {
    return;
  }

  auto weak_this = AsWeakPtr();
  UpdateWindowScale(false);
  if (!weak_this) {
    return;
  }
  if (parent_window()) {
    parent_window()->RemoveChildSubsurface(GetWidget(), nullptr, {});
  }
  AddToParentAsSubsurface();
  if (!weak_this) {
    return;
  }
  WaylandWindow::Show(inactive);
}

void WaylandBubble::Hide() {
  if (!subsurface_) {
    return;
  }

  wl::Object<wl_subsurface> subsurface = std::move(subsurface_);
  subsurface_position_.reset();
  position_pending_parent_commit_ = false;
  if (parent_window()) {
    subsurface = parent_window()->RemoveChildSubsurface(GetWidget(), nullptr,
                                                        std::move(subsurface));
  }
  WaylandWindow::Hide();
  subsurface.reset();
  // The window lost its wl_subsurface role, so it is no longer configured, see
  // IsSurfaceConfigured(). Notify the observers as AddToParentAsSubsurface()
  // does when it gets the role, and as XdgSurface does when it is destroyed.
  connection()->window_manager()->NotifyWindowConfigured(this);
  auto weak_this = AsWeakPtr();
  Deactivate();
  if (!weak_this) {
    return;
  }
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
  auto weak_this = AsWeakPtr();
  WaylandWindow::SetBoundsInDIP(bounds_dip);
  if (!weak_this) {
    return;
  }

  // A size change is latched once a frame of the new size is played back, at
  // which point OnSequencePoint() sends the new position along with that frame.
  // Changes that need no new frame (e.g. only the origin changes) are latched
  // immediately, so send their position now.
  if (subsurface_) {
    UpdateSubsurfacePosition(/*sync_with_frame=*/false);
  }
}

void WaylandBubble::SetInputRegion(
    std::optional<std::vector<gfx::Rect>> region_px) {
  if (accept_events_) {
    root_surface()->set_input_region(region_px);
  }
}

void WaylandBubble::Activate() {
  if (subsurface_ && activatable_) {
    if (auto* root = GetRootParentWindow()->AsWaylandToplevelWindow()) {
      root->SetActiveBubble(this);
    }
  }
}

void WaylandBubble::Deactivate() {
  if (auto* root = GetRootParentWindow()->AsWaylandToplevelWindow();
      root && root->IsInBubbleAncestry(this)) {
    root->SetActiveBubble(nullptr);
  }
}

void WaylandBubble::UpdateWindowScale(bool update_bounds) {
  auto weak_this = AsWeakPtr();
  WaylandWindow::UpdateWindowScale(update_bounds);
  if (!weak_this) {
    return;
  }
  if (subsurface_) {
    UpdateSubsurfacePosition(/*sync_with_frame=*/false);
  }
}

void WaylandBubble::OnSequencePoint(int64_t seq) {
  if (!IsSurfaceConfigured()) {
    return;
  }

  auto weak_this = AsWeakPtr();
  ProcessSequencePoint(seq);
  if (!weak_this) {
    return;
  }
  MaybeApplyLatestStateRequest(/*force=*/false);
  if (!weak_this || !IsSurfaceConfigured()) {
    return;
  }
  UpdateSubsurfacePosition(/*sync_with_frame=*/true);
}

void WaylandBubble::OnFrameCommitted(int64_t seq) {
  WaylandWindow::OnFrameCommitted(seq);
  CommitPendingSubsurfacePosition();
}

base::WeakPtr<WaylandWindow> WaylandBubble::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool WaylandBubble::IsActive() const {
  const auto* root = GetRootParentWindow()->AsWaylandToplevelWindow();
  bool is_active =
      activatable_ && root && root->IsActive() && root->active_bubble() == this;
  CHECK(activatable_ || !is_active);
  return is_active;
}

WaylandBubble* WaylandBubble::AsWaylandBubble() {
  return this;
}

void WaylandBubble::AddToParentAsSubsurface() {
  CHECK(parent_window());

  // We need to make sure that window scale matches the parent window.
  auto weak_this = AsWeakPtr();
  UpdateWindowScale(true);
  if (!weak_this) {
    return;
  }

  subsurface_ =
      root_surface()->CreateSubsurface(parent_window()->root_surface());
  CHECK(subsurface_, base::NotFatalUntil::M163);

  // Newly created wl_subsurfaces are placed at the top of the parent's stack.
  // Restack any visible security-surface siblings back above `this` before
  // `UpdateSubsurfacePosition()` commits the parent surface. `child_bubbles()`
  // is in creation order rather than stacking order, so multiple visible
  // security surfaces end up stacked in creation order.
  if (!is_security_surface_) {
    wl_surface* reference_surface = root_surface()->surface();
    for (WaylandBubble* sibling : parent_window()->child_bubbles()) {
      if (sibling != this && sibling->is_security_surface() &&
          sibling->subsurface()) {
        wl_subsurface_place_above(sibling->subsurface(), reference_surface);
        reference_surface = sibling->root_surface()->surface();
      }
    }
  }

  subsurface_position_.reset();
  position_pending_parent_commit_ = false;
  UpdateSubsurfacePosition(/*sync_with_frame=*/false);

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

void WaylandBubble::UpdateSubsurfacePosition(bool sync_with_frame) {
  CHECK(subsurface_);
  if (!parent_window() || !parent_window()->root_surface()) {
    position_pending_parent_commit_ = false;
    return;
  }
  // bounds_dip here are in Chromium UI coordinates space (i.e ui_scale'd).
  // Since they are going to be used to issue Wayland requests, they must be
  // reverse-transformed to Wayland DIP coordinates space. Use the latched
  // state, which matches the frame the compositor shows (or is about to).
  const auto& state = latched_state();
  const gfx::Point position =
      gfx::ScaleToEnclosingRectIgnoringError(
          wl::TranslateBoundsToParentCoordinates(
              state.bounds_dip, parent_window()->GetBoundsInDIP()),
          state.ui_scale)
          .origin();
  if (position == subsurface_position_) {
    return;
  }
  subsurface_position_ = position;

  // wl_subsurface.set_position is applied on the next parent commit. In
  // desynchronized mode, the bubble's own commits apply immediately, so the
  // new position and the frame with the new bounds would show up separately,
  // making the bubble jump for a frame. Synchronized mode caches the bubble's
  // next commit so the parent commit applies both. If PlayBackFrame() discards
  // this frame before committing, `subsurface_` stays synchronized until
  // OnFrameCommitted(), UpdateSubsurfacePosition(false), or Hide() runs.
  if (sync_with_frame && !position_pending_parent_commit_) {
    wl_subsurface_set_sync(subsurface_.get());
    position_pending_parent_commit_ = true;
  }
  wl_subsurface_set_position(subsurface_.get(), position.x(), position.y());
  if (!sync_with_frame) {
    if (position_pending_parent_commit_) {
      CommitPendingSubsurfacePosition();
    } else {
      parent_window()->root_surface()->Commit();
    }
  }
}

void WaylandBubble::CommitPendingSubsurfacePosition() {
  if (!position_pending_parent_commit_) {
    return;
  }
  position_pending_parent_commit_ = false;
  if (!parent_window() || !parent_window()->root_surface()) {
    return;
  }
  parent_window()->root_surface()->Commit(/*flush=*/false);
  // Any state cached in synchronized mode is applied immediately when
  // switching back to desynchronized mode.
  wl_subsurface_set_desync(subsurface_.get());
  connection()->Flush();
}

bool WaylandBubble::OnInitialize(PlatformWindowInitProperties properties,
                                 PlatformWindowDelegate::State* state) {
  CHECK(parent_window(), base::NotFatalUntil::M163);

  // `window_state` is always `kNormal` on WaylandBubble.
  state->window_state = PlatformWindowState::kNormal;

  state->window_scale = parent_window()->applied_state().window_scale;
  state->ui_scale = parent_window()->applied_state().ui_scale;
  activatable_ = properties.activatable;
  accept_events_ = properties.accept_events;
  is_security_surface_ = properties.is_security_surface;

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
