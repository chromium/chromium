// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/label.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/default_style.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessibility/views_utilities_aura.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/selection_controller.h"
#include "ui/views/style/typography_provider.h"

namespace {

// An enum giving different RenderText properties unique keys for the
// OnPropertyChanged call.
enum LabelPropertyKey {
  kLabelText = 1,
  kLabelShadows,
  kLabelHorizontalAlignment,
  kLabelVerticalAlignment,
  kLabelLineHeight,
  kLabelObscured,
  kLabelAllowCharacterBreak,
};

bool IsOpaque(SkColor color) {
  return SkColorGetA(color) == SK_AlphaOPAQUE;
}

}  // namespace

namespace views {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(CascadingProperty<SkColor>,
                                   kCascadingLabelEnabledColor,
                                   nullptr)

Label::Label() : Label(std::u16string()) {}

Label::Label(const std::u16string& text)
    : Label(text, style::CONTEXT_LABEL, style::STYLE_PRIMARY) {}

Label::Label(const std::u16string& text,
             int text_context,
             int text_style,
             gfx::DirectionalityMode directionality_mode)
    : text_context_(text_context),
      text_style_(text_style),
      context_menu_contents_(this) {
  Init(text, TypographyProvider::Get().GetFont(text_context, text_style),
       directionality_mode);
}

Label::Label(const std::u16string& text, const CustomFont& font)
    : text_context_(style::CONTEXT_LABEL),
      text_style_(style::STYLE_PRIMARY),
      context_menu_contents_(this) {
  Init(text, font.font_list, gfx::DirectionalityMode::DIRECTIONALITY_FROM_TEXT);
}

Label::~Label() = default;

// static
const gfx::FontList& Label::GetDefaultFontList() {
  return TypographyProvider::Get().GetFont(style::CONTEXT_LABEL,
                                           style::STYLE_PRIMARY);
}

void Label::SetFontList(const gfx::FontList& font_list) {
  full_text_->SetFontList(font_list);
  ClearDisplayText();
  PreferredSizeChanged();
}

const std::u16string& Label::GetText() const {
  return full_text_->text();
}

void Label::SetText(const std::u16string& new_text) {
  if (new_text == GetText())
    return;

  std::u16string current_text = GetText();
  full_text_->SetText(new_text);
  ClearDisplayText();

  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&full_text_, kLabelText),
      kPropertyEffectsPreferredSizeChanged);

  // The accessibility updates will cause the display text to be rebuilt and the
  // `stored_selection_range_` to be reapplied. Ensure that we cleared it before
  // running the accessibility updates.
  stored_selection_range_ = gfx::Range::InvalidRange();
  if (GetViewAccessibility().GetCachedName().empty() ||
      GetViewAccessibility().GetCachedName() == current_text) {
    if (new_text.empty()) {
      GetViewAccessibility().SetName(
          new_text, ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    } else {
      GetViewAccessibility().SetName(new_text);
    }
  }
}

void Label::AdjustAccessibleName(std::u16string& new_name,
                                 ax::mojom::NameFrom& name_from) {
  if (new_name.empty()) {
    new_name = full_text_->GetDisplayText();
  }
}

int Label::GetTextContext() const {
  return text_context_;
}

void Label::SetTextContext(int text_context) {
  if (text_context == text_context_)
    return;
  text_context_ = text_context;
  full_text_->SetFontList(
      TypographyProvider::Get().GetFont(text_context_, text_style_));
  full_text_->SetMinLineHeight(GetLineHeight());
  ClearDisplayText();
  if (GetWidget())
    UpdateColorsFromTheme();

  GetViewAccessibility().SetRole(text_context_ == style::CONTEXT_DIALOG_TITLE
                                     ? ax::mojom::Role::kTitleBar
                                     : ax::mojom::Role::kStaticText);

  OnPropertyChanged(&text_context_, kPropertyEffectsPreferredSizeChanged);
}

int Label::GetTextStyle() const {
  return text_style_;
}

void Label::SetTextStyle(int style) {
  if (style == text_style_)
    return;

  text_style_ = style;
  ApplyBaselineTextStyle();
}

void Label::ApplyBaselineTextStyle() {
  full_text_->SetFontList(
      TypographyProvider::Get().GetFont(text_context_, text_style_));
  full_text_->SetMinLineHeight(GetLineHeight());
  ClearDisplayText();
  if (GetWidget())
    UpdateColorsFromTheme();
  OnPropertyChanged(&text_style_, kPropertyEffectsPreferredSizeChanged);
}

void Label::SetTextStyleRange(int style, const gfx::Range& range) {
  if (style == text_style_ || !range.IsValid() || range.is_empty() ||
      !gfx::Range(0, GetText().size()).Contains(range)) {
    return;
  }

  const auto& typography_provider = TypographyProvider::Get();
  const auto details = typography_provider.GetFontDetails(text_context_, style);
  // This function is not prepared to handle style requests that vary by
  // anything other than weight.
  DCHECK_EQ(
      details.typeface,
      typography_provider.GetFontDetails(text_context_, text_style_).typeface);
  DCHECK_EQ(details.size_delta,
            typography_provider.GetFontDetails(text_context_, text_style_)
                .size_delta);
  full_text_->ApplyWeight(details.weight, range);
  ClearDisplayText();
  PreferredSizeChanged();
}

bool Label::GetAutoColorReadabilityEnabled() const {
  return auto_color_readability_enabled_;
}

void Label::SetAutoColorReadabilityEnabled(
    bool auto_color_readability_enabled) {
  if (auto_color_readability_enabled_ == auto_color_readability_enabled)
    return;
  auto_color_readability_enabled_ = auto_color_readability_enabled;
  RecalculateColors();
  OnPropertyChanged(&auto_color_readability_enabled_, kPropertyEffectsPaint);
}

SkColor Label::GetEnabledColor() const {
  return actual_enabled_color_;
}

