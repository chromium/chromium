// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_RANGE_H_
#define UI_ACCESSIBILITY_AX_RANGE_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_clipping_behavior.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_offscreen_result.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_manager_map.h"

namespace ui {

// Specifies how AXRange::GetText treats any formatting changes, such as
// paragraph breaks, that have been introduced by layout. For example, consider
// the following HTML snippet: "A<div>B</div>C".
enum class AXTextConcatenationBehavior {
  // Preserve any introduced formatting, such as paragraph breaks, e.g. GetText
  // = "A\nB\nC".
  kWithParagraphBreaks,
  // Ignore any introduced formatting, such as paragraph breaks, e.g. GetText =
  // "ABC".
  kWithoutParagraphBreaks
};

class AXRangeRectDelegate {
 public:
  virtual gfx::Rect GetInnerTextRangeBoundsRect(
      AXTreeID tree_id,
      AXNodeID node_id,
      int start_offset,
      int end_offset,
      AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result) = 0;
  virtual gfx::Rect GetBoundsRect(AXTreeID tree_id,
                                  AXNodeID node_id,
                                  AXOffscreenResult* offscreen_result) = 0;
};

// A range delimited by two positions in the AXTree.
//
// In order to avoid any confusion regarding whether a deep or a shallow copy is
// being performed, this class can be moved, but not copied.
template <class AXPositionType>
class AXRange {
 public:
  using AXPositionInstance = std::unique_ptr<AXPositionType>;

  // Creates an `AXRange` encompassing the contents of the given `AXNode`.
  static AXRange RangeOfContents(const AXNode& node) {
    AXPositionInstance start_position = AXNodePosition::CreatePosition(
        node, /* child_index_or_text_offset */ 0);
    AXPositionInstance end_position =
        start_position->CreatePositionAtEndOfAnchor();
    return AXRange(std::move(start_position), std::move(end_position));
  }

  AXRange()
      : anchor_(AXPositionType::CreateNullPosition()),
        focus_(AXPositionType::CreateNullPosition()) {}

  AXRange(AXPositionInstance anchor, AXPositionInstance focus) {
    anchor_ = anchor ? std::move(anchor) : AXPositionType::CreateNullPosition();
    focus_ = focus ? std::move(focus) : AXPositionType::CreateNullPosition();
  }

  AXRange(const AXRange& other) = delete;

  AXRange(AXRange&& other) : AXRange() {
    anchor_.swap(other.anchor_);
    focus_.swap(other.focus_);
  }

  virtual ~AXRange() = default;

  AXPositionType* anchor() const {
    DCHECK(anchor_);
    return anchor_.get();
  }

  AXPositionType* focus() const {
    DCHECK(focus_);
    return focus_.get();
  }

  AXRange& operator=(const AXRange& other) = delete;

  AXRange& operator=(AXRange&& other) {
    if (this != &other) {
      anchor_ = AXPositionType::CreateNullPosition();
      focus_ = AXPositionType::CreateNullPosition();
      anchor_.swap(other.anchor_);
      focus_.swap(other.focus_);
    }
    return *this;
  }

  bool operator==(const AXRange& other) const {
    if (IsNull())
      return other.IsNull();
    return !other.IsNull() && *anchor_ == *other.anchor() &&
           *focus_ == *other.focus();
  }

  bool operator!=(const AXRange& other) const { return !(*this == other); }

  // Given a pair of AXPosition, determines how the first compares with the
  // second, relative to the order they would be iterated over by using
  // AXRange::Iterator to traverse all leaf text ranges in a tree.
  //
  // Notice that this method is different from using AXPosition::CompareTo since
  // the following logic takes into account BOTH tree pre-order traversal and
  // text offsets when both positions are located within the same anchor.
  //
  // Returns:
  //         0 - If both positions are equivalent.
  //        <0 - If the first position would come BEFORE the second.
  //        >0 - If the first position would come AFTER the second.
  //   nullopt - If positions are not comparable (see AXPosition::CompareTo).
  static std::optional<int> CompareEndpoints(const AXPositionType* first,
                                             const AXPositionType* second) {
    DCHECK(first->IsValid());
    DCHECK(second->IsValid());
    std::optional<int> tree_position_comparison =
        first->AsTreePosition()->CompareTo(*second->AsTreePosition());

    // When the tree comparison is nullopt, using value_or(1) forces a default
    // value of 1, making the following statement return nullopt as well.
    return (tree_position_comparison.value_or(1) != 0)
               ? tree_position_comparison
               : first->CompareTo(*second);
  }

  AXRange AsForwardRange() const {
    return (CompareEndpoints(anchor(), focus()).value_or(0) > 0)
               ? AXRange(focus_->Clone(), anchor_->Clone())
               : AXRange(anchor_->Clone(), focus_->Clone());
  }

