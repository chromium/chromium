// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/label.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/default_style.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/native_cursor.h"
#include "ui/views/selection_controller.h"

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

Label::Label() : Label(base::string16()) {
}

Label::Label(const base::string16& text)
    : Label(text, style::CONTEXT_LABEL, style::STYLE_PRIMARY) {}

Label::Label(const base::string16& text,
             int text_context,
             int text_style,
             gfx::DirectionalityMode directionality_mode)
    : text_context_(text_context), context_menu_contents_(this) {
  Init(text, style::GetFont(text_context, text_style), directionality_mode);
  SetLineHeight(style::GetLineHeight(text_context, text_style));

  // If an explicit style is given, ignore color changes due to the NativeTheme.
  if (text_style != style::STYLE_PRIMARY)
    SetEnabledColor(style::GetColor(*this, text_context, text_style));
}

Label::Label(const base::string16& text, const CustomFont& font)
    : text_context_(style::CONTEXT_LABEL), context_menu_contents_(this) {
  Init(text, font.font_list, gfx::DirectionalityMode::DIRECTIONALITY_FROM_TEXT);
}

Label::~Label() = default;

// static
const gfx::FontList& Label::GetDefaultFontList() {
  return style::GetFont(style::CONTEXT_LABEL, style::STYLE_PRIMARY);
}

void Label::SetFontList(const gfx::FontList& font_list) {
  full_text_->SetFontList(font_list);
  ResetLayout();
}

const base::string16& Label::GetText() const {
  return full_text_->text();
}

void Label::SetText(const base::string16& new_text) {
  if (new_text == GetText())
    return;
  full_text_->SetText(new_text);
  OnPropertyChanged(&full_text_ + kLabelText, kPropertyEffectsLayout);
  stored_selection_range_ = gfx::Range::InvalidRange();
}

int Label::GetTextContext() const {
  return text_context_;
}

bool Label::GetAutoColorReadabilityEnabled() const {
  return auto_color_readability_enabled_;
}

void Label::SetAutoColorReadabilityEnabled(
    bool auto_color_readability_enabled) {
  if (auto_color_readability_enabled_ == auto_color_readability_enabled)
    return;
  auto_color_readability_enabled_ = auto_color_readability_enabled;
  OnPropertyChanged(&auto_color_readability_enabled_, kPropertyEffectsPaint);
}

SkColor Label::GetEnabledColor() const {
  return actual_enabled_color_;
}

void Label::SetEnabledColor(SkColor color) {
  if (enabled_color_set_ && requested_enabled_color_ == color)
    return;
  requested_enabled_color_ = color;
  enabled_color_set_ = true;
  OnPropertyChanged(&requested_enabled_color_, kPropertyEffectsPaint);
}

SkColor Label::GetBackgroundColor() const {
  return background_color_;
}

void Label::SetBackgroundColor(SkColor color) {
  if (background_color_set_ && background_color_ == color)
    return;
  background_color_ = color;
  background_color_set_ = true;
  OnPropertyChanged(&background_color_, kPropertyEffectsPaint);
}

SkColor Label::GetSelectionTextColor() const {
  return actual_selection_text_color_;
}

void Label::SetSelectionTextColor(SkColor color) {
  if (selection_text_color_set_ && requested_selection_text_color_ == color)
    return;
  requested_selection_text_color_ = color;
  selection_text_color_set_ = true;
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
  OnPropertyChanged(&selection_background_color_, kPropertyEffectsPaint);
}

const gfx::ShadowValues& Label::GetShadows() const {
  return full_text_->shadows();
}

void Label::SetShadows(const gfx::ShadowValues& shadows) {
  if (full_text_->shadows() == shadows)
    return;
  full_text_->set_shadows(shadows);
  OnPropertyChanged(&full_text_ + kLabelShadows, kPropertyEffectsLayout);
}

bool Label::GetSubpixelRenderingEnabled() const {
  return subpixel_rendering_enabled_;
}

