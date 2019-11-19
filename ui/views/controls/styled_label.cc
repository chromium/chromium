// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/styled_label.h"

#include <stddef.h>

#include <algorithm>
#include <limits>

#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label_listener.h"

namespace views {

StyledLabel::TestApi::TestApi(StyledLabel* view) : view_(view) {}

StyledLabel::TestApi::~TestApi() = default;

const StyledLabel::LinkTargets& StyledLabel::TestApi::link_targets() {
  return view_->link_targets_;
}

StyledLabel::RangeStyleInfo::RangeStyleInfo() = default;
StyledLabel::RangeStyleInfo::RangeStyleInfo(const RangeStyleInfo&) = default;
StyledLabel::RangeStyleInfo& StyledLabel::RangeStyleInfo::operator=(
    const RangeStyleInfo&) = default;
StyledLabel::RangeStyleInfo::~RangeStyleInfo() = default;

// static
StyledLabel::RangeStyleInfo StyledLabel::RangeStyleInfo::CreateForLink() {
  RangeStyleInfo result;
  result.disable_line_wrapping = true;
  result.text_style = style::STYLE_LINK;
  return result;
}

bool StyledLabel::RangeStyleInfo::IsLink() const {
  return text_style && text_style.value() == style::STYLE_LINK;
}

StyledLabel::LayoutSizeInfo::LayoutSizeInfo(int max_valid_width)
    : max_valid_width(max_valid_width) {}

StyledLabel::LayoutSizeInfo::LayoutSizeInfo(const LayoutSizeInfo&) = default;
StyledLabel::LayoutSizeInfo& StyledLabel::LayoutSizeInfo::operator=(
    const LayoutSizeInfo&) = default;
StyledLabel::LayoutSizeInfo::~LayoutSizeInfo() = default;

bool StyledLabel::StyleRange::operator<(
    const StyledLabel::StyleRange& other) const {
  return range.start() < other.range.start();
}

struct StyledLabel::LayoutViews {
  // The updated data for StyledLabel::link_targets_.
  LinkTargets link_targets;

  // All views to be added as children, line by line.
  std::vector<std::vector<View*>> views_per_line;

