// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_window.h"

#include <fidl/fuchsia.ui.pointer/cpp/hlcpp_conversion.h>
#include <fidl/fuchsia.ui.views/cpp/hlcpp_conversion.h>
#include <lib/async/default.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"
#include "ui/platform_window/fuchsia/scenic_window_delegate.h"

namespace ui {

namespace {

// Converts and scales Scenic's rect-based representation of insets to
// gfx::Insets. Returns zero-width insets if |view_inset| information was not
// provided in the |GetLayout()| call.
gfx::Insets ConvertInsets(float device_pixel_ratio,
                          const fuchsia::math::Inset& view_inset) {
  return gfx::ScaleToRoundedInsets(
      gfx::Insets::TLBR(view_inset.top, view_inset.left, view_inset.bottom,
                        view_inset.right),
      device_pixel_ratio);
}

}  // namespace

FlatlandWindow::FlatlandWindow(FlatlandWindowManager* window_manager,
                               PlatformWindowDelegate* platform_window_delegate,
                               PlatformWindowInitProperties properties)
    : manager_(window_manager),
      platform_window_delegate_(platform_window_delegate),
      scenic_window_delegate_(properties.scenic_window_delegate),
      window_id_(manager_->AddWindow(this)),
      view_ref_(std::move(properties.view_ref_pair.view_ref)),
      view_controller_(std::move(properties.view_controller)),
      flatland_("Chromium FlatlandWindow",
                base::BindOnce(&FlatlandWindow::OnFlatlandError,
                               base::Unretained(this))),
      bounds_(
          platform_window_delegate->ConvertRectToPixels(properties.bounds)) {
  if (view_controller_) {
    view_controller_.set_error_handler(
        fit::bind_member(this, &FlatlandWindow::OnViewControllerDisconnected));
  }
  fuchsia::ui::views::ViewIdentityOnCreation view_identity = {
      .view_ref = CloneViewRef(),
      .view_ref_control = std::move(properties.view_ref_pair.control_ref)};

  fuchsia::ui::composition::ViewBoundProtocols view_bound_protocols;
  view_bound_protocols.set_view_ref_focused(view_ref_focused_.NewRequest());
  view_ref_focused_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "ViewRefFocused disconnected.";
  });

  auto touch_source_endpoints =
      fidl::CreateEndpoints<fuchsia_ui_pointer::TouchSource>();
  ZX_CHECK(touch_source_endpoints.is_ok(),
           touch_source_endpoints.status_value());
  view_bound_protocols.set_touch_source(
      fidl::NaturalToHLCPP(std::move(touch_source_endpoints->server)));

  auto mouse_source_endpoints =
      fidl::CreateEndpoints<fuchsia_ui_pointer::MouseSource>();
  ZX_CHECK(mouse_source_endpoints.is_ok(),
           mouse_source_endpoints.status_value());
  view_bound_protocols.set_mouse_source(
      fidl::NaturalToHLCPP(std::move(mouse_source_endpoints->server)));

  pointer_handler_ = std::make_unique<PointerEventsHandler>(
      std::move(touch_source_endpoints->client),
      std::move(mouse_source_endpoints->client));
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

  // Create the infinite hit region that will cover the surface. Do not set clip
  // boundaries on this transform, so that the hit region retains maximal size.
  shield_transform_id_ = flatland_.NextTransformId();
  flatland_.flatland()->CreateTransform(shield_transform_id_);
  flatland_.flatland()->SetInfiniteHitRegion(
      shield_transform_id_,
      fuchsia::ui::composition::HitTestInteraction::DEFAULT);

  platform_window_delegate_->OnAcceleratedWidgetAvailable(window_id_);

  if (properties.enable_keyboard) {
    is_virtual_keyboard_enabled_ = properties.enable_virtual_keyboard;
    auto keyboard_client_end =
        base::fuchsia_component::Connect<fuchsia_ui_input3::Keyboard>();
    CHECK(keyboard_client_end.is_ok())
        << base::FidlConnectionErrorMessage(keyboard_client_end);
    keyboard_fidl_client_.Bind(std::move(keyboard_client_end.value()),
                               async_get_default_dispatcher(),
                               &fidl_error_event_logger_);
    keyboard_client_ = std::make_unique<KeyboardClient>(
        keyboard_fidl_client_, fidl::HLCPPToNatural(CloneViewRef()), this);
  } else {
    DCHECK(!properties.enable_virtual_keyboard);
  }
}

FlatlandWindow::~FlatlandWindow() {
  manager_->RemoveWindow(window_id_, this);
}

void FlatlandWindow::ResetSurfaceContent() {
  if (!surface_content_id_.value) {
    return;
  }
  flatland_.flatland()->RemoveChild(root_transform_id_, surface_transform_id_);
  flatland_.flatland()->RemoveChild(root_transform_id_, shield_transform_id_);

  flatland_.flatland()->ReleaseViewport(surface_content_id_, [](auto) {});
  flatland_.flatland()->ReleaseTransform(surface_transform_id_);

  surface_content_id_ = {};
  surface_transform_id_ = {};
}