void Label::SetEnabledColor(SkColor color) {
  if (enabled_color_set_ && requested_enabled_color_ == color)
    return;

  enabled_color_set_ = true;
  requested_enabled_color_ = color;
  enabled_color_id_.reset();
  RecalculateColors();
  OnPropertyChanged(&requested_enabled_color_, kPropertyEffectsPaint);
}

std::optional<ui::ColorId> Label::GetEnabledColorId() const {
  return enabled_color_id_;
}

void Label::SetEnabledColorId(std::optional<ui::ColorId> enabled_color_id) {
  if (enabled_color_id_ == enabled_color_id)
    return;

  enabled_color_id_ = enabled_color_id;
  if (GetWidget()) {
    UpdateColorsFromTheme();
    enabled_color_set_ = true;
  }
  OnPropertyChanged(&enabled_color_id_, kPropertyEffectsPaint);
}

SkColor Label::GetBackgroundColor() const {
  return background_color_;
}

void Label::SetBackgroundColor(SkColor color) {
  if (background_color_set_ && background_color_ == color)
    return;
  background_color_ = color;
  background_color_set_ = true;
  if (GetWidget()) {
    UpdateColorsFromTheme();
  } else {
    RecalculateColors();
  }
  OnPropertyChanged(&background_color_, kPropertyEffectsPaint);
}

void Label::SetBackgroundColorId(
    std::optional<ui::ColorId> background_color_id) {
  if (background_color_id_ == background_color_id)
    return;

  background_color_id_ = background_color_id;
  if (GetWidget()) {
    UpdateColorsFromTheme();
  }
  OnPropertyChanged(&background_color_id_, kPropertyEffectsPaint);
}

SkColor Label::GetSelectionTextColor() const {
  return actual_selection_text_color_;
}

void Label::SetSelectionTextColor(SkColor color) {
  if (selection_text_color_set_ && requested_selection_text_color_ == color)
    return;
  requested_selection_text_color_ = color;
  selection_text_color_set_ = true;
  RecalculateColors();
  OnPropertyChanged(&requested_selection_text_color_, kPropertyEffectsPaint);
}

SkColor Label::GetSelectionBackgroundColor() const {
  return selection_background_color_;
}

void Label::SetSelectionBackgroundColor(SkColor color) {
  if (selection_background_color_set_ && selection_background_color_ == color)
    return;
  selection_background_color_ = color;
  selection_background_color_set_ = true;
  RecalculateColors();
  OnPropertyChanged(&selection_background_color_, kPropertyEffectsPaint);
}

const gfx::ShadowValues& Label::GetShadows() const {
  return full_text_->shadows();
}

void Label::SetShadows(const gfx::ShadowValues& shadows) {
  if (full_text_->shadows() == shadows)
    return;
  full_text_->set_shadows(shadows);
  ClearDisplayText();
  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&full_text_, kLabelShadows),
      kPropertyEffectsPreferredSizeChanged);
}

bool Label::GetSubpixelRenderingEnabled() const {
  return subpixel_rendering_enabled_;
}

void Label::SetSubpixelRenderingEnabled(bool subpixel_rendering_enabled) {
  if (subpixel_rendering_enabled_ == subpixel_rendering_enabled)
    return;
  subpixel_rendering_enabled_ = subpixel_rendering_enabled;
  ApplyTextColors();
  OnPropertyChanged(&subpixel_rendering_enabled_, kPropertyEffectsPaint);
}

bool Label::GetSkipSubpixelRenderingOpacityCheck() const {
  return skip_subpixel_rendering_opacity_check_;
}

void Label::SetSkipSubpixelRenderingOpacityCheck(
    bool skip_subpixel_rendering_opacity_check) {
  if (skip_subpixel_rendering_opacity_check_ ==
      skip_subpixel_rendering_opacity_check) {
    return;
  }
  skip_subpixel_rendering_opacity_check_ =
      skip_subpixel_rendering_opacity_check;
  OnPropertyChanged(&skip_subpixel_rendering_opacity_check_,
                    kPropertyEffectsNone);
}

gfx::HorizontalAlignment Label::GetHorizontalAlignment() const {
  return full_text_->horizontal_alignment();
}

void Label::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  alignment = gfx::MaybeFlipForRTL(alignment);
  if (GetHorizontalAlignment() == alignment)
    return;
  full_text_->SetHorizontalAlignment(alignment);
  ClearDisplayText();
  OnPropertyChanged(ui::metadata::MakeUniquePropertyKey(
                        &full_text_, kLabelHorizontalAlignment),
                    kPropertyEffectsPaint);
}

gfx::VerticalAlignment Label::GetVerticalAlignment() const {
  return full_text_->vertical_alignment();
}

void Label::SetVerticalAlignment(gfx::VerticalAlignment alignment) {
  if (GetVerticalAlignment() == alignment)
    return;
  full_text_->SetVerticalAlignment(alignment);
  ClearDisplayText();
  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&full_text_, kLabelVerticalAlignment),
      kPropertyEffectsPaint);
}

int Label::GetLineHeight() const {
  // TODO(pkasting): If we can replace SetFontList() with context/style setter
  // calls, we can eliminate the reference to font_list().GetHeight() here.
  return line_height_.value_or(std::max(
      TypographyProvider::Get().GetLineHeight(text_context_, text_style_),
      font_list().GetHeight()));
}

void Label::SetLineHeight(int line_height) {
  if (line_height_ == line_height)
    return;
  line_height_ = line_height;
  full_text_->SetMinLineHeight(line_height);
  ClearDisplayText();
  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&full_text_, kLabelLineHeight),
      kPropertyEffectsPreferredSizeChanged);
}

bool Label::GetMultiLine() const {
  return multi_line_;
}