  // The subset of |views| that are not owned anywhere else.  Basically, this is
  // all non-custom views; custom views should be owned by
  // StyledLabel::custom_views_.  These appear in the same order as |views|.
  std::vector<std::unique_ptr<View>> owned_views;
};

StyledLabel::StyledLabel(const base::string16& text,
                         StyledLabelListener* listener)
    : listener_(listener) {
  base::TrimWhitespace(text, base::TRIM_TRAILING, &text_);
}

StyledLabel::~StyledLabel() = default;

const base::string16& StyledLabel::GetText() const {
  return text_;
}

void StyledLabel::SetText(const base::string16& text) {
  if (text_ == text)
    return;

  text_ = text;
  style_ranges_.clear();
  RemoveAllChildViews(true);
  OnPropertyChanged(&text_, kPropertyEffectsPreferredSizeChanged);
}

gfx::FontList StyledLabel::GetDefaultFontList() const {
  return style::GetFont(text_context_, default_text_style_);
}

void StyledLabel::AddStyleRange(const gfx::Range& range,
                                const RangeStyleInfo& style_info) {
  DCHECK(!range.is_reversed());
  DCHECK(!range.is_empty());
  DCHECK(gfx::Range(0, text_.size()).Contains(range));

  // Insert the new range in sorted order.
  StyleRanges new_range;
  new_range.push_front(StyleRange(range, style_info));
  style_ranges_.merge(new_range);

  PreferredSizeChanged();
}

void StyledLabel::AddCustomView(std::unique_ptr<View> custom_view) {
  DCHECK(custom_view->owned_by_client());
  custom_views_.insert(std::move(custom_view));
}

int StyledLabel::GetTextContext() const {
  return text_context_;
}

void StyledLabel::SetTextContext(int text_context) {
  if (text_context_ == text_context)
    return;

  text_context_ = text_context;
  OnPropertyChanged(&text_context_, kPropertyEffectsPreferredSizeChanged);
}

int StyledLabel::GetDefaultTextStyle() const {
  return default_text_style_;
}

void StyledLabel::SetDefaultTextStyle(int text_style) {
  if (default_text_style_ == text_style)
    return;

  default_text_style_ = text_style;
  OnPropertyChanged(&default_text_style_, kPropertyEffectsPreferredSizeChanged);
}

int StyledLabel::GetLineHeight() const {
  return specified_line_height_;
}

void StyledLabel::SetLineHeight(int line_height) {
  if (specified_line_height_ == line_height)
    return;

  specified_line_height_ = line_height;
  OnPropertyChanged(&specified_line_height_,
                    kPropertyEffectsPreferredSizeChanged);
}

SkColor StyledLabel::GetDisplayedOnBackgroundColor() const {
  return displayed_on_background_color_;
}

void StyledLabel::SetDisplayedOnBackgroundColor(SkColor color) {
  if (displayed_on_background_color_ == color &&
      displayed_on_background_color_set_)
    return;

  displayed_on_background_color_ = color;
  displayed_on_background_color_set_ = true;

  for (View* child : children()) {
    DCHECK((child->GetClassName() == Label::kViewClassName) ||
           (child->GetClassName() == Link::kViewClassName));
    static_cast<Label*>(child)->SetBackgroundColor(color);
  }
  OnPropertyChanged(&displayed_on_background_color_, kPropertyEffectsNone);
}

bool StyledLabel::GetAutoColorReadabilityEnabled() const {
  return auto_color_readability_enabled_;
}

void StyledLabel::SetAutoColorReadabilityEnabled(bool auto_color_readability) {
  if (auto_color_readability_enabled_ == auto_color_readability)
    return;

  auto_color_readability_enabled_ = auto_color_readability;
  OnPropertyChanged(&auto_color_readability_enabled_, kPropertyEffectsNone);
}

const StyledLabel::LayoutSizeInfo& StyledLabel::GetLayoutSizeInfoForWidth(
    int w) const {
  CalculateLayout(w);
  return layout_size_info_;
}

void StyledLabel::SizeToFit(int fixed_width) {
  CalculateLayout(fixed_width == 0 ? std::numeric_limits<int>::max()
                                   : fixed_width);
  gfx::Size size = layout_size_info_.total_size;
  size.set_width(std::max(size.width(), fixed_width));
  SetSize(size);
}

void StyledLabel::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (text_context_ == style::CONTEXT_DIALOG_TITLE)
    node_data->role = ax::mojom::Role::kTitleBar;
  else
    node_data->role = ax::mojom::Role::kStaticText;

  node_data->SetName(GetText());
}

gfx::Size StyledLabel::CalculatePreferredSize() const {
  // Respect any existing size.  If there is none, default to a single line.
  CalculateLayout((width() == 0) ? std::numeric_limits<int>::max() : width());
  return layout_size_info_.total_size;
}

int StyledLabel::GetHeightForWidth(int w) const {
  return GetLayoutSizeInfoForWidth(w).total_size.height();
}

