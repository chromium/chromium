// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/styled_label.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_fragment.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace views {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kStyledLabelCustomViewKey, false)

StyledLabel::RangeStyleInfo::RangeStyleInfo() = default;
StyledLabel::RangeStyleInfo::RangeStyleInfo(const RangeStyleInfo&) = default;
StyledLabel::RangeStyleInfo& StyledLabel::RangeStyleInfo::operator=(
    const RangeStyleInfo&) = default;
StyledLabel::RangeStyleInfo::~RangeStyleInfo() = default;

// static
StyledLabel::RangeStyleInfo StyledLabel::RangeStyleInfo::CreateForLink(
    base::RepeatingClosure callback) {
  // Adapt this closure to a Link::ClickedCallback by discarding the extra arg.
  return CreateForLink(base::BindRepeating(
      [](base::RepeatingClosure closure, const ui::Event&) { closure.Run(); },
      std::move(callback)));
}

// static
StyledLabel::RangeStyleInfo StyledLabel::RangeStyleInfo::CreateForLink(
    Link::ClickedCallback callback) {
  RangeStyleInfo result;
  result.callback = std::move(callback);
  result.text_style = style::STYLE_LINK;
  return result;
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
  // All views to be added as children, line by line.
  std::vector<std::vector<raw_ptr<View, VectorExperimental>>> views_per_line;

  // The subset of |views| that are created by StyledLabel itself.  Basically,
  // this is all non-custom views;  These appear in the same order as |views|.
  std::vector<std::unique_ptr<View>> owned_views;
};

StyledLabel::StyledLabel() {
  GetViewAccessibility().SetRole(text_context_ == style::CONTEXT_DIALOG_TITLE
                                     ? ax::mojom::Role::kTitleBar
                                     : ax::mojom::Role::kStaticText);
}

StyledLabel::~StyledLabel() = default;

const std::u16string& StyledLabel::GetText() const {
  return text_;
}

void StyledLabel::SetText(std::u16string text) {
  // Failing to trim trailing whitespace will cause later confusion when the
  // text elider tries to do so internally. There's no obvious reason to
  // preserve trailing whitespace anyway.
  base::TrimWhitespace(std::move(text), base::TRIM_TRAILING, &text);
  if (text_ == text) {
    return;
  }

  text_ = text;
  GetViewAccessibility().SetName(text_);
  style_ranges_.clear();
  RemoveOrDeleteAllChildViews();
  OnPropertyChanged(&text_, kPropertyEffectsPreferredSizeChanged);
}

gfx::FontList StyledLabel::GetFontList(const RangeStyleInfo& style_info) const {
  return style_info.custom_font.value_or(TypographyProvider::Get().GetFont(
      text_context_, style_info.text_style.value_or(default_text_style_)));
}

void StyledLabel::AddStyleRange(const gfx::Range& range,
                                const RangeStyleInfo& style_info) {
  DCHECK(!range.is_reversed());
  DCHECK(!range.is_empty());
  DCHECK(gfx::Range(0, text_.size()).Contains(range));

  // Insert the new range in sorted order.
  StyleRanges new_range;
  new_range.emplace_front(range, style_info);
  style_ranges_.merge(new_range);

  PreferredSizeChanged();
}

void StyledLabel::AddCustomView(std::unique_ptr<View> custom_view) {
  DCHECK(!custom_view->owned_by_client());
  custom_view->SetProperty(kStyledLabelCustomViewKey, true);
  custom_views_.push_back(std::move(custom_view));
}

int StyledLabel::GetTextContext() const {
  return text_context_;
}

void StyledLabel::SetTextContext(int text_context) {
  if (text_context_ == text_context) {
    return;
  }

  text_context_ = text_context;
  GetViewAccessibility().SetRole(text_context_ == style::CONTEXT_DIALOG_TITLE
                                     ? ax::mojom::Role::kTitleBar
                                     : ax::mojom::Role::kStaticText);
  OnPropertyChanged(&text_context_, kPropertyEffectsPreferredSizeChanged);
}

int StyledLabel::GetDefaultTextStyle() const {
  return default_text_style_;
}

