// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_H_
#define UI_GFX_RENDER_TEXT_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/break_list.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/range/range_f.h"
#include "ui/gfx/selection_model.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/text_constants.h"

class SkDrawLooper;
struct SkPoint;
class SkTypeface;

namespace gfx {
namespace test {
class RenderTextTestApi;
}

class Canvas;
struct DecoratedText;
class Font;

namespace internal {

class TextRunList;

// Internal helper class used by derived classes to draw text through Skia.
class GFX_EXPORT SkiaTextRenderer {
 public:
  explicit SkiaTextRenderer(Canvas* canvas);
  virtual ~SkiaTextRenderer();

  void SetDrawLooper(sk_sp<SkDrawLooper> draw_looper);
  void SetFontRenderParams(const FontRenderParams& params,
                           bool subpixel_rendering_suppressed);
  void SetTypeface(sk_sp<SkTypeface> typeface);
  void SetTextSize(SkScalar size);
  void SetForegroundColor(SkColor foreground);
  void SetShader(sk_sp<cc::PaintShader> shader);
  void DrawSelection(const std::vector<Rect>& selection, SkColor color);
  // TODO(vmpstr): Change this API to mimic SkCanvas::drawTextBlob instead.
  virtual void DrawPosText(const SkPoint* pos,
                           const uint16_t* glyphs,
                           size_t glyph_count);
  void DrawUnderline(int x, int y, int width, SkScalar thickness_factor = 1.0);
  void DrawStrike(int x, int y, int width, SkScalar thickness_factor);

 private:
  friend class test::RenderTextTestApi;

  Canvas* canvas_;
  cc::PaintCanvas* canvas_skia_;
  cc::PaintFlags flags_;
  SkFont font_;

  DISALLOW_COPY_AND_ASSIGN(SkiaTextRenderer);
};

// Internal helper class used to iterate colors, baselines, and styles.
class StyleIterator {
 public:
  StyleIterator(const BreakList<SkColor>& colors,
                const BreakList<BaselineStyle>& baselines,
                const BreakList<int>& font_size_overrides,
                const BreakList<Font::Weight>& weights,
                const std::vector<BreakList<bool>>& styles);
  ~StyleIterator();

  // Get the colors and styles at the current iterator position.
  SkColor color() const { return color_->second; }
  BaselineStyle baseline() const { return baseline_->second; }
  int font_size_override() const { return font_size_override_->second; }
  bool style(TextStyle s) const { return style_[s]->second; }
  Font::Weight weight() const { return weight_->second; }

  // Get the intersecting range of the current iterator set.
  Range GetRange() const;

  // Update the iterator to point to colors and styles applicable at |position|.
  void UpdatePosition(size_t position);

 private:
  BreakList<SkColor> colors_;
  BreakList<BaselineStyle> baselines_;
  BreakList<int> font_size_overrides_;
  BreakList<Font::Weight> weights_;
  std::vector<BreakList<bool> > styles_;

  BreakList<SkColor>::const_iterator color_;
  BreakList<BaselineStyle>::const_iterator baseline_;
  BreakList<int>::const_iterator font_size_override_;
  BreakList<Font::Weight>::const_iterator weight_;
  std::vector<BreakList<bool>::const_iterator> style_;

  DISALLOW_COPY_AND_ASSIGN(StyleIterator);
};

// Line segments are slices of the display text to be rendered on a single line.
struct LineSegment {
  LineSegment();
  ~LineSegment();

  // X coordinates of this line segment in text space.
  RangeF x_range;

  // The character range this segment corresponds to.
  Range char_range;

  // Index of the text run that generated this segment.
  size_t run;

  // Returns the width of this line segment in text space.
  float width() const { return x_range.length(); }
};

// A line of display text, comprised of a line segment list and some metrics.
struct Line {
  Line();
  Line(const Line& other);
  ~Line();

  // Segments that make up this line in visual order.
  std::vector<LineSegment> segments;

  // The sum of segment widths and the maximum of segment heights.
  SizeF size;

  // Sum of preceding lines' heights.
  int preceding_heights;

