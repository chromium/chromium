// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/display_switches.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/test/mock_zaura_shell.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/platform_window/platform_window_init_properties.h"

using ::testing::Values;

namespace ui {

namespace {

constexpr uint32_t kNumberOfDisplays = 1;
constexpr uint32_t kOutputWidth = 1024;
constexpr uint32_t kOutputHeight = 768;

class TestDisplayObserver : public display::DisplayObserver {
 public:
  TestDisplayObserver() {}

  TestDisplayObserver(const TestDisplayObserver&) = delete;
  TestDisplayObserver& operator=(const TestDisplayObserver&) = delete;

  ~TestDisplayObserver() override {}

  display::Display GetDisplay() { return std::move(display_); }
  display::Display GetRemovedDisplay() { return std::move(removed_display_); }
  uint32_t GetAndClearChangedMetrics() {
    uint32_t changed_metrics = changed_metrics_;
    changed_metrics_ = 0;
    return changed_metrics;
  }

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    display_ = new_display;
  }

  void OnDisplayRemoved(const display::Display& old_display) override {
    removed_display_ = old_display;
  }

  void OnDidRemoveDisplays() override {
    if (did_remove_display_closure_)
      did_remove_display_closure_.Run();
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    changed_metrics_ = changed_metrics;
    display_ = display;
  }

  void set_did_remove_display_closure(base::RepeatingClosure closure) {
    did_remove_display_closure_ = std::move(closure);
  }

 private:
  uint32_t changed_metrics_ = 0;
  display::Display display_;
  display::Display removed_display_;
  base::RepeatingClosure did_remove_display_closure_{};
};

}  // namespace

class WaylandScreenTest : public WaylandTest {
 public:
  WaylandScreenTest() = default;

  WaylandScreenTest(const WaylandScreenTest&) = delete;
  WaylandScreenTest& operator=(const WaylandScreenTest&) = delete;

  ~WaylandScreenTest() override = default;

  void SetUp() override {
    output_ = server_.output();

    WaylandTest::SetUp();

    output_->SetRect({kOutputWidth, kOutputHeight});
    output_->SetScale(1);
    output_->Flush();
    Sync();

    output_manager_ = connection_->wayland_output_manager();
    ASSERT_TRUE(output_manager_);

    EXPECT_TRUE(output_manager_->IsOutputReady());
    platform_screen_ = output_manager_->CreateWaylandScreen();
    output_manager_->InitWaylandScreen(platform_screen_.get());
  }

 protected:
  std::unique_ptr<WaylandWindow> CreateWaylandWindowWithProperties(
      const gfx::Rect& bounds,
      PlatformWindowType window_type,
      gfx::AcceleratedWidget parent_widget,
      MockWaylandPlatformWindowDelegate* delegate) {
    PlatformWindowInitProperties properties;
    properties.bounds = bounds;
    properties.type = window_type;
    properties.parent_widget = parent_widget;
    return delegate->CreateWaylandWindow(connection_.get(),
                                         std::move(properties));
  }

  void ValidateTheDisplayForWidget(gfx::AcceleratedWidget widget,
                                   int64_t expected_display_id) {
    display::Display display_for_widget =
        platform_screen_->GetDisplayForAcceleratedWidget(widget);
    EXPECT_EQ(display_for_widget.id(), expected_display_id);
  }

  raw_ptr<wl::TestOutput> output_ = nullptr;
  raw_ptr<WaylandOutputManager> output_manager_ = nullptr;

  std::unique_ptr<WaylandScreen> platform_screen_;
};

// Tests whether a primary output has been initialized before PlatformScreen is
// created.
TEST_P(WaylandScreenTest, OutputBaseTest) {
  // IsPrimaryOutputReady and PlatformScreen creation is done in the
  // initialization part of the tests.

  // Ensure there is only one display, which is the primary one.
  auto& all_displays = platform_screen_->GetAllDisplays();
  EXPECT_EQ(all_displays.size(), kNumberOfDisplays);

  // Ensure the size property of the primary display.
  EXPECT_EQ(platform_screen_->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, kOutputWidth, kOutputHeight));
}

// In multi-monitor setup, the `entered_outputs_` list should be updated when
// the display is unplugged or switched off.
TEST_P(WaylandScreenTest, EnteredOutputListAfterDisplayRemoval) {
  wl::TestOutput* output1 = server_.output();
  gfx::Rect output1_rect = server_.output()->GetRect();

  // Add a second display.
  wl::TestOutput* output2 = server_.CreateAndInitializeOutput();
  Sync();
  // The second display is located to the right of first display
  gfx::Rect output2_rect(output1_rect.right(), 0, 800, 600);
  output2->SetRect(output2_rect);
  output2->Flush();
  Sync();

  // Add a third display.
  wl::TestOutput* output3 = server_.CreateAndInitializeOutput();
  Sync();
  // The third display is located to the right of second display
  gfx::Rect output3_rect(output2_rect.right(), 0, 800, 600);
  output3->SetRect(output3_rect);
  output3->Flush();
  Sync();

  EXPECT_EQ(3u, platform_screen_->GetAllDisplays().size());

  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->get_surface_id());
  ASSERT_TRUE(surface);

  wl_surface_send_enter(surface->resource(), output1->resource());
  wl_surface_send_enter(surface->resource(), output2->resource());
  Sync();
  // The window entered two outputs
  auto entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(2u, entered_outputs.size());

  wl_surface_send_enter(surface->resource(), output3->resource());
  Sync();
  // The window entered three outputs
  entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(3u, entered_outputs.size());

  // Destroy third display
  output3->DestroyGlobal();
  Sync();
  entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(2u, entered_outputs.size());

  // Destroy second display
  output2->DestroyGlobal();
  Sync();
  entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(1u, entered_outputs.size());

  // Add a second display.
  output2 = server_.CreateAndInitializeOutput();
  Sync();
  // The second display is located to the right of first display
  output2->SetRect(output2_rect);
  output2->Flush();
  Sync();

  wl_surface_send_enter(surface->resource(), output2->resource());
  Sync();

  // The window entered two outputs
  entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(2u, entered_outputs.size());
}

