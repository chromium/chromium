// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_TESTS_FAKE_FLATLAND_H_
#define UI_OZONE_PLATFORM_FLATLAND_TESTS_FAKE_FLATLAND_H_

#include <fuchsia/scenic/scheduling/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl_test_base.h>
#include <fuchsia/ui/pointer/cpp/fidl.h>
#include <lib/async/dispatcher.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/fidl/cpp/interface_request.h>
#include "third_party/abseil-cpp/absl/types/optional.h"

#include <string>

#include "base/callback.h"

namespace ui {

class FakeParentViewportWatcher
    : public fuchsia::ui::composition::testing::ParentViewportWatcher_TestBase {
 public:
  explicit FakeParentViewportWatcher(
      fidl::InterfaceRequest<fuchsia::ui::composition::ParentViewportWatcher>
          request);
  ~FakeParentViewportWatcher() override;

 private:
  // |fuchsia::ui::composition::testing::Flatland_TestBase|
  void NotImplemented_(const std::string& name) override;

  fidl::Binding<fuchsia::ui::composition::ParentViewportWatcher> binding_;
};

// A lightweight fake implementation of the Flatland API.
//
// The fake has no side effects besides mutating its own internal state
// according to the rules of interacting with the Flatland API.  It makes that
// internal state available for inspection by a test.
class FakeFlatland
    : public fuchsia::ui::composition::testing::Allocator_TestBase,
      public fuchsia::ui::composition::testing::Flatland_TestBase {
 public:
  using PresentHandler =
      base::RepeatingCallback<void(fuchsia::ui::composition::PresentArgs)>;
  using ViewRefFocusedRequestHandler =
      fidl::InterfaceRequestHandler<fuchsia::ui::views::ViewRefFocused>;
  using TouchSourceRequestHandler =
      fidl::InterfaceRequestHandler<fuchsia::ui::pointer::TouchSource>;

  FakeFlatland();
  ~FakeFlatland() override;

  bool is_allocator_connected() const { return allocator_binding_.is_bound(); }

  bool is_flatland_connected() const { return flatland_binding_.is_bound(); }

  const std::string& debug_name() const { return debug_name_; }

  // Bind this instance's Allocator FIDL channel to the `dispatcher` and allow
  // processing of incoming FIDL requests.
  //
  // This can only be called once.
  fuchsia::ui::composition::FlatlandHandle ConnectFlatland(
      async_dispatcher_t* dispatcher = nullptr);

  // Returns a request handler that binds the incoming FIDL requests to this
  // session's FIDL channels on the `dispatcher`.
  //
  // This can only be called once.
  fidl::InterfaceRequestHandler<fuchsia::ui::composition::Flatland>
  GetFlatlandRequestHandler(async_dispatcher_t* dispatcher = nullptr);
  fidl::InterfaceRequestHandler<fuchsia::ui::composition::Allocator>
  GetAllocatorRequestHandler(async_dispatcher_t* dispatcher = nullptr);

  // Disconnect the session's FIDL channels with an error.
  void Disconnect(fuchsia::ui::composition::FlatlandError error);

  // Set a handler for `Present`-related FIDL calls' return values.
  void SetPresentHandler(PresentHandler present_handler);

  // Fire an `OnNextFrameBegin` event.  Call this first after a `Present` in
  // order to give additional present tokens to the client and simulate scenic's
  // normal event flow.
  void FireOnNextFrameBeginEvent(
      fuchsia::ui::composition::OnNextFrameBeginValues
          on_next_frame_begin_values);

  // Fire an `OnFramePresented` event.  Call this second after a `Present` in
  // order to inform the client of returned frames and simulate scenic's normal
  // event flow.
  void FireOnFramePresentedEvent(
      fuchsia::scenic::scheduling::FramePresentedInfo frame_presented_info);

  void SetViewRefFocusedRequestHandler(ViewRefFocusedRequestHandler handler);
  void SetTouchSourceRequestHandler(TouchSourceRequestHandler handler);

 private:
  // |fuchsia::ui::composition::testing::Flatland_TestBase|
  void NotImplemented_(const std::string& name) override;

  // |fuchsia::ui::composition::Flatland|
  void Present(fuchsia::ui::composition::PresentArgs args) override;

  // |fuchsia::ui::composition::Flatland|
  void CreateView2(
      fuchsia::ui::views::ViewCreationToken token,
      fuchsia::ui::views::ViewIdentityOnCreation view_identity,
      fuchsia::ui::composition::ViewBoundProtocols view_protocols,
      fidl::InterfaceRequest<fuchsia::ui::composition::ParentViewportWatcher>
          parent_viewport_watcher) override;

  // |fuchsia::ui::composition::Flatland|
  void SetDebugName(std::string debug_name) override;

  fidl::Binding<fuchsia::ui::composition::Allocator> allocator_binding_;
  fidl::Binding<fuchsia::ui::composition::Flatland> flatland_binding_;

  std::string debug_name_;

  PresentHandler present_handler_;
  ViewRefFocusedRequestHandler view_ref_focused_handler_;
  TouchSourceRequestHandler touch_source_request_handler_;
  absl::optional<FakeParentViewportWatcher> parent_viewport_watcher_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_TESTS_FAKE_FLATLAND_H_
