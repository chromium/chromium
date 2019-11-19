// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_STYLED_LABEL_H_
#define UI_VIEWS_CONTROLS_STYLED_LABEL_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace views {

class Label;
class Link;
class StyledLabelListener;

// A class which can apply mixed styles to a block of text. Currently, text is
// always multiline. Trailing whitespace in the styled label text is not
// supported and will be trimmed on StyledLabel construction. Leading
// whitespace is respected, provided not only whitespace fits in the first line.
// In this case, leading whitespace is ignored.
class VIEWS_EXPORT StyledLabel : public View, public LinkListener {
 public:
  METADATA_HEADER(StyledLabel);

  using LinkTargets = std::map<Link*, gfx::Range>;

  // TestApi is used for tests to get internal implementation details.
  class VIEWS_EXPORT TestApi {
   public:
    explicit TestApi(StyledLabel* view);
    ~TestApi();

    const LinkTargets& link_targets();

   private:
    StyledLabel* const view_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Parameters that define label style for a styled label's text range.
  struct VIEWS_EXPORT RangeStyleInfo {
    RangeStyleInfo();
    RangeStyleInfo(const RangeStyleInfo&);
    RangeStyleInfo& operator=(const RangeStyleInfo&);
    ~RangeStyleInfo();

    // Creates a range style info with default values for link.
    static RangeStyleInfo CreateForLink();

    bool IsLink() const;

    // Allows full customization of the font used in the range. Ignores the
    // StyledLabel's default text context and |text_style|.
    base::Optional<gfx::FontList> custom_font;

    // The style::TextStyle for this range.
    base::Optional<int> text_style;

    // Overrides the text color given by |text_style| for this range. Default is
    // SK_ColorTRANSPARENT, indicating not to override.
    // DEPRECATED: Use TextStyle.
    SkColor override_color = SK_ColorTRANSPARENT;

    // Tooltip for the range.
    base::string16 tooltip;

    // If set, the whole range will be put on a single line.
    bool disable_line_wrapping = false;

    // A custom view shown instead of the underlying text. Ownership of custom
    // views must be passed to StyledLabel via AddCustomView().
    View* custom_view = nullptr;
  };

  // Sizing information for laying out the label based on a particular width.
  struct VIEWS_EXPORT LayoutSizeInfo {
    explicit LayoutSizeInfo(int max_valid_width);
    LayoutSizeInfo(const LayoutSizeInfo&);
    LayoutSizeInfo& operator=(const LayoutSizeInfo&);
    ~LayoutSizeInfo();

    // The maximum width for which this info is guaranteed to be valid.
    // Requesting a larger width than this will force a recomputation.
    int max_valid_width = 0;

    // The actual size needed to lay out the label for a requested width of
    // |max_valid_width|.  total_size.width() is at most |max_valid_width| but
    // may be smaller depending on how line wrapping is computed.  Requesting a
    // smaller width than this will force a recomputation.
    gfx::Size total_size;

    // The sizes of each line of child views.  |size| can be computed directly
    // from these values and is kept separately just for convenience.
    std::vector<gfx::Size> line_sizes;
  };

  // Note that any trailing whitespace in |text| will be trimmed.
  StyledLabel(const base::string16& text, StyledLabelListener* listener);
  ~StyledLabel() override;

  // Sets the text to be displayed, and clears any previous styling.
  const base::string16& GetText() const;
  void SetText(const base::string16& text);

  // Returns the font list that results from the default text context and style
  // for ranges. This can be used as the basis for a range |custom_font|.
  gfx::FontList GetDefaultFontList() const;

  // Marks the given range within |text_| with style defined by |style_info|.
  // |range| must be contained in |text_|.
  void AddStyleRange(const gfx::Range& range, const RangeStyleInfo& style_info);

  // Passes ownership of a custom view for use by RangeStyleInfo structs.
  void AddCustomView(std::unique_ptr<View> custom_view);

  // Get/Set the context of this text. All ranges have the same context.
  // |text_context| must be a value from views::style::TextContext.
  int GetTextContext() const;
  void SetTextContext(int text_context);

  // Set the default text style.
  // |text_style| must be a value from views::style::TextStyle.
  int GetDefaultTextStyle() const;
  void SetDefaultTextStyle(int text_style);

  // Get or set the distance in pixels between baselines of multi-line text.
  // Default is 0, indicating the distance between lines should be the standard
  // one for the label's text, font list, and platform.
  int GetLineHeight() const;
  void SetLineHeight(int height);