TEST_P(WaylandScreenTest, MultipleOutputsAddedAndRemoved) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const int64_t old_primary_display_id =
      platform_screen_->GetPrimaryDisplay().id();
  gfx::Rect output1_rect = server_.output()->GetRect();

  // Add a second display.
  wl::TestOutput* output2 = server_.CreateAndInitializeOutput();

  Sync();

  // The second display is located to the right of first display like
  // | || |.
  gfx::Rect output2_rect(output1_rect.width(), 0, 800, 600);
  output2->SetRect(output2_rect);
  output2->Flush();

  Sync();

  // Ensure that second display is not a primary one and have a different id.
  int64_t added_display_id = observer.GetDisplay().id();
  EXPECT_NE(platform_screen_->GetPrimaryDisplay().id(), added_display_id);

  output2->DestroyGlobal();

  Sync();

  // Ensure that removed display has correct id.
  int64_t removed_display_id = observer.GetRemovedDisplay().id();
  EXPECT_EQ(added_display_id, removed_display_id);

  // Create another display again.
  output2 = server_.CreateAndInitializeOutput();

  Sync();

  // Updates rect again.
  output2->SetRect(output2_rect);
  output2->Flush();

  Sync();

  // The newly added display is not a primary yet.
  added_display_id = observer.GetDisplay().id();
  EXPECT_NE(platform_screen_->GetPrimaryDisplay().id(), added_display_id);

  // Now, rearrange displays so that second display becomes the primary one.
  output1_rect = gfx::Rect(1024, 0, 1024, 768);
  output_->SetRect(output1_rect);
  output_->Flush();

  output2_rect = gfx::Rect(0, 0, 1024, 768);
  output2->SetRect(output2_rect);
  output2->Flush();

  Sync();

  // Ensure that output2 is now the primary one.
  EXPECT_EQ(platform_screen_->GetPrimaryDisplay().id(), added_display_id);

  // Remove the primary display now.
  output2->DestroyGlobal();

  Sync();

  // Ensure that output1 is a primary display now.
  EXPECT_EQ(platform_screen_->GetPrimaryDisplay().id(), old_primary_display_id);
  // Ensure that the removed display was the one, which was a primary display.
  EXPECT_EQ(observer.GetRemovedDisplay().id(), added_display_id);

  platform_screen_->RemoveObserver(&observer);
}

TEST_P(WaylandScreenTest, OutputPropertyChangesMissingLogicalSize) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const uint32_t output_id = 7;
  const int64_t display_id = 1ll << 34;
  const gfx::Point origin(50, 70);
  const gfx::Size physical_size(1200, 1600);
  const wl_output_transform panel_transform = WL_OUTPUT_TRANSFORM_90;
  const wl_output_transform logical_transform = WL_OUTPUT_TRANSFORM_NORMAL;
  const gfx::Insets insets = gfx::Insets::TLBR(10, 20, 30, 40);
  const float scale = 2;

  // Test with missing logical size. Should fall back to calculating from
  // physical size.
  platform_screen_->OnOutputAddedOrUpdated(
      {output_id, display_id, origin, gfx::Size(), physical_size, insets, scale,
       panel_transform, logical_transform, "display"});

  const display::Display new_display(observer.GetDisplay());
  EXPECT_EQ(output_id, platform_screen_->GetOutputIdForDisplayId(display_id));
  EXPECT_EQ(new_display.id(), display_id);
  EXPECT_EQ(new_display.bounds(), gfx::Rect(origin, gfx::Size(800, 600)));
  EXPECT_EQ(new_display.GetSizeInPixel(), gfx::Size(1600, 1200));
  gfx::Rect expected_work_area(new_display.bounds());
  expected_work_area.Inset(insets);
  EXPECT_EQ(new_display.work_area(), expected_work_area);
  EXPECT_EQ(new_display.panel_rotation(), display::Display::ROTATE_270);
  EXPECT_EQ(new_display.rotation(), display::Display::ROTATE_0);
  EXPECT_EQ(new_display.device_scale_factor(), scale);
  EXPECT_EQ(new_display.label(), "display");

  platform_screen_->RemoveObserver(&observer);
}

