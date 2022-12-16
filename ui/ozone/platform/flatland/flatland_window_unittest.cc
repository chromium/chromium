// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_window.h"

#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/pointer/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/ui/scenic/cpp/testing/fake_flatland.h>
#include <lib/ui/scenic/cpp/testing/fake_touch_source.h>
#include <lib/ui/scenic/cpp/testing/fake_view_ref_focused.h>
#include <lib/ui/scenic/cpp/view_creation_tokens.h>

#include <memory>
#include <string>

#include "base/fuchsia/koid.h"
#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/fuchsia/util/pointer_event_utility.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::scenic::FakeGraph;
using ::scenic::FakeTransform;
using ::scenic::FakeTransformPtr;
using ::scenic::FakeView;
using ::scenic::FakeViewport;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Property;
using ::testing::SaveArg;
using ::testing::VariantWith;

namespace ui {

namespace {

Matcher<FakeGraph> IsWindowGraph(
    const fuchsia::ui::composition::ParentViewportWatcherPtr&
        parent_viewport_watcher,
    const fuchsia::ui::views::ViewportCreationToken& viewport_creation_token,
    std::vector<Matcher<FakeTransformPtr>> child_transform_matchers) {
  auto view_token_koid = base::GetRelatedKoid(viewport_creation_token.value);
  auto watcher_koid = base::GetRelatedKoid(parent_viewport_watcher.channel());

  return AllOf(
      Field("root_transform", &FakeGraph::root_transform,
            Pointee(AllOf(Field("translation", &FakeTransform::translation,
                                FakeTransform::kDefaultTranslation),
                          Field("scale", &FakeTransform::scale,
                                FakeTransform::kDefaultScale),
                          Field("opacity", &FakeTransform::opacity,
                                FakeTransform::kDefaultOpacity),
                          Field("children", &FakeTransform::children,
                                ElementsAreArray(child_transform_matchers))))),
      Field("view", &FakeGraph::view,
            Optional(AllOf(
                Field("view_token", &FakeView::view_token, view_token_koid),
                Field("parent_viewport_watcher",
                      &FakeView::parent_viewport_watcher, watcher_koid)))));
}

Matcher<fuchsia::ui::composition::ViewportProperties> IsViewportProperties(
    const fuchsia::math::SizeU& logical_size) {
  return AllOf(
      Property("has_logical_size",
               &fuchsia::ui::composition::ViewportProperties::has_logical_size,
               true),
      Property("logical_size",
               &fuchsia::ui::composition::ViewportProperties::logical_size,
               logical_size));
}

Matcher<FakeTransformPtr> IsViewport(
    const fuchsia::ui::views::ViewCreationToken& view_token,
    const fuchsia::math::SizeU& viewport_logical_size) {
  auto viewport_koid = base::GetRelatedKoid(view_token.value);

  return Pointee(AllOf(
      Field("translation", &FakeTransform::translation,
            FakeTransform::kDefaultTranslation),
      Field("scale", &FakeTransform::scale, FakeTransform::kDefaultScale),
      Field("opacity", &FakeTransform::opacity, FakeTransform::kDefaultOpacity),
      Field("children", &FakeTransform::children, IsEmpty()),
      Field("content", &FakeTransform::content,
            Pointee(VariantWith<FakeViewport>(AllOf(
                Field("viewport_properties", &FakeViewport::viewport_properties,
                      IsViewportProperties(viewport_logical_size)),
                Field("viewport_token", &FakeViewport::viewport_token,
                      viewport_koid)))))));
}

}  // namespace

class FlatlandWindowTest : public ::testing::Test {
 protected:
  FlatlandWindowTest()
      : fake_flatland_publisher_(test_context_.additional_services(),
                                 fake_flatland_.GetFlatlandRequestHandler()) {}
  ~FlatlandWindowTest() override = default;

  FlatlandWindow* CreateFlatlandWindow(
      PlatformWindowDelegate* delegate,
      fuchsia::ui::views::ViewCreationToken view_creation_token) {
    PlatformWindowInitProperties properties;
    properties.view_ref_pair = scenic::ViewRefPair::New();
    properties.view_creation_token = std::move(view_creation_token);
    flatland_window_ = std::make_unique<FlatlandWindow>(
        &window_manager_, delegate, std::move(properties));
    return flatland_window_.get();
  }

