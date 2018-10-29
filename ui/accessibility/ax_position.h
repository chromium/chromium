// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_POSITION_H_
#define UI_ACCESSIBILITY_AX_POSITION_H_

#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/stack.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_id.h"

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
  StopIfAlreadyAtBoundary
};

// Forward declarations.
template <class AXPositionType, class AXNodeType>
class AXPosition;
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

  static const int32_t INVALID_ANCHOR_ID = -1;
  static const int BEFORE_TEXT = -1;
  static const int INVALID_INDEX = -2;
  static const int INVALID_OFFSET = -1;

  static AXPositionInstance CreateNullPosition() {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::NULL_POSITION, AXTreeIDUnknown(),
                             INVALID_ANCHOR_ID, INVALID_INDEX, INVALID_OFFSET,
                             ax::mojom::TextAffinity::kDownstream);
    return new_position;
  }

  static AXPositionInstance CreateTreePosition(AXTreeID tree_id,
                                               int32_t anchor_id,
                                               int child_index) {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::TREE_POSITION, tree_id, anchor_id,
                             child_index, INVALID_OFFSET,
                             ax::mojom::TextAffinity::kDownstream);
    return new_position;
  }

  static AXPositionInstance CreateTextPosition(
      AXTreeID tree_id,
      int32_t anchor_id,
      int text_offset,
      ax::mojom::TextAffinity affinity) {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::TEXT_POSITION, tree_id, anchor_id,
                             INVALID_INDEX, text_offset, affinity);
    return new_position;
  }

  virtual ~AXPosition() = default;

  virtual AXPositionInstance Clone() const = 0;

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
          str_child_index = base::IntToString(child_index_);
        }
        str = "TreePosition tree_id=" + tree_id_.ToString() +
              " anchor_id=" + base::IntToString(anchor_id_) +
              " child_index=" + str_child_index;
        break;
      }
      case AXPositionKind::TEXT_POSITION: {
        std::string str_text_offset;
        if (text_offset_ == INVALID_OFFSET) {
          str_text_offset = "invalid";
        } else {
          str_text_offset = base::IntToString(text_offset_);
        }
        str = "TextPosition tree_id=" + tree_id_.ToString() +
              " anchor_id=" + base::IntToString(anchor_id_) +
              " text_offset=" + str_text_offset + " affinity=" +
              ui::ToString(static_cast<ax::mojom::TextAffinity>(affinity_));
        break;
      }
    }

    if (!IsTextPosition() || text_offset_ > MaxTextOffset())
      return str;

    std::string text = base::UTF16ToUTF8(GetInnerText());
    DCHECK_GE(text_offset_, 0);
    DCHECK_LE(text_offset_, static_cast<int>(text.length()));
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
  int32_t anchor_id() const { return anchor_id_; }

  AXNodeType* GetAnchor() const {
    if (tree_id_ == AXTreeIDUnknown() || anchor_id_ == INVALID_ANCHOR_ID)
      return nullptr;
    DCHECK_GE(anchor_id_, 0);
    return GetNodeInTree(tree_id_, anchor_id_);
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

  bool AtStartOfAnchor() const {
    if (!GetAnchor())
      return false;

    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        if (AnchorChildCount())
          return child_index_ == 0;
        return child_index_ == BEFORE_TEXT;
      case AXPositionKind::TEXT_POSITION:
        return text_offset_ == 0;
    }

    return false;
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

    return false;
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
        return base::ContainsValue(
            word_starts, static_cast<int32_t>(text_position->text_offset_));
      }
    }
    return false;
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
        return base::ContainsValue(
            word_ends, static_cast<int32_t>(text_position->text_offset_));
      }
    }
    return false;
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
        return GetPreviousOnLineID(text_position->anchor_id_) ==
                   INVALID_ANCHOR_ID &&
               text_position->AtStartOfAnchor();
    }
    return false;
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
        // If affinity has been used to specify whether the caret is at the end
        // of a line or at the start of the next one, this should have been
        // reflected in the leaf text position we got. In other cases, we
        // assume that white space is being used to separate lines.
        // Note that we don't treat a position that is at the start of a line
        // break that is on a line by itself as being at the end of the line.
        if (GetNextOnLineID(text_position->anchor_id_) == INVALID_ANCHOR_ID) {
          if (text_position->IsInWhiteSpace()) {
            return !text_position->AtStartOfLine() &&
                   text_position->AtStartOfAnchor();
          }

          return text_position->AtEndOfAnchor();
        }

        // The current anchor might be followed by a soft line break.
        if (text_position->AtEndOfAnchor())
          return text_position->CreateNextTextAnchorPosition()->AtEndOfLine();
    }
    return false;
  }

  // This method returns a position instead of a node because this allows us to
  // return the corresponding text offset or child index in the ancestor that
  // relates to the current position.
  // Also, this method uses position instead of tree logic to traverse the tree,
  // because positions can handle moving across multiple trees, while trees
  // cannot.
  AXPositionInstance LowestCommonAncestor(
      const AXPosition<AXPositionType, AXNodeType>& second) const {
    if (IsNullPosition() || second.IsNullPosition())
      return CreateNullPosition();
    if (GetAnchor() == second.GetAnchor())
      return Clone();

    base::stack<AXPositionInstance> ancestors1;
    ancestors1.push(std::move(Clone()));
    while (!ancestors1.top()->IsNullPosition())
      ancestors1.push(std::move(ancestors1.top()->CreateParentPosition()));

    base::stack<AXPositionInstance> ancestors2;
    ancestors2.push(std::move(second.Clone()));
    while (!ancestors2.top()->IsNullPosition())
      ancestors2.push(std::move(ancestors2.top()->CreateParentPosition()));

    AXPositionInstance common_ancestor;
    while (!ancestors1.empty() && !ancestors2.empty() &&
           ancestors1.top()->GetAnchor() == ancestors2.top()->GetAnchor()) {
      common_ancestor = std::move(ancestors1.top());
      ancestors1.pop();
      ancestors2.pop();
    }
    return common_ancestor;
  }

  AXPositionInstance AsTreePosition() const {
    if (IsNullPosition() || IsTreePosition())
      return Clone();

    AXPositionInstance copy = Clone();
    DCHECK(copy);
    DCHECK_GE(copy->text_offset_, 0);
    if (!copy->AnchorChildCount() &&
        copy->text_offset_ != copy->MaxTextOffset()) {
      copy->child_index_ = BEFORE_TEXT;
    } else {
      copy->child_index_ = 0;
    }

    // Blink doesn't always remove all deleted whitespace at the end of a
    // textarea even though it will have adjusted its value attribute, because
    // the extra layout objects are invisible. Therefore, we will stop at the
    // last child that we can reach with the current text offset and ignore any
    // remaining children.
    int current_offset = 0;
    for (int i = 0; i < copy->AnchorChildCount(); ++i) {
      AXPositionInstance child = copy->CreateChildPositionAt(i);
      DCHECK(child);
      int child_length = child->MaxTextOffsetInParent();
      if (copy->text_offset_ >= current_offset &&
          (copy->text_offset_ < (current_offset + child_length) ||
           (copy->affinity_ == ax::mojom::TextAffinity::kUpstream &&
            copy->text_offset_ == (current_offset + child_length)))) {
        copy->child_index_ = i;
        break;
      }

      current_offset += child_length;
    }
    if (current_offset >= copy->MaxTextOffset())
      copy->child_index_ = copy->AnchorChildCount();

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
      if (copy->text_offset_ < 0 || copy->text_offset_ >= copy->MaxTextOffset())
        copy->text_offset_ = 0;
    } else if (copy->child_index_ == copy->AnchorChildCount()) {
      copy->text_offset_ = copy->MaxTextOffset();
    } else {
      DCHECK_GE(copy->child_index_, 0);
      DCHECK_LT(copy->child_index_, copy->AnchorChildCount());
      int new_offset = 0;
      for (int i = 0; i <= child_index_; ++i) {
        AXPositionInstance child = copy->CreateChildPositionAt(i);
        DCHECK(child);
        int child_length = child->MaxTextOffsetInParent();

        // If the current text offset is valid, we don't touch it to potentially
        // allow converting from a text position to a tree position and back
        // without losing information. Otherwise, we reset it to the beginning
        // of the current child node.
        if (i == child_index_ &&
            (copy->text_offset_ < new_offset ||
             copy->text_offset_ > (new_offset + child_length) ||
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

    AXPositionInstance tree_position = AsTreePosition();
    // Adjust the text offset.
    // No need to check for "before text" positions here because they are only
    // present on leaf anchor nodes.
    int adjusted_offset = AsTextPosition()->text_offset_;
    AXPositionInstance child_position = tree_position->CreateChildPositionAt(0);
    DCHECK(child_position);
    for (int i = 1; i <= tree_position->child_index_ &&
                    i < tree_position->AnchorChildCount();
         ++i) {
      adjusted_offset -= child_position->MaxTextOffsetInParent();
      child_position = tree_position->CreateChildPositionAt(i);
      DCHECK(child_position);
    }

    child_position = child_position->AsTextPosition();
    child_position->text_offset_ = adjusted_offset;
    // Maintain affinity from parent so that we'll be able to choose the correct
    // leaf anchor if the text offset is right on the boundary between two
    // leaves.
    child_position->affinity_ = affinity_;
    return child_position->AsLeafTextPosition();
  }

  AXPositionInstance CreatePositionAtStartOfAnchor() const {
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION:
        if (!AnchorChildCount())
          return CreateTreePosition(tree_id_, anchor_id_, BEFORE_TEXT);
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

  AXPositionInstance CreateChildPositionAt(int child_index) const {
    if (IsNullPosition())
      return CreateNullPosition();

    if (child_index < 0 || child_index >= AnchorChildCount())
      return CreateNullPosition();

    AXTreeID tree_id = AXTreeIDUnknown();
    int32_t child_id = INVALID_ANCHOR_ID;
    AnchorChild(child_index, &tree_id, &child_id);
    DCHECK_NE(tree_id, AXTreeIDUnknown());
    DCHECK_NE(child_id, INVALID_ANCHOR_ID);
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
    int32_t parent_id = INVALID_ANCHOR_ID;
    AnchorParent(&tree_id, &parent_id);
    if (tree_id == AXTreeIDUnknown() || parent_id == INVALID_ANCHOR_ID)
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
        int parent_offset = AnchorTextOffsetInParent();
        ax::mojom::TextAffinity parent_affinity = affinity_;
        if (MaxTextOffset() == MaxTextOffsetInParent()) {
          parent_offset += text_offset_;
        } else if (text_offset_ > 0) {
          parent_offset += MaxTextOffsetInParent();
          parent_affinity = ax::mojom::TextAffinity::kDownstream;
        }

        AXPositionInstance parent_position = CreateTextPosition(
            tree_id, parent_id, parent_offset, parent_affinity);
        // We check if the parent position has introduced ambiguity as to
        // whether it refers to the end of the current or the start of the next
        // line. We do this check by creating the parent position and testing if
        // it is erroneously at the start of the next line. We could not have
        // checked if the child was at the end of the line, because our line end
        // testing logic takes into account line breaks, which don't apply in
        // this situation.
        if (text_offset_ == MaxTextOffset() && parent_position->AtStartOfLine())
          parent_position->affinity_ = ax::mojom::TextAffinity::kUpstream;
        return parent_position;
      }
    }

    return CreateNullPosition();
  }

  // Creates a position using the next text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreateNextTextAnchorPosition() const {
    AXPositionInstance next_leaf(CreateNextAnchorPosition());
    while (!next_leaf->IsNullPosition() && next_leaf->AnchorChildCount()) {
      next_leaf = next_leaf->CreateNextAnchorPosition();
    }

    DCHECK(next_leaf);
    return next_leaf->AsTextPosition();
  }

  // Creates a position using the previous text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreatePreviousTextAnchorPosition() const {
    AXPositionInstance previous_leaf(CreatePreviousAnchorPosition());
    while (!previous_leaf->IsNullPosition() &&
           previous_leaf->AnchorChildCount()) {
      previous_leaf = previous_leaf->CreatePreviousAnchorPosition();
    }

    DCHECK(previous_leaf);
    return previous_leaf->AsTextPosition();
  }

  AXPositionInstance CreateNextCharacterPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsTextPosition();
    if (text_position->IsNullPosition())
      return text_position;

    // Note that |BoundaryBehavior::StopIfAlreadyAtBoundary| doesn't make
    // sense for character boundaries.
    DCHECK_NE(boundary_behavior, AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary &&
        (text_position->text_offset_ + 1) >= text_position->MaxTextOffset()) {
      return Clone();
    }

    if ((text_position->text_offset_ + 1) < text_position->MaxTextOffset()) {
      text_position->text_offset_ += 1;
      // Even if our affinity was upstream, moving to the next character should
      // inevitably reset it to downstream.
      text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    } else {
      // Moving to the end of the current anchor first is essential. Otherwise
      // |CreateNextAnchorPosition| might return our deepest left-most child
      // because we are using pre-order traversal.
      text_position = text_position->CreatePositionAtEndOfAnchor();
      text_position = text_position->CreateNextTextAnchorPosition();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreatePreviousCharacterPosition(
      AXBoundaryBehavior boundary_behavior) const {
    // Note that |BoundaryBehavior::StopIfAlreadyAtBoundary| doesn't make
    // sense for character boundaries.
    DCHECK_NE(boundary_behavior, AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary &&
        AtStartOfAnchor()) {
      return Clone();
    }

    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsTextPosition();
    if (text_position->IsNullPosition())
      return text_position;

    if (text_position->text_offset_ > 0) {
      text_position->text_offset_ -= 1;
      // Even if the new position is at the beginning of the line, the affinity
      // is defaulted to downstream for simplicity.
      text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    } else {
      text_position = text_position->CreatePreviousTextAnchorPosition();
      text_position = text_position->CreatePositionAtEndOfAnchor();
      if (!text_position->AtStartOfAnchor())
        --text_position->text_offset_;
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreateNextWordStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->IsNullPosition())
      return text_position;
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtStartOfWord()) {
      AXPositionInstance clone = Clone();
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return clone;
    }

    const std::vector<int32_t> word_starts =
        text_position->GetWordStartOffsets();
    auto iterator =
        std::upper_bound(word_starts.begin(), word_starts.end(),
                         static_cast<int32_t>(text_position->text_offset_));
    if (iterator == word_starts.end()) {
      // Ignore any nodes with no text or no word boundaries.
      do {
        text_position = text_position->CreateNextTextAnchorPosition();
      } while (!text_position->IsNullPosition() &&
               (!text_position->MaxTextOffset() ||
                text_position->GetWordStartOffsets().empty()));
      if (text_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
          return CreatePositionAtEndOfAnchor();
        return text_position;
      }

      const std::vector<int32_t> updated_word_starts =
          text_position->GetWordStartOffsets();
      DCHECK(!updated_word_starts.empty());
      text_position->text_offset_ = static_cast<int>(updated_word_starts[0]);
    } else {
      text_position->text_offset_ = static_cast<int>(*iterator);
      text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    }

    // If the word boundary is in the same subtree, return a position rooted
    // at the current position. This is necessary because we don't want to
    // return any position that might be in the shadow DOM if the original
    // position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreatePreviousWordStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtStartOfWord()) {
      AXPositionInstance clone = Clone();
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return clone;
    }

    if (text_position->AtStartOfAnchor()) {
      text_position = text_position->CreatePreviousTextAnchorPosition();
      text_position = text_position->CreatePositionAtEndOfAnchor();
    }
    if (text_position->IsNullPosition()) {
      if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
        return CreatePositionAtStartOfAnchor();
      return text_position;
    }

    const std::vector<int32_t> word_starts =
        text_position->GetWordStartOffsets();
    auto iterator =
        std::lower_bound(word_starts.begin(), word_starts.end(),
                         static_cast<int32_t>(text_position->text_offset_));
    if (word_starts.empty() || iterator == word_starts.begin()) {
      // Ignore any nodes with no text or no word boundaries.
      do {
        text_position = text_position->CreatePreviousTextAnchorPosition();
      } while (!text_position->IsNullPosition() &&
               (!text_position->MaxTextOffset() ||
                text_position->GetWordStartOffsets().empty()));
      if (text_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
          return CreatePositionAtStartOfAnchor();
        return text_position;
      }

      const std::vector<int32_t> updated_word_starts =
          text_position->GetWordStartOffsets();
      DCHECK(!updated_word_starts.empty());
      text_position->text_offset_ =
          static_cast<int>(*(updated_word_starts.end() - 1));
    } else {
      text_position->text_offset_ = static_cast<int>(*(--iterator));
      text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    }

    // If the word boundary is in the same subtree, return a position rooted
    // at the current position. This is necessary because we don't want to
    // return any position that might be in the shadow DOM if the original
    // position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // Word end positions are one past the last character of the word.
  AXPositionInstance CreateNextWordEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtEndOfWord()) {
      AXPositionInstance clone = Clone();
      // If there is no ambiguity as to whether the position is at the end of
      // the current line or the start of the next line, affinity should be
      // reset in order to get consistent output from this function regardless
      // of input affinity.
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (clone->AtStartOfLine())
        clone->affinity_ = ax::mojom::TextAffinity::kUpstream;
      return clone;
    }

    if (text_position->AtEndOfAnchor())
      text_position = text_position->CreateNextTextAnchorPosition();
    if (text_position->IsNullPosition()) {
      if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
        return CreatePositionAtEndOfAnchor();
      return text_position;
    }

    const std::vector<int32_t> word_ends = text_position->GetWordEndOffsets();
    auto iterator =
        std::upper_bound(word_ends.begin(), word_ends.end(),
                         static_cast<int32_t>(text_position->text_offset_));
    if (iterator == word_ends.end()) {
      // Ignore any nodes with no text or no word boundaries.
      do {
        text_position = text_position->CreateNextTextAnchorPosition();
      } while (!text_position->IsNullPosition() &&
               (!text_position->MaxTextOffset() ||
                text_position->GetWordEndOffsets().empty()));
      if (text_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
          return CreatePositionAtEndOfAnchor();
        return text_position;
      }

      const std::vector<int32_t> updated_word_ends =
          text_position->GetWordEndOffsets();
      DCHECK(!updated_word_ends.empty());
      text_position->text_offset_ = static_cast<int>(updated_word_ends[0]);
    } else {
      text_position->text_offset_ = static_cast<int>(*iterator);
      text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    }

    // If the word boundary is in the same subtree, return a position rooted
    // at the current position. This is necessary because we don't want to
    // return any position that might be in the shadow DOM if the original
    // position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // Word end positions are one past the last character of the word.
  AXPositionInstance CreatePreviousWordEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtEndOfWord()) {
      AXPositionInstance clone = Clone();
      // If there is no ambiguity as to whether the position is at the end of
      // the current line or the start of the next line, affinity should be
      // reset in order to get consistent output from this function regardless
      // of input affinity.
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (clone->AtStartOfLine())
        clone->affinity_ = ax::mojom::TextAffinity::kUpstream;
      return clone;
    }

    if (text_position->AtStartOfAnchor()) {
      text_position = text_position->CreatePreviousTextAnchorPosition();
      text_position = text_position->CreatePositionAtEndOfAnchor();
    }
    if (text_position->IsNullPosition()) {
      if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
        return CreatePositionAtStartOfAnchor();
      return text_position;
    }

    const std::vector<int32_t> word_ends = text_position->GetWordEndOffsets();
    auto iterator =
        std::lower_bound(word_ends.begin(), word_ends.end(),
                         static_cast<int32_t>(text_position->text_offset_));
    if (word_ends.empty() || iterator == word_ends.begin()) {
      // Ignore any nodes with no text or no word boundaries.
      do {
        text_position = text_position->CreatePreviousTextAnchorPosition();
      } while (!text_position->IsNullPosition() &&
               (!text_position->MaxTextOffset() ||
                text_position->GetWordStartOffsets().empty()));
      if (text_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
          return CreatePositionAtStartOfAnchor();
        return text_position;
      }

      const std::vector<int32_t> updated_word_ends =
          text_position->GetWordEndOffsets();
      DCHECK(!updated_word_ends.empty());
      text_position->text_offset_ =
          static_cast<int>(*(updated_word_ends.end() - 1));
    } else {
      text_position->text_offset_ = static_cast<int>(*(--iterator));
      text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    }

    // If the word boundary is in the same subtree, return a position rooted
    // at the current position. This is necessary because we don't want to
    // return any position that might be in the shadow DOM if the original
    // position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreateNextLineStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->IsNullPosition())
      return text_position;
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtStartOfLine()) {
      AXPositionInstance clone = Clone();
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return clone;
    }

    do {
      text_position = text_position->CreateNextTextAnchorPosition();
    } while (!text_position->AtStartOfLine() &&
             !text_position->IsNullPosition());
    if (text_position->IsNullPosition()) {
      if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
        return CreatePositionAtEndOfAnchor();
      return text_position;
    }

    // If the line boundary is in the same subtree, return a position rooted at
    // the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreatePreviousLineStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtStartOfLine()) {
      AXPositionInstance clone = Clone();
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return clone;
    }

    if (text_position->AtStartOfAnchor()) {
      text_position = text_position->CreatePreviousTextAnchorPosition();
    } else {
      text_position = text_position->CreatePositionAtStartOfAnchor();
    }

    while (!text_position->AtStartOfLine() &&
           !text_position->IsNullPosition()) {
      text_position = text_position->CreatePreviousTextAnchorPosition();
    }
    if (text_position->IsNullPosition()) {
      if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
        return CreatePositionAtStartOfAnchor();
      return text_position;
    }

    // If the line boundary is in the same subtree, return a position rooted at
    // the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // Line end positions are one past the last character of the line, excluding
  // any white space or newline characters that separate the lines.
  AXPositionInstance CreateNextLineEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtEndOfLine()) {
      AXPositionInstance clone = Clone();
      // If there is no ambiguity as to whether the position is at the end of
      // the current line or the start of the next line, affinity should be
      // reset in order to get consistent output from this function regardless
      // of input affinity.
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (clone->AtStartOfLine())
        clone->affinity_ = ax::mojom::TextAffinity::kUpstream;
      return clone;
    }

    if (text_position->AtEndOfAnchor()) {
      text_position = text_position->CreateNextTextAnchorPosition()
                          ->CreatePositionAtEndOfAnchor();
    } else {
      text_position = text_position->CreatePositionAtEndOfAnchor();
    }

    while (!text_position->AtEndOfLine() && !text_position->IsNullPosition()) {
      text_position = text_position->CreateNextTextAnchorPosition();
      if (text_position->AtEndOfLine())
        break;
      text_position = text_position->CreatePositionAtEndOfAnchor();
    }
    if (text_position->IsNullPosition()) {
      if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
        return CreatePositionAtEndOfAnchor();
      return text_position;
    }

    // If the line boundary is in the same subtree, return a position rooted at
    // the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // Line end positions are one past the last character of the line, excluding
  // any white space or newline characters separating the lines.
  AXPositionInstance CreatePreviousLineEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->IsNullPosition())
      return text_position;
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtEndOfLine()) {
      AXPositionInstance clone = Clone();
      // If there is no ambiguity as to whether the position is at the end of
      // the current line or the start of the next line, affinity should be
      // reset in order to get consistent output from this function regardless
      // of input affinity.
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (clone->AtStartOfLine())
        clone->affinity_ = ax::mojom::TextAffinity::kUpstream;
      return clone;
    }

    do {
      text_position = text_position->CreatePreviousTextAnchorPosition()
                          ->CreatePositionAtEndOfAnchor();
      if (text_position->AtEndOfLine())
        break;
      text_position = text_position->CreatePositionAtStartOfAnchor();
    } while (!text_position->AtEndOfLine() && !text_position->IsNullPosition());
    if (text_position->IsNullPosition()) {
      if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
        return CreatePositionAtStartOfAnchor();
      return text_position;
    }

    // If the line boundary is in the same subtree, return a position rooted at
    // the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // TODO(nektar): Add sentence and paragraph navigation methods.

  // Abstract methods.

  // Returns the text that is present inside the anchor node, including any text
  // found in descendant nodes.
  virtual base::string16 GetInnerText() const = 0;
  // Returns the length of the text that is present inside the anchor node,
  // including any text found in descendant text nodes.
  virtual int MaxTextOffset() const = 0;

 protected:
  AXPosition() = default;
  AXPosition(const AXPosition<AXPositionType, AXNodeType>& other) = default;
  virtual AXPosition<AXPositionType, AXNodeType>& operator=(
      const AXPosition<AXPositionType, AXNodeType>& other) = default;

  virtual void Initialize(AXPositionKind kind,
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

    if (!GetAnchor() || kind_ == AXPositionKind::NULL_POSITION ||
        (kind_ == AXPositionKind::TREE_POSITION &&
         (child_index_ != BEFORE_TEXT &&
          (child_index_ < 0 || child_index_ > AnchorChildCount()))) ||
        (kind_ == AXPositionKind::TEXT_POSITION &&
         (text_offset_ < 0 || text_offset_ > MaxTextOffset()))) {
      // Reset to the null position.
      kind_ = AXPositionKind::NULL_POSITION;
      tree_id_ = AXTreeIDUnknown();
      anchor_id_ = INVALID_ANCHOR_ID;
      child_index_ = INVALID_INDEX;
      text_offset_ = INVALID_OFFSET;
      affinity_ = ax::mojom::TextAffinity::kDownstream;
    }
  }

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreateNextAnchorPosition() const {
    if (IsNullPosition())
      return CreateNullPosition();

    if (AnchorChildCount()) {
      if (IsTreePosition()) {
        return CreateChildPositionAt(child_index_);
      } else {
        // We have to find the child node that encompasses the current text
        // offset.
        AXPositionInstance tree_position = AsTreePosition();
        DCHECK(tree_position);
        return tree_position->CreateChildPositionAt(
            tree_position->child_index_);
      }
    }

    AXPositionInstance current_position = Clone();
    AXPositionInstance parent_position = CreateParentPosition();
    while (!parent_position->IsNullPosition()) {
      // Get the next sibling if it exists, otherwise move up to the parent's
      // next sibling.
      int index_in_parent = current_position->AnchorIndexInParent();
      if (index_in_parent < parent_position->AnchorChildCount() - 1) {
        AXPositionInstance next_sibling =
            parent_position->CreateChildPositionAt(index_in_parent + 1);
        DCHECK(next_sibling && !next_sibling->IsNullPosition());
        return next_sibling;
      }

      current_position = std::move(parent_position);
      parent_position = current_position->CreateParentPosition();
    }

    return CreateNullPosition();
  }

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreatePreviousAnchorPosition() const {
    if (IsNullPosition())
      return CreateNullPosition();

    AXPositionInstance parent_position = CreateParentPosition();
    if (parent_position->IsNullPosition())
      return CreateNullPosition();

    // Get the previous sibling's deepest last child if a previous sibling
    // exists, otherwise move up to the parent.
    int index_in_parent = AnchorIndexInParent();
    if (index_in_parent <= 0)
      return parent_position;

    AXPositionInstance leaf =
        parent_position->CreateChildPositionAt(index_in_parent - 1);
    while (!leaf->IsNullPosition() && leaf->AnchorChildCount())
      leaf = leaf->CreateChildPositionAt(leaf->AnchorChildCount() - 1);

    return leaf;
  }

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

  // Abstract methods.
  virtual void AnchorChild(int child_index,
                           AXTreeID* tree_id,
                           int32_t* child_id) const = 0;
  virtual int AnchorChildCount() const = 0;
  virtual int AnchorIndexInParent() const = 0;
  virtual void AnchorParent(AXTreeID* tree_id, int32_t* parent_id) const = 0;
  virtual AXNodeType* GetNodeInTree(AXTreeID tree_id,
                                    int32_t node_id) const = 0;
  // Returns the length of text that this anchor node takes up in its parent.
  // On some platforms, embedded objects are represented in their parent with a
  // single embedded object character.
  virtual int MaxTextOffsetInParent() const { return MaxTextOffset(); }
  virtual bool IsInWhiteSpace() const = 0;
  virtual std::vector<int32_t> GetWordStartOffsets() const = 0;
  virtual std::vector<int32_t> GetWordEndOffsets() const = 0;
  virtual int32_t GetNextOnLineID(int32_t node_id) const = 0;
  virtual int32_t GetPreviousOnLineID(int32_t node_id) const = 0;

 private:
  AXPositionKind kind_;
  AXTreeID tree_id_;
  int32_t anchor_id_;

  // For text positions, |child_index_| is initially set to |-1| and only
  // computed on demand. The same with tree positions and |text_offset_|.
  int child_index_;
  int text_offset_;

  // TODO(nektar): Get rid of affinity and make Blink handle affinity
  // internally since inline text objects don't span lines.
  ax::mojom::TextAffinity affinity_;
};