TEST_P(WaylandScreenTest, OutputPropertyChangesPrimaryDisplayChanged) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  display::Display display1(1, gfx::Rect(0, 0, 800, 600));
  display::Display display2(2, gfx::Rect(800, 0, 700, 500));

  platform_screen_->OnOutputAddedOrUpdated(
      {static_cast<uint32_t>(display1.id()), display1.id(),
       display1.bounds().origin(), display1.size(), display1.GetSizeInPixel(),
       display1.GetWorkAreaInsets(), display1.device_scale_factor(),
       WL_OUTPUT_TRANSFORM_NORMAL, WL_OUTPUT_TRANSFORM_NORMAL, std::string()});
  platform_screen_->OnOutputAddedOrUpdated(
      {static_cast<uint32_t>(display2.id()), display2.id(),
       display2.bounds().origin(), display2.size(), display2.GetSizeInPixel(),
       display2.GetWorkAreaInsets(), display2.device_scale_factor(),
       WL_OUTPUT_TRANSFORM_NORMAL, WL_OUTPUT_TRANSFORM_NORMAL, std::string()});

  EXPECT_EQ(platform_screen_->GetPrimaryDisplay(), display1);

  // Simulate setting display2 as primary by moving its origin to (0,0) and
  // shifting display1 to its left.
  display1.set_bounds(gfx::Rect(-800, 0, 800, 600));
  display2.set_bounds(gfx::Rect(0, 0, 700, 500));

  // Purposely send the output metrics out of order.
  platform_screen_->OnOutputAddedOrUpdated(
      {static_cast<uint32_t>(display2.id()), display2.id(),
       display2.bounds().origin(), display2.size(), display2.GetSizeInPixel(),
       display2.GetWorkAreaInsets(), display2.device_scale_factor(),
       WL_OUTPUT_TRANSFORM_NORMAL, WL_OUTPUT_TRANSFORM_NORMAL, std::string()});
  platform_screen_->OnOutputAddedOrUpdated(
      {static_cast<uint32_t>(display1.id()), display1.id(),
       display1.bounds().origin(), display1.size(), display1.GetSizeInPixel(),
       display1.GetWorkAreaInsets(), display1.device_scale_factor(),
       WL_OUTPUT_TRANSFORM_NORMAL, WL_OUTPUT_TRANSFORM_NORMAL, std::string()});

  EXPECT_EQ(platform_screen_->GetPrimaryDisplay(), display2);

  platform_screen_->RemoveObserver(&observer);
}

TEST_P(WaylandScreenTest, GetAcceleratedWidgetAtScreenPoint) {
  // Now, send enter event for the surface, which was created before.
  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->get_surface_id());
  ASSERT_TRUE(surface);
  wl_surface_send_enter(surface->resource(), output_->resource());

  Sync();

  // If there is no focused window (focus is set whenever a pointer enters any
  // of the windows), there must be kNullAcceleratedWidget returned. There is no
  // real way to determine what window is located on a certain screen point in
  // Wayland.
  gfx::AcceleratedWidget widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(10, 10));
  EXPECT_EQ(widget_at_screen_point, gfx::kNullAcceleratedWidget);

  // Set a focus to the main window. Now, that focused window must be returned.
  SetPointerFocusedWindow(window_.get());
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(10, 10));
  EXPECT_EQ(widget_at_screen_point, window_->GetWidget());

  // Getting a widget at a screen point outside its bounds, must result in a
  // null widget.
  const gfx::Rect window_bounds = window_->GetBoundsInDIP();
  widget_at_screen_point = platform_screen_->GetAcceleratedWidgetAtScreenPoint(
      gfx::Point(window_bounds.width() + 1, window_bounds.height() + 1));
  EXPECT_EQ(widget_at_screen_point, gfx::kNullAcceleratedWidget);

  MockWaylandPlatformWindowDelegate delegate;
  auto menu_window_bounds =
      gfx::Rect(window_->GetBoundsInDIP().width() - 10,
                window_->GetBoundsInDIP().height() - 10, 100, 100);
  std::unique_ptr<WaylandWindow> menu_window =
      CreateWaylandWindowWithProperties(menu_window_bounds,
                                        PlatformWindowType::kMenu,
                                        window_->GetWidget(), &delegate);

  Sync();

  // Imagine the mouse enters a menu window, which is located on top of the main
  // window, and gathers focus.
  SetPointerFocusedWindow(menu_window.get());

  widget_at_screen_point = platform_screen_->GetAcceleratedWidgetAtScreenPoint(
      gfx::Point(menu_window->GetBoundsInDIP().x() + 1,
                 menu_window->GetBoundsInDIP().y() + 1));
  EXPECT_EQ(widget_at_screen_point, menu_window->GetWidget());

  // Whenever a mouse pointer leaves the menu window, the accelerated widget
  // of that focused window must be returned.
  SetPointerFocusedWindow(window_.get());
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(0, 0));
  EXPECT_EQ(widget_at_screen_point, window_->GetWidget());

  // Reset the focus to avoid crash on dtor as long as there is no real pointer
  // object.
  SetPointerFocusedWindow(nullptr);

  // Part 2: test that the window is found when display's scale changes.
  // Update scale.
  output_->SetScale(2);
  output_->Flush();

  Sync();

  auto menu_bounds = menu_window->GetBoundsInDIP();
  auto point_in_screen = menu_bounds.origin();
  SetPointerFocusedWindow(menu_window.get());
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(point_in_screen);
  EXPECT_EQ(widget_at_screen_point, menu_window->GetWidget());
}

TEST_P(WaylandScreenTest, GetLocalProcessWidgetAtPoint) {
  gfx::Point point(10, 10);
  EXPECT_EQ(platform_screen_->GetLocalProcessWidgetAtPoint(point, {}),
            gfx::kNullAcceleratedWidget);

  // Set a focus to the main window. Now, that focused window must be returned.
  SetPointerFocusedWindow(window_.get());
  EXPECT_EQ(platform_screen_->GetLocalProcessWidgetAtPoint(point, {}),
            window_->GetWidget());

  // Null widget must be returned when the focused window is part of the
  // |ignore| list.
  gfx::AcceleratedWidget w = window_->GetWidget();
  EXPECT_EQ(
      platform_screen_->GetLocalProcessWidgetAtPoint(point, {w - 1, w, w + 1}),
      gfx::kNullAcceleratedWidget);
}