void StyledLabel::Layout() {
  CalculateLayout(width());

  // If the layout has been recalculated, add and position all views.
  if (layout_views_) {
    for (auto& link_target : layout_views_->link_targets)
      link_target.first->set_listener(this);
    link_targets_ = std::move(layout_views_->link_targets);

    // Delete all non-custom views on removal; custom views are owned-by-client.
    RemoveAllChildViews(true);

    DCHECK_EQ(layout_size_info_.line_sizes.size(),
              layout_views_->views_per_line.size());
    int line_y = GetInsets().top();
    auto next_owned_view = layout_views_->owned_views.begin();
    for (size_t line = 0; line < layout_views_->views_per_line.size(); ++line) {
      const auto& line_size = layout_size_info_.line_sizes[line];
      int x = StartX(width() - line_size.width());
      for (auto* view : layout_views_->views_per_line[line]) {
        gfx::Size size = view->GetPreferredSize();
        size.set_width(std::min(size.width(), width() - x));
        // Compute the view y such that the view center y and the line center y
        // match.  Because of added rounding errors, this is not the same as
        // doing (line_size.height() - size.height()) / 2.
        const int y = line_size.height() / 2 - size.height() / 2;
        view->SetBoundsRect({{x, line_y + y}, size});
        x += size.width();

        // Transfer ownership for any views in layout_views_->owned_views.  The
        // actual pointer passed is the same in both arms below, the only
        // difference is whether we're using the unique_ptr or raw pointer
        // version.
        if ((next_owned_view != layout_views_->owned_views.end()) &&
            (view == next_owned_view->get())) {
          AddChildView(std::move(*next_owned_view));
          ++next_owned_view;
        } else {
          AddChildView(view);
        }
      }
      line_y += line_size.height();
    }
    DCHECK(next_owned_view == layout_views_->owned_views.end());

    layout_views_.reset();
  } else if (horizontal_alignment_ != gfx::ALIGN_LEFT) {
    // Recompute all child X coordinates in case the width has shifted, which
    // will move the children if the label is center/right-aligned.  If the
    // width hasn't changed, all the SetX() calls below will no-op, so this
    // won't have side effects.
    int line_bottom = GetInsets().top();
    auto i = children().begin();
    for (const auto& line_size : layout_size_info_.line_sizes) {
      DCHECK(i != children().end());  // Should not have an empty trailing line.
      int x = StartX(width() - line_size.width());
      line_bottom += line_size.height();
      for (; (i != children().end()) && ((*i)->y() < line_bottom); ++i) {
        (*i)->SetX(x);
        x += (*i)->GetPreferredSize().width();
      }
    }
    DCHECK(i == children().end());  // Should not be short any lines.
  }
}

void StyledLabel::PreferredSizeChanged() {
  layout_size_info_ = LayoutSizeInfo(0);
  layout_views_.reset();
  View::PreferredSizeChanged();
}

void StyledLabel::LinkClicked(Link* source, int event_flags) {
  if (listener_)
    listener_->StyledLabelLinkClicked(this, link_targets_[source], event_flags);
}

// TODO(wutao): support gfx::ALIGN_TO_HEAD alignment.
void StyledLabel::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  DCHECK_NE(gfx::ALIGN_TO_HEAD, alignment);
  alignment = gfx::MaybeFlipForRTL(alignment);

  if (horizontal_alignment_ == alignment)
    return;
  horizontal_alignment_ = alignment;
  PreferredSizeChanged();
}

void StyledLabel::ClearStyleRanges() {
  style_ranges_.clear();
  PreferredSizeChanged();
}

int StyledLabel::StartX(int excess_space) const {
  int x = GetInsets().left();
  if (horizontal_alignment_ == gfx::ALIGN_LEFT)
    return x;
  return x + ((horizontal_alignment_ == gfx::ALIGN_CENTER) ? (excess_space / 2)
                                                           : excess_space);
}

int StyledLabel::GetDefaultLineHeight() const {
  return specified_line_height_ > 0
             ? specified_line_height_
             : std::max(
                   style::GetLineHeight(text_context_, default_text_style_),
                   GetDefaultFontList().GetHeight());
}

gfx::FontList StyledLabel::GetFontListForRange(
    const StyleRanges::const_iterator& range) const {
  if (range == style_ranges_.end())
    return GetDefaultFontList();

  return range->style_info.custom_font
             ? range->style_info.custom_font.value()
             : style::GetFont(
                   text_context_,
                   range->style_info.text_style.value_or(default_text_style_));
}