void Label::SetMultiLine(bool multi_line) {
  DCHECK(!multi_line || (elide_behavior_ == gfx::ELIDE_TAIL ||
                         elide_behavior_ == gfx::NO_ELIDE));
  if (this->GetMultiLine() == multi_line)
    return;
  multi_line_ = multi_line;
  // `max_width_` and `max_width_single_line_` are mutually exclusive.
  max_width_single_line_ = 0;
  full_text_->SetMultiline(multi_line);
  ClearDisplayText();
  OnPropertyChanged(&multi_line_, kPropertyEffectsPreferredSizeChanged);
}

size_t Label::GetMaxLines() const {
  return max_lines_;
}

void Label::SetMaxLines(size_t max_lines) {
  if (max_lines_ == max_lines)
    return;
  max_lines_ = max_lines;
  OnPropertyChanged(&max_lines_, kPropertyEffectsPreferredSizeChanged);
}

bool Label::GetObscured() const {
  return full_text_->obscured();
}

void Label::SetObscured(bool obscured) {
  if (this->GetObscured() == obscured)
    return;
  full_text_->SetObscured(obscured);
  ClearDisplayText();
  if (obscured)
    SetSelectable(false);
  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&full_text_, kLabelObscured),
      kPropertyEffectsPreferredSizeChanged);
}

bool Label::IsDisplayTextClipped() const {
  MaybeBuildDisplayText();
  if (!full_text_ || full_text_->text().empty())
    return false;
  auto text_bounds = GetTextBounds();
  return text_bounds.width() > GetContentsBounds().width() ||
         text_bounds.height() > GetContentsBounds().height();
}

bool Label::IsDisplayTextTruncated() const {
  MaybeBuildDisplayText();
  if (!full_text_ || full_text_->text().empty()) {
    return false;
  }
  return (display_text_ &&
          display_text_->text() != display_text_->GetDisplayText()) ||
         IsDisplayTextClipped();
}

bool Label::GetAllowCharacterBreak() const {
  return full_text_->word_wrap_behavior() == gfx::WRAP_LONG_WORDS;
}

void Label::SetAllowCharacterBreak(bool allow_character_break) {
  const gfx::WordWrapBehavior behavior =
      allow_character_break ? gfx::WRAP_LONG_WORDS : gfx::TRUNCATE_LONG_WORDS;
  if (full_text_->word_wrap_behavior() == behavior)
    return;
  full_text_->SetWordWrapBehavior(behavior);
  ClearDisplayText();
  OnPropertyChanged(ui::metadata::MakeUniquePropertyKey(
                        &full_text_, kLabelAllowCharacterBreak),
                    kPropertyEffectsPreferredSizeChanged);
}

size_t Label::GetTextIndexOfLine(size_t line) const {
  return full_text_->GetTextIndexOfLine(line);
}

void Label::SetTruncateLength(size_t truncate_length) {
  return full_text_->set_truncate_length(truncate_length);
}

gfx::ElideBehavior Label::GetElideBehavior() const {
  return elide_behavior_;
}

void Label::SetElideBehavior(gfx::ElideBehavior elide_behavior) {
  DCHECK(!GetMultiLine() || (elide_behavior == gfx::ELIDE_TAIL ||
                             elide_behavior == gfx::NO_ELIDE));
  if (elide_behavior_ == elide_behavior)
    return;
  elide_behavior_ = elide_behavior;
  UpdateFullTextElideBehavior();
  ClearDisplayText();
  OnPropertyChanged(&elide_behavior_, kPropertyEffectsPreferredSizeChanged);
}

std::u16string Label::GetTooltipText() const {
  return tooltip_text_;
}

void Label::SetTooltipText(const std::u16string& tooltip_text) {
  DCHECK(handles_tooltips_);
  if (tooltip_text_ == tooltip_text)
    return;
  tooltip_text_ = tooltip_text;
  TooltipTextChanged();
  OnPropertyChanged(&tooltip_text_, kPropertyEffectsNone);
}

bool Label::GetHandlesTooltips() const {
  return handles_tooltips_;
}

void Label::SetHandlesTooltips(bool enabled) {
  if (handles_tooltips_ == enabled)
    return;
  handles_tooltips_ = enabled;
  OnPropertyChanged(&handles_tooltips_, kPropertyEffectsNone);
}

int Label::GetFixedWidth() const {
  return fixed_width_;
}

void Label::SizeToFit(int fixed_width) {
  DCHECK(GetMultiLine());
  DCHECK_EQ(0, max_width_);
  fixed_width_ = fixed_width;
  SizeToPreferredSize();
}

int Label::GetMaximumWidth() const {
  return max_width_;
}

void Label::SetMaximumWidth(int max_width) {
  DCHECK(GetMultiLine());
  DCHECK_EQ(0, fixed_width_);
  if (max_width_ == max_width)
    return;
  max_width_ = max_width;
  OnPropertyChanged(&max_width_, kPropertyEffectsPreferredSizeChanged);
}

void Label::SetMaximumWidthSingleLine(int max_width) {
  DCHECK(!GetMultiLine());
  if (max_width_single_line_ == max_width)
    return;
  max_width_single_line_ = max_width;
  UpdateFullTextElideBehavior();
  OnPropertyChanged(&max_width_single_line_,
                    kPropertyEffectsPreferredSizeChanged);
}

bool Label::GetCollapseWhenHidden() const {
  return collapse_when_hidden_;
}

void Label::SetCollapseWhenHidden(bool value) {
  if (collapse_when_hidden_ == value)
    return;
  collapse_when_hidden_ = value;
  OnPropertyChanged(&collapse_when_hidden_,
                    kPropertyEffectsPreferredSizeChanged);
}

size_t Label::GetRequiredLines() const {
  return full_text_->GetNumLines();
}

const std::u16string Label::GetDisplayTextForTesting() const {
  MaybeBuildDisplayText();
  return display_text_ ? display_text_->GetDisplayText() : std::u16string();
}

base::i18n::TextDirection Label::GetTextDirectionForTesting() {
  return full_text_->GetDisplayTextDirection();
}

