// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_STYLED_LABEL_H_
#define UI_VIEWS_CONTROLS_STYLED_LABEL_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/class_property.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/link.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace views {

class Label;
class Link;
class LinkFragment;

// A class which can apply mixed styles to a block of text. Currently, text is
// always multiline. Trailing whitespace in the styled label text is not
// supported and will be trimmed on StyledLabel construction. Leading
// whitespace is respected, provided not only whitespace fits in the first line.
// In this case, leading whitespace is ignored.
class VIEWS_EXPORT StyledLabel : public View {
  METADATA_HEADER(StyledLabel, View)

 public:
  // Parameters that define label style for a styled label's text range.
  struct VIEWS_EXPORT RangeStyleInfo {
    RangeStyleInfo();
    RangeStyleInfo(const RangeStyleInfo&);
    RangeStyleInfo& operator=(const RangeStyleInfo&);
    ~RangeStyleInfo();

    // Creates a range style info with default values for link.
    static RangeStyleInfo CreateForLink(Link::ClickedCallback callback);
    static RangeStyleInfo CreateForLink(base::RepeatingClosure callback);

    // Allows full customization of the font used in the range. Ignores the
    // StyledLabel's default text context and |text_style|.
    std::optional<gfx::FontList> custom_font;

    // The style::TextStyle for this range.
    std::optional<int> text_style;

    // Overrides the text color given by |text_style| for this range.
    // DEPRECATED: Use TextStyle.
    std::optional<SkColor> override_color;

    // Overrides the text color given by |text_style| for this range.
    std::optional<ui::ColorId> override_color_id;

    // A callback to be called when this link is clicked. Only used if
    // |text_style| is style::STYLE_LINK.
    Link::ClickedCallback callback;

    // Tooltip for the range.
    std::u16string tooltip;

    // Accessible name for the range.
    std::u16string accessible_name;

    // A custom view shown instead of the underlying text. Ownership of custom
    // views must be passed to StyledLabel via AddCustomView().
    raw_ptr<View> custom_view = nullptr;
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

    // The sizes of each line of child views.  |total_size| can be computed
    // directly from these values and is kept separately just for convenience.
    std::vector<gfx::Size> line_sizes;
  };

  using ColorVariant = absl::variant<absl::monostate, SkColor, ui::ColorId>;

  StyledLabel();

  StyledLabel(const StyledLabel&) = delete;
  StyledLabel& operator=(const StyledLabel&) = delete;

  ~StyledLabel() override;

  // Sets the text to be displayed, and clears any previous styling.  Trailing
  // whitespace is trimmed from the text.
  const std::u16string& GetText() const;
  void SetText(std::u16string text);

  // Returns the FontList that should be used. |style_info| is an optional
  // argument that takes precedence over the default values.
  gfx::FontList GetFontList(
      const RangeStyleInfo& style_info = RangeStyleInfo()) const;

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

  // Set the default enabled color id.
  std::optional<ui::ColorId> GetDefaultEnabledColorId() const;
  void SetDefaultEnabledColorId(std::optional<ui::ColorId> enabled_color_id);

  // Get or set the distance in pixels between baselines of multi-line text.
  // Default is 0, indicating the distance between lines should be the standard
  // one for the label's text, font list, and platform.
  int GetLineHeight() const;
  void SetLineHeight(int height);

  // Gets/Sets the color or color id of the background on which the label is
  // drawn. This won't be explicitly drawn, but the label will force the text
  // color to be readable over it.
  ColorVariant GetDisplayedOnBackgroundColor() const;
  void SetDisplayedOnBackgroundColor(ColorVariant color);

  bool GetAutoColorReadabilityEnabled() const;
  void SetAutoColorReadabilityEnabled(bool auto_color_readability);

  bool GetSubpixelRenderingEnabled() const;
  void SetSubpixelRenderingEnabled(bool subpixel_rendering_enabled);

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

  [[nodiscard]] base::CallbackListSubscription AddTextChangedCallback(
      views::PropertyChangedCallback callback);

  // View:
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void Layout(PassKey) override;
  void PreferredSizeChanged() override;