TEST_P(WaylandScreenTest, GetDisplayMatching) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const display::Display primary_display =
      platform_screen_->GetPrimaryDisplay();

  wl::TestOutput* output2 = server_.CreateAndInitializeOutput();

  Sync();

  // Place it on the right side of the primary display.
  const gfx::Rect output2_rect =
      gfx::Rect(primary_display.bounds().width(), 0, 1024, 768);
  output2->SetRect(output2_rect);
  output2->Flush();

  Sync();

  const display::Display second_display = observer.GetDisplay();
  EXPECT_EQ(second_display.bounds(), output2_rect);

  // We have two displays: display1(0:0,1024x768) and display2(1024:0,1024x768).
  EXPECT_EQ(
      primary_display.id(),
      platform_screen_->GetDisplayMatching(gfx::Rect(0, 0, 100, 100)).id());
  EXPECT_EQ(
      second_display.id(),
      platform_screen_->GetDisplayMatching(gfx::Rect(1024, 0, 10, 10)).id());

  // More pixels on second display.
  EXPECT_EQ(
      second_display.id(),
      platform_screen_->GetDisplayMatching(gfx::Rect(1020, 0, 10, 10)).id());

  // More pixels on first display.
  EXPECT_EQ(
      primary_display.id(),
      platform_screen_->GetDisplayMatching(gfx::Rect(1018, 0, 10, 10)).id());

  // Half pixels on second and half on primary.
  EXPECT_EQ(
      primary_display.id(),
      platform_screen_->GetDisplayMatching(gfx::Rect(1019, 0, 10, 10)).id());

  // Place second display 700 pixels below along y axis (1024:700,1024x768)
  output2->SetRect(
      gfx::Rect(gfx::Point(output2_rect.x(), output2_rect.y() + 700),
                output2_rect.size()));
  output2->Flush();

  Sync();

  // The match rect is located outside the displays. Primary display must be
  // returned.
  EXPECT_EQ(
      primary_display.id(),
      platform_screen_->GetDisplayMatching(gfx::Rect(1024, 0, 10, 10)).id());

  // At least some of the pixels are located on the display.
  EXPECT_EQ(
      primary_display.id(),
      platform_screen_->GetDisplayMatching(gfx::Rect(1023, 0, 10, 10)).id());

  // Most of pixels are located on second display.
  EXPECT_EQ(
      second_display.id(),
      platform_screen_->GetDisplayMatching(gfx::Rect(1023, 695, 10, 10)).id());

  // Empty rect results in primary display.
  EXPECT_EQ(primary_display.id(),
            platform_screen_->GetDisplayMatching(gfx::Rect(0, 0, 0, 0)).id());

  platform_screen_->RemoveObserver(&observer);
  output2->DestroyGlobal();
  Sync();
}

// Regression test for https://crbug.com/1362872.
TEST_P(WaylandScreenTest, GetPrimaryDisplayAfterRemoval) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const display::Display primary_display =
      platform_screen_->GetPrimaryDisplay();

  ASSERT_NE(primary_display.id(), display::kInvalidDisplayId);
  ASSERT_EQ(1u, platform_screen_->GetAllDisplays().size());

  // This results in an ASAN error unless GetPrimaryDisplay() is correctly
  // implemented for empty display list. More details in the crbug above.
  observer.set_did_remove_display_closure(base::BindLambdaForTesting([&]() {
    ASSERT_EQ(0u, platform_screen_->GetAllDisplays().size());
    auto display = platform_screen_->GetPrimaryDisplay();
    EXPECT_EQ(display::kDefaultDisplayId, display.id());
  }));
  output_->DestroyGlobal();
  Sync();

  platform_screen_->RemoveObserver(&observer);
}

TEST_P(WaylandScreenTest, GetDisplayForAcceleratedWidget) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const display::Display primary_display =
      platform_screen_->GetPrimaryDisplay();

  // Create an additional display.
  wl::TestOutput* output2 = server_.CreateAndInitializeOutput();

  Sync();

  // Place it on the right side of the primary
  // display.
  const gfx::Rect output2_rect =
      gfx::Rect(primary_display.bounds().width(), 0, 1024, 768);
  output2->SetRect(output2_rect);
  output2->Flush();

  Sync();

  const display::Display secondary_display = observer.GetDisplay();
  EXPECT_EQ(secondary_display.bounds(), output2_rect);

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  // There must be a primary display used if the window has not received an
  // enter event yet.
  ValidateTheDisplayForWidget(widget, primary_display.id());

  // Now, send enter event for the surface, which was created before.
  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->get_surface_id());
  ASSERT_TRUE(surface);
  wl_surface_send_enter(surface->resource(), output_->resource());

  Sync();

  // The id of the entered display must correspond to the primary output.
  ValidateTheDisplayForWidget(widget, primary_display.id());

  Sync();

  // Enter the second output now.
  wl_surface_send_enter(surface->resource(), output2->resource());

  Sync();

  // The id of the entered display must still correspond to the primary output.
  ValidateTheDisplayForWidget(widget, primary_display.id());

  // Leave the first output.
  wl_surface_send_leave(surface->resource(), output_->resource());

  Sync();

  // The id of the entered display must correspond to the second output.
  ValidateTheDisplayForWidget(widget, secondary_display.id());

  // Leaving the same output twice (check comment in
  // WaylandWindow::OnEnteredOutputIdRemoved), must be ok and nothing must
  // change.
  wl_surface_send_leave(surface->resource(), output_->resource());

  Sync();

  // The id of the entered display must correspond to the second output.
  ValidateTheDisplayForWidget(widget, secondary_display.id());

  output2->DestroyGlobal();
  Sync();
}