bool Label::IsSelectionSupported() const {
  return !GetObscured();
}

bool Label::GetSelectable() const {
  return !!selection_controller_;
}

bool Label::SetSelectable(bool value) {
  if (value == GetSelectable())
    return true;

  if (!value) {
    ClearSelection();
    stored_selection_range_ = gfx::Range::InvalidRange();
    selection_controller_.reset();
    return true;
  }

  DCHECK(!stored_selection_range_.IsValid());
  if (!IsSelectionSupported())
    return false;

  selection_controller_ = std::make_unique<SelectionController>(this);
  return true;
}

bool Label::HasSelection() const {
  const gfx::RenderText* render_text = GetRenderTextForSelectionController();
  return render_text ? !render_text->selection().is_empty() : false;
}

bool Label::HasFullSelection() const {
  const gfx::RenderText* render_text = GetRenderTextForSelectionController();
  return render_text
             ? render_text->selection().length() == render_text->text().length()
             : false;
}

void Label::SelectAll() {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (!render_text)
    return;
  render_text->SelectAll(false);
  SchedulePaint();
}

void Label::ClearSelection() {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (!render_text)
    return;
  render_text->ClearSelection();
  SchedulePaint();
}

void Label::SelectRange(const gfx::Range& range) {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (render_text && render_text->SelectRange(range))
    SchedulePaint();
}

std::vector<gfx::Rect> Label::GetSubstringBounds(const gfx::Range& range) {
  auto substring_bounds = full_text_->GetSubstringBounds(range);
  for (auto& bound : substring_bounds) {
    bound.Offset(GetInsets().left(), GetInsets().top());
  }
  return substring_bounds;
}

base::CallbackListSubscription Label::AddTextChangedCallback(
    views::PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(
      ui::metadata::MakeUniquePropertyKey(&full_text_, kLabelText),
      std::move(callback));
}

base::CallbackListSubscription Label::AddTextContextChangedCallback(
    views::PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(&text_context_, std::move(callback));
}

int Label::GetBaseline() const {
  return GetInsets().top() + font_list().GetBaseline();
}

gfx::Size Label::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  // Return a size of (0, 0) if the label is not visible and if the
  // |collapse_when_hidden_| flag is set.
  // TODO(munjal): This logic probably belongs to the View class. But for now,
  // put it here since putting it in View class means all inheriting classes
  // need to respect the |collapse_when_hidden_| flag.
  if (!GetVisible() && collapse_when_hidden_) {
    return gfx::Size();
  }

  if (GetMultiLine() && fixed_width_ != 0 && !GetText().empty()) {
    return gfx::Size(fixed_width_, GetLabelHeightForWidth(fixed_width_));
  }

  gfx::Size size(GetBoundedTextSize(available_size));
  const gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());

  if (GetMultiLine() && max_width_ != 0 && max_width_ < size.width()) {
    return gfx::Size(max_width_, GetLabelHeightForWidth(max_width_));
  }

  if (GetMultiLine() && GetMaxLines() > 0) {
    return gfx::Size(size.width(), GetLabelHeightForWidth(size.width()));
  }
  return size;
}

gfx::Size Label::GetMinimumSize() const {
  if (!GetVisible() && collapse_when_hidden_)
    return gfx::Size();

  // Always reserve vertical space for at least one line.
  gfx::Size size(0, GetLineHeight());
  if (elide_behavior_ == gfx::ELIDE_HEAD ||
      elide_behavior_ == gfx::ELIDE_MIDDLE ||
      elide_behavior_ == gfx::ELIDE_TAIL ||
      elide_behavior_ == gfx::ELIDE_EMAIL) {
    size.set_width(gfx::Canvas::GetStringWidth(
        std::u16string(gfx::kEllipsisUTF16), font_list()));
  }

  if (!GetMultiLine()) {
    if (elide_behavior_ == gfx::NO_ELIDE) {
      // If elision is disabled on single-line Labels, use text size as minimum.
      // This is OK because clients can use |gfx::ElideBehavior::TRUNCATE|
      // to get a non-eliding Label that should size itself less aggressively.
      size.SetToMax(GetTextSize());
    } else {
      size.SetToMin(GetTextSize());
    }
  }

  size.Enlarge(GetInsets().width(), GetInsets().height());
  return size;
}

gfx::Size Label::GetMaximumSize() const {
  return GetPreferredSize({});
}

int Label::GetLabelHeightForWidth(int w) const {
  if (!GetVisible() && collapse_when_hidden_) {
    return 0;
  }

  w -= GetInsets().width();
  int height = 0;
  int base_line_height = GetLineHeight();
  if (!GetMultiLine() || GetText().empty() || w < 0) {
    height = base_line_height;
  } else if (w == 0) {
    height =
        std::max(base::checked_cast<int>(GetMaxLines()), 1) * base_line_height;
  } else {
    // SetDisplayRect() has a side effect for later calls of GetStringSize().
    // Be careful to invoke full_text_->SetDisplayRect(gfx::Rect()) to
    // cancel this effect before the next time GetStringSize() is called.
    // It's beneficial not to cancel here, considering that some layout managers
    // invoke GetHeightForWidth() for the same width multiple times and
    // |full_text_| can cache the height.
    full_text_->SetDisplayRect(gfx::Rect(0, 0, w, 0));
    int string_height = full_text_->GetStringSize().height();
    // Cap the number of lines to GetMaxLines() if that's set.
    height = GetMaxLines() > 0
                 ? std::min(base::checked_cast<int>(GetMaxLines()) *
                                base_line_height,
                            string_height)
                 : string_height;
  }
  height -= gfx::ShadowValue::GetMargin(full_text_->shadows()).height();
  return height + GetInsets().height();
}

View* Label::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (!handles_tooltips_ ||
      (tooltip_text_.empty() && !ShouldShowDefaultTooltip())) {
    return nullptr;
  }

  return HitTestPoint(point) ? this : nullptr;
}

