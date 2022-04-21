// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/tests/fake_flatland.h"

#include "base/check.h"
#include "base/logging.h"

namespace ui {

FakeFlatland::FakeFlatland() : binding_(this) {}

FakeFlatland::~FakeFlatland() = default;

fidl::InterfaceHandle<fuchsia::ui::composition::Flatland> FakeFlatland::Connect(
    async_dispatcher_t* dispatcher) {
  CHECK(!binding_.is_bound());

  fidl::InterfaceHandle<fuchsia::ui::composition::Flatland> flatland;
  binding_.Bind(flatland.NewRequest(), dispatcher);

  return flatland;
}

fidl::InterfaceRequestHandler<fuchsia::ui::composition::Flatland>
FakeFlatland::GetRequestHandler(async_dispatcher_t* dispatcher) {
  return
      [this, dispatcher](
          fidl::InterfaceRequest<fuchsia::ui::composition::Flatland> request) {
        CHECK(!binding_.is_bound());
        binding_.Bind(std::move(request), dispatcher);
      };
}

void FakeFlatland::Disconnect(fuchsia::ui::composition::FlatlandError error) {
  binding_.events().OnError(std::move(error));
  binding_.Unbind();
}

void FakeFlatland::SetPresentHandler(PresentHandler present_handler) {
  present_handler_ = std::move(present_handler);
}

void FakeFlatland::FireOnNextFrameBeginEvent(
    fuchsia::ui::composition::OnNextFrameBeginValues
        on_next_frame_begin_values) {
  binding_.events().OnNextFrameBegin(std::move(on_next_frame_begin_values));
}

void FakeFlatland::FireOnFramePresentedEvent(
    fuchsia::scenic::scheduling::FramePresentedInfo frame_presented_info) {
  binding_.events().OnFramePresented(std::move(frame_presented_info));
}

void FakeFlatland::SetViewRefFocusedRequestHandler(
    ViewRefFocusedRequestHandler handler) {
  view_ref_focused_handler_ = std::move(handler);
}

void FakeFlatland::SetTouchSourceRequestHandler(
    TouchSourceRequestHandler handler) {
  touch_source_request_handler_ = std::move(handler);
}

void FakeFlatland::NotImplemented_(const std::string& name) {
  LOG(ERROR) << "FakeFlatland does not implement " << name;
}

void FakeFlatland::Present(fuchsia::ui::composition::PresentArgs args) {
  // TODO(crbug.com/1307545): ApplyCommands()
  present_handler_.Run(std::move(args));
}

void FakeFlatland::CreateView2(
    fuchsia::ui::views::ViewCreationToken token,
    fuchsia::ui::views::ViewIdentityOnCreation view_identity,
    fuchsia::ui::composition::ViewBoundProtocols view_protocols,
    fidl::InterfaceRequest<fuchsia::ui::composition::ParentViewportWatcher>
        parent_viewport_watcher) {
  if (view_ref_focused_handler_ && view_protocols.has_view_ref_focused()) {
    view_ref_focused_handler_(
        std::move(*view_protocols.mutable_view_ref_focused()));
  }
  if (touch_source_request_handler_ && view_protocols.has_touch_source()) {
    touch_source_request_handler_(
        std::move(*view_protocols.mutable_touch_source()));
  }
}

void FakeFlatland::SetDebugName(std::string debug_name) {
  debug_name_ = std::move(debug_name);
}

}  // namespace ui
