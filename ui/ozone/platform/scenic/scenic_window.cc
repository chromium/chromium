// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window.h"

#include <fidl/fuchsia.ui.pointer/cpp/hlcpp_conversion.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/platform_window/fuchsia/scenic_window_delegate.h"

namespace ui {

namespace {

// Converts Scenic's rect-based representation of insets to gfx::Insets.
// Returns zero-width insets if |inset_from_min| and |inset_from_max| are
// uninitialized (indicating that no insets were provided from Scenic).
gfx::Insets ConvertInsets(
    float device_pixel_ratio,
    const fuchsia::ui::gfx::ViewProperties& view_properties) {
  return gfx::Insets::TLBR(
      device_pixel_ratio * view_properties.inset_from_min.y,
      device_pixel_ratio * view_properties.inset_from_min.x,
      device_pixel_ratio * view_properties.inset_from_max.y,
      device_pixel_ratio * view_properties.inset_from_max.x);
}

}  // namespace

ScenicWindow::ScenicWindow(ScenicWindowManager* window_manager,
                           PlatformWindowDelegate* delegate,
                           PlatformWindowInitProperties properties)
    : manager_(window_manager),
      delegate_(delegate),
      scenic_window_delegate_(properties.scenic_window_delegate),
      window_id_(manager_->AddWindow(this)),
      view_ref_(std::move(properties.view_ref_pair.view_ref)),
      view_controller_(std::move(properties.view_controller)),
      bounds_(delegate_->ConvertRectToPixels(properties.bounds)) {
  {
    // Send graphics and input endpoints to Scenic. The endpoints are dormant
    // until the Session's Present call, at the bottom of this block.
    fuchsia::ui::scenic::SessionEndpoints endpoints;
    fuchsia::ui::scenic::SessionPtr session_ptr;
    endpoints.set_session(session_ptr.NewRequest());
    fuchsia::ui::scenic::SessionListenerHandle listener_handle;
    auto listener_request = listener_handle.NewRequest();
    endpoints.set_session_listener(std::move(listener_handle));

    auto touch_source_endpoints =
        fidl::CreateEndpoints<fuchsia_ui_pointer::TouchSource>();
    ZX_CHECK(touch_source_endpoints.is_ok(),
             touch_source_endpoints.status_value());
    endpoints.set_touch_source(
        fidl::NaturalToHLCPP(std::move(touch_source_endpoints->server)));

    auto mouse_source_endpoints =
        fidl::CreateEndpoints<fuchsia_ui_pointer::MouseSource>();
    ZX_CHECK(mouse_source_endpoints.is_ok(),
             mouse_source_endpoints.status_value());
    endpoints.set_mouse_source(
        fidl::NaturalToHLCPP(std::move(mouse_source_endpoints->server)));

    endpoints.set_view_ref_focused(view_ref_focused_.NewRequest());
    manager_->GetScenic()->CreateSessionT(std::move(endpoints), [] {});

    // Set up pointer and focus event processors.
    pointer_handler_.emplace(std::move(touch_source_endpoints->client),
                             std::move(mouse_source_endpoints->client));
    pointer_handler_->StartWatching(base::BindRepeating(
        &ScenicWindow::DispatchEvent,
        // This is safe since |pointer_handler_| is a class member.
        base::Unretained(this)));

    view_ref_focused_->Watch(
        fit::bind_member(this, &ScenicWindow::OnViewRefFocusedWatchResult));
    view_ref_focused_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "Focus listener disconnected.";
    });

    // Set up GFX Session and scene resources.
    scenic_session_.emplace(std::move(session_ptr),
                            std::move(listener_request));
    scenic_session_->set_error_handler(
        fit::bind_member(this, &ScenicWindow::OnScenicError));
    scenic_session_->set_event_handler(
        fit::bind_member(this, &ScenicWindow::OnScenicEvents));
    scenic_session_->SetDebugName("Chromium ScenicWindow");

    view_.emplace(&scenic_session_.value(), std::move(properties.view_token),
                  std::move(properties.view_ref_pair.control_ref),
                  CloneViewRef(), "chromium window");

    node_.emplace(&scenic_session_.value());

    // Subscribe to metrics events from the node. Metrics events provide the
    // device pixel ratio for the screen.
    node_->SetEventMask(fuchsia::ui::gfx::kMetricsEventMask);

    // To receive metrics events on this node, attach it to the scene graph.
    view_->AddChild(*node_);

    input_node_.emplace(&scenic_session_.value());
    render_node_.emplace(&scenic_session_.value());

    safe_presenter_.emplace(&scenic_session_.value());
    safe_presenter_->QueuePresent();
  }

  if (view_controller_) {
    view_controller_.set_error_handler(
        fit::bind_member(this, &ScenicWindow::OnViewControllerDisconnected));
  }

  delegate_->OnAcceleratedWidgetAvailable(window_id_);

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

