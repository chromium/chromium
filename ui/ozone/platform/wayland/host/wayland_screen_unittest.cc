// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server.h>
#include <memory>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_observer.h"
#include "ui/display/display_switches.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

constexpr uint32_t kNumberOfDisplays = 1;
constexpr uint32_t kOutputWidth = 1024;
constexpr uint32_t kOutputHeight = 768;

class TestDisplayObserver : public display::DisplayObserver {
 public:
  TestDisplayObserver() {}
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

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    changed_metrics_ = changed_metrics;
    display_ = display;
  }

 private:
  uint32_t changed_metrics_ = 0;
  display::Display display_;
  display::Display removed_display_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayObserver);
};

}  // namespace

class WaylandScreenTest : public WaylandTest {
 public:
  WaylandScreenTest() {}
  ~WaylandScreenTest() override {}

  void SetUp() override {
    output_ = server_.output();
    output_->SetRect(gfx::Rect(0, 0, kOutputWidth, kOutputHeight));

    WaylandTest::SetUp();

    output_manager_ = connection_->wayland_output_manager();
    ASSERT_TRUE(output_manager_);

    EXPECT_TRUE(output_manager_->IsOutputReady());
    platform_screen_ = output_manager_->CreateWaylandScreen(connection_.get());
  }

 protected:
  std::unique_ptr<WaylandWindow> CreateWaylandWindowWithProperties(
      const gfx::Rect& bounds,
      PlatformWindowType window_type,
      gfx::AcceleratedWidget parent_widget,
      MockPlatformWindowDelegate* delegate) {
    auto window = std::make_unique<WaylandWindow>(delegate, connection_.get());
    PlatformWindowInitProperties properties;
    properties.bounds = bounds;
    properties.type = window_type;
    properties.parent_widget = parent_widget;
    EXPECT_TRUE(window->Initialize(std::move(properties)));
    return window;
  }

  void UpdateOutputGeometry(wl_resource* output_resource,
                            const gfx::Rect& new_rect) {
    wl_output_send_geometry(output_resource, new_rect.x(), new_rect.y(),
                            0 /* physical_width */, 0 /* physical_height */,
                            0 /* subpixel */, "unknown_make", "unknown_model",
                            0 /* transform */);
    wl_output_send_mode(output_resource, WL_OUTPUT_MODE_CURRENT,
                        new_rect.width(), new_rect.height(), 0 /* refresh */);
    wl_output_send_done(output_resource);
  }

  void ValidateTheDisplayForWidget(gfx::AcceleratedWidget widget,
                                   int64_t expected_display_id) {
    display::Display display_for_widget =
        platform_screen_->GetDisplayForAcceleratedWidget(widget);
    EXPECT_EQ(display_for_widget.id(), expected_display_id);
  }

  wl::TestOutput* output_ = nullptr;
  WaylandOutputManager* output_manager_ = nullptr;

  std::unique_ptr<WaylandScreen> platform_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandScreenTest);
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

TEST_P(WaylandScreenTest, MultipleOutputsAddedAndRemoved) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const int64_t old_primary_display_id =
      platform_screen_->GetPrimaryDisplay().id();

  // Add a second display.
  wl::TestOutput* output2 = server_.CreateAndInitializeOutput();

  Sync();

  // Update rect of that display.
  gfx::Rect output1_rect = server_.output()->GetRect();
  gfx::Rect output2_rect(output1_rect.width(), 0, 800, 600);
  // The second display is located to the right of first display like
  // | || |.
  UpdateOutputGeometry(output2->resource(), output2_rect);

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
  UpdateOutputGeometry(output2->resource(), output2_rect);

  Sync();

  // The newly added display is not a primary yet.
  added_display_id = observer.GetDisplay().id();
  EXPECT_NE(platform_screen_->GetPrimaryDisplay().id(), added_display_id);

  // Now, rearrange displays so that second display becomes a primary one.
  output1_rect = gfx::Rect(1024, 0, 1024, 768);
  output2_rect = gfx::Rect(0, 0, 1024, 768);
  UpdateOutputGeometry(server_.output()->resource(), output1_rect);
  UpdateOutputGeometry(output2->resource(), output2_rect);

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

TEST_P(WaylandScreenTest, OutputPropertyChanges) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const gfx::Rect new_rect(0, 0, 800, 600);
  UpdateOutputGeometry(output_->resource(), new_rect);

  Sync();

  uint32_t changed_values = 0;
  changed_values |= display::DisplayObserver::DISPLAY_METRIC_BOUNDS;
  changed_values |= display::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  EXPECT_EQ(observer.GetDisplay().bounds(), new_rect);

  const int32_t new_scale_value = 2;
  output_->SetScale(new_scale_value);

  Sync();

  changed_values = 0;
  changed_values |=
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  EXPECT_EQ(observer.GetDisplay().device_scale_factor(), new_scale_value);

  platform_screen_->RemoveObserver(&observer);
}

