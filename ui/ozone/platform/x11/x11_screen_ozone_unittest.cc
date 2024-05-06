// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include <memory>

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/x11_display_manager.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/font_render_params.h"
#include "ui/ozone/platform/x11/x11_window.h"
#include "ui/ozone/platform/x11/x11_window_manager.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

using ::testing::_;

namespace ui {

namespace {

constexpr gfx::Rect kPrimaryDisplayBounds(0, 0, 800, 600);

constexpr int64_t kFirstDisplay = 5321829;
constexpr int64_t kSecondDisplay = 928310;

ACTION_P(StoreWidget, widget_ptr) {
  if (widget_ptr)
    *widget_ptr = arg0;
}

int64_t NextDisplayId() {
  static int64_t next_id = 0;
  return next_id++;
}

struct MockDisplayObserver : public display::DisplayObserver {
  MockDisplayObserver() = default;
  ~MockDisplayObserver() override = default;

  MOCK_METHOD1(OnDisplayAdded, void(const display::Display& new_display));
  MOCK_METHOD1(OnDisplaysRemoved,
               void(const display::Displays& removed_displays));
  MOCK_METHOD2(OnDisplayMetricsChanged,
               void(const display::Display& display, uint32_t changed_metrics));
};

}  // namespace

class X11ScreenOzoneTest : public testing::Test {
 public:
  X11ScreenOzoneTest()
      : task_env_(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI)) {}

  X11ScreenOzoneTest(const X11ScreenOzoneTest&) = delete;
  X11ScreenOzoneTest& operator=(const X11ScreenOzoneTest&) = delete;

  ~X11ScreenOzoneTest() override = default;

  void SetUp() override {
    auto* connection = x11::Connection::Get();
    event_source_ = std::make_unique<X11EventSource>(connection);
    primary_display_ = std::make_unique<display::Display>(
        NextDisplayId(), kPrimaryDisplayBounds);
    screen_ = std::make_unique<X11ScreenOzone>();
    UpdateDisplayListForTest({*primary_display_});
    screen_->AddObserver(&display_observer_);
  }

 protected:
  X11ScreenOzone* screen() const { return screen_.get(); }
  const display::Display& primary_display() const { return *primary_display_; }

  std::unique_ptr<display::Display> CreateDisplay(gfx::Rect bounds) const {
    return std::make_unique<display::Display>(NextDisplayId(), bounds);
  }

  void AddDisplayForTest(const display::Display& display) {
    auto display_list = screen_->GetAllDisplays();
    std::vector<display::Display> new_displays(display_list);
    new_displays.push_back(display);
    UpdateDisplayListForTest(std::move(new_displays));
  }

  void RemoveDisplayForTest(const display::Display& display_to_remove) {
    auto display_list = screen_->GetAllDisplays();
    std::vector<display::Display> new_displays(display_list.size() - 1);
    std::remove_copy(display_list.begin(), display_list.end(),
                     new_displays.begin(), display_to_remove);
    UpdateDisplayListForTest(std::move(new_displays));
  }

  void UpdateDisplayListForTest(std::vector<display::Display> displays) {
    ui::XDisplayManager* manager = screen_->x11_display_manager_.get();
    std::vector<display::Display> old_displays = std::move(manager->displays_);
    manager->SetDisplayList(std::move(displays), 0);
    manager->change_notifier_.NotifyDisplaysChanged(old_displays,
                                                    manager->displays_);
  }

  std::unique_ptr<X11Window> CreatePlatformWindow(
      MockPlatformWindowDelegate* delegate,
      const gfx::Rect& bounds,
      gfx::AcceleratedWidget* widget = nullptr) {
    EXPECT_CALL(*delegate, OnAcceleratedWidgetAvailable(_))
        .WillOnce(StoreWidget(widget));
    PlatformWindowInitProperties init_params(bounds);
    auto window = std::make_unique<X11Window>(delegate);
    window->Initialize(std::move(init_params));
    return window;
  }

  MockDisplayObserver display_observer_;

 private:
  std::unique_ptr<display::Display> primary_display_;
  std::unique_ptr<X11ScreenOzone> screen_;
  std::unique_ptr<X11EventSource> event_source_;
  std::unique_ptr<base::test::TaskEnvironment> task_env_;
};

