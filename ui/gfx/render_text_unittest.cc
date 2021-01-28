// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <numeric>

#include "base/format_macros.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/break_list.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/decorated_text.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_names_testing.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/range/range_f.h"
#include "ui/gfx/render_text_harfbuzz.h"
#include "ui/gfx/render_text_test_api.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_APPLE)
#include "base/mac/mac_util.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::WideToUTF16;

namespace gfx {

namespace {

// Various weak, LTR, RTL, and Bidi string cases with three characters each.
const char kWeak[] = " . ";
const char kLtr[] = "abc";
const char kRtl[] = "\u05d0\u05d1\u05d2";
const char kLtrRtl[] = "a\u05d0\u05d1";
const char kLtrRtlLtr[] = "a\u05d1b";
const char kRtlLtr[] = "\u05d0\u05d1a";
const char kRtlLtrRtl[] = "\u05d0a\u05d1";

constexpr bool kUseWordWrap = true;
constexpr bool kUseObscuredText = true;

// Bitmasks based on gfx::TextStyle.
enum {
  ITALIC_MASK = 1 << TEXT_STYLE_ITALIC,
  STRIKE_MASK = 1 << TEXT_STYLE_STRIKE,
  UNDERLINE_MASK = 1 << TEXT_STYLE_UNDERLINE,
};

using FontSpan = std::pair<Font, Range>;

bool IsFontsSmoothingEnabled() {
#if defined(OS_WIN)
  BOOL antialiasing = TRUE;
  BOOL result = SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &antialiasing, 0);
  if (result == FALSE) {
    ADD_FAILURE() << "Failed to retrieve font aliasing configuration.";
  }
  return antialiasing;
#else
  return true;
#endif
}

// Checks whether |range| contains |index|. This is not the same as calling
// range.Contains(Range(index)), which returns true if |index| == |range.end()|.
bool IndexInRange(const Range& range, size_t index) {
  return index >= range.start() && index < range.end();
}

base::string16 GetSelectedText(RenderText* render_text) {
  return render_text->text().substr(render_text->selection().GetMin(),
                                    render_text->selection().length());
}

// A test utility function to set the application default text direction.
void SetRTL(bool rtl) {
  // Override the current locale/direction.
  base::i18n::SetICUDefaultLocale(rtl ? "he" : "en");
  EXPECT_EQ(rtl, base::i18n::IsRTL());
}

// Execute MoveCursor on the given |render_text| instance for the given
// arguments and verify the selected range matches |expected|. Also, clears the
// expectations.
void RunMoveCursorTestAndClearExpectations(RenderText* render_text,
                                           BreakType break_type,
                                           VisualCursorDirection direction,
                                           SelectionBehavior selection_behavior,
                                           std::vector<Range>* expected) {
  for (size_t i = 0; i < expected->size(); ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "BreakType-%d VisualCursorDirection-%d SelectionBehavior-%d Case-%d.",
        break_type, direction, selection_behavior, static_cast<int>(i)));

    render_text->MoveCursor(break_type, direction, selection_behavior);
    EXPECT_EQ(expected->at(i), render_text->selection());
  }
  expected->clear();
}

// Execute MoveCursor on the given |render_text| instance for the given
// arguments and verify the line matches |expected|. Also, clears the
// expectations.
void RunMoveCursorTestAndClearExpectations(RenderText* render_text,
                                           BreakType break_type,
                                           VisualCursorDirection direction,
                                           SelectionBehavior selection_behavior,
                                           std::vector<size_t>* expected) {
  int case_index = 0;
  for (auto expected_line : *expected) {
    SCOPED_TRACE(testing::Message()
                 << "Text: " << render_text->text() << " BreakType: "
                 << break_type << " VisualCursorDirection: " << direction
                 << " SelectionBehavior: " << selection_behavior
                 << " Case: " << case_index++);
    render_text->MoveCursor(break_type, direction, selection_behavior);
    EXPECT_EQ(expected_line, render_text->GetLineContainingCaret(
                                 render_text->selection_model()));
  }
  expected->clear();
}

// Ensure cursor movement in the specified |direction| yields |expected| values.
void RunMoveCursorLeftRightTest(RenderText* render_text,
                                const std::vector<SelectionModel>& expected,
                                VisualCursorDirection direction) {
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Going %s; expected value index %d.",
        direction == CURSOR_LEFT ? "left" : "right", static_cast<int>(i)));
    EXPECT_EQ(expected[i], render_text->selection_model());
    render_text->MoveCursor(CHARACTER_BREAK, direction, SELECTION_NONE);
  }
  // Check that cursoring is clamped at the line edge.
  EXPECT_EQ(expected.back(), render_text->selection_model());
  // Check that it is the line edge.
  render_text->MoveCursor(LINE_BREAK, direction, SELECTION_NONE);
  EXPECT_EQ(expected.back(), render_text->selection_model());
}

// Creates a RangedAttribute instance for a single character range at the
// given |index| with the given |weight| and |style_mask|. |index| is the
// index of the character in the DecoratedText instance and |font_index| is
// used to retrieve the font used from |font_spans|.
DecoratedText::RangedAttribute CreateRangedAttribute(
    const std::vector<FontSpan>& font_spans,
    int index,
    int font_index,
    Font::Weight weight,
    int style_mask) {
  const auto iter = std::find_if(font_spans.cbegin(), font_spans.cend(),
                                 [font_index](const FontSpan& span) {
                                   return IndexInRange(span.second, font_index);
                                 });
  DCHECK(font_spans.end() != iter);
  const Font& font = iter->first;

  int font_style = Font::NORMAL;
  if (style_mask & ITALIC_MASK)
    font_style |= Font::ITALIC;
  if (style_mask & UNDERLINE_MASK)
    font_style |= Font::UNDERLINE;

  const Font font_with_style = font.Derive(0, font_style, weight);
  DecoratedText::RangedAttribute attributes(Range(index, index + 1),
                                            font_with_style);
  attributes.strike = style_mask & STRIKE_MASK;
  return attributes;
}

// Verifies the given DecoratedText instances are equal by comparing the
// respective strings and attributes for each index. Note, corresponding
// ranged attributes from |expected| and |actual| can't be compared since the
// partition of |actual| into RangedAttributes will depend on the text runs
// generated.
void VerifyDecoratedWordsAreEqual(const DecoratedText& expected,
                                  const DecoratedText& actual) {
  ASSERT_EQ(expected.text, actual.text);

  // Compare attributes for each index.
  for (size_t i = 0; i < expected.text.length(); i++) {
    SCOPED_TRACE(base::StringPrintf("Comparing index[%" PRIuS "]", i));
    auto find_attribute_func = [i](const DecoratedText::RangedAttribute& attr) {
      return IndexInRange(attr.range, i);
    };
    const auto expected_attr =
        std::find_if(expected.attributes.begin(), expected.attributes.end(),
                     find_attribute_func);
    const auto actual_attr =
        std::find_if(actual.attributes.begin(), actual.attributes.end(),
                     find_attribute_func);
    ASSERT_NE(expected.attributes.end(), expected_attr);
    ASSERT_NE(actual.attributes.end(), actual_attr);

    EXPECT_EQ(expected_attr->strike, actual_attr->strike);
    EXPECT_EQ(expected_attr->font.GetFontName(),
              actual_attr->font.GetFontName());
    EXPECT_EQ(expected_attr->font.GetFontSize(),
              actual_attr->font.GetFontSize());
    EXPECT_EQ(expected_attr->font.GetWeight(), actual_attr->font.GetWeight());
    EXPECT_EQ(expected_attr->font.GetStyle(), actual_attr->font.GetStyle());
  }
}

// Helper method to return an obscured string of the given |length|, with the
// |reveal_index| filled with |reveal_char|.
base::string16 GetObscuredString(size_t length,
                                 size_t reveal_index,
                                 base::char16 reveal_char) {
  std::vector<base::char16> arr(length, RenderText::kPasswordReplacementChar);
  arr[reveal_index] = reveal_char;
  return base::string16(arr.begin(), arr.end());
}

// Helper method to return an obscured string of the given |length|.
base::string16 GetObscuredString(size_t length) {
  return base::string16(length, RenderText::kPasswordReplacementChar);
}

// Converts a vector of UTF8 literals into a vector of (UTF16) string16.
std::vector<base::string16> ToString16Vec(
    const std::vector<const char*>& utf8_literals) {
  std::vector<base::string16> vec;
  for (auto* const literal : utf8_literals)
    vec.push_back(UTF8ToUTF16(literal));
  return vec;
}

// Returns the combined character range from all text runs on |line|.
Range LineCharRange(const internal::Line& line) {
  if (line.segments.empty())
    return Range();
  Range ltr(line.segments.front().char_range.start(),
            line.segments.back().char_range.end());
  if (ltr.end() > ltr.start())
    return ltr;

  // For RTL, the order of segments is reversed, but the ranges are not.
  return Range(line.segments.back().char_range.start(),
               line.segments.front().char_range.end());
}

struct GlyphCountAndColor {
  size_t glyph_count = 0;
  SkColor color = SK_ColorBLACK;
};

class TextLog {
 public:
  TextLog(PointF origin, std::vector<uint16_t> glyphs, SkColor color)
      : origin_(origin), glyphs_(glyphs), color_(color) {}
  TextLog(const TextLog&) = default;

  PointF origin() const { return origin_; }
  SkColor color() const { return color_; }
  const std::vector<uint16_t>& glyphs() const { return glyphs_; }

 private:
  const PointF origin_;
  const std::vector<uint16_t> glyphs_;
  const SkColor color_ = SK_ColorTRANSPARENT;
};

// The class which records the drawing operations so that the test case can
// verify where exactly the glyphs are drawn.
class TestSkiaTextRenderer : public internal::SkiaTextRenderer {
 public:
  explicit TestSkiaTextRenderer(Canvas* canvas)
      : internal::SkiaTextRenderer(canvas) {}
  ~TestSkiaTextRenderer() override {}

  void GetTextLogAndReset(std::vector<TextLog>* text_log) {
    text_log_.swap(*text_log);
    text_log_.clear();
  }

 private:
  // internal::SkiaTextRenderer overrides:
  void DrawPosText(const SkPoint* pos,
                   const uint16_t* glyphs,
                   size_t glyph_count) override {
    if (glyph_count) {
      PointF origin =
          PointF(SkScalarToFloat(pos[0].x()), SkScalarToFloat(pos[0].y()));
      for (size_t i = 1U; i < glyph_count; ++i) {
        origin.SetToMin(
            PointF(SkScalarToFloat(pos[i].x()), SkScalarToFloat(pos[i].y())));
      }
      std::vector<uint16_t> run_glyphs(glyphs, glyphs + glyph_count);
      SkColor color =
          test::RenderTextTestApi::GetRendererPaint(this).getColor();
      text_log_.push_back(TextLog(origin, std::move(run_glyphs), color));
    }

    internal::SkiaTextRenderer::DrawPosText(pos, glyphs, glyph_count);
  }

  std::vector<TextLog> text_log_;

  DISALLOW_COPY_AND_ASSIGN(TestSkiaTextRenderer);
};

class TestRenderTextCanvas : public SkCanvas {
 public:
  TestRenderTextCanvas(int width, int height) : SkCanvas(width, height) {}

  // SkCanvas overrides:
  void onDrawTextBlob(const SkTextBlob* blob,
                      SkScalar x,
                      SkScalar y,
                      const SkPaint& paint) override {
    PointF origin = PointF(SkScalarToFloat(x), SkScalarToFloat(y));
    std::vector<uint16_t> glyphs;
    if (blob) {
      SkTextBlob::Iter::Run run;
      for (SkTextBlob::Iter it(*blob); it.next(&run);) {
        auto run_glyphs =
            base::span<const uint16_t>(run.fGlyphIndices, run.fGlyphCount);
        glyphs.insert(glyphs.end(), run_glyphs.begin(), run_glyphs.end());
      }
    }
    text_log_.push_back(TextLog(origin, std::move(glyphs), paint.getColor()));

    SkCanvas::onDrawTextBlob(blob, x, y, paint);
  }

  void GetTextLogAndReset(std::vector<TextLog>* text_log) {
    text_log_.swap(*text_log);
    text_log_.clear();
  }

  const std::vector<TextLog>& text_log() const { return text_log_; }

 private:
  std::vector<TextLog> text_log_;
};

// Given a buffer to test against, this can be used to test various areas of the
// rectangular buffer against a specific color value.
class TestRectangleBuffer {
 public:
  TestRectangleBuffer(const char* string,
                      const SkColor* buffer,
                      uint32_t stride,
                      uint32_t row_count)
      : string_(string),
        buffer_(buffer),
        stride_(stride),
        row_count_(row_count) {}

  // Test if any values in the rectangular area are anything other than |color|.
  void EnsureSolidRect(SkColor color,
                       int left,
                       int top,
                       int width,
                       int height) const {
    ASSERT_LT(top, row_count_) << string_;
    ASSERT_LE(top + height, row_count_) << string_;
    ASSERT_LT(left, stride_) << string_;
    ASSERT_LE(left + width, stride_) << string_ << ", left " << left
                                     << ", width " << width << ", stride_ "
                                     << stride_;
    for (int y = top; y < top + height; ++y) {
      for (int x = left; x < left + width; ++x) {
        SkColor buffer_color = buffer_[x + y * stride_];
        EXPECT_EQ(color, buffer_color) << string_ << " at " << x << ", " << y;
      }
    }
  }

  // Test that the rect defined by |left|, |top|, |width| and |height| is filled
  // with the same color.
  void EnsureRectIsAllSameColor(int left,
                                int top,
                                int width,
                                int height) const {
    SkColor buffer_color = buffer_[left + top * stride_];
    EnsureSolidRect(buffer_color, left, top, width, height);
  }

 private:
  const char* string_;
  const SkColor* buffer_;
  int stride_;
  int row_count_;

  DISALLOW_COPY_AND_ASSIGN(TestRectangleBuffer);
};

}  // namespace

// Test fixture class used to run parameterized tests for all RenderText
// implementations.
class RenderTextTest : public testing::Test {
 public:
  RenderTextTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        render_text_(std::make_unique<RenderTextHarfBuzz>()),
        test_api_(new test::RenderTextTestApi(render_text_.get())),
        renderer_(canvas()) {}

 protected:
  const cc::PaintFlags& GetRendererPaint() {
    return test::RenderTextTestApi::GetRendererPaint(renderer());
  }

  const SkFont& GetRendererFont() {
    return test::RenderTextTestApi::GetRendererFont(renderer());
  }

  void DrawVisualText(const std::vector<Range> selections = {}) {
    test_api()->EnsureLayout();
    test_api()->DrawVisualText(renderer(), selections);
    renderer()->GetTextLogAndReset(&text_log_);
  }

  void Draw(bool select_all = false) {
    constexpr int kCanvasWidth = 1200;
    constexpr int kCanvasHeight = 400;

    cc::PaintRecorder recorder;
    Canvas canvas(recorder.beginRecording(kCanvasWidth, kCanvasHeight), 1.0f);
    test_api_->Draw(&canvas, select_all);
    sk_sp<cc::PaintRecord> record = recorder.finishRecordingAsPicture();

    TestRenderTextCanvas test_canvas(kCanvasWidth, kCanvasHeight);
    record->Playback(&test_canvas);

    test_canvas.GetTextLogAndReset(&text_log_);
  }

  internal::TextRunList* GetHarfBuzzRunList() {
    test_api()->EnsureLayout();
    return test_api()->GetHarfBuzzRunList();
  }

  // For testing purposes, returns which fonts were chosen for which parts of
  // the text by returning a vector of Font and Range pairs, where each range
  // specifies the character range for which the corresponding font has been
  // chosen.
  std::vector<FontSpan> GetFontSpans() {
    test_api()->EnsureLayout();

    const internal::TextRunList* run_list = GetHarfBuzzRunList();
    std::vector<FontSpan> spans;
    std::transform(
        run_list->runs().begin(), run_list->runs().end(),
        std::back_inserter(spans), [this](const auto& run) {
          return FontSpan(
              run->font_params.font,
              Range(test_api()->DisplayIndexToTextIndex(run->range.start()),
                    test_api()->DisplayIndexToTextIndex(run->range.end())));
        });

    return spans;
  }

  // Converts the current run list into a human-readable string. Can be used in
  // test assertions for a readable expectation and failure message.
  //
  // The string shows the runs in visual order. Each run is enclosed in square
  // brackets, and shows the begin and end inclusive logical character position,
  // with an arrow indicating the direction of the run. Single-character runs
  // just show the character position.
  //
  // For example, the the logical bidirectional string "abc+\u05d0\u05d1\u05d2"
  // (visual string: "abc+אבג") yields "[0->2][3][6<-4]".
  std::string GetRunListStructureString() {
    test_api()->EnsureLayout();

    const internal::TextRunList* run_list = GetHarfBuzzRunList();
    std::string result;
    for (size_t i = 0; i < run_list->size(); ++i) {
      size_t logical_index = run_list->visual_to_logical(i);
      const internal::TextRunHarfBuzz& run = *run_list->runs()[logical_index];
      if (run.range.length() == 1) {
        result.append(base::StringPrintf("[%d]", run.range.start()));
      } else if (run.font_params.is_rtl) {
        result.append(base::StringPrintf("[%d<-%d]", run.range.end() - 1,
                                         run.range.start()));
      } else {
        result.append(base::StringPrintf("[%d->%d]", run.range.start(),
                                         run.range.end() - 1));
      }
    }
    return result;
  }

  // Returns a vector of text fragments corresponding to the current list of
  // text runs.
  std::vector<base::string16> GetRunListStrings() {
    std::vector<base::string16> runs_as_text;
    for (const auto& span : GetFontSpans()) {
      runs_as_text.push_back(render_text_->text().substr(span.second.GetMin(),
                                                         span.second.length()));
    }
    return runs_as_text;
  }

  // Sets the text to |text|, then returns GetRunListStrings().
  std::vector<base::string16> RunsFor(const base::string16& text) {
    render_text_->SetText(text);
    test_api()->EnsureLayout();
    return GetRunListStrings();
  }

  void ResetRenderTextInstance() {
    render_text_ = std::make_unique<RenderTextHarfBuzz>();
    test_api_ = std::make_unique<test::RenderTextTestApi>(GetRenderText());
  }

  void ResetCursorX() { test_api()->reset_cached_cursor_x(); }

  int GetLineContainingYCoord(float text_y) {
    return test_api()->GetLineContainingYCoord(text_y);
  }

  RenderTextHarfBuzz* GetRenderText() { return render_text_.get(); }

  Rect GetSubstringBoundsUnion(const Range& range) {
    const std::vector<Rect> bounds = render_text_->GetSubstringBounds(range);
    return std::accumulate(
        bounds.begin(), bounds.end(), Rect(),
        [](const Rect& a, const Rect& b) { return UnionRects(a, b); });
  }

  Rect GetSelectionBoundsUnion() {
    return GetSubstringBoundsUnion(render_text_->selection());
  }

  // Checks left-to-right text, ensuring that the caret moves to the right as
  // the cursor position increments through the logical text. Also ensures that
  // each glyph is to the right of the prior glyph. RenderText automatically
  // updates invalid cursor positions (eg. between a surrogate pair) to a valid
  // neighbor, so the positions may be unchanged for some iterations. Invoking
  // this in a test gives coverage of the sanity checks in functions such as
  // TextRunHarfBuzz::GetGraphemeBounds() which rely on sensible glyph positions
  // being provided by installed typefaces.
  void CheckBoundsForCursorPositions() {
    ASSERT_FALSE(render_text_->text().empty());

    // Use a wide display rect to avoid scrolling.
    render_text_->SetDisplayRect(gfx::Rect(0, 0, 1000, 50));
    EXPECT_LT(render_text_->GetContentWidthF(),
              render_text_->display_rect().width());

    // Assume LTR for now.
    int max_cursor_x = 0;
    int max_glyph_x = 0, max_glyph_right = 0;

    for (size_t i = 0; i <= render_text_->text().size(); ++i) {
      render_text_->SetCursorPosition(i);

      SCOPED_TRACE(testing::Message()
                   << "Cursor position: " << i
                   << " selection: " << render_text_->selection().ToString());

      const gfx::Rect cursor_bounds = render_text_->GetUpdatedCursorBounds();

      // The cursor should always be one pixel wide.
      EXPECT_EQ(1, cursor_bounds.width());
      EXPECT_LE(max_cursor_x, cursor_bounds.x());
      max_cursor_x = cursor_bounds.x();

      const gfx::Rect glyph_bounds =
          render_text_->GetCursorBounds(render_text_->selection_model(), false);
      EXPECT_LE(max_glyph_x, glyph_bounds.x());
      EXPECT_LE(max_glyph_right, glyph_bounds.right());
      max_glyph_x = glyph_bounds.x();
      max_glyph_right = glyph_bounds.right();
    }
  }

  void SetGlyphWidth(float test_width) {
    test_api()->SetGlyphWidth(test_width);
  }

  void SetGlyphHeight(float test_height) {
    test_api()->SetGlyphHeight(test_height);
  }

  bool ShapeRunWithFont(const base::string16& text,
                        const Font& font,
                        const FontRenderParams& render_params,
                        internal::TextRunHarfBuzz* run) {
    internal::TextRunHarfBuzz::FontParams font_params = run->font_params;
    font_params.ComputeRenderParamsFontSizeAndBaselineOffset();
    font_params.SetRenderParamsRematchFont(font, render_params);
    run->shape.missing_glyph_count = static_cast<size_t>(-1);
    std::vector<internal::TextRunHarfBuzz*> runs = {run};
    GetRenderText()->ShapeRunsWithFont(text, font_params, &runs);
    return runs.empty();
  }

  int GetCursorYForTesting(int line_num = 0) {
    return GetRenderText()->GetLineOffset(line_num).y() + 1;
  }

  size_t GetLineContainingCaret() {
    return GetRenderText()->GetLineContainingCaret(
        GetRenderText()->selection_model());
  }

  Canvas* canvas() { return &canvas_; }
  TestSkiaTextRenderer* renderer() { return &renderer_; }
  test::RenderTextTestApi* test_api() { return test_api_.get(); }
  const test::RenderTextTestApi* test_api() const { return test_api_.get(); }
  const std::vector<TextLog>& text_log() const { return text_log_; }

  void ExpectTextLog(std::vector<GlyphCountAndColor> runs) {
    EXPECT_EQ(runs.size(), text_log_.size());
    const size_t min_size = std::min(runs.size(), text_log_.size());
    for (size_t i = 0; i < min_size; ++i) {
      SCOPED_TRACE(testing::Message()
                   << "ExpectTextLog, run " << i << " of " << runs.size());
      EXPECT_EQ(runs[i].color, text_log_[i].color());
      EXPECT_EQ(runs[i].glyph_count, text_log_[i].glyphs().size());
    }
  }

 private:
  // Needed to bypass DCHECK in GetFallbackFont.
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<RenderTextHarfBuzz> render_text_;
  std::unique_ptr<test::RenderTextTestApi> test_api_;
  std::vector<TextLog> text_log_;
  Canvas canvas_;
  TestSkiaTextRenderer renderer_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextTest);
};

TEST_F(RenderTextTest, DefaultStyles) {
  // Check the default styles applied to new instances and adjusted text.
  RenderText* render_text = GetRenderText();
  EXPECT_TRUE(render_text->text().empty());
  const char* const cases[] = {kWeak, kLtr, "Hello", kRtl, "", ""};
  for (size_t i = 0; i < base::size(cases); ++i) {
    EXPECT_TRUE(test_api()->colors().EqualsValueForTesting(SK_ColorBLACK));
    EXPECT_TRUE(test_api()->baselines().EqualsValueForTesting(NORMAL_BASELINE));
    EXPECT_TRUE(test_api()->font_size_overrides().EqualsValueForTesting(0));
    for (size_t style = 0; style < static_cast<int>(TEXT_STYLE_COUNT); ++style)
      EXPECT_TRUE(test_api()->styles()[style].EqualsValueForTesting(false));
    render_text->SetText(UTF8ToUTF16(cases[i]));
  }
}

TEST_F(RenderTextTest, SetStyles) {
  // Ensure custom default styles persist across setting and clearing text.
  RenderText* render_text = GetRenderText();
  const SkColor color = SK_ColorRED;
  render_text->SetColor(color);
  render_text->SetBaselineStyle(SUPERSCRIPT);
  render_text->SetWeight(Font::Weight::BOLD);
  render_text->SetStyle(TEXT_STYLE_UNDERLINE, false);
  const char* const cases[] = {kWeak, kLtr, "Hello", kRtl, "", ""};
  for (size_t i = 0; i < base::size(cases); ++i) {
    EXPECT_TRUE(test_api()->colors().EqualsValueForTesting(color));
    EXPECT_TRUE(test_api()->baselines().EqualsValueForTesting(SUPERSCRIPT));
    EXPECT_TRUE(
        test_api()->weights().EqualsValueForTesting(Font::Weight::BOLD));
    EXPECT_TRUE(
        test_api()->styles()[TEXT_STYLE_UNDERLINE].EqualsValueForTesting(
            false));
    render_text->SetText(UTF8ToUTF16(cases[i]));

    // Ensure custom default styles can be applied after text has been set.
    if (i == 1)
      render_text->SetStyle(TEXT_STYLE_STRIKE, true);
    if (i >= 1)
      EXPECT_TRUE(
          test_api()->styles()[TEXT_STYLE_STRIKE].EqualsValueForTesting(true));
  }
}

TEST_F(RenderTextTest, ApplyStyles) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("012345678"));

  constexpr int kTestFontSizeOverride = 20;

  // Apply a ranged color and style and check the resulting breaks.
  render_text->ApplyColor(SK_ColorRED, Range(1, 4));
  render_text->ApplyBaselineStyle(SUPERIOR, Range(2, 4));
  render_text->ApplyWeight(Font::Weight::BOLD, Range(2, 5));
  render_text->ApplyFontSizeOverride(kTestFontSizeOverride, Range(5, 7));

  EXPECT_TRUE(test_api()->colors().EqualsForTesting(
      {{0, SK_ColorBLACK}, {1, SK_ColorRED}, {4, SK_ColorBLACK}}));

  EXPECT_TRUE(test_api()->baselines().EqualsForTesting(
      {{0, NORMAL_BASELINE}, {2, SUPERIOR}, {4, NORMAL_BASELINE}}));

  EXPECT_TRUE(test_api()->font_size_overrides().EqualsForTesting(
      {{0, 0}, {5, kTestFontSizeOverride}, {7, 0}}));

  EXPECT_TRUE(
      test_api()->weights().EqualsForTesting({{0, Font::Weight::NORMAL},
                                              {2, Font::Weight::BOLD},
                                              {5, Font::Weight::NORMAL}}));

  // Ensure that setting a value overrides the ranged values.
  render_text->SetColor(SK_ColorBLUE);
  EXPECT_TRUE(test_api()->colors().EqualsValueForTesting(SK_ColorBLUE));
  render_text->SetBaselineStyle(SUBSCRIPT);
  EXPECT_TRUE(test_api()->baselines().EqualsValueForTesting(SUBSCRIPT));
  render_text->SetWeight(Font::Weight::NORMAL);
  EXPECT_TRUE(
      test_api()->weights().EqualsValueForTesting(Font::Weight::NORMAL));

  // Apply a value over the text end and check the resulting breaks (INT_MAX
  // should be used instead of the text length for the range end)
  const size_t text_length = render_text->text().length();
  render_text->ApplyColor(SK_ColorRED, Range(0, text_length));
  render_text->ApplyBaselineStyle(SUPERIOR, Range(0, text_length));
  render_text->ApplyWeight(Font::Weight::BOLD, Range(2, text_length));

  EXPECT_TRUE(test_api()->colors().EqualsForTesting({{0, SK_ColorRED}}));
  EXPECT_TRUE(test_api()->baselines().EqualsForTesting({{0, SUPERIOR}}));
  EXPECT_TRUE(test_api()->weights().EqualsForTesting(
      {{0, Font::Weight::NORMAL}, {2, Font::Weight::BOLD}}));

  // Ensure ranged values adjust to accommodate text length changes.
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, Range(0, 2));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, Range(3, 6));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, Range(7, text_length));
  std::vector<std::pair<size_t, bool>> expected_italic = {
      {0, true}, {2, false}, {3, true}, {6, false}, {7, true}};
  EXPECT_TRUE(test_api()->styles()[TEXT_STYLE_ITALIC].EqualsForTesting(
      expected_italic));

  // Changing the text should clear any breaks except for the first one.
  render_text->SetText(ASCIIToUTF16("0123456"));
  expected_italic.resize(1);
  EXPECT_TRUE(test_api()->styles()[TEXT_STYLE_ITALIC].EqualsForTesting(
      expected_italic));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, false, Range(2, 4));
  render_text->SetText(ASCIIToUTF16("012345678"));
  EXPECT_TRUE(test_api()->styles()[TEXT_STYLE_ITALIC].EqualsForTesting(
      expected_italic));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, false, Range(0, 1));
  render_text->SetText(ASCIIToUTF16("0123456"));
  expected_italic.begin()->second = false;
  EXPECT_TRUE(test_api()->styles()[TEXT_STYLE_ITALIC].EqualsForTesting(
      expected_italic));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, Range(2, 4));
  render_text->SetText(ASCIIToUTF16("012345678"));
  EXPECT_TRUE(test_api()->styles()[TEXT_STYLE_ITALIC].EqualsForTesting(
      expected_italic));

  // Styles mid-grapheme should work. Style of first character of the grapheme
  // is used.
  render_text->SetText(UTF8ToUTF16("0\u0915\u093f1\u0915\u093f2"));
  render_text->ApplyStyle(TEXT_STYLE_UNDERLINE, true, Range(2, 5));
  EXPECT_TRUE(test_api()->styles()[TEXT_STYLE_UNDERLINE].EqualsForTesting(
      {{0, false}, {2, true}, {5, false}}));
}

TEST_F(RenderTextTest, ApplyStyleSurrogatePair) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(WideToUTF16(L"x\U0001F601x"));
  // Apply the style in the middle of a surrogate pair. The style should be
  // applied to the whole range of the codepoint.
  gfx::Range range(2, 3);
  render_text->ApplyWeight(gfx::Font::Weight::BOLD, range);
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, range);
  render_text->ApplyColor(SK_ColorRED, range);
  render_text->Draw(canvas());

  EXPECT_TRUE(test_api()->styles()[TEXT_STYLE_ITALIC].EqualsForTesting(
      {{0, false}, {2, true}, {3, false}}));
  EXPECT_TRUE(test_api()->colors().EqualsForTesting(
      {{0, SK_ColorBLACK}, {2, SK_ColorRED}, {3, SK_ColorBLACK}}));
  EXPECT_TRUE(
      test_api()->weights().EqualsForTesting({{0, Font::Weight::NORMAL},
                                              {2, Font::Weight::BOLD},
                                              {3, Font::Weight::NORMAL}}));
}

TEST_F(RenderTextTest, ApplyStyleGrapheme) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("\u0065\u0301"));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, gfx::Range(1, 2));
  render_text->ApplyStyle(TEXT_STYLE_UNDERLINE, true, gfx::Range(0, 1));
  Draw();

  // Ensures that the whole grapheme is drawn with the same style.
  ExpectTextLog({{1}});
}

TEST_F(RenderTextTest, ApplyStyleMultipleGraphemes) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("xx\u0065\u0301x"));
  // Apply the style in the middle of a grapheme.
  gfx::Range range(1, 3);
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, range);
  Draw();

  EXPECT_TRUE(test_api()->styles()[TEXT_STYLE_ITALIC].EqualsForTesting(
      {{0, false}, {1, true}, {3, false}}));

  // Ensures that the style of the grapheme is the style at its first character.
  ExpectTextLog({{1}, {2}, {1}});
}

TEST_F(RenderTextTest, ApplyColorSurrogatePair) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(WideToUTF16(L"x\U0001F601x"));
  render_text->ApplyColor(SK_ColorRED, Range(2, 3));
  Draw();

  // Ensures that the color is not applied since it is in the middle of a
  // surrogate pair.
  // There is three runs since the codepoints are not in the same script.
  ExpectTextLog({{1}, {1}, {1}});

  // Obscure the text should renders characters with the same colors.
  render_text->SetObscured(true);
  Draw();
  ExpectTextLog({{3}});
}

TEST_F(RenderTextTest, ApplyColorLongEmoji) {
  // A long emoji sequence.
  static const wchar_t kLongEmoji[] = L"\U0001F468\u200D\u2708\uFE0F";

  RenderText* render_text = GetRenderText();
  render_text->SetText(WideToUTF16(kLongEmoji));
  render_text->AppendText(WideToUTF16(kLongEmoji));
  render_text->AppendText(WideToUTF16(kLongEmoji));

  render_text->ApplyColor(SK_ColorRED, Range(0, 2));
  render_text->ApplyColor(SK_ColorBLUE, Range(8, 13));
  Draw();

  // Ensures that the color of the emoji is the color at its first character.
  ASSERT_EQ(3u, text_log().size());
  EXPECT_EQ(SK_ColorRED, text_log()[0].color());
  EXPECT_EQ(SK_ColorBLACK, text_log()[1].color());
  EXPECT_EQ(SK_ColorBLUE, text_log()[2].color());

  // Reset the color.
  render_text->SetColor(SK_ColorBLACK);
  Draw();

  // The amount of glyphs depend on the font. If the font supports the emoji,
  // the amount of glyph is 1, otherwise it vary.
  ASSERT_EQ(1u, text_log().size());
  EXPECT_EQ(SK_ColorBLACK, text_log()[0].color());
}

TEST_F(RenderTextTest, ApplyColorObscuredEmoji) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(WideToUTF16(L"\U0001F628\U0001F628\U0001F628"));
  render_text->ApplyColor(SK_ColorRED, Range(0, 2));
  render_text->ApplyColor(SK_ColorBLUE, Range(4, 5));

  const std::vector<GlyphCountAndColor> kExpectedTextLog =
      {{1, SK_ColorRED}, {1, SK_ColorBLACK}, {1, SK_ColorBLUE}};

  // Ensures that colors are applied.
  Draw();
  ExpectTextLog(kExpectedTextLog);

  // Obscure the text.
  render_text->SetObscured(true);

  // The obscured text (layout text) no longer contains surrogate pairs.
  EXPECT_EQ(render_text->text().size(), 2 * test_api()->GetLayoutText().size());

  // Obscured text should give the same colors.
  Draw();
  ExpectTextLog(kExpectedTextLog);

  for (size_t i = 0; i < render_text->text().size(); ++i) {
    render_text->RenderText::SetObscuredRevealIndex(i);

    // Revealed codepoints should give the same colors.
    Draw();
    ExpectTextLog(kExpectedTextLog);
  }
}

TEST_F(RenderTextTest, ApplyColorArabicDiacritics) {
  // Render an Arabic character with two diacritics. The color should be taken
  // from the base character.
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("\u0628\u0651\u0650"));
  render_text->ApplyColor(SK_ColorRED, Range(0, 1));
  render_text->ApplyColor(SK_ColorBLACK, Range(1, 2));
  render_text->ApplyColor(SK_ColorBLUE, Range(2, 3));
  Draw();
  ASSERT_EQ(1u, text_log().size());
  EXPECT_EQ(SK_ColorRED, text_log()[0].color());
}

TEST_F(RenderTextTest, ApplyColorArabicLigature) {
  // In Arabic, letters of each word join together whenever possible. During
  // the shaping pass of the font, characters will take their joining form:
  // Isolated, Initial, Medial or Final.

  // Render the isolated form of the first glyph.
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("\u0628"));
  Draw();
  ASSERT_EQ(1u, text_log().size());
  ASSERT_EQ(1u, text_log()[0].glyphs().size());
  uint16_t isolated_first_glyph = text_log()[0].glyphs()[0];

  // Render a pair of glyphs (initial form and final form).
  render_text->SetText(UTF8ToUTF16("\u0628\u0645"));
  Draw();
  ASSERT_EQ(1u, text_log().size());
  ASSERT_LE(2u, text_log()[0].glyphs().size());
  uint16_t initial_first_glyph = text_log()[0].glyphs()[0];
  uint16_t final_second_glyph = text_log()[0].glyphs()[1];

  // A ligature is applied between glyphs and the two glyphs (isolated and
  // initial form) are displayed differently.
  EXPECT_NE(isolated_first_glyph, initial_first_glyph);

  // Ensures that both characters didn't merge in a single glyph.
  EXPECT_NE(initial_first_glyph, final_second_glyph);

  // Applying color should not break the ligature.
  // see: https://w3c.github.io/alreq/#h_styling_individual_letters
  render_text->ApplyColor(SK_ColorRED, Range(0, 1));
  render_text->ApplyColor(SK_ColorBLACK, Range(1, 2));
  Draw();
  ASSERT_EQ(2u, text_log().size());
  ASSERT_LE(1u, text_log()[0].glyphs().size());
  ASSERT_EQ(1u, text_log()[1].glyphs().size());
  uint16_t colored_first_glyph = text_log()[1].glyphs()[0];
  uint16_t colored_second_glyph = text_log()[0].glyphs()[0];

  // Glyphs should be the same with and without color.
  EXPECT_EQ(initial_first_glyph, colored_first_glyph);
  EXPECT_EQ(final_second_glyph, colored_second_glyph);

  // Colors should be applied.
  EXPECT_EQ(SK_ColorRED, text_log()[0].color());
  EXPECT_EQ(SK_ColorBLACK, text_log()[1].color());
}

TEST_F(RenderTextTest, AppendTextKeepsStyles) {
  RenderText* render_text = GetRenderText();
  // Setup basic functionality.
  render_text->SetText(ASCIIToUTF16("abcd"));
  render_text->ApplyColor(SK_ColorRED, Range(0, 1));
  render_text->ApplyBaselineStyle(SUPERSCRIPT, Range(1, 2));
  render_text->ApplyStyle(TEXT_STYLE_UNDERLINE, true, Range(2, 3));
  render_text->ApplyFontSizeOverride(20, Range(3, 4));
  // Verify basic functionality.
  const std::vector<std::pair<size_t, SkColor>> expected_color = {
      {0, SK_ColorRED}, {1, SK_ColorBLACK}};
  EXPECT_TRUE(test_api()->colors().EqualsForTesting(expected_color));
  const std::vector<std::pair<size_t, BaselineStyle>> expected_baseline = {
      {0, NORMAL_BASELINE}, {1, SUPERSCRIPT}, {2, NORMAL_BASELINE}};
  EXPECT_TRUE(test_api()->baselines().EqualsForTesting(expected_baseline));
  const std::vector<std::pair<size_t, bool>> expected_style = {
      {0, false}, {2, true}, {3, false}};
  EXPECT_TRUE(test_api()->styles()[TEXT_STYLE_UNDERLINE].EqualsForTesting(
      expected_style));
  const std::vector<std::pair<size_t, int>> expected_font_size = {{0, 0},
                                                                  {3, 20}};
  EXPECT_TRUE(
      test_api()->font_size_overrides().EqualsForTesting(expected_font_size));

  // Ensure AppendText maintains current text styles.
  render_text->AppendText(ASCIIToUTF16("efg"));
  EXPECT_EQ(render_text->GetDisplayText(), ASCIIToUTF16("abcdefg"));
  EXPECT_TRUE(test_api()->colors().EqualsForTesting(expected_color));
  EXPECT_TRUE(test_api()->baselines().EqualsForTesting(expected_baseline));
  EXPECT_TRUE(test_api()->styles()[TEXT_STYLE_UNDERLINE].EqualsForTesting(
      expected_style));
  EXPECT_TRUE(
      test_api()->font_size_overrides().EqualsForTesting(expected_font_size));
}

TEST_F(RenderTextTest, SetSelection) {
  RenderText* render_text = GetRenderText();
  render_text->set_selection_color(SK_ColorRED);
  render_text->SetText(ASCIIToUTF16("abcdef"));
  render_text->set_focused(true);

  // Single selection
  render_text->SetSelection(
      {{{4, 100}}, LogicalCursorDirection::CURSOR_FORWARD});
  Draw();
  ExpectTextLog({{4}, {2, SK_ColorRED}});

  // Multiple selections
  render_text->SetSelection(
      {{{0, 1}, {4, 100}}, LogicalCursorDirection::CURSOR_FORWARD});
  Draw();
  ExpectTextLog({{1, SK_ColorRED}, {3}, {2, SK_ColorRED}});

  render_text->ClearSelection();
  Draw();
  ExpectTextLog({{6}});
}

TEST_F(RenderTextTest, SelectRangeColored) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdef"));
  render_text->SetColor(SK_ColorBLACK);
  render_text->set_selection_color(SK_ColorRED);
  render_text->set_focused(true);

  render_text->SelectRange(Range(0, 1));
  Draw();
  ExpectTextLog({{1, SK_ColorRED}, {5, SK_ColorBLACK}});

  render_text->SelectRange(Range(1, 3));
  Draw();
  ExpectTextLog({{1, SK_ColorBLACK}, {2, SK_ColorRED}, {3, SK_ColorBLACK}});

  render_text->ClearSelection();
  Draw();
  ExpectTextLog({{6}});
}

// Tests that when a selection is made and the selection background is
// translucent, the selection renders properly. See crbug.com/1134440.
TEST_F(RenderTextTest, SelectWithTranslucentBackground) {
  constexpr float kGlyphWidth = 5.5f;
  constexpr Size kCanvasSize(300, 50);
  constexpr SkColor kTranslucentBlue = SkColorSetARGB(0x7F, 0x00, 0x00, 0xFF);
  const char* kTestString{"A B C D"};

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kCanvasSize.width(), kCanvasSize.height()));
  cc::SkiaPaintCanvas paint_canvas(bitmap);
  Canvas canvas(&paint_canvas, 1.0f);
  paint_canvas.clear(SK_ColorWHITE);

  SetGlyphWidth(kGlyphWidth);
  RenderText* render_text = GetRenderText();
  render_text->set_selection_background_focused_color(kTranslucentBlue);
  render_text->set_focused(true);

  render_text->SetText(UTF8ToUTF16(kTestString));
  render_text->SelectRange(Range(0, 7));
  const Rect text_rect = Rect(render_text->GetStringSize());
  render_text->SetDisplayRect(text_rect);
  render_text->Draw(&canvas);
  const uint32_t* buffer = static_cast<const uint32_t*>(bitmap.getPixels());
  ASSERT_NE(nullptr, buffer);
  TestRectangleBuffer rect_buffer(kTestString, buffer, kCanvasSize.width(),
                                  kCanvasSize.height());

  // This whole slice should be the same color and opacity.
  rect_buffer.EnsureRectIsAllSameColor(0, 0, text_rect.width() - 1, 1);
}

TEST_F(RenderTextTest, SelectRangeColoredGrapheme) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("x\u0065\u0301y"));
  render_text->SetColor(SK_ColorBLACK);
  render_text->set_selection_color(SK_ColorRED);
  render_text->set_focused(true);

  render_text->SelectRange(Range(0, 1));
  Draw();
  ExpectTextLog({{1, SK_ColorRED}, {2, SK_ColorBLACK}});

  render_text->SelectRange(Range(1, 2));
  Draw();
  ExpectTextLog({{1, SK_ColorBLACK}, {1, SK_ColorRED}, {1, SK_ColorBLACK}});

  render_text->SelectRange(Range(2, 3));
  Draw();
  ExpectTextLog({{1, SK_ColorBLACK}, {1, SK_ColorRED}, {1, SK_ColorBLACK}});

  render_text->SelectRange(Range(2, 4));
  Draw();
  ExpectTextLog({{1, SK_ColorBLACK}, {2, SK_ColorRED}});
}

TEST_F(RenderTextTest, SelectRangeMultiple) {
  RenderText* render_text = GetRenderText();
  render_text->set_selection_color(SK_ColorRED);
  render_text->SetText(ASCIIToUTF16("abcdef"));
  render_text->set_focused(true);

  // Multiple selections
  render_text->SelectRange(Range(0, 1));
  render_text->SelectRange(Range(4, 2), false);
  Draw();
  ExpectTextLog({{1, SK_ColorRED}, {1}, {2, SK_ColorRED}, {2}});

  // Setting a primary selection should override secondary selections
  render_text->SelectRange(Range(5, 6));
  Draw();
  ExpectTextLog({{5}, {1, SK_ColorRED}});

  render_text->ClearSelection();
  Draw();
  ExpectTextLog({{6}});
}

TEST_F(RenderTextTest, SetCompositionRangeColored) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdef"));

  render_text->SetCompositionRange(Range(0, 1));
  Draw();
  ExpectTextLog({{1}, {5}});

  render_text->SetCompositionRange(Range(1, 3));
  Draw();
  ExpectTextLog({{1}, {2}, {3}});

  render_text->SetCompositionRange(Range::InvalidRange());
  Draw();
  ExpectTextLog({{6}});
}

TEST_F(RenderTextTest, SetCompositionRangeColoredGrapheme) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("x\u0065\u0301y"));

  render_text->SetCompositionRange(Range(0, 1));
  Draw();
  ExpectTextLog({{1}, {2}});

  render_text->SetCompositionRange(Range(2, 3));
  Draw();
  ExpectTextLog({{3}});

  render_text->SetCompositionRange(Range(2, 4));
  Draw();
  ExpectTextLog({{2}, {1}});
}

void TestVisualCursorMotionInObscuredField(
    RenderText* render_text,
    const base::string16& text,
    SelectionBehavior selection_behavior) {
  const bool select = selection_behavior != SELECTION_NONE;
  ASSERT_TRUE(render_text->obscured());
  render_text->SetText(text);
  int len = text.length();
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, selection_behavior);
  EXPECT_EQ(SelectionModel(Range(select ? 0 : len, len), CURSOR_FORWARD),
            render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, selection_behavior);
  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
  for (int j = 1; j <= len; ++j) {
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, selection_behavior);
    EXPECT_EQ(SelectionModel(Range(select ? 0 : j, j), CURSOR_BACKWARD),
              render_text->selection_model());
  }
  for (int j = len - 1; j >= 0; --j) {
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, selection_behavior);
    EXPECT_EQ(SelectionModel(Range(select ? 0 : j, j), CURSOR_FORWARD),
              render_text->selection_model());
  }
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, selection_behavior);
  EXPECT_EQ(SelectionModel(Range(select ? 0 : len, len), CURSOR_FORWARD),
            render_text->selection_model());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, selection_behavior);
  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
}

TEST_F(RenderTextTest, ObscuredText) {
  const base::string16 seuss = ASCIIToUTF16("hop on pop");
  const base::string16 no_seuss = GetObscuredString(seuss.length());
  RenderText* render_text = GetRenderText();

  // GetDisplayText() returns a string filled with
  // RenderText::kPasswordReplacementChar when the obscured bit is set.
  render_text->SetText(seuss);
  render_text->SetObscured(true);
  EXPECT_EQ(seuss, render_text->text());
  EXPECT_EQ(no_seuss, render_text->GetDisplayText());
  render_text->SetObscured(false);
  EXPECT_EQ(seuss, render_text->text());
  EXPECT_EQ(seuss, render_text->GetDisplayText());

  render_text->SetObscured(true);

  // Surrogate pairs are counted as one code point.
  const base::char16 invalid_surrogates[] = {0xDC00, 0xD800, 0};
  render_text->SetText(invalid_surrogates);
  EXPECT_EQ(GetObscuredString(2), render_text->GetDisplayText());
  const base::char16 valid_surrogates[] = {0xD800, 0xDC00, 0};
  render_text->SetText(valid_surrogates);
  EXPECT_EQ(GetObscuredString(1), render_text->GetDisplayText());
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(2U, render_text->cursor_position());

  // Test index conversion and cursor validity with a valid surrogate pair.
  // Text contains "u+D800 u+DC00" and display text contains "u+2022".
  EXPECT_EQ(0U, test_api()->TextIndexToDisplayIndex(0U));
  EXPECT_EQ(0U, test_api()->TextIndexToDisplayIndex(1U));
  EXPECT_EQ(1U, test_api()->TextIndexToDisplayIndex(2U));
  EXPECT_EQ(0U, test_api()->DisplayIndexToTextIndex(0U));
  EXPECT_EQ(2U, test_api()->DisplayIndexToTextIndex(1U));
  EXPECT_TRUE(render_text->IsValidCursorIndex(0U));
  EXPECT_FALSE(render_text->IsValidCursorIndex(1U));
  EXPECT_TRUE(render_text->IsValidCursorIndex(2U));

  // FindCursorPosition() should not return positions between a surrogate pair.
  render_text->SetDisplayRect(Rect(0, 0, 20, 20));
  const int cursor_y = GetCursorYForTesting();
  EXPECT_EQ(render_text->FindCursorPosition(Point(0, cursor_y)).caret_pos(),
            0U);
  EXPECT_EQ(render_text->FindCursorPosition(Point(20, cursor_y)).caret_pos(),
            2U);
  for (int x = -1; x <= 20; ++x) {
    SelectionModel selection =
        render_text->FindCursorPosition(Point(x, cursor_y));
    EXPECT_TRUE(selection.caret_pos() == 0U || selection.caret_pos() == 2U);
  }

  // GetCursorSpan() should yield the entire string bounds for text index 0.
  EXPECT_EQ(render_text->GetStringSize().width(),
            std::ceil(render_text->GetCursorSpan({0, 2}).length()));

  // Cursoring is independent of underlying characters when text is obscured.
  const char* const texts[] = {
      kWeak, kLtr, kLtrRtl, kLtrRtlLtr, kRtl, kRtlLtr, kRtlLtrRtl,
      "hop on pop",                              // Check LTR word boundaries.
      "\u05d0\u05d1 \u05d0\u05d2 \u05d1\u05d2",  // Check RTL word boundaries.
  };
  for (size_t i = 0; i < base::size(texts); ++i) {
    base::string16 text = UTF8ToUTF16(texts[i]);
    TestVisualCursorMotionInObscuredField(render_text, text, SELECTION_NONE);
    TestVisualCursorMotionInObscuredField(render_text, text, SELECTION_RETAIN);
  }
}

TEST_F(RenderTextTest, ObscuredTextMultiline) {
  const base::string16 test = ASCIIToUTF16("a\nbc\ndef");
  RenderText* render_text = GetRenderText();
  render_text->SetText(test);
  render_text->SetObscured(true);
  render_text->SetMultiline(true);

  // Newlines should be kept in multiline mode.
  base::string16 display_text = render_text->GetDisplayText();
  EXPECT_EQ(display_text[1], '\n');
  EXPECT_EQ(display_text[4], '\n');
}

TEST_F(RenderTextTest, ObscuredTextMultilineNewline) {
  const base::string16 test = ASCIIToUTF16("\r\r\n");
  RenderText* render_text = GetRenderText();
  render_text->SetText(test);
  render_text->SetObscured(true);
  render_text->SetMultiline(true);

  // Newlines should be kept in multiline mode.
  base::string16 display_text = render_text->GetDisplayText();
  EXPECT_EQ(display_text[0], '\r');
  EXPECT_EQ(display_text[1], '\r');
  EXPECT_EQ(display_text[2], '\n');
}

TEST_F(RenderTextTest, RevealObscuredText) {
  const base::string16 seuss = ASCIIToUTF16("hop on pop");
  const base::string16 no_seuss = GetObscuredString(seuss.length());
  RenderText* render_text = GetRenderText();

  render_text->SetText(seuss);
  render_text->SetObscured(true);
  EXPECT_EQ(seuss, render_text->text());
  EXPECT_EQ(no_seuss, render_text->GetDisplayText());

  // Valid reveal index and new revealed index clears previous one.
  render_text->RenderText::SetObscuredRevealIndex(0);
  EXPECT_EQ(GetObscuredString(seuss.length(), 0, 'h'),
            render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(1);
  EXPECT_EQ(GetObscuredString(seuss.length(), 1, 'o'),
            render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(2);
  EXPECT_EQ(GetObscuredString(seuss.length(), 2, 'p'),
            render_text->GetDisplayText());

  // Invalid reveal index.
  render_text->RenderText::SetObscuredRevealIndex(-1);
  EXPECT_EQ(no_seuss, render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(seuss.length() + 1);
  EXPECT_EQ(no_seuss, render_text->GetDisplayText());

  // SetObscured clears the revealed index.
  render_text->RenderText::SetObscuredRevealIndex(0);
  EXPECT_EQ(GetObscuredString(seuss.length(), 0, 'h'),
            render_text->GetDisplayText());
  render_text->SetObscured(false);
  EXPECT_EQ(seuss, render_text->GetDisplayText());
  render_text->SetObscured(true);
  EXPECT_EQ(no_seuss, render_text->GetDisplayText());

  // SetText clears the revealed index.
  render_text->SetText(ASCIIToUTF16("new"));
  EXPECT_EQ(GetObscuredString(3), render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(2);
  EXPECT_EQ(GetObscuredString(3, 2, 'w'), render_text->GetDisplayText());
  render_text->SetText(ASCIIToUTF16("new longer"));
  EXPECT_EQ(GetObscuredString(10), render_text->GetDisplayText());

  // Text with invalid surrogates (surrogates low 0xDC00 and high 0xD800).
  // Invalid surrogates are replaced by replacement character (e.g. 0xFFFD).
  const base::char16 invalid_surrogates[] = {0xDC00, 0xD800, 'h', 'o', 'p', 0};
  render_text->SetText(invalid_surrogates);
  EXPECT_EQ(GetObscuredString(5), render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(0);
  EXPECT_EQ(GetObscuredString(5, 0, 0xFFFD), render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(1);
  EXPECT_EQ(GetObscuredString(5, 1, 0xFFFD), render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(2);
  EXPECT_EQ(GetObscuredString(5, 2, 'h'), render_text->GetDisplayText());

  // Text with valid surrogates before and after the reveal index.
  const base::char16 valid_surrogates[] =
      {0xD800, 0xDC00, 'h', 'o', 'p', 0xD800, 0xDC00, 0};
  render_text->SetText(valid_surrogates);
  EXPECT_EQ(GetObscuredString(5), render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(0);
  const base::char16 valid_expect_0_and_1[] = {
      0xD800,
      0xDC00,
      RenderText::kPasswordReplacementChar,
      RenderText::kPasswordReplacementChar,
      RenderText::kPasswordReplacementChar,
      RenderText::kPasswordReplacementChar,
      0};
  EXPECT_EQ(valid_expect_0_and_1, render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(1);
  EXPECT_EQ(valid_expect_0_and_1, render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(2);
  EXPECT_EQ(GetObscuredString(5, 1, 'h'), render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(5);
  const base::char16 valid_expect_5_and_6[] = {
      RenderText::kPasswordReplacementChar,
      RenderText::kPasswordReplacementChar,
      RenderText::kPasswordReplacementChar,
      RenderText::kPasswordReplacementChar,
      0xD800,
      0xDC00,
      0};
  EXPECT_EQ(valid_expect_5_and_6, render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(6);
  EXPECT_EQ(valid_expect_5_and_6, render_text->GetDisplayText());
}

TEST_F(RenderTextTest, ObscuredEmoji) {
  // Ensures text itemization doesn't crash on obscured multi-char glyphs.
  RenderText* render_text = GetRenderText();
  render_text->SetObscured(true);
  // Test U+1F601 😁 "Grinning face with smiling eyes", followed by 'y'.
  // Windows requires wide strings for \Unnnnnnnn universal character names.
  render_text->SetText(WideToUTF16(L"\U0001F601y"));
  render_text->Draw(canvas());

  // Emoji codepoints are replaced by bullets (e.g. "\u2022\u2022").
  EXPECT_EQ(UTF8ToUTF16("\u2022\u2022"), render_text->GetDisplayText());
  EXPECT_EQ(0U, test_api()->TextIndexToDisplayIndex(0U));
  EXPECT_EQ(0U, test_api()->TextIndexToDisplayIndex(1U));
  EXPECT_EQ(1U, test_api()->TextIndexToDisplayIndex(2U));

  EXPECT_EQ(0U, test_api()->DisplayIndexToTextIndex(0U));
  EXPECT_EQ(2U, test_api()->DisplayIndexToTextIndex(1U));

  // Out of bound accesses.
  EXPECT_EQ(2U, test_api()->TextIndexToDisplayIndex(3U));
  EXPECT_EQ(3U, test_api()->DisplayIndexToTextIndex(2U));

  // Test two U+1F4F7 📷 "Camera" characters in a row.
  // Windows requires wide strings for \Unnnnnnnn universal character names.
  render_text->SetText(WideToUTF16(L"\U0001F4F7\U0001F4F7"));
  render_text->Draw(canvas());

  // Emoji codepoints are replaced by bullets (e.g. "\u2022\u2022").
  EXPECT_EQ(UTF8ToUTF16("\u2022\u2022"), render_text->GetDisplayText());
  EXPECT_EQ(0U, test_api()->TextIndexToDisplayIndex(0U));
  EXPECT_EQ(0U, test_api()->TextIndexToDisplayIndex(1U));
  EXPECT_EQ(1U, test_api()->TextIndexToDisplayIndex(2U));
  EXPECT_EQ(1U, test_api()->TextIndexToDisplayIndex(3U));

  EXPECT_EQ(0U, test_api()->DisplayIndexToTextIndex(0U));
  EXPECT_EQ(2U, test_api()->DisplayIndexToTextIndex(1U));

  // Reveal the first emoji.
  render_text->SetObscuredRevealIndex(0);
  render_text->Draw(canvas());

  EXPECT_EQ(WideToUTF16(L"\U0001F4F7\u2022"), render_text->GetDisplayText());
  EXPECT_EQ(0U, test_api()->TextIndexToDisplayIndex(0U));
  EXPECT_EQ(0U, test_api()->TextIndexToDisplayIndex(1U));
  EXPECT_EQ(2U, test_api()->TextIndexToDisplayIndex(2U));
  EXPECT_EQ(2U, test_api()->TextIndexToDisplayIndex(3U));

  EXPECT_EQ(0U, test_api()->DisplayIndexToTextIndex(0U));
  EXPECT_EQ(0U, test_api()->DisplayIndexToTextIndex(1U));
  EXPECT_EQ(2U, test_api()->DisplayIndexToTextIndex(2U));
}

TEST_F(RenderTextTest, ObscuredEmojiRevealed) {
  RenderText* render_text = GetRenderText();

  base::string16 text = WideToUTF16(L"123\U0001F4F7\U0001F4F7x\U0001F601-");
  for (size_t i = 0; i < text.length(); ++i) {
    render_text->SetText(text);
    render_text->SetObscured(true);
    render_text->SetObscuredRevealIndex(i);
    render_text->Draw(canvas());
  }
}

struct TextIndexConversionCase {
  const char* test_name;
  const wchar_t* text;
};

using TextIndexConversionParam =
    std::tuple<TextIndexConversionCase, bool, size_t>;

class RenderTextTestWithTextIndexConversionCase
    : public RenderTextTest,
      public ::testing::WithParamInterface<TextIndexConversionParam> {
 public:
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<TextIndexConversionParam> param_info) {
    TextIndexConversionCase param = std::get<0>(param_info.param);
    bool obscured = std::get<1>(param_info.param);
    size_t reveal_index = std::get<2>(param_info.param);
    return base::StringPrintf("%s%s%zu", param.test_name,
                              (obscured ? "Obscured" : ""), reveal_index);
  }
};

TEST_P(RenderTextTestWithTextIndexConversionCase, TextIndexConversion) {
  TextIndexConversionCase param = std::get<0>(GetParam());
  bool obscured = std::get<1>(GetParam());
  size_t reveal_index = std::get<2>(GetParam());

  RenderText* render_text = GetRenderText();
  render_text->SetText(WideToUTF16(param.text));
  render_text->SetObscured(obscured);
  render_text->SetObscuredRevealIndex(reveal_index);
  render_text->Draw(canvas());

  base::string16 text = render_text->text();
  base::string16 display_text = render_text->GetDisplayText();

  // Adjust reveal_index to point to the beginning of the surrogate pair, if
  // needed.
  U16_SET_CP_START(text.c_str(), 0, reveal_index);

  // Validate that codepoints still match.
  for (base::i18n::UTF16CharIterator iter(render_text->text()); !iter.end();
       iter.Advance()) {
    size_t text_index = iter.array_pos();
    size_t display_index = test_api()->TextIndexToDisplayIndex(text_index);
    EXPECT_EQ(text_index, test_api()->DisplayIndexToTextIndex(display_index));
    if (obscured && reveal_index != text_index) {
      EXPECT_EQ(display_text[display_index],
                RenderText::kPasswordReplacementChar);
    } else {
      EXPECT_EQ(display_text[display_index], text[text_index]);
    }
  }
}

const TextIndexConversionCase kTextIndexConversionCases[] = {
    {"simple", L"abc"},
    {"simple_obscured1", L"abc"},
    {"simple_obscured2", L"abc"},
    {"emoji_asc", L"\U0001F6281234"},
    {"emoji_asc_obscured0", L"\U0001F6281234"},
    {"emoji_asc_obscured2", L"\U0001F6281234"},
    {"picto_title", L"x☛"},
    {"simple_mixed", L"aaڭڭcc"},
    {"simple_rtl", L"أسكي"},
};

// Validate that conversion text and between display text indexes are consistent
// even when text obscured and reveal character features are used.
INSTANTIATE_TEST_SUITE_P(
    ItemizeTextToRunsConversion,
    RenderTextTestWithTextIndexConversionCase,
    ::testing::Combine(::testing::ValuesIn(kTextIndexConversionCases),
                       testing::Values(false, true),
                       testing::Values(0, 1, 4)),
    RenderTextTestWithTextIndexConversionCase::ParamInfoToString);

struct RunListCase {
  const char* test_name;
  const wchar_t* text;
  const char* expected;
  const bool multiline = false;
};

class RenderTextTestWithRunListCase
    : public RenderTextTest,
      public ::testing::WithParamInterface<RunListCase> {
 public:
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<RunListCase> param_info) {
    return param_info.param.test_name;
  }
};

TEST_P(RenderTextTestWithRunListCase, ItemizeTextToRuns) {
  RunListCase param = GetParam();
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetMultiline(param.multiline);
  render_text->SetText(WideToUTF16(param.text));
  EXPECT_EQ(param.expected, GetRunListStructureString());
}

const RunListCase kBasicsRunListCases[] = {
    {"simpleLTR", L"abc", "[0->2]"},
    {"simpleRTL", L"ښڛڜ", "[2<-0]"},
    {"asc_arb", L"abcښڛڜdef", "[0->2][5<-3][6->8]"},
    {"asc_dev_asc", L"abcऔकखdefڜ", "[0->2][3->5][6->8][9]"},
    {"phone", L"1-(800)-xxx-xxxx", "[0][1][2][3->5][6][7][8->10][11][12->15]"},
    {"dev_ZWS", L"क\u200Bख", "[0][1][2]"},
    {"numeric", L"1 2 3 4", "[0][1][2][3][4][5][6]"},
    {"joiners1", L"1\u200C2\u200C3\u200C4", "[0->6]"},
    {"joiners2", L"\u060F\u200C\u060F", "[0->2]"},
    {"combining_accents1", L"a\u0300e\u0301", "[0->3]"},
    {"combining_accents2", L"\u0065\u0308\u0435\u0308", "[0->1][2->3]"},
    {"picto_title", L"☞☛test☚☜", "[0->1][2->5][6->7]"},
    {"picto_LTR", L"☺☺☺!", "[0->2][3]"},
    {"picto_RTL", L"☺☺☺ښ", "[3][2<-0]"},
    {"paren_picto", L"(☾☹☽)", "[0][1][2][3][4]"},
    {"emoji_asc", L"\U0001F6281234",
     "[0->1][2->5]"},  // http://crbug.com/530021
    {"emoji_title", L"▶Feel goods",
     "[0][1->4][5][6->10]"},  // http://crbug.com/278913
    {"jap_paren1", L"ぬ「シ」ほ",
     "[0][1][2][3][4]"},  // http://crbug.com/396776
    {"jap_paren2", L"國哲(c)1",
     "[0->1][2][3][4][5]"},  // http://crbug.com/125792
    {"newline1", L"\n\n", "[0->1]"},
    {"newline2", L"\r\n\r\n", "[0->3]"},
    {"newline3", L"\r\r\n", "[0->2]"},
    {"multiline_newline1", L"\n\n", "[0][1]", true},
    {"multiline_newline2", L"\r\n\r\n", "[0->1][2->3]", true},
    {"multiline_newline3", L"\r\r\n", "[0][1->2]", true},
};

INSTANTIATE_TEST_SUITE_P(ItemizeTextToRunsBasics,
                         RenderTextTestWithRunListCase,
                         ::testing::ValuesIn(kBasicsRunListCases),
                         RenderTextTestWithRunListCase::ParamInfoToString);

// see 'Unicode Bidirectional Algorithm': http://unicode.org/reports/tr9/
const RunListCase kBidiRunListCases[] = {
    {"simple_ltr", L"ascii", "[0->4]"},
    {"simple_rtl", L"أسكي", "[3<-0]"},
    {"simple_mixed", L"aaڭڭcc", "[0->1][3<-2][4->5]"},
    {"simple_mixed_LRE", L"\u202Aaaڭڭcc\u202C", "[0][1->2][4<-3][5->6][7]"},
    {"simple_mixed_RLE", L"\u202Baaڭڭcc\u202C", "[7][5->6][4<-3][0][1->2]"},
    {"sequence_RLE", L"\u202Baa\u202C\u202Bbb\u202C",
     "[7][0][1->2][3->4][5->6]"},
    {"simple_mixed_LRI", L"\u2066aaڭڭcc\u2069", "[0][1->2][4<-3][5->6][7]"},
    {"simple_mixed_RLI", L"\u2067aaڭڭcc\u2069", "[0][5->6][4<-3][1->2][7]"},
    {"sequence_RLI", L"\u2067aa\u2069\u2067bb\u2069",
     "[0][1->2][3->4][5->6][7]"},
    {"override_ltr_RLO", L"\u202Eaaa\u202C", "[4][3<-1][0]"},
    {"override_rtl_LRO", L"\u202Dڭڭڭ\u202C", "[0][1->3][4]"},
    {"neutral_strong_ltr", L"a!!a", "[0][1->2][3]"},
    {"neutral_strong_rtl", L"ڭ!!ڭ", "[3][2<-1][0]"},
    {"neutral_strong_both", L"a a ڭ ڭ", "[0][1][2][3][6][5][4]"},
    {"neutral_strong_both_RLE", L"\u202Ba a ڭ ڭ\u202C",
     "[8][7][6][5][4][0][1][2][3]"},
    {"weak_numbers", L"one ڭ222ڭ", "[0->2][3][8][5->7][4]"},
    {"not_weak_letters", L"one ڭabcڭ", "[0->2][3][4][5->7][8]"},
    {"weak_arabic_numbers", L"one ڭ١٢٣ڭ", "[0->2][3][8][5->7][4]"},
    {"neutral_LRM_pre", L"\u200E\u2026\u2026", "[0->2]"},
    {"neutral_LRM_post", L"\u2026\u2026\u200E", "[0->2]"},
    {"neutral_RLM_pre", L"\u200F\u2026\u2026", "[2<-0]"},
    {"neutral_RLM_post", L"\u2026\u2026\u200F", "[2<-0]"},
    {"brackets_ltr", L"aa(ڭڭ)\u2026\u2026", "[0->1][2][4<-3][5][6->7]"},
    {"brackets_rtl", L"ڭڭ(aa)\u2026\u2026", "[7<-6][5][3->4][2][1<-0]"},
    {"mixed_with_punct", L"aa \"ڭڭ!\", aa",
     "[0->1][2][3][5<-4][6->8][9][10->11]"},
    {"mixed_with_punct_RLI", L"aa \"\u2067ڭڭ!\u2069\", aa",
     "[0->1][2][3][4][7][6<-5][8][9->10][11][12->13]"},
    {"mixed_with_punct_RLM", L"aa \"ڭڭ!\u200F\", aa",
     "[0->1][2][3][7][6][5<-4][8->9][10][11->12]"},
};

INSTANTIATE_TEST_SUITE_P(ItemizeTextToRunsBidi,
                         RenderTextTestWithRunListCase,
                         ::testing::ValuesIn(kBidiRunListCases),
                         RenderTextTestWithRunListCase::ParamInfoToString);

const RunListCase kBracketsRunListCases[] = {
    {"matched_parens", L"(a)", "[0][1][2]"},
    {"double_matched_parens", L"((a))", "[0->1][2][3->4]"},
    {"double_matched_parens2", L"((aaa))", "[0->1][2->4][5->6]"},
    {"square_brackets", L"[...]x", "[0][1->3][4][5]"},
    {"curly_brackets", L"{}x{}", "[0->1][2][3->4]"},
    {"style_brackets", L"\u300c...\u300dx", "[0][1->3][4][5]"},
    {"tibetan_brackets", L"\u0f3a\u0f3b\u0f20\u0f20\u0f3c\u0f3d",
     "[0->1][2->3][4->5]"},
    {"angle_brackets", L"\u3008\u3007\u3007\u3009", "[0][1->2][3]"},
    {"double_angle_brackets", L"\u300A\u3007\u3007\u300B", "[0][1->2][3]"},
    {"corner_angle_brackets", L"\u300C\u3007\u3007\u300D", "[0][1->2][3]"},
    {"fullwidth_parens", L"\uff08\uff01\uff09", "[0][1][2]"},
};

INSTANTIATE_TEST_SUITE_P(ItemizeTextToRunsBrackets,
                         RenderTextTestWithRunListCase,
                         ::testing::ValuesIn(kBracketsRunListCases),
                         RenderTextTestWithRunListCase::ParamInfoToString);

// Test cases to ensure the extraction of script extensions are taken into
// account while performing the text itemization.
// See table 7 from http://www.unicode.org/reports/tr24/tr24-29.html
const RunListCase kScriptExtensionRunListCases[] = {
    {"implicit_com_inherited", L"a\u0301", "[0->1]"},
    {"explicit_lat", L"\u0061d", "[0->1]"},
    {"explicit_inherited_lat", L"x\u0363d", "[0->2]"},
    {"explicit_inherited_dev", L"क\u1CD1क", "[0->2]"},
    {"multi_explicit_hira", L"は\u30FCz", "[0->1][2]"},
    {"multi_explicit_kana", L"ハ\u30FCz", "[0->1][2]"},
    {"multi_explicit_lat", L"a\u30FCz", "[0][1][2]"},
    {"multi_explicit_impl_dev", L"क\u1CD0z", "[0->1][2]"},
    {"multi_explicit_expl_dev", L"क\u096Fz", "[0->1][2]"},
};

INSTANTIATE_TEST_SUITE_P(ItemizeTextToRunsScriptExtension,
                         RenderTextTestWithRunListCase,
                         ::testing::ValuesIn(kScriptExtensionRunListCases),
                         RenderTextTestWithRunListCase::ParamInfoToString);

// Test cases to ensure ItemizeTextToRuns is splitting text based on unicode
// script (intersection of script extensions).
// See ScriptExtensions.txt and Scripts.txt from
// http://www.unicode.org/reports/tr24/tr24-29.html
const RunListCase kScriptsRunListCases[] = {
    {"lat", L"abc", "[0->2]"},
    {"lat_diac", L"e\u0308f", "[0->2]"},
    // Indic Fraction codepoints have large set of script extensions.
    {"indic_fraction", L"\uA830\uA832\uA834\uA835", "[0->3]"},
    // Devanagari Danda codepoints have large set of script extensions.
    {"dev_danda", L"\u0964\u0965", "[0->1]"},
    // Combining Diacritical Marks (inherited) should only merge with preceding.
    {"diac_lat", L"\u0308fg", "[0][1->2]"},
    {"diac_dev", L"क\u0308f", "[0->1][2]"},
    // ZWJW has the inherited script.
    {"lat_ZWNJ", L"ab\u200Ccd", "[0->4]"},
    {"dev_ZWNJ", L"क\u200Cक", "[0->2]"},
    {"lat_dev_ZWNJ", L"a\u200Cक", "[0->1][2]"},
    // Invalid codepoints.
    {"invalid_cp", L"\uFFFE", "[0]"},
    {"invalid_cps", L"\uFFFE\uFFFF", "[0->1]"},
    {"unknown", L"a\u243Fb", "[0][1][2]"},

    // Codepoints from different code block should be in same run when part of
    // the same script.
    {"blocks_lat", L"aɒɠƉĚÑ", "[0->5]"},
    {"blocks_lat_paren", L"([_!_])", "[0->1][2->4][5->6]"},
    {"blocks_lat_sub", L"ₐₑaeꬱ", "[0->4]"},
    {"blocks_lat_smallcap", L"ꟺＭ", "[0->1]"},
    {"blocks_lat_small_letter", L"ᶓᶍᶓᴔᴟ", "[0->4]"},
    {"blocks_lat_acc", L"eéěĕȩɇḕẻếⱻꞫ", "[0->10]"},
    {"blocks_com", L"⟦ℳ¥¾⟾⁸⟧Ⓔ", "[0][1][2->3][4][5][6][7]"},

    // Latin script.
    {"latin_numbers", L"a1b2c3", "[0][1][2][3][4][5]"},
    {"latin_puncts1", L"a,b,c.", "[0][1][2][3][4][5]"},
    {"latin_puncts2", L"aa,bb,cc!!", "[0->1][2][3->4][5][6->7][8->9]"},
    {"latin_diac_multi", L"a\u0300e\u0352i", "[0->4]"},

    // Common script.
    {"common_tm", L"•bug™", "[0][1->3][4]"},
    {"common_copyright", L"chromium©", "[0->7][8]"},
    {"common_math1", L"ℳ: ¬ƒ(x)=½×¾", "[0][1][2][3][4][5][6][7][8][9->11]"},
    {"common_math2", L"𝟏×𝟑", "[0->1][2][3->4]"},
    {"common_numbers", L"🄀𝟭𝟐⒓¹²", "[0->1][2->5][6][7->8]"},
    {"common_puncts", L",.\u0083!", "[0->1][2][3]"},

    // Arabic script.
    {"arabic", L"\u0633\u069b\u0763\u077f\u08A2\uFB53", "[5<-0]"},
    {"arabic_lat", L"\u0633\u069b\u0763\u077f\u08A2\uFB53abc", "[6->8][5<-0]"},
    {"arabic_word_ligatures", L"\uFDFD\uFDF3", "[1<-0]"},
    {"arabic_diac", L"\u069D\u0300", "[1<-0]"},
    {"arabic_diac_lat", L"\u069D\u0300abc", "[2->4][1<-0]"},
    {"arabic_diac_lat2", L"abc\u069D\u0300abc", "[0->2][4<-3][5->7]"},
    {"arabic_lyd", L"\U00010935\U00010930\u06B0\u06B1", "[5<-4][3<-0]"},
    {"arabic_numbers", L"12\u06D034", "[3->4][2][0->1]"},
    {"arabic_letters", L"ab\u06D0cd", "[0->1][2][3->4]"},
    {"arabic_mixed", L"a1\u06D02d", "[0][1][3][2][4]"},
    {"arabic_coptic1", L"\u06D0\U000102E2\u2CB2", "[1->3][0]"},
    {"arabic_coptic2", L"\u2CB2\U000102E2\u06D0", "[0->2][3]"},

    // Devanagari script.
    {"devanagari1", L"ञटठडढणतथ", "[0->7]"},
    {"devanagari2", L"ढ꣸ꣴ", "[0->2]"},
    {"devanagari_vowels", L"\u0915\u093F\u0915\u094C", "[0->3]"},
    {"devanagari_consonants", L"\u0915\u094D\u0937", "[0->2]"},

    // Ethiopic script.
    {"ethiopic", L"መጩጪᎅⶹⶼꬣꬦ", "[0->7]"},
    {"ethiopic_numbers", L"1ቨቤ2", "[0][1->2][3]"},
    {"ethiopic_mixed1", L"abቨቤ12", "[0->1][2->3][4->5]"},
    {"ethiopic_mixed2", L"a1ቨቤb2", "[0][1][2->3][4][5]"},

    // Georgian script.
    {"georgian1", L"ႼႽႾႿჀჁჂჳჴჵ", "[0->9]"},
    {"georgian2", L"ლⴊⴅ", "[0->2]"},
    {"georgian_numbers", L"1ლⴊⴅ2", "[0][1->3][4]"},
    {"georgian_mixed", L"a1ლⴊⴅb2", "[0][1][2->4][5][6]"},

    // Telugu script.
    {"telugu_lat", L"aaఉయ!", "[0->1][2->3][4]"},
    {"telugu_numbers", L"123౦౧౨456౩౪౫", "[0->2][3->5][6->8][9->11]"},
    {"telugu_puncts", L"కురుచ, చిఱుత, చేరువ, చెఱువు!",
     "[0->4][5][6][7->11][12][13][14->18][19][20][21->26][27]"},

    // Control Pictures.
    {"control_pictures", L"␑␒␓␔␕␖␗␘␙␚␛", "[0->10]"},
    {"control_pictures_rewrite", L"␑\t␛", "[0->2]"},

    // Unicode art.
    {"unicode_emoticon1", L"(▀̿ĺ̯▀̿ ̿)", "[0][1->2][3->4][5->6][7->8][9]"},
    {"unicode_emoticon2", L"▀̿̿Ĺ̯̿▀̿ ", "[0->2][3->5][6->7][8]"},
    {"unicode_emoticon3", L"( ͡° ͜ʖ ͡°)", "[0][1->2][3][4->5][6][7->8][9][10]"},
    {"unicode_emoticon4", L"✩·͙*̩̩͙˚̩̥̩̥( ͡ᵔ ͜ʖ ͡ᵔ )*̩̩͙✩·͙˚̩̥̩̥.",
     "[0][1->2][3->6][7->11][12][13->14][15][16->17][18][19->20][21][22][23]["
     "24->27][28][29->30][31->35][36]"},
    {"unicode_emoticon5", L"ヽ(͡◕ ͜ʖ ͡◕)ﾉ",
     "[0][1->2][3][4->5][6][7->8][9][10][11]"},
    {"unicode_art1", L"꧁༒✧ Great ✧༒꧂", "[0][1][2][3][4->8][9][10][11][12]"},
    {"unicode_art2", L"t͎e͎s͎t͎", "[0->7]"},

    // Combining diacritical sequences.
    {"unicode_diac1", L"\u2123\u0336", "[0->1]"},
    {"unicode_diac2", L"\u273c\u0325", "[0->1]"},
    {"unicode_diac3", L"\u2580\u033f", "[0->1]"},
    {"unicode_diac4", L"\u2022\u0325\u0329", "[0->2]"},
    {"unicode_diac5", L"\u2022\u0325", "[0->1]"},
    {"unicode_diac6", L"\u00b7\u0359\u0325", "[0->2]"},
    {"unicode_diac7", L"\u2027\u0329\u0325", "[0->2]"},
    {"unicode_diac8", L"\u0332\u0305\u03c1", "[0->1][2]"},
};

INSTANTIATE_TEST_SUITE_P(ItemizeTextToRunsScripts,
                         RenderTextTestWithRunListCase,
                         ::testing::ValuesIn(kScriptsRunListCases),
                         RenderTextTestWithRunListCase::ParamInfoToString);

// Test cases to ensure ItemizeTextToRuns is splitting emoji correctly.
// see: http://www.unicode.org/reports/tr51
// see: http://www.unicode.org/emoji/charts/full-emoji-list.html
const RunListCase kEmojiRunListCases[] = {
    // Samples from
    // https://www.unicode.org/Public/emoji/12.0/emoji-data.txt
    {"number_sign", L"\u0023", "[0]"},
    {"keyboard", L"\u2328", "[0]"},
    {"aries", L"\u2648", "[0]"},
    {"candle", L"\U0001F56F", "[0->1]"},
    {"anchor", L"\u2693", "[0]"},
    {"grinning_face", L"\U0001F600", "[0->1]"},
    {"face_with_monocle", L"\U0001F9D0", "[0->1]"},
    {"light_skin_tone", L"\U0001F3FB", "[0->1]"},
    {"index_pointing_up", L"\u261D", "[0]"},
    {"horse_racing", L"\U0001F3C7", "[0->1]"},
    {"kiss", L"\U0001F48F", "[0->1]"},
    {"couple_with_heart", L"\U0001F491", "[0->1]"},
    {"people_wrestling", L"\U0001F93C", "[0->1]"},
    {"eject_button", L"\u23CF", "[0]"},

    // Samples from
    // https://unicode.org/Public/emoji/12.0/emoji-sequences.txt
    {"watch", L"\u231A", "[0]"},
    {"cross_mark", L"\u274C", "[0]"},
    {"copyright", L"\u00A9\uFE0F", "[0->1]"},
    {"stop_button", L"\u23F9\uFE0F", "[0->1]"},
    {"passenger_ship", L"\U0001F6F3\uFE0F", "[0->2]"},
    {"keycap_star", L"\u002A\uFE0F\u20E3", "[0->2]"},
    {"keycap_6", L"\u0036\uFE0F\u20E3", "[0->2]"},
    {"flag_andorra", L"\U0001F1E6\U0001F1E9", "[0->3]"},
    {"flag_egypt", L"\U0001F1EA\U0001F1EC", "[0->3]"},
    {"flag_england",
     L"\U0001F3F4\U000E0067\U000E0062\U000E0065\U000E006E\U000E0067\U000E007F",
     "[0->13]"},
    {"index_up_light", L"\u261D\U0001F3FB", "[0->2]"},
    {"person_bouncing_ball_light", L"\u26F9\U0001F3FC", "[0->2]"},
    {"victory_hand_med_light", L"\u270C\U0001F3FC", "[0->2]"},
    {"horse_racing_med_dark", L"\U0001F3C7\U0001F3FE", "[0->3]"},
    {"woman_man_hands_light", L"\U0001F46B\U0001F3FB", "[0->3]"},
    {"person_haircut_med_light", L"\U0001F487\U0001F3FC", "[0->3]"},
    {"pinching_hand_light", L"\U0001F90F\U0001F3FB", "[0->3]"},
    {"love_you_light", L"\U0001F91F\U0001F3FB", "[0->3]"},
    {"leg_dark", L"\U0001F9B5\U0001F3FF", "[0->3]"},

    // Samples from
    // https://unicode.org/Public/emoji/12.0/emoji-variation-sequences.txt
    {"number_sign_text", L"\u0023\uFE0E", "[0->1]"},
    {"number_sign_emoji", L"\u0023\uFE0F", "[0->1]"},
    {"digit_eight_text", L"\u0038\uFE0E", "[0->1]"},
    {"digit_eight_emoji", L"\u0038\uFE0F", "[0->1]"},
    {"up_down_arrow_text", L"\u2195\uFE0E", "[0->1]"},
    {"up_down_arrow_emoji", L"\u2195\uFE0F", "[0->1]"},
    {"stopwatch_text", L"\u23F1\uFE0E", "[0->1]"},
    {"stopwatch_emoji", L"\u23F1\uFE0F", "[0->1]"},
    {"thermometer_text", L"\U0001F321\uFE0E", "[0->2]"},
    {"thermometer_emoji", L"\U0001F321\uFE0F", "[0->2]"},
    {"thumbs_up_text", L"\U0001F44D\uFE0E", "[0->2]"},
    {"thumbs_up_emoji", L"\U0001F44D\uFE0F", "[0->2]"},
    {"hole_text", L"\U0001F573\uFE0E", "[0->2]"},
    {"hole_emoji", L"\U0001F573\uFE0F", "[0->2]"},

    // Samples from
    // https://unicode.org/Public/emoji/12.0/emoji-zwj-sequences.txt
    {"couple_man_man", L"\U0001F468\u200D\u2764\uFE0F\u200D\U0001F468",
     "[0->7]"},
    {"kiss_man_man",
     L"\U0001F468\u200D\u2764\uFE0F\u200D\U0001F48B\u200D\U0001F468",
     "[0->10]"},
    {"family_man_woman_girl_boy",
     L"\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466", "[0->10]"},
    {"men_hands_dark_medium",
     L"\U0001F468\U0001F3FF\u200D\U0001F91D\u200D\U0001F468\U0001F3FD",
     "[0->11]"},
    {"people_hands_dark",
     L"\U0001F9D1\U0001F3FF\u200D\U0001F91D\u200D\U0001F9D1\U0001F3FF",
     "[0->11]"},
    {"man_pilot", L"\U0001F468\u200D\u2708\uFE0F", "[0->4]"},
    {"man_scientist", L"\U0001F468\u200D\U0001F52C", "[0->4]"},
    {"man_mechanic_light", L"\U0001F468\U0001F3FB\u200D\U0001F527", "[0->6]"},
    {"man_judge_medium", L"\U0001F468\U0001F3FD\u200D\u2696\uFE0F", "[0->6]"},
    {"woman_cane_dark", L"\U0001F469\U0001F3FF\u200D\U0001F9AF", "[0->6]"},
    {"woman_ball_light", L"\u26F9\U0001F3FB\u200D\u2640\uFE0F", "[0->5]"},
    {"woman_running", L"\U0001F3C3\u200D\u2640\uFE0F", "[0->4]"},
    {"woman_running_dark", L"\U0001F3C3\U0001F3FF\u200D\u2640\uFE0F", "[0->6]"},
    {"woman_turban", L"\U0001F473\u200D\u2640\uFE0F", "[0->4]"},
    {"woman_detective", L"\U0001F575\uFE0F\u200D\u2640\uFE0F", "[0->5]"},
    {"man_facepalming", L"\U0001F926\u200D\u2642\uFE0F", "[0->4]"},
    {"man_red_hair", L"\U0001F468\u200D\U0001F9B0", "[0->4]"},
    {"man_medium_curly", L"\U0001F468\U0001F3FD\u200D\U0001F9B1", "[0->6]"},
    {"woman_dark_white_hair", L"\U0001F469\U0001F3FF\u200D\U0001F9B3",
     "[0->6]"},
    {"rainbow_flag", L"\U0001F3F3\uFE0F\u200D\U0001F308", "[0->5]"},
    {"pirate_flag", L"\U0001F3F4\u200D\u2620\uFE0F", "[0->4]"},
    {"service_dog", L"\U0001F415\u200D\U0001F9BA", "[0->4]"},
    {"eye_bubble", L"\U0001F441\uFE0F\u200D\U0001F5E8\uFE0F", "[0->6]"},
};

INSTANTIATE_TEST_SUITE_P(ItemizeTextToRunsEmoji,
                         RenderTextTestWithRunListCase,
                         ::testing::ValuesIn(kEmojiRunListCases),
                         RenderTextTestWithRunListCase::ParamInfoToString);

struct ElideTextTestOptions {
  const ElideBehavior elide_behavior;
};

const bool kForceNoWhitespaceElision = false;
const bool kForceWhitespaceElision = true;

struct ElideTextCase {
  const char* test_name;
  const wchar_t* text;
  const wchar_t* display_text;
  // The available width, specified as a number of fixed-width glyphs. If no
  // value is specified, the width of the resulting |display_text| is used. This
  // helps test available widths larger than the resulting test; e.g. "a  b"
  // should yield "a\u2026" even if 3 glyph widths are available, when
  // whitespace elision is enabled.
  const base::Optional<size_t> available_width_as_glyph_count = base::nullopt;
  const base::Optional<bool> whitespace_elision = base::nullopt;
};

using ElideTextCaseParam = std::tuple<ElideTextTestOptions, ElideTextCase>;

class RenderTextTestWithElideTextCase
    : public RenderTextTest,
      public ::testing::WithParamInterface<ElideTextCaseParam> {
 public:
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<ElideTextCaseParam> param_info) {
    return std::get<1>(param_info.param).test_name;
  }
};

TEST_P(RenderTextTestWithElideTextCase, ElideText) {
  // This test requires glyphs to be the same width.
  constexpr int kGlyphWidth = 10;
  SetGlyphWidth(kGlyphWidth);

  const ElideTextTestOptions options = std::get<0>(GetParam());
  const ElideTextCase param = std::get<1>(GetParam());
  const base::string16 text = WideToUTF16(param.text);
  const base::string16 display_text = WideToUTF16(param.display_text);

  // Retrieve the display_text width without eliding.
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetText(display_text);
  const int expected_width = render_text->GetContentWidth();

  // Set the text and the eliding behavior.
  render_text->SetText(text);
  render_text->SetElideBehavior(options.elide_behavior);

  // If specified, set the whitespace elision. Otherwise, keep the eliding
  // behavior default value.
  if (param.whitespace_elision.has_value())
    render_text->SetWhitespaceElision(param.whitespace_elision.value());

  // Set the display width to trigger the eliding.
  if (param.available_width_as_glyph_count.has_value()) {
    render_text->SetDisplayRect(
        Rect(0, 0,
             param.available_width_as_glyph_count.value() * kGlyphWidth +
                 kGlyphWidth / 2,
             100));
  } else {
    render_text->SetDisplayRect(
        Rect(0, 0, expected_width + kGlyphWidth / 2, 100));
  }

  const int elided_width = render_text->GetContentWidth();

  EXPECT_EQ(text, render_text->text());
  EXPECT_EQ(display_text, render_text->GetDisplayText());
  EXPECT_EQ(elided_width, expected_width);
}

const ElideTextCase kElideHeadTextCases[] = {
    {"empty", L"", L""},
    {"letter_m_tail0", L"M", L""},
    {"letter_m_tail1", L"M", L"M"},
    {"no_eliding", L"012ab", L"012ab"},
    {"ltr_3", L"abc", L"abc"},
    {"ltr_2", L"abc", L"\u2026c"},
    {"ltr_1", L"abc", L"\u2026"},
    {"ltr_0", L"abc", L""},
    {"rtl_3", L"\u05d0\u05d1\u05d2", L"\u05d0\u05d1\u05d2"},
    {"rtl_2", L"\u05d0\u05d1\u05d2", L"\u2026\u05d2"},
    {"rtl_1", L"\u05d0\u05d1\u05d2", L"\u2026"},
    {"rtl_0", L"\u05d0\u05d1\u05d2", L""},
    {"ltr_rtl_5", L"abc\u05d0\u05d1\u05d2", L"\u2026c\u05d0\u05d1\u05d2"},
    {"ltr_rtl_4", L"abc\u05d0\u05d1\u05d2", L"\u2026\u05d0\u05d1\u05d2"},
    {"ltr_rtl_3", L"abc\u05d0\u05d1\u05d2", L"\u2026\u05d1\u05d2"},
    {"rtl_ltr_5", L"\u05d0\u05d1\u05d2abc", L"\u2026\u05d2abc"},
    {"rtl_ltr_4", L"\u05d0\u05d1\u05d2abc", L"\u2026abc"},
    {"rtl_ltr_3", L"\u05d0\u05d1\u05d2abc", L"\u2026bc"},
    {"bidi_1", L"a\u05d1b\u05d1c012", L"\u2026b\u05d1c012"},
    {"bidi_2", L"a\u05d1b\u05d1c012", L"\u2026\u05d1c012"},
    {"bidi_3", L"a\u05d1b\u05d1c012", L"\u2026c012"},
    // Test surrogate pairs. No surrogate pair should be partially elided.
    {"surrogate1", L"abc\U0001D11E\U0001D122x", L"\u2026\U0001D11E\U0001D122x"},
    {"surrogate2", L"abc\U0001D11E\U0001D122x", L"\u2026\U0001D122x"},
    {"surrogate3", L"abc\U0001D11E\U0001D122x", L"\u2026x"},
    // Test combining character sequences. U+0915 U+093F forms a compound
    // glyph, as does U+0915 U+0942. No combining sequence should be partially
    // elided.
    {"combining1", L"0123\u0915\u093f\u0915\u0942456",
     L"\u2026\u0915\u0942456"},
    {"combining2", L"0123\u0915\u093f\u0915\u0942456", L"\u2026456"},
    // 𝄞 (U+1D11E, MUSICAL SYMBOL G CLEF) should be fully elided.
    {"emoji1", L"012\U0001D11Ex", L"\u2026\U0001D11Ex"},
    {"emoji2", L"012\U0001D11Ex", L"\u2026x"},

    // Whitespace elision tests.
    {"empty_no_elision", L"", L"", 0, kForceNoWhitespaceElision},
    {"empty_elision", L"", L"", 0, kForceWhitespaceElision},
    {"xyz_no_elision", L"  x  xyz", L"\u2026 xyz", 5,
     kForceNoWhitespaceElision},
    {"xyz_elision", L"  x  xyz", L"\u2026xyz", 5, kForceWhitespaceElision},
    {"ltr_rtl_elision3", L"x  \u05d1  y    \u05d2", L"\u2026\u05d2", 3,
     kForceWhitespaceElision},
    {"ltr_rtl_elision6", L"x  \u05d1  y    \u05d2", L"\u2026\u05d2", 6,
     kForceWhitespaceElision},
    {"ltr_rtl_elision7", L"x  \u05d1  y    \u05d2", L"\u2026y    \u05d2", 7,
     kForceWhitespaceElision},
    {"ltr_rtl_elision10", L"x  \u05d1  y    \u05d2",
     L"\u2026\u05d1  y    \u05d2", 10, kForceWhitespaceElision},
    {"ltr_rtl_elision11", L"x  \u05d1  y    \u05d2",
     L"\u2026\u05d1  y    \u05d2", 11, kForceWhitespaceElision},
    // Emoji U+1F601 and emoji U+1F321 U+FE0E are graphemes that result in
    // one glyph each. Eliding a glyph must remove the whole grapheme. It is
    // invalid to break a grapheme in pieces.
    {"graphemes_elision3", L"  \U0001F601  \U0001F321\uFE0E  ", L"\u2026", 3,
     kForceWhitespaceElision},
    {"graphemes_elision4", L"  \U0001F601  \U0001F321\uFE0E  ",
     L"\u2026\U0001F321\uFE0E  ", 4, kForceWhitespaceElision},
    {"graphemes_elision6", L"  \U0001F601  \U0001F321\uFE0E  ",
     L"\u2026\U0001F321\uFE0E  ", 6, kForceWhitespaceElision},
    {"graphemes_elision7", L"  \U0001F601  \U0001F321\uFE0E  ",
     L"\u2026\U0001F601  \U0001F321\uFE0E  ", 7, kForceWhitespaceElision},
};

INSTANTIATE_TEST_SUITE_P(
    ElideHead,
    RenderTextTestWithElideTextCase,
    testing::Combine(testing::Values(ElideTextTestOptions{ELIDE_HEAD}),
                     testing::ValuesIn(kElideHeadTextCases)),
    RenderTextTestWithElideTextCase::ParamInfoToString);

const ElideTextCase kElideTailTextCases[] = {
    {"empty", L"", L""},
    {"letter_m_tail0", L"M", L""},
    {"letter_m_tail1", L"M", L"M"},
    {"letter_weak_3", L" . ", L" . "},
    {"letter_weak_2", L" . ", L"\u2026"},
    {"no_eliding", L"012ab", L"012ab"},
    {"ltr_3", L"abc", L"abc"},
    {"ltr_2", L"abc", L"a\u2026"},
    {"ltr_1", L"abc", L"\u2026"},
    {"ltr_0", L"abc", L""},
    {"rtl_3", L"\u05d0\u05d1\u05d2", L"\u05d0\u05d1\u05d2"},
    {"rtl_2", L"\u05d0\u05d1\u05d2", L"\u05d0\u2026"},
    {"rtl_1", L"\u05d0\u05d1\u05d2", L"\u2026"},
    {"rtl_0", L"\u05d0\u05d1\u05d2", L""},
    {"ltr_rtl_5", L"abc\u05d0\u05d1\u05d2", L"abc\u05d0\u2026\u200F"},
    {"ltr_rtl_4", L"abc\u05d0\u05d1\u05d2", L"abc\u2026"},
    {"ltr_rtl_3", L"abc\u05d0\u05d1\u05d2", L"ab\u2026"},
    {"rtl_ltr_5", L"\u05d0\u05d1\u05d2abc", L"\u05d0\u05d1\u05d2a\u2026\u200E"},
    {"rtl_ltr_4", L"\u05d0\u05d1\u05d2abc", L"\u05d0\u05d1\u05d2\u2026"},
    {"rtl_ltr_3", L"\u05d0\u05d1\u05d2abc", L"\u05d0\u05d1\u2026"},
    {"bidi_1", L"012a\u05d1b\u05d1c", L"012a\u2026"},
    {"bidi_2", L"012a\u05d1b\u05d1c", L"012a\u05d1\u2026\u200F"},
    {"bidi_3", L"012a\u05d1b\u05d1c", L"012a\u05d1b\u2026"},
    // No RLM marker added as digits (012) have weak directionality.
    {"no_rlm", L"01\u05d0\u05d1\u05d2", L"01\u05d0\u2026"},
    // RLM marker added as "ab" have strong LTR directionality.
    {"with_rlm", L"ab\u05d0\u05d1\u05d2cd", L"ab\u05d0\u05d1\u2026\u200f"},
    // Test surrogate pairs. The first pair 𝄞 'MUSICAL SYMBOL G CLEF' U+1D11E
    // should be kept, and the second pair 𝄢 'MUSICAL SYMBOL F CLEF' U+1D122
    // should be removed. No surrogate pair should be partially elided.
    {"surrogate", L"0123\U0001D11E\U0001D122x", L"0123\U0001D11E\u2026"},
    // Test combining character sequences. U+0915 U+093F forms a compound
    // glyph, as does U+0915 U+0942. The first should be kept; the second
    // removed. No combining sequence should be partially elided.
    {"combining", L"0123\u0915\u093f\u0915\u0942456",
     L"0123\u0915\u093f\u2026"},
    // U+05E9 U+05BC U+05C1 U+05B8 forms a four-character compound glyph.
    // It should be either fully elided, or not elided at all. If completely
    // elided, an LTR Mark (U+200E) should be added.
    {"grapheme1", L"0\u05e9\u05bc\u05c1\u05b8", L"0\u05e9\u05bc\u05c1\u05b8"},
    {"grapheme2", L"0\u05e9\u05bc\u05c1\u05b8abc", L"0\u2026\u200E"},
    {"grapheme3", L"01\u05e9\u05bc\u05c1\u05b8abc", L"01\u2026\u200E"},
    {"grapheme4", L"012\u05e9\u05bc\u05c1\u05b8abc", L"012\u2026\u200E"},
    // 𝄞 (U+1D11E, MUSICAL SYMBOL G CLEF) should be fully elided.
    {"emoji", L"012\U0001D11Ex", L"012\u2026"},

    // Whitespace elision tests.
    {"empty_no_elision", L"", L"", 0, kForceNoWhitespaceElision},
    {"empty_elision", L"", L"", 0, kForceWhitespaceElision},
    {"letter_weak_2_no_elision", L" . ", L" \u2026", 2,
     kForceNoWhitespaceElision},
    {"xyz_no_elision", L"  x  xyz", L"  x \u2026", 5,
     kForceNoWhitespaceElision},
    {"xyz_elision", L"  x  xyz", L"  x\u2026", 5, kForceWhitespaceElision},
    {"ltr_rtl_elision4", L"x  \u05d1  y    \u05d2", L"x\u2026", 4,
     kForceWhitespaceElision},
    {"ltr_rtl_elision5", L"x  \u05d1  y    \u05d2", L"x  \u05d1\u2026\u200F", 5,
     kForceWhitespaceElision},
    {"ltr_rtl_elision9", L"x  \u05d1  y    \u05d2", L"x  \u05d1  y\u2026", 9,
     kForceWhitespaceElision},
    // Emoji U+1F601 and emoji U+1F321 U+FE0E are graphemes that result in
    // one glyph each. Eliding a glyph must remove the whole grapheme. It is
    // invalid to break a grapheme in pieces.
    {"graphemes_elision3", L"  \U0001F601  \U0001F321\uFE0E  ", L"\u2026", 3,
     kForceWhitespaceElision},
    {"graphemes_elision6", L"  \U0001F601  \U0001F321\uFE0E  ",
     L"  \U0001F601\u2026", 6, kForceWhitespaceElision},
    {"graphemes_elision7", L"  \U0001F601  \U0001F321\uFE0E  ",
     L"  \U0001F601  \U0001F321\uFE0E\u2026", 7, kForceWhitespaceElision},
};

INSTANTIATE_TEST_SUITE_P(
    ElideTail,
    RenderTextTestWithElideTextCase,
    testing::Combine(testing::Values(ElideTextTestOptions{ELIDE_TAIL}),
                     testing::ValuesIn(kElideTailTextCases)),
    RenderTextTestWithElideTextCase::ParamInfoToString);

const ElideTextCase kElideTruncateTextCases[] = {
    {"empty", L"", L""},
    {"letter_m_tail0", L"M", L""},
    {"letter_m_tail1", L"M", L"M"},
    {"no_eliding", L"012ab", L"012ab"},
    {"ltr_3", L"abc", L"abc"},
    {"ltr_2", L"abc", L"ab"},
    {"ltr_1", L"abc", L"a"},
    {"ltr_0", L"abc", L""},
    {"rtl_3", L"\u05d0\u05d1\u05d2", L"\u05d0\u05d1\u05d2"},
    {"rtl_2", L"\u05d0\u05d1\u05d2", L"\u05d0\u05d1"},
    {"rtl_1", L"\u05d0\u05d1\u05d2", L"\u05d0"},
    {"rtl_0", L"\u05d0\u05d1\u05d2", L""},
    {"ltr_rtl_5", L"abc\u05d0\u05d1\u05d2", L"abc\u05d0\u05d1"},
    {"ltr_rtl_4", L"abc\u05d0\u05d1\u05d2", L"abc\u05d0"},
    {"ltr_rtl_3", L"abc\u05d0\u05d1\u05d2", L"abc"},
    {"ltr_rtl_2", L"abc\u05d0\u05d1\u05d2", L"ab"},
    {"rtl_ltr_5", L"\u05d0\u05d1\u05d2abc", L"\u05d0\u05d1\u05d2ab"},
    {"rtl_ltr_4", L"\u05d0\u05d1\u05d2abc", L"\u05d0\u05d1\u05d2a"},
    {"rtl_ltr_3", L"\u05d0\u05d1\u05d2abc", L"\u05d0\u05d1\u05d2"},
    {"rtl_ltr_2", L"\u05d0\u05d1\u05d2abc", L"\u05d0\u05d1"},
    {"bidi_1", L"012a\u05d1b\u05d1c", L"012a\u05d1b\u05d1"},
    {"bidi_2", L"012a\u05d1b\u05d1c", L"012a\u05d1b"},
    {"bidi_3", L"012a\u05d1b\u05d1c", L"012a\u05d1"},
    {"bidi_4", L"012a\u05d1b\u05d1c", L"012a\u05d1"},
    // Test surrogate pairs. The first pair 𝄞 'MUSICAL SYMBOL G CLEF' U+1D11E
    // should be kept, and the second pair 𝄢 'MUSICAL SYMBOL F CLEF' U+1D122
    // should be removed. No surrogate pair should be partially elided.
    {"surrogate1", L"0123\U0001D11E\U0001D122x", L"0123\U0001D11E\U0001D122"},
    {"surrogate2", L"0123\U0001D11E\U0001D122x", L"0123\U0001D11E"},
    {"surrogate3", L"0123\U0001D11E\U0001D122x", L"0123"},
    // Test combining character sequences. U+0915 U+093F forms a compound
    // glyph, as does U+0915 U+0942. The first should be kept; the second
    // removed. No combining sequence should be partially elided.
    {"combining", L"0123\u0915\u093f\u0915\u0942456", L"0123\u0915\u093f"},
    // 𝄞 (U+1D11E, MUSICAL SYMBOL G CLEF) should be fully elided.
    {"emoji1", L"012\U0001D11Ex", L"012\U0001D11E"},
    {"emoji2", L"012\U0001D11Ex", L"012"},

    // Whitespace elision tests.
    {"empty_no_elision", L"", L"", 0, kForceNoWhitespaceElision},
    {"empty_elision", L"", L"", 0, kForceWhitespaceElision},
    {"xyz_no_elision", L"  x  xyz", L"  x  ", 5, kForceNoWhitespaceElision},
    {"xyz_elision", L"  x  xyz", L"  x", 5, kForceWhitespaceElision},
    {"ltr_rtl_elision3", L"x  \u05d1  y    \u05d2", L"x", 3,
     kForceWhitespaceElision},
    {"ltr_rtl_elision4", L"x  \u05d1  y    \u05d2", L"x  \u05d1", 4,
     kForceWhitespaceElision},
    {"ltr_rtl_elision5", L"x  \u05d1  y    \u05d2", L"x  \u05d1", 5,
     kForceWhitespaceElision},
    {"ltr_rtl_elision9", L"x  \u05d1  y    \u05d2", L"x  \u05d1  y", 9,
     kForceWhitespaceElision},
    // Emoji U+1F601 and emoji U+1F321 U+FE0E are graphemes that result in
    // one glyph each. Eliding a glyph must remove the whole grapheme. It is
    // invalid to break a grapheme in pieces.
    {"graphemes_elision2", L"  \U0001F601  \U0001F321\uFE0E  ", L"", 2,
     kForceWhitespaceElision},
    {"graphemes_elision3", L"  \U0001F601  \U0001F321\uFE0E  ", L"  \U0001F601",
     3, kForceWhitespaceElision},
    {"graphemes_elision5", L"  \U0001F601  \U0001F321\uFE0E  ", L"  \U0001F601",
     5, kForceWhitespaceElision},
    {"graphemes_elision6", L"  \U0001F601  \U0001F321\uFE0E  ",
     L"  \U0001F601  \U0001F321\uFE0E", 6, kForceWhitespaceElision},
    {"graphemes_elision7", L"  \U0001F601  \U0001F321\uFE0E  ",
     L"  \U0001F601  \U0001F321\uFE0E", 7, kForceWhitespaceElision},
};

INSTANTIATE_TEST_SUITE_P(
    ElideTruncate,
    RenderTextTestWithElideTextCase,
    testing::Combine(testing::Values(ElideTextTestOptions{TRUNCATE}),
                     testing::ValuesIn(kElideTruncateTextCases)),
    RenderTextTestWithElideTextCase::ParamInfoToString);

const ElideTextCase kElideEmailTextCases[] = {
    // Invalid email text.
    {"empty", L"", L""},
    {"invalid_char1", L"x", L""},
    {"invalid_char3", L"xyz", L"x\u2026"},
    {"invalid_amp", L"@", L""},
    {"invalid_no_prefix0", L"@y", L""},
    {"invalid_no_prefix1", L"@y", L"\u2026"},
    {"invalid_no_prefix2", L"@xyz", L"@x\u2026"},
    {"invalid_no_suffix0", L"x@", L""},
    {"invalid_no_suffix1", L"x@", L"\u2026"},
    {"invalid_no_suffix2", L"xyz@", L"x\u2026@"},

    {"at1", L"@", L"@"},
    {"at2", L"@@", L"\u2026", 1},
    {"at3", L"@@@", L"\u2026", 2},
    {"at4", L"@@@@", L"@\u2026@", 3},

    {"small1", L"a@b", L"\u2026", 1},
    {"small2", L"a@b", L"\u2026", 2},
    {"small3", L"a@b", L"a@b", 3},
    {"small_username3", L"xyz@b", L"\u2026", 3},
    {"small_username4", L"xyz@b", L"x\u2026@b", 4},
    {"small_username5", L"xyz@b", L"xyz@b", 5},
    {"small_domain3", L"a@xyz", L"\u2026", 3},
    {"small_domain4", L"a@xyz", L"a@x\u2026", 4},
    {"small_domain5", L"a@xyz", L"a@xyz", 5},

    // Valid email.
    {"email_small", L"a@b.com", L"\u2026"},
    {"email_nobody3", L"nobody@gmail.com", L"\u2026", 3},
    {"email_nobody4", L"nobody@gmail.com", L"\u2026", 4},
    {"email_nobody5", L"nobody@gmail.com", L"n\u2026@g\u2026", 5},
    {"email_nobody6", L"nobody@gmail.com", L"no\u2026@g\u2026", 6},
    {"email_nobody7", L"nobody@gmail.com", L"no\u2026@g\u2026m", 7},
    {"email_nobody8", L"nobody@gmail.com", L"nob\u2026@g\u2026m", 8},
    {"email_nobody9", L"nobody@gmail.com", L"nob\u2026@gm\u2026m", 9},
    {"email_nobody10", L"nobody@gmail.com", L"nobo\u2026@gm\u2026m", 10},
    {"email_root", L"root@localhost", L"r\u2026@l\u2026", 5},
    {"email_myself", L"myself@127.0.0.1", L"my\u2026@1\u2026", 6},
};

INSTANTIATE_TEST_SUITE_P(
    ElideEmail,
    RenderTextTestWithElideTextCase,
    testing::Combine(testing::Values(ElideTextTestOptions{ELIDE_EMAIL}),
                     testing::ValuesIn(kElideEmailTextCases)),
    RenderTextTestWithElideTextCase::ParamInfoToString);

TEST_F(RenderTextTest, ElidedText_NoTrimWhitespace) {
  // This test requires glyphs to be the same width.
  constexpr int kGlyphWidth = 10;
  SetGlyphWidth(kGlyphWidth);

  RenderText* render_text = GetRenderText();
  gfx::test::RenderTextTestApi render_text_test_api(render_text);
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetWhitespaceElision(false);

  // Pick a sufficiently long string that's mostly whitespace.
  // Tail-eliding this with whitespace elision turned off should look like:
  // [       ...]
  // and not like:
  // [...       ]
  constexpr char kInputString[] = "                     foo";
  const base::string16 input = ASCIIToUTF16(kInputString);
  render_text->SetText(input);

  // Choose a width based on being able to display 12 characters (one of which
  // will be the trailing ellipsis).
  constexpr int kDesiredChars = 12;
  constexpr int kRequiredWidth = (kDesiredChars + 0.5f) * kGlyphWidth;
  render_text->SetDisplayRect(Rect(0, 0, kRequiredWidth, 100));

  // Verify this doesn't change the full text.
  EXPECT_EQ(input, render_text->text());

  // Verify that the string is truncated to |kDesiredChars| with the ellipsis.
  const base::string16 result = render_text->GetDisplayText();
  const base::string16 expected =
      input.substr(0, kDesiredChars - 1) + kEllipsisUTF16[0];
  EXPECT_EQ(expected, result);
}

TEST_F(RenderTextTest, SetElideBehavior) {
  // This test requires glyphs to be the same width.
  constexpr int kGlyphWidth = 10;
  SetGlyphWidth(kGlyphWidth);

  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdef"));
  render_text->SetCursorEnabled(false);
  render_text->SetDisplayRect(Rect(0, 0, 3 * kGlyphWidth, 100));
  render_text->SetElideBehavior(ELIDE_TAIL);
  EXPECT_EQ(WideToUTF16(L"ab\u2026"), render_text->GetDisplayText());

  // Setting a different eliding behavior must trigger a relayout.
  render_text->SetElideBehavior(ELIDE_HEAD);
  EXPECT_EQ(WideToUTF16(L"\u2026ef"), render_text->GetDisplayText());
}

TEST_F(RenderTextTest, SetWhitespaceElision) {
  // This test requires glyphs to be the same width.
  constexpr int kGlyphWidth = 10;
  SetGlyphWidth(kGlyphWidth);

  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("a b c d"));
  render_text->SetCursorEnabled(false);
  render_text->SetDisplayRect(Rect(0, 0, 3 * kGlyphWidth, 100));
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetWhitespaceElision(false);
  EXPECT_EQ(WideToUTF16(L"a \u2026"), render_text->GetDisplayText());

  // Setting a different whitespace elision must trigger a relayout.
  render_text->SetWhitespaceElision(true);
  EXPECT_EQ(WideToUTF16(L"a\u2026"), render_text->GetDisplayText());
}

TEST_F(RenderTextTest, ElidedObscuredText) {
  auto expected_render_text = std::make_unique<RenderTextHarfBuzz>();
  expected_render_text->SetDisplayRect(Rect(0, 0, 9999, 100));
  const base::char16 elided_obscured_text[] = {
      RenderText::kPasswordReplacementChar,
      RenderText::kPasswordReplacementChar, kEllipsisUTF16[0], 0};
  expected_render_text->SetText(elided_obscured_text);

  RenderText* render_text = GetRenderText();
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetDisplayRect(
      Rect(0, 0, expected_render_text->GetContentWidth(), 100));
  render_text->SetObscured(true);
  render_text->SetText(ASCIIToUTF16("abcdef"));
  EXPECT_EQ(ASCIIToUTF16("abcdef"), render_text->text());
  EXPECT_EQ(elided_obscured_text, render_text->GetDisplayText());
}

TEST_F(RenderTextTest, MultilineElide) {
  RenderText* render_text = GetRenderText();
  base::string16 input_text;
  // Aim for 3 lines of text.
  for (int i = 0; i < 20; ++i)
    input_text.append(ASCIIToUTF16("hello world "));
  render_text->SetText(input_text);
  // Apply a style that tweaks the layout to make sure elision is calculated
  // with these styles. This can expose a behavior in layout where text is
  // slightly different width. This must be done after |SetText()|.
  render_text->ApplyWeight(Font::Weight::BOLD, Range(1, 20));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, Range(1, 20));
  render_text->SetMultiline(true);
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetMaxLines(3);
  const Size size = render_text->GetStringSize();
  // Fit in 3 lines. (If we knew the width of a word, we could
  // anticipate word wrap better.)
  render_text->SetDisplayRect(Rect((size.width() + 96) / 3, 0));
  // Trigger rendering.
  render_text->GetStringSize();
  EXPECT_EQ(input_text, render_text->GetDisplayText());

  base::string16 actual_text;
  // Try widening the space gradually, one pixel at a time, trying
  // to trigger a failure in layout. There was an issue where, right at
  // the edge of a word getting truncated, the estimate would be wrong
  // and it would wrap instead.
  for (int i = (size.width() - 12) / 3; i < (size.width() + 30) / 3; ++i) {
    render_text->SetDisplayRect(Rect(i, 0));
    // Trigger rendering.
    render_text->GetStringSize();
    actual_text = render_text->GetDisplayText();
    EXPECT_LT(actual_text.size(), input_text.size());
    EXPECT_EQ(actual_text, input_text.substr(0, actual_text.size() - 1) +
                               base::string16(kEllipsisUTF16));
    EXPECT_EQ(3U, render_text->GetNumLines());
  }
  // Now remove line restriction.
  render_text->SetMaxLines(0);
  render_text->GetStringSize();
  EXPECT_EQ(input_text, render_text->GetDisplayText());

  // And put it back.
  render_text->SetMaxLines(3);
  render_text->GetStringSize();
  EXPECT_LT(actual_text.size(), input_text.size());
  EXPECT_EQ(actual_text, input_text.substr(0, actual_text.size() - 1) +
                             base::string16(kEllipsisUTF16));
}

TEST_F(RenderTextTest, MultilineElideWrap) {
  RenderText* render_text = GetRenderText();
  base::string16 input_text;
  for (int i = 0; i < 20; ++i)
    input_text.append(ASCIIToUTF16("hello world "));
  render_text->SetText(input_text);
  render_text->SetMultiline(true);
  render_text->SetMaxLines(3);
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetDisplayRect(Rect(30, 0));

  // ELIDE_LONG_WORDS doesn't make sense in multiline, and triggers assertion
  // failure.
  const WordWrapBehavior wrap_behaviors[] = {
      IGNORE_LONG_WORDS, TRUNCATE_LONG_WORDS, WRAP_LONG_WORDS};
  for (auto wrap_behavior : wrap_behaviors) {
    render_text->SetWordWrapBehavior(wrap_behavior);
    render_text->GetStringSize();
    base::string16 actual_text = render_text->GetDisplayText();
    EXPECT_LE(actual_text.size(), input_text.size());
    EXPECT_EQ(actual_text, input_text.substr(0, actual_text.size() - 1) +
                               base::string16(kEllipsisUTF16));
    EXPECT_LE(render_text->GetNumLines(), 3U);
  }
}

// TODO(crbug.com/866720): The current implementation of eliding is not aware
// of text styles. The elide text algorithm doesn't take into account the style
// properties when eliding the text. This lead to incorrect text size when the
// styles are applied.
TEST_F(RenderTextTest, DISABLED_MultilineElideWrapWithStyle) {
  RenderText* render_text = GetRenderText();
  base::string16 input_text;
  for (int i = 0; i < 20; ++i)
    input_text.append(ASCIIToUTF16("hello world "));
  render_text->SetText(input_text);
  render_text->ApplyWeight(Font::Weight::BOLD, Range(1, 20));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, Range(1, 20));
  render_text->SetMultiline(true);
  render_text->SetMaxLines(3);
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetDisplayRect(Rect(30, 0));

  // ELIDE_LONG_WORDS doesn't make sense in multiline, and triggers assertion
  // failure.
  const WordWrapBehavior wrap_behaviors[] = {
      IGNORE_LONG_WORDS, TRUNCATE_LONG_WORDS, WRAP_LONG_WORDS};
  for (auto wrap_behavior : wrap_behaviors) {
    render_text->SetWordWrapBehavior(wrap_behavior);
    render_text->GetStringSize();
    base::string16 actual_text = render_text->GetDisplayText();
    EXPECT_LE(actual_text.size(), input_text.size());
    EXPECT_EQ(actual_text, input_text.substr(0, actual_text.size() - 1) +
                               base::string16(kEllipsisUTF16));
    EXPECT_LE(render_text->GetNumLines(), 3U);
  }
}

TEST_F(RenderTextTest, MultilineElideWrapStress) {
  RenderText* render_text = GetRenderText();
  base::string16 input_text;
  for (int i = 0; i < 20; ++i)
    input_text.append(ASCIIToUTF16("hello world "));
  render_text->SetText(input_text);
  render_text->SetMultiline(true);
  render_text->SetMaxLines(3);
  render_text->SetElideBehavior(ELIDE_TAIL);

  // ELIDE_LONG_WORDS doesn't make sense in multiline, and triggers assertion
  // failure.
  const WordWrapBehavior wrap_behaviors[] = {
      IGNORE_LONG_WORDS, TRUNCATE_LONG_WORDS, WRAP_LONG_WORDS};
  for (auto wrap_behavior : wrap_behaviors) {
    for (int i = 1; i < 60; ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "MultilineElideWrapStress wrap_behavior = %d, width = %d",
          wrap_behavior, i));

      render_text->SetDisplayRect(Rect(i, 0));
      render_text->SetWordWrapBehavior(wrap_behavior);
      render_text->GetStringSize();
      base::string16 actual_text = render_text->GetDisplayText();
      EXPECT_LE(actual_text.size(), input_text.size());
      EXPECT_LE(render_text->GetNumLines(), 3U);
    }
  }
}

// TODO(crbug.com/866720): The current implementation of eliding is not aware
// of text styles. The elide text algorithm doesn't take into account the style
// properties when eliding the text. This lead to incorrect text size when the
// styles are applied.
TEST_F(RenderTextTest, DISABLED_MultilineElideWrapStressWithStyle) {
  RenderText* render_text = GetRenderText();
  base::string16 input_text;
  for (int i = 0; i < 20; ++i)
    input_text.append(ASCIIToUTF16("hello world "));
  render_text->SetText(input_text);
  render_text->ApplyWeight(Font::Weight::BOLD, Range(1, 20));
  render_text->SetMultiline(true);
  render_text->SetMaxLines(3);
  render_text->SetElideBehavior(ELIDE_TAIL);

  // ELIDE_LONG_WORDS doesn't make sense in multiline, and triggers assertion
  // failure.
  const WordWrapBehavior wrap_behaviors[] = {
      IGNORE_LONG_WORDS, TRUNCATE_LONG_WORDS, WRAP_LONG_WORDS};
  for (auto wrap_behavior : wrap_behaviors) {
    for (int i = 1; i < 60; ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "MultilineElideWrapStress wrap_behavior = %d, width = %d",
          wrap_behavior, i));

      render_text->SetDisplayRect(Rect(i, 0));
      render_text->SetWordWrapBehavior(wrap_behavior);
      render_text->GetStringSize();
      base::string16 actual_text = render_text->GetDisplayText();
      EXPECT_LE(actual_text.size(), input_text.size());
      EXPECT_LE(render_text->GetNumLines(), 3U);
    }
  }
}

TEST_F(RenderTextTest, MultilineElideRTL) {
  RenderText* render_text = GetRenderText();
  SetGlyphWidth(5);

  base::string16 input_text(UTF8ToUTF16("זהו המסר של ההודעה"));
  render_text->SetText(input_text);
  render_text->SetCursorEnabled(false);
  render_text->SetMultiline(true);
  render_text->SetMaxLines(1);
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetDisplayRect(Rect(45, 0));
  render_text->GetStringSize();

  EXPECT_EQ(render_text->GetDisplayText(),
            input_text.substr(0, 8) + base::string16(kEllipsisUTF16));
  EXPECT_EQ(render_text->GetNumLines(), 1U);
}

TEST_F(RenderTextTest, MultilineElideBiDi) {
  RenderText* render_text = GetRenderText();
  SetGlyphWidth(5);

  base::string16 input_text(UTF8ToUTF16("אa\nbcdבגדהefg\nhו"));
  render_text->SetText(input_text);
  render_text->SetCursorEnabled(false);
  render_text->SetMultiline(true);
  render_text->SetMaxLines(2);
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetDisplayRect(Rect(30, 0));
  test_api()->EnsureLayout();

  EXPECT_EQ(render_text->GetDisplayText(),
            UTF8ToUTF16("אa\nbcdבג") + base::string16(kEllipsisUTF16));
  EXPECT_EQ(render_text->GetNumLines(), 2U);
}

TEST_F(RenderTextTest, MultilineElideLinebreak) {
  RenderText* render_text = GetRenderText();
  SetGlyphWidth(5);

  base::string16 input_text(ASCIIToUTF16("hello\nworld"));
  render_text->SetText(input_text);
  render_text->SetCursorEnabled(false);
  render_text->SetMultiline(true);
  render_text->SetMaxLines(1);
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetDisplayRect(Rect(100, 0));
  render_text->GetStringSize();

  EXPECT_EQ(render_text->GetDisplayText(),
            input_text.substr(0, 5) + base::string16(kEllipsisUTF16));
  EXPECT_EQ(render_text->GetNumLines(), 1U);
}

TEST_F(RenderTextTest, ElidedStyledTextRtl) {
  static const char* kInputTexts[] = {
      "http://ar.wikipedia.com/فحص",
      "testحص,",
      "حص,test",
      "…",
      "…test",
      "test…",
      "حص,test…",
      "ٱ",
      "\uFEFF",  // BOM: Byte Order Marker
      "…\u200F",  // Right to left marker.
  };

  for (const auto* raw_text : kInputTexts) {
    SCOPED_TRACE(
        base::StringPrintf("ElidedStyledTextRtl text = %s", raw_text));
    base::string16 input_text(UTF8ToUTF16(raw_text));

    RenderText* render_text = GetRenderText();
    render_text->SetText(input_text);
    render_text->SetElideBehavior(ELIDE_TAIL);
    render_text->SetStyle(TEXT_STYLE_STRIKE, true);
    render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_LTR);

    constexpr int kMaxContentWidth = 2000;
    for (int i = 0; i < kMaxContentWidth; ++i) {
      SCOPED_TRACE(base::StringPrintf("ElidedStyledTextRtl width = %d", i));
      render_text->SetDisplayRect(Rect(i, 20));
      render_text->GetStringSize();
      base::string16 display_text = render_text->GetDisplayText();
      EXPECT_LE(display_text.size(), input_text.size());

      // Every size of content width was tried.
      if (display_text == input_text)
        break;
    }
  }
}

TEST_F(RenderTextTest, ElidedEmail) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("test@example.com"));
  const Size size = render_text->GetStringSize();

  const base::string16 long_email =
      ASCIIToUTF16("longemailaddresstest@example.com");
  render_text->SetText(long_email);
  render_text->SetElideBehavior(ELIDE_EMAIL);
  render_text->SetDisplayRect(Rect(size));
  EXPECT_GE(size.width(), render_text->GetStringSize().width());
  EXPECT_GT(long_email.size(), render_text->GetDisplayText().size());
}

TEST_F(RenderTextTest, TruncatedText) {
  struct {
    const wchar_t* text;
    const wchar_t* display_text;
  } cases[] = {
      // Strings shorter than the truncation length should be laid out in full.
      {L"", L""},
      {L" . ", L" . "},                                // a wide kWeak
      {L"abc", L"abc"},                                // a wide kLtr
      {L"\u05d0\u05d1\u05d2", L"\u05d0\u05d1\u05d2"},  // a wide kRtl
      {L"a\u05d0\u05d1", L"a\u05d0\u05d1"},            // a wide kLtrRtl
      {L"a\u05d1b", L"a\u05d1b"},                      // a wide kLtrRtlLtr
      {L"\u05d0\u05d1a", L"\u05d0\u05d1a"},            // a wide kRtlLtr
      {L"\u05d0a\u05d1", L"\u05d0a\u05d1"},            // a wide kRtlLtrRtl
      {L"01234", L"01234"},
      // Long strings should be truncated with an ellipsis appended at the end.
      {L"012345", L"0123\u2026"},
      {L"012 . ", L"012 \u2026"},
      {L"012abc", L"012a\u2026"},
      {L"012a\u05d0\u05d1", L"012a\u2026"},
      {L"012a\u05d1b", L"012a\u2026"},
      {L"012\u05d0\u05d1\u05d2", L"012\u05d0\u2026"},
      {L"012\u05d0\u05d1a", L"012\u05d0\u2026"},
      {L"012\u05d0a\u05d1", L"012\u05d0\u2026"},
      // Surrogate pairs should be truncated reasonably enough.
      {L"0123\u0915\u093f", L"0123\u2026"},
      {L"\u05e9\u05bc\u05c1\u05b8", L"\u05e9\u05bc\u05c1\u05b8"},
      {L"0\u05e9\u05bc\u05c1\u05b8", L"0\u05e9\u05bc\u05c1\u05b8"},
      {L"01\u05e9\u05bc\u05c1\u05b8", L"01\u2026"},
      {L"012\u05e9\u05bc\u05c1\u05b8", L"012\u2026"},
      // Codepoint U+0001D11E is using 2x 16-bit characters.
      {L"0\U0001D11Eaaa", L"0\U0001D11Ea\u2026"},
      {L"01\U0001D11Eaaa", L"01\U0001D11E\u2026"},
      {L"012\U0001D11Eaaa", L"012\u2026"},
      {L"0123\U0001D11Eaaa", L"0123\u2026"},
      {L"01234\U0001D11Eaaa", L"0123\u2026"},
      // Combining codepoint should stay together.
      // (Letter 'e' U+0065 and acute accent U+0301).
      {L"0e\u0301aaa", L"0e\u0301a\u2026"},
      {L"01e\u0301aaa", L"01e\u0301\u2026"},
      {L"012e\u0301aaa", L"012\u2026"},
      // Emoji 'keycap letter 6'.
      {L"\u0036\uFE0F\u20E3aaa", L"\u0036\uFE0F\u20E3a\u2026"},
      {L"0\u0036\uFE0F\u20E3aaa", L"0\u0036\uFE0F\u20E3\u2026"},
      {L"01\u0036\uFE0F\u20E3aaa", L"01\u2026"},
      // Emoji 'pilot'.
      {L"\U0001F468\u200D\u2708\uFE0F", L"\U0001F468\u200D\u2708\uFE0F"},
      {L"\U0001F468\u200D\u2708\uFE0F0", L"\u2026"},
      {L"0\U0001F468\u200D\u2708\uFE0F", L"0\u2026"},
  };

  RenderText* render_text = GetRenderText();
  render_text->set_truncate_length(5);
  for (size_t i = 0; i < base::size(cases); i++) {
    render_text->SetText(WideToUTF16(cases[i].text));
    EXPECT_EQ(WideToUTF16(cases[i].text), render_text->text());
    EXPECT_EQ(WideToUTF16(cases[i].display_text), render_text->GetDisplayText())
        << "For case " << i << ": " << cases[i].text;
  }
}

TEST_F(RenderTextTest, TruncatedObscuredText) {
  RenderText* render_text = GetRenderText();
  render_text->set_truncate_length(3);
  render_text->SetObscured(true);
  render_text->SetText(ASCIIToUTF16("abcdef"));
  EXPECT_EQ(ASCIIToUTF16("abcdef"), render_text->text());
  EXPECT_EQ(GetObscuredString(3, 2, kEllipsisUTF16[0]),
            render_text->GetDisplayText());
}

TEST_F(RenderTextTest, TruncatedObscuredTextWithGraphemes) {
  RenderText* render_text = GetRenderText();
  render_text->set_truncate_length(3);
  render_text->SetText(
      WideToUTF16(L"e\u0301\U0001F468\u200D\u2708\uFE0F\U0001D11E"));
  render_text->SetObscured(true);
  EXPECT_EQ(GetObscuredString(3), render_text->GetDisplayText());

  render_text->SetObscuredRevealIndex(0);
  EXPECT_EQ(UTF8ToUTF16("e\u0301\u2026"), render_text->GetDisplayText());

  render_text->SetObscuredRevealIndex(2);
  EXPECT_EQ(UTF8ToUTF16("\u2022\u2026"), render_text->GetDisplayText());

  render_text->SetObscuredRevealIndex(7);
  EXPECT_EQ(UTF8ToUTF16("\u2022\u2022\u2026"), render_text->GetDisplayText());
}

TEST_F(RenderTextTest, TruncatedCursorMovementLTR) {
  RenderText* render_text = GetRenderText();
  render_text->set_truncate_length(2);
  render_text->SetText(ASCIIToUTF16("abcd"));

  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(SelectionModel(4, CURSOR_FORWARD), render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());

  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  // The cursor hops over the ellipsis and elided text to the line end.
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  // The cursor hops over the elided text to preceeding text.
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, TruncatedCursorMovementRTL) {
  RenderText* render_text = GetRenderText();
  render_text->set_truncate_length(2);
  render_text->SetText(UTF8ToUTF16("\u05d0\u05d1\u05d2\u05d3"));

  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(SelectionModel(4, CURSOR_FORWARD), render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());

  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  // The cursor hops over the ellipsis and elided text to the line end.
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_LEFT);

  expected.clear();
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  // The cursor hops over the elided text to preceeding text.
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_RIGHT);
}

TEST_F(RenderTextTest, MoveCursor_Character) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("123 456 789"));
  std::vector<Range> expected;

  // SELECTION_NONE.
  render_text->SelectRange(Range(6));

  // Move right twice.
  expected.push_back(Range(7));
  expected.push_back(Range(8));
  RunMoveCursorTestAndClearExpectations(
      render_text, CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE, &expected);

  // Move left twice.
  expected.push_back(Range(7));
  expected.push_back(Range(6));
  RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK,
                                        CURSOR_LEFT, SELECTION_NONE, &expected);

  // SELECTION_CARET.
  render_text->SelectRange(Range(6));
  expected.push_back(Range(6, 7));

  // Move right.
  RunMoveCursorTestAndClearExpectations(
      render_text, CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_CARET, &expected);

  // Move left twice.
  expected.push_back(Range(6));
  expected.push_back(Range(6, 5));
  RunMoveCursorTestAndClearExpectations(
      render_text, CHARACTER_BREAK, CURSOR_LEFT, SELECTION_CARET, &expected);

  // SELECTION_RETAIN.
  render_text->SelectRange(Range(6));

  // Move right.
  expected.push_back(Range(6, 7));
  RunMoveCursorTestAndClearExpectations(
      render_text, CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_RETAIN, &expected);

  // Move left twice.
  expected.push_back(Range(6));
  expected.push_back(Range(6, 5));
  RunMoveCursorTestAndClearExpectations(
      render_text, CHARACTER_BREAK, CURSOR_LEFT, SELECTION_RETAIN, &expected);

  // SELECTION_EXTEND.
  render_text->SelectRange(Range(6));

  // Move right.
  expected.push_back(Range(6, 7));
  RunMoveCursorTestAndClearExpectations(
      render_text, CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_EXTEND, &expected);

  // Move left twice.
  expected.push_back(Range(7, 6));
  expected.push_back(Range(7, 5));
  RunMoveCursorTestAndClearExpectations(
      render_text, CHARACTER_BREAK, CURSOR_LEFT, SELECTION_EXTEND, &expected);
}

TEST_F(RenderTextTest, MoveCursor_Word) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("123 456 789"));
  std::vector<Range> expected;

  // SELECTION_NONE.
  render_text->SelectRange(Range(6));

  // Move left twice.
  expected.push_back(Range(4));
  expected.push_back(Range(0));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_NONE, &expected);

  // Move right twice.
#if defined(OS_WIN)  // Move word right includes space/punctuation.
  expected.push_back(Range(4));
  expected.push_back(Range(8));
#else  // Non-Windows: move word right does NOT include space/punctuation.
  expected.push_back(Range(3));
  expected.push_back(Range(7));
#endif
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_NONE, &expected);

  // SELECTION_CARET.
  render_text->SelectRange(Range(6));

  // Move left.
  expected.push_back(Range(6, 4));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_CARET, &expected);

  // Move right twice.
  expected.push_back(Range(6));
#if defined(OS_WIN)  // Select word right includes space/punctuation.
  expected.push_back(Range(6, 8));
#else  // Non-Windows: select word right does NOT include space/punctuation.
  expected.push_back(Range(6, 7));
#endif
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_CARET, &expected);

  // Move left.
  expected.push_back(Range(6));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_CARET, &expected);

  // SELECTION_RETAIN.
  render_text->SelectRange(Range(6));

  // Move left.
  expected.push_back(Range(6, 4));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_RETAIN, &expected);

  // Move right twice.
#if defined(OS_WIN)  // Select word right includes space/punctuation.
  expected.push_back(Range(6, 8));
#else  // Non-Windows: select word right does NOT include space/punctuation.
  expected.push_back(Range(6, 7));
#endif
  expected.push_back(Range(6, 11));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_RETAIN, &expected);

  // Move left.
  expected.push_back(Range(6, 8));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_RETAIN, &expected);

  // SELECTION_EXTEND.
  render_text->SelectRange(Range(6));

  // Move left.
  expected.push_back(Range(6, 4));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_EXTEND, &expected);

  // Move right twice.
#if defined(OS_WIN)  // Select word right includes space/punctuation.
  expected.push_back(Range(4, 8));
#else  // Non-Windows: select word right does NOT include space/punctuation.
  expected.push_back(Range(4, 7));
#endif
  expected.push_back(Range(4, 11));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_EXTEND, &expected);

  // Move left.
  expected.push_back(Range(4, 8));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_EXTEND, &expected);
}

TEST_F(RenderTextTest, MoveCursor_Word_RTL) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("אבג דהו זחט"));
  std::vector<Range> expected;

  // SELECTION_NONE.
  render_text->SelectRange(Range(6));

  // Move right twice.
  expected.push_back(Range(4));
  expected.push_back(Range(0));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_NONE, &expected);

  // Move left twice.
#if defined(OS_WIN)  // Move word left includes space/punctuation.
  expected.push_back(Range(4));
  expected.push_back(Range(8));
#else  // Non-Windows: move word left does NOT include space/punctuation.
  expected.push_back(Range(3));
  expected.push_back(Range(7));
#endif
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_NONE, &expected);

  // SELECTION_CARET.
  render_text->SelectRange(Range(6));

  // Move right.
  expected.push_back(Range(6, 4));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_CARET, &expected);

  // Move left twice.
  expected.push_back(Range(6));
#if defined(OS_WIN)  // Select word left includes space/punctuation.
  expected.push_back(Range(6, 8));
#else  // Non-Windows: select word left does NOT include space/punctuation.
  expected.push_back(Range(6, 7));
#endif
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_CARET, &expected);

  // Move right.
  expected.push_back(Range(6));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_CARET, &expected);

  // SELECTION_RETAIN.
  render_text->SelectRange(Range(6));

  // Move right.
  expected.push_back(Range(6, 4));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_RETAIN, &expected);

  // Move left twice.
#if defined(OS_WIN)  // Select word left includes space/punctuation.
  expected.push_back(Range(6, 8));
#else  // Non-Windows: select word left does NOT include space/punctuation.
  expected.push_back(Range(6, 7));
#endif
  expected.push_back(Range(6, 11));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_RETAIN, &expected);

  // Move right.
  expected.push_back(Range(6, 8));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_RETAIN, &expected);

  // SELECTION_EXTEND.
  render_text->SelectRange(Range(6));

  // Move right.
  expected.push_back(Range(6, 4));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_EXTEND, &expected);

  // Move left twice.
#if defined(OS_WIN)  // Select word left includes space/punctuation.
  expected.push_back(Range(4, 8));
#else  // Non-Windows: select word left does NOT include space/punctuation.
  expected.push_back(Range(4, 7));
#endif
  expected.push_back(Range(4, 11));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_LEFT,
                                        SELECTION_EXTEND, &expected);

  // Move right.
  expected.push_back(Range(4, 8));
  RunMoveCursorTestAndClearExpectations(render_text, WORD_BREAK, CURSOR_RIGHT,
                                        SELECTION_EXTEND, &expected);
}

TEST_F(RenderTextTest, MoveCursor_Line) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("123 456 789"));
  std::vector<Range> expected;

  for (auto break_type : {LINE_BREAK, FIELD_BREAK}) {
    // SELECTION_NONE.
    render_text->SelectRange(Range(6));

    // Move right twice.
    expected.push_back(Range(11));
    expected.push_back(Range(11));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_RIGHT,
                                          SELECTION_NONE, &expected);

    // Move left twice.
    expected.push_back(Range(0));
    expected.push_back(Range(0));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_LEFT,
                                          SELECTION_NONE, &expected);

    // SELECTION_CARET.
    render_text->SelectRange(Range(6));

    // Move right.
    expected.push_back(Range(6, 11));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_RIGHT,
                                          SELECTION_CARET, &expected);

    // Move left twice.
    expected.push_back(Range(6));
    expected.push_back(Range(6, 0));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_LEFT,
                                          SELECTION_CARET, &expected);

    // Move right.
    expected.push_back(Range(6));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_RIGHT,
                                          SELECTION_CARET, &expected);

    // SELECTION_RETAIN.
    render_text->SelectRange(Range(6));

    // Move right.
    expected.push_back(Range(6, 11));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_RIGHT,
                                          SELECTION_RETAIN, &expected);

    // Move left twice.
    expected.push_back(Range(6, 0));
    expected.push_back(Range(6, 0));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_LEFT,
                                          SELECTION_RETAIN, &expected);

    // Move right.
    expected.push_back(Range(6, 11));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_RIGHT,
                                          SELECTION_RETAIN, &expected);

    // SELECTION_EXTEND.
    render_text->SelectRange(Range(6));

    // Move right.
    expected.push_back(Range(6, 11));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_RIGHT,
                                          SELECTION_EXTEND, &expected);

    // Move left twice.
    expected.push_back(Range(11, 0));
    expected.push_back(Range(11, 0));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_LEFT,
                                          SELECTION_EXTEND, &expected);

    // Move right.
    expected.push_back(Range(0, 11));
    RunMoveCursorTestAndClearExpectations(render_text, break_type, CURSOR_RIGHT,
                                          SELECTION_EXTEND, &expected);
  }
}

TEST_F(RenderTextTest, MoveCursor_UpDown) {
  SetGlyphWidth(5);
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(45, 1000));
  render_text->SetMultiline(true);

  std::vector<size_t> expected_lines;
  std::vector<Range> expected_range;
  for (auto text :
       {ASCIIToUTF16("123 456 123 456 "), UTF8ToUTF16("אבג דהו זחט זחט ")}) {
    render_text->SetText(text);
    EXPECT_EQ(2U, render_text->GetNumLines());

    // SELECTION_NONE.
    render_text->SelectRange(Range(0));
    ResetCursorX();

    // Move down twice.
    expected_lines.push_back(1);
    expected_lines.push_back(1);
    RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK,
                                          CURSOR_DOWN, SELECTION_NONE,
                                          &expected_lines);

    // Move up twice.
    expected_lines.push_back(0);
    expected_lines.push_back(0);
    RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK,
                                          CURSOR_UP, SELECTION_NONE,
                                          &expected_lines);

    // SELECTION_CARET.
    render_text->SelectRange(Range(0));
    ResetCursorX();

    // Move down twice.
    expected_range.push_back(Range(0, 8));
    expected_range.push_back(Range(0, 16));
    RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK,
                                          CURSOR_DOWN, SELECTION_CARET,
                                          &expected_range);

    // Move up twice.
    expected_range.push_back(Range(0, 8));
    expected_range.push_back(Range(0));
    RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK,
                                          CURSOR_UP, SELECTION_CARET,
                                          &expected_range);
  }
}

TEST_F(RenderTextTest, MoveCursor_UpDown_Newline) {
  SetGlyphWidth(5);
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("123 456\n123 456 "));
  render_text->SetDisplayRect(Rect(100, 1000));
  render_text->SetMultiline(true);
  EXPECT_EQ(2U, render_text->GetNumLines());

  std::vector<size_t> expected_lines;
  std::vector<Range> expected_range;

  // SELECTION_NONE.
  render_text->SelectRange(Range(0));
  ResetCursorX();

  // Move down twice.
  expected_lines.push_back(1);
  expected_lines.push_back(1);
  RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK,
                                        CURSOR_DOWN, SELECTION_NONE,
                                        &expected_lines);

  // Move up twice.
  expected_lines.push_back(0);
  expected_lines.push_back(0);
  RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK, CURSOR_UP,
                                        SELECTION_NONE, &expected_lines);

  // SELECTION_CARET.
  render_text->SelectRange(Range(0));
  ResetCursorX();

  // Move down twice.
  expected_range.push_back(Range(0, 8));
  expected_range.push_back(Range(0, 16));
  RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK,
                                        CURSOR_DOWN, SELECTION_CARET,
                                        &expected_range);

  // Move up twice.
  expected_range.push_back(Range(0, 7));
  expected_range.push_back(Range(0));
  RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK, CURSOR_UP,
                                        SELECTION_CARET, &expected_range);
}

TEST_F(RenderTextTest, MoveCursor_UpDown_Cache) {
  SetGlyphWidth(5);
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("123 456\n\n123 456"));
  render_text->SetDisplayRect(Rect(45, 1000));
  render_text->SetMultiline(true);
  EXPECT_EQ(3U, render_text->GetNumLines());

  std::vector<size_t> expected_lines;
  std::vector<Range> expected_range;

  // SELECTION_NONE.
  render_text->SelectRange(Range(2));
  ResetCursorX();

  // Move down twice.
  expected_range.push_back(Range(8));
  expected_range.push_back(Range(11));
  RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK,
                                        CURSOR_DOWN, SELECTION_NONE,
                                        &expected_range);

  // Move up twice.
  expected_range.push_back(Range(8));
  expected_range.push_back(Range(2));
  RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK, CURSOR_UP,
                                        SELECTION_NONE, &expected_range);

  // Move left.
  expected_range.push_back(Range(1));
  RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK,
                                        CURSOR_LEFT, SELECTION_NONE,
                                        &expected_range);

  // Move down twice again.
  expected_range.push_back(Range(8));
  expected_range.push_back(Range(10));
  RunMoveCursorTestAndClearExpectations(render_text, CHARACTER_BREAK,
                                        CURSOR_DOWN, SELECTION_NONE,
                                        &expected_range);
}

TEST_F(RenderTextTest, MoveCursorWithNewline) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("a\r\nb"));
  render_text->SetMultiline(false);
  EXPECT_EQ(1U, render_text->GetNumLines());

  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(SelectionModel(1, CURSOR_BACKWARD), render_text->selection_model());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(SelectionModel(3, CURSOR_BACKWARD), render_text->selection_model());

  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(SelectionModel(4, CURSOR_FORWARD), render_text->selection_model());
}

TEST_F(RenderTextTest, GetTextDirectionInvalidation) {
  RenderText* render_text = GetRenderText();
  ASSERT_EQ(render_text->directionality_mode(), DIRECTIONALITY_FROM_TEXT);

  const base::i18n::TextDirection original_text_direction =
      render_text->GetTextDirection();

  render_text->SetText(ASCIIToUTF16("a"));
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, render_text->GetTextDirection());

  render_text->SetText(UTF8ToUTF16("\u05d0"));
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, render_text->GetTextDirection());

  // The codepoints u+2026 (ellipsis) has no strong direction.
  render_text->SetText(UTF8ToUTF16("\u2026"));
  EXPECT_EQ(original_text_direction, render_text->GetTextDirection());
  render_text->AppendText(ASCIIToUTF16("a"));
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, render_text->GetTextDirection());

  render_text->SetText(UTF8ToUTF16("\u2026"));
  EXPECT_EQ(original_text_direction, render_text->GetTextDirection());
  render_text->AppendText(UTF8ToUTF16("\u05d0"));
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, render_text->GetTextDirection());
}

TEST_F(RenderTextTest, GetDisplayTextDirectionInvalidation) {
  RenderText* render_text = GetRenderText();
  ASSERT_EQ(render_text->directionality_mode(), DIRECTIONALITY_FROM_TEXT);

  const base::i18n::TextDirection original_text_direction =
      render_text->GetDisplayTextDirection();

  render_text->SetText(ASCIIToUTF16("a"));
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, render_text->GetDisplayTextDirection());

  render_text->SetText(UTF8ToUTF16("\u05d0"));
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, render_text->GetDisplayTextDirection());

  // The codepoints u+2026 (ellipsis) has no strong direction.
  render_text->SetText(UTF8ToUTF16("\u2026"));
  EXPECT_EQ(original_text_direction, render_text->GetDisplayTextDirection());
  render_text->AppendText(ASCIIToUTF16("a"));
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, render_text->GetDisplayTextDirection());

  render_text->SetText(UTF8ToUTF16("\u2026"));
  EXPECT_EQ(original_text_direction, render_text->GetDisplayTextDirection());
  render_text->AppendText(UTF8ToUTF16("\u05d0"));
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, render_text->GetDisplayTextDirection());
}

TEST_F(RenderTextTest, GetTextDirectionWithDifferentDirection) {
  SetGlyphWidth(10);
  RenderText* render_text = GetRenderText();
  ASSERT_EQ(render_text->directionality_mode(), DIRECTIONALITY_FROM_TEXT);
  render_text->SetWhitespaceElision(false);
  render_text->SetText(UTF8ToUTF16("123\u0638xyz"));
  render_text->SetElideBehavior(ELIDE_HEAD);
  render_text->SetDisplayRect(Rect(25, 100));

  // The elided text is an ellipsis with neutral directionality, and a 'z' with
  // a strong LTR directionality.
  EXPECT_EQ(UTF8ToUTF16("\u2026z"), render_text->GetDisplayText());
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, render_text->GetTextDirection());
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, render_text->GetDisplayTextDirection());
}

TEST_F(RenderTextTest, DirectionalityInvalidation) {
  RenderText* render_text = GetRenderText();
  ASSERT_EQ(render_text->directionality_mode(), DIRECTIONALITY_FROM_TEXT);

  // The codepoints u+2026 (ellipsis) has weak directionality.
  render_text->SetText(UTF8ToUTF16("\u2026"));
  const base::i18n::TextDirection original_text_direction =
      render_text->GetTextDirection();

  render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_LTR);
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, render_text->GetTextDirection());
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, render_text->GetDisplayTextDirection());

  render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_RTL);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, render_text->GetTextDirection());
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, render_text->GetDisplayTextDirection());

  render_text->SetDirectionalityMode(DIRECTIONALITY_FROM_TEXT);
  EXPECT_EQ(original_text_direction, render_text->GetTextDirection());
  EXPECT_EQ(original_text_direction, render_text->GetDisplayTextDirection());
}

TEST_F(RenderTextTest, MoveCursor_UpDown_Scroll) {
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(100, 30));
  render_text->SetMultiline(true);
  render_text->SetVerticalAlignment(ALIGN_TOP);

  const size_t kLineSize = 50;
  std::string text;
  for (size_t i = 0; i < kLineSize - 1; ++i)
    text += "a\n";

  render_text->SetText(ASCIIToUTF16(text));
  EXPECT_EQ(kLineSize, render_text->GetNumLines());

  // Move cursor down with scroll.
  render_text->SelectRange(Range(0));
  // |line_height| is the distance from the top.
  float line_height =
      render_text->GetLineSizeF(render_text->selection_model()).height();
  for (size_t i = 1; i < kLineSize; ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing line [%" PRIuS "]", i));
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_DOWN, SELECTION_NONE);
    ASSERT_EQ(Range(i * 2), render_text->selection());
    ASSERT_TRUE(render_text->display_rect().Contains(
        render_text->GetUpdatedCursorBounds()));
    line_height +=
        render_text->GetLineSizeF(render_text->selection_model()).height();
    ASSERT_FLOAT_EQ(test_api()->display_offset().y(),
                    std::min(0.0f, 30.0f - line_height));
  }

  // Move cursor up with scroll.
  // |line_height| is the distance from the bottom.
  line_height =
      render_text->GetLineSizeF(render_text->selection_model()).height();
  int offset_y = test_api()->display_offset().y();
  for (size_t i = kLineSize - 2; i != size_t{-1}; --i) {
    SCOPED_TRACE(base::StringPrintf("Testing line [%" PRIuS "]", i));
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_UP, SELECTION_NONE);
    ASSERT_EQ(Range(i * 2), render_text->selection());
    ASSERT_TRUE(render_text->display_rect().Contains(
        render_text->GetUpdatedCursorBounds()));
    line_height +=
        render_text->GetLineSizeF(render_text->selection_model()).height();
    ASSERT_FLOAT_EQ(test_api()->display_offset().y(),
                    offset_y + std::max(0.0f, line_height - 30.0f));
  }
  EXPECT_EQ(0, test_api()->display_offset().y());
}

TEST_F(RenderTextTest, GetDisplayTextDirection) {
  struct {
    const char* text;
    const base::i18n::TextDirection text_direction;
  } cases[] = {
      // Blank strings and those with no/weak directionality default to LTR.
      {"", base::i18n::LEFT_TO_RIGHT},
      {kWeak, base::i18n::LEFT_TO_RIGHT},
      // Strings that begin with strong LTR characters.
      {kLtr, base::i18n::LEFT_TO_RIGHT},
      {kLtrRtl, base::i18n::LEFT_TO_RIGHT},
      {kLtrRtlLtr, base::i18n::LEFT_TO_RIGHT},
      // Strings that begin with strong RTL characters.
      {kRtl, base::i18n::RIGHT_TO_LEFT},
      {kRtlLtr, base::i18n::RIGHT_TO_LEFT},
      {kRtlLtrRtl, base::i18n::RIGHT_TO_LEFT},
  };

  RenderText* render_text = GetRenderText();
  const bool was_rtl = base::i18n::IsRTL();

  for (size_t i = 0; i < 2; ++i) {
    // Toggle the application default text direction (to try each direction).
    SetRTL(!base::i18n::IsRTL());
    const base::i18n::TextDirection ui_direction = base::i18n::IsRTL() ?
        base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT;

    // Ensure that directionality modes yield the correct text directions.
    for (size_t j = 0; j < base::size(cases); j++) {
      render_text->SetText(UTF8ToUTF16(cases[j].text));
      render_text->SetDirectionalityMode(DIRECTIONALITY_FROM_TEXT);
      EXPECT_EQ(render_text->GetDisplayTextDirection(),cases[j].text_direction);
      render_text->SetDirectionalityMode(DIRECTIONALITY_FROM_UI);
      EXPECT_EQ(render_text->GetDisplayTextDirection(), ui_direction);
      render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_LTR);
      EXPECT_EQ(render_text->GetDisplayTextDirection(),
                base::i18n::LEFT_TO_RIGHT);
      render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_RTL);
      EXPECT_EQ(render_text->GetDisplayTextDirection(),
                base::i18n::RIGHT_TO_LEFT);
      render_text->SetDirectionalityMode(DIRECTIONALITY_AS_URL);
      EXPECT_EQ(render_text->GetDisplayTextDirection(),
                base::i18n::LEFT_TO_RIGHT);
    }
  }

  EXPECT_EQ(was_rtl, base::i18n::IsRTL());

  // Ensure that text changes update the direction for DIRECTIONALITY_FROM_TEXT.
  render_text->SetDirectionalityMode(DIRECTIONALITY_FROM_TEXT);
  render_text->SetText(UTF8ToUTF16(kLtr));
  EXPECT_EQ(render_text->GetDisplayTextDirection(), base::i18n::LEFT_TO_RIGHT);
  render_text->SetText(UTF8ToUTF16(kRtl));
  EXPECT_EQ(render_text->GetDisplayTextDirection(), base::i18n::RIGHT_TO_LEFT);
}

struct GetTextIndexOfLineCase {
  const char* test_name;
  const wchar_t* const text;
  const std::vector<size_t> line_breaks;
  const bool set_word_wrap = false;
  const bool set_obscured = false;
};

class RenderTextTestWithGetTextIndexOfLineCase
    : public RenderTextTest,
      public ::testing::WithParamInterface<GetTextIndexOfLineCase> {
 public:
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<GetTextIndexOfLineCase> param_info) {
    return param_info.param.test_name;
  }
};

TEST_P(RenderTextTestWithGetTextIndexOfLineCase, GetTextIndexOfLine) {
  GetTextIndexOfLineCase param = GetParam();
  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  SetGlyphWidth(10);
  if (param.set_word_wrap) {
    render_text->SetDisplayRect(Rect(1, 1000));
    render_text->SetWordWrapBehavior(WRAP_LONG_WORDS);
  }
  render_text->SetObscured(param.set_obscured);
  render_text->SetText(base::WideToUTF16(param.text));
  for (size_t i = 0; i < param.line_breaks.size(); ++i) {
    EXPECT_EQ(param.line_breaks[i], render_text->GetTextIndexOfLine(i));
  }
}

const GetTextIndexOfLineCase kGetTextIndexOfLineCases[] = {
    {"emptyString", L"", {0}},
    // The following test strings are three character strings.
    // The word wrap makes each character fall on a new line.
    {"kWeak_minWidth", L" . ", {0, 1, 2}, kUseWordWrap},
    {"kLtr_minWidth", L"abc", {0, 1, 2}, kUseWordWrap},
    {"kLtrRtl_minWidth", L"a\u05d0\u05d1", {0, 1, 2}, kUseWordWrap},
    {"kLtrRtlLtr_minWidth", L"a\u05d1b", {0, 1, 2}, kUseWordWrap},
    {"kRtl_minWidth", L"\u05d0\u05d1\u05d2", {0, 1, 2}, kUseWordWrap},
    {"kRtlLtr_minWidth", L"\u05d0\u05d1a", {0, 1, 2}, kUseWordWrap},
    {"kRtlLtrRtl_minWidth", L"\u05d0a\u05d1", {0, 1, 2}, kUseWordWrap},
    // The following test strings have 2 graphemes separated by a newline.
    // The obscured text replace each grapheme by a single codepoint.
    {"grapheme_unobscured",
     L"\U0001F601\n\U0001F468\u200D\u2708\uFE0F\nx",
     {0, 3, 9}},
    {"grapheme_obscured",
     L"\U0001F601\n\U0001F468\u200D\u2708\uFE0F\nx",
     {0, 3, 9},
     !kUseWordWrap,
     kUseObscuredText},
    // The following test strings have a new line character.
    {"basic_newLine", L"abc\ndef", {0, 4}},
    {"basic_newLineWindows", L"abc\r\ndef", {0, 5}},
    {"spaces_newLine", L"a \n b ", {0, 3}},
    {"spaces_newLineWindows", L"a \r\n b ", {0, 4}},
    {"double_newLine", L"a\n\nb", {0, 2, 3}},
    {"double_newLineWindows", L"a\r\n\r\nb", {0, 3, 5}},
    {"start_newLine", L"\nab", {0, 1}},
    {"start_newLineWindows", L"\r\nab", {0, 2}},
    {"end_newLine", L"ab\n", {0}},
    {"end_newLineWindows", L"ab\r\n", {0}},
    {"isolated_newLine", L"\n", {0}},
    {"isolated_newLineWindows", L"\r\n", {0}},
    {"isolatedDouble_newLine", L"\n\n", {0, 1}},
    {"isolatedDouble_newLineWindows", L"\r\n\r\n", {0, 2}},
    // The following test strings have unicode characters.
    {"playSymbol_unicode", L"x\n\u25B6\ny", {0, 2, 4}},
    {"emoji_unicode", L"x\n\U0001F601\ny\n\u2728\nz", {0, 2, 5, 7, 9}},
    {"flag_unicode", L"🇬🇧\n🇯🇵", {0, 5}, false, false},
    // The following cases test that GetTextIndexOfLine returns the length of
    // the text when passed a line index larger than the number of lines.
    {"basic_outsideRange", L"abc", {0, 1, 2, 3, 3}, kUseWordWrap},
    {"emptyString_outsideRange", L"", {0, 0, 0}},
    {"newLine_outsideRange", L"\n", {0, 1, 1}},
    {"newLineWindows_outsideRange", L"\r\n", {0, 2, 2, 2}},
    {"doubleNewLine_outsideRange", L"\n\n", {0, 1, 2, 2}},
    {"doubleNewLineWindows_outsideRange", L"\r\n\r\n", {0, 2, 4, 4}},
};

INSTANTIATE_TEST_SUITE_P(
    GetTextIndexOfLine,
    RenderTextTestWithGetTextIndexOfLineCase,
    ::testing::ValuesIn(kGetTextIndexOfLineCases),
    RenderTextTestWithGetTextIndexOfLineCase::ParamInfoToString);

TEST_F(RenderTextTest, MoveCursorLeftRightInLtr) {
  RenderText* render_text = GetRenderText();
  // Pure LTR.
  render_text->SetText(ASCIIToUTF16("abc"));
  // |expected| saves the expected SelectionModel when moving cursor from left
  // to right.
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtrRtl) {
  RenderText* render_text = GetRenderText();
  // LTR-RTL
  render_text->SetText(UTF8ToUTF16("abc\u05d0\u05d1\u05d2"));
  // The last one is the expected END position.
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(6, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtrRtlLtr) {
  RenderText* render_text = GetRenderText();
  // LTR-RTL-LTR.
  render_text->SetText(UTF8ToUTF16("a\u05d1b"));
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtl) {
  RenderText* render_text = GetRenderText();
  // Pure RTL.
  render_text->SetText(UTF8ToUTF16("\u05d0\u05d1\u05d2"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  std::vector<SelectionModel> expected;

  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_LEFT);

  expected.clear();

  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_RIGHT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtlLtr) {
  RenderText* render_text = GetRenderText();
  // RTL-LTR
  render_text->SetText(UTF8ToUTF16("\u05d0\u05d1\u05d2abc"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_LEFT);

  expected.clear();
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(6, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_RIGHT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtlLtrRtl) {
  RenderText* render_text = GetRenderText();
  // RTL-LTR-RTL.
  render_text->SetText(UTF8ToUTF16("\u05d0a\u05d1"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_LEFT);

  expected.clear();
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text, expected, CURSOR_RIGHT);
}

TEST_F(RenderTextTest, MoveCursorLeftRight_ComplexScript) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("\u0915\u093f\u0915\u094d\u0915"));
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(5U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(5U, render_text->cursor_position());

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
}

TEST_F(RenderTextTest, MoveCursorLeftRight_MeiryoUILigatures) {
  RenderText* render_text = GetRenderText();
  // Meiryo UI uses single-glyph ligatures for 'ff' and 'ffi', but each letter
  // (code point) has unique bounds, so mid-glyph cursoring should be possible.
  render_text->SetFontList(FontList("Meiryo UI, 12px"));
  render_text->SetText(ASCIIToUTF16("ff ffi"));
  render_text->SetDisplayRect(gfx::Rect(100, 100));
  EXPECT_EQ(0U, render_text->cursor_position());

  gfx::Rect last_selection_bounds = GetSelectionBoundsUnion();
  for (size_t i = 0; i < render_text->text().length(); ++i) {
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_RETAIN);
    EXPECT_EQ(i + 1, render_text->cursor_position());

    // Verify the selection bounds also increase and that the correct bounds are
    // returned even when the grapheme boundary lies within a glyph.
    const gfx::Rect selection_bounds = GetSelectionBoundsUnion();
    EXPECT_GT(selection_bounds.right(), last_selection_bounds.right());
    EXPECT_EQ(selection_bounds.x(), last_selection_bounds.x());
    last_selection_bounds = selection_bounds;
  }
  EXPECT_EQ(6U, render_text->cursor_position());
}

TEST_F(RenderTextTest, GraphemeIterator) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("a\u0065\u0301b"));

  internal::GraphemeIterator iterator =
      render_text->GetGraphemeIteratorAtTextIndex(0);

  EXPECT_EQ(0U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(0U, render_text->GetDisplayTextIndex(iterator));
  ++iterator;
  EXPECT_EQ(1U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(1U, render_text->GetDisplayTextIndex(iterator));
  ++iterator;
  EXPECT_EQ(3U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(3U, render_text->GetDisplayTextIndex(iterator));
  ++iterator;
  EXPECT_EQ(4U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(4U, render_text->GetDisplayTextIndex(iterator));

  --iterator;
  EXPECT_EQ(3U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(3U, render_text->GetDisplayTextIndex(iterator));
  --iterator;
  EXPECT_EQ(1U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(1U, render_text->GetDisplayTextIndex(iterator));
  --iterator;
  EXPECT_EQ(0U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(0U, render_text->GetDisplayTextIndex(iterator));

  iterator = render_text->GetGraphemeIteratorAtTextIndex(0);
  EXPECT_EQ(0U, render_text->GetTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtTextIndex(1);
  EXPECT_EQ(1U, render_text->GetTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtTextIndex(2);
  EXPECT_EQ(1U, render_text->GetTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtTextIndex(3);
  EXPECT_EQ(3U, render_text->GetTextIndex(iterator));

  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(0);
  EXPECT_EQ(0U, render_text->GetDisplayTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(1);
  EXPECT_EQ(1U, render_text->GetDisplayTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(2);
  EXPECT_EQ(1U, render_text->GetDisplayTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(3);
  EXPECT_EQ(3U, render_text->GetDisplayTextIndex(iterator));

  render_text->SetText(UTF8ToUTF16("e\u0301b"));
  render_text->SetObscured(true);
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(0);
  EXPECT_EQ(0U, render_text->GetTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(1);
  EXPECT_EQ(2U, render_text->GetTextIndex(iterator));
  render_text->SetObscured(false);
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(0);
  EXPECT_EQ(0U, render_text->GetTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(1);
  EXPECT_EQ(0U, render_text->GetTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(2);
  EXPECT_EQ(2U, render_text->GetTextIndex(iterator));

  render_text->SetText(WideToUTF16(L"a\U0001F601b"));
  render_text->SetObscured(true);
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(0);
  EXPECT_EQ(0U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(0U, render_text->GetDisplayTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(1);
  EXPECT_EQ(1U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(1U, render_text->GetDisplayTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(2);
  EXPECT_EQ(3U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(2U, render_text->GetDisplayTextIndex(iterator));

  render_text->SetText(UTF8ToUTF16("\U0001F468\u200D\u2708\uFE0Fx"));
  render_text->SetObscured(true);
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(0);
  EXPECT_EQ(0U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(0U, render_text->GetDisplayTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(1);
  EXPECT_EQ(5U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(1U, render_text->GetDisplayTextIndex(iterator));
  render_text->SetObscured(false);
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(0);
  EXPECT_EQ(0U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(0U, render_text->GetDisplayTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(1);
  EXPECT_EQ(0U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(0U, render_text->GetDisplayTextIndex(iterator));
  iterator = render_text->GetGraphemeIteratorAtDisplayTextIndex(5);
  EXPECT_EQ(5U, render_text->GetTextIndex(iterator));
  EXPECT_EQ(5U, render_text->GetDisplayTextIndex(iterator));
}

TEST_F(RenderTextTest, GraphemeBoundaries) {
  static const wchar_t text[] =
      L"\u0065\u0301"        // Letter 'e' U+0065 and acute accent U+0301
      L"\u0036\uFE0F\u20E3"  // Emoji 'keycap letter 6'
      L"\U0001F468\u200D\u2708\uFE0F";  // Emoji 'pilot'.

  RenderText* render_text = GetRenderText();
  render_text->SetText(WideToUTF16(text));

  EXPECT_TRUE(render_text->IsGraphemeBoundary(0));
  EXPECT_FALSE(render_text->IsGraphemeBoundary(1));
  EXPECT_TRUE(render_text->IsGraphemeBoundary(2));
  EXPECT_FALSE(render_text->IsGraphemeBoundary(3));
  EXPECT_FALSE(render_text->IsGraphemeBoundary(4));
  EXPECT_TRUE(render_text->IsGraphemeBoundary(5));
  EXPECT_FALSE(render_text->IsGraphemeBoundary(6));
  EXPECT_FALSE(render_text->IsGraphemeBoundary(7));
  EXPECT_FALSE(render_text->IsGraphemeBoundary(8));
  EXPECT_FALSE(render_text->IsGraphemeBoundary(9));
  EXPECT_TRUE(render_text->IsGraphemeBoundary(10));

  EXPECT_EQ(2U, render_text->IndexOfAdjacentGrapheme(0, CURSOR_FORWARD));
  EXPECT_EQ(2U, render_text->IndexOfAdjacentGrapheme(1, CURSOR_FORWARD));
  EXPECT_EQ(5U, render_text->IndexOfAdjacentGrapheme(2, CURSOR_FORWARD));
  EXPECT_EQ(5U, render_text->IndexOfAdjacentGrapheme(3, CURSOR_FORWARD));
  EXPECT_EQ(5U, render_text->IndexOfAdjacentGrapheme(4, CURSOR_FORWARD));
  EXPECT_EQ(10U, render_text->IndexOfAdjacentGrapheme(5, CURSOR_FORWARD));
  EXPECT_EQ(10U, render_text->IndexOfAdjacentGrapheme(6, CURSOR_FORWARD));
  EXPECT_EQ(10U, render_text->IndexOfAdjacentGrapheme(7, CURSOR_FORWARD));
  EXPECT_EQ(10U, render_text->IndexOfAdjacentGrapheme(8, CURSOR_FORWARD));
  EXPECT_EQ(10U, render_text->IndexOfAdjacentGrapheme(9, CURSOR_FORWARD));
  EXPECT_EQ(10U, render_text->IndexOfAdjacentGrapheme(10, CURSOR_FORWARD));

  EXPECT_EQ(0U, render_text->IndexOfAdjacentGrapheme(0, CURSOR_BACKWARD));
  EXPECT_EQ(0U, render_text->IndexOfAdjacentGrapheme(1, CURSOR_BACKWARD));
  EXPECT_EQ(0U, render_text->IndexOfAdjacentGrapheme(2, CURSOR_BACKWARD));
  EXPECT_EQ(2U, render_text->IndexOfAdjacentGrapheme(3, CURSOR_BACKWARD));
  EXPECT_EQ(2U, render_text->IndexOfAdjacentGrapheme(4, CURSOR_BACKWARD));
  EXPECT_EQ(2U, render_text->IndexOfAdjacentGrapheme(5, CURSOR_BACKWARD));
  EXPECT_EQ(5U, render_text->IndexOfAdjacentGrapheme(6, CURSOR_BACKWARD));
  EXPECT_EQ(5U, render_text->IndexOfAdjacentGrapheme(7, CURSOR_BACKWARD));
  EXPECT_EQ(5U, render_text->IndexOfAdjacentGrapheme(8, CURSOR_BACKWARD));
  EXPECT_EQ(5U, render_text->IndexOfAdjacentGrapheme(9, CURSOR_BACKWARD));
  EXPECT_EQ(5U, render_text->IndexOfAdjacentGrapheme(10, CURSOR_BACKWARD));
}

TEST_F(RenderTextTest, GraphemePositions) {
  // LTR कि (DEVANAGARI KA with VOWEL I) (2-char grapheme), LTR abc, and LTR कि.
  const base::string16 kText1 = UTF8ToUTF16("\u0915\u093fabc\u0915\u093f");

  // LTR ab, LTR कि (DEVANAGARI KA with VOWEL I) (2-char grapheme), LTR cd.
  const base::string16 kText2 = UTF8ToUTF16("ab\u0915\u093fcd");

  // LTR ab, 𝄞 'MUSICAL SYMBOL G CLEF' U+1D11E (surrogate pair), LTR cd.
  // Windows requires wide strings for \Unnnnnnnn universal character names.
  const base::string16 kText3 = WideToUTF16(L"ab\U0001D11Ecd");

  struct {
    base::string16 text;
    size_t index;
    size_t expected_previous;
    size_t expected_next;
  } cases[] = {
    { base::string16(), 0, 0, 0 },
    { base::string16(), 1, 0, 0 },
    { base::string16(), 50, 0, 0 },
    { kText1, 0, 0, 2 },
    { kText1, 1, 0, 2 },
    { kText1, 2, 0, 3 },
    { kText1, 3, 2, 4 },
    { kText1, 4, 3, 5 },
    { kText1, 5, 4, 7 },
    { kText1, 6, 5, 7 },
    { kText1, 7, 5, 7 },
    { kText1, 8, 7, 7 },
    { kText1, 50, 7, 7 },
    { kText2, 0, 0, 1 },
    { kText2, 1, 0, 2 },
    { kText2, 2, 1, 4 },
    { kText2, 3, 2, 4 },
    { kText2, 4, 2, 5 },
    { kText2, 5, 4, 6 },
    { kText2, 6, 5, 6 },
    { kText2, 7, 6, 6 },
    { kText2, 50, 6, 6 },
    { kText3, 0, 0, 1 },
    { kText3, 1, 0, 2 },
    { kText3, 2, 1, 4 },
    { kText3, 3, 2, 4 },
    { kText3, 4, 2, 5 },
    { kText3, 5, 4, 6 },
    { kText3, 6, 5, 6 },
    { kText3, 7, 6, 6 },
    { kText3, 50, 6, 6 },
  };

  RenderText* render_text = GetRenderText();
  for (size_t i = 0; i < base::size(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    render_text->SetText(cases[i].text);

    size_t next = render_text->IndexOfAdjacentGrapheme(cases[i].index,
                                                       CURSOR_FORWARD);
    EXPECT_EQ(cases[i].expected_next, next);
    EXPECT_TRUE(render_text->IsValidCursorIndex(next));

    size_t previous = render_text->IndexOfAdjacentGrapheme(cases[i].index,
                                                           CURSOR_BACKWARD);
    EXPECT_EQ(cases[i].expected_previous, previous);
    EXPECT_TRUE(render_text->IsValidCursorIndex(previous));
  }
}

TEST_F(RenderTextTest, MidGraphemeSelectionBounds) {
  // Test that selection bounds may be set amid multi-character graphemes.
  const base::string16 kHindi = UTF8ToUTF16("\u0915\u093f");
  const base::string16 kThai = UTF8ToUTF16("\u0e08\u0e33");
  const base::string16 cases[] = { kHindi, kThai };

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(100, 1000));
  for (size_t i = 0; i < base::size(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    render_text->SetText(cases[i]);
    EXPECT_TRUE(render_text->IsValidLogicalIndex(1));
    EXPECT_FALSE(render_text->IsValidCursorIndex(1));
    EXPECT_TRUE(render_text->SelectRange(Range(2, 1)));
    EXPECT_EQ(Range(2, 1), render_text->selection());
    EXPECT_EQ(1U, render_text->cursor_position());

    // Verify that the selection bounds extend over the entire grapheme, even if
    // the selection is set amid the grapheme.
    const gfx::Rect mid_grapheme_bounds = GetSelectionBoundsUnion();
    render_text->SelectAll(false);
    EXPECT_EQ(GetSelectionBoundsUnion(), mid_grapheme_bounds);

    // Although selection bounds may be set within a multi-character grapheme,
    // cursor movement (e.g. via arrow key) should avoid those indices.
    EXPECT_TRUE(render_text->SelectRange(Range(2, 1)));
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
    EXPECT_EQ(0U, render_text->cursor_position());
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
    EXPECT_EQ(2U, render_text->cursor_position());
  }
}

TEST_F(RenderTextTest, FindCursorPosition) {
  const char* kTestStrings[] = {kLtrRtl, kLtrRtlLtr, kRtlLtr, kRtlLtrRtl};
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(0, 0, 100, 20));
  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    for (size_t j = 0; j < render_text->text().length(); ++j) {
      gfx::RangeF cursor_span = render_text->GetCursorSpan(Range(j, j + 1));
      // Test a point just inside the leading edge of the glyph bounds.
      float x = cursor_span.start() + (cursor_span.is_reversed() ? -1 : 1);
      Point point = gfx::ToCeiledPoint(PointF(x, GetCursorYForTesting()));
      EXPECT_EQ(j, render_text->FindCursorPosition(point).caret_pos());
    }
  }
}

// Tests that FindCursorPosition behaves correctly for multi-line text.
TEST_F(RenderTextTest, FindCursorPositionMultiline) {
  const char* kTestStrings[] = {"abc def",
                                "\u05d0\u05d1\u05d2 \u05d3\u05d4\u05d5"};

  SetGlyphWidth(5);
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(25, 1000));
  render_text->SetMultiline(true);

  for (size_t i = 0; i < base::size(kTestStrings); i++) {
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    EXPECT_EQ(2u, render_text->GetNumLines());

    const bool is_ltr =
        render_text->GetDisplayTextDirection() == base::i18n::LEFT_TO_RIGHT;
    for (size_t j = 0; j < render_text->text().length(); ++j) {
      SCOPED_TRACE(base::StringPrintf(
          "Testing index %" PRIuS " for case %" PRIuS "", j, i));
      render_text->SelectRange(Range(j, j + 1));

      // Test a point inside the leading edge of the glyph bounds.
      const Rect bounds = GetSelectionBoundsUnion();
      const Point cursor_position(is_ltr ? bounds.x() + 1 : bounds.right() - 1,
                                  bounds.y() + 1);

      const SelectionModel model =
          render_text->FindCursorPosition(cursor_position);
      EXPECT_EQ(j, model.caret_pos());
      EXPECT_EQ(CURSOR_FORWARD, model.caret_affinity());
    }
  }
}

// Ensure FindCursorPosition returns positions only at valid grapheme
// boundaries.
TEST_F(RenderTextTest, FindCursorPosition_GraphemeBoundaries) {
  struct {
    base::string16 text;
    std::set<size_t> expected_cursor_positions;
  } cases[] = {
      // LTR कि (DEVANAGARI KA with VOWEL I) (2-char grapheme), LTR abc, LTR कि.
      {UTF8ToUTF16("\u0915\u093fabc\u0915\u093f"), {0, 2, 3, 4, 5, 7}},
      // LTR ab, LTR कि (DEVANAGARI KA with VOWEL I) (2-char grapheme), LTR cd.
      {UTF8ToUTF16("ab\u0915\u093fcd"), {0, 1, 2, 4, 5, 6}},
      // LTR ab, surrogate pair composed of two 16 bit characters, LTR cd.
      // Windows requires wide strings for \Unnnnnnnn universal character names.
      {WideToUTF16(L"ab\U0001D11Ecd"), {0, 1, 2, 4, 5, 6}}};

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(gfx::Rect(100, 30));
  for (size_t i = 0; i < base::size(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing case %" PRIuS "", i));
    render_text->SetText(cases[i].text);
    std::set<size_t> obtained_cursor_positions;
    size_t cursor_y = GetCursorYForTesting();
    for (int x = -5; x < 105; x++)
      obtained_cursor_positions.insert(
          render_text->FindCursorPosition(gfx::Point(x, cursor_y)).caret_pos());
    EXPECT_EQ(cases[i].expected_cursor_positions, obtained_cursor_positions);
  }
}

TEST_F(RenderTextTest, EdgeSelectionModels) {
  // Simple Latin text.
  const base::string16 kLatin = ASCIIToUTF16("abc");
  // LTR कि (DEVANAGARI KA with VOWEL I).
  const base::string16 kLTRGrapheme = UTF8ToUTF16("\u0915\u093f");
  // LTR कि (DEVANAGARI KA with VOWEL I), LTR a, LTR कि.
  const base::string16 kHindiLatin = UTF8ToUTF16("\u0915\u093fa\u0915\u093f");
  // RTL נָ (Hebrew letter NUN and point QAMATS).
  const base::string16 kRTLGrapheme = UTF8ToUTF16("\u05e0\u05b8");
  // RTL נָ (Hebrew letter NUN and point QAMATS), LTR a, RTL נָ.
  const base::string16 kHebrewLatin = UTF8ToUTF16("\u05e0\u05b8a\u05e0\u05b8");

  struct {
    base::string16 text;
    base::i18n::TextDirection expected_text_direction;
  } cases[] = {
    { base::string16(), base::i18n::LEFT_TO_RIGHT },
    { kLatin,       base::i18n::LEFT_TO_RIGHT },
    { kLTRGrapheme, base::i18n::LEFT_TO_RIGHT },
    { kHindiLatin,  base::i18n::LEFT_TO_RIGHT },
    { kRTLGrapheme, base::i18n::RIGHT_TO_LEFT },
    { kHebrewLatin, base::i18n::RIGHT_TO_LEFT },
  };

  RenderText* render_text = GetRenderText();
  for (size_t i = 0; i < base::size(cases); i++) {
    render_text->SetText(cases[i].text);
    bool ltr = (cases[i].expected_text_direction == base::i18n::LEFT_TO_RIGHT);

    SelectionModel start_edge =
        test_api()->EdgeSelectionModel(ltr ? CURSOR_LEFT : CURSOR_RIGHT);
    EXPECT_EQ(start_edge, SelectionModel(0, CURSOR_BACKWARD));

    SelectionModel end_edge =
        test_api()->EdgeSelectionModel(ltr ? CURSOR_RIGHT : CURSOR_LEFT);
    EXPECT_EQ(end_edge, SelectionModel(cases[i].text.length(), CURSOR_FORWARD));
  }
}

TEST_F(RenderTextTest, SelectAll) {
  const char* const cases[] = {kWeak, kLtr,    kLtrRtl,   kLtrRtlLtr,
                               kRtl,  kRtlLtr, kRtlLtrRtl};

  // Ensure that SelectAll respects the |reversed| argument regardless of
  // application locale and text content directionality.
  RenderText* render_text = GetRenderText();
  const SelectionModel expected_reversed(Range(3, 0), CURSOR_FORWARD);
  const SelectionModel expected_forwards(Range(0, 3), CURSOR_BACKWARD);
  const bool was_rtl = base::i18n::IsRTL();

  for (size_t i = 0; i < 2; ++i) {
    SetRTL(!base::i18n::IsRTL());
    // Test that an empty string produces an empty selection model.
    render_text->SetText(base::string16());
    EXPECT_EQ(render_text->selection_model(), SelectionModel());

    // Test the weak, LTR, RTL, and Bidi string cases.
    for (size_t j = 0; j < base::size(cases); j++) {
      render_text->SetText(UTF8ToUTF16(cases[j]));
      render_text->SelectAll(false);
      EXPECT_EQ(render_text->selection_model(), expected_forwards);
      render_text->SelectAll(true);
      EXPECT_EQ(render_text->selection_model(), expected_reversed);
    }
  }

  EXPECT_EQ(was_rtl, base::i18n::IsRTL());
}

TEST_F(RenderTextTest, MoveCursorLeftRightWithSelection) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("abc\u05d0\u05d1\u05d2"));
  // Left arrow on select ranging (6, 4).
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(6), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(4), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(6), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_RETAIN);
  EXPECT_EQ(Range(6, 5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_RETAIN);
  EXPECT_EQ(Range(6, 4), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(6), render_text->selection());

  // Right arrow on select ranging (4, 6).
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(0), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(1), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(2), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(3), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(4), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_RETAIN);
  EXPECT_EQ(Range(4, 5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_RETAIN);
  EXPECT_EQ(Range(4, 6), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(4), render_text->selection());
}

TEST_F(RenderTextTest, MoveCursorLeftRightWithSelection_Multiline) {
  SetGlyphWidth(5);
  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetDisplayRect(Rect(20, 1000));
  render_text->SetText(ASCIIToUTF16("012 456\n\n90"));
  EXPECT_EQ(4U, render_text->GetNumLines());

  // Move cursor right to the end of the text.
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(4), render_text->selection());
  EXPECT_EQ(0U, GetLineContainingCaret());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(5), render_text->selection());
  EXPECT_EQ(1U, GetLineContainingCaret());
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(7), render_text->selection());
  EXPECT_EQ(1U, GetLineContainingCaret());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(8), render_text->selection());
  EXPECT_EQ(2U, GetLineContainingCaret());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(9), render_text->selection());
  EXPECT_EQ(3U, GetLineContainingCaret());
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(11), render_text->selection());
  EXPECT_EQ(3U, GetLineContainingCaret());

  // Move cursor left to the beginning of the text.
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(9), render_text->selection());
  EXPECT_EQ(3U, GetLineContainingCaret());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(8), render_text->selection());
  EXPECT_EQ(2U, GetLineContainingCaret());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(7), render_text->selection());
  EXPECT_EQ(1U, GetLineContainingCaret());
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(4), render_text->selection());
  EXPECT_EQ(1U, GetLineContainingCaret());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(3), render_text->selection());
  EXPECT_EQ(0U, GetLineContainingCaret());
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(0), render_text->selection());
  EXPECT_EQ(0U, GetLineContainingCaret());

  // Move cursor right with WORD_BREAK.
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
#if defined(OS_WIN)
  EXPECT_EQ(Range(4), render_text->selection());
#else
  EXPECT_EQ(Range(3), render_text->selection());
#endif
  EXPECT_EQ(0U, GetLineContainingCaret());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
#if defined(OS_WIN)
  EXPECT_EQ(Range(9), render_text->selection());
  EXPECT_EQ(3U, GetLineContainingCaret());
#else
  EXPECT_EQ(Range(7), render_text->selection());
  EXPECT_EQ(1U, GetLineContainingCaret());
#endif
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(11), render_text->selection());
  EXPECT_EQ(3U, GetLineContainingCaret());

  // Move cursor left with WORD_BREAK.
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(9), render_text->selection());
  EXPECT_EQ(3U, GetLineContainingCaret());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(4), render_text->selection());
  EXPECT_EQ(1U, GetLineContainingCaret());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(0), render_text->selection());
  EXPECT_EQ(0U, GetLineContainingCaret());

  // Move cursor right with FIELD_BREAK.
  render_text->MoveCursor(FIELD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(11), render_text->selection());
  EXPECT_EQ(3U, GetLineContainingCaret());

  // Move cursor left with FIELD_BREAK.
  render_text->MoveCursor(FIELD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(0), render_text->selection());
  EXPECT_EQ(0U, GetLineContainingCaret());
}

TEST_F(RenderTextTest, CenteredDisplayOffset) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdefghij"));
  render_text->SetHorizontalAlignment(ALIGN_CENTER);

  const int kEnlargement = 10;
  const int content_width = render_text->GetContentWidth();
  Rect display_rect(0, 0, content_width / 2,
                    render_text->font_list().GetHeight());
  render_text->SetDisplayRect(display_rect);

  // Move the cursor to the beginning of the text and, by checking the cursor
  // bounds, make sure no empty space is to the left of the text.
  render_text->SetCursorPosition(0);
  EXPECT_EQ(display_rect.x(), render_text->GetUpdatedCursorBounds().x());

  // Widen the display rect and, by checking the cursor bounds, make sure no
  // empty space is introduced to the left of the text.
  display_rect.Inset(0, 0, -kEnlargement, 0);
  render_text->SetDisplayRect(display_rect);
  EXPECT_EQ(display_rect.x(), render_text->GetUpdatedCursorBounds().x());

  // Move the cursor to the end of the text and, by checking the cursor
  // bounds, make sure no empty space is to the right of the text.
  render_text->SetCursorPosition(render_text->text().length());
  EXPECT_EQ(display_rect.right(),
            render_text->GetUpdatedCursorBounds().right());

  // Widen the display rect and, by checking the cursor bounds, make sure no
  // empty space is introduced to the right of the text.
  display_rect.Inset(0, 0, -kEnlargement, 0);
  render_text->SetDisplayRect(display_rect);
  EXPECT_EQ(display_rect.right(),
            render_text->GetUpdatedCursorBounds().right());
}

void MoveLeftRightByWordVerifier(RenderText* render_text, const char* str) {
  SCOPED_TRACE(str);
  const base::string16 str16(UTF8ToUTF16(str));
  render_text->SetText(str16);

  // Test moving by word from left to right.
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  const size_t num_words = (str16.length() + 1) / 4;
  for (size_t i = 0; i < num_words; ++i) {
    // First, test moving by word from a word break position, such as from
    // "|abc def" to "abc| def".
    const SelectionModel start = render_text->selection_model();
    render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
    const SelectionModel end = render_text->selection_model();

    // For testing simplicity, each word is a 3-character word.
#if defined(OS_WIN)
    // Windows moves from "|abc def" to "abc |def" instead of "abc| def", so
    // traverse 4 characters on all but the last word instead of all but the
    // first.
    const int num_character_moves = (i == num_words - 1) ? 3 : 4;
#else
    const int num_character_moves = (i == 0) ? 3 : 4;
#endif
    render_text->SetSelection(start);
    for (int j = 0; j < num_character_moves; ++j)
      render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
    EXPECT_EQ(end, render_text->selection_model());

    // Then, test moving by word from positions inside the word, such as from
    // "a|bc def" to "abc| def", and from "ab|c def" to "abc| def".
    for (int j = 1; j < num_character_moves; ++j) {
      render_text->SetSelection(start);
      for (int k = 0; k < j; ++k)
        render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
      render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
      EXPECT_EQ(end, render_text->selection_model());
    }
  }

  // Test moving by word from right to left.
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  for (size_t i = 0; i < num_words; ++i) {
    const SelectionModel start = render_text->selection_model();
    render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
    const SelectionModel end = render_text->selection_model();

    const int num_character_moves = (i == 0) ? 3 : 4;
    render_text->SetSelection(start);
    for (int j = 0; j < num_character_moves; ++j)
      render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
    EXPECT_EQ(end, render_text->selection_model());

    for (int j = 1; j < num_character_moves; ++j) {
      render_text->SetSelection(start);
      for (int k = 0; k < j; ++k)
        render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
      render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
      EXPECT_EQ(end, render_text->selection_model());
    }
  }
}

#if defined(OS_WIN)
// TODO(aleventhal): https://crbug.com/906308 Fix bugs, update verifier code
// above, and enable for Windows.
#define MAYBE_MoveLeftRightByWordInBidiText \
  DISABLED_MoveLeftRightByWordInBidiText
#else
#define MAYBE_MoveLeftRightByWordInBidiText MoveLeftRightByWordInBidiText
#endif
TEST_F(RenderTextTest, MAYBE_MoveLeftRightByWordInBidiText) {
  RenderText* render_text = GetRenderText();
  // For testing simplicity, each word is a 3-character word.
  std::vector<const char*> test;
  test.push_back("abc");
  test.push_back("abc def");
  test.push_back("\u05E1\u05E2\u05E3");
  test.push_back("\u05E1\u05E2\u05E3 \u05E4\u05E5\u05E6");
  test.push_back("abc \u05E1\u05E2\u05E3");
  test.push_back("abc def \u05E1\u05E2\u05E3 \u05E4\u05E5\u05E6");
  test.push_back(
      "abc def hij \u05E1\u05E2\u05E3 \u05E4\u05E5\u05E6"
      " \u05E7\u05E8\u05E9");

  test.push_back("abc \u05E1\u05E2\u05E3 hij");
  test.push_back("abc def \u05E1\u05E2\u05E3 \u05E4\u05E5\u05E6 hij opq");
  test.push_back(
      "abc def hij \u05E1\u05E2\u05E3 \u05E4\u05E5\u05E6"
      " \u05E7\u05E8\u05E9 opq rst uvw");

  test.push_back("\u05E1\u05E2\u05E3 abc");
  test.push_back("\u05E1\u05E2\u05E3 \u05E4\u05E5\u05E6 abc def");
  test.push_back(
      "\u05E1\u05E2\u05E3 \u05E4\u05E5\u05E6 \u05E7\u05E8\u05E9"
      " abc def hij");

  test.push_back("\u05D1\u05D2\u05D3 abc \u05E1\u05E2\u05E3");
  test.push_back(
      "\u05D1\u05D2\u05D3 \u05D4\u05D5\u05D6 abc def"
      " \u05E1\u05E2\u05E3 \u05E4\u05E5\u05E6");
  test.push_back(
      "\u05D1\u05D2\u05D3 \u05D4\u05D5\u05D6 \u05D7\u05D8\u05D9"
      " abc def hij \u05E1\u05E2\u05E3 \u05E4\u05E5\u05E6"
      " \u05E7\u05E8\u05E9");

  for (size_t i = 0; i < test.size(); ++i)
    MoveLeftRightByWordVerifier(render_text, test[i]);
}

TEST_F(RenderTextTest, MoveLeftRightByWordInBidiText_TestEndOfText) {
  RenderText* render_text = GetRenderText();

  render_text->SetText(UTF8ToUTF16("ab\u05E1"));
  // Moving the cursor by word from "abC|" to the left should return "|abC".
  // But since end of text is always treated as a word break, it returns
  // position "ab|C".
  // TODO(xji): Need to make it work as expected.
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  // EXPECT_EQ(SelectionModel(), render_text->selection_model());

  // Moving the cursor by word from "|abC" to the right returns "abC|".
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(SelectionModel(3, CURSOR_FORWARD), render_text->selection_model());

  render_text->SetText(UTF8ToUTF16("\u05E1\u05E2a"));
  // For logical text "BCa", moving the cursor by word from "aCB|" to the left
  // returns "|aCB".
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(SelectionModel(3, CURSOR_FORWARD), render_text->selection_model());

  // Moving the cursor by word from "|aCB" to the right should return "aCB|".
  // But since end of text is always treated as a word break, it returns
  // position "a|CB".
  // TODO(xji): Need to make it work as expected.
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  // EXPECT_EQ(SelectionModel(), render_text->selection_model());
}

TEST_F(RenderTextTest, MoveLeftRightByWordInTextWithMultiSpaces) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abc     def"));
  render_text->SetCursorPosition(5);
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
#if defined(OS_WIN)
  EXPECT_EQ(8U, render_text->cursor_position());
#else
  EXPECT_EQ(11U, render_text->cursor_position());
#endif

  render_text->SetCursorPosition(5);
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
}

TEST_F(RenderTextTest, MoveLeftRightByWordInThaiText) {
  RenderText* render_text = GetRenderText();
  // เรียกดูรวดเร็ว is broken to เรียก|ดู|รวดเร็ว.
  render_text->SetText(UTF8ToUTF16("เรียกดูรวดเร็ว"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(5U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(7U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(14U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(14U, render_text->cursor_position());

  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(7U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(5U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
}

// TODO(crbug.com/865527): Chinese and Japanese tokenization doesn't work on
// mobile.
#if !defined(OS_ANDROID)
TEST_F(RenderTextTest, MoveLeftRightByWordInChineseText) {
  RenderText* render_text = GetRenderText();
  // zh-Hans-CN: 我们去公园玩, broken to 我们|去|公园|玩.
  render_text->SetText(UTF8ToUTF16("\u6211\u4EEC\u53BB\u516C\u56ED\u73A9"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(3U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(5U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(6U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(6U, render_text->cursor_position());

  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(5U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(3U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
}
#endif

// Test the correct behavior of undirected selections: selections where the
// "end" of the selection that holds the cursor is only determined after the
// first cursor movement.
TEST_F(RenderTextTest, DirectedSelections) {
  RenderText* render_text = GetRenderText();

  auto ResultAfter = [&](VisualCursorDirection direction) -> base::string16 {
    render_text->MoveCursor(CHARACTER_BREAK, direction, SELECTION_RETAIN);
    return GetSelectedText(render_text);
  };

  render_text->SetText(ASCIIToUTF16("01234"));

  // Test Right, then Left. LTR.
  // Undirected, or forward when kSelectionIsAlwaysDirected.
  render_text->SelectRange({2, 4});
  EXPECT_EQ(ASCIIToUTF16("23"), GetSelectedText(render_text));
  EXPECT_EQ(ASCIIToUTF16("234"), ResultAfter(CURSOR_RIGHT));
  EXPECT_EQ(ASCIIToUTF16("23"), ResultAfter(CURSOR_LEFT));

  // Test collapsing the selection. This always ignores any existing direction.
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(2, 2), render_text->selection());  // Collapse left.

  // Undirected, or backward when kSelectionIsAlwaysDirected.
  render_text->SelectRange({4, 2});
  EXPECT_EQ(ASCIIToUTF16("23"), GetSelectedText(render_text));
  if (RenderText::kSelectionIsAlwaysDirected)
    EXPECT_EQ(ASCIIToUTF16("3"), ResultAfter(CURSOR_RIGHT));  // Keep left.
  else
    EXPECT_EQ(ASCIIToUTF16("234"), ResultAfter(CURSOR_RIGHT));  // Pick right.
  EXPECT_EQ(ASCIIToUTF16("23"), ResultAfter(CURSOR_LEFT));

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(2, 2), render_text->selection());  // Collapse left.

  // Test Left, then Right. LTR.
  // Undirected, or forward when kSelectionIsAlwaysDirected.
  render_text->SelectRange({2, 4});
  EXPECT_EQ(ASCIIToUTF16("23"), GetSelectedText(render_text));  // Sanity check,

  if (RenderText::kSelectionIsAlwaysDirected)
    EXPECT_EQ(ASCIIToUTF16("2"), ResultAfter(CURSOR_LEFT));  // Keep right.
  else
    EXPECT_EQ(ASCIIToUTF16("123"), ResultAfter(CURSOR_LEFT));  // Pick left.
  EXPECT_EQ(ASCIIToUTF16("23"), ResultAfter(CURSOR_RIGHT));

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(4, 4), render_text->selection());  // Collapse right.

  // Undirected, or backward when kSelectionIsAlwaysDirected.
  render_text->SelectRange({4, 2});
  EXPECT_EQ(ASCIIToUTF16("23"), GetSelectedText(render_text));
  EXPECT_EQ(ASCIIToUTF16("123"), ResultAfter(CURSOR_LEFT));
  EXPECT_EQ(ASCIIToUTF16("23"), ResultAfter(CURSOR_RIGHT));

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(4, 4), render_text->selection());  // Collapse right.

  auto ToHebrew = [](const char* digits) -> base::string16 {
    const base::string16 hebrew = UTF8ToUTF16("אבגדח");  // Roughly "abcde".
    DCHECK_EQ(5u, hebrew.size());
    base::string16 result;
    for (const char* d = digits; *d; d++)
      result += hebrew[*d - '0'];
    return result;
  };
  render_text->SetText(ToHebrew("01234"));

  // Test Left, then Right. RTL.
  // Undirected, or forward (to the left) when kSelectionIsAlwaysDirected.
  render_text->SelectRange({2, 4});
  EXPECT_EQ(ToHebrew("23"), GetSelectedText(render_text));
  EXPECT_EQ(ToHebrew("234"), ResultAfter(CURSOR_LEFT));
  EXPECT_EQ(ToHebrew("23"), ResultAfter(CURSOR_RIGHT));

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(4, 4), render_text->selection());  // Collapse left.

  // Undirected, or backward (to the right) when kSelectionIsAlwaysDirected.
  render_text->SelectRange({4, 2});
  EXPECT_EQ(ToHebrew("23"), GetSelectedText(render_text));
  if (RenderText::kSelectionIsAlwaysDirected)
    EXPECT_EQ(ToHebrew("3"), ResultAfter(CURSOR_LEFT));  // Keep right.
  else
    EXPECT_EQ(ToHebrew("234"), ResultAfter(CURSOR_LEFT));  // Pick left.
  EXPECT_EQ(ToHebrew("23"), ResultAfter(CURSOR_RIGHT));

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(4, 4), render_text->selection());  // Collapse left.

  // Test Right, then Left. RTL.
  // Undirected, or forward (to the left) when kSelectionIsAlwaysDirected.
  render_text->SelectRange({2, 4});
  EXPECT_EQ(ToHebrew("23"), GetSelectedText(render_text));
  if (RenderText::kSelectionIsAlwaysDirected)
    EXPECT_EQ(ToHebrew("2"), ResultAfter(CURSOR_RIGHT));  // Keep left.
  else
    EXPECT_EQ(ToHebrew("123"), ResultAfter(CURSOR_RIGHT));  // Pick right.
  EXPECT_EQ(ToHebrew("23"), ResultAfter(CURSOR_LEFT));

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(2, 2), render_text->selection());  // Collapse right.

  // Undirected, or backward (to the right) when kSelectionIsAlwaysDirected.
  render_text->SelectRange({4, 2});
  EXPECT_EQ(ToHebrew("23"), GetSelectedText(render_text));
  EXPECT_EQ(ToHebrew("123"), ResultAfter(CURSOR_RIGHT));
  EXPECT_EQ(ToHebrew("23"), ResultAfter(CURSOR_LEFT));

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(2, 2), render_text->selection());  // Collapse right.
}

TEST_F(RenderTextTest, DirectedSelections_Multiline) {
  SetGlyphWidth(5);
  RenderText* render_text = GetRenderText();

  auto ResultAfter = [&](VisualCursorDirection direction) {
    render_text->MoveCursor(CHARACTER_BREAK, direction, SELECTION_RETAIN);
    return GetSelectedText(render_text);
  };

  render_text->SetText(ASCIIToUTF16("01234\n56789\nabcde"));
  render_text->SetMultiline(true);
  render_text->SetDisplayRect(Rect(500, 500));
  ResetCursorX();

  // Test Down, then Up. LTR.
  // Undirected, or forward when kSelectionIsAlwaysDirected.
  render_text->SelectRange({2, 4});
  EXPECT_EQ(ASCIIToUTF16("23"), GetSelectedText(render_text));
  EXPECT_EQ(ASCIIToUTF16("234\n5678"), ResultAfter(CURSOR_DOWN));
  EXPECT_EQ(ASCIIToUTF16("23"), ResultAfter(CURSOR_UP));

  // Test collapsing the selection. This always ignores any existing direction.
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(2, 2), render_text->selection());  // Collapse left.

  // Undirected, or backward when kSelectionIsAlwaysDirected.
  ResetCursorX();  // Reset cached cursor x position.
  render_text->SelectRange({4, 2});
  EXPECT_EQ(ASCIIToUTF16("23"), GetSelectedText(render_text));
  if (RenderText::kSelectionIsAlwaysDirected) {
    EXPECT_EQ(ASCIIToUTF16("4\n56"), ResultAfter(CURSOR_DOWN));  // Keep left.
  } else {
    EXPECT_EQ(ASCIIToUTF16("234\n5678"),
              ResultAfter(CURSOR_DOWN));  // Pick right.
  }
  EXPECT_EQ(ASCIIToUTF16("23"), ResultAfter(CURSOR_UP));

  // Test with multi-line selection.
  // Undirected, or forward when kSelectionIsAlwaysDirected.
  ResetCursorX();
  render_text->SelectRange({2, 7});  // Select multi-line.
  EXPECT_EQ(ASCIIToUTF16("234\n5"), GetSelectedText(render_text));
  EXPECT_EQ(ASCIIToUTF16("234\n56789\na"), ResultAfter(CURSOR_DOWN));
  EXPECT_EQ(ASCIIToUTF16("234\n5"), ResultAfter(CURSOR_UP));

  // Undirected, or backward when kSelectionIsAlwaysDirected.
  ResetCursorX();
  render_text->SelectRange({7, 2});  // Select multi-line.
  EXPECT_EQ(ASCIIToUTF16("234\n5"), GetSelectedText(render_text));

  if (RenderText::kSelectionIsAlwaysDirected) {
    EXPECT_EQ(ASCIIToUTF16("6"), ResultAfter(CURSOR_DOWN));  // Keep left.
  } else {
    EXPECT_EQ(ASCIIToUTF16("234\n56789\na"),
              ResultAfter(CURSOR_DOWN));  // Pick right.
  }
  EXPECT_EQ(ASCIIToUTF16("234\n5"), ResultAfter(CURSOR_UP));
}

TEST_F(RenderTextTest, StringSizeSanity) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("Hello World"));
  const Size string_size = render_text->GetStringSize();
  EXPECT_GT(string_size.width(), 0);
  EXPECT_GT(string_size.height(), 0);
}

TEST_F(RenderTextTest, StringSizeLongStrings) {
  RenderText* render_text = GetRenderText();
  // Remove the default 100000 characters limit.
  render_text->set_truncate_length(0);
  Size previous_string_size;
  for (size_t length = 10; length < 1000000; length *= 10) {
    render_text->SetText(base::string16(length, 'a'));
    const Size string_size = render_text->GetStringSize();
    EXPECT_GT(string_size.width(), previous_string_size.width());
    EXPECT_GT(string_size.height(), 0);
    previous_string_size = string_size;
  }
}

TEST_F(RenderTextTest, StringSizeEmptyString) {
  // Ascent and descent of Arial and Symbol are different on most platforms.
  const FontList font_list(
      base::StringPrintf("Arial,%s, 16px", kSymbolFontName));
  RenderText* render_text = GetRenderText();
  render_text->SetFontList(font_list);
  render_text->SetDisplayRect(Rect(0, 0, 0, font_list.GetHeight()));

  // The empty string respects FontList metrics for non-zero height
  // and baseline.
  render_text->SetText(base::string16());
  EXPECT_EQ(font_list.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(0, render_text->GetStringSize().width());
  EXPECT_EQ(font_list.GetBaseline(), render_text->GetBaseline());

  render_text->SetText(ASCIIToUTF16(" "));
  EXPECT_EQ(font_list.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(font_list.GetBaseline(), render_text->GetBaseline());
}

TEST_F(RenderTextTest, StringSizeRespectsFontListMetrics) {
  // NOTE: On most platforms, kCJKFontName has different metrics than
  // kTestFontName, but on Android it does not.
  Font test_font(kTestFontName, 16);
  ASSERT_EQ(base::ToLowerASCII(kTestFontName),
            base::ToLowerASCII(test_font.GetActualFontName()));
  Font cjk_font(kCJKFontName, 16);
  ASSERT_EQ(base::ToLowerASCII(kCJKFontName),
            base::ToLowerASCII(cjk_font.GetActualFontName()));
  // "a" should be rendered with the test font, not with the CJK font.
  const char* test_font_text = "a";
  // "円" (U+5168 Han character YEN) should render with the CJK font, not
  // the test font.
  const char* cjk_font_text = "\u5168";
  Font smaller_font = test_font;
  Font larger_font = cjk_font;
  const char* smaller_font_text = test_font_text;
  const char* larger_font_text = cjk_font_text;
  if (cjk_font.GetHeight() < test_font.GetHeight() &&
      cjk_font.GetBaseline() < test_font.GetBaseline()) {
    std::swap(smaller_font, larger_font);
    std::swap(smaller_font_text, larger_font_text);
  }
  ASSERT_LE(smaller_font.GetHeight(), larger_font.GetHeight());
  ASSERT_LE(smaller_font.GetBaseline(), larger_font.GetBaseline());

  // Check |smaller_font_text| is rendered with the smaller font.
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16(smaller_font_text));
  render_text->SetFontList(FontList(smaller_font));
  render_text->SetDisplayRect(Rect(0, 0, 0,
                                   render_text->font_list().GetHeight()));
  EXPECT_EQ(smaller_font.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(smaller_font.GetBaseline(), render_text->GetBaseline());
  EXPECT_EQ(GetFontSpans()[0].first.GetFontName(), smaller_font.GetFontName());

  // Layout the same text with mixed fonts.  The text should be rendered with
  // the smaller font, but the height and baseline are determined with the
  // metrics of the font list, which is equal to the larger font.
  std::vector<Font> fonts;
  fonts.push_back(smaller_font);  // The primary font is the smaller font.
  fonts.push_back(larger_font);
  const FontList font_list(fonts);
  render_text->SetFontList(font_list);
  render_text->SetDisplayRect(Rect(0, 0, 0,
                                   render_text->font_list().GetHeight()));
  EXPECT_EQ(GetFontSpans()[0].first.GetFontName(), smaller_font.GetFontName());
  EXPECT_LE(smaller_font.GetHeight(), render_text->GetStringSize().height());
  EXPECT_LE(smaller_font.GetBaseline(), render_text->GetBaseline());
  EXPECT_EQ(font_list.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(font_list.GetBaseline(), render_text->GetBaseline());
}

TEST_F(RenderTextTest, StringSizeMultiline) {
  SetGlyphWidth(5);
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("Hello\nWorld"));
  const Size string_size = render_text->GetStringSize();
  EXPECT_EQ(55, string_size.width());

  render_text->SetDisplayRect(Rect(30, 1000));
  render_text->SetMultiline(true);
  EXPECT_EQ(55, render_text->TotalLineWidth());

  EXPECT_FLOAT_EQ(
      30, render_text->GetLineSizeF(SelectionModel(0, CURSOR_FORWARD)).width());
  EXPECT_FLOAT_EQ(
      25, render_text->GetLineSizeF(SelectionModel(6, CURSOR_FORWARD)).width());
  // |GetStringSize()| of multi-line text does not include newline character.
  EXPECT_EQ(25, render_text->GetStringSize().width());
  // Expect height to be 2 times the font height. This assumes simple strings
  // that do not have special metrics.
  int font_height = render_text->font_list().GetHeight();
  EXPECT_EQ(font_height * 2, render_text->GetStringSize().height());
}

TEST_F(RenderTextTest, MinLineHeight) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("Hello!"));
  SizeF default_size = render_text->GetStringSizeF();
  ASSERT_NE(0, default_size.height());
  ASSERT_NE(0, default_size.width());

  render_text->SetMinLineHeight(default_size.height() / 2);
  EXPECT_EQ(default_size.ToString(), render_text->GetStringSizeF().ToString());

  render_text->SetMinLineHeight(default_size.height() * 2);
  SizeF taller_size = render_text->GetStringSizeF();
  EXPECT_EQ(default_size.height() * 2, taller_size.height());
  EXPECT_EQ(default_size.width(), taller_size.width());
}

// Check that, for Latin characters, typesetting text in the default fonts and
// sizes does not discover any glyphs that would exceed the line spacing
// recommended by gfx::Font.
TEST_F(RenderTextTest, DefaultLineHeights) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(
      ASCIIToUTF16("A quick brown fox jumped over the lazy dog!"));

#if defined(OS_APPLE)
  const FontList body2_font = FontList().DeriveWithSizeDelta(-1);
#else
  const FontList body2_font;
#endif

  const FontList headline_font = body2_font.DeriveWithSizeDelta(8);
  const FontList title_font = body2_font.DeriveWithSizeDelta(3);
  const FontList body1_font = body2_font.DeriveWithSizeDelta(1);
#if defined(OS_WIN)
  const FontList button_font =
      body2_font.DeriveWithWeight(gfx::Font::Weight::BOLD);
#else
  const FontList button_font =
      body2_font.DeriveWithWeight(gfx::Font::Weight::MEDIUM);
#endif

  EXPECT_EQ(12, body2_font.GetFontSize());
  EXPECT_EQ(20, headline_font.GetFontSize());
  EXPECT_EQ(15, title_font.GetFontSize());
  EXPECT_EQ(13, body1_font.GetFontSize());
  EXPECT_EQ(12, button_font.GetFontSize());

  for (const auto& font :
       {headline_font, title_font, body1_font, body2_font, button_font}) {
    render_text->SetFontList(font);
    EXPECT_EQ(font.GetHeight(), render_text->GetStringSizeF().height());
  }
}

TEST_F(RenderTextTest, TextSize) {
  // Set a fractional glyph size to trigger floating rounding logic.
  const float kGlyphWidth = 1.2;
  const float kGlyphHeight = 9.2;
  SetGlyphWidth(kGlyphWidth);
  SetGlyphHeight(kGlyphHeight);

  RenderText* render_text = GetRenderText();
  for (size_t text_length = 0; text_length < 10; ++text_length) {
    render_text->SetText(ASCIIToUTF16(std::string(text_length, 'x')));

    // Ensures that conversion from float to integer ceils the values.
    const float expected_width = text_length * kGlyphWidth;
    const float expected_height = kGlyphHeight;
    const int expected_ceiled_width = std::ceil(expected_width);
    const int expected_ceiled_height = std::ceil(expected_height);

    EXPECT_FLOAT_EQ(expected_width, render_text->GetStringSizeF().width());
    EXPECT_FLOAT_EQ(expected_height, render_text->GetStringSizeF().height());
    EXPECT_EQ(expected_ceiled_width, render_text->GetStringSize().width());
    EXPECT_EQ(expected_ceiled_height, render_text->GetStringSize().height());

    EXPECT_FLOAT_EQ(expected_width, render_text->TotalLineWidth());

    // With cursor disabled, the content width is the same as string width.
    render_text->SetCursorEnabled(false);
    EXPECT_FLOAT_EQ(expected_width, render_text->GetContentWidthF());
    EXPECT_EQ(expected_ceiled_width, render_text->GetContentWidth());

    render_text->SetCursorEnabled(true);
    // The cursor is drawn one pixel beyond the int-enclosing text bounds.
    EXPECT_FLOAT_EQ(expected_ceiled_width + 1, render_text->GetContentWidthF());
    EXPECT_EQ(expected_ceiled_width + 1, render_text->GetContentWidth());
  }
}

TEST_F(RenderTextTest, TextSizeMultiline) {
  // Set a fractional glyph size to trigger floating rounding logic.
  const float kGlyphWidth = 1.2;
  const float kGlyphHeight = 9.2;
  SetGlyphWidth(kGlyphWidth);
  SetGlyphHeight(kGlyphHeight);

  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);

  for (size_t line = 0; line < 10; ++line) {
    if (line != 0)
      render_text->AppendText(ASCIIToUTF16("\n"));
    const int text_length = line;
    render_text->AppendText(ASCIIToUTF16(std::string(text_length, 'x')));

    // Ensures that conversion from float to integer ceils the values.
    const float expected_width = text_length * kGlyphWidth;
    const float expected_height = (line + 1) * kGlyphHeight;
    const int expected_ceiled_width = std::ceil(expected_width);
    const int expected_ceiled_height = std::ceil(expected_height);

    EXPECT_FLOAT_EQ(expected_width, render_text->GetStringSizeF().width());
    EXPECT_FLOAT_EQ(expected_height, render_text->GetStringSizeF().height());
    EXPECT_EQ(expected_ceiled_width, render_text->GetStringSize().width());
    EXPECT_EQ(expected_ceiled_height, render_text->GetStringSize().height());

    const int total_glyphs = render_text->text().length();
    EXPECT_FLOAT_EQ(total_glyphs * kGlyphWidth, render_text->TotalLineWidth());

    // With cursor disabled, the content width is the same as string width.
    render_text->SetCursorEnabled(false);
    EXPECT_FLOAT_EQ(expected_width, render_text->GetContentWidthF());
    EXPECT_EQ(expected_ceiled_width, render_text->GetContentWidth());

    render_text->SetCursorEnabled(true);
    // The cursor is drawn one pixel beyond the int-enclosing text bounds.
    EXPECT_FLOAT_EQ(expected_ceiled_width + 1, render_text->GetContentWidthF());
    EXPECT_EQ(expected_ceiled_width + 1, render_text->GetContentWidth());
  }
}

TEST_F(RenderTextTest, LineSizeMultiline) {
  // Set a fractional glyph size to trigger floating rounding logic.
  const float kGlyphWidth = 1.2;
  SetGlyphWidth(kGlyphWidth);

  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetText(ASCIIToUTF16("xx\nxxx\nxxxxx"));

  const float expected_line1_size = 3 * kGlyphWidth;
  const float expected_line2_size = 4 * kGlyphWidth;
  const float expected_line3_size = 5 * kGlyphWidth;

  EXPECT_FLOAT_EQ(
      expected_line1_size,
      render_text->GetLineSizeF(SelectionModel(1, CURSOR_FORWARD)).width());
  EXPECT_FLOAT_EQ(
      expected_line2_size,
      render_text->GetLineSizeF(SelectionModel(4, CURSOR_FORWARD)).width());
  EXPECT_FLOAT_EQ(
      expected_line3_size,
      render_text->GetLineSizeF(SelectionModel(10, CURSOR_FORWARD)).width());
}

TEST_F(RenderTextTest, TextPosition) {
  // Set a fractional glyph size to trigger floating rounding logic.
  // This test sets a fixed fractional width and height for glyphs to ensure
  // that computations are fixed (i.e. not font or system dependent).
  const float kGlyphWidth = 5.4;
  const float kGlyphHeight = 9.2;
  SetGlyphWidth(kGlyphWidth);
  SetGlyphHeight(kGlyphHeight);

  const int kGlyphCount = 3;

  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16(std::string(kGlyphCount, 'x')));
  render_text->SetDisplayRect(Rect(1, 1, 25, 12));
  render_text->SetCursorEnabled(false);
  render_text->SetVerticalAlignment(ALIGN_TOP);

  // Content width is 16.2px. Extra space inside display rect is 8.8px
  // (i.e. 25px - 16.2px) which is used for alignment.
  const float expected_content_width = kGlyphCount * kGlyphWidth;
  EXPECT_FLOAT_EQ(expected_content_width, render_text->GetContentWidthF());
  EXPECT_FLOAT_EQ(expected_content_width, render_text->TotalLineWidth());

  render_text->SetHorizontalAlignment(ALIGN_LEFT);
  EXPECT_EQ(1, render_text->GetLineOffset(0).x());

  EXPECT_EQ(Rect(1, 1, 6, 10), GetSubstringBoundsUnion(Range(0, 1)));
  EXPECT_EQ(Rect(6, 1, 6, 10), GetSubstringBoundsUnion(Range(1, 2)));
  EXPECT_EQ(Rect(11, 1, 7, 10), GetSubstringBoundsUnion(Range(2, 3)));

  render_text->SetHorizontalAlignment(ALIGN_CENTER);
  EXPECT_EQ(5, render_text->GetLineOffset(0).x());
  EXPECT_EQ(Rect(5, 1, 6, 10), GetSubstringBoundsUnion(Range(0, 1)));
  EXPECT_EQ(Rect(10, 1, 6, 10), GetSubstringBoundsUnion(Range(1, 2)));
  EXPECT_EQ(Rect(15, 1, 7, 10), GetSubstringBoundsUnion(Range(2, 3)));

  render_text->SetHorizontalAlignment(ALIGN_RIGHT);
  EXPECT_EQ(9, render_text->GetLineOffset(0).x());
  EXPECT_EQ(Rect(9, 1, 6, 10), GetSubstringBoundsUnion(Range(0, 1)));
  EXPECT_EQ(Rect(14, 1, 6, 10), GetSubstringBoundsUnion(Range(1, 2)));
  EXPECT_EQ(Rect(19, 1, 7, 10), GetSubstringBoundsUnion(Range(2, 3)));
}

TEST_F(RenderTextTest, SetFontList) {
  RenderText* render_text = GetRenderText();
  render_text->SetFontList(
      FontList(base::StringPrintf("Arial,%s, 13px", kSymbolFontName)));
  const std::vector<Font>& fonts = render_text->font_list().GetFonts();
  ASSERT_EQ(2U, fonts.size());
  EXPECT_EQ("Arial", fonts[0].GetFontName());
  EXPECT_EQ(kSymbolFontName, fonts[1].GetFontName());
  EXPECT_EQ(13, render_text->font_list().GetFontSize());
}

TEST_F(RenderTextTest, StringSizeBoldWidth) {
  // TODO(mboc): Add some unittests for other weights (currently not
  // implemented because of test system font configuration).
  RenderText* render_text = GetRenderText();

#if defined(OS_FUCHSIA)
  // Increase font size to ensure that bold and regular styles differ in width.
  render_text->SetFontList(FontList("Arial, 20px"));
#endif  // defined(OS_FUCHSIA)

  render_text->SetText(ASCIIToUTF16("Hello World"));

  const int plain_width = render_text->GetStringSize().width();
  EXPECT_GT(plain_width, 0);

  // Apply a bold style and check that the new width is greater.
  render_text->SetWeight(Font::Weight::BOLD);
  const int bold_width = render_text->GetStringSize().width();
  EXPECT_GT(bold_width, plain_width);

#if defined(OS_WIN)
  render_text->SetWeight(Font::Weight::SEMIBOLD);
  const int semibold_width = render_text->GetStringSize().width();
  EXPECT_GT(bold_width, semibold_width);
#endif

  // Now, apply a plain style over the first word only.
  render_text->ApplyWeight(Font::Weight::NORMAL, Range(0, 5));
  const int plain_bold_width = render_text->GetStringSize().width();
  EXPECT_GT(plain_bold_width, plain_width);
  EXPECT_LT(plain_bold_width, bold_width);
}

TEST_F(RenderTextTest, StringSizeHeight) {
  base::string16 cases[] = {
      ASCIIToUTF16("Hello World!"),  // English
      UTF8ToUTF16("\u6328\u62f6"),  // Japanese 挨拶 (characters press & near)
      UTF8ToUTF16("\u0915\u093f"),  // Hindi कि (letter KA with vowel I)
      UTF8ToUTF16("\u05e0\u05b8"),  // Hebrew נָ (letter NUN and point QAMATS)
  };

  const FontList default_font_list;
  const FontList& larger_font_list = default_font_list.DeriveWithSizeDelta(24);
  EXPECT_GT(larger_font_list.GetHeight(), default_font_list.GetHeight());

  for (size_t i = 0; i < base::size(cases); i++) {
    ResetRenderTextInstance();
    RenderText* render_text = GetRenderText();
    render_text->SetFontList(default_font_list);
    render_text->SetText(cases[i]);

    const int height1 = render_text->GetStringSize().height();
    EXPECT_GT(height1, 0);

    // Check that setting the larger font increases the height.
    render_text->SetFontList(larger_font_list);
    const int height2 = render_text->GetStringSize().height();
    EXPECT_GT(height2, height1);
  }
}

TEST_F(RenderTextTest, GetBaselineSanity) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("Hello World"));
  const int baseline = render_text->GetBaseline();
  EXPECT_GT(baseline, 0);
}

TEST_F(RenderTextTest, GetCursorBoundsInReplacementMode) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdefg"));
  render_text->SetDisplayRect(Rect(100, 17));
  SelectionModel sel_b(1, CURSOR_FORWARD);
  SelectionModel sel_c(2, CURSOR_FORWARD);
  Rect cursor_around_b = render_text->GetCursorBounds(sel_b, false);
  Rect cursor_before_b = render_text->GetCursorBounds(sel_b, true);
  Rect cursor_before_c = render_text->GetCursorBounds(sel_c, true);
  EXPECT_EQ(cursor_around_b.x(), cursor_before_b.x());
  EXPECT_EQ(cursor_around_b.right(), cursor_before_c.x());
}

TEST_F(RenderTextTest, GetCursorBoundsWithGraphemes) {
  constexpr int kGlyphWidth = 10;
  SetGlyphWidth(kGlyphWidth);
  constexpr int kGlyphHeight = 12;
  SetGlyphHeight(kGlyphHeight);

  RenderText* render_text = GetRenderText();
  render_text->SetText(
      WideToUTF16(L"a\u0300e\u0301\U0001F601x\U0001F573\uFE0F"));
  render_text->SetDisplayRect(Rect(100, 20));
  render_text->SetVerticalAlignment(ALIGN_TOP);

  static const size_t kGraphemeBoundaries[] = {0, 2, 4, 6, 7};
  for (size_t i = 0; i < base::size(kGraphemeBoundaries); ++i) {
    const size_t text_offset = kGraphemeBoundaries[i];
    EXPECT_EQ(render_text->GetCursorBounds(
                  SelectionModel(text_offset, CURSOR_FORWARD), true),
              Rect(i * kGlyphWidth, 0, 1, kGlyphHeight));
    EXPECT_EQ(render_text->GetCursorBounds(
                  SelectionModel(text_offset, CURSOR_FORWARD), false),
              Rect(i * kGlyphWidth, 0, kGlyphWidth, kGlyphHeight));
  }

  // Check cursor bounds at end of text.
  EXPECT_EQ(
      render_text->GetCursorBounds(SelectionModel(10, CURSOR_FORWARD), true),
      Rect(50, 0, 1, kGlyphHeight));
  EXPECT_EQ(
      render_text->GetCursorBounds(SelectionModel(10, CURSOR_FORWARD), false),
      Rect(50, 0, 1, kGlyphHeight));
}

TEST_F(RenderTextTest, GetTextOffset) {
  // The default horizontal text offset differs for LTR and RTL, and is only set
  // when the RenderText object is created.  This test will check the default in
  // LTR mode, and the next test will check the RTL default.
  const bool was_rtl = base::i18n::IsRTL();
  SetRTL(false);

  // Reset the render text instance since the locale was changed.
  ResetRenderTextInstance();
  RenderText* render_text = GetRenderText();

  render_text->SetText(ASCIIToUTF16("abcdefg"));
  render_text->SetFontList(FontList("Arial, 13px"));

  // Set display area's size equal to the font size.
  const Size font_size(render_text->GetContentWidth(),
                       render_text->font_list().GetHeight());
  Rect display_rect(font_size);
  render_text->SetDisplayRect(display_rect);

  Vector2d offset = render_text->GetLineOffset(0);
  EXPECT_TRUE(offset.IsZero());

  const int kEnlargementX = 2;
  display_rect.Inset(0, 0, -kEnlargementX, 0);
  render_text->SetDisplayRect(display_rect);

  // Check the default horizontal alignment.
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(0, offset.x());

  // Check explicitly setting the horizontal alignment.
  render_text->SetHorizontalAlignment(ALIGN_LEFT);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(0, offset.x());
  render_text->SetHorizontalAlignment(ALIGN_CENTER);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(kEnlargementX / 2, offset.x());
  render_text->SetHorizontalAlignment(ALIGN_RIGHT);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(kEnlargementX, offset.x());

  // Check that text is vertically centered within taller display rects.
  const int kEnlargementY = display_rect.height();
  display_rect.Inset(0, 0, 0, -kEnlargementY);
  render_text->SetDisplayRect(display_rect);
  const Vector2d prev_offset = render_text->GetLineOffset(0);
  display_rect.Inset(0, 0, 0, -2 * kEnlargementY);
  render_text->SetDisplayRect(display_rect);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(prev_offset.y() + kEnlargementY, offset.y());

  SetRTL(was_rtl);
}

TEST_F(RenderTextTest, GetTextOffsetHorizontalDefaultInRTL) {
  // This only checks the default horizontal alignment in RTL mode; all other
  // GetLineOffset(0) attributes are checked by the test above.
  const bool was_rtl = base::i18n::IsRTL();
  SetRTL(true);

  // Reset the render text instance since the locale was changed.
  ResetRenderTextInstance();
  RenderText* render_text = GetRenderText();

  render_text->SetText(ASCIIToUTF16("abcdefg"));
  render_text->SetFontList(FontList("Arial, 13px"));
  const int kEnlargement = 2;
  const Size font_size(render_text->GetContentWidth() + kEnlargement,
                       render_text->GetStringSize().height());
  Rect display_rect(font_size);
  render_text->SetDisplayRect(display_rect);
  Vector2d offset = render_text->GetLineOffset(0);
  EXPECT_EQ(kEnlargement, offset.x());
  SetRTL(was_rtl);
}

TEST_F(RenderTextTest, GetTextOffsetVerticalAlignment) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdefg"));
  render_text->SetFontList(FontList("Arial, 13px"));

  // Set display area's size equal to the font size.
  const Size font_size(render_text->GetContentWidth(),
                       render_text->font_list().GetHeight());
  Rect display_rect(font_size);
  render_text->SetDisplayRect(display_rect);

  Vector2d offset = render_text->GetLineOffset(0);
  EXPECT_TRUE(offset.IsZero());

  const int kEnlargementY = 10;
  display_rect.Inset(0, 0, 0, -kEnlargementY);
  render_text->SetDisplayRect(display_rect);

  // Check the default vertical alignment (ALIGN_MIDDLE).
  offset = render_text->GetLineOffset(0);
  // Because the line height may be odd, and because of the way this calculation
  // is done, we will accept a result within 1 DIP of half:
  EXPECT_NEAR(kEnlargementY / 2, offset.y(), 1);

  // Check explicitly setting the vertical alignment.
  render_text->SetVerticalAlignment(ALIGN_TOP);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(0, offset.y());
  render_text->SetVerticalAlignment(ALIGN_MIDDLE);
  offset = render_text->GetLineOffset(0);
  EXPECT_NEAR(kEnlargementY / 2, offset.y(), 1);
  render_text->SetVerticalAlignment(ALIGN_BOTTOM);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(kEnlargementY, offset.y());
}

TEST_F(RenderTextTest, GetTextOffsetVerticalAlignment_Multiline) {
  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetMaxLines(2);
  render_text->SetText(ASCIIToUTF16("abcdefg hijklmn"));
  render_text->SetFontList(FontList("Arial, 13px"));

  // Set display area's size equal to the font size.
  const Size font_size(render_text->GetContentWidth(),
                       render_text->font_list().GetHeight());

  // Force text onto two lines.
  Rect display_rect(Size(font_size.width() * 2 / 3, font_size.height() * 2));
  render_text->SetDisplayRect(display_rect);

  Vector2d offset = render_text->GetLineOffset(0);
  EXPECT_TRUE(offset.IsZero());

  const int kEnlargementY = 10;
  display_rect.Inset(0, 0, 0, -kEnlargementY);
  render_text->SetDisplayRect(display_rect);

  // Check the default vertical alignment (ALIGN_MIDDLE).
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(kEnlargementY / 2, offset.y());

  // Check explicitly setting the vertical alignment.
  render_text->SetVerticalAlignment(ALIGN_TOP);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(0, offset.y());
  render_text->SetVerticalAlignment(ALIGN_MIDDLE);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(kEnlargementY / 2, offset.y());
  render_text->SetVerticalAlignment(ALIGN_BOTTOM);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(kEnlargementY, offset.y());
}

TEST_F(RenderTextTest, SetDisplayOffset) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdefg"));
  render_text->SetFontList(FontList("Arial, 13px"));

  const Size font_size(render_text->GetContentWidth(),
                       render_text->font_list().GetHeight());
  const int kEnlargement = 10;

  // Set display width |kEnlargement| pixels greater than content width and test
  // different possible situations. In this case the only possible display
  // offset is zero.
  Rect display_rect(font_size);
  display_rect.Inset(0, 0, -kEnlargement, 0);
  render_text->SetDisplayRect(display_rect);

  struct {
    HorizontalAlignment alignment;
    int offset;
  } small_content_cases[] = {
    { ALIGN_LEFT, -kEnlargement },
    { ALIGN_LEFT, 0 },
    { ALIGN_LEFT, kEnlargement },
    { ALIGN_RIGHT, -kEnlargement },
    { ALIGN_RIGHT, 0 },
    { ALIGN_RIGHT, kEnlargement },
    { ALIGN_CENTER, -kEnlargement },
    { ALIGN_CENTER, 0 },
    { ALIGN_CENTER, kEnlargement },
  };

  for (size_t i = 0; i < base::size(small_content_cases); i++) {
    render_text->SetHorizontalAlignment(small_content_cases[i].alignment);
    render_text->SetDisplayOffset(small_content_cases[i].offset);
    EXPECT_EQ(0, render_text->GetUpdatedDisplayOffset().x());
  }

  // Set display width |kEnlargement| pixels less than content width and test
  // different possible situations.
  display_rect = Rect(font_size);
  display_rect.Inset(0, 0, kEnlargement, 0);
  render_text->SetDisplayRect(display_rect);

  struct {
    HorizontalAlignment alignment;
    int offset;
    int expected_offset;
  } large_content_cases[] = {
    // When text is left-aligned, display offset can be in range
    // [-kEnlargement, 0].
    { ALIGN_LEFT, -2 * kEnlargement, -kEnlargement },
    { ALIGN_LEFT, -kEnlargement / 2, -kEnlargement / 2 },
    { ALIGN_LEFT, kEnlargement, 0 },
    // When text is right-aligned, display offset can be in range
    // [0, kEnlargement].
    { ALIGN_RIGHT, -kEnlargement, 0 },
    { ALIGN_RIGHT, kEnlargement / 2, kEnlargement / 2 },
    { ALIGN_RIGHT, 2 * kEnlargement, kEnlargement },
    // When text is center-aligned, display offset can be in range
    // [-kEnlargement / 2 - 1, (kEnlargement - 1) / 2].
    { ALIGN_CENTER, -kEnlargement, -kEnlargement / 2 - 1 },
    { ALIGN_CENTER, -kEnlargement / 4, -kEnlargement / 4 },
    { ALIGN_CENTER, kEnlargement / 4, kEnlargement / 4 },
    { ALIGN_CENTER, kEnlargement, (kEnlargement - 1) / 2 },
  };

  for (size_t i = 0; i < base::size(large_content_cases); i++) {
    render_text->SetHorizontalAlignment(large_content_cases[i].alignment);
    render_text->SetDisplayOffset(large_content_cases[i].offset);
    EXPECT_EQ(large_content_cases[i].expected_offset,
              render_text->GetUpdatedDisplayOffset().x());
  }
}

TEST_F(RenderTextTest, SameFontForParentheses) {
  struct {
    const base::char16 left_char;
    const base::char16 right_char;
  } punctuation_pairs[] = {
    { '(', ')' },
    { '{', '}' },
    { '<', '>' },
  };
  struct {
    base::string16 text;
  } cases[] = {
      // English(English)
      {ASCIIToUTF16("Hello World(a)")},
      // English(English)English
      {ASCIIToUTF16("Hello World(a)Hello World")},

      // Japanese(English)
      {UTF8ToUTF16("\u6328\u62f6(a)")},
      // Japanese(English)Japanese
      {UTF8ToUTF16("\u6328\u62f6(a)\u6328\u62f6")},
      // English(Japanese)English
      {UTF8ToUTF16("Hello World(\u6328\u62f6)Hello World")},

      // Hindi(English)
      {UTF8ToUTF16("\u0915\u093f(a)")},
      // Hindi(English)Hindi
      {UTF8ToUTF16("\u0915\u093f(a)\u0915\u093f")},
      // English(Hindi)English
      {UTF8ToUTF16("Hello World(\u0915\u093f)Hello World")},

      // Hebrew(English)
      {UTF8ToUTF16("\u05e0\u05b8(a)")},
      // Hebrew(English)Hebrew
      {UTF8ToUTF16("\u05e0\u05b8(a)\u05e0\u05b8")},
      // English(Hebrew)English
      {UTF8ToUTF16("Hello World(\u05e0\u05b8)Hello World")},
  };

  RenderText* render_text = GetRenderText();
  for (size_t i = 0; i < base::size(cases); ++i) {
    base::string16 text = cases[i].text;
    const size_t start_paren_char_index = text.find('(');
    ASSERT_NE(base::string16::npos, start_paren_char_index);
    const size_t end_paren_char_index = text.find(')');
    ASSERT_NE(base::string16::npos, end_paren_char_index);

    for (size_t j = 0; j < base::size(punctuation_pairs); ++j) {
      text[start_paren_char_index] = punctuation_pairs[j].left_char;
      text[end_paren_char_index] = punctuation_pairs[j].right_char;
      render_text->SetText(text);

      const std::vector<FontSpan> spans = GetFontSpans();

      int start_paren_span_index = -1;
      int end_paren_span_index = -1;
      for (size_t k = 0; k < spans.size(); ++k) {
        if (IndexInRange(spans[k].second, start_paren_char_index))
          start_paren_span_index = k;
        if (IndexInRange(spans[k].second, end_paren_char_index))
          end_paren_span_index = k;
      }
      ASSERT_NE(-1, start_paren_span_index);
      ASSERT_NE(-1, end_paren_span_index);

      const Font& start_font = spans[start_paren_span_index].first;
      const Font& end_font = spans[end_paren_span_index].first;
      EXPECT_EQ(start_font.GetFontName(), end_font.GetFontName());
      EXPECT_EQ(start_font.GetFontSize(), end_font.GetFontSize());
      EXPECT_EQ(start_font.GetStyle(), end_font.GetStyle());
    }
  }
}

// Make sure the caret width is always >=1 so that the correct
// caret is drawn at high DPI. crbug.com/164100.
TEST_F(RenderTextTest, CaretWidth) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdefg"));
  EXPECT_GE(render_text->GetUpdatedCursorBounds().width(), 1);
}

TEST_F(RenderTextTest, SelectWord) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16(" foo  a.bc.d bar"));

  struct {
    size_t cursor;
    size_t selection_start;
    size_t selection_end;
  } cases[] = {
    { 0,   0,  1 },
    { 1,   1,  4 },
    { 2,   1,  4 },
    { 3,   1,  4 },
    { 4,   4,  6 },
    { 5,   4,  6 },
    { 6,   6,  7 },
    { 7,   7,  8 },
    { 8,   8, 10 },
    { 9,   8, 10 },
    { 10, 10, 11 },
    { 11, 11, 12 },
    { 12, 12, 13 },
    { 13, 13, 16 },
    { 14, 13, 16 },
    { 15, 13, 16 },
    { 16, 13, 16 },
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    render_text->SetCursorPosition(cases[i].cursor);
    render_text->SelectWord();
    EXPECT_EQ(Range(cases[i].selection_start, cases[i].selection_end),
              render_text->selection());
  }
}

// Make sure the last word is selected when the cursor is at text.length().
TEST_F(RenderTextTest, LastWordSelected) {
  const std::string kTestURL1 = "http://www.google.com";
  const std::string kTestURL2 = "http://www.google.com/something/";

  RenderText* render_text = GetRenderText();

  render_text->SetText(ASCIIToUTF16(kTestURL1));
  render_text->SetCursorPosition(kTestURL1.length());
  render_text->SelectWord();
  EXPECT_EQ(ASCIIToUTF16("com"), GetSelectedText(render_text));
  EXPECT_FALSE(render_text->selection().is_reversed());

  render_text->SetText(ASCIIToUTF16(kTestURL2));
  render_text->SetCursorPosition(kTestURL2.length());
  render_text->SelectWord();
  EXPECT_EQ(ASCIIToUTF16("/"), GetSelectedText(render_text));
  EXPECT_FALSE(render_text->selection().is_reversed());
}

// When given a non-empty selection, SelectWord should expand the selection to
// nearest word boundaries.
TEST_F(RenderTextTest, SelectMultipleWords) {
  const std::string kTestURL = "http://www.google.com";

  RenderText* render_text = GetRenderText();

  render_text->SetText(ASCIIToUTF16(kTestURL));
  render_text->SelectRange(Range(16, 20));
  render_text->SelectWord();
  EXPECT_EQ(ASCIIToUTF16("google.com"), GetSelectedText(render_text));
  EXPECT_FALSE(render_text->selection().is_reversed());

  // SelectWord should preserve the selection direction.
  render_text->SelectRange(Range(20, 16));
  render_text->SelectWord();
  EXPECT_EQ(ASCIIToUTF16("google.com"), GetSelectedText(render_text));
  EXPECT_TRUE(render_text->selection().is_reversed());
}

TEST_F(RenderTextTest, DisplayRectShowsCursorLTR) {
  ASSERT_FALSE(base::i18n::IsRTL());
  ASSERT_FALSE(base::i18n::ICUIsRTL());

  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdefghijklmnopqrstuvwxzyabcdefg"));
  render_text->SetCursorPosition(render_text->text().length());
  int width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(render_text->display_rect().width(),
            render_text->GetUpdatedCursorBounds().right());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(render_text->display_rect().width(),
            render_text->GetUpdatedCursorBounds().right());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());

  // Repeat the test with RTL text.
  render_text->SetText(
      UTF8ToUTF16("\u05d0\u05d1\u05d2\u05d3\u05d4\u05d5\u05d6\u05d7"
                  "\u05d8\u05d9\u05da\u05db\u05dc\u05dd\u05de\u05df"));
  render_text->SetCursorPosition(0);
  width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(render_text->display_rect().width(),
            render_text->GetUpdatedCursorBounds().right());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(render_text->display_rect().width(),
            render_text->GetUpdatedCursorBounds().right());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());
}

TEST_F(RenderTextTest, DisplayRectShowsCursorRTL) {
  // Set the application default text direction to RTL.
  const bool was_rtl = base::i18n::IsRTL();
  SetRTL(true);

  // Reset the render text instance since the locale was changed.
  ResetRenderTextInstance();
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdefghijklmnopqrstuvwxzyabcdefg"));
  render_text->SetCursorPosition(0);
  int width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Repeat the test with RTL text.
  render_text->SetText(
      UTF8ToUTF16("\u05d0\u05d1\u05d2\u05d3\u05d4\u05d5\u05d6\u05d7"
                  "\u05d8\u05d9\u05da\u05db\u05dc\u05dd\u05de\u05df"));
  render_text->SetCursorPosition(render_text->text().length());
  width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Reset the application default text direction to LTR.
  SetRTL(was_rtl);
  EXPECT_EQ(was_rtl, base::i18n::IsRTL());
}

// Changing colors between or inside ligated glyphs should not break shaping.
TEST_F(RenderTextTest, SelectionKeepsLigatures) {
  const char* kTestStrings[] = {"\u0644\u0623", "\u0633\u0627"};
  RenderText* render_text = GetRenderText();
  render_text->set_selection_color(SK_ColorRED);

  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    const int expected_width = render_text->GetStringSize().width();
    render_text->SelectRange({0, 1});
    EXPECT_EQ(expected_width, render_text->GetStringSize().width());
    // Drawing the text should not DCHECK or crash; see http://crbug.com/262119
    render_text->Draw(canvas());
    render_text->SetCursorPosition(0);
  }
}

// Test that characters commonly used in the context of several scripts do not
// cause text runs to break. For example the Japanese "long sound symbol" --
// normally only used in a Katakana script, is also used on occasion when in
// Hiragana scripts. It shouldn't cause a Hiragana text run break since that
// could upset kerning.
TEST_F(RenderTextTest, ScriptExtensionsDoNotBreak) {
  // Apparently ramen restaurants prefer "らーめん" over "らあめん". The "dash"
  // is the long sound symbol and usually just appears in Katakana writing.
  const base::string16 ramen_hiragana = UTF8ToUTF16("らーめん");
  const base::string16 ramen_katakana = UTF8ToUTF16("ラーメン");
  const base::string16 ramen_mixed = UTF8ToUTF16("らあメン");

  EXPECT_EQ(std::vector<base::string16>({ramen_hiragana}),
            RunsFor(ramen_hiragana));
  EXPECT_EQ(std::vector<base::string16>({ramen_katakana}),
            RunsFor(ramen_katakana));

  EXPECT_EQ(ToString16Vec({"らあ", "メン"}), RunsFor(ramen_mixed));
}

// Test that whitespace breaks runs of text. E.g. this can permit better fonts
// to be chosen by the fallback mechanism when a font does not provide
// whitespace glyphs for all scripts. See http://crbug.com/731563.
TEST_F(RenderTextTest, WhitespaceDoesBreak) {
  // Title of the Wikipedia page for "bit". ASCII spaces. In Hebrew and English.
  // Note that the hyphens that Wikipedia uses are different. English uses
  // ASCII (U+002D) "hyphen minus", Hebrew uses the U+2013 "EN Dash".
  const base::string16 ascii_space_he = UTF8ToUTF16("סיבית – ויקיפדיה");
  const base::string16 ascii_space_en = ASCIIToUTF16("Bit - Wikipedia");

  // This says "thank you very much" with a full-width non-ascii space (U+3000).
  const base::string16 full_width_space = UTF8ToUTF16("ども　ありがと");

  EXPECT_EQ(ToString16Vec({"סיבית", " ", "–", " ", "ויקיפדיה"}),
            RunsFor(ascii_space_he));
  EXPECT_EQ(ToString16Vec({"Bit", " ", "-", " ", "Wikipedia"}),
            RunsFor(ascii_space_en));
  EXPECT_EQ(ToString16Vec({"ども", "　", "ありがと"}),
            RunsFor(full_width_space));
}

// Ensure strings wrap onto multiple lines for a small available width.
TEST_F(RenderTextTest, Multiline_MinWidth) {
  const char* kTestStrings[] = {kWeak, kLtr,    kLtrRtl,   kLtrRtlLtr,
                                kRtl,  kRtlLtr, kRtlLtrRtl};

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(1, 1000));
  render_text->SetMultiline(true);
  render_text->SetWordWrapBehavior(WRAP_LONG_WORDS);

  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    render_text->Draw(canvas());
    EXPECT_GT(test_api()->lines().size(), 1U);
  }
}

// Ensure strings wrap onto multiple lines for a normal available width.
TEST_F(RenderTextTest, Multiline_NormalWidth) {
  // Should RenderText suppress drawing whitespace at the end of a line?
  // Currently it does not.
  const struct {
    const char* const text;
    const Range first_line_char_range;
    const Range second_line_char_range;

    // Lengths of each text run. Runs break at whitespace.
    std::vector<size_t> run_lengths;

    // The index of the text run that should start the second line.
    int second_line_run_index;

    bool is_ltr;
  } kTestStrings[] = {
      {"abc defg hijkl", Range(0, 9), Range(9, 14), {3, 1, 4, 1, 5}, 4, true},
      {"qwertyzxcvbn", Range(0, 10), Range(10, 12), {10, 2}, 1, true},
      // RTL: should render left-to-right as "<space>43210 \n cba9876".
      // Note this used to say "Arabic language", in Arabic, but the last
      // character in the string (\u0629) got fancy in an updated Mac font, so
      // now the penultimate character repeats. (See "NOTE" below).
      {"\u0627\u0644\u0644\u063A\u0629 "
       "\u0627\u0644\u0639\u0631\u0628\u064A\u064A",
       Range(0, 6),
       Range(6, 13),
       {1 /* space first */, 5, 7},
       2,
       false},
      // RTL: should render left-to-right as "<space>3210 \n cba98765".
      {"\u062A\u0641\u0627\u062D \u05EA\u05E4\u05D5\u05D6\u05D9"
       "\u05DA\u05DB\u05DD",
       Range(0, 5),
       Range(5, 13),
       {1 /* space first */, 5, 8},
       2,
       false}};

  RenderTextHarfBuzz* render_text = GetRenderText();

  // Specify the fixed width for characters to suppress the possible variations
  // of linebreak results.
  SetGlyphWidth(5);
  render_text->SetDisplayRect(Rect(50, 1000));
  render_text->SetMultiline(true);
  render_text->SetWordWrapBehavior(WRAP_LONG_WORDS);
  render_text->SetHorizontalAlignment(ALIGN_TO_HEAD);

  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i].text));
    DrawVisualText();

    ASSERT_EQ(2U, test_api()->lines().size());
    EXPECT_EQ(kTestStrings[i].first_line_char_range,
              LineCharRange(test_api()->lines()[0]));
    EXPECT_EQ(kTestStrings[i].second_line_char_range,
              LineCharRange(test_api()->lines()[1]));

    ASSERT_EQ(kTestStrings[i].run_lengths.size(), text_log().size());

    // NOTE: this expectation compares the character length and glyph counts,
    // which isn't always equal. This is okay only because all the test
    // strings are simple (like, no compound characters nor UTF16-surrogate
    // pairs). Be careful in case more complicated test strings are added.
    EXPECT_EQ(kTestStrings[i].run_lengths[0], text_log()[0].glyphs().size());
    const int second_line_start = kTestStrings[i].second_line_run_index;
    EXPECT_EQ(kTestStrings[i].run_lengths[second_line_start],
              text_log()[second_line_start].glyphs().size());
    EXPECT_LT(text_log()[0].origin().y(),
              text_log()[second_line_start].origin().y());
    if (kTestStrings[i].is_ltr) {
      EXPECT_EQ(0, text_log()[0].origin().x());
      EXPECT_EQ(0, text_log()[second_line_start].origin().x());
    } else {
      EXPECT_LT(0, text_log()[0].origin().x());
      EXPECT_LT(0, text_log()[second_line_start].origin().x());
    }
  }
}

// Ensure strings don't wrap onto multiple lines for a sufficient available
// width.
TEST_F(RenderTextTest, Multiline_SufficientWidth) {
  const char* kTestStrings[] = {"", " ", ".", " . ", "abc", "a b c",
                                "\u062E\u0628\u0632", "\u062E \u0628 \u0632"};

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(1000, 1000));
  render_text->SetMultiline(true);

  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    render_text->Draw(canvas());
    EXPECT_EQ(1U, test_api()->lines().size());
  }
}

TEST_F(RenderTextTest, Multiline_Newline) {
  const struct {
    const char* const text;
    const size_t lines_count;
    // Ranges of the characters on each line.
    const Range line_char_ranges[3];
  } kTestStrings[] = {
      {"abc\ndef", 2ul, {Range(0, 4), Range(4, 7), Range::InvalidRange()}},
      {"a \n b ", 2ul, {Range(0, 3), Range(3, 6), Range::InvalidRange()}},
      {"ab\n", 2ul, {Range(0, 3), Range(), Range::InvalidRange()}},
      {"a\n\nb", 3ul, {Range(0, 2), Range(2, 3), Range(3, 4)}},
      {"\nab", 2ul, {Range(0, 1), Range(1, 3), Range::InvalidRange()}},
      {"\n", 2ul, {Range(0, 1), Range(), Range::InvalidRange()}},
  };

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(200, 1000));
  render_text->SetMultiline(true);

  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i].text));
    render_text->Draw(canvas());
    EXPECT_EQ(kTestStrings[i].lines_count, test_api()->lines().size());
    if (kTestStrings[i].lines_count != test_api()->lines().size())
      continue;

    for (size_t j = 0; j < kTestStrings[i].lines_count; ++j) {
      SCOPED_TRACE(base::StringPrintf("Line %" PRIuS "", j));
      // There might be multiple segments in one line. Merge all the segments
      // ranges in the same line.
      const size_t segment_size = test_api()->lines()[j].segments.size();
      Range line_range;
      if (segment_size > 0)
        line_range = Range(
            test_api()->lines()[j].segments[0].char_range.start(),
            test_api()->lines()[j].segments[segment_size - 1].char_range.end());
      EXPECT_EQ(kTestStrings[i].line_char_ranges[j], line_range);
    }
  }
}

// Make sure that multiline mode ignores elide behavior.
TEST_F(RenderTextTest, Multiline_IgnoreElide) {
  const char kTestString[] =
      "very very very long string xxxxxxxxxxxxxxxxxxxxxxxxxx";
  const char kEllipsis[] = "\u2026";

  RenderText* render_text = GetRenderText();
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetDisplayRect(Rect(20, 1000));
  render_text->SetText(base::ASCIIToUTF16(kTestString));
  EXPECT_NE(base::string16::npos,
            render_text->GetDisplayText().find(base::UTF8ToUTF16(kEllipsis)));

  render_text->SetMultiline(true);
  EXPECT_EQ(base::string16::npos,
            render_text->GetDisplayText().find(base::UTF8ToUTF16(kEllipsis)));
}

TEST_F(RenderTextTest, Multiline_NewlineCharacterReplacement) {
  const char* kTestStrings[] = {
      "abc\ndef", "a \n b ", "ab\n", "a\n\nb", "\nab", "\n",
  };

  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    ResetRenderTextInstance();
    RenderText* render_text = GetRenderText();
    render_text->SetDisplayRect(Rect(200, 1000));
    render_text->SetText(ASCIIToUTF16(kTestStrings[i]));

    base::string16 display_text = render_text->GetDisplayText();
    // If RenderText is not multiline, the newline characters are replaced
    // by symbols, therefore the character should be changed.
    EXPECT_NE(ASCIIToUTF16(kTestStrings[i]), render_text->GetDisplayText());

    // Setting multiline will fix this, the newline characters will be back
    // to the original text.
    render_text->SetMultiline(true);
    EXPECT_EQ(ASCIIToUTF16(kTestStrings[i]), render_text->GetDisplayText());
  }
}

// Ensure horizontal alignment works in multiline mode.
TEST_F(RenderTextTest, Multiline_HorizontalAlignment) {
  constexpr struct {
    const char* const text;
    const HorizontalAlignment alignment;
    const base::i18n::TextDirection display_text_direction;
  } kTestStrings[] = {
      {"abcdefghi\nhijk", ALIGN_LEFT, base::i18n::LEFT_TO_RIGHT},
      {"nhij\nabcdefghi", ALIGN_LEFT, base::i18n::LEFT_TO_RIGHT},
      // Hebrew, 2nd line shorter
      {"\u05d0\u05d1\u05d2\u05d3\u05d4\u05d5\u05d6\u05d7\n"
       "\u05d0\u05d1\u05d2\u05d3",
       ALIGN_RIGHT,
       base::i18n::RIGHT_TO_LEFT},
      // Hebrew, 2nd line longer
      {"\u05d0\u05d1\u05d2\u05d3\n"
       "\u05d0\u05d1\u05d2\u05d3\u05d4\u05d5\u05d6\u05d7",
       ALIGN_RIGHT,
       base::i18n::RIGHT_TO_LEFT},
      // Arabic, 2nd line shorter.
      {"\u0627\u0627\u0627\u0627\u0627\u0627\u0627\u0627\n"
       "\u0627\u0644\u0644\u063A",
       ALIGN_RIGHT,
       base::i18n::RIGHT_TO_LEFT},
      // Arabic, 2nd line longer.
      {"\u0627\u0644\u0644\u063A\n"
       "\u0627\u0627\u0627\u0627\u0627\u0627\u0627\u0627",
       ALIGN_RIGHT,
       base::i18n::RIGHT_TO_LEFT},
  };
  const int kGlyphSize = 5;
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetHorizontalAlignment(ALIGN_TO_HEAD);
  SetGlyphWidth(kGlyphSize);
  render_text->SetDisplayRect(Rect(100, 1000));
  render_text->SetMultiline(true);

  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    SCOPED_TRACE(testing::Message("kTestStrings[")
                 << i << "] = " << kTestStrings[i].text);
    render_text->SetText(UTF8ToUTF16(kTestStrings[i].text));
    EXPECT_EQ(kTestStrings[i].display_text_direction,
              render_text->GetDisplayTextDirection());
    render_text->Draw(canvas());
    ASSERT_LE(2u, test_api()->lines().size());
    if (kTestStrings[i].alignment == ALIGN_LEFT) {
      EXPECT_EQ(0, test_api()->GetAlignmentOffset(0).x());
      EXPECT_EQ(0, test_api()->GetAlignmentOffset(1).x());
    } else {
      std::vector<base::string16> lines = base::SplitString(
          base::UTF8ToUTF16(kTestStrings[i].text), base::string16(1, '\n'),
          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      ASSERT_EQ(2u, lines.size());

      // Sanity check the input string lengths match the glyph lengths.
      EXPECT_EQ(4u, std::min(lines[0].length(), lines[1].length()));
      EXPECT_EQ(8u, std::max(lines[0].length(), lines[1].length()));
      const internal::TextRunList* run_list = GetHarfBuzzRunList();
      ASSERT_EQ(3U, run_list->runs().size());
      EXPECT_EQ(lines[0].length(), run_list->runs()[0]->shape.glyph_count);
      EXPECT_EQ(1u, run_list->runs()[1]->shape.glyph_count);  // \n.
      EXPECT_EQ(lines[1].length(), run_list->runs()[2]->shape.glyph_count);

      int difference = (lines[0].length() - lines[1].length() + 1) * kGlyphSize;
      EXPECT_EQ(test_api()->GetAlignmentOffset(0).x() + difference,
                test_api()->GetAlignmentOffset(1).x());
    }
  }
}

TEST_F(RenderTextTest, Multiline_WordWrapBehavior) {
  const int kGlyphSize = 5;
  const struct {
    const WordWrapBehavior behavior;
    const size_t num_lines;
    const Range char_ranges[4];
  } kTestScenarios[] = {
    { IGNORE_LONG_WORDS, 3u,
      { Range(0, 4), Range(4, 11), Range(11, 14), Range::InvalidRange() } },
    { TRUNCATE_LONG_WORDS, 3u,
      { Range(0, 4), Range(4, 8), Range(11, 14), Range::InvalidRange() } },
    { WRAP_LONG_WORDS, 4u,
      { Range(0, 4), Range(4, 8), Range(8, 11), Range(11, 14) } },
    // TODO(mukai): implement ELIDE_LONG_WORDS. It's not used right now.
  };
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetText(ASCIIToUTF16("foo fooooo foo"));
  SetGlyphWidth(kGlyphSize);
  render_text->SetDisplayRect(Rect(0, 0, kGlyphSize * 4, 0));

  for (size_t i = 0; i < base::size(kTestScenarios); ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "kTestScenarios[%" PRIuS "] %d", i, kTestScenarios[i].behavior));
    render_text->SetWordWrapBehavior(kTestScenarios[i].behavior);
    render_text->Draw(canvas());

    ASSERT_EQ(kTestScenarios[i].num_lines, test_api()->lines().size());
    for (size_t j = 0; j < test_api()->lines().size(); ++j) {
      SCOPED_TRACE(base::StringPrintf("%" PRIuS "-th line", j));
      EXPECT_EQ(kTestScenarios[i].char_ranges[j],
                LineCharRange(test_api()->lines()[j]));
      EXPECT_EQ(kTestScenarios[i].char_ranges[j].length() * kGlyphSize,
                test_api()->lines()[j].size.width());
    }
  }
}

TEST_F(RenderTextTest, Multiline_LineBreakerBehavior) {
  const int kGlyphSize = 5;
  const struct {
    const char* const text;
    const WordWrapBehavior behavior;
    const Range char_ranges[3];
  } kTestScenarios[] = {
      {"a single run",
       IGNORE_LONG_WORDS,
       {Range(0, 2), Range(2, 9), Range(9, 12)}},
      // 3 words: "That's ", ""good". ", "aaa" and 7 runs: "That", "'", "s ",
      // """, "good", "". ", "aaa". They all mixed together.
      {"That's \"good\". aaa", IGNORE_LONG_WORDS,
       {Range(0, 7), Range(7, 15), Range(15, 18)}},
      // Test "\"" should be put into a new line correctly.
      {"a \"good\" one.", IGNORE_LONG_WORDS,
       {Range(0, 2), Range(2, 9), Range(9, 13)}},
      // Test for full-width space.
      {"That's\u3000good.\u3000yyy", IGNORE_LONG_WORDS,
       {Range(0, 7), Range(7, 13), Range(13, 16)}},
      {"a single run", TRUNCATE_LONG_WORDS,
       {Range(0, 2), Range(2, 6), Range(9, 12)}},
      {"That's \"good\". aaa", TRUNCATE_LONG_WORDS,
       {Range(0, 4), Range(7, 11), Range(15, 18)}},
      {"That's good. aaa", TRUNCATE_LONG_WORDS,
       {Range(0, 4), Range(7, 11), Range(13, 16)}},
      {"a \"good\" one.", TRUNCATE_LONG_WORDS,
       {Range(0, 2), Range(2, 6), Range(9, 13)}},
      {"asingleword", WRAP_LONG_WORDS,
       {Range(0, 4), Range(4, 8), Range(8, 11)}},
      {"That's good", WRAP_LONG_WORDS,
       {Range(0, 4), Range(4, 7), Range(7, 11)}},
      {"That's \"g\".", WRAP_LONG_WORDS,
       {Range(0, 4), Range(4, 7), Range(7, 11)}},
  };

  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetMultiline(true);
  SetGlyphWidth(kGlyphSize);
  render_text->SetDisplayRect(Rect(0, 0, kGlyphSize * 4, 0));

  for (size_t i = 0; i < base::size(kTestScenarios); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestScenarios[i].text));
    render_text->SetWordWrapBehavior(kTestScenarios[i].behavior);
    render_text->Draw(canvas());

    ASSERT_EQ(3u, test_api()->lines().size());
    for (size_t j = 0; j < test_api()->lines().size(); ++j) {
      SCOPED_TRACE(base::StringPrintf("%" PRIuS "-th line", j));
      // Merge all the segments ranges in the same line.
      size_t segment_size = test_api()->lines()[j].segments.size();
      Range line_range;
      if (segment_size > 0)
        line_range = Range(
            test_api()->lines()[j].segments[0].char_range.start(),
            test_api()->lines()[j].segments[segment_size - 1].char_range.end());
      EXPECT_EQ(kTestScenarios[i].char_ranges[j], line_range);
      // Depending on kerning pairs in the font, a run's length is not strictly
      // equal to number of glyphs * kGlyphsize. Size should match within a
      // small margin.
      const float kSizeMargin = 0.127;
      EXPECT_LT(
          std::abs(kTestScenarios[i].char_ranges[j].length() * kGlyphSize -
                   test_api()->lines()[j].size.width()),
          kSizeMargin);
    }
  }
}

// Test that Surrogate pairs or combining character sequences do not get
// separated by line breaking.
TEST_F(RenderTextTest, Multiline_SurrogatePairsOrCombiningChars) {
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetWordWrapBehavior(WRAP_LONG_WORDS);

  // Below is 'MUSICAL SYMBOL G CLEF' (U+1D11E), which is represented in UTF-16
  // as two code units forming a surrogate pair: 0xD834 0xDD1E.
  const base::char16 kSurrogate[] = {0xD834, 0xDD1E, 0};
  const base::string16 text_surrogate(kSurrogate);
  const int kSurrogateWidth =
      GetStringWidth(kSurrogate, render_text->font_list());

  // Below is a Devanagari two-character combining sequence U+0921 U+093F. The
  // sequence forms a single display character and should not be separated.
  const base::char16 kCombiningChars[] = {0x921, 0x93F, 0};
  const base::string16 text_combining(kCombiningChars);
  const int kCombiningCharsWidth =
      GetStringWidth(kCombiningChars, render_text->font_list());

  const struct {
    const base::string16 text;
    const int display_width;
    const Range char_ranges[3];
  } kTestScenarios[] = {
      {text_surrogate + text_surrogate + text_surrogate,
       kSurrogateWidth / 2 * 3,
       {Range(0, 2), Range(2, 4), Range(4, 6)}},
      {text_surrogate + ASCIIToUTF16(" ") + kCombiningChars,
       std::min(kSurrogateWidth, kCombiningCharsWidth) / 2,
       {Range(0, 2), Range(2, 3), Range(3, 5)}},
  };

  for (size_t i = 0; i < base::size(kTestScenarios); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(kTestScenarios[i].text);
    render_text->SetDisplayRect(Rect(0, 0, kTestScenarios[i].display_width, 0));
    render_text->Draw(canvas());

    ASSERT_EQ(3u, test_api()->lines().size());
    for (size_t j = 0; j < test_api()->lines().size(); ++j) {
      SCOPED_TRACE(base::StringPrintf("%" PRIuS "-th line", j));
      // There is only one segment in each line.
      EXPECT_EQ(kTestScenarios[i].char_ranges[j],
                test_api()->lines()[j].segments[0].char_range);
    }
  }
}

// Test that Zero width characters have the correct line breaking behavior.
TEST_F(RenderTextTest, Multiline_ZeroWidthChars) {
  RenderTextHarfBuzz* render_text = GetRenderText();

  render_text->SetMultiline(true);
  render_text->SetWordWrapBehavior(WRAP_LONG_WORDS);

  const base::char16 kZeroWidthSpace = {0x200B};
  const base::string16 text(ASCIIToUTF16("test") + kZeroWidthSpace +
                            ASCIIToUTF16("\n") + kZeroWidthSpace +
                            ASCIIToUTF16("test."));
  const int kTestWidth =
      GetStringWidth(ASCIIToUTF16("test"), render_text->font_list());
  const Range char_ranges[3] = {Range(0, 6), Range(6, 11), Range(11, 12)};

  render_text->SetText(text);
  render_text->SetDisplayRect(Rect(0, 0, kTestWidth, 0));
  render_text->Draw(canvas());

  EXPECT_EQ(3u, test_api()->lines().size());
  for (size_t j = 0;
       j < std::min(base::size(char_ranges), test_api()->lines().size()); ++j) {
    SCOPED_TRACE(base::StringPrintf("%" PRIuS "-th line", j));
    int segment_size = test_api()->lines()[j].segments.size();
    ASSERT_GT(segment_size, 0);
    Range line_range(
        test_api()->lines()[j].segments[0].char_range.start(),
        test_api()->lines()[j].segments[segment_size - 1].char_range.end());
    EXPECT_EQ(char_ranges[j], line_range);
  }
}

TEST_F(RenderTextTest, Multiline_ZeroWidthNewline) {
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetMultiline(true);

  const base::string16 text(ASCIIToUTF16("\n\n"));
  render_text->SetText(text);
  EXPECT_EQ(3u, render_text->GetNumLines());
  for (const auto& line : test_api()->lines()) {
    EXPECT_EQ(0, line.size.width());
    EXPECT_LT(0, line.size.height());
  }

  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  EXPECT_EQ(2U, run_list->size());
  EXPECT_EQ(0, run_list->width());
}

TEST_F(RenderTextTest, Multiline_GetLineContainingCaret) {
  struct {
    const SelectionModel caret;
    const size_t line_num;
  } cases[] = {
      {SelectionModel(8, CURSOR_FORWARD), 1},
      {SelectionModel(9, CURSOR_BACKWARD), 1},
      {SelectionModel(9, CURSOR_FORWARD), 2},
      {SelectionModel(12, CURSOR_BACKWARD), 2},
      {SelectionModel(12, CURSOR_FORWARD), 2},
      {SelectionModel(13, CURSOR_BACKWARD), 3},
      {SelectionModel(13, CURSOR_FORWARD), 3},
      {SelectionModel(14, CURSOR_BACKWARD), 4},
      {SelectionModel(14, CURSOR_FORWARD), 4},
  };

  // Set a non-integral width to cause rounding errors in calculating cursor
  // bounds. GetCursorBounds calculates cursor position based on the horizontal
  // span of the cursor, which is compared with the line widths to find out on
  // which line the span stops. Rounding errors should be taken into
  // consideration in comparing these two non-integral values.
  SetGlyphWidth(5.3);
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(45, 1000));
  render_text->SetMultiline(true);
  render_text->SetVerticalAlignment(ALIGN_TOP);

  for (auto text : {ASCIIToUTF16("\n123 456 789\n\n123"),
                    UTF8ToUTF16("\nשנב גקכ עין\n\nחלך")}) {
    for (const auto& sample : cases) {
      SCOPED_TRACE(testing::Message()
                   << "Testing " << (text[1] == '1' ? "LTR" : "RTL")
                   << " Caret " << sample.caret << " line " << sample.line_num);
      render_text->SetText(text);
      EXPECT_EQ(5U, render_text->GetNumLines());
      EXPECT_EQ(sample.line_num,
                render_text->GetLineContainingCaret(sample.caret));

      // GetCursorBounds should be in the same line as GetLineContainingCaret.
      Rect bounds = render_text->GetCursorBounds(sample.caret, true);
      EXPECT_EQ(int{sample.line_num},
                GetLineContainingYCoord(bounds.origin().y() + 1));
    }
  }
}

TEST_F(RenderTextTest, NewlineWithoutMultilineFlag) {
  const char* kTestStrings[] = {
      "abc\ndef", "a \n b ", "ab\n", "a\n\nb", "\nab", "\n",
  };

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(200, 1000));

  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(ASCIIToUTF16(kTestStrings[i]));
    render_text->Draw(canvas());

    EXPECT_EQ(1U, test_api()->lines().size());
  }
}

TEST_F(RenderTextTest, ControlCharacterReplacement) {
  static const char kTextWithControlCharacters[] = "\b\r\a\t\n\v\f";

  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16(kTextWithControlCharacters));

  // The control characters should have been replaced by their symbols.
  EXPECT_EQ(WideToUTF16(L"␈␍␇␉␊␋␌"), render_text->GetDisplayText());

  // Setting multiline, the newline character will be back to the original text.
  render_text->SetMultiline(true);
  EXPECT_EQ(WideToUTF16(L"␈\r␇␉\n␋␌"), render_text->GetDisplayText());

  // The generic control characters should have been replaced by the replacement
  // codepoints.
  render_text->SetText(UTF8ToUTF16("\u008f\u0080"));
  EXPECT_EQ(UTF8ToUTF16("\ufffd\ufffd"), render_text->GetDisplayText());
}

TEST_F(RenderTextTest, PrivateUseCharacterReplacement) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(WideToUTF16(L"xx\ue78d\ue78fa\U00100042z"));

  // The private use characters should have been replaced. If the code point is
  // a surrogate pair, it needs to be replaced by two characters.
  EXPECT_EQ(UTF8ToUTF16("xx\ufffd\ufffda\ufffdz"),
            render_text->GetDisplayText());

  // The private use characters from Area-B must be replaced. The rewrite step
  // replaced 2 characters by 1 character.
  render_text->SetText(WideToUTF16(L"x\U00100000\U00100001\U00100002"));
  EXPECT_EQ(UTF8ToUTF16("x\ufffd\ufffd\ufffd"), render_text->GetDisplayText());
}

TEST_F(RenderTextTest, InvalidSurrogateCharacterReplacement) {
  // Text with invalid surrogates (surrogates low 0xDC00 and high 0xD800).
  RenderText* render_text = GetRenderText();
  render_text->SetText(WideToUTF16(L"\xDC00\xD800"));
  EXPECT_EQ(UTF8ToUTF16("\ufffd\ufffd"), render_text->GetDisplayText());
}

// Make sure the horizontal positions of runs in a line (left-to-right for
// LTR languages and right-to-left for RTL languages).
TEST_F(RenderTextTest, HarfBuzz_HorizontalPositions) {
  const struct {
    const char* const text;
    const char* expected_runs;
  } kTestStrings[] = {
      {"abc\u3042\u3044\u3046\u3048\u304A", "[0->2][3->7]"},
      {"\u062A\u0641\u0627\u062D\u05EA\u05E4\u05D5\u05D6", "[7<-4][3<-0]"},
  };

  RenderTextHarfBuzz* render_text = GetRenderText();

  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i].text));

    EXPECT_EQ(kTestStrings[i].expected_runs, GetRunListStructureString());

    DrawVisualText();

    const internal::TextRunList* run_list = GetHarfBuzzRunList();
    ASSERT_EQ(2U, run_list->size());
    ASSERT_EQ(2U, text_log().size());

    // Verifies the DrawText happens in the visual order and left-to-right.
    // If the text is RTL, the logically first run should be drawn at last.
    EXPECT_EQ(
        run_list->runs()[run_list->logical_to_visual(0)]->shape.glyph_count,
        text_log()[0].glyphs().size());
    EXPECT_EQ(
        run_list->runs()[run_list->logical_to_visual(1)]->shape.glyph_count,
        text_log()[1].glyphs().size());
    EXPECT_LT(text_log()[0].origin().x(), text_log()[1].origin().x());
  }
}

// Test TextRunHarfBuzz's cluster finding logic.
TEST_F(RenderTextTest, HarfBuzz_Clusters) {
  struct {
    uint32_t glyph_to_char[4];
    Range chars[4];
    Range glyphs[4];
    bool is_rtl;
  } cases[] = {
    { // From string "A B C D" to glyphs "a b c d".
      { 0, 1, 2, 3 },
      { Range(0, 1), Range(1, 2), Range(2, 3), Range(3, 4) },
      { Range(0, 1), Range(1, 2), Range(2, 3), Range(3, 4) },
      false
    },
    { // From string "A B C D" to glyphs "d c b a".
      { 3, 2, 1, 0 },
      { Range(0, 1), Range(1, 2), Range(2, 3), Range(3, 4) },
      { Range(3, 4), Range(2, 3), Range(1, 2), Range(0, 1) },
      true
    },
    { // From string "A B C D" to glyphs "ab c c d".
      { 0, 2, 2, 3 },
      { Range(0, 2), Range(0, 2), Range(2, 3), Range(3, 4) },
      { Range(0, 1), Range(0, 1), Range(1, 3), Range(3, 4) },
      false
    },
    { // From string "A B C D" to glyphs "d c c ba".
      { 3, 2, 2, 0 },
      { Range(0, 2), Range(0, 2), Range(2, 3), Range(3, 4) },
      { Range(3, 4), Range(3, 4), Range(1, 3), Range(0, 1) },
      true
    },
  };

  internal::TextRunHarfBuzz run((Font()));
  run.range = Range(0, 4);
  run.shape.glyph_count = 4;
  run.shape.glyph_to_char.resize(4);

  for (size_t i = 0; i < base::size(cases); ++i) {
    std::copy(cases[i].glyph_to_char, cases[i].glyph_to_char + 4,
              run.shape.glyph_to_char.begin());
    run.font_params.is_rtl = cases[i].is_rtl;

    for (size_t j = 0; j < 4; ++j) {
      SCOPED_TRACE(base::StringPrintf("Case %" PRIuS ", char %" PRIuS, i, j));
      Range chars;
      Range glyphs;
      run.GetClusterAt(j, &chars, &glyphs);
      EXPECT_EQ(cases[i].chars[j], chars);
      EXPECT_EQ(cases[i].glyphs[j], glyphs);
      EXPECT_EQ(cases[i].glyphs[j], run.CharRangeToGlyphRange(chars));
    }
  }
}

// Ensures GetClusterAt does not crash on invalid conditions. crbug.com/724880
TEST_F(RenderTextTest, HarfBuzz_NoCrashOnTextRunGetClusterAt) {
  internal::TextRunHarfBuzz run((Font()));
  run.range = Range(0, 4);
  run.shape.glyph_count = 4;
  // Construct a |glyph_to_char| map where no glyph maps to the first character.
  run.shape.glyph_to_char = {1u, 1u, 2u, 3u};

  Range chars, glyphs;
  // GetClusterAt should not crash asking for the cluster at position 0.
  ASSERT_NO_FATAL_FAILURE(run.GetClusterAt(0, &chars, &glyphs));
}

// Ensure that graphemes with multiple code points do not get split.
TEST_F(RenderTextTest, HarfBuzz_SubglyphGraphemeCases) {
  const char* cases[] = {
      // Ä (A with combining umlaut), followed by a "B".
      "A\u0308B",
      // कि (Devangari letter KA with vowel I), followed by an "a".
      "\u0915\u093f\u0905",
      // จำ (Thai charcters CHO CHAN and SARA AM, followed by Thai digit 0.
      "\u0e08\u0e33\u0E50",
  };

  RenderTextHarfBuzz* render_text = GetRenderText();

  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Case %" PRIuS, i));

    base::string16 text = UTF8ToUTF16(cases[i]);
    render_text->SetText(text);
    const internal::TextRunList* run_list = GetHarfBuzzRunList();
    ASSERT_EQ(1U, run_list->size());
    internal::TextRunHarfBuzz* run = run_list->runs()[0].get();

    auto first_grapheme_bounds = run->GetGraphemeBounds(render_text, 0);
    EXPECT_EQ(first_grapheme_bounds, run->GetGraphemeBounds(render_text, 1));
    auto second_grapheme_bounds = run->GetGraphemeBounds(render_text, 2);
    EXPECT_EQ(first_grapheme_bounds.end(), second_grapheme_bounds.start());
  }
}

// Test the partition of a multi-grapheme cluster into grapheme ranges.
TEST_F(RenderTextTest, HarfBuzz_SubglyphGraphemePartition) {
  struct {
    uint32_t glyph_to_char[2];
    Range bounds[4];
    bool is_rtl;
  } cases[] = {
    { // From string "A B C D" to glyphs "a bcd".
      { 0, 1 },
      { Range(0, 10), Range(10, 13), Range(13, 17), Range(17, 20) },
      false
    },
    { // From string "A B C D" to glyphs "ab cd".
      { 0, 2 },
      { Range(0, 5), Range(5, 10), Range(10, 15), Range(15, 20) },
      false
    },
    { // From string "A B C D" to glyphs "dcb a".
      { 1, 0 },
      { Range(10, 20), Range(7, 10), Range(3, 7), Range(0, 3) },
      true
    },
    { // From string "A B C D" to glyphs "dc ba".
      { 2, 0 },
      { Range(15, 20), Range(10, 15), Range(5, 10), Range(0, 5) },
      true
    },
  };

  internal::TextRunHarfBuzz run((Font()));
  run.range = Range(0, 4);
  run.shape.glyph_count = 2;
  run.shape.glyph_to_char.resize(2);
  run.shape.positions.resize(4);
  run.shape.width = 20;

  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcd"));

  for (size_t i = 0; i < base::size(cases); ++i) {
    std::copy(cases[i].glyph_to_char, cases[i].glyph_to_char + 2,
              run.shape.glyph_to_char.begin());
    run.font_params.is_rtl = cases[i].is_rtl;
    for (int j = 0; j < 2; ++j)
      run.shape.positions[j].set(j * 10, 0);

    for (size_t j = 0; j < 4; ++j) {
      SCOPED_TRACE(base::StringPrintf("Case %" PRIuS ", char %" PRIuS, i, j));
      RangeF bounds_f = run.GetGraphemeBounds(render_text, j);
      Range bounds(std::round(bounds_f.start()), std::round(bounds_f.end()));
      EXPECT_EQ(cases[i].bounds[j], bounds);
    }
  }
}

TEST_F(RenderTextTest, HarfBuzz_RunDirection) {
  RenderTextHarfBuzz* render_text = GetRenderText();
  const base::string16 mixed = UTF8ToUTF16("\u05D0\u05D11234\u05D2\u05D3abc");
  render_text->SetText(mixed);

  // Get the run list for both display directions.
  render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_LTR);
  EXPECT_EQ("[7<-6][2->5][1<-0][8->10]", GetRunListStructureString());

  render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_RTL);
  EXPECT_EQ("[8->10][7<-6][2->5][1<-0]", GetRunListStructureString());
}

TEST_F(RenderTextTest, HarfBuzz_RunDirection_URLs) {
  RenderTextHarfBuzz* render_text = GetRenderText();
  // This string, unescaped (logical order):
  // ‭www.אב.גד/הוabc/def?זח=טי‬
  const base::string16 mixed = UTF8ToUTF16(
      "www.\u05D0\u05D1.\u05D2\u05D3/\u05D4\u05D5"
      "abc/def?\u05D6\u05D7=\u05D8\u05D9");
  render_text->SetText(mixed);

  // Normal LTR text should treat URL syntax as weak (as per the normal Bidi
  // algorithm).
  render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_LTR);

  // This is complex because a new run is created for each punctuation mark, but
  // it simplifies down to: [0->3][11<-4][12->19][24<-20]
  // Should render as: ‭www.וה/דג.באabc/def?יט=חז‬
  const char kExpectedRunListNormalBidi[] =
      "[0->2][3][11<-10][9][8<-7][6][5<-4][12->14][15][16->18][19][24<-23][22]"
      "[21<-20]";
  EXPECT_EQ(kExpectedRunListNormalBidi, GetRunListStructureString());

  // DIRECTIONALITY_AS_URL should be exactly the same as
  // DIRECTIONALITY_FORCE_LTR by default.
  render_text->SetDirectionalityMode(DIRECTIONALITY_AS_URL);
  EXPECT_EQ(kExpectedRunListNormalBidi, GetRunListStructureString());
}

TEST_F(RenderTextTest, HarfBuzz_BreakRunsByUnicodeBlocks) {
  RenderTextHarfBuzz* render_text = GetRenderText();

  // The ▶ (U+25B6) "play character" should break runs. http://crbug.com/278913
  render_text->SetText(UTF8ToUTF16("x\u25B6y"));
  EXPECT_EQ(ToString16Vec({"x", "▶", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1][2]", GetRunListStructureString());

  render_text->SetText(UTF8ToUTF16("x \u25B6 y"));
  EXPECT_EQ(ToString16Vec({"x", " ", "▶", " ", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1][2][3][4]", GetRunListStructureString());
}

TEST_F(RenderTextTest, HarfBuzz_BreakRunsByEmoji) {
  RenderTextHarfBuzz* render_text = GetRenderText();

  // 😁 (U+1F601, a smile emoji) and ✨ (U+2728, a sparkle icon) can both be
  // drawn with color emoji fonts, so runs should be separated. crbug.com/448909
  // Windows requires wide strings for \Unnnnnnnn universal character names.
  render_text->SetText(WideToUTF16(L"x\U0001F601y\u2728"));
  EXPECT_EQ(ToString16Vec({"x", "😁", "y", "✨"}), GetRunListStrings());
  // U+1F601 is represented as a surrogate pair in UTF-16.
  EXPECT_EQ("[0][1->2][3][4]", GetRunListStructureString());

  // Ensure non-latin 「foo」 brackets around Emoji correctly break runs.
  render_text->SetText(UTF8ToUTF16("「🦋」「"));
  EXPECT_EQ(ToString16Vec({"「", "🦋", "」「"}), GetRunListStrings());
  // Note 🦋 is a surrogate pair [1->2].
  EXPECT_EQ("[0][1->2][3->4]", GetRunListStructureString());
}

TEST_F(RenderTextTest, HarfBuzz_BreakRunsByNewline) {
  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetText(ASCIIToUTF16("x\ny"));
  EXPECT_EQ(ToString16Vec({"x", "\n", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1][2]", GetRunListStructureString());

  // Validate that the character newline is an unknown glyph
  // (see http://crbug/972090 and http://crbug/680430).
  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  ASSERT_EQ(3U, run_list->size());
  EXPECT_EQ(0U, run_list->runs()[0]->CountMissingGlyphs());
  EXPECT_EQ(1U, run_list->runs()[1]->CountMissingGlyphs());
  EXPECT_EQ(0U, run_list->runs()[2]->CountMissingGlyphs());

  SkScalar x_width =
      run_list->runs()[0]->GetGlyphWidthForCharRange(Range(0, 1));
  EXPECT_GT(x_width, 0);

  // Newline character must have a width of zero.
  SkScalar newline_width =
      run_list->runs()[1]->GetGlyphWidthForCharRange(Range(1, 2));
  EXPECT_EQ(newline_width, 0);

  SkScalar y_width =
      run_list->runs()[2]->GetGlyphWidthForCharRange(Range(2, 3));
  EXPECT_GT(y_width, 0);
}

TEST_F(RenderTextTest, HarfBuzz_BreakRunsByEmojiVariationSelectors) {
  constexpr int kGlyphWidth = 30;
  SetGlyphWidth(30);
  RenderTextHarfBuzz* render_text = GetRenderText();

  // ☎ (U+260E BLACK TELEPHONE) and U+FE0F (a variation selector) combine to
  // form (on some platforms), ☎️, a red (or blue) telephone. The run can
  // not break between the codepoints, or the incorrect glyph will be chosen.
  render_text->SetText(UTF8ToUTF16("z\u260E\uFE0Fy"));
  render_text->SetDisplayRect(Rect(1000, 50));
  EXPECT_EQ(ToString16Vec({"z", "☎\uFE0F", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1->2][3]", GetRunListStructureString());

  // Also test moving the cursor across the telephone.
  EXPECT_EQ(gfx::Range(0, 0), render_text->selection());
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(gfx::Range(1, 1), render_text->selection());
  EXPECT_EQ(1 * kGlyphWidth, render_text->GetUpdatedCursorBounds().x());

#if defined(OS_APPLE)
  // Early versions of macOS provide a tofu glyph for the variation selector.
  // Bail out early except on 10.12 and above.
  if (base::mac::IsAtMostOS10_11())
    return;
#endif

  // TODO(865709): make this work on Android.
#if !defined(OS_ANDROID)
  // Jump over the telephone: two codepoints, but a single glyph.
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(gfx::Range(3, 3), render_text->selection());
  EXPECT_EQ(2 * kGlyphWidth, render_text->GetUpdatedCursorBounds().x());

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(gfx::Range(4, 4), render_text->selection());
  EXPECT_EQ(3 * kGlyphWidth, render_text->GetUpdatedCursorBounds().x());

  // Nothing else.
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(gfx::Range(4, 4), render_text->selection());
  EXPECT_EQ(3 * kGlyphWidth, render_text->GetUpdatedCursorBounds().x());
#endif
}

TEST_F(RenderTextTest, HarfBuzz_OrphanedVariationSelector) {
  RenderTextHarfBuzz* render_text = GetRenderText();

  // It should never happen in normal usage, but a variation selector can appear
  // by itself. In this case, it can form its own text run, with no glyphs.
  render_text->SetText(UTF8ToUTF16("\uFE0F"));
  EXPECT_EQ(ToString16Vec({"\uFE0F"}), GetRunListStrings());
  EXPECT_EQ("[0]", GetRunListStructureString());
  CheckBoundsForCursorPositions();
}

TEST_F(RenderTextTest, HarfBuzz_AsciiVariationSelector) {
  RenderTextHarfBuzz* render_text = GetRenderText();
#if defined(OS_APPLE)
  // Don't use a system font on macOS - asking for a variation selector on
  // ASCII glyphs can tickle OS bugs. See http://crbug.com/785522.
  render_text->SetFontList(FontList("Arial, 12px"));
#endif
  // A variation selector doesn't have to appear with Emoji. It will probably
  // cause the typesetter to render tofu in this case, but it should not break
  // a text run.
  render_text->SetText(UTF8ToUTF16("z\uFE0Fy"));
  EXPECT_EQ(ToString16Vec({"z\uFE0Fy"}), GetRunListStrings());
  EXPECT_EQ("[0->2]", GetRunListStructureString());
  CheckBoundsForCursorPositions();
}

TEST_F(RenderTextTest, HarfBuzz_LeadingVariationSelector) {
  RenderTextHarfBuzz* render_text = GetRenderText();

  // When a variation selector appears either side of an emoji, ensure the one
  // after is in the same run.
  render_text->SetText(UTF8ToUTF16("\uFE0F\u260E\uFE0Fy"));
  EXPECT_EQ(ToString16Vec({"\uFE0F", "☎\uFE0F", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1->2][3]", GetRunListStructureString());
  CheckBoundsForCursorPositions();
}

TEST_F(RenderTextTest, HarfBuzz_TrailingVariationSelector) {
  RenderTextHarfBuzz* render_text = GetRenderText();

  // If a redundant variation selector appears in an emoji run, it also gets
  // merged into the emoji run. Usually there should be no effect. That's
  // ultimately up to the typeface but, however it choses, cursor and glyph
  // positions should behave.
  render_text->SetText(UTF8ToUTF16("z\u260E\uFE0F\uFE0Fy"));
  EXPECT_EQ(ToString16Vec({"z", "☎\uFE0F\uFE0F", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1->3][4]", GetRunListStructureString());
  CheckBoundsForCursorPositions();
}

TEST_F(RenderTextTest, HarfBuzz_MultipleVariationSelectorEmoji) {
  RenderTextHarfBuzz* render_text = GetRenderText();

  // Two emoji with variation selectors appearing in a correct sequence should
  // be in the same run.
  render_text->SetText(UTF8ToUTF16("z\u260E\uFE0F\u260E\uFE0Fy"));
  EXPECT_EQ(ToString16Vec({"z", "☎\uFE0F☎\uFE0F", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1->4][5]", GetRunListStructureString());
  CheckBoundsForCursorPositions();
}

TEST_F(RenderTextTest, HarfBuzz_BreakRunsByAscii) {
  RenderTextHarfBuzz* render_text = GetRenderText();

  // ▶ (U+25B6, Geometric Shapes) and an ascii character should have
  // different runs.
  render_text->SetText(WideToUTF16(L"▶z"));
  EXPECT_EQ(ToString16Vec({"▶", "z"}), GetRunListStrings());
  EXPECT_EQ("[0][1]", GetRunListStructureString());

  // ★ (U+2605, Miscellaneous Symbols) and an ascii character should have
  // different runs.
  render_text->SetText(WideToUTF16(L"★1"));
  EXPECT_EQ(ToString16Vec({"★", "1"}), GetRunListStrings());
  EXPECT_EQ("[0][1]", GetRunListStructureString());

  // 🐱 (U+1F431, a cat face, Miscellaneous Symbols and Pictographs) and an
  // ASCII period should have separate runs.
  render_text->SetText(WideToUTF16(L"🐱."));
  EXPECT_EQ(ToString16Vec({"🐱", "."}), GetRunListStrings());
  // U+1F431 is represented as a surrogate pair in UTF-16.
  EXPECT_EQ("[0->1][2]", GetRunListStructureString());

  // 🥴 (U+1f974, Supplemental Symbols and Pictographs) and an ascii character
  // should have different runs.
  render_text->SetText(WideToUTF16(L"🥴$"));
  EXPECT_EQ(ToString16Vec({"🥴", "$"}), GetRunListStrings());
  EXPECT_EQ("[0->1][2]", GetRunListStructureString());
}

// Test that, on Mac, font fallback mechanisms and Harfbuzz configuration cause
// the correct glyphs to be chosen for unicode regional indicators.
TEST_F(RenderTextTest, EmojiFlagGlyphCount) {
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(1000, 1000));
  // Two flags: UK and Japan. Note macOS 10.9 only has flags for 10 countries.
  base::string16 text(UTF8ToUTF16("🇬🇧🇯🇵"));
  // Each flag is 4 UTF16 characters (2 surrogate pair code points).
  EXPECT_EQ(8u, text.length());
  render_text->SetText(text);

  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  ASSERT_EQ(1U, run_list->runs().size());
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_APPLE)
  // On Linux and macOS, the flags should be found, so two glyphs result.
  EXPECT_EQ(2u, run_list->runs()[0]->shape.glyph_count);
#elif defined(OS_ANDROID)
  // It seems that some versions of android support the flags. Older versions
  // don't support it.
  EXPECT_TRUE(2u == run_list->runs()[0]->shape.glyph_count ||
              4u == run_list->runs()[0]->shape.glyph_count);
#else
  // Elsewhere, the flags are not found, so each surrogate pair gets a
  // placeholder glyph. Eventually, all platforms should have 2 glyphs.
  EXPECT_EQ(4u, run_list->runs()[0]->shape.glyph_count);
#endif
}

TEST_F(RenderTextTest, HarfBuzz_ShapeRunsWithMultipleFonts) {
  RenderTextHarfBuzz* render_text = GetRenderText();

  // The following text will be split in 3 runs:
  //   1) u+1F3F3 u+FE0F u+FE0F  (Segoe UI Emoji)
  //   2) u+0020                 (Segoe UI)
  //   3) u+1F308 u+20E0 u+20E0  (Segoe UI Symbol)
  // The three runs are shape in the same group but are mapped with three
  // different fonts.
  render_text->SetText(
      UTF8ToUTF16(u8"\U0001F3F3\U0000FE0F\U00000020\U0001F308\U000020E0"));
  std::vector<base::string16> expected;
  expected.push_back(WideToUTF16(L"\U0001F3F3\U0000FE0F"));
  expected.push_back(WideToUTF16(L" "));
  expected.push_back(WideToUTF16(L"\U0001F308\U000020E0"));
  EXPECT_EQ(expected, GetRunListStrings());
  EXPECT_EQ("[0->2][3][4->6]", GetRunListStructureString());

#if defined(OS_WIN)
  std::vector<std::string> expected_fonts;
  if (base::win::GetVersion() < base::win::Version::WIN10)
    expected_fonts = {"Segoe UI", "Segoe UI", "Segoe UI Symbol"};
  else
    expected_fonts = {"Segoe UI Emoji", "Segoe UI", "Segoe UI Symbol"};

  std::vector<std::string> mapped_fonts;
  for (const auto& font_span : GetFontSpans())
    mapped_fonts.push_back(font_span.first.GetFontName());
  EXPECT_EQ(expected_fonts, mapped_fonts);
#endif
}

TEST_F(RenderTextTest, GlyphBounds) {
  const char* kTestStrings[] = {"asdf 1234 qwer", "\u0647\u0654",
                                "\u0645\u0631\u062D\u0628\u0627"};
  RenderText* render_text = GetRenderText();

  for (size_t i = 0; i < base::size(kTestStrings); ++i) {
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));

    for (size_t j = 0; j < render_text->text().length(); ++j)
      EXPECT_FALSE(render_text->GetCursorSpan(Range(j, j + 1)).is_empty());
  }
}

// Ensure that shaping with a non-existent font does not cause a crash.
TEST_F(RenderTextTest, HarfBuzz_NonExistentFont) {
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("test"));
  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  ASSERT_EQ(1U, run_list->size());
  internal::TextRunHarfBuzz* run = run_list->runs()[0].get();
  ShapeRunWithFont(render_text->text(), Font("TheFontThatDoesntExist", 13),
                   FontRenderParams(), run);
}

// Ensure an empty run returns sane values to queries.
TEST_F(RenderTextTest, HarfBuzz_EmptyRun) {
  internal::TextRunHarfBuzz run((Font()));
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("abcdefgh"));

  run.range = Range(3, 8);
  run.shape.glyph_count = 0;
  EXPECT_EQ(Range(), run.CharRangeToGlyphRange(Range(4, 5)));
  EXPECT_EQ(RangeF(), run.GetGraphemeBounds(render_text, 4));
  Range chars;
  Range glyphs;
  run.GetClusterAt(4, &chars, &glyphs);
  EXPECT_EQ(Range(3, 8), chars);
  EXPECT_EQ(Range(), glyphs);
}

// Ensure the line breaker doesn't compute the word's width bigger than the
// actual size. See http://crbug.com/470073
TEST_F(RenderTextTest, HarfBuzz_WordWidthWithDiacritics) {
  RenderTextHarfBuzz* render_text = GetRenderText();
  const base::string16 kWord = UTF8ToUTF16("\u0906\u092A\u0915\u0947 ");
  render_text->SetText(kWord);
  const SizeF text_size = render_text->GetStringSizeF();

  render_text->SetText(kWord + kWord);
  render_text->SetMultiline(true);
  EXPECT_EQ(text_size.width() * 2, render_text->GetStringSizeF().width());
  EXPECT_EQ(text_size.height(), render_text->GetStringSizeF().height());
  render_text->SetDisplayRect(Rect(0, 0, std::ceil(text_size.width()), 0));
  EXPECT_NEAR(text_size.width(), render_text->GetStringSizeF().width(), 1.0f);
  EXPECT_EQ(text_size.height() * 2, render_text->GetStringSizeF().height());
}

// Ensure a string fits in a display rect with a width equal to the string's.
TEST_F(RenderTextTest, StringFitsOwnWidth) {
  RenderText* render_text = GetRenderText();
  const base::string16 kString = ASCIIToUTF16("www.example.com");

  render_text->SetText(kString);
  render_text->ApplyWeight(Font::Weight::BOLD, Range(0, 3));
  render_text->SetElideBehavior(ELIDE_TAIL);

  render_text->SetDisplayRect(Rect(0, 0, 500, 100));
  EXPECT_EQ(kString, render_text->GetDisplayText());
  render_text->SetDisplayRect(Rect(0, 0, render_text->GetContentWidth(), 100));
  EXPECT_EQ(kString, render_text->GetDisplayText());
}

// TODO(865715): Figure out why this fails on Android.
#if !defined(OS_ANDROID)
// Ensure that RenderText examines all of the fonts in its FontList before
// falling back to other fonts.
TEST_F(RenderTextTest, HarfBuzz_FontListFallback) {
  // Double-check that the requested fonts are present.
  std::string format = std::string(kTestFontName) + ", %s, 12px";
  FontList font_list(base::StringPrintf(format.c_str(), kSymbolFontName));
  const std::vector<Font>& fonts = font_list.GetFonts();
  ASSERT_EQ(2u, fonts.size());
  ASSERT_EQ(base::ToLowerASCII(kTestFontName),
            base::ToLowerASCII(fonts[0].GetActualFontName()));
  ASSERT_EQ(base::ToLowerASCII(kSymbolFontName),
            base::ToLowerASCII(fonts[1].GetActualFontName()));

  // "⊕" (U+2295, CIRCLED PLUS) should be rendered with Symbol rather than
  // falling back to some other font that's present on the system.
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetFontList(font_list);
  render_text->SetText(UTF8ToUTF16("\u2295"));
  const std::vector<FontSpan> spans = GetFontSpans();
  ASSERT_EQ(static_cast<size_t>(1), spans.size());
  EXPECT_EQ(kSymbolFontName, spans[0].first.GetFontName());
}
#endif  // !defined(OS_ANDROID)

// Ensure that the fallback fonts offered by GetFallbackFonts() are tried. Note
// this test assumes the font "Arial" doesn't provide a unicode glyph for a
// particular character, and that there is a system fallback font which does.
// TODO(msw): Fallback doesn't find a glyph on Linux and Android.
#if !defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
TEST_F(RenderTextTest, HarfBuzz_UnicodeFallback) {
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetFontList(FontList("Arial, 12px"));

  // An invalid Unicode character that somehow yields Korean character "han".
  render_text->SetText(UTF8ToUTF16("\ud55c"));
  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  ASSERT_EQ(1U, run_list->size());
  EXPECT_EQ(0U, run_list->runs()[0]->CountMissingGlyphs());
}
#endif  // !defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

// Ensure that the fallback fonts offered by GetFallbackFont() support glyphs
// for different languages.
TEST_F(RenderTextTest, HarfBuzz_FallbackFontsSupportGlyphs) {
  // The word 'test' in different languages.
  static const wchar_t* kLanguageTests[] = {
      L"test", L"اختبار", L"Δοκιμή", L"परीक्षा", L"تست", L"Փորձարկում",
  };

  for (const wchar_t* text : kLanguageTests) {
    RenderTextHarfBuzz* render_text = GetRenderText();
    render_text->SetText(WideToUTF16(text));

    const internal::TextRunList* run_list = GetHarfBuzzRunList();
    ASSERT_EQ(1U, run_list->size());
    int missing_glyphs = run_list->runs()[0]->CountMissingGlyphs();
    if (missing_glyphs != 0) {
      ADD_FAILURE() << "Text '" << text << "' is missing " << missing_glyphs
                    << " glyphs.";
    }
  }
}

// Ensure that the fallback fonts offered by GetFallbackFont() support glyphs
// for different languages.
TEST_F(RenderTextTest, HarfBuzz_MultiRunsSupportGlyphs) {
  static const wchar_t* kLanguageTests[] = {
      L"www.اختبار.com",
      L"(اختبار)",
      L"/ זה (מבחן) /",
  };

  for (const wchar_t* text : kLanguageTests) {
    RenderTextHarfBuzz* render_text = GetRenderText();
    render_text->SetText(WideToUTF16(text));

    int missing_glyphs = 0;
    const internal::TextRunList* run_list = GetHarfBuzzRunList();
    for (const auto& run : run_list->runs())
      missing_glyphs += run->CountMissingGlyphs();

    if (missing_glyphs != 0) {
      ADD_FAILURE() << "Text '" << text << "' is missing " << missing_glyphs
                    << " glyphs.";
    }
  }
}

struct FallbackFontCase {
  const char* test_name;
  const wchar_t* text;
};

class RenderTextTestWithFallbackFontCase
    : public RenderTextTest,
      public ::testing::WithParamInterface<FallbackFontCase> {
 public:
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<FallbackFontCase> param_info) {
    return param_info.param.test_name;
  }
};

TEST_P(RenderTextTestWithFallbackFontCase, FallbackFont) {
  FallbackFontCase param = GetParam();
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetText(WideToUTF16(param.text));

  int missing_glyphs = 0;
  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  for (const auto& run : run_list->runs())
    missing_glyphs += run->CountMissingGlyphs();

  EXPECT_EQ(missing_glyphs, 0);
}

const FallbackFontCase kUnicodeDecomposeCases[] = {
    // Decompose to "\u0041\u0300".
    {"letter_A_with_grave", L"\u00c0"},
    // Decompose to "\u004f\u0328\u0304".
    {"letter_O_with_ogonek_macron", L"\u01ec"},
    // Decompose to "\u0041\u030a".
    {"angstrom_sign", L"\u212b"},
    // Decompose to "\u1100\u1164\u11b6".
    {"hangul_syllable_gyaelh", L"\uac63"},
    // Decompose to "\u1107\u1170\u11af".
    {"hangul_syllable_bwel", L"\ubdc0"},
    // Decompose to "\U00044039".
    {"cjk_ideograph_fad4", L"\ufad4"},
};

INSTANTIATE_TEST_SUITE_P(FallbackFontUnicodeDecompose,
                         RenderTextTestWithFallbackFontCase,
                         ::testing::ValuesIn(kUnicodeDecomposeCases),
                         RenderTextTestWithFallbackFontCase::ParamInfoToString);

// Ensures that RenderText is finding an appropriate font and that every
// codepoint can be rendered by the font. An error here can be by an incorrect
// ItemizeText(...) leading to an invalid fallback font.
const FallbackFontCase kComplexTextCases[] = {
    {"simple1", L"test"},
    {"simple2", L"اختبار"},
    {"simple3", L"Δοκιμή"},
    {"simple4", L"परीक्षा"},
    {"simple5", L"تست"},
    {"simple6", L"Փորձարկում"},
    {"mixed1", L"www.اختبار.com"},
    {"mixed2", L"(اختبار)"},
    {"mixed3", L"/ זה (מבחן) /"},
#if defined(OS_WIN)
    {"asc_arb", L"abcښڛڜdef"},
    {"devanagari", L"ञटठडढणतथ"},
    {"ethiopic", L"መጩጪᎅⶹⶼ"},
    {"greek", L"ξοπρς"},
    {"kannada", L"ಠಡಢಣತಥ"},
    {"lao", L"ປຝພຟມ"},
    {"oriya", L"ଔକଖଗଘଙ"},
    {"telugu_lat", L"aaఉయ!"},
    {"common_math", L"ℳ: ¬ƒ(x)=½×¾"},
    {"picto_title", L"☞☛test☚☜"},
    {"common_numbers", L"𝟭𝟐⒓¹²"},
    {"common_puncts", L",.!"},
    {"common_space_math1", L" 𝓐"},
    {"common_space_math2", L" 𝓉"},
    {"common_split_spaces", L"♬  𝓐"},
    {"common_mixed", L"\U0001d4c9\u24d4\U0001d42c"},
    {"arrows", L"↰↱↲↳↴↵⇚⇛⇜⇝⇞⇟"},
    {"arrows_space", L"↰ ↱ ↲ ↳ ↴ ↵ ⇚ ⇛ ⇜ ⇝ ⇞ ⇟"},
    {"emoji_title", L"▶Feel goods"},
    {"enclosed_alpha", L"ⒶⒷⒸⒹⒺⒻⒼ"},
    {"shapes", L" ▶▷▸▹►▻◀◁◂◃◄◅"},
    {"symbols", L"☂☎☏☝☫☬☭☮☯"},
    {"symbols_space", L"☂ ☎ ☏ ☝ ☫ ☬ ☭ ☮ ☯"},
    {"dingbats", L"✂✃✄✆✇✈"},
    {"cjk_compatibility_ideographs", L"賈滑串句龜"},
    {"lat_dev_ZWNJ", L"a\u200Cक"},
    {"paren_picto", L"(☾☹☽)"},
    {"emoji1", L"This is 💩!"},
    {"emoji2", L"Look [🔝]"},
    {"strange1", L"💔♬  𝓐 𝓉ⓔ𝐬т ＦỖ𝕣 ｃ卄尺𝕆ᵐ€  ♘👹"},
    {"strange2", L"˜”*°•.˜”*°• A test for chrome •°*”˜.•°*”˜"},
    {"strange3", L"𝐭єⓢт ｆσ𝐑 𝔠ʰ𝕣ό𝐌𝔢"},
    {"strange4", L"тẸⓈ𝔱 𝔽𝕠ᖇ 𝕔𝐡ŕ𝔬ⓜẸ"},
    {"url1", L"http://www.google.com"},
    {"url2", L"http://www.nowhere.com/Lörick.html"},
    {"url3", L"http://www.nowhere.com/تسجيل الدخول"},
    {"url4", L"https://xyz.com:8080/تس(1)جيل الدخول"},
    {"url5", L"http://www.script.com/test.php?abc=42&cde=12&f=%20%20"},
    {"punct1", L"This‐is‑a‒test–for—punctuations"},
    {"punct2", L"⁅All ‷magic‴ comes with a ‶price″⁆"},
    {"punct3", L"⍟ Complete my sentence… †"},
    {"parens", L"❝This❞ 「test」 has ((a)) 【lot】 [{of}] 〚parentheses〛"},
    {"games", L"Let play: ♗♘⚀⚁♠♣"},
    {"braille", L"⠞⠑⠎⠞ ⠋⠕⠗ ⠉⠓⠗⠕⠍⠑"},
    {"emoticon1", L"¯\\_(ツ)_/¯"},
    {"emoticon2", L"٩(⁎❛ᴗ❛⁎)۶"},
    {"emoticon3", L"(͡° ͜ʖ ͡°)"},
    {"emoticon4", L"[̲̅$̲̅(̲̅5̲̅)̲̅$̲̅]"},
#endif
};

INSTANTIATE_TEST_SUITE_P(FallbackFontComplexTextCases,
                         RenderTextTestWithFallbackFontCase,
                         ::testing::ValuesIn(kComplexTextCases),
                         RenderTextTestWithFallbackFontCase::ParamInfoToString);

// Test cases to ensures the COMMON unicode script is split by unicode code
// block. These tests work on Windows and Mac default fonts installation.
// On other platforms, the fonts are mock (see test_fonts).
const FallbackFontCase kCommonScriptCases[] = {
#if defined(OS_WIN)
    // The following tests are made to work on win7 and win10.
    {"common00", L"\u237b\u2ac1\u24f5\u259f\u2a87\u23ea\u25d4\u2220"},
    {"common01", L"\u2303\u2074\u2988\u32b6\u26a2\u24e5\u2a53\u2219"},
    {"common02", L"\u29b2\u25fc\u2366\u24ae\u2647\u258e\u2654\u25fe"},
    {"common03", L"\u21ea\u22b4\u29b0\u2a84\u0008\u2657\u2731\u2697"},
    {"common04", L"\u2b3c\u2932\u21c8\u23cf\u20a1\u2aa2\u2344\u0011"},
    {"common05", L"\u22c3\u2a56\u2340\u21b7\u26ba\u2798\u220f\u2404"},
    {"common06", L"\u21f9\u25fd\u008e\u21e6\u2686\u21e4\u259f\u29ee"},
    {"common07", L"\u231e\ufe39\u0008\u2349\u2262\u2270\uff09\u2b3b"},
    {"common08", L"\u24a3\u236e\u29b2\u2259\u26ea\u2705\u00ae\u2a23"},
    {"common09", L"\u33bd\u235e\u2018\u32ba\u2973\u02c1\u20b9\u25b4"},
    {"common10", L"\u2245\u2a4d\uff19\u2042\u2aa9\u2658\u276e\uff40"},
    {"common11", L"\u0007\u21b4\u23c9\u2593\u21ba\u00a0\u258f\u23b3"},
    {"common12", L"\u2938\u250c\u2240\u2676\u2297\u2b07\u237e\u2a04"},
    {"common13", L"\u2520\u233a\u20a5\u2744\u2445\u268a\u2716\ufe62"},
    {"common14", L"\ufe4d\u25d5\u2ae1\u2a35\u2323\u273c\u26be\u2a3b"},
    {"common15", L"\u2aa2\u0000\ufe65\u2962\u2573\u21f8\u2651\u02d2"},
    {"common16", L"\u225c\u2283\u2960\u4de7\uff12\uffe1\u0016\u2905"},
    {"common17", L"\uff07\u25aa\u2076\u259e\u226c\u2568\u0026\u2691"},
    {"common18", L"\u2388\u21c2\u208d\u2a7f\u22d0\u2583\u2ad5\u240f"},
    {"common19", L"\u230a\u27ac\u001e\u261e\u259d\u25c3\u33a5\u0011"},
    {"common20", L"\ufe54\u29c7\u2477\u21ed\u2069\u4dfc\u2ae2\u21e8"},
    {"common21", L"\u2131\u2ab7\u23b9\u2660\u2083\u24c7\u228d\u2a01"},
    {"common22", L"\u2587\u2572\u21df\uff3c\u02cd\ufffd\u2404\u22b3"},
    {"common23", L"\u4dc3\u02fe\uff09\u25a3\ufe14\u255c\u2128\u2698"},
    {"common24", L"\u2b36\u3382\u02f6\u2752\uff16\u22cf\u00b0\u21d6"},
    {"common25", L"\u2561\u23db\u2958\u2782\u22af\u2621\u24a3\u29ae"},
    {"common26", L"\u2693\u22e2\u2988\u2987\u33ba\u2a94\u298e\u2328"},
    {"common27", L"\u266c\u2aa5\u2405\uffeb\uff5c\u2902\u291e\u02e6"},
    {"common28", L"\u2634\u32b2\u3385\u2032\u33be\u2366\u2ac7\u23cf"},
    {"common29", L"\u2981\ua721\u25a9\u2320\u21cf\u295a\u2273\u2ac2"},
    {"common30", L"\u22d9\u2465\u2347\u2a94\u4dca\u2389\u23b0\u208d"},
    {"common31", L"\u21cc\u2af8\u2912\u23a4\u2271\u2303\u241e\u33a1"},
#elif defined(OS_ANDROID)
    {"common00", L"\u2497\uff04\u277c\u21b6\u2076\u21e4\u2068\u21b3"},
    {"common01", L"\u2663\u2466\u338e\u226b\u2734\u21be\u3389\u00ab"},
    {"common02", L"\u2062\u2197\u3392\u2681\u33be\u206d\ufe10\ufe34"},
    {"common03", L"\u02db\u00b0\u02d3\u2745\u33d1\u21e4\u24e4\u33d6"},
    {"common04", L"\u21da\u261f\u26a1\u2586\u27af\u2560\u21cd\u25c6"},
    {"common05", L"\ufe51\uff17\u0027\u21fd\u24de\uff5e\u2606\u251f"},
    {"common06", L"\u2493\u2466\u21fc\u226f\u202d\u21a9\u0040\u265d"},
    {"common07", L"\u2103\u255a\u2153\u26be\u27ac\u222e\u2490\u21a4"},
    {"common08", L"\u270b\u2486\u246b\u263c\u27b6\u21d9\u219d\u25a9"},
    {"common09", L"\u002d\u2494\u25fd\u2321\u2111\u2511\u00d7\u2535"},
    {"common10", L"\u2523\u203e\u25b2\ufe18\u2499\u2229\ufd3e\ufe16"},
    {"common11", L"\u2133\u2716\u273f\u2064\u2248\u005c\u265f\u21e6"},
    {"common12", L"\u2060\u246a\u231b\u2726\u25bd\ufe40\u002e\u25ca"},
    {"common13", L"\ufe39\u24a2\ufe18\u254b\u249c\u3396\ua71f\u2466"},
    {"common14", L"\u21b8\u2236\u251a\uff11\u2077\u0035\u27bd\u2013"},
    {"common15", L"\u2668\u2551\u221a\u02bc\u2741\u2649\u2192\u00a1"},
    {"common16", L"\u2211\u21ca\u24dc\u2536\u201b\u21c8\u2530\u25fb"},
    {"common17", L"\u231a\u33d8\u2934\u27bb\u2109\u23ec\u20a9\u3000"},
    {"common18", L"\u2069\u205f\u33d3\u2466\u24a1\u24dd\u21ac\u21e3"},
    {"common19", L"\u2737\u219a\u21f1\u2285\u226a\u00b0\u27b2\u2746"},
    {"common20", L"\u264f\u2539\u2202\u264e\u2548\u2530\u2111\u2007"},
    {"common21", L"\u2799\u0035\u25e4\u265b\u24e2\u2044\u222b\u0021"},
    {"common22", L"\u2728\u00a2\u2533\ufe43\u33c9\u27a2\u02f9\u005d"},
    {"common23", L"\ufe68\u256c\u25b6\u276c\u2771\u33c4\u2712\u24b3"},
    {"common24", L"\ufe5d\ufe31\ufe3d\u205e\u2512\u33b8\u272b\ufe4f"},
    {"common25", L"\u24e7\u25fc\u2582\u2743\u2010\u2474\u2262\u251a"},
    {"common26", L"\u2020\u211c\u24b4\u33c7\u2007\uff0f\u267f\u00b4"},
    {"common27", L"\u266c\u3399\u2570\u33a4\u276e\u00a8\u2506\u24dc"},
    {"common28", L"\u2202\ufe43\u2511\u2191\u339a\u33b0\u02d7\u2473"},
    {"common29", L"\u2517\u2297\u2762\u2460\u25bd\u24a9\u21a7\ufe64"},
    {"common30", L"\u2105\u2722\u275d\u249c\u21a2\u2590\u2260\uff5d"},
    {"common31", L"\u33ba\u21c6\u2706\u02cb\ufe64\u02e6\u0374\u2493"},
#elif defined(OS_APPLE)
    {"common00", L"\u2153\u24e0\u2109\u02f0\u2a8f\u25ed\u02c5\u2716"},
    {"common01", L"\u02f0\u208c\u2203\u2518\u2067\u2270\u21f1\ufe66"},
    {"common02", L"\u2686\u2585\u2b15\u246f\u23e3\u21b4\u2394\ufe31"},
    {"common03", L"\u23c1\u2a97\u201e\u2200\u3389\u25d3\u02c2\u259d"},
#else
    // The following tests are made for the mock fonts (see test_fonts).
    {"common00", L"\u2153\u24e0\u2109\u02f0\u2a8f\u25ed\u02c5\u2716"},
    {"common01", L"\u02f0\u208c\u2203\u2518\u2067\u2270\u21f1\ufe66"},
    {"common02", L"\u2686\u2585\u2b15\u246f\u23e3\u21b4\u2394\ufe31"},
    {"common03", L"\u23c1\u2a97\u201e\u2200\u3389\u25d3\u02c2\u259d"},
    {"common04", L"\u2075\u4dec\u252a\uff15\u4df6\u2668\u27fa\ufe17"},
    {"common05", L"\u260b\u2049\u3036\u2a85\u2b15\u23c7\u230a\u2374"},
    {"common06", L"\u2771\u27fa\u255d\uff0b\u2213\u3396\u2a85\u2276"},
    {"common07", L"\u211e\u2b06\u2255\u2727\u26c3\u33cf\u267d\u2ab2"},
    {"common08", L"\u2373\u20b3\u22b8\u2a0f\u02fd\u2585\u3036\ufe48"},
    {"common09", L"\u256d\u2940\u21d8\u4dde\u23a1\u226b\u3374\u2a99"},
    {"common10", L"\u270f\u24e5\u26c1\u2131\u21f5\u25af\u230f\u27fe"},
    {"common11", L"\u27aa\u23a2\u02ef\u2373\u2257\u2749\u2496\ufe31"},
    {"common12", L"\u230a\u25fb\u2117\u3386\u32cc\u21c5\u24c4\u207e"},
    {"common13", L"\u2467\u2791\u3393\u33bb\u02ca\u25de\ua788\u278f"},
    {"common14", L"\ua719\u25ed\u20a8\u20a1\u4dd8\u2295\u24eb\u02c8"},
    {"common15", L"\u22b6\u2520\u2036\uffee\u21df\u002d\u277a\u2b24"},
    {"common16", L"\u21f8\u211b\u22a0\u25b6\u263e\u2704\u221a\u2758"},
    {"common17", L"\ufe10\u2060\u24ac\u3385\u27a1\u2059\u2689\u2278"},
    {"common18", L"\u269b\u211b\u33a4\ufe36\u239e\u267f\u2423\u24a2"},
    {"common19", L"\u4ded\u262d\u225e\u248b\u21df\u279d\u2518\u21ba"},
    {"common20", L"\u225a\uff16\u21d4\u21c6\u02ba\u2545\u23aa\u005e"},
    {"common21", L"\u20a5\u265e\u3395\u2a6a\u2555\u22a4\u2086\u23aa"},
    {"common22", L"\u203f\u3250\u2240\u24e9\u21cb\u258f\u24b1\u3259"},
    {"common23", L"\u27bd\u263b\uff1f\u2199\u2547\u258d\u201f\u2507"},
    {"common24", L"\u2482\u2548\u02dc\u231f\u24cd\u2198\u220e\u20ad"},
    {"common25", L"\u2ff7\u2540\ufe48\u2197\u276b\u2574\u2062\u3398"},
    {"common26", L"\u2663\u21cd\u263f\u23e5\u22d7\u2518\u21b9\u2628"},
    {"common27", L"\u21fa\ufe66\u2739\u2051\u21f4\u3399\u2599\u25f7"},
    {"common28", L"\u29d3\u25ec\u27a6\u24e0\u2735\u25b4\u2737\u25db"},
    {"common29", L"\u2622\u22e8\u33d2\u21d3\u2502\u2153\u2669\u25f2"},
    {"common30", L"\u2121\u21af\u2729\u203c\u337a\u2464\u2b08\u2e24"},
    {"common31", L"\u33cd\u007b\u02d2\u22cc\u32be\u2ffa\u2787\u02e9"},
#endif
};

INSTANTIATE_TEST_SUITE_P(FallbackFontCommonScript,
                         RenderTextTestWithFallbackFontCase,
                         ::testing::ValuesIn(kCommonScriptCases),
                         RenderTextTestWithFallbackFontCase::ParamInfoToString);

#if defined(OS_WIN)
// Ensures that locale is used for fonts selection.
TEST_F(RenderTextTest, CJKFontWithLocale) {
  const wchar_t kCJKTest[] = L"\u8AA4\u904E\u9AA8";
  static const char* kLocaleTests[] = {"zh-CN", "ja-JP", "ko-KR"};

  std::set<std::string> tested_font_names;
  for (const auto* locale : kLocaleTests) {
    base::i18n::SetICUDefaultLocale(locale);
    ResetRenderTextInstance();

    RenderTextHarfBuzz* render_text = GetRenderText();
    render_text->SetText(WideToUTF16(kCJKTest));

    const std::vector<FontSpan> font_spans = GetFontSpans();
    ASSERT_EQ(font_spans.size(), 1U);

    // Expect the font name to be different for each locale.
    bool unique_font_name =
        tested_font_names.insert(font_spans[0].first.GetFontName()).second;
    EXPECT_TRUE(unique_font_name);
  }
}
#endif  // defined(OS_WIN)

TEST_F(RenderTextTest, SameFontAccrossIgnorableCodepoints) {
  RenderText* render_text = GetRenderText();

  render_text->SetText(UTF8ToUTF16("\u060F"));
  const std::vector<FontSpan> spans1 = GetFontSpans();
  ASSERT_EQ(1u, spans1.size());
  Font font1 = spans1[0].first;

  render_text->SetText(UTF8ToUTF16("\u060F\u200C\u060F"));
  const std::vector<FontSpan> spans2 = GetFontSpans();
  ASSERT_EQ(1u, spans2.size());
  Font font2 = spans2[0].first;

  // Ensures the same font is used with or without the joiners
  // (see http://crbug.com/1036652).
  EXPECT_EQ(font1.GetFontName(), font2.GetFontName());
}

TEST_F(RenderTextTest, ZeroWidthCharacters) {
  static const wchar_t* kEmptyText[] = {
      L"\u200C",  // ZERO WIDTH NON-JOINER
      L"\u200D",  // ZERO WIDTH JOINER
      L"\u200B",  // ZERO WIDTH SPACE
      L"\uFEFF",  // ZERO WIDTH NO-BREAK SPACE
  };

  for (const wchar_t* text : kEmptyText) {
    RenderTextHarfBuzz* render_text = GetRenderText();
    render_text->SetText(WideToUTF16(text));

    const internal::TextRunList* run_list = GetHarfBuzzRunList();
    EXPECT_EQ(0, run_list->width());
    ASSERT_EQ(run_list->runs().size(), 1U);
    EXPECT_EQ(run_list->runs()[0]->CountMissingGlyphs(), 0U);
  }
}

// Ensure that the width reported by RenderText is sufficient for drawing. Draws
// to a canvas and checks if any pixel beyond the bounding rectangle is colored.
TEST_F(RenderTextTest, DISABLED_TextDoesntClip) {
  const char* kTestStrings[] = {
      "            ",
      // TODO(dschuyler): Underscores draw outside GetStringSize;
      // crbug.com/459812.  This appears to be a preexisting issue that wasn't
      // revealed by the prior unit tests.
      // "TEST_______",
      "TEST some stuff", "WWWWWWWWWW", "gAXAXAXAXAXAXA",
      "g\u00C5X\u00C5X\u00C5X\u00C5X\u00C5X\u00C5X\u00C5",
      ("\u0647\u0654\u0647\u0654\u0647\u0654\u0647\u0654\u0645\u0631\u062D"
       "\u0628\u0627")};
  const Size kCanvasSize(300, 50);
  const int kTestSize = 10;

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kCanvasSize.width(), kCanvasSize.height()));
  cc::SkiaPaintCanvas paint_canvas(bitmap);
  Canvas canvas(&paint_canvas, 1.0f);
  RenderText* render_text = GetRenderText();
  render_text->SetHorizontalAlignment(ALIGN_LEFT);
  render_text->SetColor(SK_ColorBLACK);

  for (auto* string : kTestStrings) {
    paint_canvas.clear(SK_ColorWHITE);
    render_text->SetText(UTF8ToUTF16(string));
    render_text->ApplyBaselineStyle(SUPERSCRIPT, Range(1, 2));
    render_text->ApplyBaselineStyle(SUPERIOR, Range(3, 4));
    render_text->ApplyBaselineStyle(INFERIOR, Range(5, 6));
    render_text->ApplyBaselineStyle(SUBSCRIPT, Range(7, 8));
    const Size string_size = render_text->GetStringSize();
    render_text->SetWeight(Font::Weight::BOLD);
    render_text->SetDisplayRect(
        Rect(kTestSize, kTestSize, string_size.width(), string_size.height()));
    // Allow the RenderText to paint outside of its display rect.
    render_text->set_clip_to_display_rect(false);
    ASSERT_LE(string_size.width() + kTestSize * 2, kCanvasSize.width());

    render_text->Draw(&canvas);
    ASSERT_LT(string_size.width() + kTestSize, kCanvasSize.width());
    const uint32_t* buffer = static_cast<const uint32_t*>(bitmap.getPixels());
    ASSERT_NE(nullptr, buffer);
    TestRectangleBuffer rect_buffer(string, buffer, kCanvasSize.width(),
                                    kCanvasSize.height());
    {
      SCOPED_TRACE("TextDoesntClip Top Side");
      rect_buffer.EnsureSolidRect(SK_ColorWHITE, 0, 0, kCanvasSize.width(),
                                  kTestSize);
    }
    {
      SCOPED_TRACE("TextDoesntClip Bottom Side");
      rect_buffer.EnsureSolidRect(SK_ColorWHITE, 0,
                                  kTestSize + string_size.height(),
                                  kCanvasSize.width(), kTestSize);
    }
    {
      SCOPED_TRACE("TextDoesntClip Left Side");
      // TODO(dschuyler): Smoothing draws to the left of text. This appears to
      // be a preexisting issue that wasn't revealed by the prior unit tests.
      // RenderText currently only uses origins and advances and ignores
      // bounding boxes so cannot account for under- and over-hang.
      rect_buffer.EnsureSolidRect(SK_ColorWHITE, 0, kTestSize, kTestSize - 1,
                                  string_size.height());
    }
    {
      SCOPED_TRACE("TextDoesntClip Right Side");
      // TODO(dschuyler): Smoothing draws to the right of text. This appears to
      // be a preexisting issue that wasn't revealed by the prior unit tests.
      // RenderText currently only uses origins and advances and ignores
      // bounding boxes so cannot account for under- and over-hang.
      rect_buffer.EnsureSolidRect(SK_ColorWHITE,
                                  kTestSize + string_size.width() + 1,
                                  kTestSize, kTestSize - 1,
                                  string_size.height());
    }
  }
}

// Ensure that the text will clip to the display rect. Draws to a canvas and
// checks whether any pixel beyond the bounding rectangle is colored.
TEST_F(RenderTextTest, DISABLED_TextDoesClip) {
  const char* kTestStrings[] = {"TEST", "W", "WWWW", "gAXAXWWWW"};
  const Size kCanvasSize(300, 50);
  const int kTestSize = 10;

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kCanvasSize.width(), kCanvasSize.height()));
  cc::SkiaPaintCanvas paint_canvas(bitmap);
  Canvas canvas(&paint_canvas, 1.0f);
  RenderText* render_text = GetRenderText();
  render_text->SetHorizontalAlignment(ALIGN_LEFT);
  render_text->SetColor(SK_ColorBLACK);

  for (auto* string : kTestStrings) {
    paint_canvas.clear(SK_ColorWHITE);
    render_text->SetText(UTF8ToUTF16(string));
    const Size string_size = render_text->GetStringSize();
    int fake_width = string_size.width() / 2;
    int fake_height = string_size.height() / 2;
    render_text->SetDisplayRect(
        Rect(kTestSize, kTestSize, fake_width, fake_height));
    render_text->set_clip_to_display_rect(true);
    render_text->Draw(&canvas);
    ASSERT_LT(string_size.width() + kTestSize, kCanvasSize.width());
    const uint32_t* buffer = static_cast<const uint32_t*>(bitmap.getPixels());
    ASSERT_NE(nullptr, buffer);
    TestRectangleBuffer rect_buffer(string, buffer, kCanvasSize.width(),
                                    kCanvasSize.height());
    {
      SCOPED_TRACE("TextDoesClip Top Side");
      rect_buffer.EnsureSolidRect(SK_ColorWHITE, 0, 0, kCanvasSize.width(),
                                  kTestSize);
    }

    {
      SCOPED_TRACE("TextDoesClip Bottom Side");
      rect_buffer.EnsureSolidRect(SK_ColorWHITE, 0, kTestSize + fake_height,
                                  kCanvasSize.width(), kTestSize);
    }
    {
      SCOPED_TRACE("TextDoesClip Left Side");
      rect_buffer.EnsureSolidRect(SK_ColorWHITE, 0, kTestSize, kTestSize,
                                  fake_height);
    }
    {
      SCOPED_TRACE("TextDoesClip Right Side");
      rect_buffer.EnsureSolidRect(SK_ColorWHITE, kTestSize + fake_width,
                                  kTestSize, kTestSize, fake_height);
    }
  }
}

// Ensure color changes are picked up by the RenderText implementation.
TEST_F(RenderTextTest, ColorChange) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("x"));
  Draw();
  ExpectTextLog({{1, SK_ColorBLACK}});

  render_text->SetColor(SK_ColorRED);
  Draw();
  ExpectTextLog({{1, SK_ColorRED}});
}

// Ensure style information propagates to the typeface on the text renderer.
TEST_F(RenderTextTest, StylePropagated) {
  RenderText* render_text = GetRenderText();
  // Default-constructed fonts on Mac are system fonts. These can have all kinds
  // of weird weights and style, which are preserved by PlatformFontMac, but do
  // not map simply to a SkTypeface::Style (the full details in SkFontStyle is
  // needed). They also vary depending on the OS version, so set a known font.
  FontList font_list(Font("Arial", 10));

  render_text->SetText(ASCIIToUTF16("x"));
  render_text->SetFontList(font_list);

  DrawVisualText();
  EXPECT_EQ(SkFontStyle::Normal(),
            GetRendererFont().getTypeface()->fontStyle());

  render_text->SetWeight(Font::Weight::BOLD);
  DrawVisualText();
  EXPECT_EQ(SkFontStyle::Bold(), GetRendererFont().getTypeface()->fontStyle());

  render_text->SetStyle(TEXT_STYLE_ITALIC, true);
  DrawVisualText();
  EXPECT_EQ(SkFontStyle::BoldItalic(),
            GetRendererFont().getTypeface()->fontStyle());

  render_text->SetWeight(Font::Weight::NORMAL);
  DrawVisualText();
  EXPECT_EQ(SkFontStyle::Italic(),
            GetRendererFont().getTypeface()->fontStyle());
}

// Ensure the painter adheres to RenderText::subpixel_rendering_suppressed().
TEST_F(RenderTextTest, SubpixelRenderingSuppressed) {
  ASSERT_TRUE(IsFontsSmoothingEnabled())
      << "The test requires that fonts smoothing (anti-aliasing) is activated. "
         "If this assert is failing you need to manually activate the flag in "
         "your system fonts settings.";

  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("x"));

  DrawVisualText();
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID) || \
    defined(OS_FUCHSIA)
  // On Linux, whether subpixel AA is supported is determined by the platform
  // FontConfig. Force it into a particular style after computing runs. Other
  // platforms use a known default FontRenderParams from a static local.
  GetHarfBuzzRunList()
      ->runs()[0]
      ->font_params.render_params.subpixel_rendering =
      FontRenderParams::SUBPIXEL_RENDERING_RGB;
  DrawVisualText();
#endif
  EXPECT_EQ(GetRendererFont().getEdging(), SkFont::Edging::kSubpixelAntiAlias);

  render_text->set_subpixel_rendering_suppressed(true);
  DrawVisualText();
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID) || \
    defined(OS_FUCHSIA)
  // For Linux, runs shouldn't be re-calculated, and the suppression of the
  // SUBPIXEL_RENDERING_RGB set above should now take effect. But, after
  // checking, apply the override anyway to be explicit that it is suppressed.
  EXPECT_NE(GetRendererFont().getEdging(), SkFont::Edging::kSubpixelAntiAlias);
  GetHarfBuzzRunList()
      ->runs()[0]
      ->font_params.render_params.subpixel_rendering =
      FontRenderParams::SUBPIXEL_RENDERING_RGB;
  DrawVisualText();
#endif
  EXPECT_NE(GetRendererFont().getEdging(), SkFont::Edging::kSubpixelAntiAlias);
}

// Ensure the SkFont Edging is computed accurately.
TEST_F(RenderTextTest, SkFontEdging) {
  const auto edging = [this]() { return GetRendererFont().getEdging(); };

  FontRenderParams params;
  EXPECT_TRUE(params.antialiasing);
  EXPECT_EQ(params.subpixel_rendering,
            FontRenderParams::SUBPIXEL_RENDERING_NONE);

  // aa: true, subpixel: false, subpixel_suppressed: false -> kAntiAlias
  renderer()->SetFontRenderParams(params,
                                  false /*subpixel_rendering_suppressed*/);
  EXPECT_EQ(edging(), SkFont::Edging::kAntiAlias);

  // aa: false, subpixel: false, subpixel_suppressed: false -> kAlias
  params.antialiasing = false;
  renderer()->SetFontRenderParams(params,
                                  false /*subpixel_rendering_suppressed*/);
  EXPECT_EQ(edging(), SkFont::Edging::kAlias);

  // aa: true, subpixel: true, subpixel_suppressed: false -> kSubpixelAntiAlias
  params.antialiasing = true;
  params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;
  renderer()->SetFontRenderParams(params,
                                  false /*subpixel_rendering_suppressed*/);
  EXPECT_EQ(edging(), SkFont::Edging::kSubpixelAntiAlias);

  // aa: true, subpixel: true, subpixel_suppressed: true -> kAntiAlias
  renderer()->SetFontRenderParams(params,
                                  true /*subpixel_rendering_suppressed*/);
  EXPECT_EQ(edging(), SkFont::Edging::kAntiAlias);

  // aa: false, subpixel: true, subpixel_suppressed: false -> kAlias
  params.antialiasing = false;
  renderer()->SetFontRenderParams(params,
                                  false /*subpixel_rendering_suppressed*/);
  EXPECT_EQ(edging(), SkFont::Edging::kAlias);
}

// Verify GetWordLookupDataAtPoint returns the correct baseline point and
// decorated word for an LTR string.
TEST_F(RenderTextTest, GetWordLookupDataAtPoint_LTR) {
  // Set an integer glyph width; GetCursorBounds() and
  // GetWordLookupDataAtPoint() use different rounding internally.
  //
  // TODO(crbug.com/1111044): this shouldn't be necessary once RenderText keeps
  // float precision through GetCursorBounds().
  SetGlyphWidth(5);
  const base::string16 ltr = ASCIIToUTF16("  ab  c ");
  const int kWordOneStartIndex = 2;
  const int kWordTwoStartIndex = 6;

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(100, 30));
  render_text->SetText(ltr);
  render_text->ApplyWeight(Font::Weight::SEMIBOLD, Range(0, 3));
  render_text->ApplyStyle(TEXT_STYLE_UNDERLINE, true, Range(1, 5));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, Range(3, 8));
  render_text->ApplyStyle(TEXT_STYLE_STRIKE, true, Range(1, 7));
  const int cursor_y = GetCursorYForTesting();

  const std::vector<FontSpan> font_spans = GetFontSpans();

  // Create expected decorated text instances.
  DecoratedText expected_word_1;
  expected_word_1.text = ASCIIToUTF16("ab");
  // Attributes for the characters 'a' and 'b' at logical indices 2 and 3
  // respectively.
  expected_word_1.attributes.push_back(CreateRangedAttribute(
      font_spans, 0, kWordOneStartIndex, Font::Weight::SEMIBOLD,
      UNDERLINE_MASK | STRIKE_MASK));
  expected_word_1.attributes.push_back(CreateRangedAttribute(
      font_spans, 1, kWordOneStartIndex + 1, Font::Weight::NORMAL,
      UNDERLINE_MASK | ITALIC_MASK | STRIKE_MASK));
  const Rect left_glyph_word_1 = render_text->GetCursorBounds(
      SelectionModel(kWordOneStartIndex, CURSOR_FORWARD), false);

  DecoratedText expected_word_2;
  expected_word_2.text = ASCIIToUTF16("c");
  // Attributes for character 'c' at logical index |kWordTwoStartIndex|.
  expected_word_2.attributes.push_back(
      CreateRangedAttribute(font_spans, 0, kWordTwoStartIndex,
                            Font::Weight::NORMAL, ITALIC_MASK | STRIKE_MASK));
  const Rect left_glyph_word_2 = render_text->GetCursorBounds(
      SelectionModel(kWordTwoStartIndex, CURSOR_FORWARD), false);

  DecoratedText decorated_word;
  Point baseline_point;

  {
    SCOPED_TRACE(base::StringPrintf("Query to the left of text bounds"));
    EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(
        Point(-5, cursor_y), &decorated_word, &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_word_1, decorated_word);
    EXPECT_TRUE(left_glyph_word_1.Contains(baseline_point));
  }
  {
    SCOPED_TRACE(base::StringPrintf("Query to the right of text bounds"));
    EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(
        Point(105, cursor_y), &decorated_word, &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_word_2, decorated_word);
    EXPECT_TRUE(left_glyph_word_2.Contains(baseline_point));
  }

  for (size_t i = 0; i < render_text->text().length(); i++) {
    SCOPED_TRACE(base::StringPrintf("Case[%" PRIuS "]", i));
    // Query the decorated word using the origin of the i'th glyph's bounds.
    const Point query =
        render_text->GetCursorBounds(SelectionModel(i, CURSOR_FORWARD), false)
            .origin();

    EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(query, &decorated_word,
                                                      &baseline_point));

    if (i < kWordTwoStartIndex) {
      VerifyDecoratedWordsAreEqual(expected_word_1, decorated_word);
      EXPECT_TRUE(left_glyph_word_1.Contains(baseline_point));
    } else {
      VerifyDecoratedWordsAreEqual(expected_word_2, decorated_word);
      EXPECT_TRUE(left_glyph_word_2.Contains(baseline_point));
    }
  }
}

// Verify GetWordLookupDataAtPoint returns the correct baseline point and
// decorated word for an RTL string.
TEST_F(RenderTextTest, GetWordLookupDataAtPoint_RTL) {
  // Set an integer glyph width; GetCursorBounds() and
  // GetWordLookupDataAtPoint() use different rounding internally.
  //
  // TODO(crbug.com/1111044): this shouldn't be necessary once RenderText keeps
  // float precision through GetCursorBounds().
  SetGlyphWidth(5);
  const base::string16 rtl = UTF8ToUTF16(" \u0634\u0632  \u0634");
  const int kWordOneStartIndex = 1;
  const int kWordTwoStartIndex = 5;

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(100, 30));
  render_text->SetText(rtl);
  render_text->ApplyWeight(Font::Weight::SEMIBOLD, Range(2, 3));
  render_text->ApplyStyle(TEXT_STYLE_UNDERLINE, true, Range(3, 6));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, Range(0, 3));
  render_text->ApplyStyle(TEXT_STYLE_STRIKE, true, Range(2, 5));
  const int cursor_y = GetCursorYForTesting();

  const std::vector<FontSpan> font_spans = GetFontSpans();

  // Create expected decorated text instance.
  DecoratedText expected_word_1;
  expected_word_1.text = UTF8ToUTF16("\u0634\u0632");
  // Attributes for characters at logical indices 1 and 2.
  expected_word_1.attributes.push_back(CreateRangedAttribute(
      font_spans, 0, kWordOneStartIndex, Font::Weight::NORMAL, ITALIC_MASK));
  expected_word_1.attributes.push_back(
      CreateRangedAttribute(font_spans, 1, kWordOneStartIndex + 1,
                            Font::Weight::SEMIBOLD, ITALIC_MASK | STRIKE_MASK));
  // The leftmost glyph is the one at logical index 2.
  const Rect left_glyph_word_1 = render_text->GetCursorBounds(
      SelectionModel(kWordOneStartIndex + 1, CURSOR_FORWARD), false);

  DecoratedText expected_word_2;
  expected_word_2.text = UTF8ToUTF16("\u0634");
  // Attributes for character at logical index |kWordTwoStartIndex|.
  expected_word_2.attributes.push_back(CreateRangedAttribute(
      font_spans, 0, kWordTwoStartIndex, Font::Weight::NORMAL, UNDERLINE_MASK));
  const Rect left_glyph_word_2 = render_text->GetCursorBounds(
      SelectionModel(kWordTwoStartIndex, CURSOR_FORWARD), false);

  DecoratedText decorated_word;
  Point baseline_point;

  {
    SCOPED_TRACE(base::StringPrintf("Query to the left of text bounds"));
    EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(
        Point(-5, cursor_y), &decorated_word, &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_word_2, decorated_word);
    EXPECT_TRUE(left_glyph_word_2.Contains(baseline_point));
  }
  {
    SCOPED_TRACE(base::StringPrintf("Query to the right of text bounds"));
    EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(
        Point(105, cursor_y), &decorated_word, &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_word_1, decorated_word);
    EXPECT_TRUE(left_glyph_word_1.Contains(baseline_point));
  }

  for (size_t i = 0; i < render_text->text().length(); i++) {
    SCOPED_TRACE(base::StringPrintf("Case[%" PRIuS "]", i));

    // Query the decorated word using the top right point of the i'th glyph's
    // bounds.
    const Point query =
        render_text->GetCursorBounds(SelectionModel(i, CURSOR_FORWARD), false)
            .top_right();

    EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(query, &decorated_word,
                                                      &baseline_point));
    if (i < kWordTwoStartIndex) {
      VerifyDecoratedWordsAreEqual(expected_word_1, decorated_word);
      EXPECT_TRUE(left_glyph_word_1.Contains(baseline_point));
    } else {
      VerifyDecoratedWordsAreEqual(expected_word_2, decorated_word);
      EXPECT_TRUE(left_glyph_word_2.Contains(baseline_point));
    }
  }
}

// Test that GetWordLookupDataAtPoint behaves correctly for multiline text.
TEST_F(RenderTextTest, GetWordLookupDataAtPoint_Multiline) {
  const base::string16 text = ASCIIToUTF16("a b\n..\ncd.");
  const size_t kWordOneIndex = 0;    // Index of character 'a'.
  const size_t kWordTwoIndex = 2;    // Index of character 'b'.
  const size_t kWordThreeIndex = 7;  // Index of character 'c'.

  // Set up render text.
  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetDisplayRect(Rect(500, 500));
  render_text->SetText(text);
  render_text->ApplyWeight(Font::Weight::SEMIBOLD, Range(0, 3));
  render_text->ApplyStyle(TEXT_STYLE_UNDERLINE, true, Range(1, 7));
  render_text->ApplyStyle(TEXT_STYLE_STRIKE, true, Range(1, 8));
  render_text->ApplyStyle(TEXT_STYLE_ITALIC, true, Range(5, 9));

  // Set up test expectations.
  const std::vector<FontSpan> font_spans = GetFontSpans();

  DecoratedText expected_word_1;
  expected_word_1.text = ASCIIToUTF16("a");
  expected_word_1.attributes.push_back(CreateRangedAttribute(
      font_spans, 0, kWordOneIndex, Font::Weight::SEMIBOLD, 0));
  const Rect left_glyph_word_1 =
      GetSubstringBoundsUnion(Range(kWordOneIndex, kWordOneIndex + 1));

  DecoratedText expected_word_2;
  expected_word_2.text = ASCIIToUTF16("b");
  expected_word_2.attributes.push_back(CreateRangedAttribute(
      font_spans, 0, kWordTwoIndex, Font::Weight::SEMIBOLD,
      UNDERLINE_MASK | STRIKE_MASK));
  const Rect left_glyph_word_2 =
      GetSubstringBoundsUnion(Range(kWordTwoIndex, kWordTwoIndex + 1));

  DecoratedText expected_word_3;
  expected_word_3.text = ASCIIToUTF16("cd");
  expected_word_3.attributes.push_back(
      CreateRangedAttribute(font_spans, 0, kWordThreeIndex,
                            Font::Weight::NORMAL, STRIKE_MASK | ITALIC_MASK));
  expected_word_3.attributes.push_back(CreateRangedAttribute(
      font_spans, 1, kWordThreeIndex + 1, Font::Weight::NORMAL, ITALIC_MASK));

  const Rect left_glyph_word_3 =
      GetSubstringBoundsUnion(Range(kWordThreeIndex, kWordThreeIndex + 1));

  DecoratedText decorated_word;
  Point baseline_point;
  {
    // Query to the left of the first line.
    EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(
        Point(-5, GetCursorYForTesting(0)), &decorated_word, &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_word_1, decorated_word);
    EXPECT_TRUE(left_glyph_word_1.Contains(baseline_point));
  }
  {
    // Query on the second line.
    EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(
        Point(5, GetCursorYForTesting(1)), &decorated_word, &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_word_2, decorated_word);
    EXPECT_TRUE(left_glyph_word_2.Contains(baseline_point));
  }
  {
    // Query at the center point of the character 'c'.
    EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(
        left_glyph_word_3.CenterPoint(), &decorated_word, &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_word_3, decorated_word);
    EXPECT_TRUE(left_glyph_word_3.Contains(baseline_point));
  }
  {
    // Query to the right of the third line.
    EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(
        Point(505, GetCursorYForTesting(2)), &decorated_word, &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_word_3, decorated_word);
    EXPECT_TRUE(left_glyph_word_3.Contains(baseline_point));
  }
}

// Verify the boolean return value of GetWordLookupDataAtPoint.
TEST_F(RenderTextTest, GetWordLookupDataAtPoint_Return) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("..."));

  DecoratedText decorated_word;
  Point baseline_point;

  // False should be returned, when the text does not contain any word.
  Point query =
      render_text->GetCursorBounds(SelectionModel(0, CURSOR_FORWARD), false)
          .origin();
  EXPECT_FALSE(render_text->GetWordLookupDataAtPoint(query, &decorated_word,
                                                     &baseline_point));

  render_text->SetText(ASCIIToUTF16("abc"));
  query = render_text->GetCursorBounds(SelectionModel(0, CURSOR_FORWARD), false)
              .origin();
  EXPECT_TRUE(render_text->GetWordLookupDataAtPoint(query, &decorated_word,
                                                    &baseline_point));

  // False should be returned for obscured text.
  render_text->SetObscured(true);
  query = render_text->GetCursorBounds(SelectionModel(0, CURSOR_FORWARD), false)
              .origin();
  EXPECT_FALSE(render_text->GetWordLookupDataAtPoint(query, &decorated_word,
                                                     &baseline_point));
}

// Test that GetLookupDataAtPoint behaves correctly when the range spans lines.
TEST_F(RenderTextTest, GetLookupDataAtRange_Multiline) {
  const base::string16 text = ASCIIToUTF16("a\nb");
  constexpr Range kWordOneRange = Range(0, 1);  // Range of character 'a'.
  constexpr Range kWordTwoRange = Range(2, 3);  // Range of character 'b'.
  constexpr Range kTextRange = Range(0, 3);     // Range of the entire text.

  // Set up render text. Apply style ranges so that each character index gets
  // a corresponding font.
  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetDisplayRect(Rect(500, 500));
  render_text->SetText(text);
  render_text->ApplyWeight(Font::Weight::SEMIBOLD, kWordOneRange);
  render_text->ApplyStyle(TEXT_STYLE_UNDERLINE, true, kWordTwoRange);

  // Set up test expectations.
  const std::vector<FontSpan> font_spans = GetFontSpans();

  DecoratedText expected_word_1;
  expected_word_1.text = ASCIIToUTF16("a");
  expected_word_1.attributes.push_back(CreateRangedAttribute(
      font_spans, 0, kWordOneRange.start(), Font::Weight::SEMIBOLD, 0));
  const Rect left_glyph_word_1 = GetSubstringBoundsUnion(kWordOneRange);

  DecoratedText expected_word_2;
  expected_word_2.text = ASCIIToUTF16("b");
  expected_word_2.attributes.push_back(
      CreateRangedAttribute(font_spans, 0, kWordTwoRange.start(),
                            Font::Weight::NORMAL, UNDERLINE_MASK));
  const Rect left_glyph_word_2 = GetSubstringBoundsUnion(kWordTwoRange);

  DecoratedText expected_entire_text;
  expected_entire_text.text = ASCIIToUTF16("a\nb");
  expected_entire_text.attributes.push_back(
      CreateRangedAttribute(font_spans, kWordOneRange.start(),
                            kWordOneRange.start(), Font::Weight::SEMIBOLD, 0));
  expected_entire_text.attributes.push_back(
      CreateRangedAttribute(font_spans, 1, 1, Font::Weight::NORMAL, 0));
  expected_entire_text.attributes.push_back(CreateRangedAttribute(
      font_spans, kWordTwoRange.start(), kWordTwoRange.start(),
      Font::Weight::NORMAL, UNDERLINE_MASK));

  DecoratedText decorated_word;
  Point baseline_point;
  {
    // Query for the range of the first word.
    EXPECT_TRUE(render_text->GetLookupDataForRange(
        kWordOneRange, &decorated_word, &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_word_1, decorated_word);
    EXPECT_TRUE(left_glyph_word_1.Contains(baseline_point));
  }
  {
    // Query for the range of the second word.
    EXPECT_TRUE(render_text->GetLookupDataForRange(
        kWordTwoRange, &decorated_word, &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_word_2, decorated_word);
    EXPECT_TRUE(left_glyph_word_2.Contains(baseline_point));
  }
  {
    // Query the entire text range.
    EXPECT_TRUE(render_text->GetLookupDataForRange(kTextRange, &decorated_word,
                                                   &baseline_point));
    VerifyDecoratedWordsAreEqual(expected_entire_text, decorated_word);
    EXPECT_TRUE(left_glyph_word_1.Contains(baseline_point));
  }
}

// Tests text selection made at end points of individual lines of multiline
// text.
TEST_F(RenderTextTest, LineEndSelections) {
  const char* const ltr = "abc\n\ndef";
  const char* const rtl = "שנב\n\nגקכ";
  const char* const ltr_single = "abc def ghi";
  const char* const rtl_single = "שנב גקכ עין";
  const int left_x = -100;
  const int right_x = 200;
  struct {
    const char* const text;
    const int line_num;
    const int x;
    const char* const selected_text;
  } cases[] = {
      {ltr, 1, left_x, "abc\n"},
      {ltr, 1, right_x, "abc\n"},
      {ltr, 2, left_x, "abc\n\n"},
      {ltr, 2, right_x, ltr},

      {rtl, 1, left_x, "שנב\n"},
      {rtl, 1, right_x, "שנב\n"},
      {rtl, 2, left_x, rtl},
      {rtl, 2, right_x, "שנב\n\n"},

      {ltr_single, 1, left_x, "abc "},
      {ltr_single, 1, right_x, "abc def "},
      {ltr_single, 2, left_x, "abc def "},
      {ltr_single, 2, right_x, ltr_single},

      {rtl_single, 1, left_x, "שנב גקכ "},
      {rtl_single, 1, right_x, "שנב "},
      {rtl_single, 2, left_x, rtl_single},
      {rtl_single, 2, right_x, "שנב גקכ "},
  };

  SetGlyphWidth(5);
  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetDisplayRect(Rect(20, 1000));

  for (size_t i = 0; i < base::size(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing case %" PRIuS "", i));
    render_text->SetText(UTF8ToUTF16(cases[i].text));

    EXPECT_EQ(3u, render_text->GetNumLines());
    // Position the cursor at the logical beginning of text.
    render_text->SelectRange(Range(0));

    render_text->MoveCursorToPoint(
        Point(cases[i].x, GetCursorYForTesting(cases[i].line_num)), true);
    EXPECT_EQ(UTF8ToUTF16(cases[i].selected_text),
              GetSelectedText(render_text));
  }
}

TEST_F(RenderTextTest, GetSubstringBounds) {
  const float kGlyphWidth = 5;
  SetGlyphWidth(kGlyphWidth);
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("abc"));
  render_text->SetCursorEnabled(false);
  render_text->SetElideBehavior(NO_ELIDE);

  EXPECT_EQ(GetSubstringBoundsUnion(Range(0, 1)).width(), kGlyphWidth);
  EXPECT_EQ(GetSubstringBoundsUnion(Range(1, 2)).width(), kGlyphWidth);
  EXPECT_EQ(GetSubstringBoundsUnion(Range(1, 3)).width(), 2 * kGlyphWidth);

  EXPECT_EQ(GetSubstringBoundsUnion(Range(0, 0)).width(), 0);
  EXPECT_EQ(GetSubstringBoundsUnion(Range(3, 3)).width(), 0);

  // Apply eliding so display text has 2 visible character.
  render_text->SetDisplayRect(Rect(0, 0, 2 * kGlyphWidth, 100));
  render_text->SetElideBehavior(TRUNCATE);

  EXPECT_EQ(GetSubstringBoundsUnion(Range(0, 1)).width(), kGlyphWidth);
  EXPECT_EQ(GetSubstringBoundsUnion(Range(1, 2)).width(), kGlyphWidth);
  EXPECT_EQ(GetSubstringBoundsUnion(Range(1, 3)).width(), kGlyphWidth);
  // Check a fully elided range.
  EXPECT_EQ(GetSubstringBoundsUnion(Range(2, 3)).width(), 0);

  // Empty ranges result in empty rect.
  EXPECT_EQ(GetSubstringBoundsUnion(Range(0, 0)).width(), 0);
  EXPECT_EQ(GetSubstringBoundsUnion(Range(3, 3)).width(), 0);
}

// Tests that GetSubstringBounds rounds outward when glyphs have floating-point
// widths.
TEST_F(RenderTextTest, GetSubstringBoundsFloatingPoint) {
  const float kGlyphWidth = 5.8;
  SetGlyphWidth(kGlyphWidth);
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(200, 1000));
  render_text->SetText(UTF8ToUTF16("abcdef"));
  gfx::Rect bounds = GetSubstringBoundsUnion(Range(1, 2));
  // The bounds should be rounded outwards so that the full substring is always
  // contained in them.
  EXPECT_EQ(base::ClampFloor(kGlyphWidth), bounds.x());
  EXPECT_EQ(base::ClampCeil(2 * kGlyphWidth), bounds.right());
}

// Tests that GetSubstringBounds handles integer glypth widths correctly.
TEST_F(RenderTextTest, GetSubstringBoundsInt) {
  const float kGlyphWidth = 5;
  SetGlyphWidth(kGlyphWidth);
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(200, 1000));
  render_text->SetText(UTF8ToUTF16("abcdef"));
  gfx::Rect bounds = GetSubstringBoundsUnion(Range(1, 2));
  EXPECT_EQ(kGlyphWidth, bounds.x());
  EXPECT_EQ(2 * kGlyphWidth, bounds.right());
}

// Tests that GetSubstringBounds returns the correct bounds for multiline text.
TEST_F(RenderTextTest, GetSubstringBoundsMultiline) {
  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetDisplayRect(Rect(200, 1000));
  render_text->SetText(ASCIIToUTF16("abc\n\ndef"));

  const std::vector<Range> line_char_range = {Range(0, 4), Range(4, 5),
                                              Range(5, 8)};

  // Test bounds for individual lines.
  EXPECT_EQ(3u, render_text->GetNumLines());
  Rect expected_total_bounds;
  for (size_t i = 0; i < test_api()->lines().size(); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing bounds for line %" PRIuS "", i));
    const internal::Line& line = test_api()->lines()[i];
    const Size line_size(std::ceil(line.size.width()),
                         std::ceil(line.size.height()));
    const Rect expected_line_bounds =
        render_text->GetLineOffset(i) + Rect(line_size);
    expected_total_bounds.Union(expected_line_bounds);

    render_text->SelectRange(line_char_range[i]);
    EXPECT_EQ(expected_line_bounds, GetSelectionBoundsUnion());
  }

  // Test complete bounds.
  render_text->SelectAll(false);
  EXPECT_EQ(expected_total_bounds, GetSelectionBoundsUnion());
}

// Tests that RenderText doesn't crash even if it's passed an invalid font. Test
// for crbug.com/668058.
TEST_F(RenderTextTest, InvalidFont) {
  const std::string font_name = "invalid_font";
  const int kFontSize = 13;
  RenderText* render_text = GetRenderText();
  render_text->SetFontList(FontList(Font(font_name, kFontSize)));
  render_text->SetText(ASCIIToUTF16("abc"));

  DrawVisualText();
}

TEST_F(RenderTextTest, ExpandToBeVerticallySymmetric) {
  Rect test_display_rect(0, 0, 400, 100);

  // Basic case.
  EXPECT_EQ(Rect(20, 20, 400, 60),
            test::RenderTextTestApi::ExpandToBeVerticallySymmetric(
                Rect(20, 20, 400, 40), test_display_rect));

  // Expand upwards.
  EXPECT_EQ(Rect(20, 20, 400, 60),
            test::RenderTextTestApi::ExpandToBeVerticallySymmetric(
                Rect(20, 40, 400, 40), test_display_rect));

  // Original rect is entirely above the center point.
  EXPECT_EQ(Rect(10, 30, 200, 40),
            test::RenderTextTestApi::ExpandToBeVerticallySymmetric(
                Rect(10, 30, 200, 10), test_display_rect));

  // Original rect is below the display rect entirely.
  EXPECT_EQ(Rect(10, -10, 200, 120),
            test::RenderTextTestApi::ExpandToBeVerticallySymmetric(
                Rect(10, 100, 200, 10), test_display_rect));

  // Sanity check that we can handle a display rect with a non-zero origin.
  test_display_rect.Offset(10, 10);
  EXPECT_EQ(Rect(20, 20, 400, 80),
            test::RenderTextTestApi::ExpandToBeVerticallySymmetric(
                Rect(20, 20, 400, 40), test_display_rect));
}

TEST_F(RenderTextTest, MergeIntersectingRects) {
  // Basic case.
  std::vector<Rect> test_rects{Rect(0, 0, 10, 10), Rect(5, 0, 10, 10),
                               Rect(10, 0, 5, 10), Rect(12, 0, 5, 10)};
  test::RenderTextTestApi::MergeIntersectingRects(test_rects);
  ASSERT_EQ(1u, test_rects.size());
  EXPECT_EQ(Rect(0, 0, 17, 10), test_rects[0]);

  // Case where some rects intersect and some don't.
  test_rects = std::vector<Rect>{Rect(0, 0, 10, 10), Rect(5, 0, 10, 10),
                                 Rect(16, 0, 10, 10), Rect(25, 0, 10, 10),
                                 Rect(40, 0, 10, 10)};
  test::RenderTextTestApi::MergeIntersectingRects(test_rects);
  ASSERT_EQ(3u, test_rects.size());
  EXPECT_EQ(Rect(0, 0, 15, 10), test_rects[0]);
  EXPECT_EQ(Rect(16, 0, 19, 10), test_rects[1]);
  EXPECT_EQ(Rect(40, 0, 10, 10), test_rects[2]);

  // Case where no rects intersect.
  test_rects = std::vector<Rect>{Rect(0, 0, 10, 10), Rect(11, 0, 10, 10),
                                 Rect(22, 0, 10, 10), Rect(33, 0, 10, 10)};
  test::RenderTextTestApi::MergeIntersectingRects(test_rects);
  ASSERT_EQ(4u, test_rects.size());
  EXPECT_EQ(Rect(0, 0, 10, 10), test_rects[0]);
  EXPECT_EQ(Rect(11, 0, 10, 10), test_rects[1]);
  EXPECT_EQ(Rect(22, 0, 10, 10), test_rects[2]);
  EXPECT_EQ(Rect(33, 0, 10, 10), test_rects[3]);

  // Rects are out-of-order.
  test_rects = std::vector<Rect>{Rect(10, 0, 5, 10), Rect(0, 0, 10, 10),
                                 Rect(12, 0, 5, 10), Rect(5, 0, 10, 10)};
  test::RenderTextTestApi::MergeIntersectingRects(test_rects);
  ASSERT_EQ(1u, test_rects.size());
  EXPECT_EQ(Rect(0, 0, 17, 10), test_rects[0]);

  // The first 3 rects are adjacent horizontally. The 4th rect is adjacent to
  // the 3rd rect vertically, but is not merged. The last rect is adjacent to
  // the 4th rect.
  test_rects = std::vector<Rect>{Rect(0, 0, 10, 10), Rect(10, 0, 10, 10),
                                 Rect(20, 0, 10, 10), Rect(20, 10, 10, 10),
                                 Rect(30, 10, 10, 10)};
  test::RenderTextTestApi::MergeIntersectingRects(test_rects);
  ASSERT_EQ(2u, test_rects.size());
  EXPECT_EQ(Rect(0, 0, 30, 10), test_rects[0]);
  EXPECT_EQ(Rect(20, 10, 20, 10), test_rects[1]);
}

// Ensures that text is centered vertically and consistently when either the
// display rectangle height changes, or when the minimum line height changes.
// The difference between the two is the selection rectangle, which should match
// the line height.
TEST_F(RenderTextTest, BaselineWithLineHeight) {
  RenderText* render_text = GetRenderText();
  const int font_height = render_text->font_list().GetHeight();
  render_text->SetDisplayRect(Rect(500, font_height));
  render_text->SetText(ASCIIToUTF16("abc"));

  // Select everything so the test can use GetSelectionBoundsUnion().
  render_text->SelectAll(false);

  const int normal_baseline = test_api()->GetDisplayTextBaseline();
  ASSERT_EQ(1u, test_api()->lines().size());
  EXPECT_EQ(font_height, test_api()->lines()[0].size.height());

  // With a matching display height, the baseline calculated using font metrics
  // and the baseline from the layout engine should agree. This works because
  // the text string is simple (exotic glyphs may use fonts with different
  // metrics).
  EXPECT_EQ(normal_baseline, render_text->GetBaseline());
  EXPECT_EQ(0, render_text->GetLineOffset(0).y());

  const gfx::Rect normal_selection_bounds = GetSelectionBoundsUnion();

  // Sanity check: selection should start from (0,0).
  EXPECT_EQ(gfx::Vector2d(), normal_selection_bounds.OffsetFromOrigin());

  constexpr int kDelta = 16;

  // Grow just the display rectangle.
  render_text->SetDisplayRect(Rect(500, font_height + kDelta));

  // The display text baseline does not move: GetLineOffset() moves it instead.
  EXPECT_EQ(normal_baseline, test_api()->GetDisplayTextBaseline());

  ASSERT_EQ(1u, test_api()->lines().size());
  EXPECT_EQ(font_height, test_api()->lines()[0].size.height());

  // Save the baseline calculated using the display rectangle before enabling
  // multi-line or SetMinLineHeight().
  const int reported_baseline = render_text->GetBaseline();
  const int baseline_shift = reported_baseline - normal_baseline;

  // When line height matches font height, this should match the line offset.
  EXPECT_EQ(baseline_shift, render_text->GetLineOffset(0).y());

  // The actual shift depends on font metrics, and the calculations done in
  // RenderText::DetermineBaselineCenteringText(). Do a sanity check that the
  // "centering" part is happening within some tolerance by ensuring the shift
  // is within a pixel of (kDelta / 2). That is, 7, 8 or 9 pixels (for a delta
  // of 16). An unusual font in future may need more leeway.
  constexpr int kFuzz = 1;  // If the next EXPECT fails, try increasing this.
  EXPECT_LE(abs(baseline_shift - kDelta / 2), kFuzz);

  // Increasing display height (but not line height) should shift the selection
  // bounds down by |baseline_shift|, but leave a matching size.
  gfx::Rect current_selection_bounds = GetSelectionBoundsUnion();
  EXPECT_EQ(baseline_shift, current_selection_bounds.y());
  EXPECT_EQ(0, current_selection_bounds.x());
  EXPECT_EQ(normal_selection_bounds.size(), current_selection_bounds.size());

  // Now increase the line height, but remain single-line. Note the line height
  // now matches the display rect.
  render_text->SetMinLineHeight(font_height + kDelta);
  int display_text_baseline = test_api()->GetDisplayTextBaseline();
  ASSERT_EQ(1u, test_api()->lines().size());
  EXPECT_EQ(font_height + kDelta, test_api()->lines()[0].size.height());

  // The line offset should go back to zero, but now the display text baseline
  // should shift down to compensate, and the shift amount should match.
  EXPECT_EQ(0, render_text->GetLineOffset(0).y());
  EXPECT_EQ(normal_baseline + baseline_shift, display_text_baseline);

  // Now selection bounds should grow in height, but not shift its origin.
  current_selection_bounds = GetSelectionBoundsUnion();
  EXPECT_EQ(font_height + kDelta, current_selection_bounds.height());
  EXPECT_EQ(normal_selection_bounds.width(), current_selection_bounds.width());
  EXPECT_EQ(gfx::Vector2d(), current_selection_bounds.OffsetFromOrigin());

  // Flipping the multiline flag should change nothing.
  render_text->SetMultiline(true);
  display_text_baseline = test_api()->GetDisplayTextBaseline();
  ASSERT_EQ(1u, test_api()->lines().size());
  EXPECT_EQ(font_height + kDelta, test_api()->lines()[0].size.height());
  EXPECT_EQ(0, render_text->GetLineOffset(0).y());
  EXPECT_EQ(normal_baseline + baseline_shift, display_text_baseline);
  current_selection_bounds = GetSelectionBoundsUnion();
  EXPECT_EQ(font_height + kDelta, current_selection_bounds.height());
  EXPECT_EQ(normal_selection_bounds.width(), current_selection_bounds.width());
  EXPECT_EQ(gfx::Vector2d(), current_selection_bounds.OffsetFromOrigin());
}

TEST_F(RenderTextTest, TeluguGraphemeBoundaries) {
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(50, 1000));
  // This is first syllable of the Telugu word for "New" in Chrome. It's a
  // string of 4 UTF-8 characters: [క,్,ర,ొ]. When typeset with a supporting
  // font, the second and fourth characters become diacritical marks for the
  // first and third characters to form two graphemes. Then, these graphemes
  // combine into a ligature "cluster". But, unlike ligatures in English (e.g.
  // the "ffl" in "waffle"), this Telugu ligature is laid out vertically, with
  // both graphemes occupying the same horizontal space.
  render_text->SetText(UTF8ToUTF16("క్రొ"));

  const int whole_width = render_text->GetStringSize().width();
  // Sanity check. A typical width is 8 pixels. Anything less than 6 could screw
  // up the checks below with rounding.
  EXPECT_LE(6, whole_width);

  // Go to the end and perform Shift+Left. The selection should jump to a
  // grapheme boundary.
  // Before ICU 65, this was in the center of the glyph, but now it encompasses
  // the entire glyph.
  render_text->SetCursorPosition(4);
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_RETAIN);
  EXPECT_EQ(Range(4, 0), render_text->selection());

  // The cursor is already at the boundary, so there should be no change.
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_RETAIN);
  EXPECT_EQ(Range(4, 0), render_text->selection());

  // The selection should cover the entire width.
  Rect selection_bounds = GetSelectionBoundsUnion();
  EXPECT_EQ(0, selection_bounds.x());
  EXPECT_EQ(whole_width, selection_bounds.width());
}

// Test cursor bounds for Emoji flags (unicode regional indicators) when the
// flag does not merge into a single glyph.
TEST_F(RenderTextTest, MissingFlagEmoji) {
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(1000, 1000));

  // Usually these pair into country codes, but for this test we do not want
  // them to combine into a flag. Instead, the placeholder glyphs should be used
  // but cursor navigation should still behave as though they are joined. To get
  // placeholder glyphs, make up a non-existent country. The codes used are
  // based on ISO 3166-1 alpha-2. Codes starting with X are user-assigned.
  base::string16 text(WideToUTF16(L"🇽🇽🇽🇽"));
  // Each flag is 4 UTF16 characters (2 surrogate pair code points).
  EXPECT_EQ(8u, text.length());

  render_text->SetText(text);

  const int whole_width = render_text->GetStringSize().width();
  const int half_width = whole_width / 2;
  EXPECT_LE(6, whole_width);  // Sanity check.

  EXPECT_EQ("[0->7]", GetRunListStructureString());

  // Move from the left to the right.
  const Rect start_cursor = render_text->GetUpdatedCursorBounds();
  EXPECT_EQ(0, start_cursor.x());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(4, 4), render_text->selection());
  const Rect middle_cursor = render_text->GetUpdatedCursorBounds();

  // Should move about half way. Cursor bounds round to the nearest integer, so
  // account for that.
  EXPECT_LE(half_width - 1, middle_cursor.x());
  EXPECT_GE(half_width + 1, middle_cursor.x());

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(8, 8), render_text->selection());
  const Rect end_cursor = render_text->GetUpdatedCursorBounds();
  EXPECT_LE(whole_width - 1, end_cursor.x());  // Should move most of the way.
  EXPECT_GE(whole_width + 1, end_cursor.x());

  // Move right to left.
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(4, 4), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(0, 0), render_text->selection());

  // Select from the left to the right. The first "flag" should be selected.
  // Note cursor bounds and selection bounds differ on integer rounding - see
  // http://crbug.com/735346.
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_RETAIN);
  EXPECT_EQ(Range(0, 4), render_text->selection());
  Rect selection_bounds = GetSelectionBoundsUnion();
  EXPECT_EQ(0, selection_bounds.x());
  EXPECT_LE(half_width - 1, selection_bounds.width());  // Allow for rounding.
  EXPECT_GE(half_width + 1, selection_bounds.width());

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_RETAIN);
  EXPECT_EQ(Range(0, 8), render_text->selection());
  selection_bounds = GetSelectionBoundsUnion();
  EXPECT_EQ(0, selection_bounds.x());
  EXPECT_EQ(whole_width, selection_bounds.width());
}

// Ensures that glyph spacing is correctly applied to obscured text.
TEST_F(RenderTextTest, ObscuredGlyphSpacing) {
  const base::string16 seuss = ASCIIToUTF16("hop on pop");
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetText(seuss);
  render_text->SetObscured(true);

  // The default glyph spacing is zero.
  const int width_without_glyph_spacing = render_text->GetStringSize().width();
  EXPECT_EQ(0, render_text->obscured_glyph_spacing());

  constexpr int kObscuredGlyphSpacing = 5;
  render_text->SetObscuredGlyphSpacing(kObscuredGlyphSpacing);
  const int width_with_glyph_spacing = render_text->GetStringSize().width();
  EXPECT_EQ(kObscuredGlyphSpacing, render_text->obscured_glyph_spacing());

  EXPECT_EQ(width_without_glyph_spacing +
                static_cast<int>(seuss.length()) * kObscuredGlyphSpacing,
            width_with_glyph_spacing);
}

// Ensures that glyph spacing is ignored for non-obscured text.
TEST_F(RenderTextTest, ObscuredGlyphSpacingOnNonObscuredText) {
  const base::string16 seuss = ASCIIToUTF16("hop on pop");
  RenderTextHarfBuzz* render_text = GetRenderText();
  render_text->SetText(seuss);
  render_text->SetObscured(false);
  const int width_without_glyph_spacing = render_text->GetStringSize().width();

  constexpr int kObscuredGlyphSpacing = 5;
  render_text->SetObscuredGlyphSpacing(kObscuredGlyphSpacing);
  const int width_with_glyph_spacing = render_text->GetStringSize().width();
  EXPECT_EQ(width_without_glyph_spacing, width_with_glyph_spacing);
}

// Ensure font size overrides propagate through to text runs.
TEST_F(RenderTextTest, FontSizeOverride) {
  RenderTextHarfBuzz* render_text = GetRenderText();
  const int default_font_size = render_text->font_list().GetFontSize();
  const int test_font_size_override = default_font_size + 5;
  render_text->SetText(ASCIIToUTF16("0123456789"));
  render_text->ApplyFontSizeOverride(test_font_size_override, gfx::Range(3, 7));
  EXPECT_EQ(ToString16Vec({"012", "3456", "789"}), GetRunListStrings());

  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  ASSERT_EQ(3U, run_list->size());

  EXPECT_EQ(default_font_size,
            run_list->runs()[0].get()->font_params.font_size);
  EXPECT_EQ(test_font_size_override,
            run_list->runs()[1].get()->font_params.font_size);
  EXPECT_EQ(default_font_size,
            run_list->runs()[2].get()->font_params.font_size);
}

TEST_F(RenderTextTest, DrawVisualText_WithSelection) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("TheRedElephantIsEatingMyPumpkin"));
  // Ensure selected text is drawn differently than unselected text.
  render_text->set_selection_color(SK_ColorRED);
  DrawVisualText({{3, 14}});
  ExpectTextLog({{3, SK_ColorBLACK}, {11, SK_ColorRED}, {17, SK_ColorBLACK}});
}

TEST_F(RenderTextTest, DrawVisualText_WithSelectionOnObcuredEmoji) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(WideToUTF16(L"\U0001F628\U0001F628\U0001F628"));
  render_text->SetObscured(true);
  render_text->set_selection_color(SK_ColorRED);
  DrawVisualText({{4, 6}});
  ExpectTextLog({{2, SK_ColorBLACK}, {1, SK_ColorRED}});
}

TEST_F(RenderTextTest, DrawSelectAll) {
  const std::vector<GlyphCountAndColor> kUnselected = {
      {4, SK_ColorBLACK}};
  const std::vector<GlyphCountAndColor> kSelected = {
      {4, SK_ColorRED}};
  const std::vector<GlyphCountAndColor> kFocused = {
      {1, SK_ColorBLACK}, {2, SK_ColorRED}, {1, SK_ColorBLACK}};

  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("Test"));
  render_text->set_selection_color(SK_ColorRED);
  render_text->SelectRange(Range(1, 3));

  Draw(false);
  ExpectTextLog(kUnselected);
  Draw(true);
  ExpectTextLog(kSelected);
  Draw(false);
  ExpectTextLog(kUnselected);

  render_text->set_focused(true);
  Draw(false);
  ExpectTextLog(kFocused);
  Draw(true);
  ExpectTextLog(kSelected);

  render_text->set_focused(false);
  Draw(true);
  ExpectTextLog(kSelected);
  Draw(false);
  ExpectTextLog(kUnselected);
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
TEST_F(RenderTextTest, StringSizeUpdatedWhenDeviceScaleFactorChanges) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(ASCIIToUTF16("Test - 1"));
  const gfx::SizeF initial_size = render_text->GetStringSizeF();

  // Non-integer device scale factor enables subpixel positioning on Linux and
  // Chrome OS, which should update text size.
  SetFontRenderParamsDeviceScaleFactor(1.5);

  const gfx::SizeF scaled_size = render_text->GetStringSizeF();

  // Create render text with scale factor set from the beginning, and use is as
  // a baseline to which compare the original render text string size.
  ResetRenderTextInstance();
  RenderText* scaled_render_text = GetRenderText();
  scaled_render_text->SetText(ASCIIToUTF16("Test - 1"));

  // Verify that original render text string size got updated after device scale
  // factor changed.
  EXPECT_NE(initial_size.width(), scaled_size.width());
  EXPECT_EQ(scaled_render_text->GetStringSizeF(), scaled_size);
}
#endif

}  // namespace gfx
