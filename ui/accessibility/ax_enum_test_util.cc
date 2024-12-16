// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_enum_test_util.h"

#include <unordered_map>

#include "ui/accessibility/ax_enums.mojom-shared.h"

namespace ui {

ax::mojom::IntListAttribute StringToIntListAttribute(
    const std::string& attribute) {
  static const std::unordered_map<std::string, ax::mojom::IntListAttribute>
      attribute_map = {
          {"kNone", ax::mojom::IntListAttribute::kNone},
          {"kIndirectChildIds", ax::mojom::IntListAttribute::kIndirectChildIds},
          {"kActionsIds", ax::mojom::IntListAttribute::kActionsIds},
          {"kControlsIds", ax::mojom::IntListAttribute::kControlsIds},
          {"kDetailsIds", ax::mojom::IntListAttribute::kDetailsIds},
          {"kDescribedbyIds", ax::mojom::IntListAttribute::kDescribedbyIds},
          {"kErrorMessageIds", ax::mojom::IntListAttribute::kErrormessageIds},
          {"kFlowtoIds", ax::mojom::IntListAttribute::kFlowtoIds},
          {"kLabelledbyIds", ax::mojom::IntListAttribute::kLabelledbyIds},
          {"kRadioGroupIds", ax::mojom::IntListAttribute::kRadioGroupIds},
          {"kMarkerTypes", ax::mojom::IntListAttribute::kMarkerTypes},
          {"kMarkerStarts", ax::mojom::IntListAttribute::kMarkerStarts},
          {"kMarkerEnds", ax::mojom::IntListAttribute::kMarkerEnds},
          {"kHighlightTypes", ax::mojom::IntListAttribute::kHighlightTypes},
          {"kCaretBounds", ax::mojom::IntListAttribute::kCaretBounds},
          {"kCharacterOffsets", ax::mojom::IntListAttribute::kCharacterOffsets},
          {"kLineStarts", ax::mojom::IntListAttribute::kLineStarts},
          {"kLineEnds", ax::mojom::IntListAttribute::kLineEnds},
          {"kSentenceStarts", ax::mojom::IntListAttribute::kSentenceStarts},
          {"kSentenceEnds", ax::mojom::IntListAttribute::kSentenceEnds},
          {"kWordStarts", ax::mojom::IntListAttribute::kWordStarts},
          {"kWordEnds", ax::mojom::IntListAttribute::kWordEnds},
          {"kCustomActionIds", ax::mojom::IntListAttribute::kCustomActionIds},
          {"kTextOperationStartOffsets",
           ax::mojom::IntListAttribute::kTextOperationStartOffsets},
          {"kTextOperationEndOffsets",
           ax::mojom::IntListAttribute::kTextOperationEndOffsets},
          {"kTextOperationStartAnchorIds",
           ax::mojom::IntListAttribute::kTextOperationStartAnchorIds},
          {"kTextOperationEndAnchorIds",
           ax::mojom::IntListAttribute::kTextOperationEndAnchorIds},
          {"kTextOperations", ax::mojom::IntListAttribute::kTextOperations},
          {"kAriaNotificationInterruptProperties",
           ax::mojom::IntListAttribute::kAriaNotificationInterruptProperties},
          {"kAriaNotificationPriorityProperties",
           ax::mojom::IntListAttribute::kAriaNotificationPriorityProperties}};

  auto it = attribute_map.find(attribute);
  if (it != attribute_map.end()) {
    return it->second;
  }

  NOTREACHED() << "An invalid IntListAttribute was provided: " << attribute;
}

}  // namespace ui