bool Label::GetCanProcessEventsWithinSubtree() const {
  return !!GetRenderTextForSelectionController();
}

WordLookupClient* Label::GetWordLookupClient() {
  return this;
}

std::u16string Label::GetTooltipText(const gfx::Point& p) const {
  if (handles_tooltips_) {
    if (!tooltip_text_.empty())
      return tooltip_text_;

    if (ShouldShowDefaultTooltip())
      return full_text_->GetDisplayText();
  }

  return std::u16string();
}

std::unique_ptr<gfx::RenderText> Label::CreateRenderText() const {
  // Multi-line labels only support NO_ELIDE and ELIDE_TAIL for now.
  // TODO(warx): Investigate more elide text support.
  gfx::ElideBehavior elide_behavior =
      GetMultiLine() && (elide_behavior_ != gfx::NO_ELIDE) ? gfx::ELIDE_TAIL
                                                           : elide_behavior_;

  std::unique_ptr<gfx::RenderText> render_text =
      full_text_->CreateInstanceOfSameStyle(GetText());
  render_text->SetHorizontalAlignment(GetHorizontalAlignment());
  render_text->SetVerticalAlignment(GetVerticalAlignment());
  render_text->SetElideBehavior(elide_behavior);
  render_text->SetObscured(GetObscured());
  render_text->SetMinLineHeight(GetLineHeight());
  render_text->set_shadows(GetShadows());
  const bool multiline = GetMultiLine();
  render_text->SetMultiline(multiline);
  render_text->SetMaxLines(multiline ? GetMaxLines() : size_t{0});
  render_text->SetWordWrapBehavior(full_text_->word_wrap_behavior());

  // Setup render text for selection controller.
  if (GetSelectable()) {
    render_text->set_focused(HasFocus());
    if (stored_selection_range_.IsValid())
      render_text->SelectRange(stored_selection_range_);
  }

  return render_text;
}

gfx::Rect Label::GetTextBounds() const {
  MaybeBuildDisplayText();

  if (!display_text_)
    return gfx::Rect(GetTextSize());

  return gfx::Rect(gfx::Point() + display_text_->GetLineOffset(0),
                   display_text_->GetStringSize());
}

int Label::GetFontListY() const {
  MaybeBuildDisplayText();

  if (!display_text_)
    return 0;

  return GetInsets().top() + display_text_->GetBaseline() -
         font_list().GetBaseline();
}

void Label::PaintText(gfx::Canvas* canvas) {
  MaybeBuildDisplayText();

  if (display_text_)
    display_text_->Draw(canvas);

#if DCHECK_IS_ON() && !BUILDFLAG(IS_CHROMEOS_ASH) && \
    !BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/40725997): Enable this DCHECK on ChromeOS and LaCrOS by
  // fixing either this check (to correctly idenfify more paints-on-opaque
  // cases), refactoring parents to use background() or by fixing
  // subpixel-rendering issues that the DCHECK detects.
  if (!display_text_ || display_text_->subpixel_rendering_suppressed() ||
      skip_subpixel_rendering_opacity_check_) {
    return;
  }

  // Ensure that, if we're using subpixel rendering, we're painted to an opaque
  // region. Subpixel rendering will sample from the r,g,b color channels of the
  // canvas. These values are incorrect when sampling from transparent pixels.
  // Note that these checks may need to be amended for other methods of painting
  // opaquely underneath the Label. For now, individual cases can skip this
  // DCHECK by calling Label::SetSkipSubpixelRenderingOpacityCheck().
  for (View* view = this; view; view = view->parent()) {
    // This is our approximation of being painted on an opaque region. If any
    // parent has an opaque background we assume that that background covers the
    // text bounds. This is not necessarily true as the background could be
    // inset from the parent bounds, and get_color() does not imply that all of
    // the background is painted with the same opaque color.
    if (view->background() && IsOpaque(view->background()->get_color()))
      break;

    if (view->layer()) {
      // If we aren't painted to an opaque background, we must paint to an
      // opaque layer.
      DCHECK(view->layer()->fills_bounds_opaquely());
      break;
    }
  }
#endif
}

void Label::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ClearDisplayText();
}

void Label::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  PaintText(canvas);
}

void Label::OnThemeChanged() {
  View::OnThemeChanged();
  UpdateColorsFromTheme();
}

ui::Cursor Label::GetCursor(const ui::MouseEvent& event) {
  return GetRenderTextForSelectionController() ? ui::mojom::CursorType::kIBeam
                                               : ui::Cursor();
}

void Label::OnFocus() {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (render_text) {
    render_text->set_focused(true);
    SchedulePaint();
  }
  View::OnFocus();
}

void Label::OnBlur() {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (render_text) {
    render_text->set_focused(false);
    SchedulePaint();
  }
  View::OnBlur();
}

