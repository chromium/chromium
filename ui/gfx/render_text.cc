// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text.h"

#include <limits.h>

#include <algorithm>
#include <climits>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_shader.h"
#include "third_party/icu/source/common/unicode/rbbi.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/platform_font.h"
#include "ui/gfx/render_text_harfbuzz.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/utf16_indexing.h"

#if defined(OS_MACOSX)
#include "third_party/skia/include/ports/SkTypeface_mac.h"
#include "ui/gfx/render_text_mac.h"
#endif  // defined(OS_MACOSX)

namespace gfx {

namespace {

// Default color used for the text and cursor.
const SkColor kDefaultColor = SK_ColorBLACK;

// Default color used for drawing selection background.
const SkColor kDefaultSelectionBackgroundColor = SK_ColorGRAY;

// Fraction of the text size to raise the center of a strike-through line above
// the baseline.
const SkScalar kStrikeThroughOffset = (SK_Scalar1 * 65 / 252);
// Fraction of the text size to lower an underline below the baseline.
const SkScalar kUnderlineOffset = (SK_Scalar1 / 9);
// Default fraction of the text size to use for a strike-through or underline.
const SkScalar kLineThicknessFactor = (SK_Scalar1 / 18);

// Invalid value of baseline.  Assigning this value to |baseline_| causes
// re-calculation of baseline.
const int kInvalidBaseline = INT_MAX;

// Given |font| and |display_width|, returns the width of the fade gradient.
int CalculateFadeGradientWidth(const FontList& font_list, int display_width) {
  // Fade in/out about 3 characters of the beginning/end of the string.
  // Use a 1/3 of the display width if the display width is very short.
  const int narrow_width = font_list.GetExpectedTextWidth(3);
  const int gradient_width =
      std::min(narrow_width, gfx::ToRoundedInt(display_width / 3.f));
  DCHECK_GE(gradient_width, 0);
  return gradient_width;
}

// Appends to |positions| and |colors| values corresponding to the fade over
// |fade_rect| from color |c0| to color |c1|.
void AddFadeEffect(const Rect& text_rect,
                   const Rect& fade_rect,
                   SkColor c0,
                   SkColor c1,
                   std::vector<SkScalar>* positions,
                   std::vector<SkColor>* colors) {
  const SkScalar left = static_cast<SkScalar>(fade_rect.x() - text_rect.x());
  const SkScalar width = static_cast<SkScalar>(fade_rect.width());
  const SkScalar p0 = left / text_rect.width();
  const SkScalar p1 = (left + width) / text_rect.width();
  // Prepend 0.0 to |positions|, as required by Skia.
  if (positions->empty() && p0 != 0.0) {
    positions->push_back(0.0);
    colors->push_back(c0);
  }
  positions->push_back(p0);
  colors->push_back(c0);
  positions->push_back(p1);
  colors->push_back(c1);
}

// Creates a SkShader to fade the text, with |left_part| specifying the left
// fade effect, if any, and |right_part| specifying the right fade effect.
sk_sp<cc::PaintShader> CreateFadeShader(const FontList& font_list,
                                        const Rect& text_rect,
                                        const Rect& left_part,
                                        const Rect& right_part,
                                        SkColor color) {
  // The shader should only specify transparency of the fade itself, not the
  // original transparency, which will be applied by the actual renderer.
  DCHECK_EQ(SkColorGetA(color), static_cast<uint8_t>(0xff));

  // In general, fade down to 0 alpha.  But when the available width is less
  // than four characters, linearly ramp up the fade target alpha to as high as
  // 20% at zero width.  This allows the user to see the last faded characters a
  // little better when there are only a few characters shown.
  const float width_fraction =
      text_rect.width() / static_cast<float>(font_list.GetExpectedTextWidth(4));
  const SkAlpha kAlphaAtZeroWidth = 51;
  const SkAlpha alpha =
      (width_fraction < 1)
          ? gfx::ToRoundedInt((1 - width_fraction) * kAlphaAtZeroWidth)
          : 0;
  const SkColor fade_color = SkColorSetA(color, alpha);

  std::vector<SkScalar> positions;
  std::vector<SkColor> colors;

  if (!left_part.IsEmpty())
    AddFadeEffect(text_rect, left_part, fade_color, color,
                  &positions, &colors);
  if (!right_part.IsEmpty())
    AddFadeEffect(text_rect, right_part, color, fade_color,
                  &positions, &colors);
  DCHECK(!positions.empty());

  // Terminate |positions| with 1.0, as required by Skia.
  if (positions.back() != 1.0) {
    positions.push_back(1.0);
    colors.push_back(colors.back());
  }

  const SkPoint points[2] = { PointToSkPoint(text_rect.origin()),
                              PointToSkPoint(text_rect.top_right()) };
  return cc::PaintShader::MakeLinearGradient(&points[0], &colors[0],
                                             &positions[0], colors.size(),
                                             SkShader::kClamp_TileMode);
}

// Converts a FontRenderParams::Hinting value to the corresponding
// cc::PaintFlags::Hinting value.
cc::PaintFlags::Hinting FontRenderParamsHintingToPaintFlagsHinting(
    FontRenderParams::Hinting params_hinting) {
  switch (params_hinting) {
    case FontRenderParams::HINTING_NONE:
      return cc::PaintFlags::kNo_Hinting;
    case FontRenderParams::HINTING_SLIGHT:
      return cc::PaintFlags::kSlight_Hinting;
    case FontRenderParams::HINTING_MEDIUM:
      return cc::PaintFlags::kNormal_Hinting;
    case FontRenderParams::HINTING_FULL:
      return cc::PaintFlags::kFull_Hinting;
  }
  return cc::PaintFlags::kNo_Hinting;
}

// Make sure ranges don't break text graphemes.  If a range in |break_list|
// does break a grapheme in |render_text|, the range will be slightly
// extended to encompass the grapheme.
template <typename T>
void RestoreBreakList(RenderText* render_text, BreakList<T>* break_list) {
  break_list->SetMax(render_text->text().length());
  Range range;
  while (range.end() < break_list->max()) {
    const auto& current_break = break_list->GetBreak(range.end());
    range = break_list->GetRange(current_break);
    if (range.end() < break_list->max() &&
        !render_text->IsValidCursorIndex(range.end())) {
      range.set_end(
          render_text->IndexOfAdjacentGrapheme(range.end(), CURSOR_FORWARD));
      break_list->ApplyValue(current_break->second, range);
    }
  }
}

}  // namespace

namespace internal {

SkiaTextRenderer::SkiaTextRenderer(Canvas* canvas)
    : canvas_(canvas), canvas_skia_(canvas->sk_canvas()) {
  DCHECK(canvas_skia_);
  flags_.setTextEncoding(cc::PaintFlags::kGlyphID_TextEncoding);
  flags_.setStyle(cc::PaintFlags::kFill_Style);
  flags_.setAntiAlias(true);
  flags_.setSubpixelText(true);
  flags_.setLCDRenderText(true);
  flags_.setHinting(cc::PaintFlags::kNormal_Hinting);
}

SkiaTextRenderer::~SkiaTextRenderer() {
}

void SkiaTextRenderer::SetDrawLooper(sk_sp<SkDrawLooper> draw_looper) {
  flags_.setLooper(std::move(draw_looper));
}

void SkiaTextRenderer::SetFontRenderParams(const FontRenderParams& params,
                                           bool subpixel_rendering_suppressed) {
  ApplyRenderParams(params, subpixel_rendering_suppressed, &flags_);
}

void SkiaTextRenderer::SetTypeface(sk_sp<SkTypeface> typeface) {
  flags_.setTypeface(std::move(typeface));
}

void SkiaTextRenderer::SetTextSize(SkScalar size) {
  flags_.setTextSize(size);
}

void SkiaTextRenderer::SetForegroundColor(SkColor foreground) {
  flags_.setColor(foreground);
}

void SkiaTextRenderer::SetShader(sk_sp<cc::PaintShader> shader) {
  flags_.setShader(std::move(shader));
}

void SkiaTextRenderer::DrawPosText(const SkPoint* pos,
                                   const uint16_t* glyphs,
                                   size_t glyph_count) {
  SkTextBlobBuilder builder;
  const auto& run_buffer = builder.allocRunPos(flags_.ToSkPaint(), glyph_count);

  static_assert(sizeof(*glyphs) == sizeof(*run_buffer.glyphs), "");
  memcpy(run_buffer.glyphs, glyphs, glyph_count * sizeof(*glyphs));

  static_assert(sizeof(*pos) == 2 * sizeof(*run_buffer.pos), "");
  memcpy(run_buffer.pos, pos, glyph_count * sizeof(*pos));

  canvas_skia_->drawTextBlob(builder.make(), 0, 0, flags_);
}

void SkiaTextRenderer::DrawUnderline(int x,
                                     int y,
                                     int width,
                                     SkScalar thickness_factor) {
  SkScalar x_scalar = SkIntToScalar(x);
  const SkScalar text_size = flags_.getTextSize();
  SkRect r = SkRect::MakeLTRB(
      x_scalar, y + text_size * kUnderlineOffset, x_scalar + width,
      y + (text_size *
           (kUnderlineOffset + (thickness_factor * kLineThicknessFactor))));
  canvas_skia_->drawRect(r, flags_);
}

void SkiaTextRenderer::DrawStrike(int x,
                                  int y,
                                  int width,
                                  SkScalar thickness_factor) {
  const SkScalar text_size = flags_.getTextSize();
  const SkScalar height = text_size * thickness_factor;
  const SkScalar top = y - text_size * kStrikeThroughOffset - height / 2;
  SkScalar x_scalar = SkIntToScalar(x);
  const SkRect r =
      SkRect::MakeLTRB(x_scalar, top, x_scalar + width, top + height);
  canvas_skia_->drawRect(r, flags_);
}

StyleIterator::StyleIterator(const BreakList<SkColor>& colors,
                             const BreakList<BaselineStyle>& baselines,
                             const BreakList<int>& font_size_overrides,
                             const BreakList<Font::Weight>& weights,
                             const std::vector<BreakList<bool>>& styles)
    : colors_(colors),
      baselines_(baselines),
      font_size_overrides_(font_size_overrides),
      weights_(weights),
      styles_(styles) {
  color_ = colors_.breaks().begin();
  baseline_ = baselines_.breaks().begin();
  font_size_override_ = font_size_overrides_.breaks().begin();
  weight_ = weights_.breaks().begin();
  for (size_t i = 0; i < styles_.size(); ++i)
    style_.push_back(styles_[i].breaks().begin());
}

StyleIterator::~StyleIterator() {}

Range StyleIterator::GetRange() const {
  Range range(colors_.GetRange(color_));
  range = range.Intersect(baselines_.GetRange(baseline_));
  range = range.Intersect(font_size_overrides_.GetRange(font_size_override_));
  range = range.Intersect(weights_.GetRange(weight_));
  for (size_t i = 0; i < NUM_TEXT_STYLES; ++i)
    range = range.Intersect(styles_[i].GetRange(style_[i]));
  return range;
}

void StyleIterator::UpdatePosition(size_t position) {
  color_ = colors_.GetBreak(position);
  baseline_ = baselines_.GetBreak(position);
  font_size_override_ = font_size_overrides_.GetBreak(position);
  weight_ = weights_.GetBreak(position);
  for (size_t i = 0; i < NUM_TEXT_STYLES; ++i)
    style_[i] = styles_[i].GetBreak(position);
}

LineSegment::LineSegment() : run(0) {}

LineSegment::~LineSegment() {}

Line::Line() : preceding_heights(0), baseline(0) {}

Line::Line(const Line& other) = default;

Line::~Line() {}

void ApplyRenderParams(const FontRenderParams& params,
                       bool subpixel_rendering_suppressed,
                       cc::PaintFlags* flags) {
  flags->setAntiAlias(params.antialiasing);
  flags->setLCDRenderText(!subpixel_rendering_suppressed &&
                          params.subpixel_rendering !=
                              FontRenderParams::SUBPIXEL_RENDERING_NONE);
  flags->setSubpixelText(params.subpixel_positioning);
  flags->setAutohinted(params.autohinter);
  flags->setHinting(FontRenderParamsHintingToPaintFlagsHinting(params.hinting));
}

}  // namespace internal

// static
constexpr base::char16 RenderText::kPasswordReplacementChar;
constexpr bool RenderText::kDragToEndIfOutsideVerticalBounds;

RenderText::~RenderText() {
}

// static
std::unique_ptr<RenderText> RenderText::CreateHarfBuzzInstance() {
  return std::make_unique<RenderTextHarfBuzz>();
}

// static
std::unique_ptr<RenderText> RenderText::CreateFor(Typesetter typesetter) {
#if defined(OS_MACOSX)
  if (typesetter == Typesetter::NATIVE)
    return std::make_unique<RenderTextMac>();

#endif  // defined(OS_MACOSX)
  return CreateHarfBuzzInstance();
}

// static
std::unique_ptr<RenderText> RenderText::CreateInstanceDeprecated() {
  return CreateFor(Typesetter::BROWSER);
}

std::unique_ptr<RenderText> RenderText::CreateInstanceOfSameStyle(
    const base::string16& text) const {
  std::unique_ptr<RenderText> render_text = CreateInstanceOfSameType();
  // |SetText()| must be called before styles are set.
  render_text->SetText(text);
  render_text->SetFontList(font_list_);
  render_text->SetDirectionalityMode(directionality_mode_);
  render_text->SetCursorEnabled(cursor_enabled_);
  render_text->set_truncate_length(truncate_length_);
  render_text->styles_ = styles_;
  render_text->baselines_ = baselines_;
  render_text->font_size_overrides_ = font_size_overrides_;
  render_text->colors_ = colors_;
  render_text->weights_ = weights_;
  return render_text;
}

void RenderText::SetText(const base::string16& text) {
  DCHECK(!composition_range_.IsValid());
  if (text_ == text)
    return;
  text_ = text;
  UpdateStyleLengths();

  // Clear style ranges as they might break new text graphemes and apply
  // the first style to the whole text instead.
  colors_.SetValue(colors_.breaks().begin()->second);
  baselines_.SetValue(baselines_.breaks().begin()->second);
  font_size_overrides_.SetValue(font_size_overrides_.breaks().begin()->second);
  weights_.SetValue(weights_.breaks().begin()->second);
  for (size_t style = 0; style < NUM_TEXT_STYLES; ++style)
    styles_[style].SetValue(styles_[style].breaks().begin()->second);
  cached_bounds_and_offset_valid_ = false;

  // Reset selection model. SetText should always followed by SetSelectionModel
  // or SetCursorPosition in upper layer.
  SetSelectionModel(SelectionModel());

  // Invalidate the cached text direction if it depends on the text contents.
  if (directionality_mode_ == DIRECTIONALITY_FROM_TEXT)
    text_direction_ = base::i18n::UNKNOWN_DIRECTION;

  obscured_reveal_index_ = -1;
  OnTextAttributeChanged();
}

void RenderText::AppendText(const base::string16& text) {
  text_ += text;
  UpdateStyleLengths();
  cached_bounds_and_offset_valid_ = false;
  obscured_reveal_index_ = -1;
  OnTextAttributeChanged();
}

void RenderText::SetHorizontalAlignment(HorizontalAlignment alignment) {
  if (horizontal_alignment_ != alignment) {
    horizontal_alignment_ = alignment;
    display_offset_ = Vector2d();
    cached_bounds_and_offset_valid_ = false;
  }
}

void RenderText::SetFontList(const FontList& font_list) {
  font_list_ = font_list;
  const int font_style = font_list.GetFontStyle();
  weights_.SetValue(font_list.GetFontWeight());
  styles_[ITALIC].SetValue((font_style & Font::ITALIC) != 0);
  styles_[UNDERLINE].SetValue((font_style & Font::UNDERLINE) != 0);
  styles_[HEAVY_UNDERLINE].SetValue(false);
  baseline_ = kInvalidBaseline;
  cached_bounds_and_offset_valid_ = false;
  OnLayoutTextAttributeChanged(false);
}

void RenderText::SetCursorEnabled(bool cursor_enabled) {
  cursor_enabled_ = cursor_enabled;
  cached_bounds_and_offset_valid_ = false;
}

void RenderText::SetObscured(bool obscured) {
  if (obscured != obscured_) {
    obscured_ = obscured;
    obscured_reveal_index_ = -1;
    cached_bounds_and_offset_valid_ = false;
    OnTextAttributeChanged();
  }
}

void RenderText::SetObscuredRevealIndex(int index) {
  if (obscured_reveal_index_ == index)
    return;

  obscured_reveal_index_ = index;
  cached_bounds_and_offset_valid_ = false;
  OnTextAttributeChanged();
}

void RenderText::SetMultiline(bool multiline) {
  if (multiline != multiline_) {
    multiline_ = multiline;
    cached_bounds_and_offset_valid_ = false;
    lines_.clear();
    OnTextAttributeChanged();
  }
}

void RenderText::SetMaxLines(size_t max_lines) {
  max_lines_ = max_lines;
  OnDisplayTextAttributeChanged();
}

size_t RenderText::GetNumLines() {
  return lines_.size();
}

void RenderText::SetWordWrapBehavior(WordWrapBehavior behavior) {
  if (word_wrap_behavior_ == behavior)
    return;
  word_wrap_behavior_ = behavior;
  if (multiline_) {
    cached_bounds_and_offset_valid_ = false;
    lines_.clear();
    OnTextAttributeChanged();
  }
}

void RenderText::SetReplaceNewlineCharsWithSymbols(bool replace) {
  if (replace_newline_chars_with_symbols_ == replace)
    return;
  replace_newline_chars_with_symbols_ = replace;
  cached_bounds_and_offset_valid_ = false;
  OnTextAttributeChanged();
}

void RenderText::SetMinLineHeight(int line_height) {
  if (min_line_height_ == line_height)
    return;
  min_line_height_ = line_height;
  cached_bounds_and_offset_valid_ = false;
  lines_.clear();
  OnDisplayTextAttributeChanged();
}

void RenderText::SetElideBehavior(ElideBehavior elide_behavior) {
  // TODO(skanuj) : Add a test for triggering layout change.
  if (elide_behavior_ != elide_behavior) {
    elide_behavior_ = elide_behavior;
    OnDisplayTextAttributeChanged();
  }
}

void RenderText::SetDisplayRect(const Rect& r) {
  if (r != display_rect_) {
    display_rect_ = r;
    baseline_ = kInvalidBaseline;
    cached_bounds_and_offset_valid_ = false;
    lines_.clear();
    if (elide_behavior_ != NO_ELIDE &&
        elide_behavior_ != FADE_TAIL) {
      OnDisplayTextAttributeChanged();
    }
  }
}

void RenderText::SetCursorPosition(size_t position) {
  size_t cursor = std::min(position, text().length());
  if (IsValidCursorIndex(cursor)) {
    SetSelectionModel(SelectionModel(
        cursor, (cursor == 0) ? CURSOR_FORWARD : CURSOR_BACKWARD));
  }
}

void RenderText::MoveCursor(BreakType break_type,
                            VisualCursorDirection direction,
                            SelectionBehavior selection_behavior) {
  SelectionModel cursor(cursor_position(), selection_model_.caret_affinity());

  // Ensure |cursor| is at the "end" of the current selection, since this
  // determines which side should grow or shrink. If the prior change to the
  // selection wasn't from cursor movement, the selection may be undirected. Or,
  // the selection may be collapsing. In these cases, pick the "end" using
  // |direction| (e.g. the arrow key) rather than the current selection range.
  if ((!has_directed_selection_ || selection_behavior == SELECTION_NONE) &&
      !selection().is_empty()) {
    SelectionModel start = GetSelectionModelForSelectionStart();
    int start_x = GetCursorBounds(start, true).x();
    int end_x = GetCursorBounds(cursor, true).x();

    // Use the selection start if it is left (when |direction| is CURSOR_LEFT)
    // or right (when |direction| is CURSOR_RIGHT) of the selection end.
    if (direction == CURSOR_RIGHT ? start_x > end_x : start_x < end_x) {
      // In this case, a direction has been chosen that doesn't match
      // |selection_model|, so the range must be reversed to place the cursor at
      // the other end. Note the affinity won't matter: only the affinity of
      // |start| (which points "in" to the selection) determines the movement.
      Range range = selection_model_.selection();
      selection_model_ = SelectionModel(Range(range.end(), range.start()),
                                        selection_model_.caret_affinity());
      cursor = start;
    }
  }

  // Cancelling a selection moves to the edge of the selection.
  if (break_type != LINE_BREAK && !selection().is_empty() &&
      selection_behavior == SELECTION_NONE) {
    // Use the nearest word boundary in the proper |direction| for word breaks.
    if (break_type == WORD_BREAK)
      cursor = GetAdjacentSelectionModel(cursor, break_type, direction);
    // Use an adjacent selection model if the cursor is not at a valid position.
    if (!IsValidCursorIndex(cursor.caret_pos()))
      cursor = GetAdjacentSelectionModel(cursor, CHARACTER_BREAK, direction);
  } else {
    cursor = GetAdjacentSelectionModel(cursor, break_type, direction);
  }

  // |cursor| corresponds to the tentative end point of the new selection. The
  // selection direction is reversed iff the current selection is non-empty and
  // the old selection end point and |cursor| are at the opposite ends of the
  // old selection start point.
  uint32_t min_end = std::min(selection().end(), cursor.selection().end());
  uint32_t max_end = std::max(selection().end(), cursor.selection().end());
  uint32_t current_start = selection().start();

  bool selection_reversed = !selection().is_empty() &&
                            min_end <= current_start &&
                            current_start <= max_end;

  // Take |selection_behavior| into account.
  switch (selection_behavior) {
    case SELECTION_RETAIN:
      cursor.set_selection_start(current_start);
      break;
    case SELECTION_EXTEND:
      cursor.set_selection_start(selection_reversed ? selection().end()
                                                    : current_start);
      break;
    case SELECTION_CARET:
      if (selection_reversed) {
        cursor =
            SelectionModel(current_start, selection_model_.caret_affinity());
      } else {
        cursor.set_selection_start(current_start);
      }
      break;
    case SELECTION_NONE:
      // Do nothing.
      break;
  }

  SetSelection(cursor);
  has_directed_selection_ = true;
}

bool RenderText::SetSelection(const SelectionModel& model) {
  // Enforce valid selection model components.
  size_t text_length = text().length();
  Range range(
      std::min(model.selection().start(), static_cast<uint32_t>(text_length)),
      std::min(model.caret_pos(), text_length));
  // The current model only supports caret positions at valid cursor indices.
  if (!IsValidCursorIndex(range.start()) || !IsValidCursorIndex(range.end()))
    return false;
  SelectionModel sel(range, model.caret_affinity());
  bool changed = sel != selection_model_;
  SetSelectionModel(sel);
  return changed;
}

bool RenderText::MoveCursorToPoint(const gfx::Point& point, bool select) {
  gfx::SelectionModel model = FindCursorPosition(point);
  if (select)
    model.set_selection_start(selection().start());
  return SetSelection(model);
}

bool RenderText::SelectRange(const Range& range) {
  uint32_t text_length = static_cast<uint32_t>(text().length());
  Range sel(std::min(range.start(), text_length),
            std::min(range.end(), text_length));
  // Allow selection bounds at valid indicies amid multi-character graphemes.
  if (!IsValidLogicalIndex(sel.start()) || !IsValidLogicalIndex(sel.end()))
    return false;
  LogicalCursorDirection affinity =
      (sel.is_reversed() || sel.is_empty()) ? CURSOR_FORWARD : CURSOR_BACKWARD;
  SetSelectionModel(SelectionModel(sel, affinity));
  return true;
}

bool RenderText::IsPointInSelection(const Point& point) {
  if (selection().is_empty())
    return false;
  SelectionModel cursor = FindCursorPosition(point);
  return RangeContainsCaret(
      selection(), cursor.caret_pos(), cursor.caret_affinity());
}

void RenderText::ClearSelection() {
  SetSelectionModel(
      SelectionModel(cursor_position(), selection_model_.caret_affinity()));
}

void RenderText::SelectAll(bool reversed) {
  const size_t length = text().length();
  const Range all = reversed ? Range(length, 0) : Range(0, length);
  const bool success = SelectRange(all);
  DCHECK(success);
}

void RenderText::SelectWord() {
  SelectRange(ExpandRangeToWordBoundary(selection()));
}

void RenderText::SetCompositionRange(const Range& composition_range) {
  CHECK(!composition_range.IsValid() ||
        Range(0, text_.length()).Contains(composition_range));
  composition_range_.set_end(composition_range.end());
  composition_range_.set_start(composition_range.start());
  // TODO(oshima|msw): Altering composition underlines shouldn't
  // require layout changes. It's currently necessary because
  // RenderTextHarfBuzz paints text decorations by run, and
  // RenderTextMac applies all styles during layout.
  OnLayoutTextAttributeChanged(false);
}

void RenderText::SetColor(SkColor value) {
  colors_.SetValue(value);
  OnTextColorChanged();
}

void RenderText::ApplyColor(SkColor value, const Range& range) {
  colors_.ApplyValue(value, range);
  OnTextColorChanged();
}

void RenderText::SetBaselineStyle(BaselineStyle value) {
  baselines_.SetValue(value);
}

void RenderText::ApplyBaselineStyle(BaselineStyle value, const Range& range) {
  baselines_.ApplyValue(value, range);
}

void RenderText::ApplyFontSizeOverride(int font_size_override,
                                       const Range& range) {
  font_size_overrides_.ApplyValue(font_size_override, range);
}

void RenderText::SetStyle(TextStyle style, bool value) {
  styles_[style].SetValue(value);

  cached_bounds_and_offset_valid_ = false;
  // TODO(oshima|msw): Not all style change requires layout changes.
  // Consider optimizing based on the type of change.
  OnLayoutTextAttributeChanged(false);
}

void RenderText::ApplyStyle(TextStyle style, bool value, const Range& range) {
  // Do not change styles mid-grapheme to avoid breaking ligatures.
  const size_t start = IsValidCursorIndex(range.start()) ? range.start() :
      IndexOfAdjacentGrapheme(range.start(), CURSOR_BACKWARD);
  const size_t end = IsValidCursorIndex(range.end()) ? range.end() :
      IndexOfAdjacentGrapheme(range.end(), CURSOR_FORWARD);
  styles_[style].ApplyValue(value, Range(start, end));

  cached_bounds_and_offset_valid_ = false;
  // TODO(oshima|msw): Not all style change requires layout changes.
  // Consider optimizing based on the type of change.
  OnLayoutTextAttributeChanged(false);
}

void RenderText::SetWeight(Font::Weight weight) {
  weights_.SetValue(weight);

  cached_bounds_and_offset_valid_ = false;
  OnLayoutTextAttributeChanged(false);
}

void RenderText::ApplyWeight(Font::Weight weight, const Range& range) {
  weights_.ApplyValue(weight, range);

  cached_bounds_and_offset_valid_ = false;
  OnLayoutTextAttributeChanged(false);
}

bool RenderText::GetStyle(TextStyle style) const {
  return (styles_[style].breaks().size() == 1) &&
      styles_[style].breaks().front().second;
}

void RenderText::SetDirectionalityMode(DirectionalityMode mode) {
  if (mode == directionality_mode_)
    return;

  directionality_mode_ = mode;
  text_direction_ = base::i18n::UNKNOWN_DIRECTION;
  cached_bounds_and_offset_valid_ = false;
  OnLayoutTextAttributeChanged(false);
}

base::i18n::TextDirection RenderText::GetDisplayTextDirection() {
  return GetTextDirection(GetDisplayText());
}

VisualCursorDirection RenderText::GetVisualDirectionOfLogicalEnd() {
  return GetDisplayTextDirection() == base::i18n::LEFT_TO_RIGHT ? CURSOR_RIGHT
                                                                : CURSOR_LEFT;
}

VisualCursorDirection RenderText::GetVisualDirectionOfLogicalBeginning() {
  return GetDisplayTextDirection() == base::i18n::RIGHT_TO_LEFT ? CURSOR_RIGHT
                                                                : CURSOR_LEFT;
}

SizeF RenderText::GetStringSizeF() {
  return SizeF(GetStringSize());
}

float RenderText::GetContentWidthF() {
  const float string_size = GetStringSizeF().width();
  // The cursor is drawn one pixel beyond the int-enclosed text bounds.
  return cursor_enabled_ ? std::ceil(string_size) + 1 : string_size;
}

int RenderText::GetContentWidth() {
  return ToCeiledInt(GetContentWidthF());
}

int RenderText::GetBaseline() {
  if (baseline_ == kInvalidBaseline) {
    baseline_ =
        DetermineBaselineCenteringText(display_rect().height(), font_list());
  }
  DCHECK_NE(kInvalidBaseline, baseline_);
  return baseline_;
}

void RenderText::Draw(Canvas* canvas) {
  EnsureLayout();

  if (clip_to_display_rect()) {
    Rect clip_rect(display_rect());
    clip_rect.Inset(ShadowValue::GetMargin(shadows_));

    canvas->Save();
    canvas->ClipRect(clip_rect);
  }

  if (!text().empty() && focused())
    DrawSelection(canvas);

  if (!text().empty()) {
    internal::SkiaTextRenderer renderer(canvas);
    DrawVisualText(&renderer);
  }

  if (clip_to_display_rect())
    canvas->Restore();
}

bool RenderText::IsValidLogicalIndex(size_t index) const {
  // Check that the index is at a valid code point (not mid-surrgate-pair) and
  // that it's not truncated from the display text (its glyph may be shown).
  //
  // Indices within truncated text are disallowed so users can easily interact
  // with the underlying truncated text using the ellipsis as a proxy. This lets
  // users select all text, select the truncated text, and transition from the
  // last rendered glyph to the end of the text without getting invisible cursor
  // positions nor needing unbounded arrow key presses to traverse the ellipsis.
  return index == 0 || index == text().length() ||
      (index < text().length() &&
       (truncate_length_ == 0 || index < truncate_length_) &&
       IsValidCodePointIndex(text(), index));
}

Rect RenderText::GetCursorBounds(const SelectionModel& caret,
                                 bool insert_mode) {
  // TODO(ckocagil): Support multiline. This function should return the height
  //                 of the line the cursor is on. |GetStringSize()| now returns
  //                 the multiline size, eliminate its use here.
  DCHECK(!multiline_);
  EnsureLayout();
  size_t caret_pos = caret.caret_pos();
  DCHECK(IsValidLogicalIndex(caret_pos));
  // In overtype mode, ignore the affinity and always indicate that we will
  // overtype the next character.
  LogicalCursorDirection caret_affinity =
      insert_mode ? caret.caret_affinity() : CURSOR_FORWARD;
  int x = 0, width = 1;
  Size size = GetStringSize();

  // Check whether the caret is attached to a boundary. Always return a 1-dip
  // width caret at the boundary. Avoid calling IndexOfAdjacentGrapheme(), since
  // it is slow and can impact browser startup here.
  // In insert mode, index 0 is always a boundary. The end, however, is not at a
  // boundary when the string ends in RTL text and there is LTR text around it.
  const bool at_boundary =
      (insert_mode && caret_pos == 0) ||
      caret_pos == (caret_affinity == CURSOR_BACKWARD ? 0 : text().length());
  if (at_boundary) {
    const bool rtl = GetDisplayTextDirection() == base::i18n::RIGHT_TO_LEFT;
    if (rtl == (caret_pos == 0))
      x = size.width();
  } else {
    // Find the next grapheme continuing in the current direction. This
    // determines the substring range that should be highlighted.
    size_t caret_end = IndexOfAdjacentGrapheme(caret_pos, caret_affinity);
    if (caret_end < caret_pos)
      std::swap(caret_end, caret_pos);
    const Range xspan = GetCursorSpan(Range(caret_pos, caret_end));
    if (insert_mode) {
      x = (caret_affinity == CURSOR_BACKWARD) ? xspan.end() : xspan.start();
    } else {  // overtype mode
      x = xspan.GetMin();
      width = xspan.length();
    }
  }
  return Rect(ToViewPoint(Point(x, 0)), Size(width, size.height()));
}

const Rect& RenderText::GetUpdatedCursorBounds() {
  UpdateCachedBoundsAndOffset();
  return cursor_bounds_;
}

size_t RenderText::IndexOfAdjacentGrapheme(size_t index,
                                           LogicalCursorDirection direction) {
  if (index > text().length())
    return text().length();

  EnsureLayout();

  if (direction == CURSOR_FORWARD) {
    while (index < text().length()) {
      index++;
      if (IsValidCursorIndex(index))
        return index;
    }
    return text().length();
  }

  while (index > 0) {
    index--;
    if (IsValidCursorIndex(index))
      return index;
  }
  return 0;
}

SelectionModel RenderText::GetSelectionModelForSelectionStart() const {
  const Range& sel = selection();
  if (sel.is_empty())
    return selection_model_;
  return SelectionModel(sel.start(),
                        sel.is_reversed() ? CURSOR_BACKWARD : CURSOR_FORWARD);
}

RectF RenderText::GetStringRect() {
  return RectF(PointF(ToViewPoint(Point())), GetStringSizeF());
}

const Vector2d& RenderText::GetUpdatedDisplayOffset() {
  UpdateCachedBoundsAndOffset();
  return display_offset_;
}

void RenderText::SetDisplayOffset(int horizontal_offset) {
  const int extra_content = GetContentWidth() - display_rect_.width();
  const int cursor_width = cursor_enabled_ ? 1 : 0;

  int min_offset = 0;
  int max_offset = 0;
  if (extra_content > 0) {
    switch (GetCurrentHorizontalAlignment()) {
      case ALIGN_LEFT:
        min_offset = -extra_content;
        break;
      case ALIGN_RIGHT:
        max_offset = extra_content;
        break;
      case ALIGN_CENTER:
        // The extra space reserved for cursor at the end of the text is ignored
        // when centering text. So, to calculate the valid range for offset, we
        // exclude that extra space, calculate the range, and add it back to the
        // range (if cursor is enabled).
        min_offset = -(extra_content - cursor_width + 1) / 2 - cursor_width;
        max_offset = (extra_content - cursor_width) / 2;
        break;
      default:
        break;
    }
  }
  if (horizontal_offset < min_offset)
    horizontal_offset = min_offset;
  else if (horizontal_offset > max_offset)
    horizontal_offset = max_offset;

  cached_bounds_and_offset_valid_ = true;
  display_offset_.set_x(horizontal_offset);
  cursor_bounds_ = GetCursorBounds(selection_model_, true);
}

Vector2d RenderText::GetLineOffset(size_t line_number) {
  EnsureLayout();
  Vector2d offset = display_rect().OffsetFromOrigin();
  // TODO(ckocagil): Apply the display offset for multiline scrolling.
  if (!multiline()) {
    offset.Add(GetUpdatedDisplayOffset());
  } else {
    DCHECK_LT(line_number, lines().size());
    offset.Add(Vector2d(0, lines_[line_number].preceding_heights));
  }
  offset.Add(GetAlignmentOffset(line_number));
  return offset;
}

bool RenderText::GetWordLookupDataAtPoint(const Point& point,
                                          DecoratedText* decorated_word,
                                          Point* baseline_point) {
  if (obscured())
    return false;

  EnsureLayout();
  const SelectionModel model_at_point = FindCursorPosition(point);
  const size_t word_index =
      GetNearestWordStartBoundary(model_at_point.caret_pos());
  if (word_index >= text().length())
    return false;

  const Range word_range = ExpandRangeToWordBoundary(Range(word_index));
  DCHECK(!word_range.is_reversed());
  DCHECK(!word_range.is_empty());

  return GetLookupDataForRange(word_range, decorated_word, baseline_point);
}

bool RenderText::GetLookupDataForRange(const Range& range,
                                       DecoratedText* decorated_text,
                                       Point* baseline_point) {
  EnsureLayout();

  const std::vector<Rect> word_bounds = GetSubstringBounds(range);
  if (word_bounds.empty() || !GetDecoratedTextForRange(range, decorated_text)) {
    return false;
  }

  // Retrieve the baseline origin of the left-most glyph.
  const auto left_rect = std::min_element(
      word_bounds.begin(), word_bounds.end(),
      [](const Rect& lhs, const Rect& rhs) { return lhs.x() < rhs.x(); });
  const int line_index = GetLineContainingYCoord(left_rect->CenterPoint().y() -
                                                 GetLineOffset(0).y());
  if (line_index < 0 || line_index >= static_cast<int>(lines().size()))
    return false;
  *baseline_point =
      left_rect->origin() + Vector2d(0, lines()[line_index].baseline);
  return true;
}

base::string16 RenderText::GetTextFromRange(const Range& range) const {
  if (range.IsValid() && range.GetMin() < text().length())
    return text().substr(range.GetMin(), range.length());
  return base::string16();
}

RenderText::RenderText()
    : horizontal_alignment_(base::i18n::IsRTL() ? ALIGN_RIGHT : ALIGN_LEFT),
      directionality_mode_(DIRECTIONALITY_FROM_TEXT),
      text_direction_(base::i18n::UNKNOWN_DIRECTION),
      cursor_enabled_(true),
      has_directed_selection_(kSelectionIsAlwaysDirected),
      selection_color_(kDefaultColor),
      selection_background_focused_color_(kDefaultSelectionBackgroundColor),
      focused_(false),
      composition_range_(Range::InvalidRange()),
      colors_(kDefaultColor),
      baselines_(NORMAL_BASELINE),
      font_size_overrides_(0),
      weights_(Font::Weight::NORMAL),
      styles_(NUM_TEXT_STYLES),
      composition_and_selection_styles_applied_(false),
      obscured_(false),
      obscured_reveal_index_(-1),
      truncate_length_(0),
      elide_behavior_(NO_ELIDE),
      text_elided_(false),
      min_line_height_(0),
      multiline_(false),
      max_lines_(0),
      word_wrap_behavior_(IGNORE_LONG_WORDS),
      replace_newline_chars_with_symbols_(true),
      subpixel_rendering_suppressed_(false),
      clip_to_display_rect_(true),
      baseline_(kInvalidBaseline),
      cached_bounds_and_offset_valid_(false),
      strike_thickness_factor_(kLineThicknessFactor) {}

bool RenderText::IsHomogeneous() const {
  if (colors().breaks().size() > 1 || baselines().breaks().size() > 1 ||
      font_size_overrides().breaks().size() > 1 ||
      weights().breaks().size() > 1)
    return false;
  for (size_t style = 0; style < NUM_TEXT_STYLES; ++style) {
    if (styles()[style].breaks().size() > 1)
      return false;
  }
  return true;
}

SelectionModel RenderText::GetAdjacentSelectionModel(
    const SelectionModel& current,
    BreakType break_type,
    VisualCursorDirection direction) {
  EnsureLayout();

  if (break_type == LINE_BREAK || text().empty())
    return EdgeSelectionModel(direction);
  if (break_type == CHARACTER_BREAK)
    return AdjacentCharSelectionModel(current, direction);
  DCHECK(break_type == WORD_BREAK);
  return AdjacentWordSelectionModel(current, direction);
}

SelectionModel RenderText::EdgeSelectionModel(
    VisualCursorDirection direction) {
  if (direction == GetVisualDirectionOfLogicalEnd())
    return SelectionModel(text().length(), CURSOR_FORWARD);
  return SelectionModel(0, CURSOR_BACKWARD);
}

SelectionModel RenderText::LineSelectionModel(size_t line_index,
                                              VisualCursorDirection direction) {
  const internal::Line& line = lines()[line_index];
  if (line.segments.empty()) {
    // Only the last line can be empty.
    DCHECK_EQ(lines().size() - 1, line_index);
    return EdgeSelectionModel(GetVisualDirectionOfLogicalEnd());
  }

  size_t max_index = 0;
  size_t min_index = text().length();
  for (const auto& segment : line.segments) {
    min_index = std::min<size_t>(min_index, segment.char_range.GetMin());
    max_index = std::max<size_t>(max_index, segment.char_range.GetMax());
  }

  return direction == GetVisualDirectionOfLogicalEnd()
             ? SelectionModel(DisplayIndexToTextIndex(max_index),
                              CURSOR_FORWARD)
             : SelectionModel(DisplayIndexToTextIndex(min_index),
                              CURSOR_BACKWARD);
}

void RenderText::SetSelectionModel(const SelectionModel& model) {
  DCHECK_LE(model.selection().GetMax(), text().length());
  selection_model_ = model;
  cached_bounds_and_offset_valid_ = false;
  has_directed_selection_ = kSelectionIsAlwaysDirected;
}

void RenderText::OnTextColorChanged() {
}

void RenderText::UpdateDisplayText(float text_width) {
  // TODO(krb): Consider other elision modes for multiline.
  if ((multiline_ && (!max_lines_ || elide_behavior() != ELIDE_TAIL)) ||
      elide_behavior() == NO_ELIDE || elide_behavior() == FADE_TAIL ||
      (text_width > 0 && text_width < display_rect_.width()) ||
      layout_text_.empty()) {
    text_elided_ = false;
    display_text_.clear();
    return;
  }

  if (!multiline_) {
    // This doesn't trim styles so ellipsis may get rendered as a different
    // style than the preceding text. See crbug.com/327850.
    display_text_.assign(Elide(layout_text_, text_width,
                               static_cast<float>(display_rect_.width()),
                               elide_behavior_));
  } else {
    bool was_elided = text_elided_;
    text_elided_ = false;
    display_text_.clear();

    std::unique_ptr<RenderText> render_text(
        CreateInstanceOfSameStyle(layout_text_));
    render_text->SetMultiline(true);
    render_text->SetWordWrapBehavior(word_wrap_behavior_);
    render_text->SetDisplayRect(display_rect_);
    // Have it arrange words on |lines_|.
    render_text->EnsureLayout();

    if (render_text->lines_.size() > max_lines_) {
      size_t start_of_elision = render_text->lines_[max_lines_ - 1]
                                    .segments.front()
                                    .char_range.start();
      base::string16 text_to_elide = layout_text_.substr(start_of_elision);
      display_text_.assign(layout_text_.substr(0, start_of_elision) +
                           Elide(text_to_elide, 0,
                                 static_cast<float>(display_rect_.width()),
                                 ELIDE_TAIL));
      // Have GetLineBreaks() re-calculate.
      line_breaks_.SetMax(0);
    } else {
      // If elision changed, re-calculate.
      if (was_elided)
        line_breaks_.SetMax(0);
      // Initial state above is fine.
      return;
    }
  }
  text_elided_ = display_text_ != layout_text_;
  if (!text_elided_)
    display_text_.clear();
}

const BreakList<size_t>& RenderText::GetLineBreaks() {
  if (line_breaks_.max() != 0)
    return line_breaks_;

  const base::string16& layout_text = GetDisplayText();
  const size_t text_length = layout_text.length();
  line_breaks_.SetValue(0);
  line_breaks_.SetMax(text_length);
  base::i18n::BreakIterator iter(layout_text,
                                 base::i18n::BreakIterator::BREAK_LINE);
  const bool success = iter.Init();
  DCHECK(success);
  if (success) {
    do {
      line_breaks_.ApplyValue(iter.pos(), Range(iter.pos(), text_length));
    } while (iter.Advance());
  }
  return line_breaks_;
}

void RenderText::ApplyCompositionAndSelectionStyles() {
  // Save the underline and color breaks to undo the temporary styles later.
  DCHECK(!composition_and_selection_styles_applied_);
  saved_colors_ = colors_;
  saved_underlines_ = styles_[HEAVY_UNDERLINE];

  // Apply an underline to the composition range in |underlines|.
  if (composition_range_.IsValid() && !composition_range_.is_empty())
    styles_[HEAVY_UNDERLINE].ApplyValue(true, composition_range_);

  // Apply the selected text color to the [un-reversed] selection range.
  if (!selection().is_empty() && focused()) {
    const Range range(selection().GetMin(), selection().GetMax());
    colors_.ApplyValue(selection_color_, range);
  }
  composition_and_selection_styles_applied_ = true;
}

void RenderText::UndoCompositionAndSelectionStyles() {
  // Restore the underline and color breaks to undo the temporary styles.
  DCHECK(composition_and_selection_styles_applied_);
  colors_ = saved_colors_;
  styles_[HEAVY_UNDERLINE] = saved_underlines_;
  composition_and_selection_styles_applied_ = false;
}

Point RenderText::ToViewPoint(const Point& point) {
  if (!multiline())
    return point + GetLineOffset(0);

  // TODO(ckocagil): Traverse individual line segments for RTL support.
  DCHECK(!lines_.empty());
  int x = point.x();
  size_t line = 0;
  for (; line < lines_.size() && x > lines_[line].size.width(); ++line)
    x -= lines_[line].size.width();

  // If |point| is outside the text space, clip it to the end of the last line.
  if (line == lines_.size())
    x = lines_[--line].size.width();
  return Point(x, point.y()) + GetLineOffset(line);
}

HorizontalAlignment RenderText::GetCurrentHorizontalAlignment() {
  if (horizontal_alignment_ != ALIGN_TO_HEAD)
    return horizontal_alignment_;
  return GetDisplayTextDirection() == base::i18n::RIGHT_TO_LEFT ?
      ALIGN_RIGHT : ALIGN_LEFT;
}

Vector2d RenderText::GetAlignmentOffset(size_t line_number) {
  // TODO(ckocagil): Enable |lines_| usage on RenderTextMac.
  if (MultilineSupported() && multiline_)
    DCHECK_LT(line_number, lines_.size());
  Vector2d offset;
  HorizontalAlignment horizontal_alignment = GetCurrentHorizontalAlignment();
  if (horizontal_alignment != ALIGN_LEFT) {
    const int width = multiline_ ?
        std::ceil(lines_[line_number].size.width()) +
        (cursor_enabled_ ? 1 : 0) :
        GetContentWidth();
    offset.set_x(display_rect().width() - width);
    // Put any extra margin pixel on the left to match legacy behavior.
    if (horizontal_alignment == ALIGN_CENTER)
      offset.set_x((offset.x() + 1) / 2);
  }

  // Vertically center the text.
  if (multiline_) {
    const int text_height = lines_.back().preceding_heights +
        lines_.back().size.height();
    offset.set_y((display_rect_.height() - text_height) / 2);
  } else {
    offset.set_y(GetBaseline() - GetDisplayTextBaseline());
  }

  return offset;
}

void RenderText::ApplyFadeEffects(internal::SkiaTextRenderer* renderer) {
  const int width = display_rect().width();
  if (multiline() || elide_behavior_ != FADE_TAIL || GetContentWidth() <= width)
    return;

  const int gradient_width = CalculateFadeGradientWidth(font_list(), width);
  if (gradient_width == 0)
    return;

  HorizontalAlignment horizontal_alignment = GetCurrentHorizontalAlignment();
  Rect solid_part = display_rect();
  Rect left_part;
  Rect right_part;
  if (horizontal_alignment != ALIGN_LEFT) {
    left_part = solid_part;
    left_part.Inset(0, 0, solid_part.width() - gradient_width, 0);
    solid_part.Inset(gradient_width, 0, 0, 0);
  }
  if (horizontal_alignment != ALIGN_RIGHT) {
    right_part = solid_part;
    right_part.Inset(solid_part.width() - gradient_width, 0, 0, 0);
    solid_part.Inset(0, 0, gradient_width, 0);
  }

  // CreateFadeShader() expects at least one part to not be empty.
  // See https://crbug.com/706835.
  if (left_part.IsEmpty() && right_part.IsEmpty())
    return;

  Rect text_rect = display_rect();
  text_rect.Inset(GetAlignmentOffset(0).x(), 0, 0, 0);

  // TODO(msw): Use the actual text colors corresponding to each faded part.
  renderer->SetShader(
      CreateFadeShader(font_list(), text_rect, left_part, right_part,
                       SkColorSetA(colors_.breaks().front().second, 0xff)));
}

void RenderText::ApplyTextShadows(internal::SkiaTextRenderer* renderer) {
  renderer->SetDrawLooper(CreateShadowDrawLooper(shadows_));
}

base::i18n::TextDirection RenderText::GetTextDirection(
    const base::string16& text) {
  if (text_direction_ == base::i18n::UNKNOWN_DIRECTION) {
    switch (directionality_mode_) {
      case DIRECTIONALITY_FROM_TEXT:
        // Derive the direction from the display text, which differs from text()
        // in the case of obscured (password) textfields.
        text_direction_ =
            base::i18n::GetFirstStrongCharacterDirection(text);
        break;
      case DIRECTIONALITY_FROM_UI:
        text_direction_ = base::i18n::IsRTL() ? base::i18n::RIGHT_TO_LEFT :
                                                base::i18n::LEFT_TO_RIGHT;
        break;
      case DIRECTIONALITY_FORCE_LTR:
        text_direction_ = base::i18n::LEFT_TO_RIGHT;
        break;
      case DIRECTIONALITY_FORCE_RTL:
        text_direction_ = base::i18n::RIGHT_TO_LEFT;
        break;
      case DIRECTIONALITY_AS_URL:
        // Rendering as a URL implies left-to-right paragraph direction.
        // URL Standard specifies that a URL "should be rendered as if it were
        // in a left-to-right embedding".
        // https://url.spec.whatwg.org/#url-rendering
        //
        // Consider logical string for domain "ABC.com/hello" (where ABC are
        // Hebrew (RTL) characters). The normal Bidi algorithm renders this as
        // "com/hello.CBA"; by forcing LTR, it is rendered as "CBA.com/hello".
        //
        // Note that this only applies a LTR embedding at the top level; it
        // doesn't change the Bidi algorithm, so there are still some URLs that
        // will render in a confusing order. Consider the logical string
        // "abc.COM/HELLO/world", which will render as "abc.OLLEH/MOC/world".
        // See https://crbug.com/351639.
        //
        // Note that the LeftToRightUrls feature flag enables additional
        // behaviour for DIRECTIONALITY_AS_URL, but the left-to-right embedding
        // behaviour is always enabled, regardless of the flag.
        text_direction_ = base::i18n::LEFT_TO_RIGHT;
        break;
      default:
        NOTREACHED();
    }
  }

  return text_direction_;
}

size_t RenderText::TextIndexToGivenTextIndex(const base::string16& given_text,
                                             size_t index) const {
  DCHECK(given_text == layout_text() || given_text == display_text());
  DCHECK_LE(index, text().length());
  ptrdiff_t i = obscured() ? UTF16IndexToOffset(text(), 0, index) : index;
  CHECK_GE(i, 0);
  // Clamp indices to the length of the given layout or display text.
  return std::min<size_t>(given_text.length(), i);
}

void RenderText::UpdateStyleLengths() {
  const size_t text_length = text_.length();
  colors_.SetMax(text_length);
  baselines_.SetMax(text_length);
  font_size_overrides_.SetMax(text_length);
  weights_.SetMax(text_length);
  for (size_t style = 0; style < NUM_TEXT_STYLES; ++style)
    styles_[style].SetMax(text_length);
}

int RenderText::GetLineContainingYCoord(float text_y) {
  if (text_y < 0)
    return -1;

  for (size_t i = 0; i < lines().size(); i++) {
    const internal::Line& line = lines()[i];

    if (text_y <= line.size.height())
      return i;
    text_y -= line.size.height();
  }

  return lines().size();
}

// static
bool RenderText::RangeContainsCaret(const Range& range,
                                    size_t caret_pos,
                                    LogicalCursorDirection caret_affinity) {
  // NB: exploits unsigned wraparound (WG14/N1124 section 6.2.5 paragraph 9).
  size_t adjacent = (caret_affinity == CURSOR_BACKWARD) ?
      caret_pos - 1 : caret_pos + 1;
  return range.Contains(Range(caret_pos, adjacent));
}

// static
int RenderText::DetermineBaselineCenteringText(const int display_height,
                                               const FontList& font_list) {
  const int font_height = font_list.GetHeight();
  // Lower and upper bound of baseline shift as we try to show as much area of
  // text as possible.  In particular case of |display_height| == |font_height|,
  // we do not want to shift the baseline.
  const int min_shift = std::min(0, display_height - font_height);
  const int max_shift = std::abs(display_height - font_height);
  const int baseline = font_list.GetBaseline();
  const int cap_height = font_list.GetCapHeight();
  const int internal_leading = baseline - cap_height;
  // Some platforms don't support getting the cap height, and simply return
  // the entire font ascent from GetCapHeight().  Centering the ascent makes
  // the font look too low, so if GetCapHeight() returns the ascent, center
  // the entire font height instead.
  const int space =
      display_height - ((internal_leading != 0) ? cap_height : font_height);
  const int baseline_shift = space / 2 - internal_leading;
  return baseline + std::max(min_shift, std::min(max_shift, baseline_shift));
}

// static
gfx::Rect RenderText::ExpandToBeVerticallySymmetric(
    const gfx::Rect& rect,
    const gfx::Rect& display_rect) {
  // Mirror |rect| accross the horizontal line dividing |display_rect| in half.
  gfx::Rect result = rect;
  int mid_y = display_rect.CenterPoint().y();
  // The top of the mirror rect must be equidistant with the bottom of the
  // original rect from the mid-line.
  result.set_y(mid_y + (mid_y - rect.bottom()));

  // Now make a union with the original rect to ensure we are encompassing both.
  result.Union(rect);
  return result;
}

void RenderText::OnTextAttributeChanged() {
  layout_text_.clear();
  display_text_.clear();
  text_elided_ = false;
  line_breaks_.SetMax(0);

  if (obscured_) {
    size_t obscured_text_length =
        static_cast<size_t>(UTF16IndexToOffset(text_, 0, text_.length()));
    layout_text_.assign(obscured_text_length, kPasswordReplacementChar);

    if (obscured_reveal_index_ >= 0 &&
        obscured_reveal_index_ < static_cast<int>(text_.length())) {
      // Gets the index range in |text_| to be revealed.
      size_t start = obscured_reveal_index_;
      U16_SET_CP_START(text_.data(), 0, start);
      size_t end = start;
      UChar32 unused_char;
      U16_NEXT(text_.data(), end, text_.length(), unused_char);

      // Gets the index in |layout_text_| to be replaced.
      const size_t cp_start =
          static_cast<size_t>(UTF16IndexToOffset(text_, 0, start));
      if (layout_text_.length() > cp_start)
        layout_text_.replace(cp_start, 1, text_.substr(start, end - start));
    }
  } else {
    layout_text_ = text_;
  }

  const base::string16& text = layout_text_;
  if (truncate_length_ > 0 && truncate_length_ < text.length()) {
    // Truncate the text at a valid character break and append an ellipsis.
    icu::StringCharacterIterator iter(text.c_str());
    // Respect ELIDE_HEAD and ELIDE_MIDDLE preferences during truncation.
    if (elide_behavior_ == ELIDE_HEAD) {
      iter.setIndex32(text.length() - truncate_length_ + 1);
      layout_text_.assign(kEllipsisUTF16 + text.substr(iter.getIndex()));
    } else if (elide_behavior_ == ELIDE_MIDDLE) {
      iter.setIndex32(truncate_length_ / 2);
      const size_t ellipsis_start = iter.getIndex();
      iter.setIndex32(text.length() - (truncate_length_ / 2));
      const size_t ellipsis_end = iter.getIndex();
      DCHECK_LE(ellipsis_start, ellipsis_end);
      layout_text_.assign(text.substr(0, ellipsis_start) + kEllipsisUTF16 +
                          text.substr(ellipsis_end));
    } else {
      iter.setIndex32(truncate_length_ - 1);
      layout_text_.assign(text.substr(0, iter.getIndex()) + kEllipsisUTF16);
    }
  }
  static const base::char16 kNewline[] = { '\n', 0 };
  static const base::char16 kNewlineSymbol[] = { 0x2424, 0 };
  if (!multiline_ && replace_newline_chars_with_symbols_)
    base::ReplaceChars(layout_text_, kNewline, kNewlineSymbol, &layout_text_);

  OnLayoutTextAttributeChanged(true);
}

base::string16 RenderText::Elide(const base::string16& text,
                                 float text_width,
                                 float available_width,
                                 ElideBehavior behavior) {
  if (available_width <= 0 || text.empty())
    return base::string16();
  if (behavior == ELIDE_EMAIL)
    return ElideEmail(text, available_width);
  if (text_width > 0 && text_width <= available_width)
    return text;

  TRACE_EVENT0("ui", "RenderText::Elide");

  // Create a RenderText copy with attributes that affect the rendering width.
  std::unique_ptr<RenderText> render_text = CreateInstanceOfSameStyle(text);
  render_text->UpdateStyleLengths();
  if (text_width == 0)
    text_width = render_text->GetContentWidthF();
  if (text_width <= available_width)
    return text;

  const base::string16 ellipsis = base::string16(kEllipsisUTF16);
  const bool insert_ellipsis = (behavior != TRUNCATE);
  const bool elide_in_middle = (behavior == ELIDE_MIDDLE);
  const bool elide_at_beginning = (behavior == ELIDE_HEAD);

  if (insert_ellipsis) {
    render_text->SetText(ellipsis);
    const float ellipsis_width = render_text->GetContentWidthF();
    if (ellipsis_width > available_width)
      return base::string16();
  }

  StringSlicer slicer(text, ellipsis, elide_in_middle, elide_at_beginning);

  // Use binary(-like) search to compute the elided text.  In particular, do
  // an interpolation search, which is a binary search in which each guess
  // is an attempt to smartly calculate the right point rather than blindly
  // guessing midway between the endpoints.
  size_t lo = 0;
  size_t hi = text.length() - 1;
  size_t guess = std::string::npos;
  // These two widths are not exactly right but they're good enough to provide
  // some guidance to the search.  For example, |text_width| is actually the
  // length of text.length(), not text.length()-1.
  float lo_width = 0;
  float hi_width = text_width;
  const base::i18n::TextDirection text_direction = GetTextDirection(text);
  while (lo <= hi) {
    // Linearly interpolate between |lo| and |hi|, which correspond to widths
    // of |lo_width| and |hi_width| to estimate at what position
    // |available_width| would be at.  Because |lo_width| and |hi_width| are
    // both estimates (may be off by a little because, for example, |lo_width|
    // may have been calculated from |lo| minus one, not |lo|), we clamp to the
    // the valid range.
    // |last_guess| is merely used to verify that we're not repeating guesses.
    const size_t last_guess = guess;
    guess = lo + static_cast<size_t>(ToRoundedInt((available_width - lo_width) *
                                                  (hi - lo) /
                                                  (hi_width - lo_width)));
    guess = base::ClampToRange(guess, lo, hi);
    DCHECK_NE(last_guess, guess);

    // Restore colors. They will be truncated to size by SetText.
    render_text->colors_ = colors_;
    base::string16 new_text =
        slicer.CutString(guess, insert_ellipsis && behavior != ELIDE_TAIL);

    // This has to be an additional step so that the ellipsis is rendered with
    // same style as trailing part of the text.
    if (insert_ellipsis && behavior == ELIDE_TAIL) {
      // When ellipsis follows text whose directionality is not the same as that
      // of the whole text, it will be rendered with the directionality of the
      // whole text. Since we want ellipsis to indicate continuation of the
      // preceding text, we force the directionality of ellipsis to be same as
      // the preceding text using LTR or RTL markers.
      base::i18n::TextDirection trailing_text_direction =
          base::i18n::GetLastStrongCharacterDirection(new_text);
      new_text.append(ellipsis);
      if (trailing_text_direction != text_direction) {
        if (trailing_text_direction == base::i18n::LEFT_TO_RIGHT)
          new_text += base::i18n::kLeftToRightMark;
        else
          new_text += base::i18n::kRightToLeftMark;
      }
    }
    render_text->SetText(new_text);

    // Restore styles and baselines without breaking multi-character graphemes.
    render_text->styles_ = styles_;
    for (size_t style = 0; style < NUM_TEXT_STYLES; ++style)
      RestoreBreakList(render_text.get(), &render_text->styles_[style]);
    RestoreBreakList(render_text.get(), &render_text->baselines_);
    RestoreBreakList(render_text.get(), &render_text->font_size_overrides_);
    render_text->weights_ = weights_;
    RestoreBreakList(render_text.get(), &render_text->weights_);

    // We check the width of the whole desired string at once to ensure we
    // handle kerning/ligatures/etc. correctly.
    const float guess_width = render_text->GetContentWidthF();
    if (guess_width == available_width)
      break;
    if (guess_width > available_width) {
      hi = guess - 1;
      hi_width = guess_width;
      // Move back on the loop terminating condition when the guess is too wide.
      if (hi < lo) {
        lo = hi;
        lo_width = guess_width;
      }
    } else {
      lo = guess + 1;
      lo_width = guess_width;
    }
  }

  return render_text->text();
}

base::string16 RenderText::ElideEmail(const base::string16& email,
                                      float available_width) {
  // The returned string will have at least one character besides the ellipsis
  // on either side of '@'; if that's impossible, a single ellipsis is returned.
  // If possible, only the username is elided. Otherwise, the domain is elided
  // in the middle, splitting available width equally with the elided username.
  // If the username is short enough that it doesn't need half the available
  // width, the elided domain will occupy that extra width.

  // Split the email into its local-part (username) and domain-part. The email
  // spec allows for @ symbols in the username under some special requirements,
  // but not in the domain part, so splitting at the last @ symbol is safe.
  const size_t split_index = email.find_last_of('@');
  DCHECK_NE(split_index, base::string16::npos);
  base::string16 username = email.substr(0, split_index);
  base::string16 domain = email.substr(split_index + 1);
  DCHECK(!username.empty());
  DCHECK(!domain.empty());

  // Subtract the @ symbol from the available width as it is mandatory.
  const base::string16 kAtSignUTF16 = base::ASCIIToUTF16("@");
  available_width -= GetStringWidthF(kAtSignUTF16, font_list());

  // Check whether eliding the domain is necessary: if eliding the username
  // is sufficient, the domain will not be elided.
  const float full_username_width = GetStringWidthF(username, font_list());
  const float available_domain_width = available_width -
      std::min(full_username_width,
          GetStringWidthF(username.substr(0, 1) + kEllipsisUTF16, font_list()));
  if (GetStringWidthF(domain, font_list()) > available_domain_width) {
    // Elide the domain so that it only takes half of the available width.
    // Should the username not need all the width available in its half, the
    // domain will occupy the leftover width.
    // If |desired_domain_width| is greater than |available_domain_width|: the
    // minimal username elision allowed by the specifications will not fit; thus
    // |desired_domain_width| must be <= |available_domain_width| at all cost.
    const float desired_domain_width =
        std::min<float>(available_domain_width,
            std::max<float>(available_width - full_username_width,
                            available_width / 2));
    domain = Elide(domain, 0, desired_domain_width, ELIDE_MIDDLE);
    // Failing to elide the domain such that at least one character remains
    // (other than the ellipsis itself) remains: return a single ellipsis.
    if (domain.length() <= 1U)
      return base::string16(kEllipsisUTF16);
  }

  // Fit the username in the remaining width (at this point the elided username
  // is guaranteed to fit with at least one character remaining given all the
  // precautions taken earlier).
  available_width -= GetStringWidthF(domain, font_list());
  username = Elide(username, 0, available_width, ELIDE_TAIL);
  return username + kAtSignUTF16 + domain;
}

void RenderText::UpdateCachedBoundsAndOffset() {
  if (cached_bounds_and_offset_valid_)
    return;

  // TODO(ckocagil): Add support for scrolling multiline text.

  int delta_x = 0;

  if (cursor_enabled()) {
    // When cursor is enabled, ensure it is visible. For this, set the valid
    // flag true and calculate the current cursor bounds using the stale
    // |display_offset_|. Then calculate the change in offset needed to move the
    // cursor into the visible area.
    cached_bounds_and_offset_valid_ = true;
    cursor_bounds_ = GetCursorBounds(selection_model_, true);

    // TODO(bidi): Show RTL glyphs at the cursor position for ALIGN_LEFT, etc.
    if (cursor_bounds_.right() > display_rect_.right())
      delta_x = display_rect_.right() - cursor_bounds_.right();
    else if (cursor_bounds_.x() < display_rect_.x())
      delta_x = display_rect_.x() - cursor_bounds_.x();
  }

  SetDisplayOffset(display_offset_.x() + delta_x);
}

void RenderText::DrawSelection(Canvas* canvas) {
  for (Rect s : GetSubstringBounds(selection())) {
    if (symmetric_selection_visual_bounds() && !multiline())
      s = ExpandToBeVerticallySymmetric(s, display_rect());
    canvas->FillRect(s, selection_background_focused_color_);
  }
}

size_t RenderText::GetNearestWordStartBoundary(size_t index) const {
  const size_t length = text().length();
  if (obscured() || length == 0)
    return length;

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  const bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return length;

  // First search for the word start boundary in the CURSOR_BACKWARD direction,
  // then in the CURSOR_FORWARD direction.
  for (int i = std::min(index, length - 1); i >= 0; i--)
    if (iter.IsStartOfWord(i))
      return i;

  for (size_t i = index + 1; i < length; i++)
    if (iter.IsStartOfWord(i))
      return i;

  return length;
}

Range RenderText::ExpandRangeToWordBoundary(const Range& range) const {
  const size_t length = text().length();
  DCHECK_LE(range.GetMax(), length);
  if (obscured())
    return range.is_reversed() ? Range(length, 0) : Range(0, length);

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  const bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return range;

  size_t range_min = range.GetMin();
  if (range_min == length && range_min != 0)
    --range_min;

  for (; range_min != 0; --range_min)
    if (iter.IsStartOfWord(range_min) || iter.IsEndOfWord(range_min))
      break;

  size_t range_max = range.GetMax();
  if (range_min == range_max && range_max != length)
    ++range_max;

  for (; range_max < length; ++range_max)
    if (iter.IsEndOfWord(range_max) || iter.IsStartOfWord(range_max))
      break;

  return range.is_reversed() ? Range(range_max, range_min)
                             : Range(range_min, range_max);
}

internal::TextRunList* RenderText::GetRunList() {
  NOTREACHED();
  return nullptr;
}

const internal::TextRunList* RenderText::GetRunList() const {
  NOTREACHED();
  return nullptr;
}

void RenderText::SetGlyphWidthForTest(float test_width) {
  NOTREACHED();
}

}  // namespace gfx