  // Maximum baseline of all segments on this line.
  int baseline;
};

// Creates an SkTypeface from a font, |italic| and a desired |weight|.
// May return null.
sk_sp<SkTypeface> CreateSkiaTypeface(const Font& font,
                                     bool italic,
                                     Font::Weight weight);

// Applies the given FontRenderParams to the SkFont.
void ApplyRenderParams(const FontRenderParams& params,
                       bool subpixel_rendering_suppressed,
                       SkFont* font);

}  // namespace internal

// RenderText represents an abstract model of styled text and its corresponding
// visual layout. Support is built in for a cursor, a selection, simple styling,
// complex scripts, and bi-directional text. Implementations provide mechanisms
// for rendering and translation between logical and visual data.
class GFX_EXPORT RenderText {
 public:
#if defined(OS_MACOSX)
  // On Mac, while selecting text if the cursor is outside the vertical text
  // bounds, drag to the end of the text.
  static constexpr bool kDragToEndIfOutsideVerticalBounds = true;
  // Mac supports a selection that is "undirected". When undirected, the cursor
  // doesn't know which end of the selection it's at until it first moves.
  static constexpr bool kSelectionIsAlwaysDirected = false;
#else
  static constexpr bool kDragToEndIfOutsideVerticalBounds = false;
  static constexpr bool kSelectionIsAlwaysDirected = true;
#endif

  // The character used for displaying obscured text. Use a bullet character.
  // TODO(pbos): This is highly font dependent, consider replacing the character
  // with a vector glyph.
  static constexpr base::char16 kPasswordReplacementChar = 0x2022;

  virtual ~RenderText();

  // Creates an instance that renders using HarfBuzz.
  static std::unique_ptr<RenderText> CreateHarfBuzzInstance();

  // Creates an instance by matching a gfx::Typesetter constant to the current
  // build and runtime configuration. E.g. the result may use CoreText on Mac,
  // which is the only supported native typesetter.
  static std::unique_ptr<RenderText> CreateFor(Typesetter typesetter);

  // Creates another instance of the same concrete class.
  virtual std::unique_ptr<RenderText> CreateInstanceOfSameType() const = 0;

  // Like above but copies all style settings too.
  std::unique_ptr<RenderText> CreateInstanceOfSameStyle(
      const base::string16& text) const;

  const base::string16& text() const { return text_; }
  void SetText(const base::string16& text);
  void AppendText(const base::string16& text);

  HorizontalAlignment horizontal_alignment() const {
    return horizontal_alignment_;
  }
  void SetHorizontalAlignment(HorizontalAlignment alignment);

  VerticalAlignment vertical_alignment() const { return vertical_alignment_; }
  void SetVerticalAlignment(VerticalAlignment alignment);

  const FontList& font_list() const { return font_list_; }
  virtual void SetFontList(const FontList& font_list);

  bool cursor_enabled() const { return cursor_enabled_; }
  void SetCursorEnabled(bool cursor_enabled);

  SkColor selection_color() const { return selection_color_; }
  void set_selection_color(SkColor color) { selection_color_ = color; }

  SkColor selection_background_focused_color() const {
    return selection_background_focused_color_;
  }
  void set_selection_background_focused_color(SkColor color) {
    selection_background_focused_color_ = color;
  }

  bool symmetric_selection_visual_bounds() const {
    return symmetric_selection_visual_bounds_;
  }
  void set_symmetric_selection_visual_bounds(bool symmetric) {
    symmetric_selection_visual_bounds_ = symmetric;
  }

  bool focused() const { return focused_; }
  void set_focused(bool focused) { focused_ = focused; }

  bool clip_to_display_rect() const { return clip_to_display_rect_; }
  void set_clip_to_display_rect(bool clip) { clip_to_display_rect_ = clip; }

  int glyph_spacing() const { return glyph_spacing_; }
  void set_glyph_spacing(int spacing) { glyph_spacing_ = spacing; }

  // In an obscured (password) field, all text is drawn as bullets.
  bool obscured() const { return obscured_; }
  void SetObscured(bool obscured);

  // Makes a char in obscured text at |index| to be revealed. |index| should be
  // a UTF16 text index. If there is a previous revealed index, the previous one
  // is cleared and only the last set index will be revealed. If |index| is -1
  // or out of range, no char will be revealed. The revealed index is also
  // cleared when SetText or SetObscured is called.
  void SetObscuredRevealIndex(int index);

  // TODO(ckocagil): Multiline text rendering is not supported on Mac.
  bool multiline() const { return multiline_; }
  void SetMultiline(bool multiline);

  // If multiline, a non-zero value will cap the number of lines rendered,
  // and elide the rest (currently only ELIDE_TAIL supported.)
  void SetMaxLines(size_t max_lines);
  size_t max_lines() const { return max_lines_; }

  // Returns the actual number of lines, broken by |lines_|.
  size_t GetNumLines();

  // TODO(mukai): ELIDE_LONG_WORDS is not supported.
  WordWrapBehavior word_wrap_behavior() const { return word_wrap_behavior_; }
  void SetWordWrapBehavior(WordWrapBehavior behavior);

  // Returns true if this instance supports multiline rendering.
  virtual bool MultilineSupported() const = 0;

