// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_window.h"

#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/pointer/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>

#include <memory>
#include <string>

#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/fuchsia/fakes/fake_touch_source.h"
#include "ui/events/fuchsia/fakes/pointer_event_utility.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"
#include "ui/ozone/platform/flatland/tests/fake_flatland.h"
#include "ui/ozone/platform/flatland/tests/fake_view_ref_focused.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::_;
using ::testing::SaveArg;

namespace ui {

class FlatlandWindowTest : public ::testing::Test {
 protected:
  FlatlandWindowTest()
      : fake_flatland_publisher_(test_context_.additional_services(),
                                 fake_flatland_.GetFlatlandRequestHandler()),
        fake_view_ref_focused_binding_(&fake_view_ref_focused_),
        fake_touch_source_binding_(&fake_touch_source_) {
    fake_flatland_.SetViewRefFocusedRequestHandler(
        [this](fidl::InterfaceRequest<fuchsia::ui::views::ViewRefFocused>
                   request) {
          CHECK(!fake_view_ref_focused_binding_.is_bound());
          fake_view_ref_focused_binding_.Bind(std::move(request));
        });
    fake_flatland_.SetTouchSourceRequestHandler(
        [this](
            fidl::InterfaceRequest<fuchsia::ui::pointer::TouchSource> request) {
          CHECK(!fake_touch_source_binding_.is_bound());
          fake_touch_source_binding_.Bind(std::move(request));
        });
  }
  ~FlatlandWindowTest() override = default;

  FlatlandWindow* CreateFlatlandWindow(PlatformWindowDelegate* delegate) {
    PlatformWindowInitProperties properties;
    properties.view_ref_pair = scenic::ViewRefPair::New();
    fuchsia::ui::views::ViewportCreationToken parent_token;
    fuchsia::ui::views::ViewCreationToken child_token;
    zx::channel::create(0, &parent_token.value, &child_token.value);
    properties.view_creation_token = std::move(child_token);
    flatland_window_ = std::make_unique<FlatlandWindow>(
        &window_manager_, delegate, std::move(properties));
    return flatland_window_.get();
  }

  void SetLayoutInfo(float device_pixel_ratio,
                     fuchsia::math::Inset inset = {0, 0, 0, 0}) {
    fuchsia::ui::composition::LayoutInfo layout_info;
    layout_info.set_logical_size({100, 100});
    layout_info.set_device_pixel_ratio(
        {device_pixel_ratio, device_pixel_ratio});
    layout_info.set_inset(inset);
    flatland_window_->OnGetLayout(std::move(layout_info));
  }

  bool HasPendingAttachSurfaceContentClosure() {
    return !!flatland_window_->pending_attach_surface_content_closure_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  FakeFlatland fake_flatland_;
  FakeViewRefFocused fake_view_ref_focused_;
  FakeTouchSource fake_touch_source_;

 private:
  base::TestComponentContextForProcess test_context_;
  // Injects binding for responding to Flatland protocol connection requests.
  const base::ScopedServicePublisher<fuchsia::ui::composition::Flatland>
      fake_flatland_publisher_;
  // Injects binding for responding to ViewRefFocused protocol connection
  // requests coming to |fake_flatland_|.
  fidl::Binding<fuchsia::ui::views::ViewRefFocused>
      fake_view_ref_focused_binding_;
  // Injects binding for responding to TouchSource protocol connection
  // requests coming to |fake_flatland_|.
  fidl::Binding<fuchsia::ui::pointer::TouchSource> fake_touch_source_binding_;

  FlatlandWindowManager window_manager_;
  std::unique_ptr<FlatlandWindow> flatland_window_;
};

TEST_F(FlatlandWindowTest, Initialization) {
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget window_widget;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&window_widget));

  CreateFlatlandWindow(&delegate);
  ASSERT_NE(window_widget, gfx::kNullAcceleratedWidget);

  // Check that there are no crashes after flushing tasks.
  task_environment_.RunUntilIdle();
}

// Tests that FlatlandWindow processes and delegates focus signal.
TEST_F(FlatlandWindowTest, ProcessesFocusedSignal) {
  MockPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_));
  CreateFlatlandWindow(&delegate);

  // FlatlandWindow should start watching in ctor.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_view_ref_focused_.times_watched(), 1u);

  // Send focused=true signal.
  bool focus_delegated = false;
  EXPECT_CALL(delegate, OnActivationChanged(_))
      .WillRepeatedly(SaveArg<0>(&focus_delegated));
  fake_view_ref_focused_.ScheduleCallback(true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_view_ref_focused_.times_watched(), 2u);
  EXPECT_TRUE(focus_delegated);

  // Send focused=false signal.
  fake_view_ref_focused_.ScheduleCallback(false);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_view_ref_focused_.times_watched(), 3u);
  EXPECT_FALSE(focus_delegated);
}

