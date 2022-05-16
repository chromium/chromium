// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_window.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_identity.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/memory/scoped_refptr.h"
#include "flatland_connection.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"

namespace ui {

FlatlandWindow::FlatlandWindow(FlatlandWindowManager* window_manager,
                               PlatformWindowDelegate* delegate,
                               PlatformWindowInitProperties properties)
    : manager_(window_manager),
      window_delegate_(delegate),
      window_id_(manager_->AddWindow(this)),
      view_ref_(std::move(properties.view_ref_pair.view_ref)),
      view_controller_(std::move(properties.view_controller)),
      flatland_("Chromium FlatlandWindow"),
      bounds_(properties.bounds) {
  if (view_controller_) {
    view_controller_.set_error_handler(
        fit::bind_member(this, &FlatlandWindow::OnViewControllerDisconnected));
  }
  fuchsia::ui::views::ViewIdentityOnCreation view_identity = {
      .view_ref = CloneViewRef(),
      .view_ref_control = std::move(properties.view_ref_pair.control_ref)};

  fuchsia::ui::composition::ViewBoundProtocols view_bound_protocols;
  view_bound_protocols.set_view_ref_focused(view_ref_focused_.NewRequest());
  fuchsia::ui::pointer::TouchSourceHandle touch_source;
  view_bound_protocols.set_touch_source(touch_source.NewRequest());
  fuchsia::ui::pointer::MouseSourceHandle mouse_source;
  view_bound_protocols.set_mouse_source(mouse_source.NewRequest());

  pointer_handler_ = std::make_unique<PointerEventsHandler>(
      std::move(touch_source), std::move(mouse_source));
  pointer_handler_->StartWatching(base::BindRepeating(
      &FlatlandWindow::DispatchEvent,
      // This is safe since |pointer_handler_| is a class member.
      base::Unretained(this)));

  flatland_.flatland()->CreateView2(
      std::move(properties.view_creation_token), std::move(view_identity),
      std::move(view_bound_protocols), parent_viewport_watcher_.NewRequest());
  parent_viewport_watcher_->GetLayout(
      fit::bind_member(this, &FlatlandWindow::OnGetLayout));
  parent_viewport_watcher_->GetStatus(
      fit::bind_member(this, &FlatlandWindow::OnGetStatus));
  view_ref_focused_->Watch(
      fit::bind_member(this, &FlatlandWindow::OnViewRefFocusedWatchResult));

  root_transform_id_ = flatland_.NextTransformId();
  flatland_.flatland()->CreateTransform(root_transform_id_);

  window_delegate_->OnAcceleratedWidgetAvailable(window_id_);

  if (properties.enable_keyboard) {
    is_virtual_keyboard_enabled_ = properties.enable_virtual_keyboard;
    keyboard_service_ = base::ComponentContextForProcess()
                            ->svc()
                            ->Connect<fuchsia::ui::input3::Keyboard>();
    keyboard_service_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "input3.Keyboard service disconnected.";
    });
    keyboard_client_ = std::make_unique<KeyboardClient>(keyboard_service_.get(),
                                                        CloneViewRef(), this);
  } else {
    DCHECK(!properties.enable_virtual_keyboard);
  }
}

FlatlandWindow::~FlatlandWindow() {
  manager_->RemoveWindow(window_id_, this);
}

void FlatlandWindow::AttachSurfaceContent(
    fuchsia::ui::views::ViewportCreationToken token) {
  if (surface_content_id_.value) {
    flatland_.flatland()->ReleaseViewport(surface_content_id_, [](auto) {});
    flatland_.flatland()->ReleaseTransform(surface_transform_id_);
  }

  surface_transform_id_ = flatland_.NextTransformId();
  flatland_.flatland()->CreateTransform(surface_transform_id_);
  flatland_.flatland()->AddChild(root_transform_id_, surface_transform_id_);

  fuchsia::ui::composition::ViewportProperties properties;
  properties.set_logical_size({static_cast<uint32_t>(bounds_.width()),
                               static_cast<uint32_t>(bounds_.height())});

  surface_content_id_ = flatland_.NextContentId();
  fuchsia::ui::composition::ChildViewWatcherPtr content_link;
  flatland_.flatland()->CreateViewport(surface_content_id_, std::move(token),
                                       std::move(properties),
                                       content_link.NewRequest());
  flatland_.flatland()->SetContent(surface_transform_id_, surface_content_id_);
  flatland_.Present();

  // View is actually not attached but without it we dont get OutputPresenter
  // updates.
  OnViewAttachedChanged(true);
}

fuchsia::ui::views::ViewRef FlatlandWindow::CloneViewRef() {
  fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref_.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}

gfx::Rect FlatlandWindow::GetBoundsInPixels() const {
  return bounds_;
}

void FlatlandWindow::SetBoundsInPixels(const gfx::Rect& bounds) {
  // This path should only be reached in tests.
  bounds_ = bounds;
}

gfx::Rect FlatlandWindow::GetBoundsInDIP() const {
  return window_delegate_->ConvertRectToDIP(bounds_);
}

void FlatlandWindow::SetBoundsInDIP(const gfx::Rect& bounds) {
  // This path should only be reached in tests.
  bounds_ = window_delegate_->ConvertRectToPixels(bounds);
}

void FlatlandWindow::SetTitle(const std::u16string& title) {
  NOTIMPLEMENTED();
}

void FlatlandWindow::Show(bool inactive) {
  if (is_visible_)
    return;

  is_visible_ = true;
  flatland_.flatland()->SetRootTransform(root_transform_id_);
  flatland_.Present();
}