TEST_P(WaylandScreenTest, GetCursorScreenPoint) {
  MockWaylandPlatformWindowDelegate delegate;
  std::unique_ptr<WaylandWindow> second_window =
      CreateWaylandWindowWithProperties(gfx::Rect(0, 0, 1920, 1080),
                                        PlatformWindowType::kWindow,
                                        gfx::kNullAcceleratedWidget, &delegate);

  auto* surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->get_surface_id());
  ASSERT_TRUE(surface);

  // Announce pointer capability so that WaylandPointer is created on the client
  // side.
  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);

  Sync();

  wl::MockPointer* pointer = server_.seat()->pointer();
  ASSERT_TRUE(pointer);

  uint32_t serial = 0;
  uint32_t time = 1002;
  wl_pointer_send_enter(pointer->resource(), ++serial, surface->resource(), 0,
                        0);
  wl_pointer_send_frame(pointer->resource());
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(10),
                         wl_fixed_from_int(20));
  wl_pointer_send_frame(pointer->resource());

  Sync();

  // WaylandScreen must return the last pointer location.
  EXPECT_EQ(gfx::Point(10, 20), platform_screen_->GetCursorScreenPoint());

  auto* second_surface = server_.GetObject<wl::MockSurface>(
      second_window->root_surface()->get_surface_id());
  ASSERT_TRUE(second_surface);
  // Now, leave the first surface and enter second one.
  wl_pointer_send_leave(pointer->resource(), ++serial, surface->resource());
  wl_pointer_send_frame(pointer->resource());
  wl_pointer_send_enter(pointer->resource(), ++serial,
                        second_surface->resource(), 0, 0);
  wl_pointer_send_frame(pointer->resource());
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(20),
                         wl_fixed_from_int(10));
  wl_pointer_send_frame(pointer->resource());

  Sync();

  // WaylandScreen must return the last pointer location.
  EXPECT_EQ(gfx::Point(20, 10), platform_screen_->GetCursorScreenPoint());

  // Clear pointer focus.
  wl_pointer_send_leave(pointer->resource(), ++serial,
                        second_surface->resource());
  wl_pointer_send_frame(pointer->resource());

  Sync();

  // WaylandScreen must return a point, which is located outside of bounds of
  // any window. Basically, it means that it takes the largest window and adds
  // 10 pixels to its width and height, and returns the value.
  const gfx::Rect second_window_bounds = second_window->GetBoundsInDIP();
  // A second window has largest bounds. Thus, these bounds must be taken as a
  // ground for the point outside any of the surfaces.
  ASSERT_TRUE(window_->GetBoundsInDIP() < second_window_bounds);
  EXPECT_EQ(gfx::Point(second_window_bounds.width() + 10,
                       second_window_bounds.height() + 10),
            platform_screen_->GetCursorScreenPoint());

  // Create a menu window now and ensure cursor position is always sent in
  // regards to that window bounds.
  std::unique_ptr<WaylandWindow> menu_window =
      CreateWaylandWindowWithProperties(
          gfx::Rect(second_window_bounds.width() - 10,
                    second_window_bounds.height() - 10, 10, 20),
          PlatformWindowType::kMenu, second_window->GetWidget(), &delegate);

  Sync();

  auto* menu_surface = server_.GetObject<wl::MockSurface>(
      menu_window->root_surface()->get_surface_id());
  ASSERT_TRUE(menu_surface);

  wl_pointer_send_enter(pointer->resource(), ++serial, menu_surface->resource(),
                        0, 0);
  wl_pointer_send_frame(pointer->resource());
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(2),
                         wl_fixed_from_int(1));
  wl_pointer_send_frame(pointer->resource());

  Sync();

  // The cursor screen point must be converted to the top-level window
  // coordinates as long as Wayland doesn't provide global coordinates of
  // surfaces and Chromium assumes those windows are always located at origin
  // (0,0). For more information, check the comment in
  // WaylandWindow::UpdateCursorPositionFromEvent.
  EXPECT_EQ(gfx::Point(1912, 1071), platform_screen_->GetCursorScreenPoint());

  // Leave the menu window and enter the top level window.
  wl_pointer_send_leave(pointer->resource(), ++serial,
                        menu_surface->resource());
  wl_pointer_send_frame(pointer->resource());
  wl_pointer_send_enter(pointer->resource(), ++serial,
                        second_surface->resource(), 0, 0);
  wl_pointer_send_frame(pointer->resource());
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(1912),
                         wl_fixed_from_int(1071));
  wl_pointer_send_frame(pointer->resource());

  Sync();

  // WaylandWindow::UpdateCursorPositionFromEvent mustn't convert this point,
  // because it has already been located on the top-level window.
  EXPECT_EQ(gfx::Point(1912, 1071), platform_screen_->GetCursorScreenPoint());

  wl_pointer_send_leave(pointer->resource(), ++serial,
                        second_surface->resource());
  wl_pointer_send_frame(pointer->resource());

  // Now, create a nested menu window and make sure that the cursor screen point
  // still has been correct. The location of the window is on the right side of
  // the main menu window.
  const gfx::Rect menu_window_bounds = menu_window->GetBoundsInDIP();
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithProperties(
          gfx::Rect(menu_window_bounds.x() + menu_window_bounds.width(),
                    menu_window_bounds.y() + 2, 10, 20),
          PlatformWindowType::kMenu, second_window->GetWidget(), &delegate);

  Sync();

  auto* nested_menu_surface = server_.GetObject<wl::MockSurface>(
      nested_menu_window->root_surface()->get_surface_id());
  ASSERT_TRUE(nested_menu_surface);

  wl_pointer_send_enter(pointer->resource(), ++serial,
                        nested_menu_surface->resource(), 0, 0);
  wl_pointer_send_frame(pointer->resource());
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(2),
                         wl_fixed_from_int(3));
  wl_pointer_send_frame(pointer->resource());

  Sync();

  EXPECT_EQ(gfx::Point(1922, 1075), platform_screen_->GetCursorScreenPoint());

  // Leave the nested surface and enter main menu surface. The cursor screen
  // point still must be reported correctly.
  wl_pointer_send_leave(pointer->resource(), ++serial,
                        nested_menu_surface->resource());
  wl_pointer_send_frame(pointer->resource());
  wl_pointer_send_enter(pointer->resource(), ++serial, menu_surface->resource(),
                        0, 0);
  wl_pointer_send_frame(pointer->resource());
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(2),
                         wl_fixed_from_int(1));
  wl_pointer_send_frame(pointer->resource());

  Sync();

  EXPECT_EQ(gfx::Point(1912, 1071), platform_screen_->GetCursorScreenPoint());
}

