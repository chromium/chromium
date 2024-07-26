// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/evdev/touch_filter/false_touch_finder.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_switches.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

class FalseTouchFinderTest : public testing::Test {
 public:
  struct TouchEntry {
    int time_ms;
    size_t slot;
    bool touching;
    gfx::PointF location;
    float pressure;
    bool expect_delay;
  };

  static constexpr gfx::Size kTouchscreenSize = gfx::Size(4000, 4000);

  FalseTouchFinderTest() {}

  FalseTouchFinderTest(const FalseTouchFinderTest&) = delete;
  FalseTouchFinderTest& operator=(const FalseTouchFinderTest&) = delete;

  ~FalseTouchFinderTest() override {}

  bool FilterAndCheck(const TouchEntry entries[], size_t count) {
    std::vector<InProgressTouchEvdev> touches;
    size_t start_index = 0u;
    std::bitset<kNumTouchEvdevSlots> was_touching;
    for (size_t i = 0; i < count; ++i) {
      const TouchEntry& entry = entries[i];

      InProgressTouchEvdev touch;
      touch.x = entry.location.x();
      touch.y = entry.location.y();
      touch.tracking_id = entry.slot;
      touch.slot = entry.slot;
      touch.pressure = entry.pressure;
      touch.was_touching = was_touching.test(touch.slot);
      touch.touching = entry.touching;
      touches.push_back(touch);

      if (i == count - 1 || entry.time_ms != entries[i + 1].time_ms) {
        false_touch_finder_->HandleTouches(
            touches, base::TimeTicks() + base::Milliseconds(entry.time_ms));

        for (size_t j = 0; j < touches.size(); ++j) {
          bool expect_delay = entries[j + start_index].expect_delay;
          size_t slot = touches[j].slot;
          if (false_touch_finder_->SlotShouldDelay(slot) != expect_delay) {
            LOG(ERROR) << base::StringPrintf(
                "Incorrect filtering at %dms for slot %zu", entry.time_ms,
                slot);
            return false;
          }
        }

        start_index = i + 1;
        touches.clear();
      }

      was_touching.set(entry.slot, entry.touching);
    }

    return true;
  }

 private:
  // testing::Test:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEdgeTouchFiltering);
    false_touch_finder_ = FalseTouchFinder::Create(kTouchscreenSize);
  }

  std::unique_ptr<FalseTouchFinder> false_touch_finder_;
};

constexpr gfx::Size FalseTouchFinderTest::kTouchscreenSize;

// Test that a touch on the edge which never leaves is delayed and never
// released.
TEST_F(FalseTouchFinderTest, EdgeTap) {
  int touchscreen_width = kTouchscreenSize.width();
  int touchscreen_height = kTouchscreenSize.height();
  const TouchEntry kTestData[] = {
      {10, 1, true, gfx::PointF(0, 100), 0.35, true},
      {20, 1, true, gfx::PointF(0, 100), 0.35, true},
      {30, 1, false, gfx::PointF(0, 100), 0.35, true},
      {40, 2, true, gfx::PointF(touchscreen_width - 1, 100), 0.35, true},
      {50, 2, true, gfx::PointF(touchscreen_width - 1, 100), 0.35, true},
      {60, 2, false, gfx::PointF(touchscreen_width - 1, 100), 0.35, true},
      {70, 3, true, gfx::PointF(100, 0), 0.35, true},
      {80, 3, true, gfx::PointF(100, 0), 0.35, true},
      {90, 3, false, gfx::PointF(100, 0), 0.35, true},
      {100, 4, true, gfx::PointF(100, touchscreen_height - 1), 0.35, true},
      {110, 4, true, gfx::PointF(100, touchscreen_height - 1), 0.35, true},
      {120, 4, false, gfx::PointF(100, touchscreen_height - 1), 0.35, true}};
  EXPECT_TRUE(FilterAndCheck(kTestData, std::size(kTestData)));
}

// Test that a touch on the edge which starts at an edge is delayed but released
// as soon as it moves.
TEST_F(FalseTouchFinderTest, MoveFromEdge) {
  const TouchEntry kTestData[] = {
      {10, 1, true, gfx::PointF(0, 100), 0.35, true},
      {20, 1, true, gfx::PointF(0, 100), 0.35, true},
      {30, 1, true, gfx::PointF(1, 100), 0.35, false},
      {40, 1, false, gfx::PointF(1, 100), 0.35, false},
      {50, 1, true, gfx::PointF(0, 100), 0.35, true},
      {60, 1, true, gfx::PointF(0, 100), 0.35, true},
      {70, 1, true, gfx::PointF(0, 101), 0.35, false},
      {80, 1, false, gfx::PointF(0, 101), 0.35, false}};
  EXPECT_TRUE(FilterAndCheck(kTestData, std::size(kTestData)));
}

// Test that a touch on the edge which starts away from the edge is not
// cancelled when it moves to the edge.
TEST_F(FalseTouchFinderTest, MoveToEdge) {
  const TouchEntry kTestData[] = {
      {10, 1, true, gfx::PointF(100, 100), 0.35, false},
      {20, 1, true, gfx::PointF(100, 100), 0.35, false},
      {30, 1, true, gfx::PointF(0, 100), 0.35, false},
      {40, 1, false, gfx::PointF(0, 100), 0.35, false}};
  EXPECT_TRUE(FilterAndCheck(kTestData, std::size(kTestData)));
}

// Test that a pinky finger lightly pressed is not filtered out. Based on real
// logs.
TEST_F(FalseTouchFinderTest, LightPinkyPressure) {
  const TouchEntry kTestData[] = {
      {10, 1, true, gfx::PointF(10, 10), 0.243137, false},
      {20, 1, true, gfx::PointF(10, 10), 0.231373, false},
      {30, 1, true, gfx::PointF(10, 10), 0.215686, false},
      {40, 1, true, gfx::PointF(10, 10), 0.211765, false},
      {50, 1, true, gfx::PointF(10, 10), 0.203922, false}};
  EXPECT_TRUE(FilterAndCheck(kTestData, std::size(kTestData)));
}

}  // namespace ui