ScenicWindow::~ScenicWindow() {
  manager_->RemoveWindow(window_id_, this);
}

fuchsia::ui::views::ViewRef ScenicWindow::CloneViewRef() {
  fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref_.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}

gfx::Rect ScenicWindow::GetBoundsInPixels() const {
  return bounds_;
}

void ScenicWindow::SetBoundsInPixels(const gfx::Rect& bounds) {
  // This path should only be reached in tests.
  bounds_ = bounds;
}

gfx::Rect ScenicWindow::GetBoundsInDIP() const {
  return delegate_->ConvertRectToDIP(bounds_);
}

void ScenicWindow::SetBoundsInDIP(const gfx::Rect& bounds) {
  // This path should only be reached in tests.
  bounds_ = delegate_->ConvertRectToPixels(bounds);
}

void ScenicWindow::SetTitle(const std::u16string& title) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::Show(bool inactive) {
  if (is_visible_)
    return;

  is_visible_ = true;

  UpdateRootNodeVisibility();

  // Call Present2() to ensure that the scenic session commands are processed,
  // which is necessary to receive metrics event from Scenic.
  safe_presenter_->QueuePresent();
}

void ScenicWindow::Hide() {
  if (!is_visible_)
    return;

  is_visible_ = false;

  UpdateRootNodeVisibility();
}

void ScenicWindow::Close() {
  if (view_controller_) {
    view_controller_->Dismiss();
    view_controller_ = nullptr;
  }
  Hide();
  delegate_->OnClosed();
}

bool ScenicWindow::IsVisible() const {
  return is_visible_;
}

void ScenicWindow::PrepareForShutdown() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::SetCapture() {
  // TODO(crbug.com/1231516): Use Scenic capture APIs.
  NOTIMPLEMENTED_LOG_ONCE();
  has_capture_ = true;
}

void ScenicWindow::ReleaseCapture() {
  // TODO(crbug.com/1231516): Use Scenic capture APIs.
  NOTIMPLEMENTED_LOG_ONCE();
  has_capture_ = false;
}

bool ScenicWindow::HasCapture() const {
  // TODO(crbug.com/1231516): Use Scenic capture APIs.
  NOTIMPLEMENTED_LOG_ONCE();
  return has_capture_;
}

void ScenicWindow::SetFullscreen(bool fullscreen, int64_t target_display_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  DCHECK_EQ(target_display_id, display::kInvalidDisplayId);
  is_fullscreen_ = fullscreen;
}

void ScenicWindow::Maximize() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::Minimize() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::Restore() {
  NOTIMPLEMENTED_LOG_ONCE();
}

PlatformWindowState ScenicWindow::GetPlatformWindowState() const {
  if (is_fullscreen_)
    return PlatformWindowState::kFullScreen;
  if (!is_view_attached_)
    return PlatformWindowState::kMinimized;

  // TODO(crbug.com/1241868): We cannot tell what portion of the screen is
  // occupied by the View, so report is as maximized to reduce the space used
  // by any browser chrome.
  return PlatformWindowState::kMaximized;
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
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
}

gfx::Rect ScenicWindow::GetRestoredBoundsInDIP() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

void ScenicWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                  const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::SizeConstraintsChanged() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::AttachSurfaceView(
    fuchsia::ui::views::ViewHolderToken token) {
  surface_view_holder_ = std::make_unique<scenic::ViewHolder>(
      &scenic_session_.value(), std::move(token), "chromium window surface");

  // Configure the ViewHolder not to be focusable, or hit-testable, to ensure
  // that it cannot receive input.
  fuchsia::ui::gfx::ViewProperties view_properties;
  view_properties.bounding_box = {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
  view_properties.focus_change = false;
  surface_view_holder_->SetViewProperties(std::move(view_properties));
  surface_view_holder_->SetHitTestBehavior(
      fuchsia::ui::gfx::HitTestBehavior::kSuppress);

  render_node_->DetachChildren();
  render_node_->AddChild(*surface_view_holder_);

  safe_presenter_->QueuePresent();
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

void ScenicWindow::OnScenicError(zx_status_t status) {
  LOG(ERROR) << "scenic::Session failed with code " << status << ".";
  delegate_->OnCloseRequest();
}

void ScenicWindow::OnScenicEvents(
    std::vector<fuchsia::ui::scenic::Event> events) {
  for (const auto& event : events) {
    if (event.is_gfx()) {
      switch (event.gfx().Which()) {
        case fuchsia::ui::gfx::Event::kMetrics: {
          if (event.gfx().metrics().node_id != node_->id())
            continue;
          OnViewMetrics(event.gfx().metrics().metrics);
          break;
        }
        case fuchsia::ui::gfx::Event::kViewPropertiesChanged: {
          DCHECK(event.gfx().view_properties_changed().view_id == view_->id());
          OnViewProperties(event.gfx().view_properties_changed().properties);
          break;
        }
        case fuchsia::ui::gfx::Event::kViewAttachedToScene: {
          DCHECK(event.gfx().view_attached_to_scene().view_id == view_->id());
          OnViewAttachedChanged(true);
          break;
        }
        case fuchsia::ui::gfx::Event::kViewDetachedFromScene: {
          DCHECK(event.gfx().view_detached_from_scene().view_id == view_->id());
          OnViewAttachedChanged(false);

          // Detach the surface view. This is necessary to ensure that the
          // current content doesn't become visible when the view is attached
          // again.
          render_node_->DetachChildren();
          surface_view_holder_.reset();
          safe_presenter_->QueuePresent();

          // Destroy and recreate AcceleratedWidget. This will force the
          // compositor drop the current LayerTreeFrameSink together with the
          // corresponding ScenicSurface. They will be created again only after
          // the window becomes visible again.
          delegate_->OnAcceleratedWidgetDestroyed();
          delegate_->OnAcceleratedWidgetAvailable(window_id_);

          break;
        }
        default:
          break;
      }
    }
  }
}

void ScenicWindow::OnViewAttachedChanged(bool is_view_attached) {
  PlatformWindowState old_state = GetPlatformWindowState();
  is_view_attached_ = is_view_attached;
  PlatformWindowState new_state = GetPlatformWindowState();
  if (old_state != new_state) {
    delegate_->OnWindowStateChanged(old_state, new_state);
  }
}

void ScenicWindow::OnViewMetrics(const fuchsia::ui::gfx::Metrics& metrics) {
  device_pixel_ratio_ = std::max(metrics.scale_x, metrics.scale_y);
  if (scenic_window_delegate_)
    scenic_window_delegate_->OnScenicPixelScale(this, device_pixel_ratio_);

  if (view_properties_)
    UpdateSize();
}

void ScenicWindow::OnViewProperties(
    const fuchsia::ui::gfx::ViewProperties& properties) {
  view_properties_ = properties;
  if (device_pixel_ratio_ > 0.0)
    UpdateSize();
}

void ScenicWindow::OnViewRefFocusedWatchResult(
    fuchsia::ui::views::FocusState focus_state) {
  delegate_->OnActivationChanged(focus_state.focused());

  view_ref_focused_->Watch(
      fit::bind_member(this, &ScenicWindow::OnViewRefFocusedWatchResult));
}

void ScenicWindow::UpdateSize() {
  DCHECK_GT(device_pixel_ratio_, 0.0);
  DCHECK(view_properties_);

  const float width = view_properties_->bounding_box.max.x -
                      view_properties_->bounding_box.min.x;
  const float height = view_properties_->bounding_box.max.y -
                       view_properties_->bounding_box.min.y;

  const gfx::Point old_origin = bounds_.origin();
  bounds_ = gfx::Rect(ceilf(width * device_pixel_ratio_),
                      ceilf(height * device_pixel_ratio_));

  // Update the root node to be shown, or hidden, based on the View state.
  // If the root node is not visible then skip resizing content, etc.
  if (!UpdateRootNodeVisibility())
    return;

  // Translate the node by half of the view dimensions to put it in the center
  // of the view.
  node_->SetTranslation(width / 2.0, height / 2.0, 0.f);

  // Scale the render node so that surface rect can always be 1x1.
  render_node_->SetScale(width, height, 1.f);

  // Resize input node to cover the whole surface.
  scenic::Rectangle window_rect(&scenic_session_.value(), width, height);
  input_node_->SetShape(window_rect);

  // This is necessary when using vulkan because ImagePipes are presented
  // separately and we need to make sure our sizes change is committed.
  safe_presenter_->QueuePresent();

  PlatformWindowDelegate::BoundsChange bounds(old_origin != bounds_.origin());
  bounds.system_ui_overlap =
      ConvertInsets(device_pixel_ratio_, *view_properties_);
  delegate_->OnBoundsChanged(bounds);
}

bool ScenicWindow::UpdateRootNodeVisibility() {
  bool should_show_root_node = is_visible_ && !is_zero_sized();
  if (should_show_root_node != is_root_node_shown_) {
    is_root_node_shown_ = should_show_root_node;
    if (should_show_root_node) {
      // Attach nodes to render content and receive input.
      node_->AddChild(*input_node_);
      node_->AddChild(*render_node_);
    } else {
      node_->DetachChildren();
    }
  }
  return is_root_node_shown_;
}

void ScenicWindow::OnViewControllerDisconnected(zx_status_t status) {
  view_controller_ = nullptr;
  delegate_->OnCloseRequest();
}

}  // namespace ui