// Checks that the surface that backs the window receives new scale of the
// output that it is in.
TEST_P(WaylandScreenTest, SetWindowScale) {
  // Place the window onto the output.
  wl_surface_send_enter(surface_->resource(), output_->resource());

  // Change the scale of the output.  Windows looking into that output must get
  // the new scale and update scale of their buffers.  The default UI scale
  // equals the output scale.
  const int32_t kTripleScale = 3;
  output_->SetScale(kTripleScale);
  output_->Flush();

  Sync();

  EXPECT_EQ(window_->window_scale(), kTripleScale);
  EXPECT_EQ(window_->ui_scale_, kTripleScale);

  // Now simulate the --force-device-scale-factor=1.5
  const float kForcedUIScale = 1.5;
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kForceDeviceScaleFactor,
      base::StringPrintf("%.1f", kForcedUIScale));
  display::Display::ResetForceDeviceScaleFactorForTesting();

  // Change the scale of the output again.  Windows must update scale of
  // their buffers but the UI scale must get the forced value.
  const int32_t kDoubleScale = 2;
  // Question ourselves before questioning others!
  EXPECT_NE(kForcedUIScale, kDoubleScale);
  output_->SetScale(kDoubleScale);
  output_->Flush();

  Sync();

  EXPECT_EQ(window_->window_scale(), kDoubleScale);
  EXPECT_EQ(window_->ui_scale_, kForcedUIScale);

  display::Display::ResetForceDeviceScaleFactorForTesting();
}

// Regression test for https://crbug.com/1346534.
//
// Scenario: With (at least) one output connected and a surface, with no output
// associated yet, ie: wl_surface.enter event not received yet for that surface,
// which implies in its scale being set to the primary output's scale at its
// initialization, any primary output scale update (or other properties that
// lead to scale change) must be propagated to the window.
TEST_P(WaylandScreenTest, SetWindowScaleWithoutEnteredOutput) {
  // Test pre-conditions: single output setup whereas |output_| is the primary
  // output managed by |output_manager_|, with initial scale == 1.
  ASSERT_EQ(1u, output_manager_->GetAllOutputs().size());
  ASSERT_TRUE(output_);
  ASSERT_EQ(1, output_->GetScale());

  // Ensure |surface_| has not entered any wl_output. Assuming |window_| has
  // been already initialized with |output_|'s scale.
  wl_surface_send_leave(surface_->resource(), output_->resource());
  Sync();
  EXPECT_FALSE(window_->GetPreferredEnteredOutputId());

  // Change |output_|'s scale and make sure |window_|'s scale is update
  // accordingly.
  output_->SetScale(2);
  output_->Flush();
  Sync();

  EXPECT_EQ(window_->window_scale(), 2);
  EXPECT_EQ(window_->ui_scale(), 2);
}

// Checks that output transform is properly translated into Display orientation.
// The first one is counter-clockwise, while the latter is clockwise.
TEST_P(WaylandScreenTest, Transform) {
  constexpr std::pair<wl_output_transform, display::Display::Rotation>
      kTestData[] = {
          {WL_OUTPUT_TRANSFORM_NORMAL, display::Display::ROTATE_0},
          {WL_OUTPUT_TRANSFORM_90, display::Display::ROTATE_270},
          {WL_OUTPUT_TRANSFORM_180, display::Display::ROTATE_180},
          {WL_OUTPUT_TRANSFORM_270, display::Display::ROTATE_90},
          // Flipped transforms are not supported.
          {WL_OUTPUT_TRANSFORM_FLIPPED, display::Display::ROTATE_0},
          {WL_OUTPUT_TRANSFORM_FLIPPED_90, display::Display::ROTATE_0},
          {WL_OUTPUT_TRANSFORM_FLIPPED_180, display::Display::ROTATE_0},
          {WL_OUTPUT_TRANSFORM_FLIPPED_270, display::Display::ROTATE_0},
      };

  for (const auto& [transform, expected_rotation] : kTestData) {
    output_->SetTransform(transform);
    output_->Flush();

    Sync();

    auto main_display = platform_screen_->GetPrimaryDisplay();
    EXPECT_EQ(main_display.rotation(), expected_rotation);
  }
}