// This test case ensures that PlatformScreen correctly provides the display
// list as they are added/removed.
TEST_F(X11ScreenOzoneTest, AddRemoveListDisplays) {
  // Initially only primary display is expected to be in place
  EXPECT_EQ(1u, screen()->GetAllDisplays().size());
  EXPECT_CALL(display_observer_, OnDisplayAdded(_)).Times(2);
  EXPECT_CALL(display_observer_, OnDisplaysRemoved(_)).Times(2);

  auto display_2 = CreateDisplay(gfx::Rect(800, 0, 1280, 720));
  AddDisplayForTest(*display_2);
  EXPECT_EQ(2u, screen()->GetAllDisplays().size());

  auto display_3 = CreateDisplay(gfx::Rect(0, 720, 800, 600));
  AddDisplayForTest(*display_3);
  EXPECT_EQ(3u, screen()->GetAllDisplays().size());

  RemoveDisplayForTest(*display_3);
  EXPECT_EQ(2u, screen()->GetAllDisplays().size());
  RemoveDisplayForTest(*display_2);
  EXPECT_EQ(1u, screen()->GetAllDisplays().size());
}

// This test case exercises GetDisplayForAcceleratedWidget when simple cases
// for platform windows in a single-display setup.
TEST_F(X11ScreenOzoneTest, GetDisplayForWidgetSingleDisplay) {
  auto primary = primary_display();
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect bounds(100, 100, 400, 300);
  auto window = CreatePlatformWindow(&delegate, bounds, &widget);
  EXPECT_EQ(primary, screen()->GetDisplayForAcceleratedWidget(widget));
  EXPECT_EQ(primary, screen()->GetDisplayForAcceleratedWidget(
                         gfx::kNullAcceleratedWidget));

  MockPlatformWindowDelegate delegate_1;
  gfx::AcceleratedWidget widget_1;
  constexpr gfx::Rect bounds_1(kPrimaryDisplayBounds.width() + 100,
                               kPrimaryDisplayBounds.height() + 100, 200, 200);
  auto window_1 = CreatePlatformWindow(&delegate_1, bounds_1, &widget_1);
  EXPECT_EQ(primary, screen()->GetDisplayForAcceleratedWidget(widget_1));
}

// This test case exercises GetDisplayForAcceleratedWidget when simple cases
// for platform windows in a 2 side-by-side displays setup.
TEST_F(X11ScreenOzoneTest, GetDisplayForWidgetTwoDisplays) {
  auto display_2 =
      CreateDisplay(gfx::Rect(kPrimaryDisplayBounds.width(), 0, 1280, 720));
  AddDisplayForTest(*display_2);

  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect bounds(kPrimaryDisplayBounds.width() + 10, 100, 400, 300);
  auto window = CreatePlatformWindow(&delegate, bounds, &widget);
  EXPECT_EQ(*display_2, screen()->GetDisplayForAcceleratedWidget(widget));

  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(1);
  window->SetBoundsInPixels(
      gfx::Rect(kPrimaryDisplayBounds.width() - 250, 0, 400, 300));
  EXPECT_EQ(primary_display(),
            screen()->GetDisplayForAcceleratedWidget(widget));
}

// This test case exercises GetDisplayNearestPoint function simulating 2
// side-by-side displays setup.
TEST_F(X11ScreenOzoneTest, GetDisplayNearestPointTwoDisplays) {
  auto display_2 =
      CreateDisplay(gfx::Rect(kPrimaryDisplayBounds.width(), 0, 1280, 720));
  AddDisplayForTest(*display_2);

  EXPECT_EQ(primary_display(),
            screen()->GetDisplayNearestPoint(gfx::Point(10, 10)));
  EXPECT_EQ(primary_display(),
            screen()->GetDisplayNearestPoint(gfx::Point(790, 100)));
  EXPECT_EQ(*display_2, screen()->GetDisplayNearestPoint(gfx::Point(1000, 10)));
  EXPECT_EQ(*display_2,
            screen()->GetDisplayNearestPoint(gfx::Point(10000, 10000)));
}

