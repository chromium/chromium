// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_window.h"

#include <fidl/fuchsia.ui.pointer/cpp/fidl.h>
#include <fidl/fuchsia.ui.pointer/cpp/hlcpp_conversion.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/ui/scenic/cpp/testing/fake_flatland.h>
#include <lib/ui/scenic/cpp/testing/fake_touch_source.h>
#include <lib/ui/scenic/cpp/testing/fake_view_ref_focused.h>
#include <lib/zx/channel.h>

#include <memory>
#include <string>

#include "base/fuchsia/koid.h"
#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/logging.h"
#include "base/test/fidl_matchers.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/fuchsia/util/pointer_event_utility.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/fuchsia/view_ref_pair.h"

using ::scenic::FakeGraph;
using ::scenic::FakeTransform;
using ::scenic::FakeTransformPtr;
using ::scenic::FakeView;
using ::scenic::FakeViewport;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
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
    Matcher<std::vector<FakeTransformPtr>> children_transform_matcher) {
  std::optional<zx_koid_t> view_token_koid =
      base::GetRelatedKoid(viewport_creation_token.value);
  EXPECT_TRUE(view_token_koid.has_value());
  std::optional<zx_koid_t> watcher_koid =
      base::GetRelatedKoid(parent_viewport_watcher.channel());
  EXPECT_TRUE(watcher_koid.has_value());

  return AllOf(
      Field("root_transform", &FakeGraph::root_transform,
            Pointee(AllOf(
                Field("translation", &FakeTransform::translation,
                      ::base::test::FidlEq(FakeTransform::kDefaultTranslation)),
                Field("scale", &FakeTransform::scale,
                      ::base::test::FidlEq(FakeTransform::kDefaultScale)),
                Field("opacity", &FakeTransform::opacity,
                      FakeTransform::kDefaultOpacity),
                Field("children", &FakeTransform::children,
                      children_transform_matcher)))),
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
               ::base::test::FidlEq(logical_size)));
}

Matcher<FakeTransformPtr> IsViewport(
    const fuchsia::ui::views::ViewCreationToken& view_token,
    const fuchsia::math::SizeU& viewport_logical_size) {
  auto viewport_koid = base::GetRelatedKoid(view_token.value);

  return Pointee(AllOf(
      Field("translation", &FakeTransform::translation,
            ::base::test::FidlEq(FakeTransform::kDefaultTranslation)),
      Field("scale", &FakeTransform::scale,
            ::base::test::FidlEq(FakeTransform::kDefaultScale)),
      Field("opacity", &FakeTransform::opacity, FakeTransform::kDefaultOpacity),
      Field("children", &FakeTransform::children, IsEmpty()),
      Field("content", &FakeTransform::content,
            Pointee(VariantWith<FakeViewport>(AllOf(
                Field("viewport_properties", &FakeViewport::viewport_properties,
                      IsViewportProperties(viewport_logical_size)),
                Field("viewport_token", &FakeViewport::viewport_token,
                      viewport_koid)))))));
}

Matcher<FakeTransformPtr> IsHitShield() {
  return Pointee(AllOf(
      // Must not clip the hit region.
      Field("clip_bounds", &FakeTransform::clip_bounds,
            testing::Eq(std::nullopt)),
      // Hit region must be "infinite".
      Field("hit_regions", &FakeTransform::hit_regions,
            testing::Contains(
                ::base::test::FidlEq(scenic::kInfiniteHitRegion)))));
}

}  // namespace

class FlatlandWindowTest : public ::testing::Test {
 protected:
  FlatlandWindowTest()
      : fake_flatland_publisher_(test_context_.additional_services(),
                                 fake_flatland_.GetFlatlandRequestHandler()) {}
  ~FlatlandWindowTest() override = default;