  // TODO(ckocagil): Add vertical alignment and line spacing support instead.
  int min_line_height() const { return min_line_height_; }
  void SetMinLineHeight(int line_height);

  // Set the maximum length of the layout text, not the actual text.
  // A |length| of 0 forgoes a hard limit, but does not guarantee proper
  // functionality of very long strings. Applies to subsequent SetText calls.
  // WARNING: Only use this for system limits, it lacks complex text support.
  void set_truncate_length(size_t length) { truncate_length_ = length; }

  // The display text will be elided to fit |display_rect| using this behavior.
  void SetElideBehavior(ElideBehavior elide_behavior);
  ElideBehavior elide_behavior() const { return elide_behavior_; }

  // When display text is elided, determines how whitespace is handled.
  // If base::nullopt is specified, the default elision for the current elide
  // behavior will be applied.
  void SetWhitespaceElision(base::Optional<bool> elide_whitespace);
  base::Optional<bool> whitespace_elision() const {
    return whitespace_elision_;
  }

  const Rect& display_rect() const { return display_rect_; }
  void SetDisplayRect(const Rect& r);

  bool subpixel_rendering_suppressed() const {
    return subpixel_rendering_suppressed_;
  }
  void set_subpixel_rendering_suppressed(bool suppressed) {
    subpixel_rendering_suppressed_ = suppressed;
  }

  const SelectionModel& selection_model() const { return selection_model_; }
  const Range& selection() const { return selection_model_.selection(); }
  size_t cursor_position() const { return selection_model_.caret_pos(); }

  // Set the cursor to |position|, with the caret affinity trailing the previous
  // grapheme, or if there is no previous grapheme, leading the cursor position.
  // See SelectionModel::caret_affinity_ for details.
  void SetCursorPosition(size_t position);

  // Moves the cursor left or right. Cursor movement is visual, meaning that
  // left and right are relative to screen, not the directionality of the text.
  // |selection_behavior| determines whether a selection is to be made and it's
  // behavior.
  void MoveCursor(BreakType break_type,
                  VisualCursorDirection direction,
                  SelectionBehavior selection_behavior);

  // Set the selection_model_ to the value of |selection|.
  // The selection range is clamped to text().length() if out of range.
  // Returns true if the cursor position or selection range changed.
  // If any index in |selection_model| is not a cursorable position (not on a
  // grapheme boundary), it is a no-op and returns false.
  bool SetSelection(const SelectionModel& selection);

  // Moves the cursor to the text index corresponding to |point|. If |select| is
  // true, a selection is made with the current selection start index. If the
  // resultant text indices do not lie on valid grapheme boundaries, it is a no-
  // op and returns false. If this move is happening because of a drag causing a
  // selection change, and |drag_origin| is not the zero point, then
  // |drag_origin| overrides the default origin for a select-to-drag
  // (usually the existing text insertion cursor).
  bool MoveCursorToPoint(const gfx::Point& point,
                         bool select,
                         const gfx::Point& drag_origin = gfx::Point());

  // Set the selection_model_ based on |range|.
  // If the |range| start or end is greater than text length, it is modified
  // to be the text length.
  // If the |range| start or end is not a cursorable position (not on grapheme
  // boundary), it is a NO-OP and returns false. Otherwise, returns true.
  bool SelectRange(const Range& range);

  // Returns true if the local point is over selected text.
  bool IsPointInSelection(const Point& point);

  // Selects no text, keeping the current cursor position and caret affinity.
  void ClearSelection();

  // Select the entire text range. If |reversed| is true, the range will end at
  // the logical beginning of the text; this generally shows the leading portion
  // of text that overflows its display area.
  void SelectAll(bool reversed);

  // Selects the word at the current cursor position. If there is a non-empty
  // selection, the selection bounds are extended to their nearest word
  // boundaries.
  void SelectWord();

  void SetCompositionRange(const Range& composition_range);

  // Set the text color over the entire text or a logical character range.
  // The |range| should be valid, non-reversed, and within [0, text().length()].
  void SetColor(SkColor value);
  void ApplyColor(SkColor value, const Range& range);

  // DEPRECATED.
  // Set the baseline style over the entire text or a logical character range.
  // The |range| should be valid, non-reversed, and within [0, text().length()].
  // TODO(tapted): Remove this. The only client is moving to
  // ApplyFontSizeOverride.
  void SetBaselineStyle(BaselineStyle value);
  void ApplyBaselineStyle(BaselineStyle value, const Range& range);

  // Alters the font size in |range|.
  void ApplyFontSizeOverride(int font_size_override, const Range& range);

  // Set various text styles over the entire text or a logical character range.
  // The respective |style| is applied if |value| is true, or removed if false.
  // The |range| should be valid, non-reversed, and within [0, text().length()].
  void SetStyle(TextStyle style, bool value);
  void ApplyStyle(TextStyle style, bool value, const Range& range);

