// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <algorithm>
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
#include "ui/ozone/platform/scenic/scenic_window_manager.h"

namespace ui {

ScenicWindow::ScenicWindow(ScenicWindowManager* window_manager,
                           PlatformWindowDelegate* delegate,
                           PlatformWindowInitProperties properties)
    : manager_(window_manager),
      delegate_(delegate),
      window_id_(manager_->AddWindow(this)),
      event_dispatcher_(this),
      keyboard_service_(base::ComponentContextForProcess()
                            ->svc()
                            ->Connect<fuchsia::ui::input3::Keyboard>()),
      scenic_session_(manager_->GetScenic()),
      safe_presenter_(&scenic_session_),
      view_ref_(std::move(properties.view_ref_pair.view_ref)),
      view_(&scenic_session_,
            std::move(std::move(properties.view_token)),
            std::move(properties.view_ref_pair.control_ref),
            CloneViewRef(),
            "chromium window"),
      node_(&scenic_session_),
      input_node_(&scenic_session_),
      render_node_(&scenic_session_),
      bounds_(properties.bounds) {
  scenic_session_.set_error_handler(
      fit::bind_member(this, &ScenicWindow::OnScenicError));
  scenic_session_.set_event_handler(
      fit::bind_member(this, &ScenicWindow::OnScenicEvents));
  scenic_session_.SetDebugName("Chromium ScenicWindow");

  // Subscribe to metrics events from the node. These events are used to
  // get the device pixel ratio for the screen.
  node_.SetEventMask(fuchsia::ui::gfx::kMetricsEventMask);

  // Add input shape.
  node_.AddChild(input_node_);

  node_.AddChild(render_node_);

  delegate_->OnAcceleratedWidgetAvailable(window_id_);

  keyboard_service_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "input3.Keyboard service disconnected.";
  });
  keyboard_client_ = std::make_unique<KeyboardClient>(keyboard_service_.get(),
                                                      CloneViewRef(), this);
}

ScenicWindow::~ScenicWindow() {
  manager_->RemoveWindow(window_id_, this);
}

void ScenicWindow::AttachSurfaceView(
    fuchsia::ui::views::ViewHolderToken token) {
  surface_view_holder_ = std::make_unique<scenic::ViewHolder>(
      &scenic_session_, std::move(token), "chromium window surface");

  // Configure the ViewHolder not to be focusable, or hit-testable, to ensure
  // that it cannot receive input.
  fuchsia::ui::gfx::ViewProperties view_properties;
  view_properties.bounding_box = {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
  view_properties.focus_change = false;
  surface_view_holder_->SetViewProperties(std::move(view_properties));
  surface_view_holder_->SetHitTestBehavior(
      fuchsia::ui::gfx::HitTestBehavior::kSuppress);

  render_node_.DetachChildren();
  render_node_.AddChild(*surface_view_holder_);

  safe_presenter_.QueuePresent();
}

gfx::Rect ScenicWindow::GetBounds() const {
  return bounds_;
}

void ScenicWindow::SetBounds(const gfx::Rect& bounds) {
  // This path should only be reached in tests.
  bounds_ = bounds;
}

void ScenicWindow::SetTitle(const std::u16string& title) {
  NOTIMPLEMENTED();
}

void ScenicWindow::Show(bool inactive) {
  if (visible_)
    return;

  visible_ = true;

  if (!previous_view_is_zero_sized_) {
    view_.AddChild(node_);
  }

  // Call Present2() to ensure that the scenic session commands are processed,
  // which is necessary to receive metrics event from Scenic.
  safe_presenter_.QueuePresent();
}

void ScenicWindow::Hide() {
  if (!visible_)
    return;

  visible_ = false;
  if (!previous_view_is_zero_sized_) {
    node_.Detach();
  }
}

void ScenicWindow::Close() {
  Hide();
  delegate_->OnClosed();
}

bool ScenicWindow::IsVisible() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

void ScenicWindow::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

void ScenicWindow::SetCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::ReleaseCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool ScenicWindow::HasCapture() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void ScenicWindow::ToggleFullscreen() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Maximize() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Minimize() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Restore() {
  NOTIMPLEMENTED();
}

PlatformWindowState ScenicWindow::GetPlatformWindowState() const {
  NOTIMPLEMENTED();
  return PlatformWindowState::kNormal;
}

void ScenicWindow::Activate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::SetUseNativeFrame(bool use_native_frame) {}

bool ScenicWindow::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void ScenicWindow::SetCursor(scoped_refptr<PlatformCursor> cursor) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void ScenicWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Rect ScenicWindow::GetRestoredBoundsInPixels() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void ScenicWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                  const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED();
}

void ScenicWindow::SizeConstraintsChanged() {
  NOTIMPLEMENTED();
}

