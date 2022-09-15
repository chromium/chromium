// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_EVENT_INTENT_H_
#define UI_ACCESSIBILITY_AX_EVENT_INTENT_H_

#include <string>

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

// Describes what caused an accessibility event to be raised. For example, a
// character could have been typed, a word replaced, or a line deleted. Or, the
// selection could have been extended to the beginning of the previous word, or
// it could have been moved to the end of the next line.
struct AX_BASE_EXPORT AXEventIntent final {
  // Constructs an empty event intent.
  AXEventIntent();

  // Constructs an event intent which contains only a command without any other
  // arguments. This is used e.g. by the selection changed event when the
  // current selection is cleared.
  explicit AXEventIntent(ax::mojom::Command command);

  // Constructs an editing event intent; which is primarily attached to a text
  // changed or a text attributes changed event. Note that for such intents both
  // the "text_boundary" and "move_direction" members are set to "kNone".
  AXEventIntent(ax::mojom::Command command,
                ax::mojom::InputEventType input_event_type);

  // Constructs a selection event intent; which is attached to a selection
  // changed event. Note that for such intents the "input_event_type" member is
  // set to "kNone".
  AXEventIntent(ax::mojom::Command command,
                ax::mojom::TextBoundary text_boundary,
                ax::mojom::MoveDirection move_direction);

  virtual ~AXEventIntent();
  AXEventIntent(const AXEventIntent& intent);
  AXEventIntent& operator=(const AXEventIntent& intent);

  friend AX_BASE_EXPORT bool operator==(const AXEventIntent& a,
                                        const AXEventIntent& b);
  friend AX_BASE_EXPORT bool operator!=(const AXEventIntent& a,
                                        const AXEventIntent& b);

  ax::mojom::Command command = ax::mojom::Command::kNone;
  ax::mojom::InputEventType input_event_type = ax::mojom::InputEventType::kNone;
  // TODO(nektar): Split TextBoundary into TextUnit and TextBoundary.
  ax::mojom::TextBoundary text_boundary = ax::mojom::TextBoundary::kNone;
  ax::mojom::MoveDirection move_direction = ax::mojom::MoveDirection::kNone;

  // Returns a string representation of this data, for debugging.
  std::string ToString() const;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_EVENT_INTENT_H_