bool Label::OnMousePressed(const ui::MouseEvent& event) {
  if (!GetRenderTextForSelectionController())
    return false;

  const bool had_focus = HasFocus();

  // RequestFocus() won't work when the label has FocusBehavior::NEVER. Hence
  // explicitly set the focused view.
  // TODO(karandeepb): If a widget with a label having FocusBehavior::NEVER as
  // the currently focused view (due to selection) was to lose focus, focus
  // won't be restored to the label (and hence a text selection won't be drawn)
  // when the widget gets focus again. Fix this.
  // Tracked in https://crbug.com/630365.
  if ((event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      GetFocusManager() && !had_focus) {
    GetFocusManager()->SetFocusedView(this);
  }

  if (ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    if (event.IsOnlyMiddleMouseButton() && GetFocusManager() && !had_focus)
      GetFocusManager()->SetFocusedView(this);
  }

  return selection_controller_->OnMousePressed(
      event, false,
      had_focus
          ? SelectionController::InitialFocusStateOnMousePress::kFocused
          : SelectionController::InitialFocusStateOnMousePress::kUnFocused);
}

bool Label::OnMouseDragged(const ui::MouseEvent& event) {
  if (!GetRenderTextForSelectionController())
    return false;

  return selection_controller_->OnMouseDragged(event);
}

void Label::OnMouseReleased(const ui::MouseEvent& event) {
  if (!GetRenderTextForSelectionController())
    return;

  selection_controller_->OnMouseReleased(event);
}

void Label::OnMouseCaptureLost() {
  if (!GetRenderTextForSelectionController())
    return;

  selection_controller_->OnMouseCaptureLost();
}

bool Label::OnKeyPressed(const ui::KeyEvent& event) {
  if (!GetRenderTextForSelectionController())
    return false;

  const bool shift = event.IsShiftDown();
  const bool control = event.IsControlDown();
  const bool alt = event.IsAltDown() || event.IsAltGrDown();

  switch (event.key_code()) {
    case ui::VKEY_C:
      if (control && !alt && HasSelection()) {
        CopyToClipboard();
        return true;
      }
      break;
    case ui::VKEY_INSERT:
      if (control && !shift && HasSelection()) {
        CopyToClipboard();
        return true;
      }
      break;
    case ui::VKEY_A:
      if (control && !alt && !GetText().empty()) {
        SelectAll();
        DCHECK(HasSelection());
        UpdateSelectionClipboard();
        return true;
      }
      break;
    default:
      break;
  }

  return false;
}

bool Label::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Allow the "Copy" action from the Chrome menu to be invoked. E.g., if a user
  // selects a Label on a web modal dialog. "Select All" doesn't appear in the
  // Chrome menu so isn't handled here.
  if (accelerator.key_code() == ui::VKEY_C && accelerator.IsCtrlDown()) {
    CopyToClipboard();
    return true;
  }
  return false;
}

bool Label::CanHandleAccelerators() const {
  // Focus needs to be checked since the accelerator for the Copy command from
  // the Chrome menu should only be handled when the current view has focus. See
  // related comment in BrowserView::CutCopyPaste.
  return HasFocus() && GetRenderTextForSelectionController() &&
         View::CanHandleAccelerators();
}

void Label::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
  // If the accessible name changed since the last time we computed the text
  // offsets, we need to recompute them.
  if (::ui::AXPlatform::GetInstance().IsUiaProviderEnabled() &&
      ax_name_used_to_compute_offsets_ !=
          GetViewAccessibility().GetCachedName()) {
    GetViewAccessibility().ClearTextOffsets();
    ax_name_used_to_compute_offsets_.clear();

    // TODO(crbug.com/325137417): When this function is only used to initialize
    // the cache with these values, refactor this part to not rely on the cache
    // as it will cause a chicken and egg situation. For now, this is necessary
    // to keep the text offsets up to date.
    if (RefreshAccessibleTextOffsets()) {
      ax_name_used_to_compute_offsets_ = GetViewAccessibility().GetCachedName();
      node_data->AddIntListAttribute(
          ax::mojom::IntListAttribute::kCharacterOffsets,
          GetViewAccessibility().GetCharacterOffsets());
      node_data->AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                     GetViewAccessibility().GetWordStarts());
      node_data->AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                     GetViewAccessibility().GetWordEnds());
    }
  }
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
}

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
bool Label::RefreshAccessibleTextOffsets() {
  // TODO(https://crbug.com/325137417): Should we clear the display text after
  // we rebuilt it only for accessibility purposes? Investigate this once we
  // migrate the text offsets attributes.
  MaybeBuildDisplayText();
  // TODO(crbug.com/40933356): Add support for multiline textfields.
  if (!display_text_ || display_text_->multiline()) {
    return false;
  }

  GetViewAccessibility().SetCharacterOffsets(
      ComputeTextOffsets(display_text_.get()));

  WordBoundaries boundaries = ComputeWordBoundaries(GetText());
  GetViewAccessibility().SetWordStarts(boundaries.starts);
  GetViewAccessibility().SetWordEnds(boundaries.ends);

  return true;
}
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

void Label::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                       float new_device_scale_factor) {
  View::OnDeviceScaleFactorChanged(old_device_scale_factor,
                                   new_device_scale_factor);
  // When the device scale factor is changed, some font rendering parameters is
  // changed (especially, hinting). The bounding box of the text has to be
  // re-computed based on the new parameters. See crbug.com/441439
  PreferredSizeChanged();
}

void Label::VisibilityChanged(View* starting_from, bool is_visible) {
  if (!is_visible)
    ClearDisplayText();
}

void Label::ShowContextMenuForViewImpl(View* source,
                                       const gfx::Point& point,
                                       ui::MenuSourceType source_type) {
  if (!GetRenderTextForSelectionController())
    return;

  context_menu_runner_ = std::make_unique<MenuRunner>(
      &context_menu_contents_,
      MenuRunner::HAS_MNEMONICS | MenuRunner::CONTEXT_MENU);
  context_menu_runner_->RunMenuAt(GetWidget(), nullptr,
                                  gfx::Rect(point, gfx::Size()),
                                  MenuAnchorPosition::kTopLeft, source_type);
}

bool Label::GetWordLookupDataAtPoint(const gfx::Point& point,
                                     gfx::DecoratedText* decorated_word,
                                     gfx::Rect* rect) {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  if (render_text && !render_text->obscured()) {
    return render_text->GetWordLookupDataAtPoint(point, decorated_word, rect);
  }
  return false;
}

bool Label::GetWordLookupDataFromSelection(gfx::DecoratedText* decorated_text,
                                           gfx::Rect* rect) {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  return render_text ? render_text->GetLookupDataForRange(
                           render_text->selection(), decorated_text, rect)
                     : false;
}

gfx::RenderText* Label::GetRenderTextForSelectionController() {
  return const_cast<gfx::RenderText*>(
      static_cast<const Label*>(this)->GetRenderTextForSelectionController());
}

