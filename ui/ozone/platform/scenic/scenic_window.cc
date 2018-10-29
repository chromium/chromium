// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

ScenicWindow::ScenicWindow(
    ScenicWindowManager* window_manager,
    PlatformWindowDelegate* delegate,
    fidl::InterfaceRequest<fuchsia::ui::viewsv1token::ViewOwner>
        view_owner_request)
    : manager_(window_manager),
      delegate_(delegate),
      window_id_(manager_->AddWindow(this)),
      event_dispatcher_(this),
      view_listener_binding_(this),
      scenic_session_(manager_->GetScenic(), this),
      input_listener_binding_(this) {
  // Create event pair to import parent view node to Scenic. One end is passed
  // directly to Scenic in ImportResource command and the second one is passed
  // to ViewManager::CreateView(). ViewManager will passes it to Scenic when the
  // view is added to a container.
  zx::eventpair parent_export_token;
  zx::eventpair parent_import_token;
  zx_status_t status =
      zx::eventpair::create(0u, &parent_import_token, &parent_export_token);
  ZX_CHECK(status == ZX_OK, status) << "zx_eventpair_create()";

  // Create a new node and add it as a child to the parent.
  parent_node_id_ = scenic_session_.ImportResource(
      fuchsia::ui::gfx::ImportSpec::NODE, std::move(parent_import_token));
  node_id_ = scenic_session_.CreateEntityNode();
  scenic_session_.AddNodeChild(parent_node_id_, node_id_);

  // Subscribe to metrics events from the parent node. These events are used to
  // get the device pixel ratio for the screen.
  scenic_session_.SetEventMask(parent_node_id_,
                               fuchsia::ui::gfx::kMetricsEventMask);

  // Create the view.
  manager_->GetViewManager()->CreateView(
      view_.NewRequest(), std::move(view_owner_request),
      view_listener_binding_.NewBinding(), std::move(parent_export_token),
      "Chromium");
  view_.set_error_handler(fit::bind_member(this, &ScenicWindow::OnViewError));
  view_listener_binding_.set_error_handler(
      fit::bind_member(this, &ScenicWindow::OnViewError));

  // Setup input event listener.
  // TODO(crbug.com/881591): Migrate off InputConnection and use IMEService
  // for receiving keyboard input instead.
  fuchsia::sys::ServiceProviderPtr view_service_provider;
  view_->GetServiceProvider(view_service_provider.NewRequest());
  view_service_provider->ConnectToService(
      fuchsia::ui::input::InputConnection::Name_,
      input_connection_.NewRequest().TakeChannel());
  input_connection_->SetEventListener(input_listener_binding_.NewBinding());

  // Add shape node for window.
  shape_id_ = scenic_session_.CreateShapeNode();
  scenic_session_.AddNodeChild(node_id_, shape_id_);
  material_id_ = scenic_session_.CreateMaterial();
  scenic_session_.SetNodeMaterial(shape_id_, material_id_);

  // Call Present() to ensure that the scenic session commands are processed,
  // which is necessary to receive metrics event from Scenic.
  scenic_session_.Present();

  delegate_->OnAcceleratedWidgetAvailable(window_id_);
}

ScenicWindow::~ScenicWindow() {
  scenic_session_.ReleaseResource(node_id_);
  scenic_session_.ReleaseResource(parent_node_id_);
  scenic_session_.ReleaseResource(shape_id_);
  scenic_session_.ReleaseResource(material_id_);

  manager_->RemoveWindow(window_id_, this);
  view_.Unbind();
}

void ScenicWindow::SetTexture(ScenicSession::ResourceId texture) {
  scenic_session_.SetMaterialTexture(material_id_, texture);
}

gfx::Rect ScenicWindow::GetBounds() {
  return gfx::Rect(size_pixels_);
}

void ScenicWindow::SetBounds(const gfx::Rect& bounds) {
  // View dimensions are controlled by the containing view, it's not possible to
  // set them here.
}

void ScenicWindow::SetTitle(const base::string16& title) {
  NOTIMPLEMENTED();
}

void ScenicWindow::Show() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Hide() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Close() {
  NOTIMPLEMENTED();
}

void ScenicWindow::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

void ScenicWindow::SetCapture() {
  NOTIMPLEMENTED();
}

void ScenicWindow::ReleaseCapture() {
  NOTIMPLEMENTED();
}

bool ScenicWindow::HasCapture() const {
  NOTIMPLEMENTED();
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
  return PLATFORM_WINDOW_STATE_NORMAL;
}

void ScenicWindow::SetCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void ScenicWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void ScenicWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

PlatformImeController* ScenicWindow::GetPlatformImeController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void ScenicWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Rect ScenicWindow::GetRestoredBoundsInPixels() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void ScenicWindow::UpdateSize() {
  gfx::SizeF scaled = ScaleSize(size_dips_, device_pixel_ratio_);
  size_pixels_ = gfx::Size(ceilf(scaled.width()), ceilf(scaled.height()));
  gfx::Rect size_rect(size_pixels_);

  // Update this window's Screen's dimensions to match the new size.
  ScenicScreen* screen = manager_->screen();
  if (screen)
    screen->OnWindowBoundsChanged(window_id_, size_rect);

  // Translate the node by half of the view dimensions to put it in the center
  // of the view.
  const float translation[] = {size_dips_.width() / 2.0,
                               size_dips_.height() / 2.0, 0.f};

  // Set node shape to rectangle that matches size of the view.
  ScenicSession::ResourceId rect_id =
      scenic_session_.CreateRectangle(size_dips_.width(), size_dips_.height());
  scenic_session_.SetNodeShape(shape_id_, rect_id);
  scenic_session_.SetNodeTranslation(shape_id_, translation);
  scenic_session_.ReleaseResource(rect_id);
  scenic_session_.Present();

  delegate_->OnBoundsChanged(size_rect);
}

void ScenicWindow::OnPropertiesChanged(
    fuchsia::ui::viewsv1::ViewProperties properties,
    OnPropertiesChangedCallback callback) {
  if (properties.view_layout) {
    size_dips_.SetSize(properties.view_layout->size.width,
                       properties.view_layout->size.height);
    if (device_pixel_ratio_ > 0.0)
      UpdateSize();
  }

  callback();
}

void ScenicWindow::OnScenicError(const std::string& error) {
  LOG(ERROR) << "ScenicSession failed: " << error;
  delegate_->OnClosed();
}

void ScenicWindow::OnScenicEvents(
    const std::vector<fuchsia::ui::scenic::Event>& events) {
  for (const auto& event : events) {
    if (!event.is_gfx() || !event.gfx().is_metrics())
      continue;

    auto& metrics = event.gfx().metrics();
    if (metrics.node_id != parent_node_id_)
      continue;

    device_pixel_ratio_ =
        std::max(metrics.metrics.scale_x, metrics.metrics.scale_y);

    ScenicScreen* screen = manager_->screen();
    if (screen)
      screen->OnWindowMetrics(window_id_, device_pixel_ratio_);

    if (!size_dips_.IsEmpty())
      UpdateSize();
  }
}

void ScenicWindow::OnEvent(fuchsia::ui::input::InputEvent event,
                           OnEventCallback callback) {
  bool result = false;

  if (event.is_focus()) {
    delegate_->OnActivationChanged(event.focus().focused);
    result = true;
  } else {
    result = event_dispatcher_.ProcessEvent(event);
  }

  callback(result);
}

void ScenicWindow::OnViewError() {
  VLOG(1) << "viewsv1::View connection was closed.";
  delegate_->OnClosed();
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

}  // namespace ui
