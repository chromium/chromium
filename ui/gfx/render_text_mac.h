// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_MAC_H_
#define UI_GFX_RENDER_TEXT_MAC_H_

#include <ApplicationServices/ApplicationServices.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/render_text.h"

namespace gfx {

// RenderTextMac is the Mac implementation of RenderText that uses CoreText for
// layout and Skia for drawing.
//
// Note: The current implementation only supports drawing and sizing the text,
//       but not text selection or cursor movement.
class GFX_EXPORT RenderTextMac : public RenderText {
 public:
  RenderTextMac();
  ~RenderTextMac() override;

  // RenderText:
  std::unique_ptr<RenderText> CreateInstanceOfSameType() const override;
  void SetFontList(const FontList& font_list) override;
  bool MultilineSupported() const override;
  const base::string16& GetDisplayText() override;
  Size GetStringSize() override;
  SizeF GetStringSizeF() override;
  SelectionModel FindCursorPosition(const Point& point,
                                    const Point& drag_origin) override;
  bool IsSelectionSupported() const override;
  std::vector<FontSpan> GetFontSpansForTesting() override;
  size_t GetLineContainingCaret(const SelectionModel& caret) override;

 protected:
  // RenderText:
  int GetDisplayTextBaseline() override;
  SelectionModel AdjacentCharSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) override;
  SelectionModel AdjacentWordSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) override;
  SelectionModel AdjacentLineSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) override;
  RangeF GetCursorSpan(const Range& text_range) override;
  std::vector<Rect> GetSubstringBounds(const Range& range) override;
  bool IsValidCursorIndex(size_t index) override;
  void OnLayoutTextAttributeChanged(bool text_changed) override;
  void OnDisplayTextAttributeChanged() override;
  void OnTextColorChanged() override;
  void EnsureLayout() override;
  void DrawVisualText(internal::SkiaTextRenderer* renderer) override;

 private:
  friend class RenderTextMacTest;

  struct TextRun {
    CTRunRef ct_run;
    SkPoint origin;
    std::vector<uint16_t> glyphs;
    std::vector<SkPoint> glyph_positions;
    SkScalar width;
    base::ScopedCFTypeRef<CTFontRef> ct_font;
    sk_sp<SkTypeface> typeface;
    SkColor foreground;
    bool underline;
    bool strike;

    TextRun();
    TextRun(const TextRun& other) = delete;
    TextRun(TextRun&& other);
    ~TextRun();
  };

  // Returns the width used to draw |layout_text_|.
  float GetLayoutTextWidth();

  // Computes the size used to draw |line|. Stores the baseline position into
  // |baseline|.
  gfx::SizeF GetCTLineSize(CTLineRef line, SkScalar* baseline);

  // Creates Core Text line object and its attributes for the given text and
  // returns the line. |attributes| keeps the ownership of the text attributes.
  // See the comments of ArrayStyles() implementation for the ownership details.
  base::ScopedCFTypeRef<CTLineRef> EnsureLayoutInternal(
      const base::string16& text,
      base::ScopedCFTypeRef<CFMutableArrayRef>* attributes);

  // Applies RenderText styles to |attr_string| with the given |ct_font|.
  // Returns the array of attributes to keep the ownership of the attributes.
  // See the comments in .cc file for the details.
  base::ScopedCFTypeRef<CFMutableArrayRef> ApplyStyles(
      const base::string16& text,
      CFMutableAttributedStringRef attr_string,
      CTFontRef ct_font);

  // Updates |runs_| based on |line_| and sets |runs_valid_| to true.
  void ComputeRuns();

  // Clears cached style. Doesn't update display text (e.g. eliding).
  void InvalidateStyle();

  // RenderText:
  bool GetDecoratedTextForRange(const Range& range,
                                DecoratedText* decorated_text) override;

  // The Core Text line of text. Created by |EnsureLayout()|.
  base::ScopedCFTypeRef<CTLineRef> line_;

  // Array to hold CFAttributedString attributes that allows Core Text to hold
  // weak references to them without leaking.
  base::ScopedCFTypeRef<CFMutableArrayRef> attributes_;

  // Visual dimensions of the text. Computed by |EnsureLayout()|.
  SizeF string_size_;

  // Common baseline for this line of text. Computed by |EnsureLayout()|.
  SkScalar common_baseline_;

  // Visual text runs. Only valid if |runs_valid_| is true. Computed by
  // |ComputeRuns()|.
  std::vector<TextRun> runs_;

  // Indicates that |runs_| are valid, set by |ComputeRuns()|.
  bool runs_valid_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextMac);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_MAC_H_
