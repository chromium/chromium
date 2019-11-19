// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_POSITION_H_
#define UI_ACCESSIBILITY_AX_POSITION_H_

#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/stack.h"
#include "base/i18n/break_iterator.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_text_styles.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_text_boundary.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/utf16_indexing.h"

namespace ui {

// Defines the type of position in the accessibility tree.
// A tree position is used when referring to a specific child of a node in the
// accessibility tree.
// A text position is used when referring to a specific character of text inside
// a particular node.
// A null position is used to signify that the provided data is invalid or that
// a boundary has been reached.
enum class AXPositionKind { NULL_POSITION, TREE_POSITION, TEXT_POSITION };

// Defines how creating the next or previous position should behave whenever we
// are at or are crossing a boundary, such as at the start of an anchor, a word
// or a line.
enum class AXBoundaryBehavior {
  CrossBoundary,
  StopAtAnchorBoundary,
  StopIfAlreadyAtBoundary,
  StopAtLastAnchorBoundary
};

// Specifies how AXPosition::ExpandToEnclosingTextBoundary behaves.
//
// As an example, imagine we have the text "hello world" and a position before
// the space character. We want to expand to the surrounding word boundary.
// Since we are right at the end of the first word, we could either expand to
// the left first, find the start of the first word and then use that to find
// the corresponding word end, resulting in the word "Hello". Another
// possibility is to expand to the right first, find the end of the next word
// and use that as our starting point to find the previous word start, resulting
// in the word "world".
enum class AXRangeExpandBehavior {
  // Expands to the left boundary first and then uses that position as the
  // starting point to find the boundary to the right.
  kLeftFirst,
  // Expands to the right boundary first and then uses that position as the
  // starting point to find the boundary to the left.
  kRightFirst
};

// Forward declarations.
template <class AXPositionType, class AXNodeType>
class AXPosition;
template <class AXPositionType>
class AXRange;
template <class AXPositionType, class AXNodeType>
bool operator==(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second);
template <class AXPositionType, class AXNodeType>
bool operator!=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second);

// A position in the accessibility tree.
//
// This class could either represent a tree position or a text position.
// Tree positions point to either a child of a specific node or at the end of a
// node (i.e. an "after children" position).
// Text positions point to either a character offset in the text inside a
// particular node including text from all its children, or to the end of the
// node's text, (i.e. an "after text" position).
// On tree positions that have a leaf node as their anchor, we also need to
// distinguish between "before text" and "after text" positions. To do this, if
// the child index is 0 and the anchor is a leaf node, then it's an "after text"
// position. If the child index is |BEFORE_TEXT| and the anchor is a leaf node,
// then this is a "before text" position.
// It doesn't make sense to have a "before text" position on a text position,
// because it is identical to setting its offset to the first character.
//
// To avoid re-computing either the text offset or the child index when
// converting between the two types of positions, both values are saved after
// the first conversion.
//
// This class template uses static polymorphism in order to allow sub-classes to
// be created from the base class without the base class knowing the type of the
// sub-class in advance.
// The template argument |AXPositionType| should always be set to the type of
// any class that inherits from this template, making this a
// "curiously recursive template".
//
// This class can be copied using the |Clone| method. It is designed to be
// immutable.
template <class AXPositionType, class AXNodeType>
class AXPosition {
 public:
  using AXPositionInstance =
      std::unique_ptr<AXPosition<AXPositionType, AXNodeType>>;

  using AXRangeType = AXRange<AXPosition<AXPositionType, AXNodeType>>;

  using BoundaryConditionPredicate =
      base::RepeatingCallback<bool(const AXPositionInstance&)>;

  using BoundaryTextOffsetsFunc =
      base::RepeatingCallback<std::vector<int32_t>(const AXPositionInstance&)>;

  // When converting to an unignored position, determines how to adjust the new
  // position in order to make it valid.
  enum class AdjustmentBehavior { kMoveLeft, kMoveRight };

  static const int BEFORE_TEXT = -1;
  static const int INVALID_INDEX = -2;
  static const int INVALID_OFFSET = -1;

  static AXPositionInstance CreateNullPosition() {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(
        AXPositionKind::NULL_POSITION, AXTreeIDUnknown(), AXNode::kInvalidAXID,
        INVALID_INDEX, INVALID_OFFSET, ax::mojom::TextAffinity::kDownstream);
    return new_position;
  }