void StyledLabel::SetDefaultTextStyle(int text_style) {
  if (default_text_style_ == text_style) {
    return;
  }

  default_text_style_ = text_style;
  OnPropertyChanged(&default_text_style_, kPropertyEffectsPreferredSizeChanged);
}

std::optional<ui::ColorId> StyledLabel::GetDefaultEnabledColorId() const {
  return default_enabled_color_id_;
}

void StyledLabel::SetDefaultEnabledColorId(
    std::optional<ui::ColorId> enabled_color_id) {
  if (default_enabled_color_id_ == enabled_color_id) {
    return;
  }

  default_enabled_color_id_ = enabled_color_id;
  OnPropertyChanged(&default_enabled_color_id_, kPropertyEffectsPaint);
}

int StyledLabel::GetLineHeight() const {
  return line_height_.value_or(TypographyProvider::Get().GetLineHeight(
      text_context_, default_text_style_));
}

void StyledLabel::SetLineHeight(int line_height) {
  if (line_height_ == line_height) {
    return;
  }

  line_height_ = line_height;
  OnPropertyChanged(&line_height_, kPropertyEffectsPreferredSizeChanged);
}

StyledLabel::ColorVariant StyledLabel::GetDisplayedOnBackgroundColor() const {
  return displayed_on_background_color_;
}

void StyledLabel::SetDisplayedOnBackgroundColor(ColorVariant color) {
  if (color == displayed_on_background_color_) {
    return;
  }

  displayed_on_background_color_ = color;

  if (GetWidget()) {
    UpdateLabelBackgroundColor();
  }

  OnPropertyChanged(&displayed_on_background_color_, kPropertyEffectsPaint);
}

bool StyledLabel::GetAutoColorReadabilityEnabled() const {
  return auto_color_readability_enabled_;
}

void StyledLabel::SetAutoColorReadabilityEnabled(bool auto_color_readability) {
  if (auto_color_readability_enabled_ == auto_color_readability) {
    return;
  }

  auto_color_readability_enabled_ = auto_color_readability;
  OnPropertyChanged(&auto_color_readability_enabled_, kPropertyEffectsPaint);
}

bool StyledLabel::GetSubpixelRenderingEnabled() const {
  return subpixel_rendering_enabled_;
}

void StyledLabel::SetSubpixelRenderingEnabled(bool subpixel_rendering_enabled) {
  if (subpixel_rendering_enabled_ == subpixel_rendering_enabled) {
    return;
  }

  subpixel_rendering_enabled_ = subpixel_rendering_enabled;
  OnPropertyChanged(&subpixel_rendering_enabled_, kPropertyEffectsPaint);
}

const StyledLabel::LayoutSizeInfo& StyledLabel::GetLayoutSizeInfoForWidth(
    int w) const {
  if (auto it = layout_size_info_cache_.Get(w);
      it != layout_size_info_cache_.end()) {
    return it->second;
  }
  CalculateLayout(w);
  layout_size_info_cache_.Put(w, layout_size_info_);
  return layout_size_info_;
}

void StyledLabel::SizeToFit(int fixed_width) {
  DCHECK_LE(0, fixed_width);
  fixed_width_ = fixed_width;
  gfx::Size size = CalculatePreferredSize(
      SizeBounds(fixed_width_ == 0 ? SizeBound() : SizeBound(width()), {}));
  size.set_width(std::max(size.width(), fixed_width));
  SetSize(size);
}

base::CallbackListSubscription StyledLabel::AddTextChangedCallback(
    views::PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(&text_, std::move(callback));
}

gfx::Size StyledLabel::GetMinimumSize() const {
  // Overload it otherwise BubbleDialogDelegateViewTest.StyledLabelTitle will
  // fail.
  return CalculatePreferredSize(
      SizeBounds(width() == 0 ? SizeBound() : SizeBound(width()), {}));
}