  // Sets the horizontal alignment; the argument value is mirrored in RTL UI.
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);

  // Clears all the styles applied to the label.
  void ClearStyleRanges();

  // Sends a space keypress to the first child that is a link.  Assumes at least
  // one such child exists.
  void ClickFirstLinkForTesting();

  // Gets the first child that is a link. Returns nullptr if there isn't any.
  views::Link* GetFirstLinkForTesting();

 private:
  struct StyleRange {
    StyleRange(const gfx::Range& range, const RangeStyleInfo& style_info)
        : range(range), style_info(style_info) {}
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

  // Sets |layout_size_info_| and |layout_views_| for the given |width|.  No-op
  // if current_width <= width <= max_width, where:
  //   current_width = layout_size_info_.total_size.width()
  //   width = max(width, GetInsets().width())
  //   max_width = layout_size_info_.max_valid_width
  void CalculateLayout(int width) const;

  // Creates a Label for a given |text|, |style_info|, and |range|.
  std::unique_ptr<Label> CreateLabel(
      const std::u16string& text,
      const RangeStyleInfo& style_info,
      const gfx::Range& range,
      LinkFragment** previous_link_component) const;

  // Update the label background color from the theme or
  // |displayed_on_background_color_| if set.
  void UpdateLabelBackgroundColor();

  // Remove all child views. Place all custom views back into custom_views_ and
  // delete the rest.
  void RemoveOrDeleteAllChildViews();

  void RecreateChildViews();

  // The text to display.
  std::u16string text_;

  int text_context_ = style::CONTEXT_LABEL;
  int default_text_style_ = style::STYLE_PRIMARY;
  std::optional<ui::ColorId> default_enabled_color_id_;

  std::optional<int> line_height_;
  int fixed_width_ = 0;

  // Temporarily owns the custom views until they've been been placed into the
  // StyledLabel's child list. This list also holds the custom views during
  // layout.
  std::list<std::unique_ptr<View>> custom_views_;

  // Temporarily owns the views to be deleted during layout. These views might
  // still be referenced on the stack. If we delete them immediately, UaFs
  // could happen when the stack unwinds.
  std::vector<std::unique_ptr<View>> pending_delete_views_;

  // The ranges that should be linkified, sorted by start position.
  StyleRanges style_ranges_;

  // Saves the effects of the last CalculateLayout() call to avoid repeated
  // calculation.  |layout_size_info_| can then be cached until the next
  // recalculation, while |layout_views_| only exists until the next Layout().
  mutable LayoutSizeInfo layout_size_info_{0};
  mutable std::unique_ptr<LayoutViews> layout_views_;
  // Saves the LayoutSizeInfo for additional CalculateLayout() calls. Layout
  // managers sometimes repeatedly ask for size information for the same (small)
  // number of widths. Caching multiple LayoutSideInfos helps avoid doing many
  // unnecessary calculations.
  mutable base::LRUCache<int, LayoutSizeInfo> layout_size_info_cache_{16};

  // Background color on which the label is drawn, for auto color readability.
  ColorVariant displayed_on_background_color_;

  // Controls whether the text is automatically re-colored to be readable on the
  // background.
  bool auto_color_readability_enabled_ = true;

  // Controls whether subpixel rendering is enabled.
  bool subpixel_rendering_enabled_ = true;

  // Controls whether subviews need to be recreated. Recreating subviews can
  // cause some functionality to break under certain circumstances.
  // eg: If re-creating the subview occurs after OnMousePressed() and before
  // OnMouseRelease(), the link will not be clickable.
  bool need_recreate_child_ = true;

  // The horizontal alignment. This value is flipped for RTL. The default
  // behavior is to align left in LTR UI and right in RTL UI.
  gfx::HorizontalAlignment horizontal_alignment_ =
      base::i18n::IsRTL() ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, StyledLabel, View)
VIEW_BUILDER_PROPERTY(const std::u16string&, Text)
VIEW_BUILDER_PROPERTY(int, TextContext)
VIEW_BUILDER_PROPERTY(int, DefaultTextStyle)
VIEW_BUILDER_PROPERTY(int, LineHeight)
VIEW_BUILDER_PROPERTY(StyledLabel::ColorVariant, DisplayedOnBackgroundColor)
VIEW_BUILDER_PROPERTY(bool, AutoColorReadabilityEnabled)
VIEW_BUILDER_PROPERTY(gfx::HorizontalAlignment, HorizontalAlignment)
VIEW_BUILDER_PROPERTY(std::optional<ui::ColorId>, DefaultEnabledColorId)
VIEW_BUILDER_METHOD(SizeToFit, int)
VIEW_BUILDER_METHOD(AddStyleRange, gfx::Range, StyledLabel::RangeStyleInfo)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, views::StyledLabel)

#endif  // UI_VIEWS_CONTROLS_STYLED_LABEL_H_