void FlatlandWindow::Hide() {
  if (!is_visible_)
    return;

  is_visible_ = false;
  flatland_.flatland()->SetRootTransform({0});
  flatland_.Present();
}

void FlatlandWindow::Close() {
  if (view_controller_) {
    view_controller_->Dismiss();
    view_controller_ = nullptr;
  }
  Hide();
  window_delegate_->OnClosed();
}

bool FlatlandWindow::IsVisible() const {
  return is_visible_;
}

void FlatlandWindow::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

void FlatlandWindow::SetCapture() {
  has_capture_ = true;
}

void FlatlandWindow::ReleaseCapture() {
  has_capture_ = false;
}

bool FlatlandWindow::HasCapture() const {
  return has_capture_;
}

void FlatlandWindow::ToggleFullscreen() {
  NOTIMPLEMENTED_LOG_ONCE();
  is_fullscreen_ = !is_fullscreen_;
}

void FlatlandWindow::Maximize() {
  NOTIMPLEMENTED();
}

void FlatlandWindow::Minimize() {
  NOTIMPLEMENTED();
}

void FlatlandWindow::Restore() {
  NOTIMPLEMENTED();
}

PlatformWindowState FlatlandWindow::GetPlatformWindowState() const {
  NOTIMPLEMENTED_LOG_ONCE();
  if (is_fullscreen_)
    return PlatformWindowState::kFullScreen;
  if (!is_view_attached_)
    return PlatformWindowState::kMinimized;

  // TODO(crbug.com/1241868): We cannot tell what portion of the screen is
  // occupied by the View, so report is as maximized to reduce the space used
  // by any browser chrome.
  return PlatformWindowState::kMaximized;
}

void FlatlandWindow::Activate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void FlatlandWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void FlatlandWindow::SetUseNativeFrame(bool use_native_frame) {}

bool FlatlandWindow::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void FlatlandWindow::SetCursor(scoped_refptr<PlatformCursor> cursor) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void FlatlandWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void FlatlandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void FlatlandWindow::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Rect FlatlandWindow::GetRestoredBoundsInDIP() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void FlatlandWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                    const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED();
}

void FlatlandWindow::SizeConstraintsChanged() {
  NOTIMPLEMENTED();
}

void FlatlandWindow::OnGetLayout(fuchsia::ui::composition::LayoutInfo info) {
  // TODO(https://fxbug.dev/99312): Read device pixel ratio from LayoutInfo when
  // available.
  view_properties_ = info.logical_size();

  if (view_properties_ || device_pixel_ratio_ > 0.0)
    UpdateSize();

  // Size update is sent via |delegate_| and SetViewportProperties().
  if (surface_content_id_.value) {
    fuchsia::ui::composition::ViewportProperties properties;
    properties.set_logical_size(info.logical_size());
    flatland_.flatland()->SetViewportProperties(surface_content_id_,
                                                std::move(properties));
    flatland_.Present();
  }

  parent_viewport_watcher_->GetLayout(
      fit::bind_member(this, &FlatlandWindow::OnGetLayout));
}

void FlatlandWindow::OnGetStatus(
    fuchsia::ui::composition::ParentViewportStatus status) {
  switch (status) {
    case fuchsia::ui::composition::ParentViewportStatus::CONNECTED_TO_DISPLAY:
      OnViewAttachedChanged(true);
      break;
    case fuchsia::ui::composition::ParentViewportStatus::
        DISCONNECTED_FROM_DISPLAY:
      OnViewAttachedChanged(false);
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
  parent_viewport_watcher_->GetStatus(
      fit::bind_member(this, &FlatlandWindow::OnGetStatus));
}

void FlatlandWindow::OnViewRefFocusedWatchResult(
    fuchsia::ui::views::FocusState focus_state) {
  window_delegate_->OnActivationChanged(focus_state.focused());

  view_ref_focused_->Watch(
      fit::bind_member(this, &FlatlandWindow::OnViewRefFocusedWatchResult));
}

void FlatlandWindow::UpdateSize() {
  DCHECK_GT(device_pixel_ratio_, 0.0);
  DCHECK(view_properties_);

  const uint32_t width = view_properties_->width;
  const uint32_t height = view_properties_->height;

  bounds_ = gfx::Rect(ceilf(width * device_pixel_ratio_),
                      ceilf(height * device_pixel_ratio_));

  PlatformWindowDelegate::BoundsChange bounds;
  bounds.bounds = bounds_;
  // TODO(fxbug.dev/93998): Calculate insets and update.
  window_delegate_->OnBoundsChanged(bounds);
}

void FlatlandWindow::OnViewAttachedChanged(bool is_view_attached) {
  PlatformWindowState old_state = GetPlatformWindowState();
  is_view_attached_ = is_view_attached;
  PlatformWindowState new_state = GetPlatformWindowState();
  if (old_state != new_state) {
    window_delegate_->OnWindowStateChanged(old_state, new_state);
  }
}

void FlatlandWindow::DispatchEvent(ui::Event* event) {
  if (event->IsLocatedEvent()) {
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    gfx::PointF location = located_event->location_f();
    location.Scale(device_pixel_ratio_);
    located_event->set_location_f(location);
  }
  window_delegate_->DispatchEvent(event);
}

void FlatlandWindow::OnViewControllerDisconnected(zx_status_t status) {
  view_controller_ = nullptr;
  window_delegate_->OnCloseRequest();
}

}  // namespace ui