gfx::Size StyledLabel::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  int width = 0;
  if (fixed_width_) {
    width = fixed_width_;
  } else if (available_size.width().is_bounded()) {
    width = available_size.width().value();
  }

  // Respect any existing size.  If there is none, default to a single line.
  return GetLayoutSizeInfoForWidth(width == 0 ? std::numeric_limits<int>::max()
                                              : width)
      .total_size;
}

void StyledLabel::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (previous_bounds.width() == width()) {
    return;
  }

  need_recreate_child_ = true;
}

void StyledLabel::Layout(PassKey) {
  if (!need_recreate_child_) {
    return;
  }

  RecreateChildViews();
}

void StyledLabel::PreferredSizeChanged() {
  need_recreate_child_ = true;
  layout_size_info_ = LayoutSizeInfo(0);
  layout_size_info_cache_.Clear();
  layout_views_.reset();
  View::PreferredSizeChanged();
}

// TODO(wutao): support gfx::ALIGN_TO_HEAD alignment.
void StyledLabel::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  DCHECK_NE(gfx::ALIGN_TO_HEAD, alignment);
  alignment = gfx::MaybeFlipForRTL(alignment);

  if (horizontal_alignment_ == alignment) {
    return;
  }
  horizontal_alignment_ = alignment;
  PreferredSizeChanged();
}

void StyledLabel::ClearStyleRanges() {
  style_ranges_.clear();
  PreferredSizeChanged();
}

void StyledLabel::ClickFirstLinkForTesting() {
  GetFirstLinkForTesting()->OnKeyPressed(  // IN-TEST
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_SPACE, ui::EF_NONE));
}

views::Link* StyledLabel::GetFirstLinkForTesting() {
  const auto it = base::ranges::find_if(children(), &IsViewClass<LinkFragment>);
  return (it == children().cend()) ? nullptr : static_cast<views::Link*>(*it);
}