  void CreateWindow() {
    EXPECT_FALSE(flatland_window_);

    fuchsia::ui::views::ViewCreationToken view_token;
    fuchsia::ui::views::ViewportCreationToken viewport_token;
    auto status =
        zx::channel::create(0, &viewport_token.value, &view_token.value);
    CHECK_EQ(ZX_OK, status);
    viewport_token_ = std::move(viewport_token);

    EXPECT_CALL(window_delegate_, OnAcceleratedWidgetAvailable(_))
        .WillOnce(SaveArg<0>(&window_widget_));

    PlatformWindowInitProperties properties;
    properties.view_ref_pair = ViewRefPair::New();
    properties.view_creation_token = std::move(view_token);
    flatland_window_ = std::make_unique<FlatlandWindow>(
        &window_manager_, &window_delegate_, std::move(properties));
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

  void SetWindowStatus(fuchsia::ui::composition::ParentViewportStatus status) {
    flatland_window_->OnGetStatus(status);
  }

  void SetViewRefFocusedHandle(
      fuchsia::ui::views::ViewRefFocusedHandle view_ref_focused_handle) {
    flatland_window_->view_ref_focused_.Bind(
        std::move(view_ref_focused_handle));
    flatland_window_->view_ref_focused_->Watch(fit::bind_member(
        flatland_window_.get(), &FlatlandWindow::OnViewRefFocusedWatchResult));
  }

  void SetTouchSource(
      fidl::ClientEnd<fuchsia_ui_pointer::TouchSource> touch_source) {
    auto mouse_endpoints =
        fidl::CreateEndpoints<fuchsia_ui_pointer::MouseSource>();
    EXPECT_TRUE(mouse_endpoints.is_ok()) << mouse_endpoints.status_string();
    flatland_window_->pointer_handler_ = std::make_unique<PointerEventsHandler>(
        std::move(touch_source), std::move(mouse_endpoints->client));
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

  base::TestComponentContextForProcess test_context_;
  // Injects binding for responding to Flatland protocol connection requests.
  const base::ScopedServicePublisher<fuchsia::ui::composition::Flatland>
      fake_flatland_publisher_;

  FlatlandWindowManager window_manager_;

  MockPlatformWindowDelegate window_delegate_;
  std::unique_ptr<FlatlandWindow> flatland_window_;

  gfx::AcceleratedWidget window_widget_ = gfx::kNullAcceleratedWidget;

  fuchsia::ui::views::ViewportCreationToken viewport_token_;
};

TEST_F(FlatlandWindowTest, Initialization) {
  CreateWindow();
  ASSERT_NE(window_widget_, gfx::kNullAcceleratedWidget);

  // Check that there are no crashes after flushing tasks.
  task_environment_.RunUntilIdle();
}

TEST_F(FlatlandWindowTest, PresentsOnShow) {
  size_t presents_called = 0u;
  fake_flatland_.SetPresentHandler(
      [&presents_called](auto) { ++presents_called; });

  CreateWindow();
  ASSERT_NE(window_widget_, gfx::kNullAcceleratedWidget);

  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, presents_called);

  flatland_window_->Show(/*inactive=*/false);
  EXPECT_EQ(0u, presents_called);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, presents_called);
  EXPECT_THAT(
      fake_flatland_.graph(),
      IsWindowGraph(parent_viewport_watcher(), viewport_token_, IsEmpty()));
}