  void SetLayoutInfo(uint32_t width,
                     uint32_t height,
                     float dpr,
                     fuchsia::math::Inset inset = {0, 0, 0, 0}) {
    fuchsia::ui::composition::LayoutInfo layout_info;
    layout_info.set_logical_size({width, height});
    layout_info.set_device_pixel_ratio({dpr, dpr});
    layout_info.set_inset(inset);
    flatland_window_->OnGetLayout(std::move(layout_info));
  }

  void SetViewRefFocusedHandle(
      fuchsia::ui::views::ViewRefFocusedHandle view_ref_focused_handle) {
    flatland_window_->view_ref_focused_.Bind(
        std::move(view_ref_focused_handle));
    flatland_window_->view_ref_focused_->Watch(fit::bind_member(
        flatland_window_.get(), &FlatlandWindow::OnViewRefFocusedWatchResult));
  }

  void SetTouchSourceHandle(
      fuchsia::ui::pointer::TouchSourceHandle touch_source_handle) {
    fuchsia::ui::pointer::MouseSourceHandle mouse_source;
    flatland_window_->pointer_handler_ = std::make_unique<PointerEventsHandler>(
        std::move(touch_source_handle), std::move(mouse_source));
    flatland_window_->pointer_handler_->StartWatching(base::BindRepeating(
        &FlatlandWindow::DispatchEvent,
        // This is safe since |flatland_window_| is a class member.
        base::Unretained(flatland_window_.get())));
  }

  bool HasPendingAttachSurfaceContentClosure() {
    return !!flatland_window_->pending_attach_surface_content_closure_;
  }

  const fuchsia::ui::composition::ParentViewportWatcherPtr&
  parent_viewport_watcher() {
    return flatland_window_->parent_viewport_watcher_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  scenic::FakeFlatland fake_flatland_;

 private:
  base::TestComponentContextForProcess test_context_;
  // Injects binding for responding to Flatland protocol connection requests.
  const base::ScopedServicePublisher<fuchsia::ui::composition::Flatland>
      fake_flatland_publisher_;

  FlatlandWindowManager window_manager_;
  std::unique_ptr<FlatlandWindow> flatland_window_;
};

TEST_F(FlatlandWindowTest, Initialization) {
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget window_widget;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&window_widget));

  auto token_pair = scenic::ViewCreationTokenPair::New();
  CreateFlatlandWindow(&delegate, std::move(token_pair.view_token));
  ASSERT_NE(window_widget, gfx::kNullAcceleratedWidget);

  // Check that there are no crashes after flushing tasks.
  task_environment_.RunUntilIdle();
}

TEST_F(FlatlandWindowTest, PresentsOnShow) {
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget window_widget;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&window_widget));
  size_t presents_called = 0u;
  fake_flatland_.SetPresentHandler(
      [&presents_called](auto) { ++presents_called; });

  auto token_pair = scenic::ViewCreationTokenPair::New();
  FlatlandWindow* flatland_window =
      CreateFlatlandWindow(&delegate, std::move(token_pair.view_token));
  ASSERT_NE(window_widget, gfx::kNullAcceleratedWidget);

  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, presents_called);

  flatland_window->Show(/*inactive=*/false);
  EXPECT_EQ(0u, presents_called);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, presents_called);
  EXPECT_THAT(
      fake_flatland_.graph(),
      IsWindowGraph(parent_viewport_watcher(), token_pair.viewport_token, {}));
}