  void SetWeight(Font::Weight weight);
  void ApplyWeight(Font::Weight weight, const Range& range);

  // Returns whether this style is enabled consistently across the entire
  // RenderText.
  bool GetStyle(TextStyle style) const;

  // Set or get the text directionality mode and get the text direction yielded.
  void SetDirectionalityMode(DirectionalityMode mode);
  DirectionalityMode directionality_mode() const {
      return directionality_mode_;
  }
  base::i18n::TextDirection GetDisplayTextDirection();

  // Returns the visual movement direction corresponding to the logical
  // end/beginning of the text, considering only the dominant direction returned
  // by |GetDisplayTextDirection()|, not the direction of a particular run.
  VisualCursorDirection GetVisualDirectionOfLogicalEnd();
  VisualCursorDirection GetVisualDirectionOfLogicalBeginning();

  // Returns the text used to display, which may be obscured, truncated or
  // elided. The subclass may compute elided text on the fly, or use
  // precomputed the elided text.
  virtual const base::string16& GetDisplayText() = 0;

  // Returns the size required to display the current string (which is the
  // wrapped size in multiline mode). The returned size does not include space
  // reserved for the cursor or the offset text shadows.
  virtual Size GetStringSize() = 0;

  // This is same as GetStringSize except that fractional size is returned.
  // The default implementation is same as GetStringSize. Certain platforms that
  // compute the text size as floating-point values, like Mac, will override
  // this method.
  // See comment in Canvas::GetStringWidthF for its usage.
  virtual SizeF GetStringSizeF();

  // Returns the size of the line containing |caret|.
  virtual Size GetLineSize(const SelectionModel& caret);

  // Returns the sum of all the line widths.
  virtual float TotalLineWidth();

  // Returns the width of the content (which is the wrapped width in multiline
  // mode). Reserves room for the cursor if |cursor_enabled_| is true.
  float GetContentWidthF();

  // Same as GetContentWidthF with the width rounded up.
  int GetContentWidth();

  // Returns the common baseline of the text. The return value is the vertical
  // offset from the top of |display_rect_| to the text baseline, in pixels.
  // The baseline is determined from the font list and display rect, and does
  // not depend on the text.
  int GetBaseline();

  void Draw(Canvas* canvas);

  // Gets the SelectionModel from a visual point in local coordinates. If
  // |drag_origin| is nonzero, it is used as the baseline for
  // out-of-vertical-bounds drags on platforms that have them, instead of the
  // default origin (the insertion cursor's position).
  virtual SelectionModel FindCursorPosition(
      const Point& point,
      const Point& drag_origin = gfx::Point()) = 0;

  // Returns true if the position is a valid logical index into text(), and is
  // also a valid grapheme boundary, which may be used as a cursor position.
  virtual bool IsValidCursorIndex(size_t index) = 0;

  // Returns true if the position is a valid logical index into text(). Indices
  // amid multi-character graphemes are allowed here, unlike IsValidCursorIndex.
  virtual bool IsValidLogicalIndex(size_t index) const;

  // Returns true if this instance supports text selection.
  virtual bool IsSelectionSupported() const = 0;

  // Get the visual bounds of a cursor at |caret|. These bounds typically
  // represent a vertical line if |insert_mode| is true. Pass false for
  // |insert_mode| to retrieve the bounds of the associated glyph. These bounds
  // are in local coordinates, but may be outside the visible region if the text
  // is longer than the textfield. Subsequent text, cursor, or bounds changes
  // may invalidate returned values. Note that |caret| must be placed at
  // grapheme boundary, i.e. caret.caret_pos() must be a cursorable position.
  // TODO(crbug.com/248597): Add multiline support.
  Rect GetCursorBounds(const SelectionModel& caret, bool insert_mode);

  // Compute the current cursor bounds, panning the text to show the cursor in
  // the display rect if necessary. These bounds are in local coordinates.
  // Subsequent text, cursor, or bounds changes may invalidate returned values.
  const Rect& GetUpdatedCursorBounds();

  // Given an |index| in text(), return the next or previous grapheme boundary
  // in logical order (i.e. the nearest cursorable index). The return value is
  // in the range 0 to text().length() inclusive (the input is clamped if it is
  // out of that range). Always moves by at least one character index unless the
  // supplied index is already at the boundary of the string.
  size_t IndexOfAdjacentGrapheme(size_t index,
                                 LogicalCursorDirection direction);

  // Return a SelectionModel with the cursor at the current selection's start.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel GetSelectionModelForSelectionStart() const;