// This test case exercises GetDisplayMatching function with both single and
// side-by-side display setup
TEST_F(X11ScreenOzoneTest, GetDisplayMatchingMultiple) {
  auto primary = primary_display();
  EXPECT_EQ(primary, screen()->GetDisplayMatching(gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(primary,
            screen()->GetDisplayMatching(gfx::Rect(1000, 600, 100, 100)));

  auto second =
      CreateDisplay(gfx::Rect(kPrimaryDisplayBounds.width(), 0, 1280, 720));
  AddDisplayForTest(*second);
  EXPECT_EQ(primary, screen()->GetDisplayMatching(gfx::Rect(50, 50, 100, 100)));
  EXPECT_EQ(*second,
            screen()->GetDisplayMatching(gfx::Rect(1000, 100, 100, 100)));
  EXPECT_EQ(*second,
            screen()->GetDisplayMatching(gfx::Rect(1000, 600, 100, 100)));

  // Check rectangle overlapping 2 displays
  EXPECT_EQ(primary, screen()->GetDisplayMatching(gfx::Rect(740, 0, 100, 100)));
  EXPECT_EQ(*second,
            screen()->GetDisplayMatching(gfx::Rect(760, 100, 100, 100)));
}

TEST_F(X11ScreenOzoneTest, BoundsChangeSingleMonitor) {
  EXPECT_CALL(display_observer_, OnDisplayMetricsChanged(_, _)).Times(1);
  EXPECT_CALL(display_observer_, OnDisplayAdded(_)).Times(0);
  EXPECT_CALL(display_observer_, OnDisplaysRemoved(_)).Times(0);

  std::vector<display::Display> displays;
  displays.emplace_back(primary_display().id(), gfx::Rect(0, 0, 1024, 768));
  UpdateDisplayListForTest(displays);
}

TEST_F(X11ScreenOzoneTest, AddMonitorToTheRight) {
  EXPECT_CALL(display_observer_, OnDisplayMetricsChanged(_, _)).Times(0);
  EXPECT_CALL(display_observer_, OnDisplayAdded(_)).Times(1);
  EXPECT_CALL(display_observer_, OnDisplaysRemoved(_)).Times(0);

  std::vector<display::Display> displays;
  displays.emplace_back(primary_display().id(), kPrimaryDisplayBounds);
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);
}

TEST_F(X11ScreenOzoneTest, AddMonitorToTheLeft) {
  EXPECT_CALL(display_observer_, OnDisplayMetricsChanged(_, _)).Times(1);
  EXPECT_CALL(display_observer_, OnDisplayAdded(_)).Times(1);
  EXPECT_CALL(display_observer_, OnDisplaysRemoved(_)).Times(0);

  std::vector<display::Display> displays;
  displays.emplace_back(primary_display().id(), gfx::Rect(0, 0, 1024, 768));
  displays.emplace_back(kFirstDisplay, gfx::Rect(1024, 0, 640, 480));
  UpdateDisplayListForTest(displays);
}

TEST_F(X11ScreenOzoneTest, RemoveMonitorOnRight) {
  std::vector<display::Display> displays;
  displays.emplace_back(primary_display().id(), kPrimaryDisplayBounds);
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_CALL(display_observer_, OnDisplayMetricsChanged(_, _)).Times(0);
  EXPECT_CALL(display_observer_, OnDisplayAdded(_)).Times(0);
  EXPECT_CALL(display_observer_, OnDisplaysRemoved(_)).Times(1);

  displays.clear();
  displays.emplace_back(primary_display().id(), kPrimaryDisplayBounds);
  UpdateDisplayListForTest(displays);
}

TEST_F(X11ScreenOzoneTest, RemoveMonitorOnLeft) {
  std::vector<display::Display> displays;
  displays.emplace_back(primary_display().id(), kPrimaryDisplayBounds);
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_CALL(display_observer_, OnDisplayMetricsChanged(_, _)).Times(1);
  EXPECT_CALL(display_observer_, OnDisplayAdded(_)).Times(0);
  EXPECT_CALL(display_observer_, OnDisplaysRemoved(_)).Times(1);

  displays.clear();
  displays.emplace_back(kSecondDisplay, gfx::Rect(0, 0, 1024, 768));
  UpdateDisplayListForTest(displays);
}

TEST_F(X11ScreenOzoneTest, GetDisplayNearestPoint) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_EQ(kFirstDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(630, 10)).id());
  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(650, 10)).id());
  EXPECT_EQ(kFirstDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(10, 10)).id());
  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(10000, 10000)).id());
  EXPECT_EQ(kFirstDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(639, -10)).id());
  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(641, -20)).id());
  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(600, 760)).id());
  EXPECT_EQ(kFirstDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(-1000, 760)).id());
}

TEST_F(X11ScreenOzoneTest, GetDisplayMatchingBasic) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayMatching(gfx::Rect(700, 20, 100, 100)).id());
}

TEST_F(X11ScreenOzoneTest, GetDisplayMatchingOverlap) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayMatching(gfx::Rect(630, 20, 100, 100)).id());
}