// Tests that FlatlandWindow processes and delegates focus signal.
TEST_F(FlatlandWindowTest, ProcessesFocusedSignal) {
  MockPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_));
  auto token_pair = scenic::ViewCreationTokenPair::New();
  FlatlandWindow* flatland_window =
      CreateFlatlandWindow(&delegate, std::move(token_pair.view_token));
  flatland_window->Show(/*inactive=*/false);
  task_environment_.RunUntilIdle();

  scenic::FakeViewRefFocused fake_view_ref_focused;
  fidl::Binding<fuchsia::ui::views::ViewRefFocused>
      fake_view_ref_focused_binding(&fake_view_ref_focused);
  SetViewRefFocusedHandle(fake_view_ref_focused_binding.NewBinding());

  // FlatlandWindow should start watching.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_view_ref_focused.times_watched(), 1u);

  // Send focused=true signal.
  bool focus_delegated = false;
  EXPECT_CALL(delegate, OnActivationChanged(_))
      .WillRepeatedly(SaveArg<0>(&focus_delegated));
  fake_view_ref_focused.ScheduleCallback(true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_view_ref_focused.times_watched(), 2u);
  EXPECT_TRUE(focus_delegated);

  // Send focused=false signal.
  fake_view_ref_focused.ScheduleCallback(false);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_view_ref_focused.times_watched(), 3u);
  EXPECT_FALSE(focus_delegated);
}

TEST_F(FlatlandWindowTest, AppliesDevicePixelRatio) {
  MockPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_));
  auto token_pair = scenic::ViewCreationTokenPair::New();
  CreateFlatlandWindow(&delegate, std::move(token_pair.view_token));
  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(1);
  SetLayoutInfo(100, 100, 1.f);

  scenic::FakeTouchSource fake_touch_source;
  fidl::Binding<fuchsia::ui::pointer::TouchSource> fake_touch_source_binding(
      &fake_touch_source);
  SetTouchSourceHandle(fake_touch_source_binding.NewBinding());
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
  fake_touch_source.ScheduleCallback(std::move(events));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(event_received);

  // Update device pixel ratio.
  const float kDPR = 2.f;
  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(1);
  SetLayoutInfo(100, 100, kDPR);

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
  fake_touch_source.ScheduleCallback(std::move(events));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(event_received);
}

TEST_F(FlatlandWindowTest, WaitsForNonZeroSizeToAttachSurfaceContent) {
  MockPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_));
  size_t presents_called = 0u;
  fake_flatland_.SetPresentHandler(
      [&presents_called](auto) { ++presents_called; });

  auto token_pair = scenic::ViewCreationTokenPair::New();
  FlatlandWindow* flatland_window =
      CreateFlatlandWindow(&delegate, std::move(token_pair.view_token));

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

  const uint32_t kWidth = 200;
  const uint32_t kHeight = 100;
  const fuchsia::math::SizeU expected_size = {kWidth, kHeight};
  SetLayoutInfo(kWidth, kHeight, 1.f);
  EXPECT_FALSE(HasPendingAttachSurfaceContentClosure());

  // There should be a present call in FakeFlatland after flushing the tasks.
  EXPECT_EQ(0u, presents_called);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, presents_called);

  // Show to attach the scene graph.
  flatland_window->Show(/*inactive=*/false);
  fuchsia::ui::composition::OnNextFrameBeginValues on_next_frame_begin_values;
  on_next_frame_begin_values.set_additional_present_credits(1);
  fake_flatland_.FireOnNextFrameBeginEvent(
      std::move(on_next_frame_begin_values));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2u, presents_called);
  EXPECT_THAT(
      fake_flatland_.graph(),
      IsWindowGraph(parent_viewport_watcher(), token_pair.viewport_token,
                    {IsViewport(child_token, expected_size)}));
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
  auto token_pair = scenic::ViewCreationTokenPair::New();
  CreateFlatlandWindow(&delegate, std::move(token_pair.view_token));
  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(1);
  SetLayoutInfo(100, 100, 1.f);

  const fuchsia::math::Inset inset = {1, 1, 1, 1};
  const float dpr = GetParam();

  // Setting LayoutInfo should trigger a change in the bounds.
  PlatformWindowDelegate::BoundsChange bounds(false);
  EXPECT_CALL(delegate, OnBoundsChanged(_)).WillOnce(SaveArg<0>(&bounds));
  SetLayoutInfo(100, 100, dpr, inset);

  EXPECT_EQ(bounds.system_ui_overlap.top(), dpr * inset.top);
  EXPECT_EQ(bounds.system_ui_overlap.left(), dpr * inset.left);
  EXPECT_EQ(bounds.system_ui_overlap.bottom(), dpr * inset.bottom);
  EXPECT_EQ(bounds.system_ui_overlap.right(), dpr * inset.right);
}

}  // namespace ui
