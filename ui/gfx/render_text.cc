// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text.h"

#include <limits.h>

#include <algorithm>
#include <climits>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/draw_looper.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_shader.h"
#include "third_party/icu/source/common/unicode/rbbi.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/platform_font.h"
#include "ui/gfx/render_text_harfbuzz.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/utf16_indexing.h"

namespace gfx {

namespace {

// Replacement codepoint for elided text.
constexpr char16_t kEllipsisCodepoint = 0x2026;

// Fraction of the text size to raise the center of a strike-through line above
// the baseline.
const SkScalar kStrikeThroughOffset = (SK_Scalar1 * 65 / 252);
// Fraction of the text size to lower an underline below the baseline.
const SkScalar kUnderlineOffset = (SK_Scalar1 / 9);

// Float comparison needs epsilon to consider rounding errors in float
// arithmetic. Epsilon should be dependent on the context and here, we are
// dealing with glyph widths, use a fairly large number.
const float kFloatComparisonEpsilon = 0.001f;
float Clamp(float f) {
  return f < kFloatComparisonEpsilon ? 0 : f;
}

// Given |font| and |display_width|, returns the width of the fade gradient.
int CalculateFadeGradientWidth(const FontList& font_list, int display_width) {
  // Fade in/out about 3 characters of the beginning/end of the string.
  // Use a 1/3 of the display width if the display width is very short.
  const int narrow_width = font_list.GetExpectedTextWidth(3);
  const int gradient_width =
      std::min(narrow_width, base::ClampRound(display_width / 3.f));
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
          ? base::ClampRound<SkAlpha>((1 - width_fraction) * kAlphaAtZeroWidth)
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
  // TODO(crbug.com/40219248): Remove this helper vector colors4f and make all
  // SkColor4f.
  std::vector<SkColor4f> colors4f;
  colors4f.reserve(colors.size());
  for (auto& c : colors)
    colors4f.push_back(SkColor4f::FromColor(c));
  return cc::PaintShader::MakeLinearGradient(
      &points[0], &colors4f[0], &positions[0],
      static_cast<int>(colors4f.size()), SkTileMode::kClamp);
}

// Converts a FontRenderParams::Hinting value to the corresponding
// SkFontHinting value.
SkFontHinting FontRenderParamsHintingToSkFontHinting(
    FontRenderParams::Hinting params_hinting) {
  switch (params_hinting) {
    case FontRenderParams::HINTING_NONE:
      return SkFontHinting::kNone;
    case FontRenderParams::HINTING_SLIGHT:
      return SkFontHinting::kSlight;
    case FontRenderParams::HINTING_MEDIUM:
      return SkFontHinting::kNormal;
    case FontRenderParams::HINTING_FULL:
      return SkFontHinting::kFull;
  }
  return SkFontHinting::kNone;
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

// Move the iterator |iter| forward until |position| is included in the range.
template <typename T>
typename BreakList<T>::const_iterator IncrementBreakListIteratorToPosition(
    const BreakList<T>& break_list,
    typename BreakList<T>::const_iterator iter,
    size_t position) {
  DCHECK_LT(position, break_list.max());
  for (;;) {
    CHECK(iter != break_list.breaks().end());
    const Range range = break_list.GetRange(iter);
    if (position >= range.start() && position < range.end())
      return iter;
    ++iter;
  }
}

// Replaces the unicode control characters, control characters and PUA (Private
// Use Areas) codepoints.
UChar32 ReplaceControlCharacter(UChar32 codepoint) {
  // 'REPLACEMENT CHARACTER' used to replace an unknown,
  // unrecognized or unrepresentable character.
  constexpr char16_t kReplacementCodepoint = 0xFFFD;
  // Control Pictures block (see:
  // https://unicode.org/charts/PDF/U2400.pdf).
  constexpr char16_t kSymbolsCodepoint = 0x2400;

  if (codepoint >= 0 && codepoint <= 0x1F) {
    switch (codepoint) {
      case 0x09:
        // Replace character tabulation ('\t') with its visual arrow symbol.
        return 0x21E5;
      case 0x0A:
        // Replace line feed ('\n') with space character.
        return 0x20;
      default:
        // Replace codepoints with their visual symbols, which are
        // at the same offset from kSymbolsCodepoint.
        return kSymbolsCodepoint + codepoint;
    }
  }
  if (codepoint == 0x7F) {
    // Replace the 'del' codepoint by its symbol (u2421).
    return kSymbolsCodepoint + 0x21;
  }
  if (!U_IS_UNICODE_CHAR(codepoint)) {
    // Unicode codepoint that can't be assigned a character.
    // This handles:
    // - single surrogate codepoints,
    // - last two codepoints on each plane,
    // - invalid characters (e.g. u+fdd0..u+fdef)
    // - codepoints above u+10ffff
    return kReplacementCodepoint;
  }
  if (codepoint > 0x7F) {
    // Private use codepoints are working with a pair of font
    // and codepoint, but they are not used in Chrome.
#if BUILDFLAG(IS_MAC)
    // Support Apple defined PUA on Mac.
    // see: http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/CORPCHAR.TXT
    if (codepoint == 0xF8FF)
      return codepoint;
#endif
#if BUILDFLAG(IS_WIN)
    // Support Microsoft defined PUA on Windows.
    // see:
    // https://docs.microsoft.com/en-us/windows/uwp/design/style/segoe-ui-symbol-font
    switch (codepoint) {
      case 0xF093:  // ButtonA
      case 0xF094:  // ButtonB
      case 0xF095:  // ButtonY
      case 0xF096:  // ButtonX
      case 0xF108:  // LeftStick
      case 0xF109:  // RightStick
      case 0xF10A:  // TriggerLeft
      case 0xF10B:  // TriggerRight
      case 0xF10C:  // BumperLeft
      case 0xF10D:  // BumperRight
      case 0xF10E:  // Dpad
      case 0xEECA:  // ButtonView2
      case 0xEDE3:  // ButtonMenu
        return codepoint;
      default:
        break;
    }
#endif
    const int8_t codepoint_category = u_charType(codepoint);
    if (codepoint_category == U_PRIVATE_USE_CHAR ||
        codepoint_category == U_CONTROL_CHAR) {
      return kReplacementCodepoint;
    }
  }

  return codepoint;
}

// Returns the line segment index for the |line|, |text_x| pair. |text_x| is
// relative to text in the given line. Returns -1 if |text_x| is to the left
// of text in the line and |line|.segments.size() if it's to the right.
// |offset_relative_segment| will contain the offset of |text_x| relative to
// the start of the segment it is contained in.
int GetLineSegmentContainingXCoord(const internal::Line& line,
                                   float line_x,
                                   float* offset_relative_segment) {
  DCHECK(offset_relative_segment);

  *offset_relative_segment = 0;
  if (line_x < 0)
    return -1;
  for (size_t i = 0; i < line.segments.size(); i++) {
    const internal::LineSegment& segment = line.segments[i];
    // segment.x_range is not used because it is in text space.
    if (line_x < segment.width()) {
      *offset_relative_segment = line_x;
      return i;
    }
    line_x -= segment.width();
  }
  return line.segments.size();
}

}  // namespace

namespace internal {

SkiaTextRenderer::SkiaTextRenderer(Canvas* canvas)
    : canvas_(canvas), canvas_skia_(canvas->sk_canvas()) {
  DCHECK(canvas_skia_);
  SetFillStyle(cc::PaintFlags::kFill_Style);

  font_.setEdging(SkFont::Edging::kSubpixelAntiAlias);
  font_.setSubpixel(true);
  font_.setHinting(SkFontHinting::kNormal);
}

SkiaTextRenderer::~SkiaTextRenderer() {
}

void SkiaTextRenderer::SetDrawLooper(sk_sp<cc::DrawLooper> draw_looper) {
  flags_.setLooper(std::move(draw_looper));
}

void SkiaTextRenderer::SetFontRenderParams(const FontRenderParams& params,
                                           bool subpixel_rendering_suppressed) {
  ApplyRenderParams(params, subpixel_rendering_suppressed, &font_);
}

void SkiaTextRenderer::SetTypeface(sk_sp<SkTypeface> typeface) {
  font_.setTypeface(std::move(typeface));
}

void SkiaTextRenderer::SetTextSize(SkScalar size) {
  font_.setSize(size);
}

void SkiaTextRenderer::SetForegroundColor(SkColor foreground) {
  flags_.setColor(foreground);
}

void SkiaTextRenderer::SetShader(sk_sp<cc::PaintShader> shader) {
  flags_.setShader(std::move(shader));
}

void SkiaTextRenderer::SetFillStyle(cc::PaintFlags::Style fill_style) {
  flags_.setStyle(fill_style);
}

void SkiaTextRenderer::SetStrokeWidth(SkScalar stroke_width) {
  flags_.setStrokeWidth(stroke_width);
}

void SkiaTextRenderer::DrawPosText(const SkPoint* pos,
                                   const uint16_t* glyphs,
                                   size_t glyph_count) {
  SkTextBlobBuilder builder;
  const auto& run_buffer = builder.allocRunPos(font_, glyph_count);

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
  const SkScalar text_size = font_.getSize();
  SkRect r = SkRect::MakeLTRB(
      x_scalar, y + text_size * kUnderlineOffset, x_scalar + width,
      y + (text_size *
           (kUnderlineOffset +
            (thickness_factor * RenderText::kLineThicknessFactor))));
  canvas_skia_->drawRect(r, flags_);
}

void SkiaTextRenderer::DrawStrike(int x,
                                  int y,
                                  int width,
                                  SkScalar thickness_factor) {
  const SkScalar text_size = font_.getSize();
  // Strike should have a minimum height of 1.0f.
  const SkScalar height = std::max(1.0f, text_size * thickness_factor);
  const SkScalar top = y - text_size * kStrikeThroughOffset - height / 2;
  SkScalar x_scalar = SkIntToScalar(x);
  const SkRect r =
      SkRect::MakeLTRB(x_scalar, top, x_scalar + width, top + height);
  canvas_skia_->drawRect(r, flags_);
}

StyleIterator::StyleIterator(
    const BreakList<SkColor>* colors,
    const BreakList<BaselineStyle>* baselines,
    const BreakList<int>* font_size_overrides,
    const BreakList<Font::Weight>* weights,
    const BreakList<SkTypefaceID>* resolved_typefaces,
    const BreakList<cc::PaintFlags::Style>* fill_styles,
    const BreakList<SkScalar>* stroke_widths,
    const StyleArray* styles)
    : colors_(colors),
      baselines_(baselines),
      font_size_overrides_(font_size_overrides),
      weights_(weights),
      resolved_typefaces_(resolved_typefaces),
      fill_styles_(fill_styles),
      stroke_widths_(stroke_widths),
      styles_(styles) {
  color_ = colors_->breaks().begin();
  baseline_ = baselines_->breaks().begin();
  font_size_override_ = font_size_overrides_->breaks().begin();
  weight_ = weights_->breaks().begin();
  resolved_typeface_ = resolved_typefaces_->breaks().begin();
  fill_style_ = fill_styles_->breaks().begin();
  stroke_width_ = stroke_widths_->breaks().begin();
  for (size_t i = 0; i < styles_->size(); ++i)
    style_[i] = (*styles_)[i].breaks().begin();
}

StyleIterator::StyleIterator(const StyleIterator& style) = default;
StyleIterator::~StyleIterator() = default;
StyleIterator& StyleIterator::operator=(const StyleIterator& style) = default;

Range StyleIterator::GetRange() const {
  return GetTextBreakingRange().Intersect(colors_->GetRange(color_));
}

Range StyleIterator::GetTextBreakingRange() const {
  Range range = baselines_->GetRange(baseline_);
  range = range.Intersect(font_size_overrides_->GetRange(font_size_override_));
  range = range.Intersect(weights_->GetRange(weight_));
  range = range.Intersect(resolved_typefaces_->GetRange(resolved_typeface_));
  range = range.Intersect(fill_styles_->GetRange(fill_style_));
  range = range.Intersect(stroke_widths_->GetRange(stroke_width_));
  for (size_t i = 0; i < styles_->size(); ++i)
    range = range.Intersect((*styles_)[i].GetRange(style_[i]));
  return range;
}

void StyleIterator::IncrementToPosition(size_t position) {
  color_ = IncrementBreakListIteratorToPosition(*colors_, color_, position);
  baseline_ =
      IncrementBreakListIteratorToPosition(*baselines_, baseline_, position);
  font_size_override_ = IncrementBreakListIteratorToPosition(
      *font_size_overrides_, font_size_override_, position);
  weight_ = IncrementBreakListIteratorToPosition(*weights_, weight_, position);
  resolved_typeface_ = IncrementBreakListIteratorToPosition(
      *resolved_typefaces_, resolved_typeface_, position);
  fill_style_ = IncrementBreakListIteratorToPosition(*fill_styles_, fill_style_,
                                                     position);
  stroke_width_ = IncrementBreakListIteratorToPosition(*stroke_widths_,
                                                       stroke_width_, position);
  for (size_t i = 0; i < styles_->size(); ++i) {
    style_[i] = IncrementBreakListIteratorToPosition((*styles_)[i], style_[i],
                                                     position);
  }
}

LineSegment::LineSegment() : run(0) {}

LineSegment::~LineSegment() {}

Line::Line() : preceding_heights(0), baseline(0) {}

Line::Line(const Line& other) = default;

Line::~Line() {}

ShapedText::ShapedText(std::vector<Line> lines) : lines_(std::move(lines)) {}
ShapedText::~ShapedText() = default;

void ApplyRenderParams(const FontRenderParams& params,
                       bool subpixel_rendering_suppressed,
                       SkFont* font) {
  if (!params.antialiasing) {
    font->setEdging(SkFont::Edging::kAlias);
  } else if (subpixel_rendering_suppressed ||
             params.subpixel_rendering ==
                 FontRenderParams::SUBPIXEL_RENDERING_NONE) {
    font->setEdging(SkFont::Edging::kAntiAlias);
  } else {
    font->setEdging(SkFont::Edging::kSubpixelAntiAlias);
  }

  font->setSubpixel(params.subpixel_positioning);
  font->setForceAutoHinting(params.autohinter);
  font->setHinting(FontRenderParamsHintingToSkFontHinting(params.hinting));
}

}  // namespace internal

// static
constexpr char16_t RenderText::kPasswordReplacementChar;
constexpr bool RenderText::kDragToEndIfOutsideVerticalBounds;
constexpr int RenderText::kInvalidBaseline;
constexpr SkScalar RenderText::kLineThicknessFactor;

RenderText::~RenderText() = default;

// static
std::unique_ptr<RenderText> RenderText::CreateRenderText() {
  return std::make_unique<RenderTextHarfBuzz>();
}

std::unique_ptr<RenderText> RenderText::CreateInstanceOfSameStyle(
    const std::u16string& text) const {
  std::unique_ptr<RenderText> render_text = CreateRenderText();
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
  render_text->resolved_typefaces_ = resolved_typefaces_;
  render_text->fill_styles_ = fill_styles_;
  render_text->stroke_widths_ = stroke_widths_;
  render_text->glyph_width_for_test_ = glyph_width_for_test_;
  return render_text;
}

void RenderText::SetText(std::u16string text) {
  DCHECK(!composition_range_.IsValid());
  if (text_ == text)
    return;
  text_ = std::move(text);
  UpdateStyleLengths();

  // Clear style ranges as they might break new text graphemes and apply
  // the first style to the whole text instead.
  colors_.Reset();
  baselines_.Reset();
  font_size_overrides_.Reset();
  weights_.Reset();
  resolved_typefaces_.Reset();
  fill_styles_.Reset();
  stroke_widths_.Reset();
  for (auto& style : styles_)
    style.Reset();
  elidings_.ClearAndSetInitialValue(false);
  cached_bounds_and_offset_valid_ = false;

  // Reset selection model. SetText should always followed by SetSelectionModel
  // or SetCursorPosition in upper layer.
  SetSelectionModel(SelectionModel());

  // Invalidate the cached text direction if it depends on the text contents.
  if (directionality_mode_ == DIRECTIONALITY_FROM_TEXT)
    text_direction_ = base::i18n::UNKNOWN_DIRECTION;

  obscured_reveal_index_ = std::nullopt;
  OnTextAttributeChanged();
}

void RenderText::AppendText(const std::u16string& text) {
  text_ += text;
  UpdateStyleLengths();
  cached_bounds_and_offset_valid_ = false;
  obscured_reveal_index_ = std::nullopt;

  // Invalidate the cached text direction if it depends on the text contents.
  if (directionality_mode_ == DIRECTIONALITY_FROM_TEXT)
    text_direction_ = base::i18n::UNKNOWN_DIRECTION;

  OnTextAttributeChanged();
}

void RenderText::SetHorizontalAlignment(HorizontalAlignment alignment) {
  if (horizontal_alignment_ != alignment) {
    horizontal_alignment_ = alignment;
    display_offset_ = Vector2d();
    cached_bounds_and_offset_valid_ = false;
  }
}

void RenderText::SetVerticalAlignment(VerticalAlignment alignment) {
  if (vertical_alignment_ != alignment) {
    vertical_alignment_ = alignment;
    display_offset_ = Vector2d();
    cached_bounds_and_offset_valid_ = false;
  }
}

void RenderText::SetFontList(const FontList& font_list) {
  font_list_ = font_list;
  const int font_style = font_list.GetFontStyle();
  weights_.ClearAndSetInitialValue(font_list.GetFontWeight());
  styles_[TEXT_STYLE_ITALIC].ClearAndSetInitialValue(
      (font_style & Font::ITALIC) != 0);
  styles_[TEXT_STYLE_UNDERLINE].ClearAndSetInitialValue(
      (font_style & Font::UNDERLINE) != 0);
  styles_[TEXT_STYLE_HEAVY_UNDERLINE].ClearAndSetInitialValue(false);
  styles_[TEXT_STYLE_STRIKE].ClearAndSetInitialValue(
      (font_style & Font::STRIKE_THROUGH) != 0);
  baseline_ = kInvalidBaseline;
  cached_bounds_and_offset_valid_ = false;
  OnLayoutTextAttributeChanged();
}

void RenderText::SetCursorEnabled(bool cursor_enabled) {
  if (cursor_enabled_ != cursor_enabled) {
    cursor_enabled_ = cursor_enabled;
    cached_bounds_and_offset_valid_ = false;
  }
}

void RenderText::SetObscured(bool obscured) {
  if (obscured != obscured_) {
    obscured_ = obscured;
    obscured_reveal_index_ = std::nullopt;
    cached_bounds_and_offset_valid_ = false;
    OnTextAttributeChanged();
  }
}

void RenderText::SetObscuredRevealIndex(std::optional<size_t> index) {
  if (obscured_reveal_index_ != index) {
    obscured_reveal_index_ = index;
    cached_bounds_and_offset_valid_ = false;
    OnTextAttributeChanged();
  }
}

void RenderText::SetObscuredGlyphSpacing(int spacing) {
  if (obscured_glyph_spacing_ != spacing) {
    obscured_glyph_spacing_ = spacing;
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::SetMultiline(bool multiline) {
  if (multiline != multiline_) {
    multiline_ = multiline;
    cached_bounds_and_offset_valid_ = false;
    OnTextAttributeChanged();
  }
}

void RenderText::SetMaxLines(size_t max_lines) {
  if (max_lines_ != max_lines) {
    max_lines_ = max_lines;
    OnDisplayTextAttributeChanged();
  }
}

size_t RenderText::GetNumLines() {
  return GetShapedText()->lines().size();
}

size_t RenderText::GetTextIndexOfLine(size_t line) {
  const std::vector<internal::Line>& lines = GetShapedText()->lines();
  if (line >= lines.size())
    return text_.size();
  return DisplayIndexToTextIndex(lines[line].display_text_index);
}

void RenderText::SetWordWrapBehavior(WordWrapBehavior behavior) {
  // TODO(crbug.com/40157791): ELIDE_LONG_WORDS is not supported.
  DCHECK_NE(behavior, ELIDE_LONG_WORDS);

  if (word_wrap_behavior_ != behavior) {
    word_wrap_behavior_ = behavior;
    if (multiline_) {
      cached_bounds_and_offset_valid_ = false;
      OnTextAttributeChanged();
    }
  }
}

void RenderText::SetMinLineHeight(int line_height) {
  if (min_line_height_ != line_height) {
    min_line_height_ = line_height;
    cached_bounds_and_offset_valid_ = false;
    OnDisplayTextAttributeChanged();
  }
}

void RenderText::SetElideBehavior(ElideBehavior elide_behavior) {
  if (elide_behavior_ != elide_behavior) {
    elide_behavior_ = elide_behavior;
    OnDisplayTextAttributeChanged();
  }
}

void RenderText::SetWhitespaceElision(std::optional<bool> whitespace_elision) {
  if (whitespace_elision_ != whitespace_elision) {
    whitespace_elision_ = whitespace_elision;
    OnDisplayTextAttributeChanged();
  }
}

void RenderText::SetDisplayRect(const Rect& r) {
  if (r != display_rect_) {
    display_rect_ = r;
    baseline_ = kInvalidBaseline;
    cached_bounds_and_offset_valid_ = false;
    OnDisplayTextAttributeChanged();
  }
}

const std::vector<Range> RenderText::GetAllSelections() const {
  return selection_model_.GetAllSelections();
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
    SelectionModel selection_start = GetSelectionModelForSelectionStart();
    Point start = GetCursorBounds(selection_start, true).origin();
    Point end = GetCursorBounds(cursor, true).origin();

    // Use the selection start if it is left (when |direction| is CURSOR_LEFT)
    // or right (when |direction| is CURSOR_RIGHT) of the selection end.
    // Consider only the y-coordinates if the selection start and end are on
    // different lines.
    const bool cursor_is_leading =
        (start.y() > end.y()) ||
        ((start.y() == end.y()) && (start.x() > end.x()));
    const bool cursor_should_be_trailing =
        (direction == CURSOR_RIGHT) || (direction == CURSOR_DOWN);
    if (cursor_is_leading == cursor_should_be_trailing) {
      // In this case, a direction has been chosen that doesn't match
      // |selection_model|, so the range must be reversed to place the cursor at
      // the other end. Note the affinity won't matter: only the affinity of
      // |start| (which points "in" to the selection) determines the movement.
      Range range = selection_model_.selection();
      selection_model_ = SelectionModel(Range(range.end(), range.start()),
                                        selection_model_.caret_affinity());
      cursor = selection_start;
    }
  }

  // Cancelling a selection moves to the edge of the selection.
  if (break_type != FIELD_BREAK && break_type != LINE_BREAK &&
      !selection().is_empty() && selection_behavior == SELECTION_NONE) {
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

  // |cached_cursor_x| keeps the initial x-coordinates where CURSOR_UP or
  // CURSOR_DOWN starts. This enables the cursor to keep the same x-coordinates
  // even when the cursor passes through empty or short lines. The cached
  // x-coordinates should be reset when the cursor moves in a horizontal
  // direction.
  if (direction != CURSOR_UP && direction != CURSOR_DOWN)
    reset_cached_cursor_x();
}

bool RenderText::SetSelection(const SelectionModel& model) {
  // Enforce valid selection model components.
  size_t text_length = text().length();
  std::vector<Range> ranges = model.GetAllSelections();
  for (auto& range : ranges) {
    range = {std::min(range.start(), text_length),
             std::min(range.end(), text_length)};
    // The current model only supports caret positions at valid cursor indices.
    if (!IsValidCursorIndex(range.start()) || !IsValidCursorIndex(range.end()))
      return false;
  }
  SelectionModel sel = SelectionModel(ranges, model.caret_affinity());
  bool changed = sel != selection_model_;
  SetSelectionModel(sel);
  return changed;
}

bool RenderText::MoveCursorToPoint(const Point& point,
                                   bool select,
                                   const Point& drag_origin) {
  reset_cached_cursor_x();
  SelectionModel model = FindCursorPosition(point, drag_origin);
  if (select)
    model.set_selection_start(selection().start());
  return SetSelection(model);
}

bool RenderText::SelectRange(const Range& range, bool primary) {
  size_t text_length = text().length();
  Range sel(std::min(range.start(), text_length),
            std::min(range.end(), text_length));
  // Allow selection bounds at valid indices amid multi-character graphemes.
  if (!IsValidLogicalIndex(sel.start()) || !IsValidLogicalIndex(sel.end()))
    return false;
  if (primary) {
    LogicalCursorDirection affinity = (sel.is_reversed() || sel.is_empty())
                                          ? CURSOR_FORWARD
                                          : CURSOR_BACKWARD;
    SetSelectionModel(SelectionModel(sel, affinity));
  } else {
    AddSecondarySelection(sel);
  }
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
  OnLayoutTextAttributeChanged();
}

void RenderText::SetColor(SkColor value) {
  if (colors_.ClearAndSetInitialValue(value)) {
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::ApplyColor(SkColor value, const Range& range) {
  if (colors_.ApplyValue(value, range))
    OnLayoutTextAttributeChanged();
}

void RenderText::SetBaselineStyle(BaselineStyle value) {
  if (baselines_.ClearAndSetInitialValue(value)) {
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::ApplyBaselineStyle(BaselineStyle value, const Range& range) {
  if (baselines_.ApplyValue(value, range))
    OnLayoutTextAttributeChanged();
}

void RenderText::ApplyFontSizeOverride(int font_size_override,
                                       const Range& range) {
  if (font_size_overrides_.ApplyValue(font_size_override, range))
    OnLayoutTextAttributeChanged();
}

void RenderText::SetStyle(TextStyle style, bool value) {
  if (styles_[style].ClearAndSetInitialValue(value)) {
    cached_bounds_and_offset_valid_ = false;
    // TODO(oshima|msw): Not all style change requires layout changes.
    // Consider optimizing based on the type of change.
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::ApplyStyle(TextStyle style, bool value, const Range& range) {
  if (styles_[style].ApplyValue(value, range)) {
    cached_bounds_and_offset_valid_ = false;
    // TODO(oshima|msw): Not all style change requires layout changes.
    // Consider optimizing based on the type of change.
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::SetWeight(Font::Weight weight) {
  if (weights_.ClearAndSetInitialValue(weight)) {
    cached_bounds_and_offset_valid_ = false;
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::ApplyWeight(Font::Weight weight, const Range& range) {
  if (weights_.ApplyValue(weight, range)) {
    cached_bounds_and_offset_valid_ = false;
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::SetFillStyle(cc::PaintFlags::Style style) {
  if (fill_styles_.ClearAndSetInitialValue(style)) {
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::ApplyFillStyle(cc::PaintFlags::Style style,
                                const Range& range) {
  if (fill_styles_.ApplyValue(style, range)) {
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::SetStrokeWidth(SkScalar stroke_width) {
  if (stroke_widths_.ClearAndSetInitialValue(stroke_width)) {
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::ApplyStrokeWidth(SkScalar stroke_width, const Range& range) {
  if (stroke_widths_.ApplyValue(stroke_width, range)) {
    OnLayoutTextAttributeChanged();
  }
}

void RenderText::SetEliding(bool value) {
  elidings_.ClearAndSetInitialValue(value);
  OnLayoutTextAttributeChanged();
}

void RenderText::ApplyEliding(bool value, const Range& range) {
  elidings_.ApplyValue(value, range);
  OnLayoutTextAttributeChanged();
}

bool RenderText::GetStyle(TextStyle style) const {
  return (styles_[style].breaks().size() == 1) &&
         styles_[style].breaks().front().second;
}

void RenderText::SetDirectionalityMode(DirectionalityMode mode) {
  if (mode != directionality_mode_) {
    directionality_mode_ = mode;
    text_direction_ = base::i18n::UNKNOWN_DIRECTION;
    cached_bounds_and_offset_valid_ = false;
    OnLayoutTextAttributeChanged();
  }
}

base::i18n::TextDirection RenderText::GetTextDirection() const {
  if (text_direction_ == base::i18n::UNKNOWN_DIRECTION)
    text_direction_ = GetTextDirectionForGivenText(text_);
  return text_direction_;
}

base::i18n::TextDirection RenderText::GetDisplayTextDirection() {
  EnsureLayout();
  if (display_text_direction_ == base::i18n::UNKNOWN_DIRECTION)
    display_text_direction_ = GetTextDirectionForGivenText(GetDisplayText());
  return display_text_direction_;
}

VisualCursorDirection RenderText::GetVisualDirectionOfLogicalEnd() {
  return GetDisplayTextDirection() == base::i18n::LEFT_TO_RIGHT ? CURSOR_RIGHT
                                                                : CURSOR_LEFT;
}

VisualCursorDirection RenderText::GetVisualDirectionOfLogicalBeginning() {
  return GetDisplayTextDirection() == base::i18n::RIGHT_TO_LEFT ? CURSOR_RIGHT
                                                                : CURSOR_LEFT;
}

Size RenderText::GetStringSize() {
  const SizeF size_f = GetStringSizeF();
  return Size(base::ClampCeil(size_f.width()),
              base::ClampCeil(size_f.height()));
}

float RenderText::TotalLineWidth() {
  float total_width = 0;
  const internal::ShapedText* shaped_text = GetShapedText();
  for (const auto& line : shaped_text->lines())
    total_width += line.size.width();
  return total_width;
}

float RenderText::GetContentWidthF() {
  const float string_size = GetStringSizeF().width();
  // The cursor is drawn one pixel beyond the int-enclosed text bounds.
  return cursor_enabled_ ? std::ceil(string_size) + 1 : string_size;
}

int RenderText::GetContentWidth() {
  return base::ClampCeil(GetContentWidthF());
}

int RenderText::GetBaseline() {
  if (baseline_ == kInvalidBaseline) {
    const int centering_height =
        (vertical_alignment_ == ALIGN_MIDDLE)
            ? display_rect().height()
            : std::max(font_list().GetHeight(), min_line_height());
    baseline_ = DetermineBaselineCenteringText(centering_height, font_list());
    if (vertical_alignment_ == ALIGN_BOTTOM)
      baseline_ += display_rect().height() - centering_height;
  }
  DCHECK_NE(kInvalidBaseline, baseline_);
  return baseline_;
}

void RenderText::Draw(Canvas* canvas, bool select_all) {
  EnsureLayout();

  if (clip_to_display_rect()) {
    Rect clip_rect(display_rect());
    clip_rect.Inset(ShadowValue::GetMargin(shadows_));

    canvas->Save();
    canvas->ClipRect(clip_rect);
  }

  if (!text().empty()) {
    std::vector<Range> draw_selections;
    if (select_all)
      draw_selections = {Range(0, text().length())};
    else if (focused())
      draw_selections = GetAllSelections();

    DrawSelections(canvas, draw_selections);
    internal::SkiaTextRenderer renderer(canvas);
    DrawVisualText(&renderer, draw_selections);
  }

  if (clip_to_display_rect())
    canvas->Restore();
}

SelectionModel RenderText::FindCursorPosition(const Point& view_point,
                                              const Point& drag_origin) {
  const internal::ShapedText* shaped_text = GetShapedText();
  DCHECK(!shaped_text->lines().empty());

  int line_index = GetLineContainingYCoord((view_point - GetLineOffset(0)).y());
  // Handle kDragToEndIfOutsideVerticalBounds above or below the text in a
  // single-line by extending towards the mouse cursor.
  if (RenderText::kDragToEndIfOutsideVerticalBounds && !multiline() &&
      (line_index < 0 ||
       line_index >= static_cast<int>(shaped_text->lines().size()))) {
    SelectionModel selection_start = GetSelectionModelForSelectionStart();
    int edge = drag_origin.x() == 0 ? GetCursorBounds(selection_start, true).x()
                                    : drag_origin.x();
    bool left = view_point.x() < edge;
    return EdgeSelectionModel(left ? CURSOR_LEFT : CURSOR_RIGHT);
  }
  // Otherwise, clamp |line_index| to a valid value or drag to logical ends.
  if (line_index < 0) {
    if (RenderText::kDragToEndIfOutsideVerticalBounds)
      return EdgeSelectionModel(GetVisualDirectionOfLogicalBeginning());
    line_index = 0;
  }
  if (line_index >= static_cast<int>(shaped_text->lines().size())) {
    if (RenderText::kDragToEndIfOutsideVerticalBounds)
      return EdgeSelectionModel(GetVisualDirectionOfLogicalEnd());
    line_index = shaped_text->lines().size() - 1;
  }
  const internal::Line& line = shaped_text->lines()[line_index];
  // Newline segment should be ignored in finding segment index with x
  // coordinate because it's not drawn.
  Vector2d newline_offset;
  if (line.segments.size() >= 1 && IsNewlineSegment(line.segments.front()))
    newline_offset.set_x(line.segments.front().width());

  float point_offset_relative_segment = 0;
  const int segment_index = GetLineSegmentContainingXCoord(
      line, (view_point - GetLineOffset(line_index) + newline_offset).x(),
      &point_offset_relative_segment);
  if (segment_index < 0)
    return LineSelectionModel(line_index, CURSOR_LEFT);
  if (segment_index >= static_cast<int>(line.segments.size()))
    return LineSelectionModel(line_index, CURSOR_RIGHT);
  const internal::LineSegment& segment = line.segments[segment_index];

  const internal::TextRunHarfBuzz& run = *GetRunList()->runs()[segment.run];
  const size_t segment_min_glyph_index =
      run.CharRangeToGlyphRange(segment.char_range).GetMin();
  const float segment_offset_relative_run =
      segment_min_glyph_index != 0
          ? SkScalarToFloat(run.shape.positions[segment_min_glyph_index].x())
          : 0;
  const float point_offset_relative_run =
      point_offset_relative_segment + segment_offset_relative_run;

  // TODO(crbug.com/40499140): Use offset within the glyph to return the correct
  // grapheme position within a multi-grapheme glyph.
  for (size_t i = 0; i < run.shape.glyph_count; ++i) {
    const float end = i + 1 == run.shape.glyph_count
                          ? run.shape.width
                          : SkScalarToFloat(run.shape.positions[i + 1].x());
    const float middle =
        (end + SkScalarToFloat(run.shape.positions[i].x())) / 2;
    const size_t index = DisplayIndexToTextIndex(run.shape.glyph_to_char[i]);
    if (point_offset_relative_run < middle) {
      return run.font_params.is_rtl ? SelectionModel(IndexOfAdjacentGrapheme(
                                                         index, CURSOR_FORWARD),
                                                     CURSOR_BACKWARD)
                                    : SelectionModel(index, CURSOR_FORWARD);
    }
    if (point_offset_relative_run < end) {
      return run.font_params.is_rtl ? SelectionModel(index, CURSOR_FORWARD)
                                    : SelectionModel(IndexOfAdjacentGrapheme(
                                                         index, CURSOR_FORWARD),
                                                     CURSOR_BACKWARD);
    }
  }

  return LineSelectionModel(line_index, CURSOR_RIGHT);
}

bool RenderText::IsValidLogicalIndex(size_t index) const {
  // Check that the index is at a valid code point (not mid-surrogate-pair) and
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

bool RenderText::IsValidCursorIndex(size_t index) const {
  return index == 0 || index == text().length() ||
         (IsValidLogicalIndex(index) && IsGraphemeBoundary(index));
}

Rect RenderText::GetCursorBounds(const SelectionModel& caret,
                                 bool insert_mode) {
  EnsureLayout();
  size_t caret_pos = caret.caret_pos();
  DCHECK(IsValidLogicalIndex(caret_pos));

  // In overtype mode, ignore the affinity and always indicate that we will
  // overtype the next character.
  LogicalCursorDirection caret_affinity =
      insert_mode ? caret.caret_affinity() : CURSOR_FORWARD;
  float x = 0;
  int width = 1;

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
      x = TotalLineWidth();
  } else {
    // Find the next grapheme continuing in the current direction. This
    // determines the substring range that should be highlighted.
    size_t caret_end = IndexOfAdjacentGrapheme(caret_pos, caret_affinity);
    if (caret_end < caret_pos)
      std::swap(caret_end, caret_pos);

    const RangeF xspan = GetCursorSpan(Range(caret_pos, caret_end));
    if (insert_mode) {
      x = (caret_affinity == CURSOR_BACKWARD) ? xspan.end() : xspan.start();
    } else {  // overtype mode
      x = xspan.GetMin();
      // Ceil the start and end of the |xspan| because the cursor x-coordinates
      // are always ceiled.
      width = base::ClampCeil(Clamp(xspan.GetMax())) -
              base::ClampCeil(Clamp(xspan.GetMin()));
    }
  }
  Size line_size = gfx::ToCeiledSize(GetLineSizeF(caret));
  size_t line = GetLineContainingCaret(caret);
  return Rect(ToViewPoint(PointF(x, 0), line), Size(width, line_size.height()));
}

const Rect& RenderText::GetUpdatedCursorBounds() {
  UpdateCachedBoundsAndOffset();
  return cursor_bounds_;
}

internal::GraphemeIterator RenderText::GetGraphemeIteratorAtTextIndex(
    size_t index) const {
  EnsureLayoutTextUpdated();
  return GetGraphemeIteratorAtIndex(
      text_, &internal::TextToDisplayIndex::text_index, index);
}

internal::GraphemeIterator RenderText::GetGraphemeIteratorAtDisplayTextIndex(
    size_t index) const {
  EnsureLayoutTextUpdated();
  return GetGraphemeIteratorAtIndex(
      layout_text_, &internal::TextToDisplayIndex::display_index, index);
}

size_t RenderText::GetTextIndex(internal::GraphemeIterator iter) const {
  DCHECK(layout_text_up_to_date_);
  return iter == text_to_display_indices_.end() ? text_.length()
                                                : iter->text_index;
}

size_t RenderText::GetDisplayTextIndex(internal::GraphemeIterator iter) const {
  DCHECK(layout_text_up_to_date_);
  return iter == text_to_display_indices_.end() ? layout_text_.length()
                                                : iter->display_index;
}

bool RenderText::IsGraphemeBoundary(size_t index) const {
  return index >= text_.length() ||
         GetTextIndex(GetGraphemeIteratorAtTextIndex(index)) == index;
}

size_t RenderText::IndexOfAdjacentGrapheme(
    size_t index,
    LogicalCursorDirection direction) const {
  // The input is clamped if it is out of that range.
  if (text_.empty())
    return 0;
  if (index > text_.length())
    return text_.length();

  EnsureLayoutTextUpdated();

  internal::GraphemeIterator iter = index == text_.length()
                                        ? text_to_display_indices_.end()
                                        : GetGraphemeIteratorAtTextIndex(index);
  if (direction == CURSOR_FORWARD) {
    if (iter != text_to_display_indices_.end())
      ++iter;
  } else {
    DCHECK_EQ(direction, CURSOR_BACKWARD);
    // If the index was not at the beginning of the grapheme, it will have been
    // moved back to the grapheme start.
    if (iter != text_to_display_indices_.begin() && GetTextIndex(iter) == index)
      --iter;
  }
  return GetTextIndex(iter);
}

SelectionModel RenderText::GetSelectionModelForSelectionStart() const {
  const Range& sel = selection();
  if (sel.is_empty())
    return selection_model_;
  return SelectionModel(sel.start(),
                        sel.is_reversed() ? CURSOR_BACKWARD : CURSOR_FORWARD);
}

const Vector2d& RenderText::GetUpdatedDisplayOffset() {
  UpdateCachedBoundsAndOffset();
  return display_offset_;
}

void RenderText::SetDisplayOffset(int horizontal_offset) {
  SetDisplayOffset({horizontal_offset, display_offset_.y()});
}

void RenderText::SetDisplayOffset(Vector2d offset) {
  // Use ClampedNumeric for extra content, as it can otherwise overflow during
  // later operations if GetContentWidth() returns INT_MAX and
  // display_rect_.width() is 0.
  const base::ClampedNumeric<int> extra_content =
      base::ClampedNumeric<int>(GetContentWidth()) - display_rect_.width();
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

  const int horizontal_offset = std::clamp(offset.x(), min_offset, max_offset);

  // y-offset is set only when the vertical alignment is ALIGN_TOP.
  // TODO(jongkown.lee): Support other vertical alignments.
  DCHECK(vertical_alignment_ == ALIGN_TOP || offset.y() == 0);
  const int vertical_offset = std::clamp(
      offset.y(),
      std::min(display_rect_.height() - GetStringSize().height(), 0), 0);

  cached_bounds_and_offset_valid_ = true;
  display_offset_ = {horizontal_offset, vertical_offset};
  cursor_bounds_ = GetCursorBounds(selection_model_, true);
}

Vector2d RenderText::GetLineOffset(size_t line_number) {
  const internal::ShapedText* shaped_text = GetShapedText();
  Vector2d offset = display_rect().OffsetFromOrigin();
  if (!multiline()) {
    offset.Add(GetUpdatedDisplayOffset());
  } else {
    DCHECK_LT(line_number, shaped_text->lines().size());
    offset.Add(GetUpdatedDisplayOffset());
    offset.Add(
        Vector2d(0, shaped_text->lines()[line_number].preceding_heights));
  }
  offset.Add(GetAlignmentOffset(line_number));
  return offset;
}

bool RenderText::GetWordLookupDataAtPoint(const Point& point,
                                          DecoratedText* decorated_word,
                                          Rect* rect) {
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

  return GetLookupDataForRange(word_range, decorated_word, rect);
}

bool RenderText::GetLookupDataForRange(const Range& range,
                                       DecoratedText* decorated_text,
                                       Rect* rect) {
  const internal::ShapedText* shaped_text = GetShapedText();

  const std::vector<Rect> word_bounds = GetSubstringBounds(range);
  if (word_bounds.empty()) {
    return false;
  }
  GetDecoratedTextForRange(range, decorated_text);

  // Retrieve the baseline origin of the left-most glyph.
  const auto left_rect = std::min_element(
      word_bounds.begin(), word_bounds.end(),
      [](const Rect& lhs, const Rect& rhs) { return lhs.x() < rhs.x(); });
  const int line_index = GetLineContainingYCoord(left_rect->CenterPoint().y() -
                                                 GetLineOffset(0).y());
  if (line_index < 0 ||
      line_index >= static_cast<int>(shaped_text->lines().size()))
    return false;
  *rect = Rect(left_rect->origin() +
                   Vector2d(0, shaped_text->lines()[line_index].baseline),
               left_rect->size());
  return true;
}

std::u16string RenderText::GetTextFromRange(const Range& range) const {
  if (range.IsValid() && range.GetMin() < text().length())
    return text().substr(range.GetMin(), range.length());
  return std::u16string();
}

Range RenderText::ExpandRangeToGraphemeBoundary(const Range& range) const {
  const auto snap_to_grapheme = [this](auto index, auto direction) {
    return IsValidCursorIndex(index)
               ? index
               : IndexOfAdjacentGrapheme(index, direction);
  };

  const size_t min_index = snap_to_grapheme(range.GetMin(), CURSOR_BACKWARD);
  const size_t max_index = snap_to_grapheme(range.GetMax(), CURSOR_FORWARD);
  return Range(min_index, max_index).MatchDirection(range);
}

Range RenderText::ExpandRangeToWordBoundary(const Range& range) const {
  const size_t length = text().length();
  DCHECK_LE(range.GetMax(), length);
  if (obscured()) {
    return Range(0, length).MatchDirection(range);
  }

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  const bool success = iter.Init();
  DCHECK(success);
  if (!success) {
    return range;
  }

  size_t range_min = range.GetMin();
  if (range_min == length && range_min != 0) {
    --range_min;
  }

  for (; range_min != 0; --range_min) {
    if (iter.IsStartOfWord(range_min) || iter.IsEndOfWord(range_min)) {
      break;
    }
  }

  size_t range_max = range.GetMax();
  if (range_min == range_max && range_max != length) {
    ++range_max;
  }

  for (; range_max < length; ++range_max) {
    if (iter.IsEndOfWord(range_max) || iter.IsStartOfWord(range_max)) {
      break;
    }
  }
  return Range(range_min, range_max).MatchDirection(range);
}

bool RenderText::IsNewlineSegment(const internal::LineSegment& segment) const {
  return IsNewlineSegment(text_, segment);
}

bool RenderText::IsNewlineSegment(const std::u16string& text,
                                  const internal::LineSegment& segment) const {
  const size_t offset = segment.char_range.start();
  const size_t length = segment.char_range.length();
  DCHECK_LT(offset + length - 1, text.length());
  return (length == 1 && (text[offset] == '\r' || text[offset] == '\n')) ||
         (length == 2 && text[offset] == '\r' && text[offset + 1] == '\n');
}

Range RenderText::GetLineRange(const std::u16string& text,
                               const internal::Line& line) const {
  // This will find the logical start and end indices of the given line.
  size_t max_index = 0;
  size_t min_index = text.length();
  for (const auto& segment : line.segments) {
    min_index = std::min<size_t>(min_index, segment.char_range.GetMin());
    max_index = std::max<size_t>(max_index, segment.char_range.GetMax());
  }

  // Do not include the newline character, as that could be considered leading
  // into the next line. Note that the newline character is always the last
  // character of the line regardless of the text direction, so decrease the
  // |max_index|.
  if (!line.segments.empty() &&
      (IsNewlineSegment(text, line.segments.back()) ||
       IsNewlineSegment(text, line.segments.front()))) {
    --max_index;
  }

  return Range(min_index, max_index);
}

RenderText::RenderText() = default;

internal::StyleIterator RenderText::GetTextStyleIterator() const {
  return internal::StyleIterator(&colors_, &baselines_, &font_size_overrides_,
                                 &weights_, &resolved_typefaces_, &fill_styles_,
                                 &stroke_widths_, &styles_);
}

internal::StyleIterator RenderText::GetLayoutTextStyleIterator() const {
  EnsureLayoutTextUpdated();
  return internal::StyleIterator(
      &layout_colors_, &layout_baselines_, &layout_font_size_overrides_,
      &layout_weights_, &layout_resolved_typefaces_, &layout_fill_styles_,
      &layout_stroke_widths_, &layout_styles_);
}

bool RenderText::IsHomogeneous() const {
  if (colors().breaks().size() > 1 || baselines().breaks().size() > 1 ||
      font_size_overrides().breaks().size() > 1 ||
      weights().breaks().size() > 1) {
    return false;
  }

  return base::ranges::none_of(
      styles(), [](const auto& style) { return style.breaks().size() > 1; });
}

internal::ShapedText* RenderText::GetShapedText() {
  EnsureLayout();
  DCHECK(shaped_text_);
  return shaped_text_.get();
}

int RenderText::GetDisplayTextBaseline() {
  DCHECK(!GetShapedText()->lines().empty());
  return GetShapedText()->lines()[0].baseline;
}

SelectionModel RenderText::GetAdjacentSelectionModel(
    const SelectionModel& current,
    BreakType break_type,
    VisualCursorDirection direction) {
  EnsureLayout();

  if (direction == CURSOR_UP || direction == CURSOR_DOWN)
    return AdjacentLineSelectionModel(current, direction);
  if (break_type == FIELD_BREAK || text().empty())
    return EdgeSelectionModel(direction);
  if (break_type == LINE_BREAK)
    return LineSelectionModel(GetLineContainingCaret(current), direction);
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
  DCHECK(direction == CURSOR_LEFT || direction == CURSOR_RIGHT);
  DCHECK_LT(line_index, GetShapedText()->lines().size());
  const internal::Line& line = GetShapedText()->lines()[line_index];
  if (line.segments.empty()) {
    // Only the last line can be empty.
    DCHECK_EQ(GetShapedText()->lines().size() - 1, line_index);
    return EdgeSelectionModel(GetVisualDirectionOfLogicalEnd());
  }
  if (line_index ==
      (direction == GetVisualDirectionOfLogicalEnd() ? GetNumLines() - 1 : 0)) {
    return EdgeSelectionModel(direction);
  }

  DCHECK_GT(GetNumLines(), 1U);
  Range line_range = GetLineRange(text(), line);

  // Cursor affinity should be the opposite of visual direction to preserve the
  // line number of the cursor in multiline text.
  return direction == GetVisualDirectionOfLogicalEnd()
             ? SelectionModel(DisplayIndexToTextIndex(line_range.end()),
                              CURSOR_BACKWARD)
             : SelectionModel(DisplayIndexToTextIndex(line_range.start()),
                              CURSOR_FORWARD);
}

void RenderText::SetSelectionModel(const SelectionModel& model) {
  DCHECK_LE(model.selection().GetMax(), text().length());
  selection_model_ = model;
  cached_bounds_and_offset_valid_ = false;
  has_directed_selection_ = kSelectionIsAlwaysDirected;
}

void RenderText::AddSecondarySelection(const Range selection) {
  DCHECK_LE(selection.GetMax(), text().length());
  selection_model_.AddSecondarySelection(selection);
}

size_t RenderText::TextIndexToDisplayIndex(size_t index) const {
  return GetDisplayTextIndex(GetGraphemeIteratorAtTextIndex(index));
}

size_t RenderText::DisplayIndexToTextIndex(size_t index) const {
  return GetTextIndex(GetGraphemeIteratorAtDisplayTextIndex(index));
}

void RenderText::OnLayoutTextAttributeChanged() {
  layout_text_up_to_date_ = false;
}

void RenderText::EnsureLayoutTextUpdated() const {
  if (layout_text_up_to_date_)
    return;

  layout_text_.clear();
  text_to_display_indices_.clear();

  display_text_direction_ = base::i18n::UNKNOWN_DIRECTION;

  // Reset the previous layout text attributes. Allocate enough space for
  // layout text attributes (upper limit to 2x characters per codepoint). The
  // actual size will be updated at the end of the function.
  UpdateLayoutStyleLengths(2 * text_.length());

  // Create an grapheme iterator to ensure layout BreakLists don't break
  // graphemes.
  base::i18n::BreakIterator grapheme_iter(
      text_, base::i18n::BreakIterator::BREAK_CHARACTER);
  bool success = grapheme_iter.Init();
  DCHECK(success);

  // Ensures the reveal index is at a codepoint boundary (e.g. not in a middle
  // of a surrogate pairs).
  size_t reveal_index = text_.size();
  if (obscured_reveal_index_.has_value()) {
    reveal_index = obscured_reveal_index_.value();
    // Move |reveal_index| to the beginning of the surrogate pair, if needed.
    if (reveal_index < text_.size()) {
      // SAFETY: U16_SET_CP_START() internally checks for underflow, and we know
      // that reveal_index is before the end of the string since it is checked
      // right above.
      UNSAFE_BUFFERS(U16_SET_CP_START(text_.data(), 0, reveal_index));
    }
  }

  BreakList<bool>::const_iterator eliding_iterator = elidings_.breaks().begin();
  bool previous_grapheme_elided = false;

  // Iterates through graphemes from |text_| and rewrite its codepoints to
  // |layout_text_|.
  base::i18n::UTF16CharIterator text_iter(text_);
  internal::StyleIterator styles = GetTextStyleIterator();
  bool text_truncated = false;
  while (!text_iter.end() && !text_truncated) {
    std::vector<uint32_t> grapheme_codepoints;
    const size_t text_grapheme_start_position = text_iter.array_pos();
    // We have not added the codepoints of the current grapheme to
    // `layout_text_` yet. The rest of the loop will either add the codepoints
    // of the current grapheme to `layout_text_` or skip the grapheme if it will
    // not exist in `layout_text_`. Therefore, layout_text_.size() will either
    // be the start of the current grapeheme or indicate that the grapheme does
    // not exist in `layout_text_`.
    const size_t layout_grapheme_start_position = layout_text_.size();

    // Retrieve codepoints of the current grapheme.
    do {
      grapheme_codepoints.push_back(text_iter.get());
      text_iter.Advance();
    } while (!grapheme_iter.IsGraphemeBoundary(text_iter.array_pos()) &&
             !text_iter.end());
    const size_t text_grapheme_end_position = text_iter.array_pos();

    // Keep track of the mapping between |text_| and |layout_text_| indices.
    internal::TextToDisplayIndex mapping = {text_grapheme_start_position,
                                            layout_grapheme_start_position};
    text_to_display_indices_.push_back(mapping);

    // Flag telling if the current grapheme is a newline control sequence.
    const bool is_newline_grapheme =
        (grapheme_codepoints.size() == 1 &&
         (grapheme_codepoints[0] == '\r' || grapheme_codepoints[0] == '\n')) ||
        (grapheme_codepoints.size() == 2 && grapheme_codepoints[0] == '\r' &&
         grapheme_codepoints[1] == '\n');

    // Obscure the layout text by replacing the grapheme by a bullet.
    if (obscured_ &&
        (reveal_index < text_grapheme_start_position ||
         reveal_index >= text_grapheme_end_position) &&
        (!is_newline_grapheme || !multiline_)) {
      grapheme_codepoints.clear();
      grapheme_codepoints.push_back(RenderText::kPasswordReplacementChar);
    }

    // Handle unicode control characters ISO 6429 (block C0). Range from 0 to
    // 0x1F and 0x7F. The newline character should be kept as-is when
    // rendertext is multiline.
    if (!multiline_ || !is_newline_grapheme) {
      for (uint32_t& codepoint : grapheme_codepoints)
        codepoint = ReplaceControlCharacter(codepoint);
    }

    // Truncate text when the input text it above |truncate_length_|.
    text_truncated = (truncate_length_ != 0 &&
                      ((text_grapheme_end_position > truncate_length_) ||
                       (!text_iter.end() &&
                        (text_grapheme_end_position == truncate_length_))));

    // If the text is elided, replace it by an ellipsis. Do not append an
    // ellipsis if it was already inserted.
    eliding_iterator = IncrementBreakListIteratorToPosition(
        elidings_, eliding_iterator, text_grapheme_start_position);
    const bool elided_grapheme = eliding_iterator->second;
    if (elided_grapheme || text_truncated) {
      grapheme_codepoints.clear();
      // Append an ellipsis if not already done.
      if (!previous_grapheme_elided) {
        grapheme_codepoints.push_back(kEllipsisCodepoint);
      }
    }
    previous_grapheme_elided = elided_grapheme;

    for (uint32_t codepoint : grapheme_codepoints) {
      // Append the codepoint to the layout text.
      const size_t current_layout_text_position = layout_text_.size();
      const size_t codepoint_length = U16_LENGTH(codepoint);
      if (codepoint_length == 1) {
        layout_text_ += codepoint;
      } else {
        layout_text_ += U16_LEAD(codepoint);
        layout_text_ += U16_TRAIL(codepoint);
      }

      // Apply the style at current grapheme position to the layout text.
      styles.IncrementToPosition(text_grapheme_start_position);

      Range range(current_layout_text_position, layout_text_.size());
      layout_colors_.ApplyValue(styles.color(), range);
      layout_baselines_.ApplyValue(styles.baseline(), range);
      layout_font_size_overrides_.ApplyValue(styles.font_size_override(),
                                             range);
      layout_resolved_typefaces_.ApplyValue(styles.resolved_typeface(), range);
      layout_fill_styles_.ApplyValue(styles.fill_style(), range);
      layout_stroke_widths_.ApplyValue(styles.stroke_width(), range);
      layout_weights_.ApplyValue(styles.weight(), range);
      for (size_t i = 0; i < layout_styles_.size(); ++i) {
        layout_styles_[i].ApplyValue(styles.style(static_cast<TextStyle>(i)),
                                     range);
      }

      // Apply an underline to the composition range in |underlines|.
      const Range grapheme_start_range(text_grapheme_start_position,
                                       text_grapheme_start_position + 1);
      if (composition_range_.Contains(grapheme_start_range))
        layout_styles_[TEXT_STYLE_HEAVY_UNDERLINE].ApplyValue(true, range);
    }
  }

  // Resize the layout text attributes to the actual layout text length.
  UpdateLayoutStyleLengths(layout_text_.length());

  // Ensures that the text got truncated correctly, when needed.
  DCHECK(truncate_length_ == 0 || layout_text_.size() <= truncate_length_);

  // Wait to reset |layout_text_up_to_date_| until the end, to ensure this
  // function's implementation doesn't indirectly rely on it being up to date
  // anywhere.
  layout_text_up_to_date_ = true;
}

const std::u16string& RenderText::GetLayoutText() const {
  EnsureLayoutTextUpdated();
  return layout_text_;
}

void RenderText::UpdateDisplayText(float text_width) {
  EnsureLayoutTextUpdated();

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
    text_elided_ = false;
    display_text_.clear();

    std::unique_ptr<RenderText> render_text(
        CreateInstanceOfSameStyle(layout_text_));
    render_text->SetMultiline(true);
    render_text->SetWordWrapBehavior(word_wrap_behavior_);
    render_text->SetDisplayRect(display_rect_);
    // Have it arrange words on |lines_|.
    render_text->EnsureLayout();

    if (render_text->GetShapedText()->lines().size() > max_lines_) {
      // Find the start and end index of the line to be elided.
      Range line_range = GetLineRange(
          layout_text_, render_text->GetShapedText()->lines()[max_lines_ - 1]);
      // Add an ellipsis character in case the last line is short enough to fit
      // on a single line. Otherwise that character will be elided anyway.
      std::u16string text_to_elide =
          layout_text_.substr(line_range.start(), line_range.length()) +
          std::u16string(kEllipsisUTF16);
      display_text_.assign(layout_text_.substr(0, line_range.start()) +
                           Elide(text_to_elide, 0,
                                 static_cast<float>(display_rect_.width()),
                                 ELIDE_TAIL));
    } else {
      // Initial state above is fine.
      return;
    }
  }
  text_elided_ = display_text_ != layout_text_;
  if (!text_elided_)
    display_text_.clear();
}

Point RenderText::ToViewPoint(const PointF& point, size_t line) {
  if (GetNumLines() == 1) {
    return Point(base::ClampCeil(Clamp(point.x())),
                 base::ClampRound(point.y())) +
           GetLineOffset(0);
  }

  const internal::ShapedText* shaped_text = GetShapedText();
  float x = point.x();

  if (GetDisplayTextDirection() == base::i18n::RIGHT_TO_LEFT) {
    // |xspan| returned from |GetCursorSpan| in |GetCursorBounds| starts to grow
    // from the last character in RTL. On the other hand, the last character is
    // positioned in the last line in RTL. So, traverse from the last line.
    for (size_t l = GetNumLines() - 1; l > line; --l) {
      x -= shaped_text->lines()[l].size.width();
    }
  } else {
    // TODO(crbug.com/40163177): This doesn't account for line breaks caused by
    // wrapping, in which case the cursor may end up right after the trailing
    // space on the top line instead of before the first character of the second
    // line depending on which direction the cursor is moving. Both positions
    // are "correct" but most text editors only allow one or the other for
    // consistency.
    for (size_t l = 0; l < line; ++l) {
      x -= shaped_text->lines()[l].size.width();
    }
  }

  return Point(base::ClampCeil(Clamp(x)), base::ClampRound(point.y())) +
         GetLineOffset(line);
}

HorizontalAlignment RenderText::GetCurrentHorizontalAlignment() {
  if (horizontal_alignment_ != ALIGN_TO_HEAD)
    return horizontal_alignment_;

  if (directionality_mode_ == gfx::DIRECTIONALITY_FROM_TEXT) {
    if (base::i18n::GetForcedTextDirection() == base::i18n::RIGHT_TO_LEFT) {
      return ALIGN_RIGHT;
    }
    if (base::i18n::GetForcedTextDirection() == base::i18n::LEFT_TO_RIGHT) {
      return ALIGN_LEFT;
    }
  }

  return GetDisplayTextDirection() == base::i18n::RIGHT_TO_LEFT ?
      ALIGN_RIGHT : ALIGN_LEFT;
}

Vector2d RenderText::GetAlignmentOffset(size_t line_number) {
  DCHECK(!multiline_ || (line_number < GetShapedText()->lines().size()));

  Vector2d offset;
  HorizontalAlignment horizontal_alignment = GetCurrentHorizontalAlignment();
  if (horizontal_alignment != ALIGN_LEFT) {
    const int width =
        multiline_ ? base::ClampCeil(
                         GetShapedText()->lines()[line_number].size.width() +
                         (cursor_enabled_ ? 1.0f : 0.0f))
                   : GetContentWidth();
    offset.set_x(display_rect().width() - width);

    // Put any extra margin pixel on the left to match legacy behavior.
    if (horizontal_alignment == ALIGN_CENTER)
      offset.set_x((offset.x() + 1) / 2);
  }

  switch (vertical_alignment_) {
    case ALIGN_TOP:
      offset.set_y(0);
      break;
    case ALIGN_MIDDLE:
      if (multiline_)
        offset.set_y((display_rect_.height() - GetStringSize().height()) / 2);
      else
        offset.set_y(GetBaseline() - GetDisplayTextBaseline());
      break;
    case ALIGN_BOTTOM:
      offset.set_y(display_rect_.height() - GetStringSize().height());
      break;
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
    left_part.Inset(
        gfx::Insets::TLBR(0, 0, 0, solid_part.width() - gradient_width));
    solid_part.Inset(gfx::Insets::TLBR(0, gradient_width, 0, 0));
  }
  if (horizontal_alignment != ALIGN_RIGHT) {
    right_part = solid_part;
    right_part.Inset(
        gfx::Insets::TLBR(0, solid_part.width() - gradient_width, 0, 0));
    solid_part.Inset(gfx::Insets::TLBR(0, 0, 0, gradient_width));
  }

  // CreateFadeShader() expects at least one part to not be empty.
  // See https://crbug.com/706835.
  if (left_part.IsEmpty() && right_part.IsEmpty())
    return;

  Rect text_rect = display_rect();
  text_rect.Inset(gfx::Insets::TLBR(0, GetAlignmentOffset(0).x(), 0, 0));

  // TODO(msw): Use the actual text colors corresponding to each faded part.
  renderer->SetShader(
      CreateFadeShader(font_list(), text_rect, left_part, right_part,
                       SkColorSetA(colors_.breaks().front().second, 0xff)));
}

void RenderText::ApplyTextShadows(internal::SkiaTextRenderer* renderer) {
  renderer->SetDrawLooper(CreateShadowDrawLooper(shadows_));
}

base::i18n::TextDirection RenderText::GetTextDirectionForGivenText(
    const std::u16string& text) const {
  switch (directionality_mode_) {
    case DIRECTIONALITY_FROM_TEXT:
      // Derive the direction from the display text, which differs from text()
      // in the case of obscured (password) textfields.
      return base::i18n::GetFirstStrongCharacterDirection(text);
    case DIRECTIONALITY_FORCE_LTR:
      return base::i18n::LEFT_TO_RIGHT;
    case DIRECTIONALITY_FORCE_RTL:
      return base::i18n::RIGHT_TO_LEFT;
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
      return base::i18n::LEFT_TO_RIGHT;
    default:
      NOTREACHED_IN_MIGRATION();
      return base::i18n::UNKNOWN_DIRECTION;
  }
}

void RenderText::UpdateStyleLengths() {
  const size_t text_length = text_.length();
  colors_.SetMax(text_length);
  baselines_.SetMax(text_length);
  font_size_overrides_.SetMax(text_length);
  weights_.SetMax(text_length);
  resolved_typefaces_.SetMax(text_length);
  fill_styles_.SetMax(text_length);
  stroke_widths_.SetMax(text_length);
  for (auto& style : styles_)
    style.SetMax(text_length);
  elidings_.SetMax(text_length);
}

void RenderText::UpdateLayoutStyleLengths(size_t max_length) const {
  layout_colors_.SetMax(max_length);
  layout_baselines_.SetMax(max_length);
  layout_font_size_overrides_.SetMax(max_length);
  layout_weights_.SetMax(max_length);
  layout_fill_styles_.SetMax(max_length);
  layout_resolved_typefaces_.SetMax(max_length);
  layout_stroke_widths_.SetMax(max_length);
  for (auto& layout_style : layout_styles_)
    layout_style.SetMax(max_length);
}

int RenderText::GetLineContainingYCoord(float text_y) {
  if (text_y < 0)
    return -1;

  internal::ShapedText* shaper_text = GetShapedText();
  for (size_t i = 0; i < shaper_text->lines().size(); i++) {
    const internal::Line& line = shaper_text->lines()[i];

    if (text_y <= line.size.height())
      return i;
    text_y -= line.size.height();
  }

  return shaper_text->lines().size();
}

// static
bool RenderText::RangeContainsCaret(const Range& range,
                                    size_t caret_pos,
                                    LogicalCursorDirection caret_affinity) {
  if (caret_pos == 0 && caret_affinity == CURSOR_BACKWARD)
    return false;
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
  return baseline + std::clamp(baseline_shift, min_shift, max_shift);
}

// static
Rect RenderText::ExpandToBeVerticallySymmetric(const Rect& rect,
                                               const Rect& display_rect) {
  // Mirror |rect| across the horizontal line dividing |display_rect| in half.
  Rect result = rect;
  int mid_y = display_rect.CenterPoint().y();
  // The top of the mirror rect must be equidistant with the bottom of the
  // original rect from the mid-line.
  result.set_y(mid_y + (mid_y - rect.bottom()));

  // Now make a union with the original rect to ensure we are encompassing both.
  result.Union(rect);
  return result;
}

// static
void RenderText::MergeIntersectingRects(std::vector<Rect>& rects) {
  if (rects.size() < 2)
    return;

  std::sort(rects.begin(), rects.end(),
            [](const Rect& a, const Rect& b) { return a.x() < b.x(); });

  size_t merge_candidate = 0;
  for (size_t i = 1; i < rects.size(); i++) {
    if (rects[i].Intersects(rects[merge_candidate]) ||
        rects[i].SharesEdgeWith(rects[merge_candidate])) {
      DCHECK_EQ(rects[i].y(), rects[merge_candidate].y());
      DCHECK_EQ(rects[i].height(), rects[merge_candidate].height());
      rects[merge_candidate].Union(rects[i]);
    } else {
      merge_candidate++;
      if (merge_candidate != i)
        rects[merge_candidate] = rects[i];
    }
  }

  rects.resize(merge_candidate + 1);
}

void RenderText::OnTextAttributeChanged() {
  layout_text_.clear();
  display_text_.clear();
  text_elided_ = false;

  layout_text_up_to_date_ = false;
  OnLayoutTextAttributeChanged();
}

std::u16string RenderText::Elide(const std::u16string& text,
                                 float text_width,
                                 float available_width,
                                 ElideBehavior behavior) {
  if (available_width <= 0 || text.empty())
    return std::u16string();
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

  const std::u16string ellipsis = std::u16string(kEllipsisUTF16);
  const bool insert_ellipsis = (behavior != TRUNCATE);
  const bool elide_in_middle = (behavior == ELIDE_MIDDLE);
  const bool elide_at_beginning = (behavior == ELIDE_HEAD);

  if (insert_ellipsis) {
    render_text->SetText(ellipsis);
    const float ellipsis_width = render_text->GetContentWidthF();
    if (ellipsis_width > available_width)
      return std::u16string();
  }

  StringSlicer slicer(text, ellipsis, elide_in_middle, elide_at_beginning,
                      whitespace_elision_);

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
  const base::i18n::TextDirection text_direction = GetTextDirection();
  while (lo <= hi) {
    // Linearly interpolate between |lo| and |hi|, which correspond to widths
    // of |lo_width| and |hi_width| to estimate at what position
    // |available_width| would be at.  Because |lo_width| and |hi_width| are
    // both estimates (may be off by a little because, for example, |lo_width|
    // may have been calculated from |lo| minus one, not |lo|), we clamp to the
    // the valid range.
    // |last_guess| is merely used to verify that we're not repeating guesses.
    const size_t last_guess = guess;
    if (hi_width != lo_width) {
      guess = lo + base::ClampRound<size_t>((available_width - lo_width) *
                                            (hi - lo) / (hi_width - lo_width));
    }
    guess = std::clamp(guess, lo, hi);
    DCHECK_NE(last_guess, guess);

    // Restore colors. They will be truncated to size by SetText.
    render_text->colors_ = colors_;
    std::u16string new_text =
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

      // Ensures that the |new_text| will always be smaller or equal to the
      // original text. There is a corner case when only one character is elided
      // and two characters are added back (ellipsis and directional marker).
      if (trailing_text_direction != text_direction &&
          new_text.length() + 2 > text.length() && guess >= 1) {
        new_text = slicer.CutString(guess - 1, false);
        trailing_text_direction =
            base::i18n::GetLastStrongCharacterDirection(new_text);
      }

      // Append the ellipsis and the optional directional marker characters.
      // Do not append the BiDi marker if the only codepoint in the text is
      // an ellipsis.
      new_text.append(ellipsis);
      if (new_text.size() != 1 && trailing_text_direction != text_direction) {
        if (trailing_text_direction == base::i18n::LEFT_TO_RIGHT)
          new_text += base::i18n::kLeftToRightMark;
        else
          new_text += base::i18n::kRightToLeftMark;
      }
    }

    // The elided text must be smaller in bytes. Otherwise, break-lists are not
    // consistent and the characters after the last range are not styled.
    DCHECK_LE(new_text.size(), text.size());
    render_text->SetText(std::move(new_text));

    // Restore styles and baselines without breaking multi-character graphemes.
    render_text->styles_ = styles_;
    for (auto& style : render_text->styles_)
      RestoreBreakList(render_text.get(), &style);
    RestoreBreakList(render_text.get(), &render_text->baselines_);
    RestoreBreakList(render_text.get(), &render_text->font_size_overrides_);
    render_text->weights_ = weights_;
    RestoreBreakList(render_text.get(), &render_text->weights_);
    RestoreBreakList(render_text.get(), &render_text->resolved_typefaces_);
    RestoreBreakList(render_text.get(), &render_text->fill_styles_);
    RestoreBreakList(render_text.get(), &render_text->stroke_widths_);

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

std::u16string RenderText::ElideEmail(const std::u16string& email,
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
  if (split_index == std::u16string::npos)
    return Elide(email, 0, available_width, ELIDE_TAIL);

  std::u16string username = email.substr(0, split_index);
  std::u16string domain = email.substr(split_index + 1);

  // TODO(http://crbug.com/1085014): Fix eliding of text with styles.
  DCHECK(IsHomogeneous())
      << "ElideEmail(...) doesn't work with non homogeneous styles.";
  auto render_text = CreateInstanceOfSameStyle(std::u16string());
  auto get_string_width = [&](const std::u16string& text) {
    render_text->SetText(text);
    return render_text->GetStringSizeF().width();
  };

  // Subtract the @ symbol from the available width as it is mandatory.
  const std::u16string kAtSignUTF16 = u"@";
  float at_width = get_string_width(kAtSignUTF16);
  if (available_width < at_width)
    return Elide(kEllipsisUTF16, 0, available_width, ELIDE_TAIL);
  const float remaining_width = available_width - at_width;

  // Handle corner cases where one of username or domain is empty.
  if (username.empty() && domain.empty()) {
    return Elide(email, 0, available_width, ELIDE_TAIL);
  } else if (username.empty()) {
    domain = Elide(domain, 0, remaining_width, ELIDE_MIDDLE);
    if (domain.empty() || domain == kEllipsisUTF16)
      return Elide(kEllipsisUTF16, 0, available_width, ELIDE_TAIL);
    return kAtSignUTF16 + domain;
  } else if (domain.empty()) {
    username = Elide(username, 0, remaining_width, ELIDE_TAIL);
    if (username.empty() || username == kEllipsisUTF16)
      return Elide(kEllipsisUTF16, 0, available_width, ELIDE_TAIL);
    return username + kAtSignUTF16;
  }

  // Check whether eliding the domain is necessary: if eliding the username
  // is sufficient, the domain will not be elided.
  const float full_username_width = get_string_width(username);
  const float available_domain_width =
      remaining_width -
      std::min(full_username_width,
               get_string_width(username.substr(0, 1) + kEllipsisUTF16));
  if (get_string_width(domain) > available_domain_width) {
    // Elide the domain so that it only takes half of the available width.
    // Should the username not need all the width available in its half, the
    // domain will occupy the leftover width.
    // If |desired_domain_width| is greater than |available_domain_width|: the
    // minimal username elision allowed by the specifications will not fit; thus
    // |desired_domain_width| must be <= |available_domain_width| at all cost.
    const float desired_domain_width =
        std::min<float>(available_domain_width,
                        std::max<float>(remaining_width - full_username_width,
                                        remaining_width / 2));
    domain = Elide(domain, 0, desired_domain_width, ELIDE_MIDDLE);
    // Failing to elide the domain such that at least one character remains
    // (other than the ellipsis itself) remains: return a single ellipsis.
    if (domain.empty() || domain == kEllipsisUTF16)
      return Elide(kEllipsisUTF16, 0, available_width, ELIDE_TAIL);
  }

  // Fit the username in the remaining width (at this point the elided username
  // is guaranteed to fit with at least one character remaining given all the
  // precautions taken earlier).
  const float domain_width = get_string_width(domain);
  const float available_username_width = remaining_width - domain_width;
  username = Elide(username, 0, available_username_width, ELIDE_TAIL);

  return username + kAtSignUTF16 + domain;
}

void RenderText::UpdateCachedBoundsAndOffset() {
  if (cached_bounds_and_offset_valid_)
    return;

  int delta_x = 0;
  int delta_y = 0;

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

    if (vertical_alignment_ == ALIGN_TOP) {
      if (cursor_bounds_.bottom() > display_rect_.bottom())
        delta_y = display_rect_.bottom() - cursor_bounds_.bottom();
      else if (cursor_bounds_.y() < display_rect_.y())
        delta_y = display_rect_.y() - cursor_bounds_.y();
    }
  }

  SetDisplayOffset(display_offset_ + Vector2d(delta_x, delta_y));
}

internal::GraphemeIterator RenderText::GetGraphemeIteratorAtIndex(
    const std::u16string& text,
    const size_t internal::TextToDisplayIndex::*field,
    size_t index) const {
  DCHECK_LE(index, text.length());
  if (index == text.length())
    return text_to_display_indices_.end();

  CHECK(layout_text_up_to_date_);
  CHECK(!text_to_display_indices_.empty());

  // The function std::lower_bound(...) finds the first not less than |index|.
  internal::GraphemeIterator iter = std::lower_bound(
      text_to_display_indices_.begin(), text_to_display_indices_.end(), index,
      [field](const internal::TextToDisplayIndex& lhs, size_t rhs) {
        return lhs.*field < rhs;
      });

  if (iter == text_to_display_indices_.end() || *iter.*field != index) {
    CHECK(iter != text_to_display_indices_.begin());
    --iter;
  }

  return iter;
}

void RenderText::DrawSelections(Canvas* canvas,
                                const std::vector<Range>& selections) {
  for (auto selection : selections) {
    if (!selection.is_empty()) {
      for (Rect s : GetSubstringBounds(selection)) {
        if (symmetric_selection_visual_bounds() && !multiline())
          s = ExpandToBeVerticallySymmetric(s, display_rect());
        canvas->FillRect(s, selection_background_focused_color_);
      }
    }
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
  for (int i = static_cast<int>(std::min(index, length - 1)); i >= 0; i--)
    if (iter.IsStartOfWord(i))
      return i;

  for (size_t i = index + 1; i < length; i++)
    if (iter.IsStartOfWord(i))
      return i;

  return length;
}

}  // namespace gfx