int StyledLabel::StartX(int excess_space) const {
  int x = GetInsets().left();
  // If the element should be aligned to the leading side (left in LTR, or right
  // in RTL), position it at the leading side Insets (left).
  if (horizontal_alignment_ ==
      (base::i18n::IsRTL() ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT)) {
    return x;
  }
  return x + (horizontal_alignment_ == gfx::ALIGN_CENTER ? (excess_space / 2)
                                                         : excess_space);
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
  const int line_height = GetLineHeight();
  RangeStyleInfo default_style;
  default_style.text_style = default_text_style_;
  int max_width = 0, total_height = 0;

  // Try to preserve leading whitespace on the first line.
  bool can_trim_leading_whitespace = false;
  StyleRanges::const_iterator current_range = style_ranges_.begin();

  // A pointer to the previous link fragment if a logical link consists of
  // multiple `LinkFragment` elements.
  LinkFragment* previous_link_fragment = nullptr;

  for (std::u16string remaining_string = text_;
       content_width > 0 && !remaining_string.empty();) {
    layout_size_info_.line_sizes.emplace_back(0, line_height);
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
      if (current_range != style_ranges_.end()) {
        range = current_range->range;
      }

      const size_t position = text_.size() - remaining_string.size();
      std::vector<std::u16string> substrings;
      // If the current range is not a custom_view, then we use
      // ElideRectangleText() to determine the line wrapping. Note: if it is a
      // custom_view, then the |position| should equal range.start() because the
      // custom_view is treated as one unit.
      if (position != range.start() ||
          (current_range != style_ranges_.end() &&
           !current_range->style_info.custom_view)) {
        const gfx::Rect chunk_bounds(line_size.width(), 0,
                                     content_width - line_size.width(),
                                     line_height);
        // If the start of the remaining text is inside a styled range, the font
        // style may differ from the base font. The font specified by the range
        // should be used when eliding text.
        gfx::FontList text_font_list =
            GetFontList((position >= range.start()) ? current_range->style_info
                                                    : RangeStyleInfo());
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

      std::u16string chunk;
      View* custom_view = nullptr;
      std::unique_ptr<Label> label;
      if (position >= range.start()) {
        const RangeStyleInfo& style_info = current_range->style_info;

        if (style_info.custom_view) {
          custom_view = style_info.custom_view;
          // Custom views must be marked as such.
          DCHECK(custom_view->GetProperty(kStyledLabelCustomViewKey));
          // Do not allow wrap in custom view.
          DCHECK_EQ(position, range.start());
          chunk = remaining_string.substr(0, range.end() - position);
        } else {
          chunk = substrings[0];
        }

        if (custom_view && position == range.start() &&
            line_size.width() != 0) {
          SizeBounds chunk_size(content_width - line_size.width(), {});
          int custom_view_width =
              custom_view->GetPreferredSize(chunk_size).width();
          if (line_size.width() + custom_view_width > content_width) {
            // If the chunk should not be wrapped, try to fit it entirely on the
            // next line.
            break;
          }
        }

        if (chunk.size() > range.end() - position)
          chunk = chunk.substr(0, range.end() - position);

        if (!custom_view) {
          label =
              CreateLabel(chunk, style_info, range, &previous_link_fragment);
        } else {
          previous_link_fragment = nullptr;
        }

        if (position + chunk.size() >= range.end()) {
          ++current_range;
          // Links do not connect across separate style ranges.
          previous_link_fragment = nullptr;
        }
      } else {
        chunk = substrings[0];
        if (position + chunk.size() > range.start())
          chunk = chunk.substr(0, range.start() - position);

        // This chunk is normal text.
        label =
            CreateLabel(chunk, default_style, range, &previous_link_fragment);
      }

      View* child_view = custom_view ? custom_view : label.get();
      const gfx::Size child_size = child_view->GetPreferredSize(
          SizeBounds(content_width - line_size.width(), {}));
      // A custom view could be wider than the available width.
      line_size.SetSize(
          std::min(line_size.width() + child_size.width(), content_width),
          std::max(line_size.height(), child_size.height()));

      views.push_back(child_view);
      if (label) {
        layout_views_->owned_views.push_back(std::move(label));
      }

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
    const std::u16string& text,
    const RangeStyleInfo& style_info,
    const gfx::Range& range,
    LinkFragment** previous_link_fragment) const {
  std::unique_ptr<Label> result;
  if (style_info.text_style == style::STYLE_LINK ||
      style_info.text_style == style::STYLE_LINK_3 ||
      style_info.text_style == style::STYLE_LINK_5) {
    // Nothing should (and nothing does) use a custom font for links.
    DCHECK(!style_info.custom_font);

    // Note this ignores |default_text_style_|, in favor of `style::STYLE_LINK`.
    auto link = std::make_unique<LinkFragment>(
        text, text_context_, *style_info.text_style, *previous_link_fragment);
    *previous_link_fragment = link.get();
    link->SetCallback(style_info.callback);
    if (!style_info.accessible_name.empty())
      link->GetViewAccessibility().SetName(style_info.accessible_name);

    result = std::move(link);
  } else if (style_info.custom_font) {
    result = std::make_unique<Label>(
        text, Label::CustomFont{style_info.custom_font.value()});
  } else {
    result = std::make_unique<Label>(
        text, text_context_,
        style_info.text_style.value_or(default_text_style_));
  }

  if (style_info.override_color_id) {
    result->SetEnabledColorId(style_info.override_color_id.value());
  } else if (style_info.override_color) {
    result->SetEnabledColor(style_info.override_color.value());
  } else if (default_enabled_color_id_) {
    result->SetEnabledColorId(default_enabled_color_id_);
  }
  if (!style_info.tooltip.empty()) {
    result->SetTooltipText(style_info.tooltip);
  }
  if (!style_info.accessible_name.empty())
    result->GetViewAccessibility().SetName(style_info.accessible_name);
  if (absl::holds_alternative<SkColor>(displayed_on_background_color_)) {
    result->SetBackgroundColor(
        absl::get<SkColor>(displayed_on_background_color_));
  } else if (absl::holds_alternative<ui::ColorId>(
                 displayed_on_background_color_)) {
    result->SetBackgroundColorId(
        absl::get<ui::ColorId>(displayed_on_background_color_));
  }
  result->SetAutoColorReadabilityEnabled(auto_color_readability_enabled_);
  result->SetSubpixelRenderingEnabled(subpixel_rendering_enabled_);
  return result;
}

void StyledLabel::UpdateLabelBackgroundColor() {
  for (View* child : children()) {
    if (!child->GetProperty(kStyledLabelCustomViewKey)) {
      // TODO(kylixrd): Should updating the label background color even be
      // allowed if there are custom views?
      DCHECK(IsViewClass<Label>(child) || IsViewClass<LinkFragment>(child));
      static_cast<Label*>(child)->SetBackgroundColorId(
          absl::holds_alternative<ui::ColorId>(displayed_on_background_color_)
              ? std::optional<ui::ColorId>(
                    absl::get<ui::ColorId>(displayed_on_background_color_))
              : std::nullopt);
      if (absl::holds_alternative<SkColor>(displayed_on_background_color_)) {
        static_cast<Label*>(child)->SetBackgroundColor(
            absl::get<SkColor>(displayed_on_background_color_));
      }
    }
  }
}

void StyledLabel::RemoveOrDeleteAllChildViews() {
  pending_delete_views_.clear();
  while (children().size() > 0) {
    std::unique_ptr<View> view = RemoveChildViewT(children()[0]);
    if (view->GetProperty(kStyledLabelCustomViewKey)) {
      custom_views_.push_back(std::move(view));
    } else {
      pending_delete_views_.push_back(std::move(view));
    }
  }
}

void StyledLabel::RecreateChildViews() {
  need_recreate_child_ = false;

  CalculateLayout(width());

  // If the layout has been recalculated, add and position all views.
  if (layout_views_) {
    // Delete all non-custom views on removal; custom views are temporarily
    // moved to |custom_views_|.
    RemoveOrDeleteAllChildViews();

    DCHECK_EQ(layout_size_info_.line_sizes.size(),
              layout_views_->views_per_line.size());
    int line_y = GetInsets().top();
    auto next_owned_view = layout_views_->owned_views.begin();
    for (size_t line = 0; line < layout_views_->views_per_line.size(); ++line) {
      const auto& line_size = layout_size_info_.line_sizes[line];
      int x = StartX(width() - line_size.width());
      for (views::View* view : layout_views_->views_per_line[line]) {
        gfx::Size size = view->GetPreferredSize(SizeBounds(line_size));
        size.set_width(std::min(size.width(), width() - x));
        // Compute the view y such that the view center y and the line center y
        // match.  Because of added rounding errors, this is not the same as
        // doing (line_size.height() - size.height()) / 2.
        const int y = line_size.height() / 2 - size.height() / 2;
        view->SetBoundsRect({{x, line_y + y}, size});
        x += size.width();

        // Transfer ownership for any views in layout_views_->owned_views or
        // custom_views_.  The actual pointer is the same in both arms below.
        if (view->GetProperty(kStyledLabelCustomViewKey)) {
          auto custom_view = base::ranges::find(custom_views_, view,
                                                &std::unique_ptr<View>::get);
          DCHECK(custom_view != custom_views_.end());
          AddChildView(std::move(*custom_view));
          custom_views_.erase(custom_view);
        } else {
          DCHECK(next_owned_view != layout_views_->owned_views.end());
          DCHECK(view == next_owned_view->get());
          AddChildView(std::move(*next_owned_view));
          ++next_owned_view;
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
        x += (*i)->GetPreferredSize(SizeBounds(line_size)).width();
      }
    }
    DCHECK(i == children().end());  // Should not be short any lines.
  }
}

BEGIN_METADATA(StyledLabel)
ADD_PROPERTY_METADATA(std::u16string, Text)
ADD_PROPERTY_METADATA(int, TextContext)
ADD_PROPERTY_METADATA(int, DefaultTextStyle)
ADD_PROPERTY_METADATA(int, LineHeight)
ADD_PROPERTY_METADATA(bool, AutoColorReadabilityEnabled)
ADD_PROPERTY_METADATA(StyledLabel::ColorVariant, DisplayedOnBackgroundColor)
ADD_PROPERTY_METADATA(std::optional<ui::ColorId>, DefaultEnabledColorId)
END_METADATA

}  // namespace views