  static AXPositionInstance CreateTreePosition(AXTreeID tree_id,
                                               AXNode::AXID anchor_id,
                                               int child_index) {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::TREE_POSITION, tree_id, anchor_id,
                             child_index, INVALID_OFFSET,
                             ax::mojom::TextAffinity::kDownstream);
    return new_position;
  }

  static AXPositionInstance CreateTextPosition(
      AXTreeID tree_id,
      AXNode::AXID anchor_id,
      int text_offset,
      ax::mojom::TextAffinity affinity) {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::TEXT_POSITION, tree_id, anchor_id,
                             INVALID_INDEX, text_offset, affinity);
    return new_position;
  }

  virtual ~AXPosition() = default;

  // Implemented based on the copy and swap idiom.
  AXPosition& operator=(const AXPosition& other) {
    AXPositionInstance clone = other.Clone();
    swap(*clone);
    return *this;
  }

  virtual AXPositionInstance Clone() const = 0;

  // A serialization of a position as POD. Not for sharing on disk or sharing
  // across thread or process boundaries, just for passing a position to an
  // API that works with positions as opaque objects.
  struct SerializedPosition {
    AXPositionKind kind;
    AXNode::AXID anchor_id;
    int child_index;
    int text_offset;
    ax::mojom::TextAffinity affinity;
    char tree_id[33];
  };

  static_assert(std::is_trivially_copyable<SerializedPosition>::value,
                "SerializedPosition must be POD");

  SerializedPosition Serialize() {
    SerializedPosition result;
    result.kind = kind_;

    // A tree ID can be serialized as a 32-byte string.
    std::string tree_id_string = tree_id_.ToString();
    DCHECK_LE(tree_id_string.size(), 32U);
    strncpy(result.tree_id, tree_id_string.c_str(), 32);
    result.tree_id[32] = 0;

    result.anchor_id = anchor_id_;
    result.child_index = child_index_;
    result.text_offset = text_offset_;
    result.affinity = affinity_;
    return result;
  }

  static AXPositionInstance Unserialize(
      const SerializedPosition& serialization) {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(serialization.kind,
                             ui::AXTreeID::FromString(serialization.tree_id),
                             serialization.anchor_id, serialization.child_index,
                             serialization.text_offset, serialization.affinity);
    return new_position;
  }

  virtual bool IsIgnoredPosition() const { return false; }

  virtual AXPositionInstance AsUnignoredTextPosition(
      AdjustmentBehavior adjustment_behavior) const {
    return Clone();
  }

  std::string ToString() const {
    std::string str;
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return "NullPosition";
      case AXPositionKind::TREE_POSITION: {
        std::string str_child_index;
        if (child_index_ == BEFORE_TEXT) {
          str_child_index = "before_text";
        } else if (child_index_ == INVALID_INDEX) {
          str_child_index = "invalid";
        } else {
          str_child_index = base::NumberToString(child_index_);
        }
        str = "TreePosition tree_id=" + tree_id_.ToString() +
              " anchor_id=" + base::NumberToString(anchor_id_) +
              " child_index=" + str_child_index;
        break;
      }
      case AXPositionKind::TEXT_POSITION: {
        std::string str_text_offset;
        if (text_offset_ == INVALID_OFFSET) {
          str_text_offset = "invalid";
        } else {
          str_text_offset = base::NumberToString(text_offset_);
        }
        str = "TextPosition anchor_id=" + base::NumberToString(anchor_id_) +
              " text_offset=" + str_text_offset + " affinity=" +
              ui::ToString(static_cast<ax::mojom::TextAffinity>(affinity_));
        break;
      }
    }

    if (!IsTextPosition() || text_offset_ > MaxTextOffset())
      return str;

    std::string text = base::UTF16ToUTF8(GetText());
    DCHECK_GE(text_offset_, 0);
    DCHECK_LE(text_offset_, int{text.length()});
    std::string annotated_text;
    if (text_offset_ == MaxTextOffset()) {
      annotated_text = text + "<>";
    } else {
      annotated_text = text.substr(0, text_offset_) + "<" + text[text_offset_] +
                       ">" + text.substr(text_offset_ + 1);
    }

    return str + " annotated_text=" + annotated_text;
  }

  AXTreeID tree_id() const { return tree_id_; }
  AXNode::AXID anchor_id() const { return anchor_id_; }

  AXNodeType* GetAnchor() const {
    if (tree_id_ == AXTreeIDUnknown() || anchor_id_ == AXNode::kInvalidAXID)
      return nullptr;
    DCHECK_GE(anchor_id_, 0);
    return GetNodeInTree(tree_id_, anchor_id_);
  }

  bool IsIgnored() const {
    AXNodeType* anchor = GetAnchor();
    return anchor && anchor->IsIgnored();
  }

  AXPositionKind kind() const { return kind_; }
  int child_index() const { return child_index_; }
  int text_offset() const { return text_offset_; }
  ax::mojom::TextAffinity affinity() const { return affinity_; }

  bool IsNullPosition() const {
    return kind_ == AXPositionKind::NULL_POSITION || !GetAnchor();
  }

  bool IsTreePosition() const {
    return GetAnchor() && kind_ == AXPositionKind::TREE_POSITION;
  }

  bool IsTextPosition() const {
    return GetAnchor() && kind_ == AXPositionKind::TEXT_POSITION;
  }

  bool IsLeafTextPosition() const {
    return IsTextPosition() && !AnchorChildCount();
  }

  // Returns true if this is a valid position, e.g. the child_index_ or
  // text_offset_ is within a valid range.
  bool IsValid() const {
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return tree_id_ == AXTreeIDUnknown() &&
               anchor_id_ == AXNode::kInvalidAXID &&
               child_index_ == INVALID_INDEX &&
               text_offset_ == INVALID_OFFSET &&
               affinity_ == ax::mojom::TextAffinity::kDownstream;
      case AXPositionKind::TREE_POSITION:
        return GetAnchor() &&
               (child_index_ == BEFORE_TEXT ||
                (child_index_ >= 0 && child_index_ <= AnchorChildCount()));
      case AXPositionKind::TEXT_POSITION:
        if (!GetAnchor())
          return false;

        // For performance reasons we skip any validation of the text offset
        // that involves retrieving the anchor's text, if the offset is set to
        // 0, because 0 is frequently used and always valid regardless of the
        // actual text.
        return text_offset_ == 0 ||
               (text_offset_ > 0 && text_offset_ <= MaxTextOffset());
    }
  }

  // TODO(nektar): Update logic of AtStartOfAnchor() for text_offset_ == 0 and
  // fix related bug.
  bool AtStartOfAnchor() const {
    if (!GetAnchor())
      return false;
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        if (text_offset_ > 0)
          return false;
        if (AnchorChildCount() || text_offset_ == 0)
          return child_index_ == 0;
        return child_index_ == BEFORE_TEXT;
      case AXPositionKind::TEXT_POSITION:
        return text_offset_ == 0;
    }
  }

  bool AtEndOfAnchor() const {
    if (!GetAnchor())
      return false;
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        return child_index_ == AnchorChildCount();
      case AXPositionKind::TEXT_POSITION:
        return text_offset_ == MaxTextOffset();
    }
  }

  bool AtStartOfWord() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        const std::vector<int32_t> word_starts =
            text_position->GetWordStartOffsets();
        return base::Contains(word_starts,
                              int32_t{text_position->text_offset_});
      }
    }
  }

  bool AtEndOfWord() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        const std::vector<int32_t> word_ends =
            text_position->GetWordEndOffsets();
        return base::Contains(word_ends, int32_t{text_position->text_offset_});
      }
    }
  }

  bool AtStartOfLine() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION:
        // We treat a position after some white space that is not connected to
        // any node after it via "next on line ID", to be equivalent to a
        // position before the next line, and therefore as being at start of
        // line.
        //
        // We assume that white space separates lines.
        if (text_position->IsInWhiteSpace() &&
            GetNextOnLineID(text_position->anchor_id_) ==
                AXNode::kInvalidAXID &&
            text_position->AtEndOfAnchor()) {
          return true;
        }

        return GetPreviousOnLineID(text_position->anchor_id_) ==
                   AXNode::kInvalidAXID &&
               text_position->AtStartOfAnchor();
    }
  }

  bool AtEndOfLine() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION:
        // Text positions on objects with no text should not be considered at
        // end of line because the empty position may share a text offset with
        // a non-empty text position in which case the end of line iterators
        // must move to the line end of the non-empty content. Specified next
        // line IDs are ignored.
        if (!text_position->MaxTextOffset())
          return false;

        // If affinity has been used to specify whether the caret is at the end
        // of a line or at the start of the next one, this should have been
        // reflected in the leaf text position we got. In other cases, we
        // assume that white space is being used to separate lines.
        //
        // We don't treat a position that is at the start of white space that is
        // on a line by itself as being at the end of the line. However, we do
        // treat positions at the start of white space that end a line of text
        // as being at the end of that line. We also treat positions at the end
        // of white space that is on a line by itself as being at the end of
        // that line. Note that white space that ends a line of text should be
        // connected to that text with a "previous on line ID".
        if (GetNextOnLineID(text_position->anchor_id_) == AXNode::kInvalidAXID)
          return (!text_position->IsInWhiteSpace() ||
                  GetPreviousOnLineID(text_position->anchor_id_) ==
                      AXNode::kInvalidAXID)
                     ? text_position->AtEndOfAnchor()
                     : text_position->AtStartOfAnchor();

        // The current anchor might be followed by a soft line break.
        return text_position->AtEndOfAnchor() &&
               text_position->CreateNextLeafTextPosition()->AtEndOfLine();
    }
  }

  // |AtStartOfParagraph| is asymmetric from |AtEndOfParagraph| because of
  // trailing whitespace collapse rules.
  // The start of a paragraph should be a leaf text position (or equivalent),
  // either at the start of the document, or at the start of the next leaf text
  // position from the one representing the end of the previous paragraph.
  // A position |AsLeafTextPosition| is the start of a paragraph if all of the
  // following are true :
  // 1. The current leaf text position must be an unignored position at
  //    the start of an anchor.
  // 2. The current position is not whitespace only, unless it is also
  //    the first leaf text position within the document.
  // 3. Either (a) the current leaf text position is the first leaf text
  //    position in the document, or (b) there are no line breaking
  //    objects between it and the previous non-whitespace leaf text
  //    position.
  bool AtStartOfParagraph() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        // 1. The current leaf text position must be an unignored position at
        //    the start of an anchor.
        if (text_position->IsIgnored() || !text_position->AtStartOfAnchor())
          return false;

        // 2. The current position is not whitespace only, unless it is also
        //    the first leaf text position within the document.
        if (text_position->IsInWhiteSpace())
          return text_position->CreatePreviousLeafTextPosition()
              ->IsNullPosition();

        // 3. Either (a) the current leaf text position is the first leaf text
        //    position in the document, or (b) there are no line breaking
        //    objects between it and the previous non-whitespace leaf text
        //    position.
        //
        // Search for the previous text position within the current paragraph,
        // using the paragraph boundary abort predicate.
        // If a valid position was found, then this position cannot be
        // the start of a paragraph.
        // This will return a null position when an anchor movement would
        // cross a paragraph boundary, or the start of document was reached.
        bool crossed_potential_boundary_token = false;
        const AbortMovePredicate abort_move_predicate =
            base::BindRepeating(&AbortMoveAtParagraphBoundary,
                                std::ref(crossed_potential_boundary_token));
        AXPositionInstance previous_text_position = text_position->Clone();
        do {
          previous_text_position =
              previous_text_position->CreatePreviousTextAnchorPosition(
                  abort_move_predicate);
          // If the previous position is whitespace, then continue searching
          // until a non-whitespace leaf text position is found within the
          // current paragraph because whitespace is supposed to be collapsed.
          // There's a chance that |CreatePreviousTextAnchorPosition| will
          // return whitespace that should be appended to a previous paragraph
          // rather than separating two pieces of the current paragraph.
        } while (previous_text_position->IsInWhiteSpace() ||
                 previous_text_position->IsIgnored());
        return previous_text_position->IsNullPosition();
      }
    }
  }

  // |AtEndOfParagraph| is asymmetric from |AtStartOfParagraph| because of
  // trailing whitespace collapse rules.
  // The end of a paragraph should be a leaf text position (or equivalent),
  // either at the end of the document, or at the end of the previous leaf text
  // position from the one representing the start of the next paragraph.
  // A position |AsLeafTextPosition| is the end of a paragraph if all of the
  // following are true :
  // 1. The current leaf text position must be an unignored position at
  //    the end of an anchor.
  // 2. Either (a) the current leaf text position is the last leaf text
  //    position in the document, or (b) there are no line breaking
  //    objects between it and the next leaf text position except when
  //    the next leaf text position is whitespace only since whitespace
  //    must be collapsed.
  // 3. If there is a next leaf text position then it must not be
  //    whitespace only.
  // 4. If there is a next leaf text position and it is not whitespace
  //    only, it must also be the start of a paragraph for the current
  //    position to be the end of a paragraph.
  bool AtEndOfParagraph() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        // 1. The current leaf text position must be an unignored position at
        //    the end of an anchor.
        if (text_position->IsIgnored() || !text_position->AtEndOfAnchor())
          return false;

        // 2. Either (a) the current leaf text position is the last leaf text
        //    position in the document, or (b) there are no line breaking
        //    objects between it and the next leaf text position except when
        //    the next leaf text position is whitespace only since whitespace
        //    must be collapsed.
        //
        // Search for the next text position within the current paragraph,
        // using the paragraph boundary abort predicate.
        // If a null position was found, then this position must be the end of
        // a paragraph.
        // |CreateNextTextAnchorPosition| + |AbortMoveAtParagraphBoundary|
        // will return a null position when an anchor movement would
        // cross a paragraph boundary and there is no doubt that it is the end
        // of a paragraph, or the end of document was reached.
        // There are some fringe cases related to whitespace collapse that
        // cannot be handled easily with only |AbortMoveAtParagraphBoundary|.
        bool crossed_potential_boundary_token = false;
        const AbortMovePredicate abort_move_predicate =
            base::BindRepeating(&AbortMoveAtParagraphBoundary,
                                std::ref(crossed_potential_boundary_token));

        AXPositionInstance next_text_position = text_position->Clone();
        do {
          next_text_position = next_text_position->CreateNextTextAnchorPosition(
              abort_move_predicate);
        } while (next_text_position->IsIgnored());
        if (next_text_position->IsNullPosition())
          return true;

        // 3. If there is a next leaf text position then it must not be
        //    whitespace only.
        if (next_text_position->IsInWhiteSpace())
          return false;

        // 4. If there is a next leaf text position and it is not whitespace
        //    only, it must also be the start of a paragraph for the current
        //    position to be the end of a paragraph.
        //
        // Consider the following example :
        // ++{1} kStaticText "First Paragraph"
        // ++++{2} kInlineTextBox "First Paragraph"
        // ++{3} kStaticText "\n Second Paragraph"
        // ++++{4} kInlineTextBox "\n" kIsLineBreakingObject
        // ++++{5} kInlineTextBox " "
        // ++++{6} kInlineTextBox "Second Paragraph"
        // A position at the end of {5} is the end of a paragraph, because
        // the first paragraph must collapse trailing whitespace and contain
        // leaf text anchors {2, 4, 5}. The second paragraph is only {6}.
        return next_text_position->CreatePositionAtStartOfAnchor()
            ->AtStartOfParagraph();
      }
    }
  }

  bool AtStartOfPage() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        if (!text_position->AtStartOfAnchor())
          return false;

        // Search for the previous text position within the current page,
        // using the page boundary abort predicate.
        // If a valid position was found, then this position cannot be
        // the start of a page.
        // This will return a null position when an anchor movement would
        // cross a page boundary, or the start of document was reached.
        AXPositionInstance previous_text_position =
            text_position->CreatePreviousTextAnchorPosition(
                base::BindRepeating(&AbortMoveAtPageBoundary));
        return previous_text_position->IsNullPosition();
      }
    }
  }

  bool AtEndOfPage() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        if (!text_position->AtEndOfAnchor())
          return false;

        // Search for the next text position within the current page,
        // using the page boundary abort predicate.
        // If a valid position was found, then this position cannot be
        // the end of a page.
        // This will return a null position when an anchor movement would
        // cross a page boundary, or the end of document was reached.
        AXPositionInstance next_text_position =
            text_position->CreateNextTextAnchorPosition(
                base::BindRepeating(&AbortMoveAtPageBoundary));
        return next_text_position->IsNullPosition();
      }
    }
  }

  bool AtStartOfFormat() const {
    // Since formats are stored on text anchors, the start of a format boundary
    // must be at the start of an anchor.
    if (IsNullPosition() || !AtStartOfAnchor())
      return false;

    // Treat the first iterable node as a format boundary.
    if (CreatePreviousLeafTreePosition()->IsNullPosition())
      return true;

    // Iterate over anchors until a format boundary is found. This will return a
    // null position upon crossing a boundary.
    AXPositionInstance previous_position = CreatePreviousLeafTreePosition(
        base::BindRepeating(&AbortMoveAtFormatBoundary));
    return previous_position->IsNullPosition();
  }

  bool AtEndOfFormat() const {
    // Since formats are stored on text anchors, the end of a format break must
    // be at the end of an anchor.
    if (IsNullPosition() || !AtEndOfAnchor())
      return false;

    // Treat the last iterable node as a format boundary
    if (CreateNextLeafTreePosition()->IsNullPosition())
      return true;

    // Iterate over anchors until a format boundary is found. This will return a
    // null position upon crossing a boundary.
    AXPositionInstance next_position = CreateNextLeafTreePosition(
        base::BindRepeating(&AbortMoveAtFormatBoundary));
    return next_position->IsNullPosition();
  }

  bool AtStartOfInlineBlock() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        if (text_position->AtStartOfAnchor()) {
          AXPositionInstance previous_position =
              text_position->CreatePreviousLeafTreePosition();

          // Check that this position is not the start of the first anchor.
          if (!previous_position->IsNullPosition()) {
            previous_position = text_position->CreatePreviousLeafTreePosition(
                base::BindRepeating(&AbortMoveAtStartOfInlineBlock));

            // If we get a null position here it means we have crossed an inline
            // block's start, thus this position is located at such start.
            if (previous_position->IsNullPosition())
              return true;
          }
        }
        if (text_position->AtEndOfAnchor()) {
          AXPositionInstance next_position =
              text_position->CreateNextLeafTreePosition();

          // Check that this position is not the end of the last anchor.
          if (!next_position->IsNullPosition()) {
            next_position = text_position->CreateNextLeafTreePosition(
                base::BindRepeating(&AbortMoveAtStartOfInlineBlock));

            // If we get a null position here it means we have crossed an inline
            // block's start, thus this position is located at such start.
            if (next_position->IsNullPosition())
              return true;
          }
        }
        return false;
      }
    }
  }

  bool AtStartOfDocument() const {
    if (IsNullPosition())
      return false;
    return IsDocument(GetRole()) && AtStartOfAnchor();
  }

  bool AtEndOfDocument() const {
    if (IsNullPosition())
      return false;
    return CreateNextAnchorPosition()->IsNullPosition() && AtEndOfAnchor();
  }

  // This method finds the lowest common AXNodeType of |this| and |second|.
  AXNodeType* LowestCommonAnchor(const AXPosition& second) const {
    if (IsNullPosition() || second.IsNullPosition())
      return nullptr;
    if (GetAnchor() == second.GetAnchor())
      return GetAnchor();

    base::stack<AXNodeType*> our_ancestors = GetAncestorAnchors();
    base::stack<AXNodeType*> other_ancestors = second.GetAncestorAnchors();

    AXNodeType* common_anchor = nullptr;
    while (!our_ancestors.empty() && !other_ancestors.empty() &&
           our_ancestors.top() == other_ancestors.top()) {
      common_anchor = our_ancestors.top();
      our_ancestors.pop();
      other_ancestors.pop();
    }
    return common_anchor;
  }

  // This method returns a position instead of a node because this allows us to
  // return the corresponding text offset or child index in the ancestor that
  // relates to the current position.
  // Also, this method uses position instead of tree logic to traverse the tree,
  // because positions can handle moving across multiple trees, while trees
  // cannot.
  AXPositionInstance LowestCommonAncestor(const AXPosition& second) const {
    return CreateAncestorPosition(LowestCommonAnchor(second));
  }

  AXPositionInstance CreateAncestorPosition(
      const AXNodeType* ancestor_anchor) const {
    if (!ancestor_anchor)
      return CreateNullPosition();

    AXPositionInstance ancestor_position = Clone();
    while (!ancestor_position->IsNullPosition() &&
           ancestor_position->GetAnchor() != ancestor_anchor) {
      ancestor_position = ancestor_position->CreateParentPosition();
    }
    return ancestor_position;
  }

  AXPositionInstance AsTreePosition() const {
    if (IsNullPosition() || IsTreePosition())
      return Clone();

    AXPositionInstance copy = Clone();
    DCHECK(copy);
    DCHECK_GE(copy->text_offset_, 0);
    if (!copy->AnchorChildCount()) {
      const int max_text_offset = copy->MaxTextOffset();
      copy->child_index_ =
          (max_text_offset != 0 && copy->text_offset_ != max_text_offset)
              ? BEFORE_TEXT
              : 0;
      copy->kind_ = AXPositionKind::TREE_POSITION;
      return copy;
    }

    // Blink doesn't always remove all deleted whitespace at the end of a
    // textarea even though it will have adjusted its value attribute, because
    // the extra layout objects are invisible. Therefore, we will stop at the
    // last child that we can reach with the current text offset and ignore any
    // remaining children.
    int current_offset = 0;
    int child_index = 0;
    for (; child_index < copy->AnchorChildCount(); ++child_index) {
      AXPositionInstance child = copy->CreateChildPositionAt(child_index);
      DCHECK(child);
      int child_length = child->MaxTextOffsetInParent();
      // If the text offset falls on the boundary between two adjacent children,
      // we look at the affinity to decide whether to place the tree position on
      // the first child vs. the second child. Upstream affinity would always
      // choose the first child, whilst downstream affinity the second. This
      // also has implications when converting the resulting tree position back
      // to a text position. In that case, maintaining an upstream affinity
      // would place the text position at the end of the first child, whilst
      // maintaining a downstream affinity will place the text position at the
      // beginning of the second child.
      //
      // This is vital for text positions on soft line breaks, as well as text
      // positions before and after character, to work properly.
      //
      // See also `CreateLeafTextPositionBeforeCharacter` and
      // `CreateLeafTextPositionAfterCharacter`.
      if (copy->text_offset_ >= current_offset &&
          (copy->text_offset_ < (current_offset + child_length) ||
           (copy->affinity_ == ax::mojom::TextAffinity::kUpstream &&
            copy->text_offset_ == (current_offset + child_length)))) {
        break;
      }

      current_offset += child_length;
    }

    copy->child_index_ = child_index;
    copy->kind_ = AXPositionKind::TREE_POSITION;
    return copy;
  }

  AXPositionInstance AsTextPosition() const {
    if (IsNullPosition() || IsTextPosition())
      return Clone();

    AXPositionInstance copy = Clone();
    DCHECK(copy);
    // Check if it is a "before text" position.
    if (copy->child_index_ == BEFORE_TEXT) {
      // "Before text" positions can only appear on leaf nodes.
      DCHECK(!copy->AnchorChildCount());
      // If the current text offset is valid, we don't touch it to potentially
      // allow converting from a text position to a tree position and back
      // without losing information.
      //
      // We test for INVALID_OFFSET first, due to the possible performance
      // implications of calling MaxTextOffset().
      DCHECK_GE(copy->text_offset_, INVALID_OFFSET)
          << "Unrecognized text offset.";
      if (copy->text_offset_ == INVALID_OFFSET ||
          (copy->text_offset_ > 0 &&
           copy->text_offset_ >= copy->MaxTextOffset())) {
        copy->text_offset_ = 0;
      }
    } else if (copy->child_index_ == copy->AnchorChildCount()) {
      copy->text_offset_ = copy->MaxTextOffset();
    } else {
      DCHECK_GE(copy->child_index_, 0);
      DCHECK_LT(copy->child_index_, copy->AnchorChildCount());
      int new_offset = 0;
      for (int i = 0; i <= child_index_; ++i) {
        AXPositionInstance child = copy->CreateChildPositionAt(i);
        DCHECK(child);
        // If the current text offset is valid, we don't touch it to
        // potentially allow converting from a text position to a tree
        // position and back without losing information. Otherwise, if the
        // text_offset is invalid, equals to 0 or is smaller than
        // |new_offset|, we reset it to the beginning of the current child
        // node.
        if (i == child_index_ && copy->text_offset_ <= new_offset) {
          copy->text_offset_ = new_offset;
          break;
        }

        int child_length = child->MaxTextOffsetInParent();
        // Same comment as above: we don't touch the text offset if it's
        // already valid.
        if (i == child_index_ &&
            (copy->text_offset_ > (new_offset + child_length) ||
             // When the text offset is equal to the text's length but this is
             // not an "after text" position.
             (!copy->AtEndOfAnchor() &&
              copy->text_offset_ == (new_offset + child_length)))) {
          copy->text_offset_ = new_offset;
          break;
        }

        new_offset += child_length;
      }
    }

    // Affinity should always be left as downstream. The only case when the
    // resulting text position is at the end of the line is when we get an
    // "after text" leaf position, but even in this case downstream is
    // appropriate because there is no ambiguity whetehr the position is at the
    // end of the current line vs. the start of the next line. It would always
    // be the former.
    copy->kind_ = AXPositionKind::TEXT_POSITION;
    return copy;
  }

  AXPositionInstance AsLeafTextPosition() const {
    if (IsNullPosition() || !AnchorChildCount())
      return AsTextPosition();

    // Adjust the text offset.
    // No need to check for "before text" positions here because they are only
    // present on leaf anchor nodes.
    AXPositionInstance text_position = AsTextPosition();
    int adjusted_offset = text_position->text_offset_;
    do {
      AXPositionInstance child_position =
          text_position->CreateChildPositionAt(0);
      DCHECK(child_position);

      // If the text offset corresponds to multiple child positions because some
      // of the children have empty text, the condition "adjusted_offset > 0"
      // below ensures that the first child will be chosen.
      for (int i = 1;
           i < text_position->AnchorChildCount() && adjusted_offset > 0; ++i) {
        const int max_text_offset_in_parent =
            child_position->MaxTextOffsetInParent();
        if (adjusted_offset < max_text_offset_in_parent) {
          break;
        }
        if (affinity_ == ax::mojom::TextAffinity::kUpstream &&
            adjusted_offset == max_text_offset_in_parent) {
          // Maintain upstream affinity so that we'll be able to choose the
          // correct leaf anchor if the text offset is right on the boundary
          // between two leaves.
          child_position->affinity_ = ax::mojom::TextAffinity::kUpstream;
          break;
        }
        child_position = text_position->CreateChildPositionAt(i);
        adjusted_offset -= max_text_offset_in_parent;
      }

      text_position = std::move(child_position);
    } while (text_position->AnchorChildCount());

    DCHECK(text_position);
    DCHECK(text_position->IsLeafTextPosition());
    text_position->text_offset_ = adjusted_offset;
    // Leaf Text positions are always downstream since there is no ambiguity
    // as to whether it refers to the end of the current or the start of
    // the next line.
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    return text_position;
  }

  // Searches backwards and forwards from this position until it finds the given
  // text boundary, and creates an AXRange that spans from the former to the
  // latter. The resulting AXRange is always a forward range: its anchor always
  // comes before its focus in document order. The resulting AXRange is bounded
  // by the anchor of this position, i.e. the AXBoundaryBehavior is set to
  // StopAtAnchorBoundary. The exception is AXTextBoundary::kWebPage, where this
  // behavior won't make sense. This behavior is based on current platform needs
  // and might be relaxed if necessary in the future.
  //
  // Please note that |expand_behavior| should have no effect for
  // AXTextBoundary::kObject and AXTextBoundary::kWebPage because the range
  // should be the same regardless if we first move left or right.
  AXRangeType ExpandToEnclosingTextBoundary(
      AXTextBoundary boundary,
      AXRangeExpandBehavior expand_behavior) const {
    AXBoundaryBehavior boundary_behavior =
        AXBoundaryBehavior::StopAtAnchorBoundary;
    if (boundary == AXTextBoundary::kWebPage)
      boundary_behavior = AXBoundaryBehavior::CrossBoundary;

    switch (expand_behavior) {
      case AXRangeExpandBehavior::kLeftFirst: {
        AXPositionInstance left_position = CreatePositionAtTextBoundary(
            boundary, AXTextBoundaryDirection::kBackwards, boundary_behavior);
        AXPositionInstance right_position =
            left_position->CreatePositionAtTextBoundary(
                boundary, AXTextBoundaryDirection::kForwards,
                boundary_behavior);
        return AXRangeType(std::move(left_position), std::move(right_position));
      }
      case AXRangeExpandBehavior::kRightFirst: {
        AXPositionInstance right_position = CreatePositionAtTextBoundary(
            boundary, AXTextBoundaryDirection::kForwards, boundary_behavior);
        AXPositionInstance left_position =
            right_position->CreatePositionAtTextBoundary(
                boundary, AXTextBoundaryDirection::kBackwards,
                boundary_behavior);
        return AXRangeType(std::move(left_position), std::move(right_position));
      }
    }
  }

  // Starting from this position, moves in the given direction until it finds
  // the given text boundary, and creates a new position at that location.
  //
  // When a boundary has the "StartOrEnd" suffix, it means that this method will
  // find the start boundary when moving in the backwards direction, and the end
  // boundary when moving in the forwards direction.
  AXPositionInstance CreatePositionAtTextBoundary(
      AXTextBoundary boundary,
      AXTextBoundaryDirection direction,
      AXBoundaryBehavior boundary_behavior) const {
    AXPositionInstance resulting_position = CreateNullPosition();
    switch (boundary) {
      case AXTextBoundary::kCharacter:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousCharacterPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextCharacterPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kFormatChange:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousFormatStartPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextFormatEndPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kLineEnd:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousLineEndPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextLineEndPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kLineStart:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousLineStartPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextLineStartPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kLineStartOrEnd:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousLineStartPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextLineEndPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kObject:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position = CreatePositionAtStartOfAnchor();
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreatePositionAtEndOfAnchor();
            break;
        }
        break;

      case AXTextBoundary::kPageEnd:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousPageEndPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextPageEndPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kPageStart:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousPageStartPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextPageStartPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kPageStartOrEnd:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousPageStartPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextPageEndPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kParagraphEnd:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousParagraphEndPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position =
                CreateNextParagraphEndPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kParagraphStart:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousParagraphStartPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position =
                CreateNextParagraphStartPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kParagraphStartOrEnd:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousParagraphStartPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position =
                CreateNextParagraphEndPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kSentenceEnd:
        NOTREACHED() << "Sentence boundaries are not yet supported.";
        return CreateNullPosition();

      case AXTextBoundary::kSentenceStart:
        NOTREACHED() << "Sentence boundaries are not yet supported.";
        return CreateNullPosition();

      case AXTextBoundary::kSentenceStartOrEnd:
        NOTREACHED() << "Sentence boundaries are not yet supported.";
        return CreateNullPosition();

      case AXTextBoundary::kWebPage:
        DCHECK_EQ(boundary_behavior, AXBoundaryBehavior::CrossBoundary)
            << "We can't reach the start of the document if we are disallowed "
               "from crossing boundaries.";
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position = CreatePositionAtStartOfDocument();
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreatePositionAtEndOfDocument();
            break;
        }
        break;

      case AXTextBoundary::kWordEnd:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousWordEndPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextWordEndPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kWordStart:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousWordStartPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextWordStartPosition(boundary_behavior);
            break;
        }
        break;

      case AXTextBoundary::kWordStartOrEnd:
        switch (direction) {
          case AXTextBoundaryDirection::kBackwards:
            resulting_position =
                CreatePreviousWordStartPosition(boundary_behavior);
            break;
          case AXTextBoundaryDirection::kForwards:
            resulting_position = CreateNextWordEndPosition(boundary_behavior);
            break;
        }
        break;
    }
    return resulting_position;
  }

  AXPositionInstance CreatePositionAtStartOfAnchor() const {
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION:
        if (!AnchorChildCount()) {
          return CreateTreePosition(tree_id_, anchor_id_, BEFORE_TEXT);
        }
        return CreateTreePosition(tree_id_, anchor_id_, 0 /* child_index */);
      case AXPositionKind::TEXT_POSITION:
        return CreateTextPosition(tree_id_, anchor_id_, 0 /* text_offset */,
                                  ax::mojom::TextAffinity::kDownstream);
    }
    return CreateNullPosition();
  }

  AXPositionInstance CreatePositionAtEndOfAnchor() const {
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION:
        return CreateTreePosition(tree_id_, anchor_id_, AnchorChildCount());
      case AXPositionKind::TEXT_POSITION:
        return CreateTextPosition(tree_id_, anchor_id_, MaxTextOffset(),
                                  ax::mojom::TextAffinity::kDownstream);
    }
    return CreateNullPosition();
  }

  AXPositionInstance CreatePositionAtStartOfDocument() const {
    AXPositionInstance position =
        AsTreePosition()->CreateDocumentAncestorPosition();
    if (!position->IsNullPosition()) {
      position = position->CreatePositionAtStartOfAnchor();
      if (IsTextPosition())
        position = position->AsTextPosition();
    }
    return position;
  }

  AXPositionInstance CreatePositionAtEndOfDocument() const {
    AXPositionInstance position =
        AsTreePosition()->CreateDocumentAncestorPosition();
    if (!position->IsNullPosition()) {
      while (position->AnchorChildCount()) {
        position =
            position->CreateChildPositionAt(position->AnchorChildCount() - 1);
      }
      position = position->CreatePositionAtEndOfAnchor();
      if (IsTextPosition())
        position = position->AsTextPosition();
    }
    return position;
  }

  AXPositionInstance CreateChildPositionAt(int child_index) const {
    if (IsNullPosition())
      return CreateNullPosition();

    if (child_index < 0 || child_index >= AnchorChildCount())
      return CreateNullPosition();

    AXTreeID tree_id = AXTreeIDUnknown();
    AXNode::AXID child_id = AXNode::kInvalidAXID;
    AnchorChild(child_index, &tree_id, &child_id);
    DCHECK_NE(tree_id, AXTreeIDUnknown());
    DCHECK_NE(child_id, AXNode::kInvalidAXID);
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        NOTREACHED();
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION: {
        AXPositionInstance child_position =
            CreateTreePosition(tree_id, child_id, 0 /* child_index */);
        // If the child's anchor is a leaf node, make this a "before text"
        // position.
        if (!child_position->AnchorChildCount())
          child_position->child_index_ = BEFORE_TEXT;
        return child_position;
      }
      case AXPositionKind::TEXT_POSITION:
        return CreateTextPosition(tree_id, child_id, 0 /* text_offset */,
                                  ax::mojom::TextAffinity::kDownstream);
    }

    return CreateNullPosition();
  }

  AXPositionInstance CreateParentPosition() const {
    if (IsNullPosition())
      return CreateNullPosition();

    AXTreeID tree_id = AXTreeIDUnknown();
    AXNode::AXID parent_id = AXNode::kInvalidAXID;
    AnchorParent(&tree_id, &parent_id);
    if (tree_id == AXTreeIDUnknown() || parent_id == AXNode::kInvalidAXID)
      return CreateNullPosition();

    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        NOTREACHED();
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION:
        return CreateTreePosition(tree_id, parent_id, AnchorIndexInParent());
      case AXPositionKind::TEXT_POSITION: {
        // If our parent contains all our text, we need to maintain the affinity
        // and the text offset. Otherwise, we return a position that is either
        // before or after the child. We always recompute the affinity when the
        // position is after the child.
        // Recomputing the affinity is important because even though a text
        // position might unambiguously be at the end of a line, its parent
        // position might be the same as the parent position of the position
        // representing the start of the next line.
        const int max_text_offset = MaxTextOffset();
        const int max_text_offset_in_parent =
            IsEmbeddedObjectInParent() ? 1 : max_text_offset;
        int parent_offset = AnchorTextOffsetInParent();
        ax::mojom::TextAffinity parent_affinity = affinity_;
        if (max_text_offset == max_text_offset_in_parent) {
          parent_offset += text_offset_;
        } else if (text_offset_ > 0) {
          parent_offset += max_text_offset_in_parent;
          parent_affinity = ax::mojom::TextAffinity::kDownstream;
        }

        AXPositionInstance parent_position = CreateTextPosition(
            tree_id, parent_id, parent_offset, parent_affinity);
        if (parent_position->IsNullPosition()) {
          // Workaround: When the autofill feature populates a text field, it
          // doesn't immediately update its value, which causes the text inside
          // the user-agent shadow DOM to be different than the text in the text
          // field itself. As a result, the parent_offset calculated above might
          // appear to be temporarily invalid.
          // TODO(nektar): Fix this better by ensuring that the text field's
          // hypertext is always kept up to date.
          parent_position =
              CreateTextPosition(tree_id, parent_id, 0 /* text_offset */,
                                 ax::mojom::TextAffinity::kDownstream);
        }

        // We check if the parent position has introduced ambiguity as to
        // whether it refers to the end of the current or the start of the next
        // line. We do this check by creating the parent position and testing if
        // it is erroneously at the start of the next line. We could not have
        // checked if the child was at the end of the line, because our line end
        // testing logic takes into account line breaks, which don't apply in
        // this situation.
        if (text_offset_ == max_text_offset && parent_position->AtStartOfLine())
          parent_position->affinity_ = ax::mojom::TextAffinity::kUpstream;
        return parent_position;
      }
    }

    return CreateNullPosition();
  }

  // Creates a tree position using the next text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreateNextLeafTreePosition() const {
    return CreateNextLeafTreePosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Creates a tree position using the previous text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreatePreviousLeafTreePosition() const {
    return CreatePreviousLeafTreePosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Creates a text position using the next text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreateNextLeafTextPosition() const {
    return CreateNextTextAnchorPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Creates a text position using the previous text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreatePreviousLeafTextPosition() const {
    return CreatePreviousTextAnchorPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Returns a text position located right before the next character (from this
  // position) in the tree's text representation, following these conditions:
  //
  //   - If this position is at the end of its anchor, normalize it to the start
  //   of the next text anchor, regardless of the position's affinity.
  //   Both text positions are equal when compared, but we consider the start of
  //   an anchor to be a position BEFORE its first character and the end of the
  //   previous to be AFTER its last character.
  //
  //   - Skip any empty text anchors; they're "invisible" to the text
  //   representation and the next character could be ahead.
  //
  //   - Return a null position if there is no next character forward.
  //
  // If possible, return a position anchored at the current position's anchor;
  // this is necessary because we don't want to return any position that might
  // be located in the shadow DOM or in a position anchored at a node that is
  // not visible to a specific platform's APIs.
  //
  // Also, |text_offset| is adjusted to point to a valid character offset, i.e.
  // it cannot be pointing to a low surrogate pair or to the middle of a
  // grapheme cluster.
  AXPositionInstance AsLeafTextPositionBeforeCharacter() const {
    if (IsNullPosition())
      return Clone();

    AXPositionInstance text_position = AsTextPosition();
    // In case the input affinity is upstream, reset it to downstream.
    //
    // This is to ensure that when we find the equivalent leaf text position, it
    // will be at the start of anchor if the original position is anchored to a
    // node higher up in the tree and pointing to a text offset that falls on
    // the boundary between two leaf nodes. In other words, the returned
    // position will always be "before character".
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    text_position = text_position->AsLeafTextPosition();
    DCHECK(!text_position->IsNullPosition())
        << "Adjusting to a leaf position should never turn a non-null position "
           "into a null one.";

    if (!text_position->IsIgnored() && !text_position->AtEndOfAnchor()) {
      std::unique_ptr<base::i18n::BreakIterator> grapheme_iterator =
          text_position->GetGraphemeIterator();
      DCHECK_GE(text_position->text_offset_, 0);
      DCHECK_LE(text_position->text_offset_,
                int{text_position->name_.length()});
      while (
          !text_position->AtStartOfAnchor() &&
          (!gfx::IsValidCodePointIndex(text_position->name_,
                                       size_t{text_position->text_offset_}) ||
           (grapheme_iterator && !grapheme_iterator->IsGraphemeBoundary(
                                     size_t{text_position->text_offset_})))) {
        --text_position->text_offset_;
      }
      return text_position;
    }

    text_position = text_position->CreateNextLeafTextPosition();
    while (!text_position->IsNullPosition() &&
           (text_position->IsIgnored() || !text_position->MaxTextOffset())) {
      text_position = text_position->CreateNextLeafTextPosition();
    }
    return text_position;
  }

  // Returns a text position located right after the previous character (from
  // this position) in the tree's text representation.
  //
  // See `AsLeafTextPositionBeforeCharacter`, as this is its "reversed" version.
  AXPositionInstance AsLeafTextPositionAfterCharacter() const {
    if (IsNullPosition())
      return Clone();

    AXPositionInstance text_position = AsTextPosition();
    // Temporarily set the affinity to upstream.
    //
    // This is to ensure that when we find the equivalent leaf text position, it
    // will be at the end of anchor if the original position is anchored to a
    // node higher up in the tree and pointing to a text offset that falls on
    // the boundary between two leaf nodes. In other words, the returned
    // position will always be "after character".
    text_position->affinity_ = ax::mojom::TextAffinity::kUpstream;
    text_position = text_position->AsLeafTextPosition();
    DCHECK(!text_position->IsNullPosition())
        << "Adjusting to a leaf position should never turn a non-null position "
           "into a null one.";

    if (!text_position->IsIgnored() && !text_position->AtStartOfAnchor()) {
      std::unique_ptr<base::i18n::BreakIterator> grapheme_iterator =
          text_position->GetGraphemeIterator();
      DCHECK_GE(text_position->text_offset_, 0);
      DCHECK_LE(text_position->text_offset_,
                int{text_position->name_.length()});
      while (
          !text_position->AtEndOfAnchor() &&
          (!gfx::IsValidCodePointIndex(text_position->name_,
                                       size_t{text_position->text_offset_}) ||
           (grapheme_iterator && !grapheme_iterator->IsGraphemeBoundary(
                                     size_t{text_position->text_offset_})))) {
        ++text_position->text_offset_;
      }

      // Reset the affinity to downstream, because an upstream affinity doesn't
      // make sense on a leaf anchor.
      text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return text_position;
    }

    text_position = text_position->CreatePreviousLeafTextPosition();
    while (!text_position->IsNullPosition() &&
           (text_position->IsIgnored() || !text_position->MaxTextOffset())) {
      text_position = text_position->CreatePreviousLeafTextPosition();
    }
    return text_position->CreatePositionAtEndOfAnchor();
  }

  // Creates a position pointing to before the next character, which is defined
  // as the start of the next grapheme cluster. Also, ensures that the created
  // position will not point to a low surrogate pair.
  //
  // A grapheme cluster is what an end-user would consider a character and it
  // could include a letter with additional diacritics. It could be more than
  // one Unicode code unit in length.
  //
  // See also http://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries
  AXPositionInstance CreateNextCharacterPosition(
      AXBoundaryBehavior boundary_behavior) const {
    if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary &&
        AtEndOfAnchor()) {
      return Clone();
    }

    const bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPositionBeforeCharacter();

    // There is no next character position.
    if (text_position->IsNullPosition()) {
      if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary ||
          boundary_behavior == AXBoundaryBehavior::StopAtLastAnchorBoundary) {
        text_position = Clone();
      }
      return text_position;
    }

    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        *text_position == *this) {
      return Clone();
    }

    DCHECK_LT(text_position->text_offset_, text_position->MaxTextOffset());
    std::unique_ptr<base::i18n::BreakIterator> grapheme_iterator =
        text_position->GetGraphemeIterator();
    do {
      ++text_position->text_offset_;
    } while (!text_position->AtEndOfAnchor() && grapheme_iterator &&
             !grapheme_iterator->IsGraphemeBoundary(
                 size_t{text_position->text_offset_}));
    DCHECK_GT(text_position->text_offset_, 0);
    DCHECK_LE(text_position->text_offset_, text_position->MaxTextOffset());

    // If the character boundary is in the same subtree, return a position
    // rooted at this position's anchor. This is necessary because we don't want
    // to return a position that might be in the shadow DOM when this position
    // is not.
    const AXNodeType* common_anchor = text_position->LowestCommonAnchor(*this);
    if (GetAnchor() == common_anchor) {
      text_position = text_position->CreateAncestorPosition(common_anchor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      NOTREACHED() << "Original text position was not at end of anchor.";
    }

    // Even if the resulting position is right on a soft line break, affinity is
    // defaulted to downstream so that this method will always produce the same
    // result regardless of the direction of motion or the input affinity.
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;

    if (was_tree_position)
      return text_position->AsTreePosition();
    return text_position;
  }

  // Creates a position pointing to before the previous character, which is
  // defined as the start of the previous grapheme cluster. Also, ensures that
  // the created position will not point to a low surrogate pair.
  //
  // See the comment above `CreateNextCharacterPosition` for the definition of a
  // grapheme cluster.
  AXPositionInstance CreatePreviousCharacterPosition(
      AXBoundaryBehavior boundary_behavior) const {
    if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary &&
        AtStartOfAnchor()) {
      return Clone();
    }

    const bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPositionAfterCharacter();

    // There is no previous character position.
    if (text_position->IsNullPosition()) {
      if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary ||
          boundary_behavior == AXBoundaryBehavior::StopAtLastAnchorBoundary) {
        text_position = Clone();
      }
      return text_position;
    }

    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        *text_position == *this) {
      return Clone();
    }

    DCHECK_GT(text_position->text_offset_, 0);
    std::unique_ptr<base::i18n::BreakIterator> grapheme_iterator =
        text_position->GetGraphemeIterator();
    do {
      --text_position->text_offset_;
    } while (!text_position->AtStartOfAnchor() && grapheme_iterator &&
             !grapheme_iterator->IsGraphemeBoundary(
                 size_t{text_position->text_offset_}));
    DCHECK_GE(text_position->text_offset_, 0);
    DCHECK_LT(text_position->text_offset_, text_position->MaxTextOffset());

    // The character boundary should be in the same subtree. Return a position
    // rooted at this position's anchor. This is necessary because we don't want
    // to return a position that might be in the shadow DOM when this position
    // is not.
    const AXNodeType* common_anchor = text_position->LowestCommonAnchor(*this);
    if (GetAnchor() == common_anchor) {
      text_position = text_position->CreateAncestorPosition(common_anchor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      NOTREACHED() << "Original text position was not at start of anchor.";
    }

    // Even if the resulting position is right on a soft line break, affinity is
    // defaulted to downstream so that this method will always produce the same
    // result regardless of the direction of motion or the input affinity.
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;

    if (was_tree_position)
      return text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreateNextWordStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryStartPosition(
        boundary_behavior, AXTextBoundaryDirection::kForwards,
        base::BindRepeating(&AtStartOfWordPredicate),
        base::BindRepeating(&AtEndOfWordPredicate),
        base::BindRepeating(&GetWordStartOffsetsFunc));
  }

  AXPositionInstance CreatePreviousWordStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryStartPosition(
        boundary_behavior, AXTextBoundaryDirection::kBackwards,
        base::BindRepeating(&AtStartOfWordPredicate),
        base::BindRepeating(&AtEndOfWordPredicate),
        base::BindRepeating(&GetWordStartOffsetsFunc));
      }

  // Word end positions are one past the last character of the word.
  AXPositionInstance CreateNextWordEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryEndPosition(
        boundary_behavior, AXTextBoundaryDirection::kForwards,
        base::BindRepeating(&AtStartOfWordPredicate),
        base::BindRepeating(&AtEndOfWordPredicate),
        base::BindRepeating(&GetWordEndOffsetsFunc));
  }

  // Word end positions are one past the last character of the word.
  AXPositionInstance CreatePreviousWordEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryEndPosition(
        boundary_behavior, AXTextBoundaryDirection::kBackwards,
        base::BindRepeating(&AtStartOfWordPredicate),
        base::BindRepeating(&AtEndOfWordPredicate),
        base::BindRepeating(&GetWordEndOffsetsFunc));
  }

  AXPositionInstance CreateNextLineStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryStartPosition(
        boundary_behavior, AXTextBoundaryDirection::kForwards,
        base::BindRepeating(&AtStartOfLinePredicate),
        base::BindRepeating(&AtEndOfLinePredicate));
  }

  AXPositionInstance CreatePreviousLineStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryStartPosition(
        boundary_behavior, AXTextBoundaryDirection::kBackwards,
        base::BindRepeating(&AtStartOfLinePredicate),
        base::BindRepeating(&AtEndOfLinePredicate));
  }

  // Line end positions are one past the last character of the line, excluding
  // any white space or newline characters that separate the lines.
  AXPositionInstance CreateNextLineEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryEndPosition(
        boundary_behavior, AXTextBoundaryDirection::kForwards,
        base::BindRepeating(&AtStartOfLinePredicate),
        base::BindRepeating(&AtEndOfLinePredicate));
  }

  // Line end positions are one past the last character of the line, excluding
  // any white space or newline characters separating the lines.
  AXPositionInstance CreatePreviousLineEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryEndPosition(
        boundary_behavior, AXTextBoundaryDirection::kBackwards,
        base::BindRepeating(&AtStartOfLinePredicate),
        base::BindRepeating(&AtEndOfLinePredicate));
  }

  AXPositionInstance CreatePreviousFormatStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    if (IsNullPosition())
      return Clone();

    // AtStartOfFormat() always returns true if we are at the first iterable
    // position, i.e. CreatePreviousLeafTreePosition()->IsNullPosition().
    if (AtStartOfFormat()) {
      if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary ||
          (boundary_behavior == AXBoundaryBehavior::StopAtLastAnchorBoundary &&
           CreatePreviousLeafTreePosition()->IsNullPosition())) {
        AXPositionInstance clone = Clone();
        // In order to make equality checks simpler, affinity should be reset so
        // that we would get consistent output from this function regardless of
        // input affinity.
        clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
        return clone;
      } else if (boundary_behavior == AXBoundaryBehavior::CrossBoundary &&
                 CreatePreviousLeafTreePosition()->IsNullPosition()) {
        // If we're at a format boundary and there are no more text positions
        // to traverse, return a null position for cross-boundary moves.
        return CreateNullPosition();
      }
    }

    const bool was_text_position = IsTextPosition();
    AXPositionInstance tree_position =
        AsTreePosition()->CreatePositionAtStartOfAnchor();
    AXPositionInstance previous_tree_position =
        tree_position->CreatePreviousLeafTreePosition();

    // If moving to the start of the current anchor hasn't changed our position
    // from the original position, we need to test the previous leaf tree
    // position.
    if (AtStartOfAnchor() &&
        boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary) {
      tree_position = std::move(previous_tree_position);
      previous_tree_position = tree_position->CreatePreviousLeafTreePosition();
    }

    // The first position in the document is also a format start boundary, so we
    // should not return NullPosition unless we started from that location.
    while (!previous_tree_position->IsNullPosition() &&
           !tree_position->AtStartOfFormat()) {
      tree_position = std::move(previous_tree_position);
      previous_tree_position = tree_position->CreatePreviousLeafTreePosition();
    }

    // If the format boundary is in the same subtree, return a position rooted
    // at the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    const AXNodeType* common_anchor = tree_position->LowestCommonAnchor(*this);
    if (GetAnchor() == common_anchor) {
      tree_position = tree_position->CreateAncestorPosition(common_anchor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_text_position)
      tree_position = tree_position->AsTextPosition();
    return tree_position;
  }

  AXPositionInstance CreateNextFormatEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    if (IsNullPosition())
      return Clone();

    // AtEndOfFormat() always returns true if we are at the last iterable
    // position, i.e. CreateNextLeafTreePosition()->IsNullPosition().
    if (AtEndOfFormat()) {
      if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary ||
          (boundary_behavior == AXBoundaryBehavior::StopAtLastAnchorBoundary &&
           CreateNextLeafTreePosition()->IsNullPosition())) {
        AXPositionInstance clone = Clone();
        // In order to make equality checks simpler, affinity should be reset so
        // that we would get consistent output from this function regardless of
        // input affinity.
        clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
        return clone;
      } else if (boundary_behavior == AXBoundaryBehavior::CrossBoundary &&
                 CreateNextLeafTreePosition()->IsNullPosition()) {
        // If we're at a format boundary and there are no more text positions
        // to traverse, return a null position for cross-boundary moves.
        return CreateNullPosition();
      }
    }

    const bool was_text_position = IsTextPosition();
    AXPositionInstance tree_position =
        AsTreePosition()->CreatePositionAtEndOfAnchor();
    AXPositionInstance next_tree_position =
        tree_position->CreateNextLeafTreePosition()
            ->CreatePositionAtEndOfAnchor();

    // If moving to the end of the current anchor hasn't changed our original
    // position, we need to test the next leaf tree position.
    if (AtEndOfAnchor() &&
        boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary) {
      tree_position = std::move(next_tree_position);
      next_tree_position = tree_position->CreateNextLeafTreePosition()
                               ->CreatePositionAtEndOfAnchor();
    }

    // The last position in the document is also a format end boundary, so we
    // should not return NullPosition unless we started from that location.
    while (!next_tree_position->IsNullPosition() &&
           !tree_position->AtEndOfFormat()) {
      tree_position = std::move(next_tree_position);
      next_tree_position = tree_position->CreateNextLeafTreePosition()
                               ->CreatePositionAtEndOfAnchor();
    }

    // If the format boundary is in the same subtree, return a position
    // rooted at the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    const AXNodeType* common_anchor = tree_position->LowestCommonAnchor(*this);
    if (GetAnchor() == common_anchor) {
      tree_position = tree_position->CreateAncestorPosition(common_anchor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_text_position)
      tree_position = tree_position->AsTextPosition();
    return tree_position;
  }

  AXPositionInstance CreateNextParagraphStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryStartPosition(
        boundary_behavior, AXTextBoundaryDirection::kForwards,
        base::BindRepeating(&AtStartOfParagraphPredicate),
        base::BindRepeating(&AtEndOfParagraphPredicate));
  }

  AXPositionInstance CreatePreviousParagraphStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryStartPosition(
        boundary_behavior, AXTextBoundaryDirection::kBackwards,
        base::BindRepeating(&AtStartOfParagraphPredicate),
        base::BindRepeating(&AtEndOfParagraphPredicate));
  }

  AXPositionInstance CreateNextParagraphEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryEndPosition(
        boundary_behavior, AXTextBoundaryDirection::kForwards,
        base::BindRepeating(&AtStartOfParagraphPredicate),
        base::BindRepeating(&AtEndOfParagraphPredicate));
  }

  AXPositionInstance CreatePreviousParagraphEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    AXPositionInstance previous_position = CreateBoundaryEndPosition(
        boundary_behavior, AXTextBoundaryDirection::kBackwards,
        base::BindRepeating(&AtStartOfParagraphPredicate),
        base::BindRepeating(&AtEndOfParagraphPredicate));
    if (boundary_behavior == AXBoundaryBehavior::CrossBoundary ||
        boundary_behavior == AXBoundaryBehavior::StopAtLastAnchorBoundary) {
      // This is asymmetric with CreateNextParagraphEndPosition due to
      // asymmetries in text anchor movement. Consider:
      //
      // ++1 rootWebArea
      // ++++2 staticText name="FIRST"
      // ++++3 genericContainer isLineBreakingObject=true
      // ++++++4 genericContainer isLineBreakingObject=true
      // ++++++5 staticText name="SECOND"
      //
      // Node 2 offset 5 FIRST<> is a paragraph end since node 3 is a line-
      // breaking object that's not collapsible (since it's not a leaf). When
      // looking for the next text anchor position from there, we advance to
      // sibling node 3, then since that node has descendants, we convert to a
      // tree position to find the leaf node that maps to "node 3 offset 0".
      // Since node 4 has no text, we skip it and land on node 5. We end up at
      // node 5 offset 6 SECOND<> as our next paragraph end.
      //
      // The set of paragraph ends should be consistent when moving in the
      // reverse direction. But starting from node 5 offset 6, the previous text
      // anchor position is previous sibling node 4. We'll consider that a
      // paragraph end since it's a leaf line-breaking object and stop.
      //
      // Essentially, we have two consecutive line-breaking objects, each of
      // which stops movement in the "outward" direction, for different reasons.
      //
      // We handle this by looking back one more step after finding a candidate
      // for previous paragraph end, then testing a forward step from the look-
      // back position. That will land us on the candidate position if it's a
      // valid paragraph boundary.
      //
      while (!previous_position->IsNullPosition()) {
        AXPositionInstance look_back_position =
            previous_position->AsLeafTextPosition()
                ->CreatePreviousLeafTextPosition()
                ->CreatePositionAtEndOfAnchor();
        if (look_back_position->IsNullPosition()) {
          // Nowhere to look back to, so our candidate must be a valid paragraph
          // boundary.
          break;
        }
        AXPositionInstance forward_step_position =
            look_back_position->CreateNextLeafTextPosition()
                ->CreatePositionAtEndOfAnchor();
        if (*forward_step_position == *previous_position)
          break;

        previous_position = previous_position->CreateBoundaryEndPosition(
            boundary_behavior, AXTextBoundaryDirection::kBackwards,
            base::BindRepeating(&AtStartOfParagraphPredicate),
            base::BindRepeating(&AtEndOfParagraphPredicate));
      }
    }

    return previous_position;
  }

  AXPositionInstance CreateNextPageStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryStartPosition(
        boundary_behavior, AXTextBoundaryDirection::kForwards,
        base::BindRepeating(&AtStartOfPagePredicate),
        base::BindRepeating(&AtEndOfPagePredicate));
  }

  AXPositionInstance CreatePreviousPageStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryStartPosition(
        boundary_behavior, AXTextBoundaryDirection::kBackwards,
        base::BindRepeating(&AtStartOfPagePredicate),
        base::BindRepeating(&AtEndOfPagePredicate));
  }

  AXPositionInstance CreateNextPageEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryEndPosition(
        boundary_behavior, AXTextBoundaryDirection::kForwards,
        base::BindRepeating(&AtStartOfPagePredicate),
        base::BindRepeating(&AtEndOfPagePredicate));
  }

  AXPositionInstance CreatePreviousPageEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    return CreateBoundaryEndPosition(
        boundary_behavior, AXTextBoundaryDirection::kBackwards,
        base::BindRepeating(&AtStartOfPagePredicate),
        base::BindRepeating(&AtEndOfPagePredicate));
  }

  AXPositionInstance CreateBoundaryStartPosition(
      AXBoundaryBehavior boundary_behavior,
      AXTextBoundaryDirection boundary_direction,
      BoundaryConditionPredicate at_start_condition,
      BoundaryConditionPredicate at_end_condition,
      BoundaryTextOffsetsFunc get_start_offsets =
          BoundaryTextOffsetsFunc()) const {
    const bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->IsNullPosition())
      return text_position;

    while (!at_start_condition.Run(text_position) ||
           (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
            *this == *text_position)) {
      if (*this == *text_position) {
        AXPositionInstance next_position =
            text_position->CreatePositionAtNextOffsetBoundary(
                boundary_direction, get_start_offsets);
        if (*next_position != *text_position) {
          text_position = std::move(next_position);
          break;
        }
      }

      AXPositionInstance next_position;
      if (boundary_direction == AXTextBoundaryDirection::kForwards) {
        next_position = text_position->CreateNextLeafTextPosition();
      } else {
        if (text_position->AtStartOfAnchor()) {
          next_position = text_position->CreatePreviousLeafTextPosition();
        } else {
          text_position = text_position->CreatePositionAtStartOfAnchor();
          DCHECK(!text_position->IsNullPosition());
          continue;
        }
      }

      if (next_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
          return (boundary_direction == AXTextBoundaryDirection::kForwards)
                     ? CreatePositionAtEndOfAnchor()
                     : CreatePositionAtStartOfAnchor();
        }
        if (boundary_behavior == AXBoundaryBehavior::StopAtLastAnchorBoundary) {
          // We can't simply return the following position; break and after this
          // loop we'll try to do some adjustments to text_position.
          text_position =
              (boundary_direction == AXTextBoundaryDirection::kForwards)
                  ? text_position->CreatePositionAtEndOfAnchor()
                  : text_position->CreatePositionAtStartOfAnchor();
          break;
        }
        return next_position;
      }

      // Continue searching for the next boundary start in the specified
      // direction until the next logical text position is reached.
      text_position = next_position->CreatePositionAtFirstOffsetBoundary(
          boundary_direction, get_start_offsets);
    }

    // If the boundary is in the same subtree, return a position rooted at this
    // position's anchor. This is necessary because we don't want to return a
    // position that might be in the shadow DOM when this position is not.
    const AXNodeType* common_anchor = text_position->LowestCommonAnchor(*this);
    if (GetAnchor() == common_anchor) {
      text_position = text_position->CreateAncestorPosition(common_anchor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return (boundary_direction == AXTextBoundaryDirection::kForwards)
                 ? CreatePositionAtEndOfAnchor()
                 : CreatePositionAtStartOfAnchor();
    }

    // Affinity is only upstream at the end of a line, and so a start boundary
    // will never have an upstream affinity.
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreateBoundaryEndPosition(
      AXBoundaryBehavior boundary_behavior,
      AXTextBoundaryDirection boundary_direction,
      BoundaryConditionPredicate at_start_condition,
      BoundaryConditionPredicate at_end_condition,
      BoundaryTextOffsetsFunc get_end_offsets =
          BoundaryTextOffsetsFunc()) const {
    const bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->IsNullPosition())
      return text_position;

    while (!at_end_condition.Run(text_position) ||
           (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
            *this == *text_position)) {
      if (*this == *text_position) {
        AXPositionInstance next_position =
            text_position->CreatePositionAtNextOffsetBoundary(
                boundary_direction, get_end_offsets);
        if (*next_position != *text_position) {
          text_position = std::move(next_position);
          break;
        }
      }

      AXPositionInstance next_position;
      if (boundary_direction == AXTextBoundaryDirection::kForwards) {
        if (text_position->AtEndOfAnchor()) {
          next_position = text_position->CreateNextLeafTextPosition();
        } else {
          text_position = text_position->CreatePositionAtEndOfAnchor();
          DCHECK(!text_position->IsNullPosition());
          continue;
        }
      } else {
        next_position = text_position->CreatePreviousLeafTextPosition()
                            ->CreatePositionAtEndOfAnchor();
      }

      if (next_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
          return (boundary_direction == AXTextBoundaryDirection::kForwards)
                     ? CreatePositionAtEndOfAnchor()
                     : CreatePositionAtStartOfAnchor();
        }
        if (boundary_behavior == AXBoundaryBehavior::StopAtLastAnchorBoundary) {
          // We can't simply return the following position; break and after this
          // loop we'll try to do some adjustments to text_position.
          text_position =
              (boundary_direction == AXTextBoundaryDirection::kForwards)
                  ? text_position->CreatePositionAtEndOfAnchor()
                  : text_position->CreatePositionAtStartOfAnchor();
          break;
        }
        return next_position;
      }

      // Continue searching for the next boundary end in the specified direction
      // until the next logical text position is reached.
      text_position = next_position->CreatePositionAtFirstOffsetBoundary(
          boundary_direction, get_end_offsets);
    }

    // If the boundary is in the same subtree, return a position rooted at this
    // position's anchor. This is necessary because we don't want to return a
    // position that might be in the shadow DOM when this position is not.
    const AXNodeType* common_anchor = text_position->LowestCommonAnchor(*this);
    if (GetAnchor() == common_anchor) {
      text_position = text_position->CreateAncestorPosition(common_anchor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return (boundary_direction == AXTextBoundaryDirection::kForwards)
                 ? CreatePositionAtEndOfAnchor()
                 : CreatePositionAtStartOfAnchor();
    }

    // If there is no ambiguity as to whether the position is at the end of
    // the current boundary or the start of the next boundary, an upstream
    // affinity should be reset to downstream in order to get consistent output
    // from this method, regardless of input affinity.
    //
    // Note that there could be no ambiguity if the boundary is either at the
    // start or the end of the current anchor, so we should always reset to
    // downstream affinity in those cases.
    if (text_position->affinity_ == ax::mojom::TextAffinity::kUpstream) {
      AXPositionInstance downstream_position = text_position->Clone();
      downstream_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (downstream_position->AtStartOfAnchor() ||
          downstream_position->AtEndOfAnchor() ||
          !at_start_condition.Run(downstream_position)) {
        text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
      }
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // TODO(nektar): Add sentence navigation methods.

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreateNextAnchorPosition() const {
    return CreateNextAnchorPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreatePreviousAnchorPosition() const {
    return CreatePreviousAnchorPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Returns an optional integer indicating the logical order of this position
  // compared to another position or returns an empty optional if the positions
  // are not comparable. Any text position at the same character location is
  // logically equivalent although they may be on different anchors or have
  // different text offsets. Positions are not comparable when one position is
  // null and the other is not or if the positions do not have any common
  // ancestor.
  //    0: if this position is logically equivalent to the other position
  //   <0: if this position is logically less than the other position
  //   >0: if this position is logically greater than the other position
  base::Optional<int> CompareTo(const AXPosition& other) const {
    if (this->IsNullPosition() && other.IsNullPosition())
      return base::Optional<int>(0);
    if (this->IsNullPosition() || other.IsNullPosition())
      return base::Optional<int>(base::nullopt);

    // It is potentially costly to compute the parent position of a text
    // position, whilst computing the parent position of a tree position is
    // really inexpensive. In order to find the lowest common ancestor,
    // especially if that ancestor is all the way up to the root of the tree,
    // this will need to be done repeatedly. We avoid the performance hit by
    // converting both positions to tree positions and only falling back to text
    // positions if both are text positions and the lowest common ancestor is
    // not one of their anchors. Essentially, the question we need to answer is:
    // "When are two non equivalent positions going to have the same lowest
    // common ancestor position when converted to tree positions?" The answer is
    // when they are both text positions and they either have the same anchor,
    // or one is the ancestor of the other.
    const AXNodeType* common_anchor = this->LowestCommonAnchor(other);
    if (!common_anchor)
      return base::Optional<int>(base::nullopt);

    // Attempt to avoid recomputing the lowest common ancestor because we may
    // already have its anchor in which case just find the text offset.
    if (this->IsTextPosition() && other.IsTextPosition()) {
      // This text position's anchor is the common ancestor of the other text
      // position's anchor.
      if (this->GetAnchor() == common_anchor) {
        AXPositionInstance other_text_position =
            other.CreateAncestorPosition(common_anchor);
        return base::Optional<int>(this->text_offset_ -
                                   other_text_position->text_offset_);
      }

      // The other text position's anchor is the common ancestor of this text
      // position's anchor.
      if (other.GetAnchor() == common_anchor) {
        AXPositionInstance this_text_position =
            this->CreateAncestorPosition(common_anchor);
        return base::Optional<int>(this_text_position->text_offset_ -
                                   other.text_offset_);
      }

      // All optimizations failed. Fall back to comparing text positions with
      // the common text position ancestor.
      AXPositionInstance this_text_position_ancestor =
          this->CreateAncestorPosition(common_anchor);
      AXPositionInstance other_text_position_ancestor =
          other.CreateAncestorPosition(common_anchor);
      DCHECK(this_text_position_ancestor->IsTextPosition());
      DCHECK(other_text_position_ancestor->IsTextPosition());
      DCHECK_EQ(common_anchor, this_text_position_ancestor->GetAnchor());
      DCHECK_EQ(common_anchor, other_text_position_ancestor->GetAnchor());

      // TODO - This does not take into account |affinity_|, so we may return
      // a false positive when comparing at the end of a line.
      // For example :
      // ++1 kRootWebArea
      // ++++2 kTextField "Line 1\nLine 2"
      // ++++++3 kStaticText "Line 1"
      // ++++++++4 kInlineTextBox "Line 1"
      // ++++++5 kLineBreak "\n"
      // ++++++6 kStaticText "Line 2"
      // ++++++++7 kInlineTextBox "Line 2"
      //
      // TextPosition anchor_id=5 text_offset=1
      // affinity=downstream annotated_text=\n<>
      //
      // TextPosition anchor_id=7 text_offset=0
      // affinity=downstream annotated_text=<L>ine 2
      //
      // |LowestCommonAncestor| for both will be :
      // TextPosition anchor_id=2 text_offset=7
      // ... except anchor_id=5 creates a kUpstream position, while
      // anchor_id=7 creates a kDownstream position.
      return base::Optional<int>(this_text_position_ancestor->text_offset_ -
                                 other_text_position_ancestor->text_offset_);
    }

    // All optimizations failed. Fall back to comparing child index with
    // the common tree position ancestor.
    AXPositionInstance this_tree_position_ancestor =
        this->AsTreePosition()->CreateAncestorPosition(common_anchor);
    AXPositionInstance other_tree_position_ancestor =
        other.AsTreePosition()->CreateAncestorPosition(common_anchor);
    DCHECK(this_tree_position_ancestor->IsTreePosition());
    DCHECK(other_tree_position_ancestor->IsTreePosition());
    DCHECK_EQ(common_anchor, this_tree_position_ancestor->GetAnchor());
    DCHECK_EQ(common_anchor, other_tree_position_ancestor->GetAnchor());

    return base::Optional<int>(this_tree_position_ancestor->child_index() -
                               other_tree_position_ancestor->child_index());
  }

  void swap(AXPosition& other) {
    std::swap(kind_, other.kind_);
    std::swap(tree_id_, other.tree_id_);
    std::swap(anchor_id_, other.anchor_id_);
    std::swap(child_index_, other.child_index_);
    std::swap(text_offset_, other.text_offset_);
    std::swap(affinity_, other.affinity_);
    // We explicitly don't swap any cached members.
    name_ = base::string16();
    other.name_ = base::string16();
  }

  // Abstract methods.

  // Returns the text that is present inside the anchor node, including any text
  // found in descendant text nodes, based on the platform's text
  // representation. Some platforms use an embedded object character that
  // replaces the text coming from each child node.
  virtual base::string16 GetText() const = 0;

  // Determines if the anchor containing this position is a <br> or a text
  // object whose parent's anchor is an enclosing <br>.
  virtual bool IsInLineBreak() const = 0;

  // Determines if the anchor containing this position is a text object.
  virtual bool IsInTextObject() const = 0;

  // Determines if the text representation of this position's anchor contains
  // only whitespace characters; <br> objects span a single '\n' character, so
  // positions inside line breaks are also considered "in whitespace".
  virtual bool IsInWhiteSpace() const = 0;

  // Returns the length of the text that is present inside the anchor node,
  // including any text found in descendant text nodes. This is based on the
  // platform's text representation. Some platforms use an embedded object
  // character that replaces the text coming from each child node.
  //
  // Similar to "text_offset_", the length of the text is in UTF16 code units,
  // not in grapheme clusters.
  virtual int MaxTextOffset() const {
    if (IsNullPosition())
      return INVALID_OFFSET;
    return int{GetText().length()};
  }

 protected:
  AXPosition()
      : kind_(AXPositionKind::NULL_POSITION),
        tree_id_(AXTreeIDUnknown()),
        anchor_id_(AXNode::kInvalidAXID),
        child_index_(INVALID_INDEX),
        text_offset_(INVALID_OFFSET),
        affinity_(ax::mojom::TextAffinity::kDownstream) {}

  // We explicitly don't copy any cached members.
  AXPosition(const AXPosition& other)
      : kind_(other.kind_),
        tree_id_(other.tree_id_),
        anchor_id_(other.anchor_id_),
        child_index_(other.child_index_),
        text_offset_(other.text_offset_),
        affinity_(other.affinity_),
        name_() {}

  // Returns the character offset inside our anchor's parent at which our text
  // starts.
  int AnchorTextOffsetInParent() const {
    if (IsNullPosition())
      return INVALID_OFFSET;

    // Calculate how much text there is to the left of this anchor.
    AXPositionInstance tree_position = AsTreePosition();
    DCHECK(tree_position);
    AXPositionInstance parent_position = tree_position->CreateParentPosition();
    DCHECK(parent_position);
    if (parent_position->IsNullPosition())
      return 0;

    int offset_in_parent = 0;
    for (int i = 0; i < parent_position->child_index(); ++i) {
      AXPositionInstance child = parent_position->CreateChildPositionAt(i);
      DCHECK(child);
      offset_in_parent += child->MaxTextOffsetInParent();
    }
    return offset_in_parent;
  }

  // In the case of a text position, lazily initializes or returns the existing
  // grapheme iterator for the position's text. The grapheme iterator breaks at
  // every grapheme cluster boundary.
  //
  // We only allow creating this iterator on leaf nodes. We currently don't need
  // to move by grapheme boundaries on non-leaf nodes and computing plus caching
  // the inner text for all nodes is costly.
  std::unique_ptr<base::i18n::BreakIterator> GetGraphemeIterator() const {
    if (!IsTextPosition() || AnchorChildCount())
      return {};

    name_ = GetText();
    auto grapheme_iterator = std::make_unique<base::i18n::BreakIterator>(
        name_, base::i18n::BreakIterator::BREAK_CHARACTER);
    if (!grapheme_iterator->Init())
      return {};
    return grapheme_iterator;
  }

  void Initialize(AXPositionKind kind,
                  AXTreeID tree_id,
                  int32_t anchor_id,
                  int child_index,
                  int text_offset,
                  ax::mojom::TextAffinity affinity) {
    kind_ = kind;
    tree_id_ = tree_id;
    anchor_id_ = anchor_id;
    child_index_ = child_index;
    text_offset_ = text_offset;
    affinity_ = affinity;

    if (!IsValid()) {
      // Reset to the null position.
      kind_ = AXPositionKind::NULL_POSITION;
      tree_id_ = AXTreeIDUnknown();
      anchor_id_ = AXNode::kInvalidAXID;
      child_index_ = INVALID_INDEX;
      text_offset_ = INVALID_OFFSET;
      affinity_ = ax::mojom::TextAffinity::kDownstream;
    }
  }

  // Abstract methods.
  virtual void AnchorChild(int child_index,
                           AXTreeID* tree_id,
                           int32_t* child_id) const = 0;
  virtual int AnchorChildCount() const = 0;
  virtual int AnchorIndexInParent() const = 0;
  virtual base::stack<AXNodeType*> GetAncestorAnchors() const = 0;
  virtual void AnchorParent(AXTreeID* tree_id, int32_t* parent_id) const = 0;
  virtual AXNodeType* GetNodeInTree(AXTreeID tree_id,
                                    int32_t node_id) const = 0;

  // Returns the length of text that this anchor node takes up in its parent.
  // On some platforms, embedded objects are represented in their parent with a
  // single embedded object character.
  int MaxTextOffsetInParent() const {
    return IsEmbeddedObjectInParent() ? 1 : MaxTextOffset();
  }

  // Returns whether or not this anchor is represented in their parent with a
  // single embedded object character.
  virtual bool IsEmbeddedObjectInParent() const { return false; }

  // Determines if the anchor containing this position produces a hard line
  // break in the text representation, e.g. a block level element or a <br>.
  virtual bool IsInLineBreakingObject() const = 0;

  virtual ax::mojom::Role GetRole() const = 0;
  virtual AXNodeTextStyles GetTextStyles() const = 0;
  virtual std::vector<int32_t> GetWordStartOffsets() const = 0;
  virtual std::vector<int32_t> GetWordEndOffsets() const = 0;
  virtual int32_t GetNextOnLineID(int32_t node_id) const = 0;
  virtual int32_t GetPreviousOnLineID(int32_t node_id) const = 0;

 private:
  // Defines the relationship between positions during traversal.
  // For example, moving from a descendant to an ancestor, is a kAncestor move.
  enum class AXMoveType {
    kAncestor,
    kDescendant,
    kSibling,
  };

  // Defines the direction of position movement, either next / previous in tree.
  enum class AXMoveDirection {
    kNextInTree,
    kPreviousInTree,
  };

  // Type of predicate function called during anchor navigation.
  // When the predicate returns |true|, the navigation stops and returns a
  // null position object.
  using AbortMovePredicate =
      base::RepeatingCallback<bool(const AXPosition& move_from,
                                   const AXPosition& move_to,
                                   const AXMoveType type,
                                   const AXMoveDirection direction)>;

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreateNextAnchorPosition(
      const AbortMovePredicate& abort_predicate) const {
    if (IsNullPosition())
      return CreateNullPosition();

    AXPositionInstance current_position = AsTreePosition();
    DCHECK(!current_position->IsNullPosition());

    if (AnchorChildCount()) {
      const int child_index = current_position->child_index_;
      if (child_index < current_position->AnchorChildCount()) {
        AXPositionInstance child_position =
            current_position->CreateChildPositionAt(child_index);

        if (abort_predicate.Run(*current_position, *child_position,
                                AXMoveType::kDescendant,
                                AXMoveDirection::kNextInTree)) {
          return CreateNullPosition();
        }
        return child_position;
      }
    }

    AXPositionInstance parent_position =
        current_position->CreateParentPosition();

    // Get the next sibling if it exists, otherwise move up the AXTree to the
    // lowest next sibling of this position's ancestors.
    while (!parent_position->IsNullPosition()) {
      const int index_in_parent = current_position->AnchorIndexInParent();
      if (index_in_parent + 1 < parent_position->AnchorChildCount()) {
        AXPositionInstance next_sibling =
            parent_position->CreateChildPositionAt(index_in_parent + 1);
        DCHECK(!next_sibling->IsNullPosition());

        if (abort_predicate.Run(*current_position, *next_sibling,
                                AXMoveType::kSibling,
                                AXMoveDirection::kNextInTree)) {
          return CreateNullPosition();
        }
        return next_sibling;
      }

      if (abort_predicate.Run(*current_position, *parent_position,
                              AXMoveType::kAncestor,
                              AXMoveDirection::kNextInTree)) {
        return CreateNullPosition();
      }

      current_position = std::move(parent_position);
      parent_position = current_position->CreateParentPosition();
    }
    return CreateNullPosition();
  }

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreatePreviousAnchorPosition(
      const AbortMovePredicate& abort_predicate) const {
    if (IsNullPosition())
      return CreateNullPosition();

    AXPositionInstance current_position = AsTreePosition();
    DCHECK(!current_position->IsNullPosition());

    AXPositionInstance parent_position =
        current_position->CreateParentPosition();
    if (parent_position->IsNullPosition())
      return CreateNullPosition();

    // If there is no previous sibling, move up to the parent.
    const int index_in_parent = current_position->AnchorIndexInParent();
    if (index_in_parent <= 0) {
      if (abort_predicate.Run(*current_position, *parent_position,
                              AXMoveType::kAncestor,
                              AXMoveDirection::kPreviousInTree)) {
        return CreateNullPosition();
      }
      return parent_position;
    }

    // Get the previous sibling's deepest last child.
    AXPositionInstance rightmost_leaf =
        parent_position->CreateChildPositionAt(index_in_parent - 1);
    DCHECK(!rightmost_leaf->IsNullPosition());

    if (abort_predicate.Run(*current_position, *rightmost_leaf,
                            AXMoveType::kSibling,
                            AXMoveDirection::kPreviousInTree)) {
      return CreateNullPosition();
    }

    while (rightmost_leaf->AnchorChildCount()) {
      parent_position = std::move(rightmost_leaf);
      rightmost_leaf = parent_position->CreateChildPositionAt(
          parent_position->AnchorChildCount() - 1);
      DCHECK(!rightmost_leaf->IsNullPosition());

      if (abort_predicate.Run(*parent_position, *rightmost_leaf,
                              AXMoveType::kDescendant,
                              AXMoveDirection::kPreviousInTree)) {
        return CreateNullPosition();
      }
    }
    return rightmost_leaf;
  }

  // Creates a position using the next text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreateNextTextAnchorPosition(
      const AbortMovePredicate& abort_predicate) const {
    // If this is an ancestor text position, resolve to its leaf text position.
    if (IsTextPosition() && AnchorChildCount())
      return AsLeafTextPosition();

    AXPositionInstance next_leaf = CreateNextAnchorPosition(abort_predicate);
    while (!next_leaf->IsNullPosition() && next_leaf->AnchorChildCount()) {
      next_leaf = next_leaf->CreateNextAnchorPosition(abort_predicate);
    }

    DCHECK(next_leaf);
    return next_leaf->AsLeafTextPosition();
  }

  // Creates a position using the previous text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreatePreviousTextAnchorPosition(
      const AbortMovePredicate& abort_predicate) const {
    // If this is an ancestor text position, resolve to its leaf text position.
    if (IsTextPosition() && AnchorChildCount())
      return AsLeafTextPosition();

    AXPositionInstance previous_leaf =
        CreatePreviousAnchorPosition(abort_predicate);
    while (!previous_leaf->IsNullPosition() &&
           previous_leaf->AnchorChildCount()) {
      previous_leaf =
          previous_leaf->CreatePreviousAnchorPosition(abort_predicate);
    }

    DCHECK(previous_leaf);
    return previous_leaf->AsLeafTextPosition();
  }

  // Creates a tree position using the next text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreateNextLeafTreePosition(
      const AbortMovePredicate& abort_predicate) const {
    AXPositionInstance next_leaf =
        AsTreePosition()->CreateNextAnchorPosition(abort_predicate);
    while (!next_leaf->IsNullPosition() && next_leaf->AnchorChildCount()) {
      next_leaf = next_leaf->CreateNextAnchorPosition(abort_predicate);
    }

    DCHECK(next_leaf);
    return next_leaf;
  }

  // Creates a tree position using the previous text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreatePreviousLeafTreePosition(
      const AbortMovePredicate& abort_predicate) const {
    AXPositionInstance previous_leaf =
        AsTreePosition()->CreatePreviousAnchorPosition(abort_predicate);
    while (!previous_leaf->IsNullPosition() &&
           previous_leaf->AnchorChildCount()) {
      previous_leaf =
          previous_leaf->CreatePreviousAnchorPosition(abort_predicate);
    }

    DCHECK(previous_leaf);
    return previous_leaf;
  }

  // Static helpers for lambda usage.
  static bool AtStartOfParagraphPredicate(const AXPositionInstance& position) {
    return position->AtStartOfParagraph();
  }

  static bool AtEndOfParagraphPredicate(const AXPositionInstance& position) {
    return position->AtEndOfParagraph();
  }

  static bool AtStartOfPagePredicate(const AXPositionInstance& position) {
    return !position->IsIgnored() && position->AtStartOfPage();
  }

  static bool AtEndOfPagePredicate(const AXPositionInstance& position) {
    return !position->IsIgnored() && position->AtEndOfPage();
  }

  static bool AtStartOfLinePredicate(const AXPositionInstance& position) {
    return !position->IsIgnored() && position->AtStartOfLine();
  }

  static bool AtEndOfLinePredicate(const AXPositionInstance& position) {
    return !position->IsIgnored() && position->AtEndOfLine();
  }

  static bool AtStartOfWordPredicate(const AXPositionInstance& position) {
    return !position->IsIgnored() && position->AtStartOfWord();
  }

  static bool AtEndOfWordPredicate(const AXPositionInstance& position) {
    return !position->IsIgnored() && position->AtEndOfWord();
  }

  // Default behavior is to never abort.
  static bool DefaultAbortMovePredicate(const AXPosition& move_from,
                                        const AXPosition& move_to,
                                        const AXMoveType move_type,
                                        const AXMoveDirection direction) {
    return false;
  }

  // AbortMovePredicate function used to detect format boundaries.
  static bool AbortMoveAtFormatBoundary(const AXPosition& move_from,
                                        const AXPosition& move_to,
                                        const AXMoveType move_type,
                                        const AXMoveDirection direction) {
    if (move_from.IsNullPosition() || move_to.IsNullPosition())
      return true;

    // Treat moving to a leaf with different tags as a format break.
    if ((move_to.AnchorChildCount() == 0) &&
        move_from.GetAnchor()->GetStringAttribute(
            ax::mojom::StringAttribute::kHtmlTag) !=
            move_to.GetAnchor()->GetStringAttribute(
                ax::mojom::StringAttribute::kHtmlTag)) {
      return true;
    }

    // Stop moving when text styles differ.
    return move_from.AsLeafTextPosition()->GetTextStyles() !=
           move_to.AsLeafTextPosition()->GetTextStyles();
  }

  // AbortMovePredicate function used to detect paragraph boundaries.
  static bool AbortMoveAtParagraphBoundary(
      bool& crossed_potential_boundary_token,
      const AXPosition& move_from,
      const AXPosition& move_to,
      const AXMoveType move_type,
      const AXMoveDirection direction) {
    if (move_from.IsNullPosition() || move_to.IsNullPosition())
      return true;

    const bool move_from_break = move_from.IsInLineBreakingObject();
    const bool move_to_break = move_to.IsInLineBreakingObject();

    switch (move_type) {
      case AXMoveType::kAncestor:
        // For Ancestor moves, only abort when exiting a block descendant.
        // We don't care if the ancestor is a block or not, since the
        // descendant is contained by it.
        crossed_potential_boundary_token |= move_from_break;
        break;
      case AXMoveType::kDescendant:
        // For Descendant moves, only abort when entering a block descendant.
        // We don't care if the ancestor is a block or not, since the
        // descendant is contained by it.
        crossed_potential_boundary_token |= move_to_break;
        break;
      case AXMoveType::kSibling:
        // For Sibling moves, abort if at least one of the siblings are a block,
        // because that would mean exiting and/or entering a block.
        crossed_potential_boundary_token |= (move_from_break || move_to_break);
        break;
    }

    if (crossed_potential_boundary_token && !move_to.AnchorChildCount()) {
      // If there's a sequence of whitespace-only anchors, collapse so only the
      // last whitespace-only anchor is considered a paragraph boundary.
      if (direction == AXMoveDirection::kNextInTree &&
          move_to.IsInWhiteSpace()) {
        return false;
      }
      return true;
    }
    return false;
  }

  // AbortMovePredicate function used to detect page boundaries.
  static bool AbortMoveAtPageBoundary(const AXPosition& move_from,
                                      const AXPosition& move_to,
                                      const AXMoveType move_type,
                                      const AXMoveDirection direction) {
    if (move_from.IsNullPosition() || move_to.IsNullPosition())
      return true;

    const bool move_from_break = move_from.GetAnchor()->GetBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject);
    const bool move_to_break = move_to.GetAnchor()->GetBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject);

    switch (move_type) {
      case AXMoveType::kAncestor:
        // For Ancestor moves, only abort when exiting a page break.
        // We don't care if the ancestor is a page break or not, since the
        // descendant is contained by it.
        return move_from_break;
      case AXMoveType::kDescendant:
        // For Descendant moves, only abort when entering a page break
        // descendant. We don't care if the ancestor is a page break  or not,
        // since the descendant is contained by it.
        return move_to_break;
      case AXMoveType::kSibling:
        // For Sibling moves, abort if at both of the siblings are a page
        // break, because that would mean exiting and/or entering a page break.
        return move_from_break && move_to_break;
    }
    NOTREACHED();
    return false;
  }

  static bool AbortMoveAtStartOfInlineBlock(const AXPosition& move_from,
                                            const AXPosition& move_to,
                                            const AXMoveType move_type,
                                            const AXMoveDirection direction) {
    if (move_from.IsNullPosition() || move_to.IsNullPosition())
      return true;

    // These will only be available if AXMode has kHTML set.
    const bool move_from_is_inline_block =
        move_from.GetAnchor()->GetStringAttribute(
            ax::mojom::StringAttribute::kDisplay) == "inline-block";
    const bool move_to_is_inline_block =
        move_to.GetAnchor()->GetStringAttribute(
            ax::mojom::StringAttribute::kDisplay) == "inline-block";

    switch (direction) {
      case AXMoveDirection::kNextInTree:
        // When moving forward, break if we enter an inline block.
        return move_to_is_inline_block &&
               (move_type == AXMoveType::kDescendant ||
                move_type == AXMoveType::kSibling);
      case AXMoveDirection::kPreviousInTree:
        // When moving backward, break if we exit an inline block.
        return move_from_is_inline_block &&
               (move_type == AXMoveType::kAncestor ||
                move_type == AXMoveType::kSibling);
    }
    NOTREACHED();
    return false;
  }

  static std::vector<int32_t> GetWordStartOffsetsFunc(
      const AXPositionInstance& position) {
    return position->GetWordStartOffsets();
  }

  static std::vector<int32_t> GetWordEndOffsetsFunc(
      const AXPositionInstance& position) {
    return position->GetWordEndOffsets();
  }

  AXPositionInstance CreateDocumentAncestorPosition() const {
    AXPositionInstance iterator = Clone();
    while (!iterator->IsNullPosition()) {
      if (IsDocument(iterator->GetRole()) &&
          iterator->CreateParentPosition()->IsNullPosition()) {
        break;
      }
      iterator = iterator->CreateParentPosition();
    }
    return iterator;
  }

  // Creates a text position that is in the same anchor as the current position,
  // but starting from the current text offset, adjusts to the next or the
  // previous boundary offset depending on the boundary direction. If there is
  // no next / previous offset, the current text offset is unchanged.
  AXPositionInstance CreatePositionAtNextOffsetBoundary(
      AXTextBoundaryDirection boundary_direction,
      BoundaryTextOffsetsFunc get_offsets) const {
    if (IsNullPosition() || get_offsets.is_null())
      return Clone();

    AXPositionInstance text_position = AsTextPosition();
    const std::vector<int32_t> boundary_offsets =
        get_offsets.Run(text_position);
    if (boundary_offsets.empty())
      return text_position;

    switch (boundary_direction) {
      case AXTextBoundaryDirection::kForwards: {
        const auto offsets_iterator =
            std::upper_bound(boundary_offsets.begin(), boundary_offsets.end(),
                             int32_t{text_position->text_offset_});
        // If there is no next offset, the current offset should be unchanged.
        if (offsets_iterator < boundary_offsets.end()) {
          text_position->text_offset_ = int{*offsets_iterator};
          text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
        }
        break;
      }
      case AXTextBoundaryDirection::kBackwards: {
        auto offsets_iterator =
            std::lower_bound(boundary_offsets.begin(), boundary_offsets.end(),
                             int32_t{text_position->text_offset_});
        // If there is no previous offset, the current offset should be
        // unchanged.
        if (offsets_iterator > boundary_offsets.begin()) {
          // Since we already checked if "boundary_offsets" are non-empty, we
          // can safely move the iterator one position back, even if it's
          // currently at the vector's end.
          --offsets_iterator;
          text_position->text_offset_ = int{*offsets_iterator};
          text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
        }
        break;
      }
    }

    return text_position;
  }

  // Creates a text position that is in the same anchor as the current position,
  // but adjusts its text offset to be either at the first or last offset
  // boundary, based on the boundary direction. When moving forward, the text
  // position is adjusted to point to the first offset boundary, or to the end
  // of its anchor if there are no offset boundaries. When moving backward, it
  // is adjusted to point to the last offset boundary, or to the start of its
  // anchor if there are no offset boundaries.
  AXPositionInstance CreatePositionAtFirstOffsetBoundary(
      AXTextBoundaryDirection boundary_direction,
      BoundaryTextOffsetsFunc get_offsets) const {
    if (IsNullPosition() || get_offsets.is_null())
      return Clone();

    AXPositionInstance text_position = AsTextPosition();
    const std::vector<int32_t> boundary_offsets =
        get_offsets.Run(text_position);
    switch (boundary_direction) {
      case AXTextBoundaryDirection::kForwards:
        if (boundary_offsets.empty()) {
          return text_position->CreatePositionAtEndOfAnchor();
        } else {
          text_position->text_offset_ = int{boundary_offsets[0]};
          return text_position;
        }
        break;
      case AXTextBoundaryDirection::kBackwards:
        if (boundary_offsets.empty()) {
          return text_position->CreatePositionAtStartOfAnchor();
        } else {
          text_position->text_offset_ =
              int{boundary_offsets[boundary_offsets.size() - 1]};
          return text_position;
        }
        break;
    }
  }

  AXPositionKind kind_;
  AXTreeID tree_id_;
  int32_t anchor_id_;

  // For text positions, |child_index_| is initially set to |-1| and only
  // computed on demand. The same with tree positions and |text_offset_|.
  int child_index_;
  // "text_offset_" represents the number of UTF16 code units before this
  // position. It doesn't count grapheme clusters.
  int text_offset_;

  // Affinity is used to distinguish between two text positions that point to
  // the same text offset, but which happens to fall on a soft line break. A
  // soft line break doesn't insert any white space in the accessibility tree,
  // so without affinity there would be no way to determine whether a text
  // position is before or after the soft line break. An upstream affinity means
  // that the position is before the soft line break, whilst a downstream
  // affinity means that the position is after the soft line break.
  //
  // Please note that affinity could only be set to upstream for positions that
  // are anchored to non-leaf nodes. When on a leaf node, there could never be
  // an ambiguity as to which line a position points to because Blink creates
  // separate inline text boxes for each line of text. Therefore, a leaf text
  // position before the soft line break would be pointing to the end of its
  // anchor node, whilst a leaf text position after the soft line break would be
  // pointing to the start of the next node.
  ax::mojom::TextAffinity affinity_;

  //
  // Cached members that should be lazily created on first use.
  //

  // In the case of a leaf position, the name of its anchor used for
  // initializing a grapheme break iterator.
  mutable base::string16 name_;
};

template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::BEFORE_TEXT;
template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::INVALID_INDEX;
template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::INVALID_OFFSET;

template <class AXPositionType, class AXNodeType>
bool operator==(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() == 0;
}

template <class AXPositionType, class AXNodeType>
bool operator!=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() != 0;
}

template <class AXPositionType, class AXNodeType>
bool operator<(const AXPosition<AXPositionType, AXNodeType>& first,
               const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() < 0;
}

template <class AXPositionType, class AXNodeType>
bool operator<=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() <= 0;
}

template <class AXPositionType, class AXNodeType>
bool operator>(const AXPosition<AXPositionType, AXNodeType>& first,
               const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() > 0;
}

template <class AXPositionType, class AXNodeType>
bool operator>=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() >= 0;
}

template <class AXPositionType, class AXNodeType>
void swap(AXPosition<AXPositionType, AXNodeType>& first,
          AXPosition<AXPositionType, AXNodeType>& second) {
  first.swap(second);
}

template <class AXPositionType, class AXNodeType>
std::ostream& operator<<(
    std::ostream& stream,
    const AXPosition<AXPositionType, AXNodeType>& position) {
  return stream << position.ToString();
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_POSITION_H_