TEST_P(WaylandScreenTest, GetAcceleratedWidgetAtScreenPoint) {
  // If there is no focused window (focus is set whenever a pointer enters any
  // of the windows), there must be kNullAcceleratedWidget returned. There is no
  // real way to determine what window is located on a certain screen point in
  // Wayland.
  gfx::AcceleratedWidget widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(10, 10));
  EXPECT_EQ(widget_at_screen_point, gfx::kNullAcceleratedWidget);

  // Set a focus to the main window. Now, that focused window must be returned.
  window_->set_pointer_focus(true);
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(10, 10));
  EXPECT_EQ(widget_at_screen_point, window_->GetWidget());

  // Getting a widget at a screen point outside its bounds, must result in a
  // null widget.
  const gfx::Rect window_bounds = window_->GetBounds();
  widget_at_screen_point = platform_screen_->GetAcceleratedWidgetAtScreenPoint(
      gfx::Point(window_bounds.width() + 1, window_bounds.height() + 1));
  EXPECT_EQ(widget_at_screen_point, gfx::kNullAcceleratedWidget);

  MockPlatformWindowDelegate delegate;
  std::unique_ptr<WaylandWindow> menu_window =
      CreateWaylandWindowWithProperties(
          gfx::Rect(window_->GetBounds().width() - 10,
                    window_->GetBounds().height() - 10, 100, 100),
          PlatformWindowType::kPopup, window_->GetWidget(), &delegate);

  Sync();

  // Imagine the mouse enters a menu window, which is located on top of the main
  // window, and gathers focus.
  window_->set_pointer_focus(false);
  menu_window->set_pointer_focus(true);
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(
          menu_window->GetBounds().x() + 1, menu_window->GetBounds().y() + 1));
  EXPECT_EQ(widget_at_screen_point, menu_window->GetWidget());

  // Whenever a mouse pointer leaves the menu window, the accelerated widget
  // of that focused window must be returned.
  window_->set_pointer_focus(true);
  menu_window->set_pointer_focus(false);
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(0, 0));
  EXPECT_EQ(widget_at_screen_point, window_->GetWidget());

  // Reset the focus to avoid crash on dtor as long as there is no real pointer
  // object.
  window_->set_pointer_focus(false);
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
  UpdateOutputGeometry(output2->resource(), output2_rect);

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
  UpdateOutputGeometry(
      output2->resource(),
      gfx::Rect(gfx::Point(output2_rect.x(), output2_rect.y() + 700),
                output2_rect.size()));

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
  UpdateOutputGeometry(output2->resource(), output2_rect);

  Sync();

  const display::Display secondary_display = observer.GetDisplay();
  EXPECT_EQ(secondary_display.bounds(), output2_rect);

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  // There must be a primary display used if the window has not received an
  // enter event yet.
  ValidateTheDisplayForWidget(widget, primary_display.id());

  // Now, send enter event for the surface, which was created before.
  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(widget);
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
  // WaylandWindow::RemoveEnteredOutputId), must be ok and nothing must change.
  wl_surface_send_leave(surface->resource(), output_->resource());

  Sync();

  // The id of the entered display must correspond to the second output.
  ValidateTheDisplayForWidget(widget, secondary_display.id());
}