void Label::SetSubpixelRenderingEnabled(bool subpixel_rendering_enabled) {
  if (subpixel_rendering_enabled_ == subpixel_rendering_enabled)
    return;
  subpixel_rendering_enabled_ = subpixel_rendering_enabled;
  OnPropertyChanged(&subpixel_rendering_enabled_, kPropertyEffectsPaint);
}

gfx::HorizontalAlignment Label::GetHorizontalAlignment() const {
  return full_text_->horizontal_alignment();
}

void Label::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  alignment = gfx::MaybeFlipForRTL(alignment);
  if (GetHorizontalAlignment() == alignment)
    return;
  full_text_->SetHorizontalAlignment(alignment);
  OnPropertyChanged(&full_text_ + kLabelHorizontalAlignment,
                    kPropertyEffectsLayout);
}

gfx::VerticalAlignment Label::GetVerticalAlignment() const {
  return full_text_->vertical_alignment();
}

void Label::SetVerticalAlignment(gfx::VerticalAlignment alignment) {
  if (GetVerticalAlignment() == alignment)
    return;
  full_text_->SetVerticalAlignment(alignment);
  // TODO(dfried): consider if this should be kPropertyEffectsPaint instead.
  OnPropertyChanged(&full_text_ + kLabelVerticalAlignment,
                    kPropertyEffectsLayout);
}

int Label::GetLineHeight() const {
  return full_text_->min_line_height();
}

void Label::SetLineHeight(int height) {
  if (GetLineHeight() == height)
    return;
  full_text_->SetMinLineHeight(height);
  OnPropertyChanged(&full_text_ + kLabelLineHeight, kPropertyEffectsLayout);
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
  full_text_->SetMultiline(multi_line);
  OnPropertyChanged(&multi_line_, kPropertyEffectsLayout);
}

int Label::GetMaxLines() const {
  return max_lines_;
}

void Label::SetMaxLines(int max_lines) {
  if (max_lines_ == max_lines)
    return;
  max_lines_ = max_lines;
  OnPropertyChanged(&max_lines_, kPropertyEffectsLayout);
}

bool Label::GetObscured() const {
  return full_text_->obscured();
}

void Label::SetObscured(bool obscured) {
  if (this->GetObscured() == obscured)
    return;
  full_text_->SetObscured(obscured);
  if (obscured)
    SetSelectable(false);
  OnPropertyChanged(&full_text_ + kLabelObscured, kPropertyEffectsLayout);
}

bool Label::IsDisplayTextTruncated() const {
  MaybeBuildDisplayText();
  if (!full_text_ || full_text_->text().empty())
    return false;
  auto text_bounds = GetTextBounds();
  return (display_text_ &&
          display_text_->text() != display_text_->GetDisplayText()) ||
         text_bounds.width() > GetContentsBounds().width() ||
         text_bounds.height() > GetContentsBounds().height();
}

bool Label::GetAllowCharacterBreak() const {
  return full_text_->word_wrap_behavior() == gfx::WRAP_LONG_WORDS ? true
                                                                  : false;
}

void Label::SetAllowCharacterBreak(bool allow_character_break) {
  const gfx::WordWrapBehavior behavior =
      allow_character_break ? gfx::WRAP_LONG_WORDS : gfx::TRUNCATE_LONG_WORDS;
  if (full_text_->word_wrap_behavior() == behavior)
    return;
  full_text_->SetWordWrapBehavior(behavior);
  OnPropertyChanged(&full_text_ + kLabelAllowCharacterBreak,
                    kPropertyEffectsLayout);
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
  OnPropertyChanged(&elide_behavior_, kPropertyEffectsLayout);
}

base::string16 Label::GetTooltipText() const {
  return tooltip_text_;
}