namespace {

class LazilyConfiguredScreenTest
    : public WaylandTest,
      public wl::TestWaylandServerThread::OutputDelegate {
 public:
  LazilyConfiguredScreenTest() = default;
  LazilyConfiguredScreenTest(const LazilyConfiguredScreenTest&) = delete;
  LazilyConfiguredScreenTest& operator=(const LazilyConfiguredScreenTest&) =
      delete;
  ~LazilyConfiguredScreenTest() override = default;

  void SetUp() override {
    // Being the server's output delegate allows LazilyConfiguredScreenTest to
    // manipulate wl_outputs during the server's global objects initialization
    // phase. See SetupOutputs() function below.
    server_.set_output_delegate(this);

    WaylandTest::SetUp();

    output_manager_ = connection_->wayland_output_manager();
    ASSERT_TRUE(output_manager_);
  }

  void TearDown() override {
    WaylandTest::TearDown();
    server_.set_output_delegate(nullptr);
  }

 protected:
  // wl::TestWaylandServerThread::OutputDelegate:
  void SetupOutputs(wl::TestOutput* primary) override {
    // Keep the first wl_output announced "unconfigured" and just caches it for
    // now, so we can exercise WaylandOutputManager::IsOutputReady() function
    // when wl_output events come in unordered.
    primary_output_ = primary;

    // Create/announce a second wl_output object and makes it the first one to
    // get configuration events (eg: geometry, done, etc). This is achieved by
    // setting its bounds here.
    aux_output_ = server_.CreateAndInitializeOutput();
    aux_output_->SetRect({0, 0, 800, 600});
  }

  raw_ptr<wl::TestOutput> primary_output_ = nullptr;
  raw_ptr<wl::TestOutput> aux_output_ = nullptr;
  raw_ptr<WaylandOutputManager> output_manager_ = nullptr;
  bool auto_configure;
};

}  // namespace

// Ensures WaylandOutputManager and WaylandScreen properly handle scenarios
// where multiple wl_output objects are announced but not "configured" (ie:
// size, position, mode, etc sent to client) at bind time.
TEST_P(LazilyConfiguredScreenTest, DualOutput) {
  // Ensure WaylandScreen got properly created and fed with a single display
  // object, ie: |aux_output_| at server side.
  EXPECT_TRUE(output_manager_->IsOutputReady());
  EXPECT_TRUE(screen_);
  EXPECT_EQ(1u, screen_->GetAllDisplays().size());
  Sync();

  // Send wl_output configuration events for the first advertised wl_output
  // object. ie: |primary_output_| at server side.
  primary_output_->SetRect({800, 0, kOutputWidth, kOutputHeight});
  primary_output_->SetScale(1);
  primary_output_->Flush();
  Sync();

  // And make sure it makes its way into the WaylandScreen's display list at
  // client side.
  EXPECT_EQ(2u, screen_->GetAllDisplays().size());
}

using WaylandAuraShellScreenTest = WaylandScreenTest;

TEST_P(WaylandAuraShellScreenTest, OutputPropertyChanges) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const gfx::Rect physical_bounds{800, 600};
  output_->SetRect(physical_bounds);
  output_->Flush();

  Sync();

  uint32_t changed_values = display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                            display::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  const gfx::Rect expected_bounds{800, 600};
  EXPECT_EQ(observer.GetDisplay().bounds(), expected_bounds);
  const gfx::Size expected_size_in_pixels{800, 600};
  EXPECT_EQ(observer.GetDisplay().GetSizeInPixel(), expected_size_in_pixels);
  EXPECT_EQ(observer.GetDisplay().work_area(), expected_bounds);

  // Test work area.
  const gfx::Rect new_work_area{10, 20, 700, 500};
  const gfx::Insets expected_inset = expected_bounds.InsetsFrom(new_work_area);
  ASSERT_TRUE(output_->GetAuraOutput());
  output_->GetAuraOutput()->SetInsets(expected_inset);
  output_->Flush();

  Sync();

  changed_values = display::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  // Bounds should be unchanged.
  EXPECT_EQ(observer.GetDisplay().bounds(), expected_bounds);
  EXPECT_EQ(observer.GetDisplay().GetSizeInPixel(), expected_size_in_pixels);
  // Work area should have new value.
  EXPECT_EQ(observer.GetDisplay().work_area(), new_work_area);

  // Test scaling.
  const int32_t new_scale_value = 2;
  output_->SetScale(new_scale_value);
  output_->Flush();

  Sync();

  changed_values =
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
      display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
      display::DisplayObserver::DISPLAY_METRIC_BOUNDS;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  EXPECT_EQ(observer.GetDisplay().device_scale_factor(), new_scale_value);
  // Logical bounds should shrink due to scaling.
  const gfx::Rect scaled_bounds{400, 300};
  EXPECT_EQ(observer.GetDisplay().bounds(), scaled_bounds);
  // Size in pixel should stay unscaled.
  EXPECT_EQ(observer.GetDisplay().GetSizeInPixel(), expected_size_in_pixels);
  gfx::Rect scaled_work_area(scaled_bounds);
  scaled_work_area.Inset(expected_inset);
  EXPECT_EQ(observer.GetDisplay().work_area(), scaled_work_area);

  // Test rotation.
  output_->SetTransform(WL_OUTPUT_TRANSFORM_90);
  output_->Flush();

  Sync();

  changed_values = display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
                   display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                   display::DisplayObserver::DISPLAY_METRIC_ROTATION;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  // Logical bounds should now be rotated to portrait.
  const gfx::Rect rotated_bounds{300, 400};
  EXPECT_EQ(observer.GetDisplay().bounds(), rotated_bounds);
  // Size in pixel gets rotated too, but stays unscaled.
  const gfx::Size rotated_size_in_pixels{600, 800};
  EXPECT_EQ(observer.GetDisplay().GetSizeInPixel(), rotated_size_in_pixels);
  gfx::Rect rotated_work_area(rotated_bounds);
  rotated_work_area.Inset(expected_inset);
  EXPECT_EQ(observer.GetDisplay().work_area(), rotated_work_area);
  EXPECT_EQ(observer.GetDisplay().panel_rotation(),
            display::Display::Rotation::ROTATE_270);
  EXPECT_EQ(observer.GetDisplay().rotation(),
            display::Display::Rotation::ROTATE_270);

  platform_screen_->RemoveObserver(&observer);
}

