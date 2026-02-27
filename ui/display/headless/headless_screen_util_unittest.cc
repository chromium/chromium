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
