// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_event_intent_mac.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_intent.h"

namespace ui {

// static
AXTextSelection AXTextSelection::FromDirectionAndGranularity(
    ax::mojom::TextBoundary text_boundary,
    ax::mojom::MoveDirection move_direction) {
  bool has_stayed_within_same_text_unit = false;
  AXTextSelectionDirection direction = AXTextSelectionDirection::kDiscontiguous;
  AXTextSelectionGranularity granularity = AXTextSelectionGranularity::kUnknown;

  switch (text_boundary) {
    case ax::mojom::TextBoundary::kNone:
      break;
    case ax::mojom::TextBoundary::kCharacter:
      granularity = AXTextSelectionGranularity::kCharacter;
      break;
    case ax::mojom::TextBoundary::kFormatEnd:
    case ax::mojom::TextBoundary::kFormatStart:
    case ax::mojom::TextBoundary::kFormatStartOrEnd:
      break;  // Not supported on Mac.
    case ax::mojom::TextBoundary::kLineEnd:
      granularity = AXTextSelectionGranularity::kLine;
      break;
    case ax::mojom::TextBoundary::kLineStart:
      granularity = AXTextSelectionGranularity::kLine;
      break;
    case ax::mojom::TextBoundary::kLineStartOrEnd:
      has_stayed_within_same_text_unit = true;
      granularity = AXTextSelectionGranularity::kLine;
      break;
    case ax::mojom::TextBoundary::kObject:
      break;
    case ax::mojom::TextBoundary::kPageEnd:
      granularity = AXTextSelectionGranularity::kPage;
      break;
    case ax::mojom::TextBoundary::kPageStart:
      granularity = AXTextSelectionGranularity::kPage;
      break;
    case ax::mojom::TextBoundary::kPageStartOrEnd:
      has_stayed_within_same_text_unit = true;
      granularity = AXTextSelectionGranularity::kPage;
      break;
    case ax::mojom::TextBoundary::kParagraphEnd:
      granularity = AXTextSelectionGranularity::kParagraph;
      break;
    case ax::mojom::TextBoundary::kParagraphStart:
      granularity = AXTextSelectionGranularity::kParagraph;
      break;
    case ax::mojom::TextBoundary::kParagraphStartSkippingEmptyParagraphs:
      granularity = AXTextSelectionGranularity::kParagraph;
      break;
    case ax::mojom::TextBoundary::kParagraphStartOrEnd:
      has_stayed_within_same_text_unit = true;
      granularity = AXTextSelectionGranularity::kParagraph;
      break;
    case ax::mojom::TextBoundary::kSentenceEnd:
      granularity = AXTextSelectionGranularity::kSentence;
      break;
    case ax::mojom::TextBoundary::kSentenceStart:
      granularity = AXTextSelectionGranularity::kSentence;
      break;
    case ax::mojom::TextBoundary::kSentenceStartOrEnd:
      has_stayed_within_same_text_unit = true;
      granularity = AXTextSelectionGranularity::kSentence;
      break;
    case ax::mojom::TextBoundary::kWebPage:
      has_stayed_within_same_text_unit = true;
      granularity = AXTextSelectionGranularity::kDocument;
      break;
    case ax::mojom::TextBoundary::kWordEnd:
      granularity = AXTextSelectionGranularity::kWord;
      break;
    case ax::mojom::TextBoundary::kWordStart:
      granularity = AXTextSelectionGranularity::kWord;
      break;
    case ax::mojom::TextBoundary::kWordStartOrEnd:
      has_stayed_within_same_text_unit = true;
      granularity = AXTextSelectionGranularity::kWord;
      break;
  }

  switch (move_direction) {
    case ax::mojom::MoveDirection::kNone:
      break;
    case ax::mojom::MoveDirection::kBackward:
      if (has_stayed_within_same_text_unit) {
        direction = AXTextSelectionDirection::kBeginning;
      } else {
        direction = AXTextSelectionDirection::kPrevious;
      }
      break;
    case ax::mojom::MoveDirection::kForward:
      if (has_stayed_within_same_text_unit) {
        direction = AXTextSelectionDirection::kEnd;
      } else {
        direction = AXTextSelectionDirection::kNext;
      }
      break;
  }

  return AXTextSelection(direction, granularity, /*focus_change=*/false);
}

AXTextSelection::AXTextSelection() = default;

AXTextSelection::AXTextSelection(AXTextSelectionDirection direction,
                                 AXTextSelectionGranularity granularity,
                                 bool focus_change)
    : direction(direction),
      granularity(granularity),
      focus_change(focus_change) {}

AXTextSelection::AXTextSelection(const AXTextSelection& selection) = default;

AXTextSelection::~AXTextSelection() = default;

AXTextSelection& AXTextSelection::operator=(const AXTextSelection& selection) =
    default;

// static
AXTextStateChangeIntent
AXTextStateChangeIntent::DefaultFocusTextStateChangeIntent() {
  return AXTextStateChangeIntent(
      AXTextStateChangeType::kSelectionMove,
      AXTextSelection(AXTextSelectionDirection::kDiscontiguous,
                      AXTextSelectionGranularity::kUnknown,
                      /*focus_change=*/true));
}

// static
AXTextStateChangeIntent
AXTextStateChangeIntent::DefaultSelectionChangeIntent() {
  return AXTextStateChangeIntent(
      AXTextStateChangeType::kSelectionMove,
      AXTextSelection(AXTextSelectionDirection::kDiscontiguous,
                      AXTextSelectionGranularity::kUnknown,
                      /*focus_change=*/false));
}

AXTextStateChangeIntent::AXTextStateChangeIntent() = default;

AXTextStateChangeIntent::AXTextStateChangeIntent(AXTextStateChangeType type,
                                                 AXTextSelection selection)
    : type(type), selection(selection) {}

AXTextStateChangeIntent::AXTextStateChangeIntent(AXTextEditType edit)
    : type(AXTextStateChangeType::kEdit), edit(edit) {}

AXTextStateChangeIntent::AXTextStateChangeIntent(
    const AXTextStateChangeIntent& intent) = default;

AXTextStateChangeIntent::~AXTextStateChangeIntent() = default;

AXTextStateChangeIntent& AXTextStateChangeIntent::operator=(
    const AXTextStateChangeIntent& intent) = default;

AXTextStateChangeIntent FromEventIntent(const AXEventIntent& event_intent) {
  switch (event_intent.command) {
    case ax::mojom::Command::kNone:
      return AXTextStateChangeIntent();  // An unknown intent.
    case ax::mojom::Command::kClearSelection:
      return AXTextStateChangeIntent::DefaultSelectionChangeIntent();
    case ax::mojom::Command::kDelete:
      switch (event_intent.input_event_type) {
        case ax::mojom::InputEventType::kDeleteByCut:
        case ax::mojom::InputEventType::kDeleteByDrag:
          return AXTextStateChangeIntent(AXTextEditType::kCut);
        default:
          return AXTextStateChangeIntent(AXTextEditType::kDelete);
      }
    case ax::mojom::Command::kDictate:
      return AXTextStateChangeIntent(AXTextEditType::kDictation);
    case ax::mojom::Command::kExtendSelection:
      return AXTextStateChangeIntent(
          AXTextStateChangeType::kSelectionExtend,
          AXTextSelection::FromDirectionAndGranularity(
              event_intent.text_boundary, event_intent.move_direction));
    case ax::mojom::Command::kFormat:
      return AXTextStateChangeIntent(AXTextEditType::kAttributesChange);
    case ax::mojom::Command::kHistory:
      return AXTextStateChangeIntent();  // Not currently implemented on Mac.
    case ax::mojom::Command::kInsert:
      switch (event_intent.input_event_type) {
        case ax::mojom::InputEventType::kInsertText:
        case ax::mojom::InputEventType::kInsertLineBreak:
        case ax::mojom::InputEventType::kInsertParagraph:
        case ax::mojom::InputEventType::kInsertHorizontalRule:
          return AXTextStateChangeIntent(AXTextEditType::kTyping);
        case ax::mojom::InputEventType::kInsertFromPaste:
        case ax::mojom::InputEventType::kInsertFromDrop:
        case ax::mojom::InputEventType::kInsertFromYank:
          return AXTextStateChangeIntent(AXTextEditType::kPaste);
        default:
          return AXTextStateChangeIntent(AXTextEditType::kInsert);
      }
    case ax::mojom::Command::kMarker:
      return AXTextStateChangeIntent();  // Not currently implemented on Mac.
    case ax::mojom::Command::kMoveSelection:
      // Mac calls a "kSelectionBoundary" an operation that moves the caret
      // within an editable element by a given text unit, e.g. by word.
      return AXTextStateChangeIntent(
          AXTextStateChangeType::kSelectionBoundary,
          AXTextSelection::FromDirectionAndGranularity(
              event_intent.text_boundary, event_intent.move_direction));
    case ax::mojom::Command::kSetSelection:
      // Mac calls a "kSelectionMove" an operation that sets the selection to a
      // completely new location, such as tabbing to an edit field.
      if (event_intent.text_boundary == ax::mojom::TextBoundary::kNone) {
        // No granularity information is available. This means that focus must
        // have moved to another control, or a new selection has been made using
        // the mouse or touch in the same control. In the latter case, we'll
        // also pretend that focus has changed.
        return AXTextStateChangeIntent::DefaultFocusTextStateChangeIntent();
      }

      return AXTextStateChangeIntent(
          AXTextStateChangeType::kSelectionMove,
          AXTextSelection::FromDirectionAndGranularity(
              event_intent.text_boundary, event_intent.move_direction));
  }
}

}  // namespace ui