void StyledLabel::CalculateLayout(int width) const {
  const gfx::Insets insets = GetInsets();
  width = std::max(width, insets.width());
  if (width >= layout_size_info_.total_size.width() &&
      width <= layout_size_info_.max_valid_width)
    return;

  layout_size_info_ = LayoutSizeInfo(width);
  layout_views_ = std::make_unique<LayoutViews>();

  const int content_width = width - insets.width();
  const int default_line_height = GetDefaultLineHeight();
  RangeStyleInfo default_style;
  default_style.text_style = default_text_style_;
  int max_width = 0, total_height = 0;

  // Try to preserve leading whitespace on the first line.
  bool can_trim_leading_whitespace = false;
  StyleRanges::const_iterator current_range = style_ranges_.begin();
  for (base::string16 remaining_string = text_;
       content_width > 0 && !remaining_string.empty();) {
    layout_size_info_.line_sizes.emplace_back(0, default_line_height);
    auto& line_size = layout_size_info_.line_sizes.back();
    layout_views_->views_per_line.emplace_back();
    auto& views = layout_views_->views_per_line.back();
    while (!remaining_string.empty()) {
      if (views.empty() && can_trim_leading_whitespace) {
        if (remaining_string.front() == '\n') {
          // Wrapped to the next line on \n, remove it. Other whitespace,
          // e.g. spaces to indent the next line, are preserved.
          remaining_string.erase(0, 1);
        } else {
          // Wrapped on whitespace character or characters in the middle of the
          // line - none of them are needed at the beginning of the next line.
          base::TrimWhitespace(remaining_string, base::TRIM_LEADING,
                               &remaining_string);
        }
      }

      gfx::Range range = gfx::Range::InvalidRange();
      if (current_range != style_ranges_.end())
        range = current_range->range;

      const size_t position = text_.size() - remaining_string.size();
      std::vector<base::string16> substrings;
      // If the current range is not a custom_view, then we use
      // ElideRectangleText() to determine the line wrapping. Note: if it is a
      // custom_view, then the |position| should equal range.start() because the
      // custom_view is treated as one unit.
      if (position != range.start() ||
          (current_range != style_ranges_.end() &&
           !current_range->style_info.custom_view)) {
        const gfx::Rect chunk_bounds(line_size.width(), 0,
                                     content_width - line_size.width(),
                                     default_line_height);
        // If the start of the remaining text is inside a styled range, the font
        // style may differ from the base font. The font specified by the range
        // should be used when eliding text.
        gfx::FontList text_font_list = position >= range.start()
                                           ? GetFontListForRange(current_range)
                                           : GetDefaultFontList();
        int elide_result = gfx::ElideRectangleText(
            remaining_string, text_font_list, chunk_bounds.width(),
            chunk_bounds.height(), gfx::WRAP_LONG_WORDS, &substrings);

        if (substrings.empty()) {
          // There is no room for anything. Since wrapping is enabled, this
          // should only occur if there is insufficient vertical space
          // remaining. ElideRectangleText() always adds a single character,
          // even if there is no room horizontally.
          DCHECK_NE(0, elide_result & gfx::INSUFFICIENT_SPACE_VERTICAL);
          // There's no way to continue processing; clear |remaining_string| so
          // the outer loop will terminate after this iteration completes.
          remaining_string.clear();
          break;
        }

        // Views are aligned to integer coordinates, but typesetting is not.
        // This means that it's possible for an ElideRectangleText on a prior
        // iteration to fit a word on the current line, which does not fit after
        // that word is wrapped in a View for its chunk at the end of the line.
        // In most cases, this will just wrap more words on to the next line.
        // However, if the remaining chunk width is insufficient for the very
        // _first_ word, that word will be incorrectly split. In this case,
        // start a new line instead.
        bool truncated_chunk =
            line_size.width() != 0 &&
            (elide_result & gfx::INSUFFICIENT_SPACE_FOR_FIRST_WORD) != 0;
        if (substrings[0].empty() || truncated_chunk) {
          // The entire line is \n, or nothing else fits on this line.  Wrap,
          // unless this is the first line, in which case we strip leading
          // whitespace and try again.
          if ((line_size.width() != 0) ||
              (layout_views_->views_per_line.size() > 1))
            break;
          can_trim_leading_whitespace = true;
          continue;
        }
      }

      base::string16 chunk;
      View* custom_view = nullptr;
      std::unique_ptr<Label> label;
      if (position >= range.start()) {
        const RangeStyleInfo& style_info = current_range->style_info;

        if (style_info.custom_view) {
          custom_view = style_info.custom_view;
          // Ownership of the custom view must be passed to StyledLabel.
          DCHECK(std::find_if(custom_views_.cbegin(), custom_views_.cend(),
                              [custom_view](const auto& view) {
                                return view.get() == custom_view;
                              }) != custom_views_.cend());
          // Do not allow wrap in custom view.
          DCHECK_EQ(position, range.start());
          chunk = remaining_string.substr(0, range.end() - position);
        } else {
          chunk = substrings[0];
        }

        if (((custom_view &&
              line_size.width() + custom_view->GetPreferredSize().width() >
                  content_width) ||
             (style_info.disable_line_wrapping &&
              chunk.size() < range.length())) &&
            position == range.start() && line_size.width() != 0) {
          // If the chunk should not be wrapped, try to fit it entirely on the
          // next line.
          break;
        }

        if (chunk.size() > range.end() - position)
          chunk = chunk.substr(0, range.end() - position);

        if (!custom_view)
          label = CreateLabel(chunk, style_info, range);

        if (position + chunk.size() >= range.end())
          ++current_range;
      } else {
        chunk = substrings[0];
        if (position + chunk.size() > range.start())
          chunk = chunk.substr(0, range.start() - position);

        // This chunk is normal text.
        label = CreateLabel(chunk, default_style, range);
      }

      View* child_view = custom_view ? custom_view : label.get();
      const gfx::Size child_size = child_view->GetPreferredSize();
      // A custom view could be wider than the available width.
      line_size.SetSize(
          std::min(line_size.width() + child_size.width(), content_width),
          std::max(line_size.height(), child_size.height()));

      views.push_back(child_view);
      if (label)
        layout_views_->owned_views.push_back(std::move(label));

      remaining_string = remaining_string.substr(chunk.size());

      // If |gfx::ElideRectangleText| returned more than one substring, that
      // means the whole text did not fit into remaining line width, with text
      // after |susbtring[0]| spilling into next line. If whole |substring[0]|
      // was added to the current line (this may not be the case if part of the
      // substring has different style), proceed to the next line.
      if (!custom_view && substrings.size() > 1 &&
          chunk.size() == substrings[0].size()) {
        break;
      }
    }

    if (views.empty() && remaining_string.empty()) {
      // Remove an empty last line.
      layout_size_info_.line_sizes.pop_back();
      layout_views_->views_per_line.pop_back();
    } else {
      max_width = std::max(max_width, line_size.width());
      total_height += line_size.height();

      // Trim whitespace at the start of the next line.
      can_trim_leading_whitespace = true;
    }
  }

  layout_size_info_.total_size.SetSize(max_width + insets.width(),
                                       total_height + insets.height());
}

