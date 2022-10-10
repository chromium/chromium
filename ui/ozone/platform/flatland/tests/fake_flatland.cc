// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/tests/fake_flatland.h"

#include "base/check.h"
#include "base/logging.h"

namespace ui {

FakeFlatland::FakeFlatland()
    : allocator_binding_(this), flatland_binding_(this) {}

FakeFlatland::~FakeFlatland() = default;

fuchsia::ui::composition::FlatlandHandle FakeFlatland::ConnectFlatland(
    async_dispatcher_t* dispatcher) {
  CHECK(!flatland_binding_.is_bound());

  fuchsia::ui::composition::FlatlandHandle flatland;
  flatland_binding_.Bind(flatland.NewRequest(), dispatcher);

  return flatland;
}

fidl::InterfaceRequestHandler<fuchsia::ui::composition::Flatland>
FakeFlatland::GetFlatlandRequestHandler(async_dispatcher_t* dispatcher) {
  return
      [this, dispatcher](
          fidl::InterfaceRequest<fuchsia::ui::composition::Flatland> request) {
        CHECK(!flatland_binding_.is_bound());
        flatland_binding_.Bind(std::move(request), dispatcher);
      };
}

fidl::InterfaceRequestHandler<fuchsia::ui::composition::Allocator>
FakeFlatland::GetAllocatorRequestHandler(async_dispatcher_t* dispatcher) {
  return
      [this, dispatcher](
          fidl::InterfaceRequest<fuchsia::ui::composition::Allocator> request) {
        CHECK(!allocator_binding_.is_bound());
        allocator_binding_.Bind(std::move(request), dispatcher);
      };
}

void FakeFlatland::Disconnect(fuchsia::ui::composition::FlatlandError error) {
  flatland_binding_.events().OnError(std::move(error));
  flatland_binding_.Unbind();
}

void FakeFlatland::SetPresentHandler(PresentHandler present_handler) {
  present_handler_ = std::move(present_handler);
}

void FakeFlatland::FireOnNextFrameBeginEvent(
    fuchsia::ui::composition::OnNextFrameBeginValues
        on_next_frame_begin_values) {
  flatland_binding_.events().OnNextFrameBegin(
      std::move(on_next_frame_begin_values));
}

void FakeFlatland::FireOnFramePresentedEvent(
    fuchsia::scenic::scheduling::FramePresentedInfo frame_presented_info) {
  flatland_binding_.events().OnFramePresented(std::move(frame_presented_info));
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
  DCHECK(present_handler_);
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
  parent_viewport_watcher_.emplace(std::move(parent_viewport_watcher));
}

void FakeFlatland::SetDebugName(std::string debug_name) {
  debug_name_ = std::move(debug_name);
}

FakeParentViewportWatcher::FakeParentViewportWatcher(
    fidl::InterfaceRequest<fuchsia::ui::composition::ParentViewportWatcher>
        request)
    : binding_(this, std::move(request)) {}

FakeParentViewportWatcher::~FakeParentViewportWatcher() = default;

void FakeParentViewportWatcher::NotImplemented_(const std::string& name) {
  LOG(ERROR) << "FakeParentViewportWatcher does not implement " << name;
}

}  // namespace ui