  AXRange AsBackwardRange() const {
    return (CompareEndpoints(anchor(), focus()).value_or(0) < 0)
               ? AXRange(focus_->Clone(), anchor_->Clone())
               : AXRange(anchor_->Clone(), focus_->Clone());
  }

  bool IsCollapsed() const { return !IsNull() && *anchor_ == *focus_; }

  // We define a "leaf text range" as an AXRange whose endpoints are leaf text
  // positions located within the same anchor of the AXTree.
  bool IsLeafTextRange() const {
    return !IsNull() && anchor_->GetAnchor() == focus_->GetAnchor() &&
           anchor_->IsLeafTextPosition() && focus_->IsLeafTextPosition();
  }

  bool IsNull() const {
    DCHECK(anchor_ && focus_);
    return anchor_->IsNullPosition() || focus_->IsNullPosition();
  }

  std::string ToString() const {
    return "Range\nAnchor:" + anchor_->ToString() +
           "\nFocus:" + focus_->ToString();
  }

  // We can decompose any given AXRange into multiple "leaf text ranges".
  // As an example, consider the following HTML code:
  //
  //   <p>line with text<br><input type="checkbox">line with checkbox</p>
  //
  // It will produce the following AXTree; notice that the leaf text nodes
  // (enclosed in parenthesis) compose its text representation:
  //
  //   paragraph
  //     staticText name='line with text'
  //       (inlineTextBox name='line with text')
  //     lineBreak name='<newline>'
  //       (inlineTextBox name='<newline>')
  //     (checkBox)
  //     staticText name='line with checkbox'
  //       (inlineTextBox name='line with checkbox')
  //
  // Suppose we have an AXRange containing all elements from the example above.
  // The text representation of such range, with AXRange's endpoints marked by
  // opening and closing brackets, will look like the following:
  //
  //   "[line with text\n{checkBox}line with checkbox]"
  //
  // Note that in the text representation {checkBox} is not visible, but it is
  // effectively a "leaf text range", so we include it in the example above only
  // to visualize how the iterator should work.
  //
  // Decomposing the AXRange above into its "leaf text ranges" would result in:
  //
  //   "[line with text][\n][{checkBox}][line with checkbox]"
  //
  // This class allows AXRange to be iterated through all "leaf text ranges"
  // contained between its endpoints, composing the entire range.
  class Iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = AXRange;
    using difference_type = std::ptrdiff_t;
    using pointer = AXRange*;
    using reference = AXRange&;

    Iterator()
        : current_start_(AXPositionType::CreateNullPosition()),
          iterator_end_(AXPositionType::CreateNullPosition()) {}

    Iterator(AXPositionInstance start, AXPositionInstance end) {
      if (end && !end->IsNullPosition()) {
        current_start_ = !start ? AXPositionType::CreateNullPosition()
                                : start->AsLeafTextPosition();
        iterator_end_ = end->AsLeafTextPosition();
      } else {
        current_start_ = AXPositionType::CreateNullPosition();
        iterator_end_ = AXPositionType::CreateNullPosition();
      }
    }

    Iterator(const Iterator& other) = delete;

    Iterator(Iterator&& other)
        : current_start_(std::move(other.current_start_)),
          iterator_end_(std::move(other.iterator_end_)) {}

    ~Iterator() = default;

    bool operator==(const Iterator& other) const {
      return current_start_->GetAnchor() == other.current_start_->GetAnchor() &&
             iterator_end_->GetAnchor() == other.iterator_end_->GetAnchor() &&
             *current_start_ == *other.current_start_ &&
             *iterator_end_ == *other.iterator_end_;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    // Only forward iteration is supported, so operator-- is not implemented.
    Iterator& operator++() {
      DCHECK(!current_start_->IsNullPosition());
      if (current_start_->GetAnchor() == iterator_end_->GetAnchor()) {
        current_start_ = AXPositionType::CreateNullPosition();
      } else {
        current_start_ = current_start_->CreateNextLeafTreePosition();
        DCHECK_LE(*current_start_, *iterator_end_);
      }
      return *this;
    }

    AXRange operator*() const {
      DCHECK(!current_start_->IsNullPosition());
      AXPositionInstance current_end =
          (current_start_->GetAnchor() != iterator_end_->GetAnchor())
              ? current_start_->CreatePositionAtEndOfAnchor()
              : iterator_end_->Clone();
      DCHECK_LE(*current_end, *iterator_end_);

      AXRange current_leaf_text_range(current_start_->AsTextPosition(),
                                      current_end->AsTextPosition());
      DCHECK(current_leaf_text_range.IsLeafTextRange());
      return std::move(current_leaf_text_range);
    }

   private:
    AXPositionInstance current_start_;
    AXPositionInstance iterator_end_;
  };