void FlatlandWindow::AttachSurfaceContent(
    fuchsia::ui::views::ViewportCreationToken token) {
  // 0x0 is not a valid Viewport size for Flatland. Sending these commands will
  // cause an error that results in channel closure. We will receive a non-zero
  // size at OnGetLayout(), so we wait until then to run these commands.
  if (!logical_size_) {
    pending_attach_surface_content_closure_ =
        base::BindOnce(&FlatlandWindow::AttachSurfaceContent,
                       base::Unretained(this), std::move(token));
    return;
  }

  ResetSurfaceContent();

  surface_transform_id_ = flatland_.NextTransformId();
  flatland_.flatland()->CreateTransform(surface_transform_id_);
  // Hit-testing starts from the last child transform added, and propagates
  // forward to the first. Adding the shield transform last therefore allows it
  // to consume all hit-tests, preventing the surface from handling them to
  // capture input.
  flatland_.flatland()->AddChild(root_transform_id_, surface_transform_id_);
  flatland_.flatland()->AddChild(root_transform_id_, shield_transform_id_);

  fuchsia::ui::composition::ViewportProperties properties;
  properties.set_logical_size({static_cast<uint32_t>(logical_size_->width()),
                               static_cast<uint32_t>(logical_size_->height())});

  surface_content_id_ = flatland_.NextContentId();
  fuchsia::ui::composition::ChildViewWatcherPtr content_link;
  flatland_.flatland()->CreateViewport(surface_content_id_, std::move(token),
                                       std::move(properties),
                                       content_link.NewRequest());
  flatland_.flatland()->SetContent(surface_transform_id_, surface_content_id_);
  flatland_.Present();
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
  // TODO(crbug.com/42050542): Remove the hardcoded values and return
  // |logical_size_|.
  return platform_window_delegate_->ConvertRectToDIP(bounds_);
}

void FlatlandWindow::SetBoundsInDIP(const gfx::Rect& bounds) {
  // This path should only be reached in tests.
  bounds_ = platform_window_delegate_->ConvertRectToPixels(bounds);
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
  platform_window_delegate_->OnClosed();
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

void FlatlandWindow::SetFullscreen(bool fullscreen, int64_t target_display_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  DCHECK_EQ(target_display_id, display::kInvalidDisplayId);
  is_fullscreen_ = fullscreen;
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
  if (is_fullscreen_)
    return PlatformWindowState::kFullScreen;
  if (!is_view_attached_)
    return PlatformWindowState::kMinimized;

  // TODO(crbug.com/42050332): We cannot tell what portion of the screen is
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
  logical_size_ =
      gfx::Size(info.logical_size().width, info.logical_size().height);
  device_pixel_ratio_ =
      std::max(info.device_pixel_ratio().x, info.device_pixel_ratio().y);
  DCHECK_EQ(info.device_pixel_ratio().x, info.device_pixel_ratio().y);

  if (info.has_inset()) {
    view_inset_ = ConvertInsets(device_pixel_ratio_, info.inset());
  }

  if (scenic_window_delegate_) {
    scenic_window_delegate_->OnScenicPixelScale(this, device_pixel_ratio_);
  }

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
      // We may get here after the initial `GetStatus()` call. There is no need
      // to do anything in this case.
      if (!is_view_attached_) {
        break;
      }

      OnViewAttachedChanged(false);

      // Detach the surface view. This is necessary to ensure that the
      // current content doesn't become visible when the view is attached
      // again.
      ResetSurfaceContent();
      flatland_.Present();
      pending_attach_surface_content_closure_.Reset();

      // Destroy and recreate AcceleratedWidget. This will force the
      // compositor drop the current LayerTreeFrameSink together with the
      // corresponding ScenicSurface. They will be created again only after
      // the window becomes visible again.
      platform_window_delegate_->OnAcceleratedWidgetDestroyed();
      platform_window_delegate_->OnAcceleratedWidgetAvailable(window_id_);

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
  platform_window_delegate_->OnActivationChanged(focus_state.focused());

  view_ref_focused_->Watch(
      fit::bind_member(this, &FlatlandWindow::OnViewRefFocusedWatchResult));
}

void FlatlandWindow::UpdateSize() {
  DCHECK(logical_size_);
  if (pending_attach_surface_content_closure_) {
    std::move(pending_attach_surface_content_closure_).Run();
  }

  const auto old_bounds = bounds_;
  bounds_ = gfx::Rect(
      gfx::ScaleToCeiledSize(logical_size_.value(), device_pixel_ratio_));

  PlatformWindowDelegate::BoundsChange bounds(old_bounds.origin() !=
                                              bounds_.origin());
  bounds.system_ui_overlap = view_inset_;
  platform_window_delegate_->OnBoundsChanged(bounds);
}

void FlatlandWindow::OnViewAttachedChanged(bool is_view_attached) {
  PlatformWindowState old_state = GetPlatformWindowState();
  is_view_attached_ = is_view_attached;
  PlatformWindowState new_state = GetPlatformWindowState();
  if (old_state != new_state) {
    platform_window_delegate_->OnWindowStateChanged(old_state, new_state);
  }
}

void FlatlandWindow::DispatchEvent(ui::Event* event) {
  if (event->IsLocatedEvent()) {
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    gfx::PointF location = located_event->location_f();
    location.Scale(device_pixel_ratio_);
    located_event->set_location_f(location);
  }
  platform_window_delegate_->DispatchEvent(event);
}

void FlatlandWindow::OnFlatlandError(
    fuchsia::ui::composition::FlatlandError error) {
  LOG(ERROR) << "Flatland error: " << static_cast<int>(error);
  platform_window_delegate_->OnClosed();
}

void FlatlandWindow::OnViewControllerDisconnected(zx_status_t status) {
  view_controller_ = nullptr;
  platform_window_delegate_->OnClosed();
}

}  // namespace ui