// Regression test for crbug.com/1310981.
// Some devices use display panels built in portrait orientation, but are used
// in landscape orientation. Thus their physical bounds are in portrait
// orientation along with an offset transform, which differs from the usual
// landscape oriented bounds.
TEST_P(WaylandAuraShellScreenTest,
       OutputPropertyChangesWithPortraitPanelRotation) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  // wl_output.geometry origin is set in DIP screen coordinates.
  const gfx::Point origin(50, 70);
  // wl_output.mode size is sent in physical coordinates, so it has portrait
  // dimensions for a display panel with portrait natural orientation.
  const gfx::Size physical_size(1200, 1600);
  output_->SetRect({origin, physical_size});

  // Inset is sent in logical coordinates.
  const gfx::Insets insets = gfx::Insets::TLBR(10, 20, 30, 40);
  ASSERT_TRUE(output_->GetAuraOutput());
  output_->GetAuraOutput()->SetInsets(insets);

  // Display panel's natural orientation is in portrait, so it needs a transform
  // of 90 degrees to be in landscape.
  output_->SetTransform(WL_OUTPUT_TRANSFORM_90);
  // Begin with the logical transform at 0 degrees.
  output_->GetAuraOutput()->SetLogicalTransform(WL_OUTPUT_TRANSFORM_NORMAL);

  output_->SetScale(2);
  output_->Flush();

  Sync();

  uint32_t changed_values =
      display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
      display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
      display::DisplayObserver::DISPLAY_METRIC_ROTATION;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);

  // Logical bounds should be in landscape.
  const gfx::Rect expected_bounds(origin, gfx::Size(800, 600));
  EXPECT_EQ(observer.GetDisplay().bounds(), expected_bounds);
  const gfx::Size expected_size_in_pixels(1600, 1200);
  EXPECT_EQ(observer.GetDisplay().GetSizeInPixel(), expected_size_in_pixels);

  gfx::Rect expected_work_area(expected_bounds);
  expected_work_area.Inset(insets);
  EXPECT_EQ(observer.GetDisplay().work_area(), expected_work_area);

  // Panel rotation and display rotation should have an offset.
  EXPECT_EQ(observer.GetDisplay().panel_rotation(),
            display::Display::Rotation::ROTATE_270);
  EXPECT_EQ(observer.GetDisplay().rotation(),
            display::Display::Rotation::ROTATE_0);

  // Further rotate the display to logical portrait orientation, which is 180
  // with the natural orientation offset.
  output_->SetTransform(WL_OUTPUT_TRANSFORM_180);
  output_->GetAuraOutput()->SetLogicalTransform(WL_OUTPUT_TRANSFORM_90);
  output_->Flush();

  Sync();

  changed_values = display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                   display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
                   display::DisplayObserver::DISPLAY_METRIC_ROTATION;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);

  // Logical bounds should now be portrait.
  const gfx::Rect portrait_bounds(origin, gfx::Size(600, 800));
  EXPECT_EQ(observer.GetDisplay().bounds(), portrait_bounds);
  const gfx::Size portrait_size_in_pixels(1200, 1600);
  EXPECT_EQ(observer.GetDisplay().GetSizeInPixel(), portrait_size_in_pixels);

  gfx::Rect portrait_work_area(portrait_bounds);
  portrait_work_area.Inset(insets);
  EXPECT_EQ(observer.GetDisplay().work_area(), portrait_work_area);

  // Panel rotation and display rotation should still have an offset.
  EXPECT_EQ(observer.GetDisplay().panel_rotation(),
            display::Display::Rotation::ROTATE_180);
  EXPECT_EQ(observer.GetDisplay().rotation(),
            display::Display::Rotation::ROTATE_270);

  platform_screen_->RemoveObserver(&observer);
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandScreenTest,
                         Values(wl::ServerConfig{}));

INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithAuraShell,
    WaylandScreenTest,
    Values(wl::ServerConfig{
        .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled}));

INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTest,
    WaylandAuraShellScreenTest,
    Values(wl::ServerConfig{
        .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled}));

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         LazilyConfiguredScreenTest,
                         Values(wl::ServerConfig{}));

}  // namespace ui
