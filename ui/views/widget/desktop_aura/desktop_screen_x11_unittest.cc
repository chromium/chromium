// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/x/x11_display_manager.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

namespace {

// Class which allows for the designation of non-client component targets of
// hit tests.
class TestDesktopNativeWidgetAura : public views::DesktopNativeWidgetAura {
 public:
  explicit TestDesktopNativeWidgetAura(
      views::internal::NativeWidgetDelegate* delegate)
      : views::DesktopNativeWidgetAura(delegate) {}
  ~TestDesktopNativeWidgetAura() override = default;

  void set_window_component(int window_component) {
    window_component_ = window_component;
  }

  // DesktopNativeWidgetAura:
  int GetNonClientComponent(const gfx::Point& point) const override {
    return window_component_;
  }

 private:
  int window_component_;

  DISALLOW_COPY_AND_ASSIGN(TestDesktopNativeWidgetAura);
};

}  // namespace

namespace views {

constexpr int64_t kFirstDisplay = 5321829;
constexpr int64_t kSecondDisplay = 928310;

class DesktopScreenX11Test : public views::ViewsTestBase,
                             public display::DisplayObserver {
 public:
  DesktopScreenX11Test() = default;
  ~DesktopScreenX11Test() override = default;

  // Overridden from testing::Test:
  void SetUp() override {
    ViewsTestBase::SetUp();
    // Initialize the world to the single monitor case.
    std::vector<display::Display> displays;
    displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
    screen_ = std::make_unique<DesktopScreenX11>();
    UpdateDisplayListForTest(displays);
    screen_->AddObserver(this);
  }

  void TearDown() override {
    screen_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  std::vector<display::Display> changed_display_;
  std::vector<display::Display> added_display_;
  std::vector<display::Display> removed_display_;

  DesktopScreenX11* screen() { return screen_.get(); }

  void UpdateDisplayListForTest(std::vector<display::Display> displays) {
    ui::XDisplayManager* manager = screen_->x11_display_manager_.get();
    std::vector<display::Display> old_displays = std::move(manager->displays_);
    manager->SetDisplayList(std::move(displays));
    manager->change_notifier_.NotifyDisplaysChanged(old_displays,
                                                    manager->displays_);
  }

  void ResetDisplayChanges() {
    changed_display_.clear();
    added_display_.clear();
    removed_display_.clear();
  }

  Widget* BuildTopLevelDesktopWidget(const gfx::Rect& bounds,
                                     bool use_test_native_widget) {
    Widget* toplevel = new Widget;
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    if (use_test_native_widget) {
      toplevel_params.native_widget = new TestDesktopNativeWidgetAura(toplevel);
    } else {
      toplevel_params.native_widget =
          new views::DesktopNativeWidgetAura(toplevel);
    }
    toplevel_params.bounds = bounds;
    toplevel_params.remove_standard_frame = true;
    toplevel->Init(std::move(toplevel_params));
    return toplevel;
  }

 private:
  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    added_display_.push_back(new_display);
  }

  void OnDisplayRemoved(const display::Display& old_display) override {
    removed_display_.push_back(old_display);
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override {
    changed_display_.push_back(display);
  }

  std::unique_ptr<DesktopScreenX11> screen_;

  DISALLOW_COPY_AND_ASSIGN(DesktopScreenX11Test);
};

TEST_F(DesktopScreenX11Test, BoundsChangeSingleMonitor) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_EQ(1u, changed_display_.size());
  EXPECT_EQ(0u, added_display_.size());
  EXPECT_EQ(0u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, AddMonitorToTheRight) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_EQ(0u, changed_display_.size());
  EXPECT_EQ(1u, added_display_.size());
  EXPECT_EQ(0u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, AddMonitorToTheLeft) {
  std::vector<display::Display> displays;
  displays.emplace_back(kSecondDisplay, gfx::Rect(0, 0, 1024, 768));
  displays.emplace_back(kFirstDisplay, gfx::Rect(1024, 0, 640, 480));
  UpdateDisplayListForTest(displays);

  EXPECT_EQ(1u, changed_display_.size());
  EXPECT_EQ(1u, added_display_.size());
  EXPECT_EQ(0u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, RemoveMonitorOnRight) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  ResetDisplayChanges();

  displays.clear();
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  UpdateDisplayListForTest(displays);

  EXPECT_EQ(0u, changed_display_.size());
  EXPECT_EQ(0u, added_display_.size());
  EXPECT_EQ(1u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, RemoveMonitorOnLeft) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  ResetDisplayChanges();

  displays.clear();
  displays.emplace_back(kSecondDisplay, gfx::Rect(0, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_EQ(1u, changed_display_.size());
  EXPECT_EQ(0u, added_display_.size());
  EXPECT_EQ(1u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, GetDisplayNearestPoint) {
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

TEST_F(DesktopScreenX11Test, GetDisplayMatchingBasic) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayMatching(gfx::Rect(700, 20, 100, 100)).id());
}

TEST_F(DesktopScreenX11Test, GetDisplayMatchingOverlap) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayMatching(gfx::Rect(630, 20, 100, 100)).id());
}

TEST_F(DesktopScreenX11Test, GetPrimaryDisplay) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(640, 0, 1024, 768));
  displays.emplace_back(kSecondDisplay, gfx::Rect(0, 0, 640, 480));
  UpdateDisplayListForTest(displays);