bool Label::IsReadOnly() const {
  return true;
}

bool Label::SupportsDrag() const {
  // TODO(crbug.com/40491606): Labels should support dragging selected text.
  return false;
}

bool Label::HasTextBeingDragged() const {
  return false;
}

void Label::SetTextBeingDragged(bool value) {
  NOTREACHED();
}

int Label::GetViewHeight() const {
  return height();
}

int Label::GetViewWidth() const {
  return width();
}

int Label::GetDragSelectionDelay() const {
  // Labels don't need to use a repeating timer to update the drag selection.
  // Since the cursor is disabled for labels, a selection outside the display
  // area won't change the text in the display area. It is expected that all the
  // text will fit in the display area for labels anyway.
  return 0;
}

void Label::OnBeforePointerAction() {}

void Label::OnAfterPointerAction(bool text_changed, bool selection_changed) {
  DCHECK(!text_changed);
  if (selection_changed)
    SchedulePaint();
}

bool Label::PasteSelectionClipboard() {
  NOTREACHED();
}

void Label::UpdateSelectionClipboard() {
  if (ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    if (!GetObscured()) {
      ui::ScopedClipboardWriter(ui::ClipboardBuffer::kSelection)
          .WriteText(GetSelectedText());
    }
  }
}

bool Label::IsCommandIdChecked(int command_id) const {
  return true;
}

bool Label::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case MenuCommands::kCopy:
      return HasSelection() && !GetObscured();
    case MenuCommands::kSelectAll:
      return GetRenderTextForSelectionController() && !GetText().empty();
  }
  return false;
}

void Label::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case MenuCommands::kCopy:
      CopyToClipboard();
      break;
    case MenuCommands::kSelectAll:
      SelectAll();
      DCHECK(HasSelection());
      UpdateSelectionClipboard();
      break;
    default:
      NOTREACHED();
  }
}

bool Label::GetAcceleratorForCommandId(int command_id,
                                       ui::Accelerator* accelerator) const {
  switch (command_id) {
    case MenuCommands::kCopy:
      *accelerator = ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);
      return true;

    case MenuCommands::kSelectAll:
      *accelerator = ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN);
      return true;

    default:
      return false;
  }
}

const gfx::RenderText* Label::GetRenderTextForSelectionController() const {
  if (!GetSelectable())
    return nullptr;
  MaybeBuildDisplayText();

  // This may be null when the content bounds of the view are empty.
  return display_text_.get();
}

void Label::Init(const std::u16string& text,
                 const gfx::FontList& font_list,
                 gfx::DirectionalityMode directionality_mode) {
  full_text_ = gfx::RenderText::CreateRenderText();
  full_text_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  full_text_->SetFontList(font_list);
  full_text_->SetCursorEnabled(false);
  full_text_->SetWordWrapBehavior(gfx::TRUNCATE_LONG_WORDS);
  full_text_->SetMinLineHeight(GetLineHeight());
  UpdateFullTextElideBehavior();
  full_text_->SetDirectionalityMode(directionality_mode);

  GetViewAccessibility().SetRole(text_context_ == style::CONTEXT_DIALOG_TITLE
                                     ? ax::mojom::Role::kTitleBar
                                     : ax::mojom::Role::kStaticText);
  GetViewAccessibility().SetName(text);

  SetText(text);

  // Only selectable labels will get requests to show the context menu, due to
  // GetCanProcessEventsWithinSubtree().
  BuildContextMenuContents();
  set_context_menu_controller(this);
  GetViewAccessibility().set_needs_ax_tree_manager(true);

  // This allows the BrowserView to pass the copy command from the Chrome menu
  // to the Label.
  AddAccelerator(ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN));
}

void Label::MaybeBuildDisplayText() const {
  if (display_text_)
    return;

  gfx::Rect rect = GetContentsBounds();
  if (rect.IsEmpty())
    return;

  rect.Inset(-gfx::ShadowValue::GetMargin(GetShadows()));
  display_text_ = CreateRenderText();
  display_text_->SetDisplayRect(rect);
  stored_selection_range_ = gfx::Range::InvalidRange();
  ApplyTextColors();
}

gfx::Size Label::GetTextSize() const {
  return GetBoundedTextSize({width(), {}});
}

gfx::Size Label::GetBoundedTextSize(const SizeBounds& available_size) const {
  gfx::Size size;
  const int base_line_height = GetLineHeight();
  SizeBound w =
      std::max<SizeBound>(0, available_size.width() - GetInsets().width());
  if (GetText().empty() || (w == 0 && GetMultiLine())) {
    size = gfx::Size(0, base_line_height);
  } else if (max_width_single_line_ > 0) {
    DCHECK(!GetMultiLine());
    // Enable eliding during text width calculation. This allows the RenderText
    // to report an accurate width given the constraints and how it determines
    // to elide the text. If we simply clamp the width to the max after the
    // fact, then there may be some empty space left over *after* an ellipsis.
    // TODO(kerenzhu): `available_size` should be respected, but doing so will
    // break tests. Fix that.
    full_text_->SetDisplayRect(
        gfx::Rect(0, 0, max_width_single_line_ - GetInsets().width(), 0));
    size = full_text_->GetStringSize();

    if (base_line_height > 0) {
      size.set_height(base::checked_cast<int>(GetRequiredLines()) *
                      base_line_height);
    }
  } else {
    const int width = w.is_bounded() ? w.value() : 0;
    // SetDisplayRect() has side-effect. The text height will change to respect
    // width.
    full_text_->SetDisplayRect(gfx::Rect(0, 0, width, 0));
    size = full_text_->GetStringSize();

    if (base_line_height > 0) {
      size.set_height(base::checked_cast<int>(GetRequiredLines()) *
                      base_line_height);
    }
  }

  const gfx::Insets shadow_margin = -gfx::ShadowValue::GetMargin(GetShadows());
  size.Enlarge(shadow_margin.width(), shadow_margin.height());
  return size;
}

