// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_TEST_API_H_
#define UI_GFX_RENDER_TEXT_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/break_list.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/selection_model.h"

namespace gfx {
namespace test {

class RenderTextTestApi {
 public:
  RenderTextTestApi(RenderText* render_text) : render_text_(render_text) {}

  RenderTextTestApi(const RenderTextTestApi&) = delete;
  RenderTextTestApi& operator=(const RenderTextTestApi&) = delete;

  static const cc::PaintFlags& GetRendererPaint(
      internal::SkiaTextRenderer* renderer) {
    return renderer->flags_;
  }

  static const SkFont& GetRendererFont(internal::SkiaTextRenderer* renderer) {
    return renderer->font_;
  }

  // Callers must ensure that the associated RenderText object is a
  // RenderTextHarfBuzz instance.
  internal::TextRunList* GetHarfBuzzRunList() {
    return render_text_->GetRunList();
  }

  void DrawVisualText(internal::SkiaTextRenderer* renderer,
                      const std::vector<Range> selection) {
    render_text_->DrawVisualText(renderer, selection);
  }

  void Draw(Canvas* canvas, bool select_all = false) {
    render_text_->Draw(canvas, select_all);
  }

  HorizontalAlignment GetCurrentHorizontalAlignment() {
    return render_text_->GetCurrentHorizontalAlignment();
  }

  const std::u16string& GetLayoutText() {
    return render_text_->GetLayoutText();
  }

  const BreakList<SkColor>& colors() const { return render_text_->colors(); }

  const BreakList<BaselineStyle>& baselines() const {
    return render_text_->baselines();
  }

  const BreakList<int>& font_size_overrides() const {
    return render_text_->font_size_overrides();
  }

  const BreakList<Font::Weight>& weights() const {
    return render_text_->weights();
  }

  const BreakList<cc::PaintFlags::Style>& fill_styles() const {
    return render_text_->fill_styles();
  }

  const BreakList<SkScalar>& stroke_widths() const {
    return render_text_->stroke_widths();
  }

  const internal::StyleArray& styles() const { return render_text_->styles(); }

  const std::vector<internal::Line>& lines() const {
    return render_text_->GetShapedText()->lines();
  }

  const Vector2d& display_offset() const {
    return render_text_->display_offset_;
  }

  SelectionModel EdgeSelectionModel(VisualCursorDirection direction) {
    return render_text_->EdgeSelectionModel(direction);
  }

  size_t TextIndexToDisplayIndex(size_t index) {
    return render_text_->TextIndexToDisplayIndex(index);
  }

  size_t DisplayIndexToTextIndex(size_t index) {
    return render_text_->DisplayIndexToTextIndex(index);
  }

  void EnsureLayout() { render_text_->EnsureLayout(); }

  Vector2d GetAlignmentOffset(size_t line_number) {
    return render_text_->GetAlignmentOffset(line_number);
  }

  int GetDisplayTextBaseline() {
    return render_text_->GetDisplayTextBaseline();
  }

  void SetGlyphWidth(float test_width) {
    render_text_->set_glyph_width_for_test(test_width);
  }

  void SetGlyphHeight(float test_height) {
    render_text_->set_glyph_height_for_test(test_height);
  }

  static gfx::Rect ExpandToBeVerticallySymmetric(
      const gfx::Rect& rect,
      const gfx::Rect& display_rect) {
    return RenderText::ExpandToBeVerticallySymmetric(rect, display_rect);
  }

  static void MergeIntersectingRects(std::vector<Rect>& rects) {
    RenderText::MergeIntersectingRects(rects);
  }

  void reset_cached_cursor_x() { render_text_->reset_cached_cursor_x(); }

  int GetLineContainingYCoord(float text_y) {
    return render_text_->GetLineContainingYCoord(text_y);
  }

 private:
  raw_ptr<RenderText> render_text_;
};

}  // namespace test
}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_TEST_API_H_