TEST_F(FlatlandWindowTest, AppliesDevicePixelRatio) {
  MockPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_));
  CreateFlatlandWindow(&delegate);
  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(1);
  SetLayoutInfo(1.f);

  // FlatlandWindow should start watching touch events in ctor.
  task_environment_.RunUntilIdle();

  // Send a touch event and expect coordinates to be the same as TouchEvent.
  const float kLocationX = 9.f;
  const float kLocationY = 10.f;
  bool event_received = false;
  EXPECT_CALL(delegate, DispatchEvent(_))
      .WillOnce([&event_received, kLocationX, kLocationY](ui::Event* event) {
        EXPECT_EQ(event->AsTouchEvent()->location_f().x(), kLocationX);
        EXPECT_EQ(event->AsTouchEvent()->location_f().y(), kLocationY);
        event_received = true;
      });
  std::vector<fuchsia::ui::pointer::TouchEvent> events;
  events.push_back(
      TouchEventBuilder()
          .SetPosition({kLocationX, kLocationY})
          .SetTouchInteractionStatus(
              fuchsia::ui::pointer::TouchInteractionStatus::GRANTED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(event_received);

  // Update device pixel ratio.
  const float kDPR = 2.f;
  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(1);
  SetLayoutInfo(kDPR);

  // Send the same touch event and expect coordinates to be scaled from
  // TouchEvent.
  event_received = false;
  EXPECT_CALL(delegate, DispatchEvent(_))
      .WillOnce([&event_received, kLocationX, kLocationY,
                 kDPR](ui::Event* event) {
        EXPECT_EQ(event->AsTouchEvent()->location_f().x(), kLocationX * kDPR);
        EXPECT_EQ(event->AsTouchEvent()->location_f().y(), kLocationY * kDPR);
        event_received = true;
      });
  events.clear();
  events.push_back(
      TouchEventBuilder()
          .SetPosition({kLocationX, kLocationY})
          .SetTouchInteractionStatus(
              fuchsia::ui::pointer::TouchInteractionStatus::GRANTED)
          .Build());
  fake_touch_source_.ScheduleCallback(std::move(events));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(event_received);
}

TEST_F(FlatlandWindowTest, WaitForNonZeroSize) {
  MockPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_));
  FlatlandWindow* flatland_window = CreateFlatlandWindow(&delegate);

  // FlatlandWindow should start watching callbacks in ctor.
  task_environment_.RunUntilIdle();

  // Create a ViewportCreationToken.
  fuchsia::ui::views::ViewportCreationToken parent_token;
  fuchsia::ui::views::ViewCreationToken child_token;
  auto status = zx::channel::create(0, &parent_token.value, &child_token.value);
  ASSERT_EQ(ZX_OK, status);

  // Try attaching the content. It should only be a closure.
  flatland_window->AttachSurfaceContent(std::move(parent_token));
  EXPECT_TRUE(HasPendingAttachSurfaceContentClosure());

  // Setting layout info should trigger the closure and delegate calls.
  EXPECT_CALL(delegate, OnWindowStateChanged(_, _)).Times(1);
  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(1);
  SetLayoutInfo(1.f);
  EXPECT_FALSE(HasPendingAttachSurfaceContentClosure());
}

class ParameterizedViewInsetTest : public FlatlandWindowTest,
                                   public testing::WithParamInterface<float> {};

INSTANTIATE_TEST_SUITE_P(ViewInsetTest,
                         ParameterizedViewInsetTest,
                         testing::Values(1.f, 2.f, 3.f));

// Tests whether view insets are properly set in |FlatlandWindow|.
TEST_P(ParameterizedViewInsetTest, ViewInsetsTest) {
  MockPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_));
  CreateFlatlandWindow(&delegate);
  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(1);
  SetLayoutInfo(1.f);

  const fuchsia::math::Inset inset = {1, 1, 1, 1};
  const float dpr = GetParam();

  // Setting LayoutInfo should trigger a change in the bounds.
  PlatformWindowDelegate::BoundsChange bounds(false);
  EXPECT_CALL(delegate, OnBoundsChanged(_)).WillOnce(SaveArg<0>(&bounds));
  SetLayoutInfo(dpr, inset);

  EXPECT_EQ(bounds.system_ui_overlap.top(), dpr * inset.top);
  EXPECT_EQ(bounds.system_ui_overlap.left(), dpr * inset.left);
  EXPECT_EQ(bounds.system_ui_overlap.bottom(), dpr * inset.bottom);
  EXPECT_EQ(bounds.system_ui_overlap.right(), dpr * inset.right);
}

}  // namespace ui
