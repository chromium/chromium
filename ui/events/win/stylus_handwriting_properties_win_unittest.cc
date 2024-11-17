// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/win/stylus_handwriting_properties_win.h"

#include <cstdint>
#include <limits>

#include "base/test/gtest_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace ui {

namespace {

TouchEvent GetTouchEvent() {
  return TouchEvent{EventType::kTouchPressed, gfx::PointF(), gfx::PointF(),
                    base::TimeTicks(), PointerDetails()};
}

constexpr uint32_t kValidPointerId = 10;
constexpr uint64_t kValidStrokeId = std::numeric_limits<uint32_t>::max() + 1;

constexpr uint64_t kInvalidPointerId = kValidStrokeId;
constexpr uint32_t kInvalidStrokeId = kValidPointerId;

}  // namespace

TEST(StylusHandwritingPropertiesWinTest, SetGetProperties) {
  TouchEvent event = GetTouchEvent();
  StylusHandwritingPropertiesWin properties(kValidPointerId, kValidStrokeId);
  SetStylusHandwritingProperties(event, properties);

  const std::optional<StylusHandwritingPropertiesWin> result_properties =
      GetStylusHandwritingProperties(event);
  ASSERT_TRUE(result_properties.has_value());
  EXPECT_EQ(result_properties->handwriting_pointer_id, kValidPointerId);
  EXPECT_EQ(result_properties->handwriting_stroke_id, kValidStrokeId);
}

TEST(StylusHandwritingPropertiesWinTest, PropertiesNotSet) {
  TouchEvent event = GetTouchEvent();
  const std::optional<StylusHandwritingPropertiesWin> properties =
      GetStylusHandwritingProperties(event);
  EXPECT_FALSE(properties.has_value());
}

TEST(StylusHandwritingPropertiesWinTest, PropertiesOverflowCheck) {
  TouchEvent event = GetTouchEvent();
  event.SetProperty(GetPropertyHandwritingPointerIdKeyForTesting(),
                    ConvertToEventPropertyValue(kInvalidPointerId));
  EXPECT_CHECK_DEATH(GetStylusHandwritingProperties(event));

  event = GetTouchEvent();
  event.SetProperty(GetPropertyHandwritingStrokeIdKeyForTesting(),
                    ConvertToEventPropertyValue(kInvalidStrokeId));
  EXPECT_CHECK_DEATH(GetStylusHandwritingProperties(event));
}

}  // namespace ui
