// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/test_ax_tree_update.h"

#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

std::pair<std::string, std::string> ConvertTo(
    ax::mojom::IntListAttribute int_list_attribute) {
  switch (int_list_attribute) {
    case ax::mojom::IntListAttribute::kNone:
      return {"kNone", "none"};
    case ax::mojom::IntListAttribute::kIndirectChildIds:
      return {"kIndirectChildIds,2", "indirect_child_ids=2"};
    case ax::mojom::IntListAttribute::kActionsIds:
      return {"kActionsIds,2", "actions_ids=2"};
    case ax::mojom::IntListAttribute::kControlsIds:
      return {"kControlsIds,2", "controls_ids=2"};
    case ax::mojom::IntListAttribute::kDetailsIds:
      return {"kDetailsIds,2", "details_ids=2"};
    case ax::mojom::IntListAttribute::kDescribedbyIds:
      return {"kDescribedbyIds,2", "describedby_ids=2"};
    case ax::mojom::IntListAttribute::kErrormessageIds:
      return {"kErrorMessageIds,2", "errormessage_ids=2"};
    case ax::mojom::IntListAttribute::kFlowtoIds:
      return {"kFlowtoIds,2", "flowto_ids=2"};
    case ax::mojom::IntListAttribute::kLabelledbyIds:
      return {"kLabelledbyIds,2", "labelledby_ids=2"};
    case ax::mojom::IntListAttribute::kRadioGroupIds:
      return {"kRadioGroupIds,2", "radio_group_ids=2"};
    case ax::mojom::IntListAttribute::kMarkerTypes:
      return {"kMarkerTypes,2", "marker_types=grammar"};
    case ax::mojom::IntListAttribute::kMarkerStarts:
      return {"kMarkerStarts,2", "marker_starts=2"};
    case ax::mojom::IntListAttribute::kMarkerEnds:
      return {"kMarkerEnds,2", "marker_ends=2"};
    case ax::mojom::IntListAttribute::kHighlightTypes:
      return {"kHighlightTypes,2", "highlight_types=spelling-error"};
    case ax::mojom::IntListAttribute::kCaretBounds:
      return {"kCaretBounds,2", "caret_bounds=2"};
    case ax::mojom::IntListAttribute::kCharacterOffsets:
      return {"kCharacterOffsets,2", "character_offsets=2"};
    case ax::mojom::IntListAttribute::kLineStarts:
      return {"kLineStarts,2", "line_start_offsets=2"};
    case ax::mojom::IntListAttribute::kLineEnds:
      return {"kLineEnds,2", "line_end_offsets=2"};
    case ax::mojom::IntListAttribute::kSentenceStarts:
      return {"kSentenceStarts,2", "sentence_start_offsets=2"};
    case ax::mojom::IntListAttribute::kSentenceEnds:
      return {"kSentenceEnds,2", "sentence_end_offsets=2"};
    case ax::mojom::IntListAttribute::kWordStarts:
      return {"kWordStarts,2", "word_starts=2"};
    case ax::mojom::IntListAttribute::kWordEnds:
      return {"kWordEnds,2", "word_ends=2"};
    case ax::mojom::IntListAttribute::kCustomActionIds:
      return {"kCustomActionIds,2", "custom_action_ids=2"};
    case ax::mojom::IntListAttribute::kTextOperationStartOffsets:
      return {"kTextOperationStartOffsets,2", "text_operation_start_offsets=2"};
    case ax::mojom::IntListAttribute::kTextOperationEndOffsets:
      return {"kTextOperationEndOffsets,2", "text_operation_end_offsets=2"};
    case ax::mojom::IntListAttribute::kTextOperationStartAnchorIds:
      return {"kTextOperationStartAnchorIds,2",
              "text_operation_start_anchor_ids=2"};
    case ax::mojom::IntListAttribute::kTextOperationEndAnchorIds:
      return {"kTextOperationEndAnchorIds,2",
              "text_operation_end_anchor_ids=2"};
    case ax::mojom::IntListAttribute::kTextOperations:
      return {"kTextOperations,2", "text_operations=2"};
    case ax::mojom::IntListAttribute::kAriaNotificationInterruptProperties:
      return {"kAriaNotificationInterruptProperties,2",
              "aria_notification_interrupt_properties=pending"};
    case ax::mojom::IntListAttribute::kAriaNotificationPriorityProperties:
      return {"kAriaNotificationPriorityProperties,1",
              "aria_notification_priority_properties=high"};
  }
}

TEST(TestAXTreeUpdateTest, IntListAttribute) {
  for (int i = static_cast<int>(ax::mojom::IntListAttribute::kMinValue) + 1;
       i <= static_cast<int>(ax::mojom::IntListAttribute::kMaxValue); ++i) {
    ax::mojom::IntListAttribute attribute =
        static_cast<ax::mojom::IntListAttribute>(i);

    auto [attribute_value, check_value] = ConvertTo(attribute);
    std::string update_value = absl::StrFormat(R"HTML(
    ++1 kGroup intListAttribute=%s
    ++++2 kGroup
  )HTML",
                                               attribute_value);

    TestAXTreeUpdate update(update_value);
    EXPECT_THAT(update.ToString(), ::testing::HasSubstr(check_value));
  }
}

}  // namespace ui