SkColor Label::GetForegroundColor(SkColor foreground,
                                  SkColor background) const {
  return (auto_color_readability_enabled_ && IsOpaque(background))
             ? color_utils::BlendForMinContrast(foreground, background).color
             : foreground;
}

void Label::RecalculateColors() {
  actual_enabled_color_ =
      GetForegroundColor(requested_enabled_color_, background_color_);
  // Using GetResultingPaintColor() here allows non-opaque selection backgrounds
  // to still participate in auto color readability, assuming
  // |background_color_| is itself opaque.
  actual_selection_text_color_ =
      GetForegroundColor(requested_selection_text_color_,
                         color_utils::GetResultingPaintColor(
                             selection_background_color_, background_color_));

  ApplyTextColors();
  SchedulePaint();
}

void Label::ApplyTextColors() const {
  if (!display_text_)
    return;

  display_text_->SetColor(actual_enabled_color_);
  display_text_->set_selection_color(actual_selection_text_color_);
  display_text_->set_selection_background_focused_color(
      selection_background_color_);
  const bool subpixel_rendering_enabled =
      subpixel_rendering_enabled_ && IsOpaque(background_color_);
  display_text_->set_subpixel_rendering_suppressed(!subpixel_rendering_enabled);
}

void Label::UpdateColorsFromTheme() {
  ui::ColorProvider* color_provider = GetColorProvider();
  if (enabled_color_id_.has_value()) {
    requested_enabled_color_ = color_provider->GetColor(*enabled_color_id_);
  } else if (!enabled_color_set_) {
    const std::optional<SkColor> cascading_color =
        GetCascadingProperty(this, kCascadingLabelEnabledColor);
    requested_enabled_color_ =
        cascading_color.value_or(GetColorProvider()->GetColor(
            TypographyProvider::Get().GetColorId(text_context_, text_style_)));
  }

  if (background_color_id_.has_value()) {
    background_color_ = color_provider->GetColor(*background_color_id_);
  } else if (!background_color_set_) {
    background_color_ = color_provider->GetColor(ui::kColorDialogBackground);
  }

  if (!selection_text_color_set_) {
    requested_selection_text_color_ =
        color_provider->GetColor(ui::kColorLabelSelectionForeground);
  }
  if (!selection_background_color_set_) {
    selection_background_color_ =
        color_provider->GetColor(ui::kColorLabelSelectionBackground);
  }
  RecalculateColors();
}

bool Label::ShouldShowDefaultTooltip() const {
  const gfx::Size text_size = GetTextSize();
  const gfx::Size size = GetContentsBounds().size();
  return !GetObscured() &&
         (text_size.width() > size.width() ||
          (GetMultiLine() && text_size.height() > size.height()));
}

void Label::ClearDisplayText() {
  // The HasSelection() call below will build |display_text_| in case it is
  // empty. Return early to avoid this.
  if (!display_text_)
    return;

  // Persist the selection range if there is an active selection.
  if (HasSelection()) {
    stored_selection_range_ =
        GetRenderTextForSelectionController()->selection();
  }
  display_text_ = nullptr;

  SchedulePaint();
}

std::u16string Label::GetSelectedText() const {
  const gfx::RenderText* render_text = GetRenderTextForSelectionController();
  return render_text ? render_text->GetTextFromRange(render_text->selection())
                     : std::u16string();
}

void Label::CopyToClipboard() {
  if (!HasSelection() || GetObscured())
    return;
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(GetSelectedText());
}

void Label::BuildContextMenuContents() {
  context_menu_contents_.AddItemWithStringId(MenuCommands::kCopy, IDS_APP_COPY);
  context_menu_contents_.AddItemWithStringId(MenuCommands::kSelectAll,
                                             IDS_APP_SELECT_ALL);
}

void Label::UpdateFullTextElideBehavior() {
  // In single line mode when a max width has been set, |full_text_| uses
  // elision to properly calculate the text size. Otherwise, it is not elided.
  full_text_->SetElideBehavior(max_width_single_line_ > 0 ? elide_behavior_
                                                          : gfx::NO_ELIDE);
}

BEGIN_METADATA(Label)
ADD_PROPERTY_METADATA(std::u16string, Text)
ADD_PROPERTY_METADATA(int, TextContext)
ADD_PROPERTY_METADATA(int, TextStyle)
ADD_PROPERTY_METADATA(bool, AutoColorReadabilityEnabled)
ADD_PROPERTY_METADATA(SkColor, EnabledColor, ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(gfx::ElideBehavior, ElideBehavior)
ADD_PROPERTY_METADATA(SkColor, BackgroundColor, ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(SkColor,
                      SelectionTextColor,
                      ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(SkColor,
                      SelectionBackgroundColor,
                      ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(bool, SubpixelRenderingEnabled)
ADD_PROPERTY_METADATA(bool, SkipSubpixelRenderingOpacityCheck)
ADD_PROPERTY_METADATA(gfx::ShadowValues, Shadows)
ADD_PROPERTY_METADATA(gfx::HorizontalAlignment, HorizontalAlignment)
ADD_PROPERTY_METADATA(gfx::VerticalAlignment, VerticalAlignment)
ADD_PROPERTY_METADATA(int, LineHeight)
ADD_PROPERTY_METADATA(bool, MultiLine)
ADD_PROPERTY_METADATA(size_t, MaxLines)
ADD_PROPERTY_METADATA(bool, Obscured)
ADD_PROPERTY_METADATA(bool, AllowCharacterBreak)
ADD_PROPERTY_METADATA(std::u16string, TooltipText)
ADD_PROPERTY_METADATA(bool, HandlesTooltips)
ADD_PROPERTY_METADATA(bool, CollapseWhenHidden)
ADD_PROPERTY_METADATA(int, MaximumWidth)
END_METADATA

}  // namespace views
