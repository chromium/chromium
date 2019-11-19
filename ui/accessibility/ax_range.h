// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_RANGE_H_
#define UI_ACCESSIBILITY_AX_RANGE_H_

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_offscreen_result.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_manager_map.h"

namespace ui {

// Specifies how AXRange::GetText treats line breaks introduced by layout.
// For example, consider the following HTML snippet: "A<div>B</div>C".
enum class AXTextConcatenationBehavior {
  // Preserve any introduced line breaks, e.g. GetText = "A\nB\nC".
  kAsInnerText,
  // Ignore any introduced line breaks, e.g. GetText = "ABC".
  kAsTextContent
};

class AXRangeScreenRectDelegate {
 public:
  virtual gfx::Rect GetInnerTextRangeBoundsRect(
      AXTreeID tree_id,
      AXNode::AXID node_id,
      int start_offset,
      int end_offset,
      AXOffscreenResult* offscreen_result) = 0;
  virtual gfx::Rect GetBoundsRect(AXTreeID tree_id,
                                  AXNode::AXID node_id,
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

  AXRange AsForwardRange() const {
    // When we have a range with an empty text representation, its endpoints
    // would be considered equal as text positions, but they could be located in
    // different anchors of the AXTree. Compare them as tree positions first to
    // preserve their relative order from the pre-order traversal of the tree,
    // so that AXRange::Iterator behaves correctly.
    AXPositionInstance anchor_tree_position = anchor_->AsTreePosition();
    AXPositionInstance focus_tree_position = focus_->AsTreePosition();

    return (*focus_tree_position < *anchor_tree_position || *focus_ < *anchor_)
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
  class Iterator : public std::iterator<std::input_iterator_tag, AXRange> {
   public:
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
  // Pass -1 for max_count to retrieve all text.
  base::string16 GetText(AXTextConcatenationBehavior concatenation_behavior =
                             AXTextConcatenationBehavior::kAsTextContent,
                         int max_count = -1,
                         bool include_ignored = false,
                         size_t* appended_newlines_count = nullptr) const {
    base::string16 range_text;
    bool should_append_newline = false;
    bool found_trailing_newline = false;
    size_t computed_newlines_count = 0;
    for (const AXRange& leaf_text_range : *this) {
      AXPositionType* start = leaf_text_range.anchor();
      AXPositionType* end = leaf_text_range.focus();

      DCHECK_GE(start->text_offset(), 0);
      DCHECK_LE(start->text_offset(), end->text_offset());

      if (should_append_newline) {
        range_text += base::ASCIIToUTF16("\n");
        computed_newlines_count++;
      }

      base::string16 current_anchor_text = start->GetText();
      int current_leaf_text_length = end->text_offset() - start->text_offset();

      if (current_leaf_text_length > 0) {
        int characters_to_append =
            (max_count >= 0) ? std::min(max_count - int{range_text.length()},
                                        current_leaf_text_length)
                             : current_leaf_text_length;

        // Collapse all whitespace following any line break.
        found_trailing_newline =
            start->IsInLineBreak() ||
            (found_trailing_newline && start->IsInWhiteSpace());

        if (!include_ignored && !start->IsIgnored()) {
          range_text += current_anchor_text.substr(start->text_offset(),
                                                   characters_to_append);
        }
      }

      DCHECK(max_count < 0 || int{range_text.length()} <= max_count);
      if (int{range_text.length()} == max_count)
        break;

      // When preserving layout line breaks, don't append a newline next if the
      // current leaf range is a <br> (already ending with a '\n' character) or
      // its respective anchor is invisible to the text representation.
      if (concatenation_behavior == AXTextConcatenationBehavior::kAsInnerText)
        should_append_newline =
            !found_trailing_newline && end->AtEndOfParagraph();
    }
    if (appended_newlines_count)
      *appended_newlines_count = computed_newlines_count;
    return range_text;
  }

  // Appends rects in screen coordinates of all anchor nodes that span between
  // anchor_ and focus_. Rects outside of the viewport are skipped.
  std::vector<gfx::Rect> GetScreenRects(
      AXRangeScreenRectDelegate* delegate) const {
    std::vector<gfx::Rect> rects;

    for (const AXRange& leaf_text_range : *this) {
      DCHECK(!leaf_text_range.IsNull());
      AXPositionInstance current_line_end =
          leaf_text_range.focus()->AsLeafTextPosition();
      AXPositionInstance current_line_start =
          leaf_text_range.anchor()->AsLeafTextPosition();

      if (current_line_start->GetAnchor()->data().role ==
          ax::mojom::Role::kInlineTextBox) {
        current_line_start = current_line_start->CreateParentPosition();
        current_line_end = current_line_end->CreateParentPosition();
      }

      AXTreeID current_tree_id = current_line_start->tree_id();

      // For text anchors, we retrieve the bounding rectangles of its text
      // content. For non-text anchors (such as checkboxes, images, etc.), we
      // want to directly retrieve their bounding rectangles.
      AXOffscreenResult offscreen_result;
      gfx::Rect current_rect =
          (current_line_start->IsInLineBreak() ||
           current_line_start->IsInTextObject())
              ? delegate->GetInnerTextRangeBoundsRect(
                    current_tree_id, current_line_start->anchor_id(),
                    current_line_start->text_offset(),
                    current_line_end->text_offset(), &offscreen_result)
              : delegate->GetBoundsRect(current_tree_id,
                                        current_line_start->anchor_id(),
                                        &offscreen_result);

      // We only add rects that do not represent a degenerate range and rects
      // that are onscreen.
      // If the represented range is degenerate, the bounding rectangles will be
      // empty.
      // If the represented range is offscreen, the bounding rectangles will be
      // clipped, with its width/height set to 1. The bounding rectangles is not
      // set to empty when it is offscreen.
      // Documentation for offscreen bounding box size calculation:
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