void Label::SetTooltipText(const base::string16& tooltip_text) {
  DCHECK(handles_tooltips_);
  if (tooltip_text_ == tooltip_text)
    return;
  tooltip_text_ = tooltip_text;
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

base::string16 Label::GetDisplayTextForTesting() {
  ClearDisplayText();
  MaybeBuildDisplayText();
  return display_text_ ? display_text_->GetDisplayText() : base::string16();
}

base::i18n::TextDirection Label::GetTextDirectionForTesting() {
  return full_text_->GetDisplayTextDirection();
}

bool Label::IsSelectionSupported() const {
  return !GetObscured() && full_text_->IsSelectionSupported();
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

int Label::GetBaseline() const {
  return GetInsets().top() + font_list().GetBaseline();
}

gfx::Size Label::CalculatePreferredSize() const {
  // Return a size of (0, 0) if the label is not visible and if the
  // |collapse_when_hidden_| flag is set.
  // TODO(munjal): This logic probably belongs to the View class. But for now,
  // put it here since putting it in View class means all inheriting classes
  // need to respect the |collapse_when_hidden_| flag.
  if (!GetVisible() && collapse_when_hidden_)
    return gfx::Size();

  if (GetMultiLine() && fixed_width_ != 0 && !GetText().empty())
    return gfx::Size(fixed_width_, GetHeightForWidth(fixed_width_));

  gfx::Size size(GetTextSize());
  const gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());

  if (GetMultiLine() && max_width_ != 0 && max_width_ < size.width())
    return gfx::Size(max_width_, GetHeightForWidth(max_width_));

  if (GetMultiLine() && GetMaxLines() > 0)
    return gfx::Size(size.width(), GetHeightForWidth(size.width()));
  return size;
}

gfx::Size Label::GetMinimumSize() const {
  if (!GetVisible() && collapse_when_hidden_)
    return gfx::Size();

  // Always reserve vertical space for at least one line.
  gfx::Size size(0, std::max(font_list().GetHeight(), GetLineHeight()));
  if (elide_behavior_ == gfx::ELIDE_HEAD ||
      elide_behavior_ == gfx::ELIDE_MIDDLE ||
      elide_behavior_ == gfx::ELIDE_TAIL ||
      elide_behavior_ == gfx::ELIDE_EMAIL) {
    size.set_width(gfx::Canvas::GetStringWidth(
        base::string16(gfx::kEllipsisUTF16), font_list()));
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

int Label::GetHeightForWidth(int w) const {
  if (!GetVisible() && collapse_when_hidden_)
    return 0;

  w -= GetInsets().width();
  int height = 0;
  int base_line_height = std::max(GetLineHeight(), font_list().GetHeight());
  if (!GetMultiLine() || GetText().empty() || w <= 0) {
    height = base_line_height;
  } else {
    // SetDisplayRect() has a side effect for later calls of GetStringSize().
    // Be careful to invoke |full_text_->SetDisplayRect(gfx::Rect())| to
    // cancel this effect before the next time GetStringSize() is called.
    // It would be beneficial not to cancel here, considering that some layout
    // managers invoke GetHeightForWidth() for the same width multiple times
    // and |full_text_| can cache the height.
    full_text_->SetDisplayRect(gfx::Rect(0, 0, w, 0));
    int string_height = full_text_->GetStringSize().height();
    // Cap the number of lines to |GetMaxLines()| if multi-line and non-zero
    // |GetMaxLines()|.
    height = GetMultiLine() && GetMaxLines() > 0
                 ? std::min(GetMaxLines() * base_line_height, string_height)
                 : string_height;
  }
  height -= gfx::ShadowValue::GetMargin(full_text_->shadows()).height();
  return height + GetInsets().height();
}

View* Label::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (!handles_tooltips_ ||
      (tooltip_text_.empty() && !ShouldShowDefaultTooltip()))
    return nullptr;

  return HitTestPoint(point) ? this : nullptr;
}

bool Label::CanProcessEventsWithinSubtree() const {
  return !!GetRenderTextForSelectionController();
}

WordLookupClient* Label::GetWordLookupClient() {
  return this;
}

void Label::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (text_context_ == style::CONTEXT_DIALOG_TITLE)
    node_data->role = ax::mojom::Role::kTitleBar;
  else
    node_data->role = ax::mojom::Role::kStaticText;

  node_data->SetName(full_text_->GetDisplayText());
}

base::string16 Label::GetTooltipText(const gfx::Point& p) const {
  if (handles_tooltips_) {
    if (!tooltip_text_.empty())
      return tooltip_text_;

    if (ShouldShowDefaultTooltip())
      return full_text_->GetDisplayText();
  }

  return base::string16();
}

void Label::OnHandlePropertyChangeEffects(PropertyEffects property_effects) {
  if (property_effects & kPropertyEffectsPreferredSizeChanged)
    SizeToPreferredSize();
  if (property_effects & kPropertyEffectsLayout)
    ResetLayout();
  if (property_effects & kPropertyEffectsPaint)
    RecalculateColors();
}

std::unique_ptr<gfx::RenderText> Label::CreateRenderText() const {
  // Multi-line labels only support NO_ELIDE and ELIDE_TAIL for now.
  // TODO(warx): Investigate more elide text support.
  gfx::ElideBehavior elide_behavior =
      GetMultiLine() && (elide_behavior_ != gfx::NO_ELIDE) ? gfx::ELIDE_TAIL
                                                           : elide_behavior_;

  auto render_text = gfx::RenderText::CreateHarfBuzzInstance();
  render_text->SetHorizontalAlignment(GetHorizontalAlignment());
  render_text->SetVerticalAlignment(GetVerticalAlignment());
  render_text->SetDirectionalityMode(full_text_->directionality_mode());
  render_text->SetElideBehavior(elide_behavior);
  render_text->SetObscured(GetObscured());
  render_text->SetMinLineHeight(GetLineHeight());
  render_text->SetFontList(font_list());
  render_text->set_shadows(GetShadows());
  render_text->SetCursorEnabled(false);
  render_text->SetText(GetText());
  const bool multiline = GetMultiLine();
  render_text->SetMultiline(multiline);
  render_text->SetMaxLines(multiline ? GetMaxLines() : 0);
  render_text->SetWordWrapBehavior(full_text_->word_wrap_behavior());

  // Setup render text for selection controller.
  if (GetSelectable()) {
    render_text->set_focused(HasFocus());
    if (stored_selection_range_.IsValid())
      render_text->SelectRange(stored_selection_range_);
  }

  return render_text;
}

void Label::PaintFocusRing(gfx::Canvas* canvas) const {
  // No focus ring by default.
}

gfx::Rect Label::GetTextBounds() const {
  MaybeBuildDisplayText();

  if (!display_text_)
    return gfx::Rect(GetTextSize());

  return gfx::Rect(gfx::Point() + display_text_->GetLineOffset(0),
                   display_text_->GetStringSize());
}

void Label::PaintText(gfx::Canvas* canvas) {
  MaybeBuildDisplayText();

  if (display_text_)
    display_text_->Draw(canvas);

#if DCHECK_IS_ON()
  // Attempt to ensure that if we're using subpixel rendering, we're painting
  // to an opaque background. What we don't want to find is an ancestor in the
  // hierarchy that paints to a non-opaque layer.
  if (!display_text_ || display_text_->subpixel_rendering_suppressed())
    return;

  for (View* view = this; view; view = view->parent()) {
    if (view->background() && IsOpaque(view->background()->get_color()))
      break;

    if (view->layer() && view->layer()->fills_bounds_opaquely()) {
      DLOG(WARNING) << "Ancestor view has a non-opaque layer: "
                    << view->GetClassName() << " with ID " << view->GetID();
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
  if (HasFocus())
    PaintFocusRing(canvas);
}

void Label::OnThemeChanged() {
  UpdateColorsFromTheme();
}

gfx::NativeCursor Label::GetCursor(const ui::MouseEvent& event) {
  return GetRenderTextForSelectionController() ? GetNativeIBeamCursor()
                                               : gfx::kNullCursor;
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

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (event.IsOnlyMiddleMouseButton() && GetFocusManager() && !had_focus)
    GetFocusManager()->SetFocusedView(this);
#endif

  return selection_controller_->OnMousePressed(
      event, false, had_focus ? SelectionController::FOCUSED
                              : SelectionController::UNFOCUSED);
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

void Label::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                       float new_device_scale_factor) {
  View::OnDeviceScaleFactorChanged(old_device_scale_factor,
                                   new_device_scale_factor);
  // When the device scale factor is changed, some font rendering parameters is
  // changed (especially, hinting). The bounding box of the text has to be
  // re-computed based on the new parameters. See crbug.com/441439
  ResetLayout();
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
                                     gfx::Point* baseline_point) {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  return render_text ? render_text->GetWordLookupDataAtPoint(
                           point, decorated_word, baseline_point)
                     : false;
}

bool Label::GetWordLookupDataFromSelection(gfx::DecoratedText* decorated_text,
                                           gfx::Point* baseline_point) {
  gfx::RenderText* render_text = GetRenderTextForSelectionController();
  return render_text
             ? render_text->GetLookupDataForRange(
                   render_text->selection(), decorated_text, baseline_point)
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
  // TODO(crbug.com/661379): Labels should support dragging selected text.
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
  return false;
}

void Label::UpdateSelectionClipboard() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (!GetObscured()) {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kSelection)
        .WriteText(GetSelectedText());
  }
#endif
}

bool Label::IsCommandIdChecked(int command_id) const {
  return true;
}

bool Label::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDS_APP_COPY:
      return HasSelection() && !GetObscured();
    case IDS_APP_SELECT_ALL:
      return GetRenderTextForSelectionController() && !GetText().empty();
  }
  return false;
}