  // Gets/Sets the color of the background on which the label is drawn. This
  // won't be explicitly drawn, but the label will force the text color to be
  // readable over it.
  SkColor GetDisplayedOnBackgroundColor() const;
  void SetDisplayedOnBackgroundColor(SkColor color);

  bool GetAutoColorReadabilityEnabled() const;
  void SetAutoColorReadabilityEnabled(bool auto_color_readability);

  // Returns the layout size information that would be used to layout the label
  // at width |w|.  This can be used by callers who need more detail than what's
  // provided by GetHeightForWidth().
  const LayoutSizeInfo& GetLayoutSizeInfoForWidth(int w) const;

  // Resizes the label so its width is set to the fixed width and its height
  // deduced accordingly. Even if all widths of the lines are shorter than
  // |fixed_width|, the given value is applied to the element's width.
  // This is only intended for multi-line labels and is useful when the label's
  // text contains several lines separated with \n.
  // |fixed_width| is the fixed width that will be used (longer lines will be
  // wrapped).  If 0, no fixed width is enforced.
  void SizeToFit(int fixed_width);

  // View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;
  void PreferredSizeChanged() override;

  // LinkListener implementation:
  void LinkClicked(Link* source, int event_flags) override;

  // Sets the horizontal alignment; the argument value is mirrored in RTL UI.
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);

  // Clears all the styles applied to the label.
  void ClearStyleRanges();

 private:
  struct StyleRange {
    StyleRange(const gfx::Range& range,
               const RangeStyleInfo& style_info)
        : range(range),
          style_info(style_info) {
    }
    ~StyleRange() = default;

    bool operator<(const StyleRange& other) const;

    gfx::Range range;
    RangeStyleInfo style_info;
  };
  using StyleRanges = std::list<StyleRange>;

  // Child view-related information for layout.
  struct LayoutViews;

  // Returns the starting X coordinate for the views in a line, based on the
  // current |horizontal_alignment_| and insets and given the amount of excess
  // space available on that line.
  int StartX(int excess_space) const;

  // Returns the default line height, based on the default style.
  int GetDefaultLineHeight() const;

  // Returns the FontList that should be used for |range|.
  gfx::FontList GetFontListForRange(
      const StyleRanges::const_iterator& range) const;

  // Sets |layout_size_info_| and |layout_views_| for the given |width|.  No-op
  // if current_width <= width <= max_width, where:
  //   current_width = layout_size_info_.total_size.width()
  //   width = max(width, GetInsets().width())
  //   max_width = layout_size_info_.max_valid_width
  void CalculateLayout(int width) const;

  // Creates a Label for a given |text|, |style_info|, and |range|.
  std::unique_ptr<Label> CreateLabel(const base::string16& text,
                                     const RangeStyleInfo& style_info,
                                     const gfx::Range& range) const;

  // The text to display.
  base::string16 text_;

  int text_context_ = style::CONTEXT_LABEL;
  int default_text_style_ = style::STYLE_PRIMARY;

  // Line height. If zero, style::GetLineHeight() is used.
  int specified_line_height_ = 0;

  // The listener that will be informed of link clicks.
  StyledLabelListener* listener_;

  // The ranges that should be linkified, sorted by start position.
  StyleRanges style_ranges_;

  // A mapping from a view to the range it corresponds to in |text_|. Only views
  // that correspond to ranges with is_link style set will be added to the map.
  LinkTargets link_targets_;

  // Owns the custom views used to replace ranges of text with icons, etc.
  std::set<std::unique_ptr<View>> custom_views_;

  // Saves the effects of the last CalculateLayout() call to avoid repeated
  // calculation.  |layout_size_info_| can then be cached until the next
  // recalculation, while |layout_views_| only exists until the next Layout().
  mutable LayoutSizeInfo layout_size_info_{0};
  mutable std::unique_ptr<LayoutViews> layout_views_;

  // Background color on which the label is drawn, for auto color readability.
  SkColor displayed_on_background_color_ = SK_ColorWHITE;
  bool displayed_on_background_color_set_ = false;

  // Controls whether the text is automatically re-colored to be readable on the
  // background.
  bool auto_color_readability_enabled_ = true;

  // The horizontal alignment. This value is flipped for RTL. The default
  // behavior is to align left in LTR UI and right in RTL UI.
  gfx::HorizontalAlignment horizontal_alignment_ = gfx::ALIGN_LEFT;

  DISALLOW_COPY_AND_ASSIGN(StyledLabel);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_STYLED_LABEL_H_