template <class AXPositionType, class AXNodeType>
const int32_t AXPosition<AXPositionType, AXNodeType>::INVALID_ANCHOR_ID;
template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::BEFORE_TEXT;
template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::INVALID_INDEX;
template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::INVALID_OFFSET;

template <class AXPositionType, class AXNodeType>
bool operator==(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  if (first.IsNullPosition() && second.IsNullPosition())
    return true;
  return first.tree_id() == second.tree_id() &&
         first.anchor_id() == second.anchor_id() &&
         first.child_index() == second.child_index() &&
         first.text_offset() == second.text_offset() &&
         first.affinity() == second.affinity();
}

template <class AXPositionType, class AXNodeType>
bool operator!=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  return !(first == second);
}

template <class AXPositionType, class AXNodeType>
bool operator<(const AXPosition<AXPositionType, AXNodeType>& first,
               const AXPosition<AXPositionType, AXNodeType>& second) {
  if (first.IsNullPosition() || second.IsNullPosition())
    return false;

  // It is potentially costly to compute the parent position of a text position,
  // whilst computing the parent position of a tree position is really
  // inexpensive. In order to find the lowest common ancestor, especially if
  // that ancestor is all the way up to the root of the tree, this will need to
  // be done repeatedly. We avoid the performance hit by converting both
  // positions to tree positions and only falling back to text positions if both
  // are text positions and the lowest common ancestor is not one of their
  // anchors. Essentially, the question we need to answer is: "When are two non
  // equivalent positions going to have the same lowest common ancestor position
  // when converted to tree positions?" The answer is when they are both text
  // positions and they either have the same anchor, or one is the ancestor of
  // the other.
  std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> tree_first =
      first.AsTreePosition();
  std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> tree_second =
      second.AsTreePosition();
  std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> first_ancestor =
      tree_first->LowestCommonAncestor(*tree_second);
  std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> second_ancestor =
      tree_second->LowestCommonAncestor(*tree_first);
  DCHECK_EQ(first_ancestor->GetAnchor(), second_ancestor->GetAnchor());
  if (first_ancestor->IsNullPosition())
    return false;
  DCHECK(first_ancestor->IsTreePosition() && second_ancestor->IsTreePosition());

  if (first.IsTextPosition() && second.IsTextPosition()) {
    // We avoid recomputing lowest common ancestor, because we already have its
    // anchor. We just need its text offset.
    if (first.GetAnchor() == first_ancestor->GetAnchor()) {
      // If both positions have the same anchor, or if the first is an ancestor
      // of the second.
      std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> text_second =
          second.Clone();
      while (text_second->GetAnchor() != first.GetAnchor())
        text_second = text_second->CreateParentPosition();
      return first.text_offset() < text_second->text_offset();
    } else if (second.GetAnchor() == second_ancestor->GetAnchor()) {
      // If the second position is an ancestor of the first.
      std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> text_first =
          first.Clone();
      while (text_first->GetAnchor() != second.GetAnchor())
        text_first = text_first->CreateParentPosition();
      return text_first->text_offset() < second.text_offset();
    }
  }

  return first_ancestor->child_index() < second_ancestor->child_index();
}

template <class AXPositionType, class AXNodeType>
bool operator<=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  return first == second || first < second;
}

template <class AXPositionType, class AXNodeType>
bool operator>(const AXPosition<AXPositionType, AXNodeType>& first,
               const AXPosition<AXPositionType, AXNodeType>& second) {
  if (first.IsNullPosition() || second.IsNullPosition())
    return false;
  return !(first <= second);
}

template <class AXPositionType, class AXNodeType>
bool operator>=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  return first == second || first > second;
}

template <class AXPositionType, class AXNodeType>
std::ostream& operator<<(
    std::ostream& stream,
    const AXPosition<AXPositionType, AXNodeType>& position) {
  return stream << position.ToString();
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_POSITION_H_