void ScenicWindow::UpdateSize() {
  DCHECK_GT(device_pixel_ratio_, 0.0);
  DCHECK(view_properties_);

  const float width = view_properties_->bounding_box.max.x -
                      view_properties_->bounding_box.min.x;
  const float height = view_properties_->bounding_box.max.y -
                       view_properties_->bounding_box.min.y;

  bounds_ = gfx::Rect(ceilf(width * device_pixel_ratio_),
                      ceilf(height * device_pixel_ratio_));

  // Update this window's Screen's dimensions to match the new size.
  ScenicScreen* screen = manager_->screen();
  if (screen)
    screen->OnWindowBoundsChanged(window_id_, bounds_);

  // If the width or height of the window is zero, then we shouldn't render
  // the node. Instead, we should detach it from its parent.
  if (width == 0.f || height == 0.f) {
    if (!previous_view_is_zero_sized_) {
      if (visible_) {
        node_.Detach();
      }
      previous_view_is_zero_sized_ = true;
    }
    return;
  }

  // Otherwise we add them back to the View.
  if (previous_view_is_zero_sized_) {
    if (visible_) {
      view_.AddChild(node_);
    }
    previous_view_is_zero_sized_ = false;
  }

  // Translate the node by half of the view dimensions to put it in the center
  // of the view.
  node_.SetTranslation(width / 2.0, height / 2.0, 0.f);

  // Scale the render node so that surface rect can always be 1x1.
  render_node_.SetScale(width, height, 1.f);

  // Resize input node to cover the whole surface.
  scenic::Rectangle window_rect(&scenic_session_, width, height);
  input_node_.SetShape(window_rect);

  // This is necessary when using vulkan because ImagePipes are presented
  // separately and we need to make sure our sizes change is committed.
  safe_presenter_.QueuePresent();

  PlatformWindowDelegate::BoundsChange bounds;
  bounds.bounds = bounds_;
  bounds.system_ui_overlap =
      ConvertInsets(device_pixel_ratio_, *view_properties_);
  delegate_->OnBoundsChanged(bounds);
}

fuchsia::ui::views::ViewRef ScenicWindow::CloneViewRef() {
  fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref_.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}

void ScenicWindow::OnScenicError(zx_status_t status) {
  LOG(ERROR) << "scenic::Session failed with code " << status << ".";
  delegate_->OnClosed();
}

void ScenicWindow::OnScenicEvents(
    std::vector<fuchsia::ui::scenic::Event> events) {
  for (const auto& event : events) {
    if (event.is_gfx()) {
      switch (event.gfx().Which()) {
        case fuchsia::ui::gfx::Event::kMetrics: {
          if (event.gfx().metrics().node_id != node_.id())
            continue;
          OnViewMetrics(event.gfx().metrics().metrics);
          break;
        }
        case fuchsia::ui::gfx::Event::kViewPropertiesChanged: {
          DCHECK(event.gfx().view_properties_changed().view_id == view_.id());
          OnViewProperties(event.gfx().view_properties_changed().properties);
          break;
        }
        case fuchsia::ui::gfx::Event::kViewAttachedToScene: {
          DCHECK(event.gfx().view_attached_to_scene().view_id == view_.id());
          OnViewAttachedChanged(true);
          break;
        }
        case fuchsia::ui::gfx::Event::kViewDetachedFromScene: {
          DCHECK(event.gfx().view_detached_from_scene().view_id == view_.id());
          OnViewAttachedChanged(false);
          break;
        }
        default:
          break;
      }
    } else if (event.is_input()) {
      OnInputEvent(event.input());
    }
  }
}

void ScenicWindow::OnViewMetrics(const fuchsia::ui::gfx::Metrics& metrics) {
  device_pixel_ratio_ = std::max(metrics.scale_x, metrics.scale_y);

  if (view_properties_)
    UpdateSize();
}

void ScenicWindow::OnViewProperties(
    const fuchsia::ui::gfx::ViewProperties& properties) {
  view_properties_ = properties;
  if (device_pixel_ratio_ > 0.0)
    UpdateSize();
}

void ScenicWindow::OnViewAttachedChanged(bool is_view_attached) {
  delegate_->OnWindowStateChanged(is_view_attached
                                      ? PlatformWindowState::kNormal
                                      : PlatformWindowState::kMinimized);
}

void ScenicWindow::OnInputEvent(const fuchsia::ui::input::InputEvent& event) {
  if (event.is_focus()) {
    delegate_->OnActivationChanged(event.focus().focused);
  } else {
    // Scenic doesn't care if the input event was handled, so ignore the
    // "handled" status.
    ignore_result(event_dispatcher_.ProcessEvent(event));
  }
}

void ScenicWindow::DispatchEvent(ui::Event* event) {
  if (event->IsLocatedEvent()) {
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    gfx::PointF location = located_event->location_f();
    location.Scale(device_pixel_ratio_);
    located_event->set_location_f(location);
  }
  delegate_->DispatchEvent(event);
}

// static
gfx::Insets ScenicWindow::ConvertInsets(
    float device_pixel_ratio,
    const fuchsia::ui::gfx::ViewProperties& view_properties) {
  return gfx::Insets(device_pixel_ratio * view_properties.inset_from_min.y,
                     device_pixel_ratio * view_properties.inset_from_min.x,
                     device_pixel_ratio * view_properties.inset_from_max.y,
                     device_pixel_ratio * view_properties.inset_from_max.x);
}

}  // namespace ui
