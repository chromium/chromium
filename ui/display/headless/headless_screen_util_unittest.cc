// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/headless/headless_screen_util.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_list.h"
#include "ui/display/display_observer.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace display {
namespace {

// HeadlessScreenManager::SetDisplayGeometry tests ----------------------------

class HeadlessDisplayGeometryTest : public ::testing::Test {
 public:
  HeadlessDisplayGeometryTest() = default;
  ~HeadlessDisplayGeometryTest() override = default;

  HeadlessDisplayGeometryTest(const HeadlessDisplayGeometryTest&) = delete;
  HeadlessDisplayGeometryTest& operator=(const HeadlessDisplayGeometryTest&) =
      delete;

 protected:
  void SetDisplayGeometry(const gfx::Rect& bounds_in_pixels,
                          const gfx::Insets& work_area_insets_pixels,
                          float device_pixel_ratio) {
    headless::SetDisplayGeometry(display_, bounds_in_pixels,
                                 work_area_insets_pixels, device_pixel_ratio);
  }

  Display display_;
};

TEST_F(HeadlessDisplayGeometryTest, DefaultDisplay) {
  SetDisplayGeometry(gfx::Rect(0, 0, 800, 600), gfx::Insets(), 1.0f);

  EXPECT_EQ(display_.bounds(), gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ(display_.work_area(), gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ(display_.device_scale_factor(), 1.0f);
}

TEST_F(HeadlessDisplayGeometryTest, DefaultDisplayWorkArea) {
  SetDisplayGeometry(gfx::Rect(0, 0, 800, 600), gfx::Insets(10), 1.0f);

  EXPECT_EQ(display_.bounds(), gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ(display_.work_area(), gfx::Rect(10, 10, 780, 580));
  EXPECT_EQ(display_.device_scale_factor(), 1.0f);
}

TEST_F(HeadlessDisplayGeometryTest, Scaled2xDisplay) {
  SetDisplayGeometry(gfx::Rect(100, 100, 1600, 1200), gfx::Insets(), 2.0f);

  EXPECT_EQ(display_.bounds(), gfx::Rect(100, 100, 800, 600));
  EXPECT_EQ(display_.work_area(), gfx::Rect(100, 100, 800, 600));
  EXPECT_EQ(display_.device_scale_factor(), 2.0f);
}

TEST_F(HeadlessDisplayGeometryTest, Scaled2xDisplayWorkArea) {
  SetDisplayGeometry(gfx::Rect(100, 100, 1600, 1200), gfx::Insets(10), 2.0f);

  EXPECT_EQ(display_.bounds(), gfx::Rect(100, 100, 800, 600));
  EXPECT_EQ(display_.work_area(), gfx::Rect(105, 105, 790, 590));
  EXPECT_EQ(display_.device_scale_factor(), 2.0f);
}

// headless::UpdateDisplay tests ----------------------------------------------

class HeadlessUpdateDisplayTest : public ::testing::Test {
 public:
  HeadlessUpdateDisplayTest() = default;
  ~HeadlessUpdateDisplayTest() override = default;

  HeadlessUpdateDisplayTest(const HeadlessUpdateDisplayTest&) = delete;
  HeadlessUpdateDisplayTest& operator=(const HeadlessUpdateDisplayTest&) =
      delete;

 protected:
  // NOLINTNEXTLINE(google-explicit-constructor)
  struct UpdateDisplayParams {
    std::optional<int> left;
    std::optional<int> top;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> top_work_area_inset;
    std::optional<int> left_work_area_inset;
    std::optional<int> bottom_work_area_inset;
    std::optional<int> right_work_area_inset;
    std::optional<double> device_pixel_ratio;
    std::optional<int> rotation;
    std::optional<int> color_depth;
    std::optional<std::string> label;
    std::optional<bool> is_internal;
  };

  void UpdateDisplay(const UpdateDisplayParams& params) {
    headless::UpdateDisplay(
        display_, params.left, params.top, params.width, params.height,
        params.top_work_area_inset, params.left_work_area_inset,
        params.bottom_work_area_inset, params.right_work_area_inset,
        params.device_pixel_ratio, params.rotation, params.color_depth,
        params.label, params.is_internal);
  }

  int64_t display_id() const { return display_.id(); }
  const gfx::Rect& bounds() const { return display_.bounds(); }
  const gfx::Rect& work_area() const { return display_.work_area(); }
  Display::Rotation rotation() const { return display_.rotation(); }
  int color_depth() const { return display_.color_depth(); }
  const std::string& label() const { return display_.label(); }

  static constexpr gfx::Rect kDefaultBounds = gfx::Rect(0, 0, 800, 600);
  Display display_ = Display(1L, kDefaultBounds);
};

TEST_F(HeadlessUpdateDisplayTest, Bounds) {
  UpdateDisplay({.left = 10});
  EXPECT_EQ(bounds(), gfx::Rect(10, 0, 800, 600));
  EXPECT_EQ(work_area(), gfx::Rect(10, 0, 800, 600));

  UpdateDisplay({.top = 20});
  EXPECT_EQ(bounds(), gfx::Rect(10, 20, 800, 600));
  EXPECT_EQ(work_area(), gfx::Rect(10, 20, 800, 600));

  UpdateDisplay({.width = 1000});
  EXPECT_EQ(bounds(), gfx::Rect(10, 20, 1000, 600));
  EXPECT_EQ(work_area(), gfx::Rect(10, 20, 1000, 600));

  UpdateDisplay({.height = 2000});
  EXPECT_EQ(bounds(), gfx::Rect(10, 20, 1000, 2000));
  EXPECT_EQ(work_area(), gfx::Rect(10, 20, 1000, 2000));
}

TEST_F(HeadlessUpdateDisplayTest, BoundsScaled) {
  display_.SetScaleAndBounds(2.0f, kDefaultBounds);
  ASSERT_EQ(bounds(), gfx::Rect(0, 0, 400, 300));
  ASSERT_EQ(work_area(), gfx::Rect(0, 0, 400, 300));

  UpdateDisplay({.left = 10});
  EXPECT_EQ(bounds(), gfx::Rect(10, 0, 400, 300));
  EXPECT_EQ(work_area(), gfx::Rect(10, 0, 400, 300));

  UpdateDisplay({.top = 20});
  EXPECT_EQ(bounds(), gfx::Rect(10, 20, 400, 300));
  EXPECT_EQ(work_area(), gfx::Rect(10, 20, 400, 300));

  UpdateDisplay({.width = 1000});
  EXPECT_EQ(bounds(), gfx::Rect(10, 20, 500, 300));
  EXPECT_EQ(work_area(), gfx::Rect(10, 20, 500, 300));

  UpdateDisplay({.height = 2000});
  EXPECT_EQ(bounds(), gfx::Rect(10, 20, 500, 1000));
  EXPECT_EQ(work_area(), gfx::Rect(10, 20, 500, 1000));
}

TEST_F(HeadlessUpdateDisplayTest, WorkArea) {
  UpdateDisplay({.top_work_area_inset = 10});
  EXPECT_EQ(bounds(), gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ(work_area(), gfx::Rect(0, 10, 800, 590));

  UpdateDisplay({.left_work_area_inset = 20});
  EXPECT_EQ(bounds(), gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ(work_area(), gfx::Rect(20, 10, 780, 590));

  UpdateDisplay({.bottom_work_area_inset = 10});
  EXPECT_EQ(bounds(), gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ(work_area(), gfx::Rect(20, 10, 780, 580));

  UpdateDisplay({.right_work_area_inset = 20});
  EXPECT_EQ(bounds(), gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ(work_area(), gfx::Rect(20, 10, 760, 580));
}

TEST_F(HeadlessUpdateDisplayTest, WorkAreaScaled) {
  display_.SetScaleAndBounds(2.0f, kDefaultBounds);
  ASSERT_EQ(bounds(), gfx::Rect(0, 0, 400, 300));
  ASSERT_EQ(work_area(), gfx::Rect(0, 0, 400, 300));

  UpdateDisplay({.top_work_area_inset = 10});
  EXPECT_EQ(bounds(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(work_area(), gfx::Rect(0, 5, 400, 295));

  UpdateDisplay({.left_work_area_inset = 20});
  EXPECT_EQ(bounds(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(work_area(), gfx::Rect(10, 5, 390, 295));

  UpdateDisplay({.bottom_work_area_inset = 10});
  EXPECT_EQ(bounds(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(work_area(), gfx::Rect(10, 5, 390, 290));

  UpdateDisplay({.right_work_area_inset = 20});
  EXPECT_EQ(bounds(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(work_area(), gfx::Rect(10, 5, 380, 290));
}

TEST_F(HeadlessUpdateDisplayTest, BoundsWorkAreaAndScale) {
  UpdateDisplay({.left = 10,
                 .top = 20,
                 .width = 1000,
                 .height = 2000,
                 .device_pixel_ratio = 2.0f});
  EXPECT_EQ(bounds(), gfx::Rect(10, 20, 500, 1000));
  EXPECT_EQ(work_area(), gfx::Rect(10, 20, 500, 1000));

  UpdateDisplay({.top_work_area_inset = 10,
                 .left_work_area_inset = 20,
                 .bottom_work_area_inset = 10,
                 .right_work_area_inset = 20});
  EXPECT_EQ(bounds(), gfx::Rect(10, 20, 500, 1000));
  EXPECT_EQ(work_area(), gfx::Rect(20, 25, 480, 990));

  UpdateDisplay({.device_pixel_ratio = 1.0f});
  EXPECT_EQ(bounds(), gfx::Rect(10, 20, 1000, 2000));
  EXPECT_EQ(work_area(), gfx::Rect(30, 30, 960, 1980));
}

TEST_F(HeadlessUpdateDisplayTest, Rotation) {
  UpdateDisplay({.rotation = 0});
  EXPECT_EQ(rotation(), Display::Rotation::ROTATE_0);

  UpdateDisplay({.rotation = 90});
  EXPECT_EQ(rotation(), Display::Rotation::ROTATE_90);

  UpdateDisplay({.rotation = 180});
  EXPECT_EQ(rotation(), Display::Rotation::ROTATE_180);

  UpdateDisplay({.rotation = 270});
  EXPECT_EQ(rotation(), Display::Rotation::ROTATE_270);
}

TEST_F(HeadlessUpdateDisplayTest, ColorDepth) {
  UpdateDisplay({.color_depth = 24});
  EXPECT_EQ(color_depth(), 24);

  UpdateDisplay({.color_depth = 32});
  EXPECT_EQ(color_depth(), 32);
}

TEST_F(HeadlessUpdateDisplayTest, Label) {
  UpdateDisplay({.label = "FooBar"});
  EXPECT_EQ(label(), "FooBar");
}

TEST_F(HeadlessUpdateDisplayTest, IsInternal) {
  UpdateDisplay({.is_internal = true});
  EXPECT_TRUE(IsInternalDisplayId(display_id()));

  UpdateDisplay({.is_internal = false});
  EXPECT_FALSE(IsInternalDisplayId(display_id()));
}

// HeadlessScreenManager::SetPrimaryDisplay tests -----------------------------

class HeadlessPrimaryDisplayTest : public ::testing::Test,
                                   public DisplayObserver {
 public:
  HeadlessPrimaryDisplayTest() { display_list_.AddObserver(this); }
  ~HeadlessPrimaryDisplayTest() override = default;

  HeadlessPrimaryDisplayTest(const HeadlessPrimaryDisplayTest&) = delete;
  HeadlessPrimaryDisplayTest& operator=(const HeadlessPrimaryDisplayTest&) =
      delete;

  // DisplayObserver:
  void OnDisplayMetricsChanged(const Display& display,
                               uint32_t changed_metrics) override {
    display_metrics_changes_.push_back(
        {.display_id = display.id(), .changed_metrics = changed_metrics});
  }

 protected:
  const DisplayList::Displays& displays() { return display_list_.displays(); }

  void SetPrimaryDisplay(int64_t display_id) {
    headless::SetPrimaryDisplay(display_list_, display_id);
  }

  display::DisplayList display_list_;

  struct DisplayMetricsChange {
    int64_t display_id = 0;
    uint32_t changed_metrics = 0;

    bool operator==(const DisplayMetricsChange& that) const {
      return display_id == that.display_id &&
             changed_metrics == that.changed_metrics;
    }
  };

  std::vector<DisplayMetricsChange> display_metrics_changes_;
};

TEST_F(HeadlessPrimaryDisplayTest, SetPrimaryDisplayBasic) {
  display_list_.AddDisplay(Display(1, gfx::Rect(0, 0, 800, 600)),
                           DisplayList::Type::PRIMARY);
  display_list_.AddDisplay(Display(2, gfx::Rect(800, 600, 800, 600)),
                           DisplayList::Type::NOT_PRIMARY);
  ASSERT_EQ(displays()[0].bounds(), gfx::Rect(0, 0, 800, 600));
  ASSERT_EQ(displays()[1].bounds(), gfx::Rect(800, 600, 800, 600));

  SetPrimaryDisplay(2);

  EXPECT_EQ(displays()[0].bounds(), gfx::Rect(-800, -600, 800, 600));
  EXPECT_EQ(displays()[1].bounds(), gfx::Rect(0, 0, 800, 600));

  // Unsetting primary display is not reported, see comments in
  // DisplayList::UpdateDisplay().
  static constexpr uint32_t kExpectedMetrics1 =
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_BOUNDS |
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_WORK_AREA;

  static constexpr uint32_t kExpectedMetrics2 =
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_PRIMARY |
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_BOUNDS |
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_WORK_AREA;

  static const std::vector<DisplayMetricsChange> kExpectedChanges = {
      {.display_id = 2, .changed_metrics = kExpectedMetrics2},
      {.display_id = 1, .changed_metrics = kExpectedMetrics1}};

  EXPECT_EQ(display_metrics_changes_, kExpectedChanges);
}

TEST_F(HeadlessPrimaryDisplayTest, SetPrimaryDisplayNonZeroOrigin) {
  display_list_.AddDisplay(Display(1, gfx::Rect(100, 200, 800, 600)),
                           DisplayList::Type::PRIMARY);
  ASSERT_EQ(displays()[0].bounds(), gfx::Rect(100, 200, 800, 600));

  SetPrimaryDisplay(1);

  EXPECT_EQ(displays()[0].bounds(), gfx::Rect(0, 0, 800, 600));

  static constexpr uint32_t kExpectedMetrics =
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_BOUNDS |
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_WORK_AREA;

  static const std::vector<DisplayMetricsChange> kExpectedChanges = {
      {.display_id = 1, .changed_metrics = kExpectedMetrics}};

  EXPECT_EQ(display_metrics_changes_, kExpectedChanges);
}

TEST_F(HeadlessPrimaryDisplayTest, SetPrimaryDisplayNonZeroOrigin2) {
  display_list_.AddDisplay(Display(1, gfx::Rect(100, 200, 800, 600)),
                           DisplayList::Type::PRIMARY);
  display_list_.AddDisplay(Display(2, gfx::Rect(900, 800, 800, 600)),
                           DisplayList::Type::NOT_PRIMARY);
  ASSERT_EQ(displays()[0].bounds(), gfx::Rect(100, 200, 800, 600));
  ASSERT_EQ(displays()[1].bounds(), gfx::Rect(900, 800, 800, 600));

  SetPrimaryDisplay(2);

  EXPECT_EQ(displays()[0].bounds(), gfx::Rect(-800, -600, 800, 600));
  EXPECT_EQ(displays()[1].bounds(), gfx::Rect(0, 0, 800, 600));

  static constexpr uint32_t kExpectedMetrics1 =
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_BOUNDS |
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_WORK_AREA;

  static constexpr uint32_t kExpectedMetrics2 =
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_PRIMARY |
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_BOUNDS |
      DisplayObserver::DisplayMetric::DISPLAY_METRIC_WORK_AREA;

  static const std::vector<DisplayMetricsChange> kExpectedChanges = {
      {.display_id = 2, .changed_metrics = kExpectedMetrics2},
      {.display_id = 1, .changed_metrics = kExpectedMetrics1}};

  EXPECT_EQ(display_metrics_changes_, kExpectedChanges);
}

}  // namespace
}  // namespace display
