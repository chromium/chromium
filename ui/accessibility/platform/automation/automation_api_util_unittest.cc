// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/automation/automation_api_util.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_generator.h"

namespace ui {

TEST(AutomationApiUtilTest, ConvertToAndFromTuples) {
  for (int i = static_cast<int>(ax::mojom::Event::kMinValue);
       i < static_cast<int>(ax::mojom::Event::kMaxValue); i++) {
    ax::mojom::Event event = static_cast<ax::mojom::Event>(i);
    auto first_tuple = MakeTupleForAutomationFromEventTypes(
        event, AXEventGenerator::Event::NONE);
    auto second_tuple = AutomationEventTypeToAXEventTuple(ToString(event));
    EXPECT_EQ(first_tuple, second_tuple);
  }
  for (int i = static_cast<int>(AXEventGenerator::Event::NONE);
       i < static_cast<int>(AXEventGenerator::Event::MAX_VALUE); i++) {
    AXEventGenerator::Event event = static_cast<AXEventGenerator::Event>(i);
    auto first_tuple =
        MakeTupleForAutomationFromEventTypes(ax::mojom::Event::kNone, event);
    auto second_tuple = AutomationEventTypeToAXEventTuple(ToString(event));
    EXPECT_EQ(first_tuple, second_tuple);
  }
}

}  // namespace ui
