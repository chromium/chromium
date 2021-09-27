// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_window.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"

namespace ui {

FlatlandWindow::FlatlandWindow(FlatlandWindowManager* window_manager,
                               PlatformWindowDelegate* delegate,
                               PlatformWindowInitProperties properties)
    : manager_(window_manager),
      delegate_(delegate),
      window_id_(manager_->AddWindow(this)),
      event_dispatcher_(this),
      bounds_(properties.bounds) {
  // TODO(crbug.com/1230150): Add OnError after SDK roll.
  flatland_.flatland()->SetDebugName("Chromium FlatlandWindow");

  // TODO(crbug.com/1230150): Link to parent using |properties.view_token|.

  root_transform_id_ = {++next_transform_id_};
  flatland_.flatland()->CreateTransform(root_transform_id_);

  render_transform_id_ = {++next_transform_id_};
  flatland_.flatland()->CreateTransform(render_transform_id_);
  flatland_.QueuePresent();

  delegate_->OnAcceleratedWidgetAvailable(window_id_);

  if (properties.enable_keyboard) {
    virtual_keyboard_enabled_ = properties.enable_virtual_keyboard;
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

fuchsia::ui::views::ViewRef FlatlandWindow::CloneViewRef() {
  // TODO(crbug.com/1230150): Tie ViewRef creation to Flatland creation.
  fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref_.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}

gfx::Rect FlatlandWindow::GetBounds() const {
  return bounds_;
}

void FlatlandWindow::SetBounds(const gfx::Rect& bounds) {
  // This path should only be reached in tests.
  bounds_ = bounds;
}

void FlatlandWindow::SetTitle(const std::u16string& title) {
  NOTIMPLEMENTED();
}

void FlatlandWindow::Show(bool inactive) {
  if (visible_)
    return;

  flatland_.flatland()->SetRootTransform(root_transform_id_);
  flatland_.QueuePresent();
}

void FlatlandWindow::Hide() {
  if (!visible_)
    return;

  flatland_.flatland()->SetRootTransform({0});
  flatland_.QueuePresent();
}

void FlatlandWindow::Close() {
  Hide();
  delegate_->OnClosed();
}

bool FlatlandWindow::IsVisible() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

void FlatlandWindow::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

void FlatlandWindow::SetCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void FlatlandWindow::ReleaseCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool FlatlandWindow::HasCapture() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void FlatlandWindow::ToggleFullscreen() {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return PlatformWindowState::kNormal;
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

void FlatlandWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Rect FlatlandWindow::GetRestoredBoundsInPixels() const {
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
  OnViewMetrics(info.pixel_scale());
  OnViewProperties(info.logical_size());
}

void FlatlandWindow::UpdateSize() {
  DCHECK_GT(device_pixel_ratio_, 0.0);
  DCHECK(view_properties_);

  const uint32_t width = view_properties_->width;
  const uint32_t height = view_properties_->height;

  bounds_ = gfx::Rect(ceilf(width * device_pixel_ratio_),
                      ceilf(height * device_pixel_ratio_));

  // TODO(crbug.com/1230150): Handle zero size scenario.

  // Translate the node by half of the view dimensions to put it in the center
  // of the view.
  flatland_.flatland()->SetTranslation(
      root_transform_id_,
      {static_cast<int32_t>(width / 2), static_cast<int32_t>(height / 2)});

  // Scale the rendered image.
  flatland_.flatland()->SetImageDestinationSize(surface_content_id_,
                                                {width, height});

  // This is necessary when using vulkan because ImagePipes are presented
  // separately and we need to make sure our sizes change is committed.
  flatland_.QueuePresent();

  PlatformWindowDelegate::BoundsChange bounds;
  bounds.bounds = bounds_;
  // TODO(crbug.com/1230150): Calculate insets and update.
  delegate_->OnBoundsChanged(bounds);
}

void FlatlandWindow::OnViewMetrics(const fuchsia::math::SizeU& metrics) {
  device_pixel_ratio_ = std::max(metrics.width, metrics.height);

  if (view_properties_)
    UpdateSize();
}

void FlatlandWindow::OnViewProperties(const fuchsia::math::SizeU& properties) {
  view_properties_ = properties;
  if (device_pixel_ratio_ > 0.0)
    UpdateSize();
}

void FlatlandWindow::OnViewAttachedChanged(bool is_view_attached) {
  if (is_view_attached) {
    delegate_->OnWindowStateChanged(PlatformWindowState::kMinimized,
                                    PlatformWindowState::kNormal);
  } else {
    delegate_->OnWindowStateChanged(PlatformWindowState::kNormal,
                                    PlatformWindowState::kMinimized);
  }
}

void FlatlandWindow::OnInputEvent(const fuchsia::ui::input::InputEvent& event) {
  if (event.is_focus()) {
    delegate_->OnActivationChanged(event.focus().focused);
  } else {
    // Flatland doesn't care if the input event was handled, so ignore the
    // "handled" status.
    ignore_result(event_dispatcher_.ProcessEvent(event));
  }
}

void FlatlandWindow::DispatchEvent(ui::Event* event) {
  if (event->IsLocatedEvent()) {
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    gfx::PointF location = located_event->location_f();
    location.Scale(device_pixel_ratio_);
    located_event->set_location_f(location);
  }
  delegate_->DispatchEvent(event);
}

}  // namespace ui