TEST_P(WaylandScreenTest, GetCursorScreenPoint) {
  MockPlatformWindowDelegate delegate;
  std::unique_ptr<WaylandWindow> second_window =
      CreateWaylandWindowWithProperties(gfx::Rect(0, 0, 1920, 1080),
                                        PlatformWindowType::kWindow,
                                        gfx::kNullAcceleratedWidget, &delegate);

  auto* surface = server_.GetObject<wl::MockSurface>(window_->GetWidget());
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
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(10),
                         wl_fixed_from_int(20));

  Sync();

  // WaylandScreen must return the last pointer location.
  EXPECT_EQ(gfx::Point(10, 20), platform_screen_->GetCursorScreenPoint());

  auto* second_surface =
      server_.GetObject<wl::MockSurface>(second_window->GetWidget());
  ASSERT_TRUE(second_surface);
  // Now, leave the first surface and enter second one.
  wl_pointer_send_leave(pointer->resource(), ++serial, surface->resource());
  wl_pointer_send_enter(pointer->resource(), ++serial,
                        second_surface->resource(), 0, 0);
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(20),
                         wl_fixed_from_int(10));

  Sync();

  // WaylandScreen must return the last pointer location.
  EXPECT_EQ(gfx::Point(20, 10), platform_screen_->GetCursorScreenPoint());

  // Clear pointer focus.
  wl_pointer_send_leave(pointer->resource(), ++serial,
                        second_surface->resource());

  Sync();

  // WaylandScreen must return a point, which is located outside of bounds of
  // any window. Basically, it means that it takes the largest window and adds
  // 10 pixels to its width and height, and returns the value.
  const gfx::Rect second_window_bounds = second_window->GetBounds();
  // A second window has largest bounds. Thus, these bounds must be taken as a
  // ground for the point outside any of the surfaces.
  ASSERT_TRUE(window_->GetBounds() < second_window_bounds);
  EXPECT_EQ(gfx::Point(second_window_bounds.width() + 10,
                       second_window_bounds.height() + 10),
            platform_screen_->GetCursorScreenPoint());

  // Create a menu window now and ensure cursor position is always sent in
  // regards to that window bounds.
  std::unique_ptr<WaylandWindow> menu_window =
      CreateWaylandWindowWithProperties(
          gfx::Rect(second_window_bounds.width() - 10,
                    second_window_bounds.height() - 10, 10, 20),
          PlatformWindowType::kPopup, second_window->GetWidget(), &delegate);

  Sync();

  auto* menu_surface =
      server_.GetObject<wl::MockSurface>(menu_window->GetWidget());
  ASSERT_TRUE(menu_surface);

  wl_pointer_send_enter(pointer->resource(), ++serial, menu_surface->resource(),
                        0, 0);
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(2),
                         wl_fixed_from_int(1));

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
  wl_pointer_send_enter(pointer->resource(), ++serial,
                        second_surface->resource(), 0, 0);
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(1912),
                         wl_fixed_from_int(1071));

  Sync();

  // WaylandWindow::UpdateCursorPositionFromEvent mustn't convert this point,
  // because it has already been located on the top-level window.
  EXPECT_EQ(gfx::Point(1912, 1071), platform_screen_->GetCursorScreenPoint());

  wl_pointer_send_leave(pointer->resource(), ++serial,
                        second_surface->resource());

  // Now, create a nested menu window and make sure that the cursor screen point
  // still has been correct. The location of the window is on the right side of
  // the main menu window.
  const gfx::Rect menu_window_bounds = menu_window->GetBounds();
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithProperties(
          gfx::Rect(menu_window_bounds.x() + menu_window_bounds.width(),
                    menu_window_bounds.y() + 2, 10, 20),
          PlatformWindowType::kPopup, second_window->GetWidget(), &delegate);

  Sync();

  auto* nested_menu_surface =
      server_.GetObject<wl::MockSurface>(nested_menu_window->GetWidget());
  ASSERT_TRUE(nested_menu_surface);

  wl_pointer_send_enter(pointer->resource(), ++serial,
                        nested_menu_surface->resource(), 0, 0);
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(2),
                         wl_fixed_from_int(3));

  Sync();

  EXPECT_EQ(gfx::Point(1922, 1075), platform_screen_->GetCursorScreenPoint());

  // Leave the nested surface and enter main menu surface. The cursor screen
  // point still must be reported correctly.
  wl_pointer_send_leave(pointer->resource(), ++serial,
                        nested_menu_surface->resource());
  wl_pointer_send_enter(pointer->resource(), ++serial, menu_surface->resource(),
                        0, 0);
  wl_pointer_send_motion(pointer->resource(), ++time, wl_fixed_from_int(2),
                         wl_fixed_from_int(1));

  Sync();

  EXPECT_EQ(gfx::Point(1912, 1071), platform_screen_->GetCursorScreenPoint());
}

// Checks that the surface that backs the window receives new scale of the
// output that it is in.
TEST_P(WaylandScreenTest, SetBufferScale) {
  // Place the window onto the output.
  wl_surface_send_enter(surface_->resource(), output_->resource());

  // Change the scale of the output.  Windows looking into that output must get
  // the new scale and update scale of their buffers.  The default UI scale
  // equals the output scale.
  const int32_t kTripleScale = 3;
  EXPECT_CALL(*surface_, SetBufferScale(kTripleScale));
  output_->SetScale(kTripleScale);

  Sync();

  EXPECT_EQ(window_->buffer_scale(), kTripleScale);
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
  EXPECT_CALL(*surface_, SetBufferScale(kDoubleScale));
  output_->SetScale(kDoubleScale);

  Sync();

  EXPECT_EQ(window_->buffer_scale(), kDoubleScale);
  EXPECT_EQ(window_->ui_scale_, kForcedUIScale);

  display::Display::ResetForceDeviceScaleFactorForTesting();
}

INSTANTIATE_TEST_SUITE_P(XdgVersionV5Test,
                         WaylandScreenTest,
                         ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandScreenTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