void Label::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDS_APP_COPY:
      CopyToClipboard();
      break;
    case IDS_APP_SELECT_ALL:
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
    case IDS_APP_COPY:
      *accelerator = ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);
      return true;

    case IDS_APP_SELECT_ALL:
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

void Label::Init(const base::string16& text,
                 const gfx::FontList& font_list,
                 gfx::DirectionalityMode directionality_mode) {
  full_text_ = gfx::RenderText::CreateHarfBuzzInstance();
  DCHECK(full_text_->MultilineSupported());
  full_text_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  full_text_->SetDirectionalityMode(directionality_mode);
  // NOTE: |full_text_| should not be elided at all. This is used to keep
  // some properties and to compute the size of the string.
  full_text_->SetElideBehavior(gfx::NO_ELIDE);
  full_text_->SetFontList(font_list);
  full_text_->SetCursorEnabled(false);
  full_text_->SetWordWrapBehavior(gfx::TRUNCATE_LONG_WORDS);

  elide_behavior_ = gfx::ELIDE_TAIL;
  stored_selection_range_ = gfx::Range::InvalidRange();
  enabled_color_set_ = background_color_set_ = false;
  selection_text_color_set_ = selection_background_color_set_ = false;
  subpixel_rendering_enabled_ = true;
  auto_color_readability_enabled_ = true;
  multi_line_ = false;
  max_lines_ = 0;
  UpdateColorsFromTheme();
  handles_tooltips_ = true;
  collapse_when_hidden_ = false;
  fixed_width_ = 0;
  max_width_ = 0;
  SetText(text);

  // Only selectable labels will get requests to show the context menu, due to
  // CanProcessEventsWithinSubtree().
  BuildContextMenuContents();
  set_context_menu_controller(this);

  // This allows the BrowserView to pass the copy command from the Chrome menu
  // to the Label.
  AddAccelerator(ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN));
}