std::unique_ptr<Label> StyledLabel::CreateLabel(
    const base::string16& text,
    const RangeStyleInfo& style_info,
    const gfx::Range& range) const {
  std::unique_ptr<Label> result;
  if (style_info.IsLink()) {
    // Nothing should (and nothing does) use a custom font for links.
    DCHECK(!style_info.custom_font);

    // Note this ignores |default_text_style_|, in favor of style::STYLE_LINK.
    auto link = std::make_unique<Link>(text, text_context_);

    // Links in a StyledLabel do not get underlines.
    link->SetUnderline(false);

    layout_views_->link_targets[link.get()] = range;

    result = std::move(link);
  } else if (style_info.custom_font) {
    result = std::make_unique<Label>(
        text, Label::CustomFont{style_info.custom_font.value()});
  } else {
    result = std::make_unique<Label>(
        text, text_context_,
        style_info.text_style.value_or(default_text_style_));
  }

  if (style_info.override_color != SK_ColorTRANSPARENT)
    result->SetEnabledColor(style_info.override_color);
  if (!style_info.tooltip.empty())
    result->SetTooltipText(style_info.tooltip);
  if (displayed_on_background_color_set_)
    result->SetBackgroundColor(displayed_on_background_color_);
  result->SetAutoColorReadabilityEnabled(auto_color_readability_enabled_);

  return result;
}

BEGIN_METADATA(StyledLabel)
ADD_PROPERTY_METADATA(StyledLabel, base::string16, Text)
ADD_PROPERTY_METADATA(StyledLabel, int, TextContext)
ADD_PROPERTY_METADATA(StyledLabel, int, DefaultTextStyle)
ADD_PROPERTY_METADATA(StyledLabel, int, LineHeight)
ADD_PROPERTY_METADATA(StyledLabel, bool, AutoColorReadabilityEnabled)
ADD_PROPERTY_METADATA(StyledLabel, SkColor, DisplayedOnBackgroundColor)
METADATA_PARENT_CLASS(View)
END_METADATA()

}  // namespace views