// Tests that FlatlandWindow processes and delegates focus signal.
TEST_F(FlatlandWindowTest, ProcessesFocusedSignal) {
  CreateWindow();
  flatland_window_->Show(/*inactive=*/false);
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
  EXPECT_CALL(window_delegate_, OnActivationChanged(_))
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
  CreateWindow();
  EXPECT_CALL(window_delegate_, OnBoundsChanged(_)).Times(1);
  SetLayoutInfo(100, 100, 1.f);

  scenic::FakeTouchSource fake_touch_source;
  fidl::Binding<fuchsia::ui::pointer::TouchSource> fake_touch_source_binding(
      &fake_touch_source);
  SetTouchSource(fidl::HLCPPToNatural(fake_touch_source_binding.NewBinding()));
  task_environment_.RunUntilIdle();

  // Send a touch event and expect coordinates to be the same as TouchEvent.
  const float kLocationX = 9.f;
  const float kLocationY = 10.f;
  bool event_received = false;
  EXPECT_CALL(window_delegate_, DispatchEvent(_))
      .WillOnce([&event_received, kLocationX, kLocationY](ui::Event* event) {
        EXPECT_EQ(event->AsTouchEvent()->location_f().x(), kLocationX);
        EXPECT_EQ(event->AsTouchEvent()->location_f().y(), kLocationY);
        event_received = true;
      });
  std::vector<fuchsia_ui_pointer::TouchEvent> events;
  events.push_back(TouchEventBuilder()
                       .SetPosition({kLocationX, kLocationY})
                       .SetTouchInteractionStatus(
                           fuchsia_ui_pointer::TouchInteractionStatus::kGranted)
                       .Build());
  fake_touch_source.ScheduleCallback(fidl::NaturalToHLCPP(std::move(events)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(event_received);

  // Update device pixel ratio.
  const float kDPR = 2.f;
  EXPECT_CALL(window_delegate_, OnBoundsChanged(_)).Times(1);
  SetLayoutInfo(100, 100, kDPR);

  // Send the same touch event and expect coordinates to be scaled from
  // TouchEvent.
  event_received = false;
  EXPECT_CALL(window_delegate_, DispatchEvent(_))
      .WillOnce([&event_received, kLocationX, kLocationY,
                 kDPR](ui::Event* event) {
        EXPECT_EQ(event->AsTouchEvent()->location_f().x(), kLocationX * kDPR);
        EXPECT_EQ(event->AsTouchEvent()->location_f().y(), kLocationY * kDPR);
        event_received = true;
      });
  events.clear();
  events.push_back(TouchEventBuilder()
                       .SetPosition({kLocationX, kLocationY})
                       .SetTouchInteractionStatus(
                           fuchsia_ui_pointer::TouchInteractionStatus::kGranted)
                       .Build());
  fake_touch_source.ScheduleCallback(fidl::NaturalToHLCPP(std::move(events)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(event_received);
}

TEST_F(FlatlandWindowTest, WaitsForNonZeroSizeToAttachSurfaceContent) {
  size_t presents_called = 0u;
  fake_flatland_.SetPresentHandler(
      [&presents_called](auto) { ++presents_called; });

  CreateWindow();

  // FlatlandWindow should start watching callbacks in ctor.
  task_environment_.RunUntilIdle();

  // Try attaching the content. It should only be a closure.
  fuchsia::ui::views::ViewCreationToken view_token;
  fuchsia::ui::views::ViewportCreationToken viewport_token;
  auto status =
      zx::channel::create(0, &viewport_token.value, &view_token.value);
  CHECK_EQ(ZX_OK, status);
  flatland_window_->AttachSurfaceContent(std::move(viewport_token));
  EXPECT_TRUE(HasPendingAttachSurfaceContentClosure());

  // Setting layout info should trigger the closure and delegate calls.
  EXPECT_CALL(window_delegate_, OnWindowStateChanged(_, _)).Times(1);
  EXPECT_CALL(window_delegate_, OnBoundsChanged(_)).Times(1);

  const uint32_t kWidth = 200;
  const uint32_t kHeight = 100;
  const fuchsia::math::SizeU expected_size = {kWidth, kHeight};

  SetWindowStatus(
      fuchsia::ui::composition::ParentViewportStatus::CONNECTED_TO_DISPLAY);
  SetLayoutInfo(kWidth, kHeight, 1.f);
  EXPECT_FALSE(HasPendingAttachSurfaceContentClosure());

  // There should be a present call in FakeFlatland after flushing the tasks.
  EXPECT_EQ(0u, presents_called);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, presents_called);

  // Show to attach the scene graph.
  flatland_window_->Show(/*inactive=*/false);
  fuchsia::ui::composition::OnNextFrameBeginValues on_next_frame_begin_values;
  on_next_frame_begin_values.set_additional_present_credits(1);
  fake_flatland_.FireOnNextFrameBeginEvent(
      std::move(on_next_frame_begin_values));

  // Spin the loop to process Present().
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2u, presents_called);
  EXPECT_THAT(fake_flatland_.graph(),
              IsWindowGraph(parent_viewport_watcher(), viewport_token_,
                            Contains(IsViewport(view_token, expected_size))));
}

// Verify that surface is cleared when the window is disconnected from the
// display.
TEST_F(FlatlandWindowTest, ResetSurfaceOnDisconnect) {
  CreateWindow();

  EXPECT_CALL(window_delegate_, OnBoundsChanged(_));
  SetLayoutInfo(100, 100, 1.f);
  task_environment_.RunUntilIdle();

  // Try attaching the content. It should only be a closure.
  fuchsia::ui::views::ViewCreationToken view_token;
  fuchsia::ui::views::ViewportCreationToken viewport_token;
  auto status =
      zx::channel::create(0, &viewport_token.value, &view_token.value);
  CHECK_EQ(ZX_OK, status);
  flatland_window_->AttachSurfaceContent(std::move(viewport_token));

  SetWindowStatus(
      fuchsia::ui::composition::ParentViewportStatus::CONNECTED_TO_DISPLAY);

  // Show to attach the scene graph.
  flatland_window_->Show(/*inactive=*/false);
  fuchsia::ui::composition::OnNextFrameBeginValues on_next_frame_begin_values;
  on_next_frame_begin_values.set_additional_present_credits(1);
  fake_flatland_.FireOnNextFrameBeginEvent(
      std::move(on_next_frame_begin_values));

  // Spin the loop to process Present().
  task_environment_.RunUntilIdle();

  // surface view should be attached once the window is shown.
  EXPECT_THAT(fake_flatland_.graph(),
              IsWindowGraph(parent_viewport_watcher(), viewport_token_, _));

  // Remove the window from the screen and verify that it simulates destruction
  // of AcceleratedWidget, which is necessary to ensure that WindowSurface is
  // re-initialized.
  testing::Mock::VerifyAndClearExpectations(&window_delegate_);
  EXPECT_CALL(window_delegate_, OnAcceleratedWidgetDestroyed());
  EXPECT_CALL(window_delegate_, OnAcceleratedWidgetAvailable(window_widget_));

  SetWindowStatus(fuchsia::ui::composition::ParentViewportStatus::
                      DISCONNECTED_FROM_DISPLAY);

  on_next_frame_begin_values.set_additional_present_credits(1);
  fake_flatland_.FireOnNextFrameBeginEvent(
      std::move(on_next_frame_begin_values));

  // Spin the loop to process Present().
  task_environment_.RunUntilIdle();

  // Verify that the surface view is cleared.
  EXPECT_THAT(
      fake_flatland_.graph(),
      IsWindowGraph(parent_viewport_watcher(), viewport_token_, IsEmpty()));
}

// Verify that when surface is attached, a hit region accompanies the surface.
TEST_F(FlatlandWindowTest, SurfaceHasHitTestHitShield) {
  CreateWindow();

  EXPECT_CALL(window_delegate_, OnBoundsChanged(_));
  const uint32_t kWidth = 200;
  const uint32_t kHeight = 100;
  const fuchsia::math::SizeU expected_size = {kWidth, kHeight};
  SetLayoutInfo(kWidth, kHeight, 1.f);

  // Spin the loop to propagate layout.
  task_environment_.RunUntilIdle();

  fuchsia::ui::views::ViewCreationToken view_token;
  fuchsia::ui::views::ViewportCreationToken viewport_token;
  auto status =
      zx::channel::create(0, &viewport_token.value, &view_token.value);
  CHECK_EQ(ZX_OK, status);
  flatland_window_->AttachSurfaceContent(std::move(viewport_token));

  // Show() the window, to trigger creation of the scene graph, including
  // surface and hit shield.
  flatland_window_->Show(/*inactive=*/false);
  fuchsia::ui::composition::OnNextFrameBeginValues on_next_frame_begin_values;
  on_next_frame_begin_values.set_additional_present_credits(1);
  fake_flatland_.FireOnNextFrameBeginEvent(
      std::move(on_next_frame_begin_values));

  // Spin the loop to process Present().
  task_environment_.RunUntilIdle();

  // Surface should be accompanied by input shield, in that order.
  EXPECT_THAT(
      fake_flatland_.graph(),
      Field("root_transform", &FakeGraph::root_transform,
            Pointee(Field("children", &FakeTransform::children,
                          ElementsAre(IsViewport(view_token, expected_size),
                                      IsHitShield())))));
}

class ParameterizedViewInsetTest : public FlatlandWindowTest,
                                   public testing::WithParamInterface<float> {};

INSTANTIATE_TEST_SUITE_P(ViewInsetTest,
                         ParameterizedViewInsetTest,
                         testing::Values(1.f, 2.f, 3.f));

// Tests whether view insets are properly set in |FlatlandWindow|.
TEST_P(ParameterizedViewInsetTest, ViewInsetsTest) {
  CreateWindow();
  EXPECT_CALL(window_delegate_, OnBoundsChanged(_)).Times(1);
  SetLayoutInfo(100, 100, 1.f);

  const fuchsia::math::Inset inset = {1, 1, 1, 1};
  const float dpr = GetParam();

  // Setting LayoutInfo should trigger a change in the bounds.
  PlatformWindowDelegate::BoundsChange bounds(false);
  EXPECT_CALL(window_delegate_, OnBoundsChanged(_))
      .WillOnce(SaveArg<0>(&bounds));
  SetLayoutInfo(100, 100, dpr, inset);

  EXPECT_EQ(bounds.system_ui_overlap.top(), dpr * inset.top);
  EXPECT_EQ(bounds.system_ui_overlap.left(), dpr * inset.left);
  EXPECT_EQ(bounds.system_ui_overlap.bottom(), dpr * inset.bottom);
  EXPECT_EQ(bounds.system_ui_overlap.right(), dpr * inset.right);
}

}  // namespace ui