TEST_F(X11ScreenOzoneTest, GetPrimaryDisplay) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(640, 0, 1024, 768));
  displays.emplace_back(kSecondDisplay, gfx::Rect(0, 0, 640, 480));
  UpdateDisplayListForTest(displays);

  // The first display in the list is always the primary, even if other
  // displays are to the left in screen layout.
  EXPECT_EQ(kFirstDisplay, screen()->GetPrimaryDisplay().id());
}

TEST_F(X11ScreenOzoneTest, GetDisplayNearestWindow) {
  // Set up a two monitor situation.
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  MockPlatformWindowDelegate delegate1;
  auto window_one = CreatePlatformWindow(&delegate1, gfx::Rect(10, 10, 10, 10));
  MockPlatformWindowDelegate delegate2;
  auto window_two =
      CreatePlatformWindow(&delegate2, gfx::Rect(650, 50, 10, 10));

  EXPECT_EQ(
      kFirstDisplay,
      screen()->GetDisplayForAcceleratedWidget(window_one->GetWidget()).id());
  EXPECT_EQ(
      kSecondDisplay,
      screen()->GetDisplayForAcceleratedWidget(window_two->GetWidget()).id());

  window_one->Close();
  window_two->Close();
}

// Test that rotating the displays notifies the DisplayObservers.
TEST_F(X11ScreenOzoneTest, RotationChange) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_CALL(display_observer_, OnDisplayMetricsChanged(_, _)).Times(5);
  EXPECT_CALL(display_observer_, OnDisplayAdded(_)).Times(0);
  EXPECT_CALL(display_observer_, OnDisplaysRemoved(_)).Times(0);

  displays[0].set_rotation(display::Display::ROTATE_90);
  UpdateDisplayListForTest(displays);

  displays[1].set_rotation(display::Display::ROTATE_90);
  UpdateDisplayListForTest(displays);

  displays[0].set_rotation(display::Display::ROTATE_270);
  UpdateDisplayListForTest(displays);

  displays[0].set_rotation(display::Display::ROTATE_270);
  UpdateDisplayListForTest(displays);

  displays[0].set_rotation(display::Display::ROTATE_0);
  displays[1].set_rotation(display::Display::ROTATE_0);
  UpdateDisplayListForTest(displays);
}

// Test that changing the displays workarea notifies the DisplayObservers.
TEST_F(X11ScreenOzoneTest, WorkareaChange) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_CALL(display_observer_, OnDisplayMetricsChanged(_, _)).Times(4);
  EXPECT_CALL(display_observer_, OnDisplayAdded(_)).Times(0);
  EXPECT_CALL(display_observer_, OnDisplaysRemoved(_)).Times(0);

  displays[0].set_work_area(gfx::Rect(0, 0, 300, 300));
  UpdateDisplayListForTest(displays);

  displays[1].set_work_area(gfx::Rect(0, 0, 300, 300));
  UpdateDisplayListForTest(displays);

  displays[0].set_work_area(gfx::Rect(0, 0, 300, 300));
  UpdateDisplayListForTest(displays);

  displays[1].set_work_area(gfx::Rect(0, 0, 300, 300));
  UpdateDisplayListForTest(displays);

  displays[0].set_work_area(gfx::Rect(0, 0, 640, 480));
  displays[1].set_work_area(gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);
}

// Test that changing the device scale factor notifies the DisplayObservers.
TEST_F(X11ScreenOzoneTest, DeviceScaleFactorChange) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_CALL(display_observer_, OnDisplayMetricsChanged(_, _)).Times(4);
  EXPECT_CALL(display_observer_, OnDisplayAdded(_)).Times(0);
  EXPECT_CALL(display_observer_, OnDisplaysRemoved(_)).Times(0);

  displays[0].set_device_scale_factor(2.5f);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(2.5f, gfx::GetFontRenderParamsDeviceScaleFactor());

  displays[1].set_device_scale_factor(2.5f);
  UpdateDisplayListForTest(displays);

  displays[0].set_device_scale_factor(2.5f);
  UpdateDisplayListForTest(displays);

  displays[1].set_device_scale_factor(2.5f);
  UpdateDisplayListForTest(displays);

  displays[0].set_device_scale_factor(1.f);
  displays[1].set_device_scale_factor(1.f);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(1.f, gfx::GetFontRenderParamsDeviceScaleFactor());
}

}  // namespace ui