  Iterator begin() const {
    if (IsNull())
      return Iterator(nullptr, nullptr);
    AXRange forward_range = AsForwardRange();
    return Iterator(std::move(forward_range.anchor_),
                    std::move(forward_range.focus_));
  }

  Iterator end() const {
    if (IsNull())
      return Iterator(nullptr, nullptr);
    AXRange forward_range = AsForwardRange();
    return Iterator(nullptr, std::move(forward_range.focus_));
  }

  // Returns the concatenation of the accessible names of all text nodes
  // contained between this AXRange's endpoints.
  // Pass a |max_count| of -1 to retrieve all text in the AXRange.
  // Note that if this AXRange has its anchor or focus located at an ignored
  // position, we shrink the range to the closest unignored positions.
  std::u16string GetText(
      AXTextConcatenationBehavior concatenation_behavior =
          AXTextConcatenationBehavior::kWithoutParagraphBreaks,
      AXEmbeddedObjectBehavior embedded_object_behavior =
          AXEmbeddedObjectBehavior::kExposeCharacterForHypertext,
      int max_count = -1,
      bool include_ignored = false,
      std::vector<size_t>* appended_newlines_indices = nullptr) const {
    if (max_count == 0 || IsNull())
      return std::u16string();

    std::optional<int> endpoint_comparison =
        CompareEndpoints(anchor(), focus());
    if (!endpoint_comparison)
      return std::u16string();

    AXPositionInstance start = (endpoint_comparison.value() < 0)
                                   ? anchor_->AsLeafTextPosition()
                                   : focus_->AsLeafTextPosition();
    AXPositionInstance end = (endpoint_comparison.value() < 0)
                                 ? focus_->AsLeafTextPosition()
                                 : anchor_->AsLeafTextPosition();

    std::u16string range_text;
    bool is_first_non_whitespace_leaf = true;
    bool crossed_paragraph_boundary = false;
    bool is_first_included_leaf = true;
    bool found_trailing_newline = false;

    while (!start->IsNullPosition()) {
      DCHECK(start->IsLeafTextPosition());
      DCHECK_GE(start->text_offset(), 0);
      const bool start_is_unignored = !start->IsIgnored();
      const bool start_is_in_white_space = start->IsInWhiteSpace();

      if (include_ignored || start_is_unignored) {
        if (concatenation_behavior ==
                AXTextConcatenationBehavior::kWithParagraphBreaks &&
            !start_is_in_white_space) {
          if (is_first_non_whitespace_leaf && !is_first_included_leaf) {
            // The first non-whitespace leaf in the range could be preceded by
            // whitespace spanning even before the start of this range, we need
            // to check such positions in order to correctly determine if this
            // is a paragraph's start (see |AXPosition::AtStartOfParagraph|).
            // However, if the first paragraph boundary in the range is ignored,
            // e.g. <div aria-hidden="true"></div>, we do not take it into
            // consideration even when `include_ignored` == true, because the
            // beginning of the text range, as experienced by the user, is after
            // any trailing ignored nodes.
            crossed_paragraph_boundary =
                start_is_unignored && start->AtStartOfParagraph();
          }

          // When preserving layout line breaks, don't append `\n` next if the
          // previous leaf position was a <br> (already ending with a newline).
          if (crossed_paragraph_boundary && !found_trailing_newline) {
            range_text += u"\n";
            if (appended_newlines_indices) {
              appended_newlines_indices->push_back(range_text.length() - 1);
            }
          }

          is_first_non_whitespace_leaf = false;
          crossed_paragraph_boundary = false;
        }

        int current_end_offset =
            (start->GetAnchor() != end->GetAnchor())
                ? start->MaxTextOffset(embedded_object_behavior)
                : end->text_offset();

        if (current_end_offset > start->text_offset()) {
          int characters_to_append =
              (max_count > 0)
                  ? std::min(max_count - static_cast<int>(range_text.length()),
                             current_end_offset - start->text_offset())
                  : current_end_offset - start->text_offset();

          std::u16string position_text =
              start->GetText(embedded_object_behavior);
          if (start->text_offset() < static_cast<int>(position_text.length())) {
            range_text += position_text.substr(start->text_offset(),
                                               characters_to_append);
          }

          // To minimize user confusion, collapse all whitespace following any
          // line break unless it is a hard line break (<br> or a text node with
          // a single '\n' character), or an empty object such as an empty text
          // field.
          found_trailing_newline =
              start->GetAnchor()->IsLineBreak() ||
              (found_trailing_newline && start_is_in_white_space);
        }

        DCHECK(max_count < 0 ||
               static_cast<int>(range_text.length()) <= max_count);
        is_first_included_leaf = false;
      }

      if (start->GetAnchor() == end->GetAnchor() ||
          static_cast<int>(range_text.length()) == max_count) {
        break;
      }

      ax::mojom::Role prev_role = start->GetAnchor()->GetRole();
      start = start->CreateNextLeafTextPosition();
      // We should not mark `cross_paragraph_boundary` as true if the start
      // anchor is a `kListMarker` since there should be no newline added
      // by default after the `kListMarker` node.
      if (concatenation_behavior ==
              AXTextConcatenationBehavior::kWithParagraphBreaks &&
          !crossed_paragraph_boundary && !is_first_non_whitespace_leaf &&
          prev_role != ax::mojom::Role::kListMarker) {
        crossed_paragraph_boundary = start->AtStartOfParagraph();
      }
    }

    return range_text;
  }