  // Sets shadows to drawn with text.
  void set_shadows(const ShadowValues& shadows) { shadows_ = shadows; }
  const ShadowValues& shadows() const { return shadows_; }

  typedef std::pair<Font, Range> FontSpan;
  // For testing purposes, returns which fonts were chosen for which parts of
  // the text by returning a vector of Font and Range pairs, where each range
  // specifies the character range for which the corresponding font has been
  // chosen.
  virtual std::vector<FontSpan> GetFontSpansForTesting() = 0;

  // Returns rectangle surrounding the current string (from origin to size)
  RectF GetStringRect();

  // Get the visual bounds containing the logical substring within the |range|.
  // If |range| is empty, the result is empty. These bounds could be visually
  // discontinuous if the substring is split by a LTR/RTL level change.
  // These bounds are in local coordinates, but may be outside the visible
  // region if the text is longer than the textfield. Subsequent text, cursor,
  // or bounds changes may invalidate returned values.
  virtual std::vector<Rect> GetSubstringBounds(const Range& range) = 0;

  // Gets the horizontal span (relative to the left of the text, not the view)
  // of the sequence of glyphs in |text_range|, over which the cursor will
  // jump when breaking by characters. If the glyphs are RTL then the returned
  // Range will have is_reversed() true.  (This does not return a Rect because a
  // Rect can't have a negative width.)
  virtual RangeF GetCursorSpan(const Range& text_range) = 0;

  const Vector2d& GetUpdatedDisplayOffset();
  void SetDisplayOffset(int horizontal_offset);

  // Returns the line offset from the origin after applying the text alignment
  // and the display offset.
  Vector2d GetLineOffset(size_t line_number);

  // Retrieves the word displayed at the given |point| along with its styling
  // information. |point| is in the view's coordinates. If no word is displayed
  // at the point, returns a nearby word. |baseline_point| should correspond to
  // the baseline point of the leftmost glyph of the |word| in the view's
  // coordinates. Returns false, if no word can be retrieved.
  bool GetWordLookupDataAtPoint(const Point& point,
                                DecoratedText* decorated_word,
                                Point* baseline_point);

  // Retrieves the text at |range| along with its styling information.
  // |baseline_point| should correspond to the baseline point of
  // the leftmost glyph of the text in the view's coordinates. If the text
  // spans multiple lines, |baseline_point| will correspond with the leftmost
  // glyph on the first line in the range. Returns false, if no text can be
  // retrieved.
  bool GetLookupDataForRange(const Range& range,
                             DecoratedText* decorated_text,
                             Point* baseline_point);

  // Retrieves the text in the given |range|.
  base::string16 GetTextFromRange(const Range& range) const;

  void set_strike_thickness_factor(SkScalar f) { strike_thickness_factor_ = f; }

  // Return the line index that contains the argument; or the index of the last
  // line if the |caret| exceeds the text length.
  virtual size_t GetLineContainingCaret(const SelectionModel& caret) = 0;

 protected:
  RenderText();

  // Whether |segment| corresponds to the newline character. This uses |text_|
  // to look up the corresponding character.
  bool IsNewlineSegment(const internal::LineSegment& segment) const;

  // Whether |segment| corresponds to the newline character inside |text|.
  bool IsNewlineSegment(const base::string16& text,
                        const internal::LineSegment& segment) const;

  // Returns the character range of segments in |line| excluding the trailing
  // newline segment.
  Range GetLineRange(const base::string16& text,
                     const internal::Line& line) const;

  // NOTE: The value of these accessors may be stale. Please make sure
  // that these fields are up to date before accessing them.
  const base::string16& layout_text() const { return layout_text_; }
  const base::string16& display_text() const { return display_text_; }
  bool text_elided() const { return text_elided_; }

  const BreakList<SkColor>& colors() const { return colors_; }
  const BreakList<BaselineStyle>& baselines() const { return baselines_; }
  const BreakList<int>& font_size_overrides() const {
    return font_size_overrides_;
  }
  const BreakList<Font::Weight>& weights() const { return weights_; }
  const std::vector<BreakList<bool> >& styles() const { return styles_; }
  SkScalar strike_thickness_factor() const { return strike_thickness_factor_; }

  // Whether all the BreakLists have only one break.
  bool IsHomogeneous() const;

  const std::vector<internal::Line>& lines() const { return lines_; }
  void set_lines(std::vector<internal::Line>* lines) { lines_.swap(*lines); }

