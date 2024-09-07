// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_EVENT_INTENT_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_EVENT_INTENT_MAC_H_

#include "base/component_export.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {

struct AXEventIntent;

// Describes the type of the AXTextStateChangeIntent, as defined by Mac's
// accessibility framework.
//
// Broadly, there are two types of intents that may be attached to accessibility
// events: an editing intent or a selection intent.
//
// The enumeration values are taken from the WebKit source code, but they are
// modified slightly to comply with Chromium's C++ Style Guide. Please do not
// renumber.
enum class AXTextStateChangeType {
  kUnknown = 0,
  kEdit = 1,
  kSelectionMove = 2,
  kSelectionExtend = 3,
  kSelectionBoundary = 4
};

// Describes the type of the editing intent, as defined by Mac's accessibility
// framework.
//
// The enumeration values are taken from the WebKit source code, but they are
// modified slightly to comply with Chromium's C++ Style Guide. Please do not
// renumber.
enum class AXTextEditType {
  kUnknown = 0,
  kDelete = 1,
  kInsert = 2,
  kTyping = 3,
  kDictation = 4,
  kCut = 5,
  kPaste = 6,
  kAttributesChange = 7  // Change font, style, alignment, color, etc.
};

// Describes the directionality of a selection intent, as defined by Mac's
// accessibility framework.
//
// The enumeration values are taken from the WebKit source code, but they are
// modified slightly to comply with Chromium's C++ Style Guide. Please do not
// renumber.
enum class AXTextSelectionDirection {
  kUnknown = 0,
  kBeginning = 1,
  kEnd = 2,
  kPrevious = 3,
  kNext = 4,
  kDiscontiguous = 5
};

// Describes the amount by which the selection has moved or has been extended
// by, as defined by Mac's accessibility framework.
//
// The enumeration values are taken from the WebKit source code, but they are
// modified slightly to comply with Chromium's C++ Style Guide. Please do not
// renumber.
enum class AXTextSelectionGranularity {
  kUnknown = 0,
  kCharacter = 1,
  kWord = 2,
  kLine = 3,
  kSentence = 4,
  kParagraph = 5,
  kPage = 6,
  kDocument = 7,

  // All granularity represents the action of selecting the whole document as a
  // single action. Extending selection by some other granularity until it
  // encompasses the whole document should not result in a all granularity
  // notification.
  kAll = 8
};

// Describes a selection operation in an AXTextStateChangeIntent.
struct AXTextSelection final {
  // Constructs a description of a selection operation, translating the given
  // direction and granularity from Chromium's internal representation to what
  // the Mac accessibility framework expects.
  static AXTextSelection FromDirectionAndGranularity(
      ax::mojom::TextBoundary text_boundary,
      ax::mojom::MoveDirection move_direction);

  // Constructs a description of a selection operation that has no extra
  // information, such as when the selection is set using the mouse and so its
  // granularity would be hard to ascertain.
  AXTextSelection();

  // Constructs a description of a selection operation for which extra
  // information can be provided.
  AXTextSelection(AXTextSelectionDirection direction,
                  AXTextSelectionGranularity granularity,
                  bool focus_change);

  AXTextSelection(const AXTextSelection& selection);

  virtual ~AXTextSelection();

  AXTextSelection& operator=(const AXTextSelection& selection);

  AXTextSelectionDirection direction = AXTextSelectionDirection::kUnknown;
  AXTextSelectionGranularity granularity = AXTextSelectionGranularity::kUnknown;
  bool focus_change = false;
};

// The equivalent of an accessibility event intent (AXEventIntent), as
// defined by Mac's accessibility framework.
struct COMPONENT_EXPORT(AX_PLATFORM) AXTextStateChangeIntent final {
  // Constructs an intent that is used when the selection is set to a different
  // iframe or control, and focus has move to that element.
  static AXTextStateChangeIntent DefaultFocusTextStateChangeIntent();

  // Constructs an intent that is used when the selection has moved to another
  // location within the same control, (i.e. browser focus hasn't moved), or
  // when the selection has been cleared.
  static AXTextStateChangeIntent DefaultSelectionChangeIntent();

  // Constructs an empty intent.
  AXTextStateChangeIntent();

  // Constructs a selection intent.
  AXTextStateChangeIntent(AXTextStateChangeType type,
                          AXTextSelection selection);

  // Constructs an editing intent.
  explicit AXTextStateChangeIntent(AXTextEditType edit);

  AXTextStateChangeIntent(const AXTextStateChangeIntent& intent);

  virtual ~AXTextStateChangeIntent();

  AXTextStateChangeIntent& operator=(const AXTextStateChangeIntent& intent);

  AXTextStateChangeType type = AXTextStateChangeType::kUnknown;
  AXTextEditType edit = AXTextEditType::kUnknown;
  AXTextSelection selection;
};

// Converts from Chromium's AXEventIntent to Mac's AXTextStateChangeIntent.
COMPONENT_EXPORT(AX_PLATFORM)
AXTextStateChangeIntent FromEventIntent(const AXEventIntent& event_intent);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_EVENT_INTENT_MAC_H_