  // Appends rects of all anchor nodes that span between anchor_ and focus_.
  // Rects outside of the viewport are skipped.
  // Coordinate system is determined by the passed-in delegate.
  std::vector<gfx::Rect> GetRects(AXRangeRectDelegate* delegate) const {
    std::vector<gfx::Rect> rects;

    AXPositionInstance range_start = anchor()->AsLeafTextPosition();
    AXPositionInstance range_end = focus()->AsLeafTextPosition();

    // For a degenerate range, we want to fetch unclipped bounding rect, because
    // text with the same start and end off set (i.e. degenerate) will have an
    // inner text bounding rect with height of the character and width of 0,
    // which the browser platform will consider as an empty rect and ends up
    // clipping it, resulting in size 0x1 rect.
    // After we retrieve the unclipped bounding rect, we want to set its width
    // to 1 to represent a caret/insertion point.
    //
    // Note: The caller of this function is only UIA TextPattern, so displaying
    // bounding rects for degenerate range is only limited for UIA currently.
    if (IsCollapsed() && range_start->IsInTextObject()) {
      AXOffscreenResult offscreen_result;
      gfx::Rect degenerate_range_rect = delegate->GetInnerTextRangeBoundsRect(
          range_start->tree_id(), range_start->anchor_id(),
          range_start->text_offset(), range_end->text_offset(),
          AXClippingBehavior::kUnclipped, &offscreen_result);
      if (offscreen_result == AXOffscreenResult::kOnscreen) {
        DCHECK(degenerate_range_rect.width() == 0);
        degenerate_range_rect.set_width(1);
        rects.push_back(degenerate_range_rect);
      }

      return rects;
    }

    for (const AXRange& leaf_text_range : *this) {
      DCHECK(leaf_text_range.IsLeafTextRange());
      AXPositionType* current_line_start = leaf_text_range.anchor();
      AXPositionType* current_line_end = leaf_text_range.focus();

      // We want to skip ranges from ignored nodes.
      if (current_line_start->IsIgnored())
        continue;

      // For text anchors, we retrieve the bounding rectangles of its text
      // content. For non-text anchors (such as checkboxes, images, etc.), we
      // want to directly retrieve their bounding rectangles. Since text fields
      // in Views do not have text nodes as children (the text is in the text
      // field itself), we need to expose the inner bounds of those nodes too.
      AXOffscreenResult offscreen_result;
      gfx::Rect current_rect;
      if (current_line_start->GetAnchor()->IsLineBreak() ||
          current_line_start->IsInTextObject() ||
          (current_line_start->GetAnchor()->IsView() &&
           current_line_start->IsInTextField())) {
        current_rect = delegate->GetInnerTextRangeBoundsRect(
            current_line_start->tree_id(), current_line_start->anchor_id(),
            current_line_start->text_offset(), current_line_end->text_offset(),
            AXClippingBehavior::kClipped, &offscreen_result);
      } else {
        current_rect = delegate->GetBoundsRect(current_line_start->tree_id(),
                                               current_line_start->anchor_id(),
                                               &offscreen_result);
      }

      // If the bounding box of the current range is clipped because it lies
      // outside an ancestor’s bounds, then the bounding box is pushed to the
      // nearest edge of such ancestor's bounds, with its width and height
      // forced to be 1, and the node will be marked as "offscreen".
      //
      // Only add rectangles that are not empty and not marked as "offscreen".
      //
      // See the documentation for how bounding boxes are calculated in AXTree:
      // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/accessibility/offscreen.md
      if (!current_rect.IsEmpty() &&
          offscreen_result == AXOffscreenResult::kOnscreen)
        rects.push_back(current_rect);
    }
    return rects;
  }

 private:
  AXPositionInstance anchor_;
  AXPositionInstance focus_;
};

template <class AXPositionType>
std::ostream& operator<<(std::ostream& stream,
                         const AXRange<AXPositionType>& range) {
  return stream << range.ToString();
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_RANGE_H_