  // Returns the baseline of the current text.  The return value depends on
  // the text and its layout while the return value of GetBaseline() doesn't.
  // GetAlignmentOffset() takes into account the difference between them.
  //
  // We'd like a RenderText to show the text always on the same baseline
  // regardless of the text, so the text does not jump up or down depending
  // on the text.  However, underlying layout engines return different baselines
  // depending on the text.  In general, layout engines determine the minimum
  // bounding box for the text and return the baseline from the top of the
  // bounding box.  So the baseline changes depending on font metrics used to
  // layout the text.
  //
  // For example, suppose there are FontA and FontB and the baseline of FontA
  // is smaller than the one of FontB.  If the text is laid out only with FontA,
  // then the baseline of FontA may be returned.  If the text includes some
  // characters which are laid out with FontB, then the baseline of FontB may
  // be returned.
  //
  // GetBaseline() returns the fixed baseline regardless of the text.
  // GetDisplayTextBaseline() returns the baseline determined by the underlying
  // layout engine, and it changes depending on the text.  GetAlignmentOffset()
  // returns the difference between them.
  virtual int GetDisplayTextBaseline() = 0;

  void set_cached_bounds_and_offset_valid(bool valid) {
    cached_bounds_and_offset_valid_ = valid;
  }

  // Get the selection model that visually neighbors |position| by |break_type|.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel GetAdjacentSelectionModel(const SelectionModel& current,
                                           BreakType break_type,
                                           VisualCursorDirection direction);