  // The first display in the list is always the primary, even if other
  // displays are to the left in screen layout.
  EXPECT_EQ(kFirstDisplay, screen()->GetPrimaryDisplay().id());
}

TEST_F(DesktopScreenX11Test, GetDisplayNearestWindow) {
  // Set up a two monitor situation.
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);

  Widget* window_one =
      BuildTopLevelDesktopWidget(gfx::Rect(10, 10, 10, 10), false);
  Widget* window_two =
      BuildTopLevelDesktopWidget(gfx::Rect(650, 50, 10, 10), false);

  EXPECT_EQ(
      kFirstDisplay,
      screen()->GetDisplayNearestWindow(window_one->GetNativeWindow()).id());
  EXPECT_EQ(
      kSecondDisplay,
      screen()->GetDisplayNearestWindow(window_two->GetNativeWindow()).id());

  window_one->CloseNow();
  window_two->CloseNow();
}

// Test that rotating the displays notifies the DisplayObservers.
TEST_F(DesktopScreenX11Test, RotationChange) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);
  ResetDisplayChanges();

  displays[0].set_rotation(display::Display::ROTATE_90);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(1u, changed_display_.size());

  displays[1].set_rotation(display::Display::ROTATE_90);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(2u, changed_display_.size());

  displays[0].set_rotation(display::Display::ROTATE_270);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(3u, changed_display_.size());

  displays[0].set_rotation(display::Display::ROTATE_270);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(3u, changed_display_.size());

  displays[0].set_rotation(display::Display::ROTATE_0);
  displays[1].set_rotation(display::Display::ROTATE_0);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(5u, changed_display_.size());
}

// Test that changing the displays workarea notifies the DisplayObservers.
TEST_F(DesktopScreenX11Test, WorkareaChange) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);
  ResetDisplayChanges();

  displays[0].set_work_area(gfx::Rect(0, 0, 300, 300));
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(1u, changed_display_.size());

  displays[1].set_work_area(gfx::Rect(0, 0, 300, 300));
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(2u, changed_display_.size());

  displays[0].set_work_area(gfx::Rect(0, 0, 300, 300));
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(2u, changed_display_.size());

  displays[1].set_work_area(gfx::Rect(0, 0, 300, 300));
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(2u, changed_display_.size());

  displays[0].set_work_area(gfx::Rect(0, 0, 640, 480));
  displays[1].set_work_area(gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(4u, changed_display_.size());
}

// Test that changing the device scale factor notifies the DisplayObservers.
TEST_F(DesktopScreenX11Test, DeviceScaleFactorChange) {
  std::vector<display::Display> displays;
  displays.emplace_back(kFirstDisplay, gfx::Rect(0, 0, 640, 480));
  displays.emplace_back(kSecondDisplay, gfx::Rect(640, 0, 1024, 768));
  UpdateDisplayListForTest(displays);
  ResetDisplayChanges();

  displays[0].set_device_scale_factor(2.5f);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(1u, changed_display_.size());
  EXPECT_EQ(2.5f, gfx::GetFontRenderParamsDeviceScaleFactor());

  displays[1].set_device_scale_factor(2.5f);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(2u, changed_display_.size());

  displays[0].set_device_scale_factor(2.5f);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(2u, changed_display_.size());

  displays[1].set_device_scale_factor(2.5f);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(2u, changed_display_.size());

  displays[0].set_device_scale_factor(1.f);
  displays[1].set_device_scale_factor(1.f);
  UpdateDisplayListForTest(displays);
  EXPECT_EQ(4u, changed_display_.size());
  EXPECT_EQ(1.f, gfx::GetFontRenderParamsDeviceScaleFactor());
}

}  // namespace views