void Label::ResetLayout() {
  PreferredSizeChanged();
  SchedulePaint();
  ClearDisplayText();
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
  gfx::Size size;
  if (GetText().empty()) {
    size = gfx::Size(0, std::max(GetLineHeight(), font_list().GetHeight()));
  } else {
    // Cancel the display rect of |full_text_|. The display rect may be
    // specified in GetHeightForWidth(), and specifying empty Rect cancels
    // its effect. See also the comment in GetHeightForWidth().
    // TODO(mukai): use gfx::Rect() to compute the ideal size rather than
    // the current width(). See crbug.com/468494, crbug.com/467526, and
    // the comment for MultilinePreferredSizeTest in label_unittest.cc.
    full_text_->SetDisplayRect(gfx::Rect(0, 0, width(), 0));
    size = full_text_->GetStringSize();
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
  ui::NativeTheme* theme = GetNativeTheme();
  if (!enabled_color_set_) {
    requested_enabled_color_ =
        style::GetColor(*this, text_context_, style::STYLE_PRIMARY);
  }
  if (!background_color_set_) {
    background_color_ =
        theme->GetSystemColor(ui::NativeTheme::kColorId_DialogBackground);
  }
  if (!selection_text_color_set_) {
    requested_selection_text_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelTextSelectionColor);
  }
  if (!selection_background_color_set_) {
    selection_background_color_ = theme->GetSystemColor(
        ui::NativeTheme::kColorId_LabelTextSelectionBackgroundFocused);
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

void Label::ClearDisplayText() const {
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
}

base::string16 Label::GetSelectedText() const {
  const gfx::RenderText* render_text = GetRenderTextForSelectionController();
  return render_text ? render_text->GetTextFromRange(render_text->selection())
                     : base::string16();
}

void Label::CopyToClipboard() {
  if (!HasSelection() || GetObscured())
    return;
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(GetSelectedText());
}

void Label::BuildContextMenuContents() {
  context_menu_contents_.AddItemWithStringId(IDS_APP_COPY, IDS_APP_COPY);
  context_menu_contents_.AddItemWithStringId(IDS_APP_SELECT_ALL,
                                             IDS_APP_SELECT_ALL);
}

BEGIN_METADATA(Label)
METADATA_PARENT_CLASS(View)
ADD_PROPERTY_METADATA(Label, bool, AutoColorReadabilityEnabled)
ADD_PROPERTY_METADATA(Label, base::string16, Text)
ADD_PROPERTY_METADATA(Label, SkColor, EnabledColor)
ADD_PROPERTY_METADATA(Label, gfx::ElideBehavior, ElideBehavior)
ADD_PROPERTY_METADATA(Label, SkColor, BackgroundColor)
ADD_PROPERTY_METADATA(Label, SkColor, SelectionTextColor)
ADD_PROPERTY_METADATA(Label, SkColor, SelectionBackgroundColor)
ADD_PROPERTY_METADATA(Label, bool, SubpixelRenderingEnabled)
ADD_PROPERTY_METADATA(Label, gfx::ShadowValues, Shadows)
ADD_PROPERTY_METADATA(Label, gfx::HorizontalAlignment, HorizontalAlignment)
ADD_PROPERTY_METADATA(Label, gfx::VerticalAlignment, VerticalAlignment)
ADD_PROPERTY_METADATA(Label, int, LineHeight)
ADD_PROPERTY_METADATA(Label, bool, MultiLine)
ADD_PROPERTY_METADATA(Label, int, MaxLines)
ADD_PROPERTY_METADATA(Label, bool, Obscured)
ADD_PROPERTY_METADATA(Label, bool, AllowCharacterBreak)
ADD_PROPERTY_METADATA(Label, base::string16, TooltipText)
ADD_PROPERTY_METADATA(Label, bool, HandlesTooltips)
ADD_PROPERTY_METADATA(Label, bool, CollapseWhenHidden)
ADD_PROPERTY_METADATA(Label, int, MaximumWidth)
ADD_READONLY_PROPERTY_METADATA(Label, int, TextContext)
END_METADATA()

}  // namespace views