  // Get the selection model visually left/right of |selection| by one grapheme.
  // The returned value represents a cursor/caret position without a selection.
  virtual SelectionModel AdjacentCharSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) = 0;

  // Get the selection model visually left/right of |selection| by one word.
  // The returned value represents a cursor/caret position without a selection.
  virtual SelectionModel AdjacentWordSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) = 0;

  // Get the selection model visually above/below |selection| by one line.
  // The returned value represents a cursor/caret position without a selection.
  virtual SelectionModel AdjacentLineSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) = 0;

  // Get the selection model corresponding to visual text ends.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel EdgeSelectionModel(VisualCursorDirection direction);

  // Get the selection model corresponding to visual text ends for |line_index|.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel LineSelectionModel(size_t line_index,
                                    gfx::VisualCursorDirection direction);

  // Sets the selection model, |model| is assumed to be valid.
  void SetSelectionModel(const SelectionModel& model);

  // Convert between indices into |text_| and indices into
  // GetDisplayText(), which differ when the text is obscured,
  // truncated or elided. Regardless of whether or not the text is
  // obscured, the character (code point) offsets always match.
  size_t TextIndexToDisplayIndex(size_t index);
  size_t DisplayIndexToTextIndex(size_t index);

  // Notifies that layout text, or attributes that affect the layout text
  // shape have changed. |text_changed| is true if the content of the
  // |layout_text_| has changed, not just attributes.
  virtual void OnLayoutTextAttributeChanged(bool text_changed) = 0;

  // Notifies that attributes that affect the display text shape have changed.
  virtual void OnDisplayTextAttributeChanged() = 0;

  // Called when the text color changes.
  virtual void OnTextColorChanged();

  // Ensure the text is laid out, lines are computed, and |lines_| is valid.
  virtual void EnsureLayout() = 0;

  // Draw the text.
  virtual void DrawVisualText(internal::SkiaTextRenderer* renderer) = 0;

  // Update the display text.
  void UpdateDisplayText(float text_width);

  // Returns display text positions that are suitable for breaking lines.
  const BreakList<size_t>& GetLineBreaks();

  // Apply (and undo) temporary composition underlines and selection colors.
  void ApplyCompositionAndSelectionStyles();
  void UndoCompositionAndSelectionStyles();

  // Convert points from the text space to the view space. Handles the display
  // area, display offset, application LTR/RTL mode and multiline.
  Point ToViewPoint(const PointF& point, LogicalCursorDirection caret_affinity);

  // Get the alignment, resolving ALIGN_TO_HEAD with the current text direction.
  HorizontalAlignment GetCurrentHorizontalAlignment();

  // Returns the line offset from the origin, accounts for text alignment only.
  Vector2d GetAlignmentOffset(size_t line_number);

  // Applies fade effects to |renderer|.
  void ApplyFadeEffects(internal::SkiaTextRenderer* renderer);

  // Applies text shadows to |renderer|.
  void ApplyTextShadows(internal::SkiaTextRenderer* renderer);

  // Get the text direction for the current directionality mode and given
  // |text|.
  base::i18n::TextDirection GetTextDirection(const base::string16& text);

  // Convert an index in |text_| to the index in |given_text|. The
  // |given_text| should be either |display_text_| or |layout_text_|
  // depending on the elide state.
  size_t TextIndexToGivenTextIndex(const base::string16& given_text,
                                   size_t index) const;

  // Convert an index in |given_text_| to the index in |text|. The
  // |given_text| should be either |display_text_| or |layout_text_|
  // depending on the elide state.
  size_t GivenTextIndexToTextIndex(const base::string16& given_text,
                                   size_t index) const;

  // Adjust ranged styles to accommodate a new text length.
  void UpdateStyleLengths();

  // Returns the line index for the given argument. |text_y| is relative to
  // the text bounds. Returns -1 if |text_y| is above the text and
  // lines().size() if |text_y| is below it.
  int GetLineContainingYCoord(float text_y);

  // A convenience function to check whether the glyph attached to the caret
  // is within the given range.
  static bool RangeContainsCaret(const Range& range,
                                 size_t caret_pos,
                                 LogicalCursorDirection caret_affinity);

  // Returns the baseline, with which the text best appears vertically centered.
  static int DetermineBaselineCenteringText(const int display_height,
                                            const FontList& font_list);

  // Returns an expanded version of |rect| that is vertically symmetric with
  // respect to the center of |display_rect|.
  static gfx::Rect ExpandToBeVerticallySymmetric(const gfx::Rect& rect,
                                                 const gfx::Rect& display_rect);

  // Resets |cached_cursor_x_| to null. When non-null, CURSOR_UP, CURSOR_DOWN
  // movements use this value instead of the current cursor x position to
  // determine the next cursor x position.
  void reset_cached_cursor_x() { cached_cursor_x_.reset(); }

  void set_cached_cursor_x(int x) { cached_cursor_x_ = x; }
  base::Optional<int> cached_cursor_x() const { return cached_cursor_x_; }

  // Fixed width of glyphs. This should only be set in test environments.
  float glyph_width_for_test_ = 0;

 private:
  friend class test::RenderTextTestApi;

  // Updates |layout_text_| and |display_text_| as needed (or marks them dirty).
  void OnTextAttributeChanged();

  // Elides |text| as needed to fit in the |available_width| using |behavior|.
  // |text_width| is the pre-calculated width of the text shaped by this render
  // text, or pass 0 if the width is unknown.
  base::string16 Elide(const base::string16& text,
                       float text_width,
                       float available_width,
                       ElideBehavior behavior);

  // Elides |email| as needed to fit the |available_width|.
  base::string16 ElideEmail(const base::string16& email, float available_width);

  // Update the cached bounds and display offset to ensure that the current
  // cursor is within the visible display area.
  void UpdateCachedBoundsAndOffset();

  // Draw the selection.
  void DrawSelection(Canvas* canvas);

  // Returns the nearest word start boundary for |index|. First searches in the
  // CURSOR_BACKWARD direction, then in the CURSOR_FORWARD direction. Returns
  // the text length if no valid boundary is found.
  size_t GetNearestWordStartBoundary(size_t index) const;

  // Expands |range| to its nearest word boundaries and returns the resulting
  // range. Maintains directionality of |range|.
  Range ExpandRangeToWordBoundary(const Range& range) const;

  // Expands |range| to its nearest grapheme boundaries and returns the
  // resulting range.
  Range ExpandRangeToGraphemeBoundary(const Range& range);

  // Returns an implementation-specific run list, if implemented.
  virtual internal::TextRunList* GetRunList();
  virtual const internal::TextRunList* GetRunList() const;

  // Returns the decorated text corresponding to |range|. Returns false if the
  // text cannot be retrieved, e.g. if the text is obscured.
  virtual bool GetDecoratedTextForRange(const Range& range,
                                        DecoratedText* decorated_text) = 0;

  // Specify the width of a glyph for test. The width of glyphs is very
  // platform-dependent and environment-dependent. Otherwise multiline text
  // will become really flaky.
  void set_glyph_width_for_test(float width) { glyph_width_for_test_ = width; }

  // Logical UTF-16 string data to be drawn.
  base::string16 text_;

  // Horizontal alignment of the text with respect to |display_rect_|.  The
  // default is to align left if the application UI is LTR and right if RTL.
  HorizontalAlignment horizontal_alignment_;

  // Vertical alignment of the text with respect to |display_rect_|. Only
  // applicable when |multiline_| is true. The default is to align center.
  VerticalAlignment vertical_alignment_;

  // The text directionality mode, defaults to DIRECTIONALITY_FROM_TEXT.
  DirectionalityMode directionality_mode_;

  // The cached text direction, potentially computed from the text or UI locale.
  // Use GetTextDirection(), do not use this potentially invalid value directly!
  base::i18n::TextDirection text_direction_;

  // A list of fonts used to render |text_|.
  FontList font_list_;

  // Logical selection range and visual cursor position.
  SelectionModel selection_model_;

  // The cached cursor bounds; get these bounds with GetUpdatedCursorBounds.
  Rect cursor_bounds_;

  // Specifies whether the cursor is enabled. If disabled, no space is reserved
  // for the cursor when positioning text.
  bool cursor_enabled_;

  // Whether the current selection has a known direction. That is, whether a
  // directional input (e.g. arrow key) has been received for the current
  // selection to indicate which end of the selection has the caret. When true,
  // directed inputs preserve (rather than replace) the selection affinity.
  bool has_directed_selection_;

  // The color used for drawing selected text.
  SkColor selection_color_;

  // The background color used for drawing the selection when focused.
  SkColor selection_background_focused_color_;

  // Whether the selection visual bounds should be expanded vertically to be
  // vertically symmetric with respect to the display rect. Note this flag has
  // no effect on multi-line text.
  bool symmetric_selection_visual_bounds_ = false;

  // The focus state of the text.
  bool focused_;

  // Composition text range.
  Range composition_range_;

  // Color, baseline, and style breaks, used to modify ranges of text.
  // BreakList positions are stored with text indices, not display indices.
  // TODO(msw): Expand to support cursor, selection, background, etc. colors.
  BreakList<SkColor> colors_;
  BreakList<BaselineStyle> baselines_;
  BreakList<int> font_size_overrides_;
  BreakList<Font::Weight> weights_;
  std::vector<BreakList<bool> > styles_;

  // Breaks saved without temporary composition and selection styling.
  BreakList<SkColor> saved_colors_;
  BreakList<bool> saved_underlines_;
  bool composition_and_selection_styles_applied_;

  // A flag to obscure actual text with asterisks for password fields.
  bool obscured_;
  // The index at which the char should be revealed in the obscured text.
  int obscured_reveal_index_;

  // The maximum length of text to display, 0 forgoes a hard limit.
  size_t truncate_length_;

  // The obscured and/or truncated text used to layout the text to display.
  base::string16 layout_text_;

  // The elided text displayed visually. This is empty if the text
  // does not have to be elided, or became empty as a result of eliding.
  // TODO(oshima): When the text is elided, painting can be done only with
  // display text info, so it should be able to clear the |layout_text_| and
  // associated information.
  base::string16 display_text_;

  // The behavior for eliding, fading, or truncating.
  ElideBehavior elide_behavior_;

  // The behavior for eliding whitespace when eliding or truncating.
  base::Optional<bool> whitespace_elision_ = base::nullopt;

  // True if the text is elided given the current behavior and display area.
  bool text_elided_;

  // The minimum height a line should have.
  int min_line_height_;

  // Whether the text should be broken into multiple lines. Uses the width of
  // |display_rect_| as the width cap.
  bool multiline_;

  // If multiple lines, the maximum number of lines to render, or 0.
  size_t max_lines_;

  // The wrap behavior when the text is broken into lines. Do nothing unless
  // |multiline_| is set. The default value is IGNORE_LONG_WORDS.
  WordWrapBehavior word_wrap_behavior_;

  // Set to true to suppress subpixel rendering due to non-font reasons (eg.
  // if the background is transparent). The default value is false.
  bool subpixel_rendering_suppressed_;

  // The local display area for rendering the text.
  Rect display_rect_;

  // Flag to work around a Skia bug with the PDF path (http://crbug.com/133548)
  // that results in incorrect clipping when drawing to the document margins.
  // This field allows disabling clipping to work around the issue.
  // TODO(asvitkine): Remove this when the underlying Skia bug is fixed.
  bool clip_to_display_rect_;

  // The offset for the text to be drawn, relative to the display area.
  // Get this point with GetUpdatedDisplayOffset (or risk using a stale value).
  Vector2d display_offset_;

  // The baseline of the text.  This is determined from the height of the
  // display area and the cap height of the font list so the text is vertically
  // centered.
  int baseline_;

  // The cached bounds and offset are invalidated by changes to the cursor,
  // selection, font, and other operations that adjust the visible text bounds.
  bool cached_bounds_and_offset_valid_;

  // Text shadows to be drawn.
  ShadowValues shadows_;

  // A list of valid display text line break positions.
  BreakList<size_t> line_breaks_;

  // Lines computed by EnsureLayout. These should be invalidated upon
  // OnLayoutTextAttributeChanged and OnDisplayTextAttributeChanged calls.
  std::vector<internal::Line> lines_;

  // The ratio of strike-through line thickness to text height.
  SkScalar strike_thickness_factor_;

  // Extra spacing placed between glyphs; used for obscured text styling.
  int glyph_spacing_ = 0;

  // The cursor position in view space, used to traverse lines of varied widths.
  base::Optional<int> cached_cursor_x_;

  DISALLOW_COPY_AND_ASSIGN(RenderText);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_H_
