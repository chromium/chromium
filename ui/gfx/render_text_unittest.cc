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
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
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
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/range/range_f.h"
#include "ui/gfx/render_text_harfbuzz.h"
#include "ui/gfx/render_text_test_api.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/text_utils.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/gfx/platform_font_win.h"
#endif

#if defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>

#include "base/mac/mac_util.h"
#include "ui/gfx/render_text_mac.h"
#endif

using base::UTF8ToUTF16;
using base::WideToUTF16;

namespace gfx {

namespace {

// An enum specifying the different RenderText implementations to be tested.
enum RenderTextBackend {
  RENDER_TEXT_HARFBUZZ,
  RENDER_TEXT_MAC,
};

// Parameterised test name generator for tests depending on RenderTextBackend.
struct PrintRenderTextBackend {
  template <class ParamType>
  std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
    switch (info.param) {
      case RENDER_TEXT_HARFBUZZ:
        return "HarfBuzz";
      case RENDER_TEXT_MAC:
        return "Mac";
    }
    NOTREACHED();
    return std::string();
  }
};

// Various weak, LTR, RTL, and Bidi string cases with three characters each.
const char kWeak[] = " . ";
const char kLtr[] = "abc";
const char kRtl[] = "\u05d0\u05d1\u05d2";
const char kLtrRtl[] = "a\u05d0\u05d1";
const char kLtrRtlLtr[] = "a\u05d1b";
const char kRtlLtr[] = "\u05d0\u05d1a";
const char kRtlLtrRtl[] = "\u05d0a\u05d1";

// Bitmasks based on gfx::TextStyle.
enum {
  ITALIC_MASK = 1 << ITALIC,
  STRIKE_MASK = 1 << STRIKE,
  UNDERLINE_MASK = 1 << UNDERLINE,
};

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
    const std::vector<RenderText::FontSpan>& font_spans,
    int index,
    int font_index,
    Font::Weight weight,
    int style_mask) {
  const auto iter =
      std::find_if(font_spans.begin(), font_spans.end(),
                   [font_index](const RenderText::FontSpan& span) {
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

// The class which records the drawing operations so that the test case can
// verify where exactly the glyphs are drawn.
class TestSkiaTextRenderer : public internal::SkiaTextRenderer {
 public:
  struct TextLog {
    TextLog() : glyph_count(0u), color(SK_ColorTRANSPARENT) {}
    PointF origin;
    size_t glyph_count;
    SkColor color;
  };

  explicit TestSkiaTextRenderer(Canvas* canvas)
      : internal::SkiaTextRenderer(canvas) {}
  ~TestSkiaTextRenderer() override {}

  void GetTextLogAndReset(std::vector<TextLog>* text_log) {
    text_log_.swap(*text_log);
    text_log_.clear();
  }

 private:
  // internal::SkiaTextRenderer:
  void DrawPosText(const SkPoint* pos,
                   const uint16_t* glyphs,
                   size_t glyph_count) override {
    TextLog log_entry;
    log_entry.glyph_count = glyph_count;
    if (glyph_count > 0) {
      log_entry.origin =
          PointF(SkScalarToFloat(pos[0].x()), SkScalarToFloat(pos[0].y()));
      for (size_t i = 1U; i < glyph_count; ++i) {
        log_entry.origin.SetToMin(
            PointF(SkScalarToFloat(pos[i].x()), SkScalarToFloat(pos[i].y())));
      }
    }
    log_entry.color =
        test::RenderTextTestApi::GetRendererPaint(this).getColor();
    text_log_.push_back(log_entry);
    internal::SkiaTextRenderer::DrawPosText(pos, glyphs, glyph_count);
  }

  std::vector<TextLog> text_log_;

  DISALLOW_COPY_AND_ASSIGN(TestSkiaTextRenderer);
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
class RenderTextTest : public testing::Test,
                       public ::testing::WithParamInterface<RenderTextBackend> {
 public:
  RenderTextTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        render_text_(CreateRenderTextInstance()),
        test_api_(new test::RenderTextTestApi(render_text_.get())),
        renderer_(canvas()) {}

 protected:
  std::unique_ptr<RenderText> CreateRenderTextInstance() const {
    switch (GetParam()) {
      case RENDER_TEXT_HARFBUZZ:
        return std::make_unique<RenderTextHarfBuzz>();

      case RENDER_TEXT_MAC:
#if defined(OS_MACOSX)
        return std::make_unique<RenderTextMac>();
#else
        break;
#endif
    }
    NOTREACHED();
    return nullptr;
  }

  cc::PaintFlags& GetRendererPaint() {
    return test::RenderTextTestApi::GetRendererPaint(renderer());
  }

  void DrawVisualText() { test_api_->DrawVisualText(renderer()); }

  const internal::TextRunList* GetHarfBuzzRunList() const {
    DCHECK_EQ(RENDER_TEXT_HARFBUZZ, GetParam());
    return test_api_->GetHarfBuzzRunList();
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
  std::string GetRunListStructureString() const {
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
  std::vector<base::string16> GetRunListStrings() const {
    std::vector<base::string16> runs_as_text;
    const std::vector<RenderText::FontSpan> spans =
        render_text_->GetFontSpansForTesting();
    for (const auto& span : spans) {
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
    render_text_ = CreateRenderTextInstance();
    test_api_.reset(new test::RenderTextTestApi(GetRenderText()));
  }

  RenderText* GetRenderText() { return render_text_.get(); };

  RenderTextHarfBuzz* GetRenderTextHarfBuzz() {
    DCHECK_EQ(RENDER_TEXT_HARFBUZZ, GetParam());
    return static_cast<RenderTextHarfBuzz*>(GetRenderText());
  }

#if defined(OS_MACOSX)
  RenderTextMac* GetRenderTextMac() {
    DCHECK_EQ(RENDER_TEXT_MAC, GetParam());
    return static_cast<RenderTextMac*>(GetRenderText());
  }
#endif

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
    test_api()->EnsureLayout();
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

  Canvas* canvas() { return &canvas_; }
  TestSkiaTextRenderer* renderer() { return &renderer_; }
  test::RenderTextTestApi* test_api() { return test_api_.get(); };

 private:
  // Needed to bypass DCHECK in GetFallbackFont.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<RenderText> render_text_;
  std::unique_ptr<test::RenderTextTestApi> test_api_;
  Canvas canvas_;
  TestSkiaTextRenderer renderer_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextTest);
};

// Test fixture class. Use for tests which are only to be run for
// RenderTextHarfBuzz.
class RenderTextHarfBuzzTest : public RenderTextTest {
 public:
  RenderTextHarfBuzzTest() {}

  // Overridden from testing::Test:
  void SetUp() override {
    RenderTextTest::SetUp();
    DCHECK_EQ(RENDER_TEXT_HARFBUZZ, GetParam());
  }

 protected:
  void SetGlyphWidth(float test_width) {
    test_api()->SetGlyphWidth(test_width);
  }

  bool ShapeRunWithFont(const base::string16& text,
                        const Font& font,
                        const FontRenderParams& render_params,
                        internal::TextRunHarfBuzz* run) {
    internal::TextRunHarfBuzz::FontParams font_params = run->font_params;
    font_params.ComputeRenderParamsFontSizeAndBaselineOffset();
    font_params.SetFontAndRenderParams(font, render_params);
    run->shape.missing_glyph_count = static_cast<size_t>(-1);
    std::vector<internal::TextRunHarfBuzz*> runs = {run};
    GetRenderTextHarfBuzz()->ShapeRunsWithFont(text, font_params, &runs);
    return runs.empty();
  }

  int GetCursorYForTesting(int line_num = 0) {
    return GetRenderText()->GetLineOffset(line_num).y() + 1;
  }

  // Do not use this function to ensure layout. This is only used to run a
  // subset of the EnsureLayout functionality and check intermediate state.
  void EnsureLayoutRunList() { GetRenderTextHarfBuzz()->EnsureLayoutRunList(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderTextHarfBuzzTest);
};

#if defined(OS_MACOSX)
// Test fixture class. Use for tests which are only to be run for RenderTextMac.
class RenderTextMacTest : public RenderTextTest {
 public:
  RenderTextMacTest() {}

  // Overridden from testing::Test:
  void SetUp() override {
    RenderTextTest::SetUp();
    DCHECK_EQ(RENDER_TEXT_MAC, GetParam());
  }

 protected:
  base::ScopedCFTypeRef<CTLineRef> GetCoreTextLine() {
    return GetRenderTextMac()->line_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderTextMacTest);
};
#endif

TEST_P(RenderTextTest, DefaultStyles) {
  // Check the default styles applied to new instances and adjusted text.
  RenderText* render_text = GetRenderText();
  EXPECT_TRUE(render_text->text().empty());
  const char* const cases[] = {kWeak, kLtr, "Hello", kRtl, "", ""};
  for (size_t i = 0; i < arraysize(cases); ++i) {
    EXPECT_TRUE(test_api()->colors().EqualsValueForTesting(SK_ColorBLACK));
    EXPECT_TRUE(test_api()->baselines().EqualsValueForTesting(NORMAL_BASELINE));
    EXPECT_TRUE(test_api()->font_size_overrides().EqualsValueForTesting(0));
    for (size_t style = 0; style < NUM_TEXT_STYLES; ++style)
      EXPECT_TRUE(test_api()->styles()[style].EqualsValueForTesting(false));
    render_text->SetText(UTF8ToUTF16(cases[i]));
  }
}

TEST_P(RenderTextTest, SetStyles) {
  // Ensure custom default styles persist across setting and clearing text.
  RenderText* render_text = GetRenderText();
  const SkColor color = SK_ColorRED;
  render_text->SetColor(color);
  render_text->SetBaselineStyle(SUPERSCRIPT);
  render_text->SetWeight(Font::Weight::BOLD);
  render_text->SetStyle(UNDERLINE, false);
  const char* const cases[] = {kWeak, kLtr, "Hello", kRtl, "", ""};
  for (size_t i = 0; i < arraysize(cases); ++i) {
    EXPECT_TRUE(test_api()->colors().EqualsValueForTesting(color));
    EXPECT_TRUE(test_api()->baselines().EqualsValueForTesting(SUPERSCRIPT));
    EXPECT_TRUE(
        test_api()->weights().EqualsValueForTesting(Font::Weight::BOLD));
    EXPECT_TRUE(test_api()->styles()[UNDERLINE].EqualsValueForTesting(false));
    render_text->SetText(UTF8ToUTF16(cases[i]));

    // Ensure custom default styles can be applied after text has been set.
    if (i == 1)
      render_text->SetStyle(STRIKE, true);
    if (i >= 1)
      EXPECT_TRUE(test_api()->styles()[STRIKE].EqualsValueForTesting(true));
  }
}

TEST_P(RenderTextTest, ApplyStyles) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("012345678"));

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
  render_text->ApplyStyle(ITALIC, true, Range(0, 2));
  render_text->ApplyStyle(ITALIC, true, Range(3, 6));
  render_text->ApplyStyle(ITALIC, true, Range(7, text_length));
  std::vector<std::pair<size_t, bool>> expected_italic = {
      {0, true}, {2, false}, {3, true}, {6, false}, {7, true}};
  EXPECT_TRUE(test_api()->styles()[ITALIC].EqualsForTesting(expected_italic));

  // Changing the text should clear any breaks except for the first one.
  render_text->SetText(UTF8ToUTF16("0123456"));
  expected_italic.resize(1);
  EXPECT_TRUE(test_api()->styles()[ITALIC].EqualsForTesting(expected_italic));
  render_text->ApplyStyle(ITALIC, false, Range(2, 4));
  render_text->SetText(UTF8ToUTF16("012345678"));
  EXPECT_TRUE(test_api()->styles()[ITALIC].EqualsForTesting(expected_italic));
  render_text->ApplyStyle(ITALIC, false, Range(0, 1));
  render_text->SetText(UTF8ToUTF16("0123456"));
  expected_italic.begin()->second = false;
  EXPECT_TRUE(test_api()->styles()[ITALIC].EqualsForTesting(expected_italic));
  render_text->ApplyStyle(ITALIC, true, Range(2, 4));
  render_text->SetText(UTF8ToUTF16("012345678"));
  EXPECT_TRUE(test_api()->styles()[ITALIC].EqualsForTesting(expected_italic));

  // TODO(tmoniuszko): Enable when RenderTextMac::IsValidCursorIndex is
  //                   implemented. See http://crbug.com/131618.
  if (GetParam() != RENDER_TEXT_MAC) {
    // Styles shouldn't be changed mid-grapheme.
    render_text->SetText(UTF8ToUTF16("0\u0915\u093f1\u0915\u093f2"));
    render_text->ApplyStyle(UNDERLINE, true, Range(2, 5));
    EXPECT_TRUE(test_api()->styles()[UNDERLINE].EqualsForTesting(
        {{0, false}, {1, true}, {6, false}}));
  }
}

TEST_P(RenderTextTest, AppendTextKeepsStyles) {
  RenderText* render_text = GetRenderText();
  // Setup basic functionality.
  render_text->SetText(UTF8ToUTF16("abcd"));
  render_text->ApplyColor(SK_ColorRED, Range(0, 1));
  render_text->ApplyBaselineStyle(SUPERSCRIPT, Range(1, 2));
  render_text->ApplyStyle(UNDERLINE, true, Range(2, 3));
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
  EXPECT_TRUE(test_api()->styles()[UNDERLINE].EqualsForTesting(expected_style));
  const std::vector<std::pair<size_t, int>> expected_font_size = {{0, 0},
                                                                  {3, 20}};
  EXPECT_TRUE(
      test_api()->font_size_overrides().EqualsForTesting(expected_font_size));

  // Ensure AppendText maintains current text styles.
  render_text->AppendText(UTF8ToUTF16("efg"));
  EXPECT_EQ(render_text->GetDisplayText(), UTF8ToUTF16("abcdefg"));
  EXPECT_TRUE(test_api()->colors().EqualsForTesting(expected_color));
  EXPECT_TRUE(test_api()->baselines().EqualsForTesting(expected_baseline));
  EXPECT_TRUE(test_api()->styles()[UNDERLINE].EqualsForTesting(expected_style));
  EXPECT_TRUE(
      test_api()->font_size_overrides().EqualsForTesting(expected_font_size));
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, ObscuredText) {
  const base::string16 seuss = UTF8ToUTF16("hop on pop");
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
  EXPECT_EQ(0U, test_api()->TextIndexToDisplayIndex(0U));
  EXPECT_EQ(1U, test_api()->TextIndexToDisplayIndex(1U));
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
            static_cast<int>(render_text->GetCursorSpan({0, 1}).length()));

  // Cursoring is independent of underlying characters when text is obscured.
  const char* const texts[] = {
      kWeak, kLtr, kLtrRtl, kLtrRtlLtr, kRtl, kRtlLtr, kRtlLtrRtl,
      "hop on pop",                              // Check LTR word boundaries.
      "\u05d0\u05d1 \u05d0\u05d2 \u05d1\u05d2",  // Check RTL word boundaries.
  };
  for (size_t i = 0; i < arraysize(texts); ++i) {
    base::string16 text = UTF8ToUTF16(texts[i]);
    TestVisualCursorMotionInObscuredField(render_text, text, SELECTION_NONE);
    TestVisualCursorMotionInObscuredField(render_text, text, SELECTION_RETAIN);
  }
}

TEST_P(RenderTextTest, RevealObscuredText) {
  const base::string16 seuss = UTF8ToUTF16("hop on pop");
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
  render_text->SetText(UTF8ToUTF16("new"));
  EXPECT_EQ(GetObscuredString(3), render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(2);
  EXPECT_EQ(GetObscuredString(3, 2, 'w'), render_text->GetDisplayText());
  render_text->SetText(UTF8ToUTF16("new longer"));
  EXPECT_EQ(GetObscuredString(10), render_text->GetDisplayText());

  // Text with invalid surrogates.
  const base::char16 invalid_surrogates[] = {0xDC00, 0xD800, 'h', 'o', 'p', 0};
  render_text->SetText(invalid_surrogates);
  EXPECT_EQ(GetObscuredString(5), render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(0);
  EXPECT_EQ(GetObscuredString(5, 0, 0xDC00), render_text->GetDisplayText());
  render_text->RenderText::SetObscuredRevealIndex(1);
  EXPECT_EQ(GetObscuredString(5, 1, 0xD800), render_text->GetDisplayText());
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

TEST_P(RenderTextTest, ObscuredEmoji) {
  // Ensures text itemization doesn't crash on obscured multi-char glyphs.
  RenderText* render_text = GetRenderText();
  render_text->SetObscured(true);
  // Test U+1F601 😁 "Grinning face with smiling eyes", followed by 'y'.
  // Windows requires wide strings for \Unnnnnnnn universal character names.
  render_text->SetText(WideToUTF16(L"\U0001F601y"));
  render_text->Draw(canvas());
  // Test two U+1F4F7 📷 "Camera" characters in a row.
  // Windows requires wide strings for \Unnnnnnnn universal character names.
  render_text->SetText(WideToUTF16(L"\U0001F4F7\U0001F4F7"));
  render_text->Draw(canvas());
}

TEST_P(RenderTextTest, ElidedText) {
  // TODO(skanuj) : Add more test cases for following
  // - RenderText styles.
  // - Cross interaction of truncate, elide and obscure.
  // - ElideText tests from text_elider.cc.
  struct {
    const wchar_t* text;
    const wchar_t* display_text;
    const bool elision_expected;
  } cases[] = {
      // Strings shorter than the elision width should be laid out in full.
      {L"", L"", false},
      {L"M", L"", false},
      {L" . ", L" . ", false},  // a wide kWeak
      {L"abc", L"abc", false},  // a wide kLtr
      {L"\u05d0\u05d1\u05d2", L"\u05d0\u05d1\u05d2", false},  // a wide kRtl
      {L"a\u05d0\u05d1", L"a\u05d0\u05d1", false},  // a wide kLtrRtl
      {L"a\u05d1b", L"a\u05d1b", false},  // a wide kLtrRtlLtr
      {L"\u05d0\u05d1a", L"\u05d0\u05d1a", false},  // a wide kRtlLtr
      {L"\u05d0a\u05d1", L"\u05d0a\u05d1", false},  // a wide kRtlLtrRtl
      // Strings as long as the elision width should be laid out in full.
      {L"012ab", L"012ab", false},
      // Long strings should be elided with an ellipsis appended at the end.
      {L"012abc", L"012a\u2026", true},
      {L"012ab\u05d0\u05d1", L"012a\u2026", true},
      {L"012a\u05d1b", L"012a\u2026", true},
      // No RLM marker added as digits (012) have weak directionality.
      {L"01\u05d0\u05d1\u05d2", L"01\u05d0\u05d1\u2026", true},
      // RLM marker added as "ab" have strong LTR directionality.
      {L"ab\u05d0\u05d1\u05d2", L"ab\u05d0\u05d1\u2026\u200f", true},
      // Test surrogate pairs. The first pair 𝄞 'MUSICAL SYMBOL G CLEF' U+1D11E
      // should be kept, and the second pair 𝄢 'MUSICAL SYMBOL F CLEF' U+1D122
      // should be removed. No surrogate pair should be partially elided.
      // Windows requires wide strings for \Unnnnnnnn universal character names.
      {L"0123\U0001D11E\U0001D122", L"0123\U0001D11E\u2026", true},
      // Test combining character sequences. U+0915 U+093F forms a compound
      // glyph, as does U+0915 U+0942. The first should be kept; the second
      // removed. No combining sequence should be partially elided.
      {L"0123\u0915\u093f\u0915\u0942", L"0123\u0915\u093f\u2026", true},
      // U+05E9 U+05BC U+05C1 U+05B8 forms a four-character compound glyph.
      // It should be either fully elided, or not elided at all. If completely
      // elided, an LTR Mark (U+200E) should be added.
      {L"0\u05e9\u05bc\u05c1\u05b8", L"0\u05e9\u05bc\u05c1\u05b8", false},
      {L"0\u05e9\u05bc\u05c1\u05b8", L"0\u2026\u200E", true},
      {L"01\u05e9\u05bc\u05c1\u05b8", L"01\u2026\u200E", true},
      {L"012\u05e9\u05bc\u05c1\u05b8", L"012\u2026\u200E", true},
      // 𝄞 (U+1D11E, MUSICAL SYMBOL G CLEF) should be fully elided.
      // Windows requires wide strings for \Unnnnnnnn universal character names.
      {L"012\U0001D11E", L"012\u2026", true},
  };

  std::unique_ptr<RenderText> expected_render_text(CreateRenderTextInstance());
  expected_render_text->SetFontList(FontList("serif, Sans serif, 12px"));
  expected_render_text->SetDisplayRect(Rect(0, 0, 9999, 100));

  RenderText* render_text = GetRenderText();
  render_text->SetFontList(FontList("serif, Sans serif, 12px"));
  render_text->SetElideBehavior(ELIDE_TAIL);

  for (size_t i = 0; i < arraysize(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "] '%ls'", i,
                                    cases[i].text));

    // Compute expected width
    expected_render_text->SetText(WideToUTF16(cases[i].display_text));
    int expected_width = expected_render_text->GetContentWidth();

    base::string16 input = WideToUTF16(cases[i].text);
    // Extend the input text to ensure that it is wider than the display_text,
    // and so it will get elided.
    if (cases[i].elision_expected)
      input.append(UTF8ToUTF16(" MMMMMMMMMMM"));
    render_text->SetText(input);
    render_text->SetDisplayRect(Rect(0, 0, expected_width, 100));
    EXPECT_EQ(input, render_text->text());
    EXPECT_EQ(WideToUTF16(cases[i].display_text),
              render_text->GetDisplayText());
    expected_render_text->SetText(base::string16());
  }
}

TEST_P(RenderTextTest, ElidedObscuredText) {
  std::unique_ptr<RenderText> expected_render_text(CreateRenderTextInstance());
  expected_render_text->SetFontList(FontList("serif, Sans serif, 12px"));
  expected_render_text->SetDisplayRect(Rect(0, 0, 9999, 100));
  const base::char16 elided_obscured_text[] = {
      RenderText::kPasswordReplacementChar,
      RenderText::kPasswordReplacementChar, 0x2026, 0};
  expected_render_text->SetText(elided_obscured_text);

  RenderText* render_text = GetRenderText();
  render_text->SetFontList(FontList("serif, Sans serif, 12px"));
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetDisplayRect(
      Rect(0, 0, expected_render_text->GetContentWidth(), 100));
  render_text->SetObscured(true);
  render_text->SetText(UTF8ToUTF16("abcdef"));
  EXPECT_EQ(UTF8ToUTF16("abcdef"), render_text->text());
  EXPECT_EQ(elided_obscured_text, render_text->GetDisplayText());
}

// TODO(PORT): Fails for RenderTextMac.
TEST_P(RenderTextHarfBuzzTest, MultilineElide) {
  RenderText* render_text = GetRenderText();
  base::string16 input_text;
  // Aim for 3 lines of text.
  for (int i = 0; i < 20; ++i)
    input_text.append(UTF8ToUTF16("hello world "));
  render_text->SetText(input_text);
  // Apply a style that tweaks the layout to make sure elision is calculated
  // with these styles. This can expose a behavior in layout where text is
  // slightly different width. This must be done after |SetText()|.
  render_text->ApplyWeight(Font::Weight::BOLD, Range(1, 20));
  render_text->ApplyStyle(ITALIC, true, Range(1, 20));
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

  const base::char16 kEllipsisUTF16[] = {0x2026, 0};
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

TEST_P(RenderTextHarfBuzzTest, MultilineElideWrap) {
  RenderText* render_text = GetRenderText();
  base::string16 input_text;
  for (int i = 0; i < 20; ++i)
    input_text.append(UTF8ToUTF16("hello world "));
  render_text->SetText(input_text);

  render_text->ApplyWeight(Font::Weight::BOLD, Range(1, 20));
  render_text->ApplyStyle(ITALIC, true, Range(1, 20));
  render_text->SetMultiline(true);
  render_text->SetMaxLines(3);
  render_text->SetElideBehavior(ELIDE_TAIL);

  render_text->SetDisplayRect(Rect(30, 0));

  const base::char16 kEllipsisUTF16[] = {0x2026, 0};
  base::string16 actual_text;

  // ELIDE_LONG_WORDS doesn't make sense in multiline, and triggers assertion
  // failure.
  const WordWrapBehavior wrap_behaviors[] = {
      IGNORE_LONG_WORDS, TRUNCATE_LONG_WORDS, WRAP_LONG_WORDS};
  for (auto wrap_behavior : wrap_behaviors) {
    render_text->SetWordWrapBehavior(wrap_behavior);
    render_text->GetStringSize();
    actual_text = render_text->GetDisplayText();
    EXPECT_LE(actual_text.size(), input_text.size());
    EXPECT_EQ(actual_text, input_text.substr(0, actual_text.size() - 1) +
                               base::string16(kEllipsisUTF16));
    EXPECT_LE(render_text->GetNumLines(), 3U);
  }
}

TEST_P(RenderTextHarfBuzzTest, MultilineElideWrapStress) {
  RenderText* render_text = GetRenderText();
  base::string16 input_text;
  for (int i = 0; i < 20; ++i)
    input_text.append(UTF8ToUTF16("hello world "));
  render_text->SetText(input_text);

  // TODO(crbug.com/866720): with the line about ApplyWeight() uncommented, when
  // |i| (the width of display rect) is 23, there would be actually 4 lines
  // rendered, more than the max lines setting. It could be that ellipsis is
  // wrapped to the 4th line due to different word segmentation from the
  // original text.

  // render_text->ApplyWeight(Font::Weight::BOLD, Range(1, 20));
  render_text->SetMultiline(true);
  render_text->SetMaxLines(3);
  render_text->SetElideBehavior(ELIDE_TAIL);

  base::string16 actual_text;

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
      actual_text = render_text->GetDisplayText();
      EXPECT_LE(actual_text.size(), input_text.size());
      EXPECT_LE(render_text->GetNumLines(), 3U);
    }
  }
}

TEST_P(RenderTextTest, ElidedEmail) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("test@example.com"));
  const Size size = render_text->GetStringSize();

  const base::string16 long_email =
      UTF8ToUTF16("longemailaddresstest@example.com");
  render_text->SetText(long_email);
  render_text->SetElideBehavior(ELIDE_EMAIL);
  render_text->SetDisplayRect(Rect(size));
  EXPECT_GE(size.width(), render_text->GetStringSize().width());
  EXPECT_GT(long_email.size(), render_text->GetDisplayText().size());
}

TEST_P(RenderTextTest, TruncatedText) {
  struct {
    const wchar_t* text;
    const wchar_t* display_text;
  } cases[] = {
      // Strings shorter than the truncation length should be laid out in full.
      {L"", L""},
      {L" . ", L" . "},  // a wide kWeak
      {L"abc", L"abc"},  // a wide kLtr
      {L"\u05d0\u05d1\u05d2", L"\u05d0\u05d1\u05d2"},  // a wide kRtl
      {L"a\u05d0\u05d1", L"a\u05d0\u05d1"},  // a wide kLtrRtl
      {L"a\u05d1b", L"a\u05d1b"},  // a wide kLtrRtlLtr
      {L"\u05d0\u05d1a", L"\u05d0\u05d1a"},  // a wide kRtlLtr
      {L"\u05d0a\u05d1", L"\u05d0a\u05d1"},  // a wide kRtlLtrRtl
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
      {L"0\u05e9\u05bc\u05c1\u05b8", L"0\u05e9\u05bc\u05c1\u05b8"},
      {L"01\u05e9\u05bc\u05c1\u05b8", L"01\u05e9\u05bc\u2026"},
      {L"012\u05e9\u05bc\u05c1\u05b8", L"012\u05e9\u2026"},
      {L"0123\u05e9\u05bc\u05c1\u05b8", L"0123\u2026"},
      {L"01234\u05e9\u05bc\u05c1\u05b8", L"0123\u2026"},
      // Windows requires wide strings for \Unnnnnnnn universal character names.
      {L"0123\U0001D11E", L"0123\u2026"},
  };

  RenderText* render_text = GetRenderText();
  render_text->set_truncate_length(5);
  for (size_t i = 0; i < arraysize(cases); i++) {
    render_text->SetText(WideToUTF16(cases[i].text));
    EXPECT_EQ(WideToUTF16(cases[i].text), render_text->text());
    EXPECT_EQ(WideToUTF16(cases[i].display_text), render_text->GetDisplayText())
        << "For case " << i << ": " << cases[i].text;
  }
}

TEST_P(RenderTextTest, TruncatedObscuredText) {
  RenderText* render_text = GetRenderText();
  render_text->set_truncate_length(3);
  render_text->SetObscured(true);
  render_text->SetText(UTF8ToUTF16("abcdef"));
  EXPECT_EQ(UTF8ToUTF16("abcdef"), render_text->text());
  EXPECT_EQ(GetObscuredString(3, 2, 0x2026), render_text->GetDisplayText());
}

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, TruncatedCursorMovementLTR) {
  RenderText* render_text = GetRenderText();
  render_text->set_truncate_length(2);
  render_text->SetText(UTF8ToUTF16("abcd"));

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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, TruncatedCursorMovementRTL) {
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursor_Character) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("123 456 789"));
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursor_Word) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("123 456 789"));
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursor_Word_RTL) {
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

TEST_P(RenderTextTest, MoveCursor_Line) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("123 456 789"));
  std::vector<Range> expected;

  // SELECTION_NONE.
  render_text->SelectRange(Range(6));

  // Move right twice.
  expected.push_back(Range(11));
  expected.push_back(Range(11));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_RIGHT,
                                        SELECTION_NONE, &expected);

  // Move left twice.
  expected.push_back(Range(0));
  expected.push_back(Range(0));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_LEFT,
                                        SELECTION_NONE, &expected);

  // SELECTION_CARET.
  render_text->SelectRange(Range(6));

  // Move right.
  expected.push_back(Range(6, 11));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_RIGHT,
                                        SELECTION_CARET, &expected);

  // Move left twice.
  expected.push_back(Range(6));
  expected.push_back(Range(6, 0));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_LEFT,
                                        SELECTION_CARET, &expected);

  // Move right.
  expected.push_back(Range(6));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_RIGHT,
                                        SELECTION_CARET, &expected);

  // SELECTION_RETAIN.
  render_text->SelectRange(Range(6));

  // Move right.
  expected.push_back(Range(6, 11));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_RIGHT,
                                        SELECTION_RETAIN, &expected);

  // Move left twice.
  expected.push_back(Range(6, 0));
  expected.push_back(Range(6, 0));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_LEFT,
                                        SELECTION_RETAIN, &expected);

  // Move right.
  expected.push_back(Range(6, 11));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_RIGHT,
                                        SELECTION_RETAIN, &expected);

  // SELECTION_EXTEND.
  render_text->SelectRange(Range(6));

  // Move right.
  expected.push_back(Range(6, 11));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_RIGHT,
                                        SELECTION_EXTEND, &expected);

  // Move left twice.
  expected.push_back(Range(11, 0));
  expected.push_back(Range(11, 0));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_LEFT,
                                        SELECTION_EXTEND, &expected);

  // Move right.
  expected.push_back(Range(0, 11));
  RunMoveCursorTestAndClearExpectations(render_text, LINE_BREAK, CURSOR_RIGHT,
                                        SELECTION_EXTEND, &expected);
}

TEST_P(RenderTextTest, GetDisplayTextDirection) {
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
    for (size_t j = 0; j < arraysize(cases); j++) {
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursorLeftRightInLtr) {
  RenderText* render_text = GetRenderText();
  // Pure LTR.
  render_text->SetText(UTF8ToUTF16("abc"));
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursorLeftRightInLtrRtl) {
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursorLeftRightInLtrRtlLtr) {
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursorLeftRightInRtl) {
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursorLeftRightInRtlLtr) {
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursorLeftRightInRtlLtrRtl) {
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursorLeftRight_ComplexScript) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("\u0915\u093f\u0915\u094d\u0915"));
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(4U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(5U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(5U, render_text->cursor_position());

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(4U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
}

TEST_P(RenderTextHarfBuzzTest, MoveCursorLeftRight_MeiryoUILigatures) {
  RenderText* render_text = GetRenderText();
  // Meiryo UI uses single-glyph ligatures for 'ff' and 'ffi', but each letter
  // (code point) has unique bounds, so mid-glyph cursoring should be possible.
  render_text->SetFontList(FontList("Meiryo UI, 12px"));
  render_text->SetText(UTF8ToUTF16("ff ffi"));
  render_text->SetDisplayRect(gfx::Rect(100, 100));
  test_api()->EnsureLayout();
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

TEST_P(RenderTextHarfBuzzTest, GraphemePositions) {
  // LTR कि (DEVANAGARI KA with VOWEL I) (2-char grapheme), LTR abc, and LTR कि.
  const base::string16 kText1 = WideToUTF16(L"\u0915\u093fabc\u0915\u093f");

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
  for (size_t i = 0; i < arraysize(cases); i++) {
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MidGraphemeSelectionBounds) {
  // Test that selection bounds may be set amid multi-character graphemes.
  const base::string16 kHindi = UTF8ToUTF16("\u0915\u093f");
  const base::string16 kThai = UTF8ToUTF16("\u0e08\u0e33");
  const base::string16 cases[] = { kHindi, kThai };

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(100, 1000));
  for (size_t i = 0; i < arraysize(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    render_text->SetText(cases[i]);
    EXPECT_TRUE(render_text->IsValidLogicalIndex(1));
    EXPECT_FALSE(render_text->IsValidCursorIndex(1));
    EXPECT_TRUE(render_text->SelectRange(Range(2, 1)));
    EXPECT_EQ(Range(2, 1), render_text->selection());
    EXPECT_EQ(1U, render_text->cursor_position());

    // Verify that the selection bounds extend over the entire grapheme, even if
    // the selection is set amid the grapheme.
    test_api()->EnsureLayout();
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, FindCursorPosition) {
  const char* kTestStrings[] = {kLtrRtl, kLtrRtlLtr, kRtlLtr, kRtlLtrRtl};
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(0, 0, 100, 20));
  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    for (size_t j = 0; j < render_text->text().length(); ++j) {
      const Range range(render_text->GetCursorSpan(Range(j, j + 1)));
      // Test a point just inside the leading edge of the glyph bounds.
      int x = range.is_reversed() ? range.GetMax() - 1 : range.GetMin() + 1;
      EXPECT_EQ(
          j, render_text->FindCursorPosition(Point(x, GetCursorYForTesting()))
                 .caret_pos());
    }
  }
}

// Tests that FindCursorPosition behaves correctly for multi-line text.
TEST_P(RenderTextHarfBuzzTest, FindCursorPositionMultiline) {
  const char* kTestStrings[] = {"abc def",
                                "\u05d0\u05d1\u05d2 \u05d3\u05d4\u05d5"};

  SetGlyphWidth(5);
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(25, 1000));
  render_text->SetMultiline(true);

  for (size_t i = 0; i < arraysize(kTestStrings); i++) {
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    test_api()->EnsureLayout();
    EXPECT_EQ(2u, test_api()->lines().size());

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
TEST_P(RenderTextHarfBuzzTest, FindCursorPosition_GraphemeBoundaries) {
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
  for (size_t i = 0; i < arraysize(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing case %" PRIuS "", i));
    render_text->SetText(cases[i].text);
    test_api()->EnsureLayout();
    std::set<size_t> obtained_cursor_positions;
    size_t cursor_y = GetCursorYForTesting();
    for (int x = -5; x < 105; x++)
      obtained_cursor_positions.insert(
          render_text->FindCursorPosition(gfx::Point(x, cursor_y)).caret_pos());
    EXPECT_EQ(cases[i].expected_cursor_positions, obtained_cursor_positions);
  }
}

TEST_P(RenderTextTest, EdgeSelectionModels) {
  // Simple Latin text.
  const base::string16 kLatin = UTF8ToUTF16("abc");
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
  for (size_t i = 0; i < arraysize(cases); i++) {
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

TEST_P(RenderTextTest, SelectAll) {
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
    for (size_t j = 0; j < arraysize(cases); j++) {
      render_text->SetText(UTF8ToUTF16(cases[j]));
      render_text->SelectAll(false);
      EXPECT_EQ(render_text->selection_model(), expected_forwards);
      render_text->SelectAll(true);
      EXPECT_EQ(render_text->selection_model(), expected_reversed);
    }
  }

  EXPECT_EQ(was_rtl, base::i18n::IsRTL());
}

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveCursorLeftRightWithSelection) {
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

TEST_P(RenderTextTest, CenteredDisplayOffset) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("abcdefghij"));
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

  // TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
  if (GetParam() != RENDER_TEXT_MAC) {
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
}

void MoveLeftRightByWordVerifier(RenderText* render_text, const char* str) {
  render_text->SetText(UTF8ToUTF16(str));

  // Test moving by word from left to right.
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, SELECTION_NONE);
  bool first_word = true;
  while (true) {
    // First, test moving by word from a word break position, such as from
    // "|abc def" to "abc| def".
    const SelectionModel start = render_text->selection_model();
    render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
    const SelectionModel end = render_text->selection_model();
    if (end == start)  // reach the end.
      break;

    // For testing simplicity, each word is a 3-character word.
    int num_of_character_moves = first_word ? 3 : 4;
    first_word = false;
    render_text->SetSelection(start);
    for (int j = 0; j < num_of_character_moves; ++j)
      render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
    EXPECT_EQ(end, render_text->selection_model());

    // Then, test moving by word from positions inside the word, such as from
    // "a|bc def" to "abc| def", and from "ab|c def" to "abc| def".
    for (int j = 1; j < num_of_character_moves; ++j) {
      render_text->SetSelection(start);
      for (int k = 0; k < j; ++k)
        render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
      render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
      EXPECT_EQ(end, render_text->selection_model());
    }
  }

  // Test moving by word from right to left.
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  first_word = true;
  while (true) {
    const SelectionModel start = render_text->selection_model();
    render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
    const SelectionModel end = render_text->selection_model();
    if (end == start)  // reach the end.
      break;

    int num_of_character_moves = first_word ? 3 : 4;
    first_word = false;
    render_text->SetSelection(start);
    for (int j = 0; j < num_of_character_moves; ++j)
      render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
    EXPECT_EQ(end, render_text->selection_model());

    for (int j = 1; j < num_of_character_moves; ++j) {
      render_text->SetSelection(start);
      for (int k = 0; k < j; ++k)
        render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
      render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
      EXPECT_EQ(end, render_text->selection_model());
    }
  }
}

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
// TODO(msw): Make these work on Windows. http://crbug.com/196326
#if !defined(OS_WIN)
TEST_P(RenderTextHarfBuzzTest, MoveLeftRightByWordInBidiText) {
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveLeftRightByWordInBidiText_TestEndOfText) {
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

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveLeftRightByWordInTextWithMultiSpaces) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("abc     def"));
  render_text->SetCursorPosition(5);
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(11U, render_text->cursor_position());

  render_text->SetCursorPosition(5);
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(0U, render_text->cursor_position());
}
#endif  // !defined(OS_WIN)

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MoveLeftRightByWordInThaiText) {
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
#if defined(OS_ANDROID)
#define MAYBE_MoveLeftRightByWordInChineseText \
  DISABLED_MoveLeftRightByWordInChineseText
#else
#define MAYBE_MoveLeftRightByWordInChineseText MoveLeftRightByWordInChineseText
#endif
// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, MAYBE_MoveLeftRightByWordInChineseText) {
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

// Test the correct behavior of undirected selections: selections where the
// "end" of the selection that holds the cursor is only determined after the
// first cursor movement.
TEST_P(RenderTextHarfBuzzTest, DirectedSelections) {
  RenderText* render_text = GetRenderText();

  auto ResultAfter = [&](VisualCursorDirection direction) -> base::string16 {
    render_text->MoveCursor(CHARACTER_BREAK, direction, SELECTION_RETAIN);
    return GetSelectedText(render_text);
  };

  render_text->SetText(UTF8ToUTF16("01234"));

  // Test Right, then Left. LTR.
  // Undirected, or forward when kSelectionIsAlwaysDirected.
  render_text->SelectRange({2, 4});
  EXPECT_EQ(UTF8ToUTF16("23"), GetSelectedText(render_text));
  EXPECT_EQ(UTF8ToUTF16("234"), ResultAfter(CURSOR_RIGHT));
  EXPECT_EQ(UTF8ToUTF16("23"), ResultAfter(CURSOR_LEFT));

  // Test collapsing the selection. This always ignores any existing direction.
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(2, 2), render_text->selection());  // Collapse left.

  // Undirected, or backward when kSelectionIsAlwaysDirected.
  render_text->SelectRange({4, 2});
  EXPECT_EQ(UTF8ToUTF16("23"), GetSelectedText(render_text));
  if (RenderText::kSelectionIsAlwaysDirected)
    EXPECT_EQ(UTF8ToUTF16("3"), ResultAfter(CURSOR_RIGHT));  // Keep left.
  else
    EXPECT_EQ(UTF8ToUTF16("234"), ResultAfter(CURSOR_RIGHT));  // Pick right.
  EXPECT_EQ(UTF8ToUTF16("23"), ResultAfter(CURSOR_LEFT));

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_NONE);
  EXPECT_EQ(Range(2, 2), render_text->selection());  // Collapse left.

  // Test Left, then Right. LTR.
  // Undirected, or forward when kSelectionIsAlwaysDirected.
  render_text->SelectRange({2, 4});
  EXPECT_EQ(UTF8ToUTF16("23"), GetSelectedText(render_text));  // Sanity check,

  if (RenderText::kSelectionIsAlwaysDirected)
    EXPECT_EQ(UTF8ToUTF16("2"), ResultAfter(CURSOR_LEFT));  // Keep right.
  else
    EXPECT_EQ(UTF8ToUTF16("123"), ResultAfter(CURSOR_LEFT));  // Pick left.
  EXPECT_EQ(UTF8ToUTF16("23"), ResultAfter(CURSOR_RIGHT));

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(Range(4, 4), render_text->selection());  // Collapse right.

  // Undirected, or backward when kSelectionIsAlwaysDirected.
  render_text->SelectRange({4, 2});
  EXPECT_EQ(UTF8ToUTF16("23"), GetSelectedText(render_text));
  EXPECT_EQ(UTF8ToUTF16("123"), ResultAfter(CURSOR_LEFT));
  EXPECT_EQ(UTF8ToUTF16("23"), ResultAfter(CURSOR_RIGHT));

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

TEST_P(RenderTextTest, StringSizeSanity) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("Hello World"));
  const Size string_size = render_text->GetStringSize();
  EXPECT_GT(string_size.width(), 0);
  EXPECT_GT(string_size.height(), 0);
}

TEST_P(RenderTextTest, StringSizeLongStrings) {
  RenderText* render_text = GetRenderText();
  Size previous_string_size;
  for (size_t length = 10; length < 1000000; length *= 10) {
    render_text->SetText(base::string16(length, 'a'));
    const Size string_size = render_text->GetStringSize();
    EXPECT_GT(string_size.width(), previous_string_size.width());
    EXPECT_GT(string_size.height(), 0);
    previous_string_size = string_size;
  }
}

TEST_P(RenderTextTest, StringSizeEmptyString) {
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

  render_text->SetText(UTF8ToUTF16(" "));
  EXPECT_EQ(font_list.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(font_list.GetBaseline(), render_text->GetBaseline());
}

// TODO(865540): Find two system fonts on Android with different baseline and
// different height.
// See also: FontListTest::Fonts_GetHeight_GetBaseline.
#if defined(OS_ANDROID)
#define MAYBE_StringSizeRespectsFontListMetrics \
  DISABLED_StringSizeRespectsFontListMetrics
#else
#define MAYBE_StringSizeRespectsFontListMetrics \
  StringSizeRespectsFontListMetrics
#endif
TEST_P(RenderTextTest, MAYBE_StringSizeRespectsFontListMetrics) {
  // Check that the test font and the CJK font have different font metrics.
  Font test_font(kTestFontName, 16);
  ASSERT_EQ(base::ToLowerASCII(kTestFontName),
            base::ToLowerASCII(test_font.GetActualFontNameForTesting()));
  Font cjk_font(kCJKFontName, 16);
  ASSERT_EQ(base::ToLowerASCII(kCJKFontName),
            base::ToLowerASCII(cjk_font.GetActualFontNameForTesting()));
  EXPECT_NE(test_font.GetHeight(), cjk_font.GetHeight());
  EXPECT_NE(test_font.GetBaseline(), cjk_font.GetBaseline());
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
  ASSERT_LT(smaller_font.GetHeight(), larger_font.GetHeight());
  ASSERT_LT(smaller_font.GetBaseline(), larger_font.GetBaseline());

  // Check |smaller_font_text| is rendered with the smaller font.
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16(smaller_font_text));
  render_text->SetFontList(FontList(smaller_font));
  render_text->SetDisplayRect(Rect(0, 0, 0,
                                   render_text->font_list().GetHeight()));
  EXPECT_EQ(smaller_font.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(smaller_font.GetBaseline(), render_text->GetBaseline());
  EXPECT_STRCASEEQ(
      render_text->GetFontSpansForTesting()[0].first.GetFontName().c_str(),
      smaller_font.GetFontName().c_str());

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
  EXPECT_STRCASEEQ(
      render_text->GetFontSpansForTesting()[0].first.GetFontName().c_str(),
      smaller_font.GetFontName().c_str());
  EXPECT_LT(smaller_font.GetHeight(), render_text->GetStringSize().height());
  EXPECT_LT(smaller_font.GetBaseline(), render_text->GetBaseline());
  EXPECT_EQ(font_list.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(font_list.GetBaseline(), render_text->GetBaseline());
}

TEST_P(RenderTextTest, MinLineHeight) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("Hello!"));
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
// Disabled since this relies on machine configuration. http://crbug.com/701241.
TEST_P(RenderTextTest, DISABLED_DefaultLineHeights) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(
      UTF8ToUTF16("A quick brown fox jumped over the lazy dog!"));

#if defined(OS_MACOSX)
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

TEST_P(RenderTextTest, SetFontList) {
  RenderText* render_text = GetRenderText();
  render_text->SetFontList(
      FontList(base::StringPrintf("Arial,%s, 13px", kSymbolFontName)));
  const std::vector<Font>& fonts = render_text->font_list().GetFonts();
  ASSERT_EQ(2U, fonts.size());
  EXPECT_EQ("Arial", fonts[0].GetFontName());
  EXPECT_EQ(kSymbolFontName, fonts[1].GetFontName());
  EXPECT_EQ(13, render_text->font_list().GetFontSize());
}

// http://crbug/624513
#if !defined(OS_WIN)
TEST_P(RenderTextTest, StringSizeBoldWidth) {
  // TODO(mboc): Add some unittests for other weights (currently not
  // implemented because of test system font configuration).
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("Hello World"));

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
#endif  // !defined(OS_WIN)

TEST_P(RenderTextTest, StringSizeHeight) {
  base::string16 cases[] = {
      UTF8ToUTF16("Hello World!"),  // English
      UTF8ToUTF16("\u6328\u62f6"),  // Japanese 挨拶 (characters press & near)
      UTF8ToUTF16("\u0915\u093f"),  // Hindi कि (letter KA with vowel I)
      UTF8ToUTF16("\u05e0\u05b8"),  // Hebrew נָ (letter NUN and point QAMATS)
  };

  const FontList default_font_list;
  const FontList& larger_font_list = default_font_list.DeriveWithSizeDelta(24);
  EXPECT_GT(larger_font_list.GetHeight(), default_font_list.GetHeight());

  for (size_t i = 0; i < arraysize(cases); i++) {
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

TEST_P(RenderTextTest, GetBaselineSanity) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("Hello World"));
  const int baseline = render_text->GetBaseline();
  EXPECT_GT(baseline, 0);
}

TEST_P(RenderTextTest, CursorBoundsInReplacementMode) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("abcdefg"));
  render_text->SetDisplayRect(Rect(100, 17));
  SelectionModel sel_b(1, CURSOR_FORWARD);
  SelectionModel sel_c(2, CURSOR_FORWARD);
  Rect cursor_around_b = render_text->GetCursorBounds(sel_b, false);
  Rect cursor_before_b = render_text->GetCursorBounds(sel_b, true);
  Rect cursor_before_c = render_text->GetCursorBounds(sel_c, true);
  EXPECT_EQ(cursor_around_b.x(), cursor_before_b.x());
  EXPECT_EQ(cursor_around_b.right(), cursor_before_c.x());
}

TEST_P(RenderTextTest, GetTextOffset) {
  // The default horizontal text offset differs for LTR and RTL, and is only set
  // when the RenderText object is created.  This test will check the default in
  // LTR mode, and the next test will check the RTL default.
  const bool was_rtl = base::i18n::IsRTL();
  SetRTL(false);

  // Reset the render text instance since the locale was changed.
  ResetRenderTextInstance();
  RenderText* render_text = GetRenderText();

  render_text->SetText(UTF8ToUTF16("abcdefg"));
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

TEST_P(RenderTextTest, GetTextOffsetHorizontalDefaultInRTL) {
  // This only checks the default horizontal alignment in RTL mode; all other
  // GetLineOffset(0) attributes are checked by the test above.
  const bool was_rtl = base::i18n::IsRTL();
  SetRTL(true);

  // Reset the render text instance since the locale was changed.
  ResetRenderTextInstance();
  RenderText* render_text = GetRenderText();

  render_text->SetText(UTF8ToUTF16("abcdefg"));
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

TEST_P(RenderTextTest, SetDisplayOffset) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("abcdefg"));
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

  for (size_t i = 0; i < arraysize(small_content_cases); i++) {
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

  for (size_t i = 0; i < arraysize(large_content_cases); i++) {
    render_text->SetHorizontalAlignment(large_content_cases[i].alignment);
    render_text->SetDisplayOffset(large_content_cases[i].offset);
    EXPECT_EQ(large_content_cases[i].expected_offset,
              render_text->GetUpdatedDisplayOffset().x());
  }
}

TEST_P(RenderTextTest, SameFontForParentheses) {
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
      {UTF8ToUTF16("Hello World(a)")},
      // English(English)English
      {UTF8ToUTF16("Hello World(a)Hello World")},

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
  for (size_t i = 0; i < arraysize(cases); ++i) {
    base::string16 text = cases[i].text;
    const size_t start_paren_char_index = text.find('(');
    ASSERT_NE(base::string16::npos, start_paren_char_index);
    const size_t end_paren_char_index = text.find(')');
    ASSERT_NE(base::string16::npos, end_paren_char_index);

    for (size_t j = 0; j < arraysize(punctuation_pairs); ++j) {
      text[start_paren_char_index] = punctuation_pairs[j].left_char;
      text[end_paren_char_index] = punctuation_pairs[j].right_char;
      render_text->SetText(text);

      const std::vector<RenderText::FontSpan> spans =
          render_text->GetFontSpansForTesting();

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
TEST_P(RenderTextTest, CaretWidth) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("abcdefg"));
  EXPECT_GE(render_text->GetUpdatedCursorBounds().width(), 1);
}

TEST_P(RenderTextTest, SelectWord) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16(" foo  a.bc.d bar"));

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

  for (size_t i = 0; i < arraysize(cases); ++i) {
    render_text->SetCursorPosition(cases[i].cursor);
    render_text->SelectWord();
    EXPECT_EQ(Range(cases[i].selection_start, cases[i].selection_end),
              render_text->selection());
  }
}

// Make sure the last word is selected when the cursor is at text.length().
TEST_P(RenderTextTest, LastWordSelected) {
  const std::string kTestURL1 = "http://www.google.com";
  const std::string kTestURL2 = "http://www.google.com/something/";

  RenderText* render_text = GetRenderText();

  render_text->SetText(UTF8ToUTF16(kTestURL1));
  render_text->SetCursorPosition(kTestURL1.length());
  render_text->SelectWord();
  EXPECT_EQ(UTF8ToUTF16("com"), GetSelectedText(render_text));
  EXPECT_FALSE(render_text->selection().is_reversed());

  render_text->SetText(UTF8ToUTF16(kTestURL2));
  render_text->SetCursorPosition(kTestURL2.length());
  render_text->SelectWord();
  EXPECT_EQ(UTF8ToUTF16("/"), GetSelectedText(render_text));
  EXPECT_FALSE(render_text->selection().is_reversed());
}

// When given a non-empty selection, SelectWord should expand the selection to
// nearest word boundaries.
TEST_P(RenderTextTest, SelectMultipleWords) {
  const std::string kTestURL = "http://www.google.com";

  RenderText* render_text = GetRenderText();

  render_text->SetText(UTF8ToUTF16(kTestURL));
  render_text->SelectRange(Range(16, 20));
  render_text->SelectWord();
  EXPECT_EQ(UTF8ToUTF16("google.com"), GetSelectedText(render_text));
  EXPECT_FALSE(render_text->selection().is_reversed());

  // SelectWord should preserve the selection direction.
  render_text->SelectRange(Range(20, 16));
  render_text->SelectWord();
  EXPECT_EQ(UTF8ToUTF16("google.com"), GetSelectedText(render_text));
  EXPECT_TRUE(render_text->selection().is_reversed());
}

// TODO(asvitkine): RenderTextMac cursor movements. http://crbug.com/131618
TEST_P(RenderTextHarfBuzzTest, DisplayRectShowsCursorLTR) {
  ASSERT_FALSE(base::i18n::IsRTL());
  ASSERT_FALSE(base::i18n::ICUIsRTL());

  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("abcdefghijklmnopqrstuvwxzyabcdefg"));
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

TEST_P(RenderTextTest, DisplayRectShowsCursorRTL) {
  // Set the application default text direction to RTL.
  const bool was_rtl = base::i18n::IsRTL();
  SetRTL(true);

  // Reset the render text instance since the locale was changed.
  ResetRenderTextInstance();
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("abcdefghijklmnopqrstuvwxzyabcdefg"));
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
TEST_P(RenderTextTest, SelectionKeepsLigatures) {
  const char* kTestStrings[] = {"\u0644\u0623", "\u0633\u0627"};
  RenderText* render_text = GetRenderText();
  render_text->set_selection_color(SK_ColorRED);

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
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
TEST_P(RenderTextTest, ScriptExtensionsDoNotBreak) {
  // Apparently ramen restaurants prefer "らーめん" over "らあめん". The "dash"
  // is the long sound symbol and usually just appears in Katakana writing.
  const base::string16 ramen_hiragana = UTF8ToUTF16("らーめん");
  const base::string16 ramen_katakana = UTF8ToUTF16("ラーメン");
  const base::string16 ramen_mixed = UTF8ToUTF16("らあメン");

  EXPECT_EQ(std::vector<base::string16>({ramen_hiragana}),
            RunsFor(ramen_hiragana));
  EXPECT_EQ(std::vector<base::string16>({ramen_katakana}),
            RunsFor(ramen_katakana));

  // Currently Harfbuzz breaks this, but not Mac.
  if (GetParam() == RENDER_TEXT_HARFBUZZ)
    EXPECT_EQ(ToString16Vec({"らあ", "メン"}), RunsFor(ramen_mixed));
  else
    EXPECT_EQ(std::vector<base::string16>({ramen_mixed}), RunsFor(ramen_mixed));
}

// Test that whitespace breaks runs of text. E.g. this can permit better fonts
// to be chosen by the fallback mechanism when a font does not provide
// whitespace glyphs for all scripts. See http://crbug.com/731563.
TEST_P(RenderTextTest, WhitespaceDoesBreak) {
  // Title of the Wikipedia page for "bit". ASCII spaces. In Hebrew and English.
  // Note that the hyphens that Wikipedia uses are different. English uses
  // ASCII (U+002D) "hyphen minus", Hebrew uses the U+2013 "EN Dash".
  const base::string16 ascii_space_he = UTF8ToUTF16("סיבית – ויקיפדיה");
  const base::string16 ascii_space_en = UTF8ToUTF16("Bit - Wikipedia");

  // This says "thank you very much" with a full-width non-ascii space (U+3000).
  const base::string16 full_width_space = UTF8ToUTF16("ども　ありがと");

  if (GetParam() == RENDER_TEXT_HARFBUZZ) {
    // Old behavior:
    // { "סיבית ", "–", " ", "ויקיפדיה"" }
    // { "Bit ", "- ", "Wikipedia" }
    // { "ども　ありがと"" }.
    EXPECT_EQ(ToString16Vec({"סיבית", " ", "–", " ", "ויקיפדיה"}),
              RunsFor(ascii_space_he));
    EXPECT_EQ(ToString16Vec({"Bit", " - ", "Wikipedia"}),
              RunsFor(ascii_space_en));
    EXPECT_EQ(ToString16Vec({"ども", "　", "ありがと"}),
              RunsFor(full_width_space));
  } else {
    // Mac is a black box. From this it seems to draw RTL runs right to left and
    // is able to use typeface characteristics when breaking runs. On 10.9, the
    // Hebrew string is given a single run, and on 10.12 it's given 3 runs. It
    // doesn't really matter as far as the rest of the RenderText machinery is
    // concerned.
    auto actual = RunsFor(ascii_space_he);
    if (actual.size() == 3) {
      EXPECT_EQ(ToString16Vec({"ויקיפדיה", " – ", "סיבית"}), actual);
    } else {
      EXPECT_EQ(ToString16Vec({"סיבית – ויקיפדיה"}), actual);
    }
    EXPECT_EQ(ToString16Vec({"Bit - Wikipedia"}), RunsFor(ascii_space_en));
    EXPECT_EQ(ToString16Vec({"ども　ありがと"}), RunsFor(full_width_space));
  }
}

// Ensure strings wrap onto multiple lines for a small available width.
TEST_P(RenderTextHarfBuzzTest, Multiline_MinWidth) {
  const char* kTestStrings[] = {kWeak, kLtr,    kLtrRtl,   kLtrRtlLtr,
                                kRtl,  kRtlLtr, kRtlLtrRtl};

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(1, 1000));
  render_text->SetMultiline(true);
  render_text->SetWordWrapBehavior(WRAP_LONG_WORDS);

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    render_text->Draw(canvas());
    EXPECT_GT(test_api()->lines().size(), 1U);
  }
}

// Ensure strings wrap onto multiple lines for a normal available width.
TEST_P(RenderTextHarfBuzzTest, Multiline_NormalWidth) {
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

  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  // Specify the fixed width for characters to suppress the possible variations
  // of linebreak results.
  SetGlyphWidth(5);
  render_text->SetDisplayRect(Rect(50, 1000));
  render_text->SetMultiline(true);
  render_text->SetWordWrapBehavior(WRAP_LONG_WORDS);
  render_text->SetHorizontalAlignment(ALIGN_TO_HEAD);

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i].text));
    DrawVisualText();

    ASSERT_EQ(2U, test_api()->lines().size());
    EXPECT_EQ(kTestStrings[i].first_line_char_range,
              LineCharRange(test_api()->lines()[0]));
    EXPECT_EQ(kTestStrings[i].second_line_char_range,
              LineCharRange(test_api()->lines()[1]));

    std::vector<TestSkiaTextRenderer::TextLog> text_log;
    renderer()->GetTextLogAndReset(&text_log);

    ASSERT_EQ(kTestStrings[i].run_lengths.size(), text_log.size());

    // NOTE: this expectation compares the character length and glyph counts,
    // which isn't always equal. This is okay only because all the test
    // strings are simple (like, no compound characters nor UTF16-surrogate
    // pairs). Be careful in case more complicated test strings are added.
    EXPECT_EQ(kTestStrings[i].run_lengths[0], text_log[0].glyph_count);
    const int second_line_start = kTestStrings[i].second_line_run_index;
    EXPECT_EQ(kTestStrings[i].run_lengths[second_line_start],
              text_log[second_line_start].glyph_count);
    EXPECT_LT(text_log[0].origin.y(), text_log[second_line_start].origin.y());
    if (kTestStrings[i].is_ltr) {
      EXPECT_EQ(0, text_log[0].origin.x());
      EXPECT_EQ(0, text_log[second_line_start].origin.x());
    } else {
      EXPECT_LT(0, text_log[0].origin.x());
      EXPECT_LT(0, text_log[second_line_start].origin.x());
    }
  }
}

// Ensure strings don't wrap onto multiple lines for a sufficient available
// width.
TEST_P(RenderTextHarfBuzzTest, Multiline_SufficientWidth) {
  const char* kTestStrings[] = {"", " ", ".", " . ", "abc", "a b c",
                                "\u062E\u0628\u0632", "\u062E \u0628 \u0632"};

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(1000, 1000));
  render_text->SetMultiline(true);

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    render_text->Draw(canvas());
    EXPECT_EQ(1U, test_api()->lines().size());
  }
}

TEST_P(RenderTextHarfBuzzTest, Multiline_Newline) {
  const struct {
    const char* const text;
    const size_t lines_count;
    // Ranges of the characters on each line preceding the newline.
    const Range line_char_ranges[3];
  } kTestStrings[] = {
      {"abc\ndef", 2ul, {Range(0, 3), Range(4, 7), Range::InvalidRange()}},
      {"a \n b ", 2ul, {Range(0, 2), Range(3, 6), Range::InvalidRange()}},
      {"ab\n", 2ul, {Range(0, 2), Range(), Range::InvalidRange()}},
      {"a\n\nb", 3ul, {Range(0, 1), Range(2, 3), Range(3, 4)}},
      {"\nab", 2ul, {Range(0, 1), Range(1, 3), Range::InvalidRange()}},
      {"\n", 2ul, {Range(0, 1), Range(), Range::InvalidRange()}},
  };

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(200, 1000));
  render_text->SetMultiline(true);

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
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
TEST_P(RenderTextHarfBuzzTest, Multiline_IgnoreElide) {
  const char kTestString[] =
      "very very very long string xxxxxxxxxxxxxxxxxxxxxxxxxx";
  const char kEllipsis[] = "\u2026";

  RenderText* render_text = GetRenderText();
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetDisplayRect(Rect(20, 1000));
  render_text->SetText(base::UTF8ToUTF16(kTestString));
  EXPECT_NE(base::string16::npos,
            render_text->GetDisplayText().find(base::UTF8ToUTF16(kEllipsis)));

  render_text->SetMultiline(true);
  EXPECT_EQ(base::string16::npos,
            render_text->GetDisplayText().find(base::UTF8ToUTF16(kEllipsis)));
}

TEST_P(RenderTextHarfBuzzTest, Multiline_NewlineCharacterReplacement) {
  const char* kTestStrings[] = {
      "abc\ndef", "a \n b ", "ab\n", "a\n\nb", "\nab", "\n",
  };

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    ResetRenderTextInstance();
    RenderText* render_text = GetRenderText();
    render_text->SetDisplayRect(Rect(200, 1000));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));

    base::string16 display_text = render_text->GetDisplayText();
    // If RenderText is not multiline, the newline characters are replaced
    // by symbols, therefore the character should be changed.
    EXPECT_NE(UTF8ToUTF16(kTestStrings[i]), render_text->GetDisplayText());

    // Setting multiline will fix this, the newline characters will be back
    // to the original text.
    render_text->SetMultiline(true);
    EXPECT_EQ(UTF8ToUTF16(kTestStrings[i]), render_text->GetDisplayText());
  }
}

// Ensure horizontal alignment works in multiline mode.
TEST_P(RenderTextHarfBuzzTest, Multiline_HorizontalAlignment) {
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
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetHorizontalAlignment(ALIGN_TO_HEAD);
  SetGlyphWidth(kGlyphSize);
  render_text->SetDisplayRect(Rect(100, 1000));
  render_text->SetMultiline(true);

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
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

      int difference = (lines[0].length() - lines[1].length()) * kGlyphSize;
      EXPECT_EQ(test_api()->GetAlignmentOffset(0).x() + difference,
                test_api()->GetAlignmentOffset(1).x());
    }
  }
}

TEST_P(RenderTextHarfBuzzTest, Multiline_WordWrapBehavior) {
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
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetMultiline(true);
  render_text->SetText(UTF8ToUTF16("foo fooooo foo"));
  SetGlyphWidth(kGlyphSize);
  render_text->SetDisplayRect(Rect(0, 0, kGlyphSize * 4, 0));

  for (size_t i = 0; i < arraysize(kTestScenarios); ++i) {
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

TEST_P(RenderTextHarfBuzzTest, Multiline_LineBreakerBehavior) {
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

  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetMultiline(true);
  SetGlyphWidth(kGlyphSize);
  render_text->SetDisplayRect(Rect(0, 0, kGlyphSize * 4, 0));

  for (size_t i = 0; i < arraysize(kTestScenarios); ++i) {
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
      EXPECT_EQ(kTestScenarios[i].char_ranges[j].length() * kGlyphSize,
                test_api()->lines()[j].size.width());
    }
  }
}

// Test that Surrogate pairs or combining character sequences do not get
// separated by line breaking.
TEST_P(RenderTextHarfBuzzTest, Multiline_SurrogatePairsOrCombiningChars) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
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
      { text_surrogate + text_surrogate + text_surrogate,
        kSurrogateWidth / 2 * 3,
        { Range(0, 2), Range(2, 4),  Range(4, 6) } },
      { text_surrogate + UTF8ToUTF16(" ") + kCombiningChars,
        std::min(kSurrogateWidth, kCombiningCharsWidth) / 2,
        { Range(0, 2), Range(2, 3), Range(3, 5) } },
  };

  for (size_t i = 0; i < arraysize(kTestScenarios); ++i) {
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
TEST_P(RenderTextHarfBuzzTest, Multiline_ZeroWidthChars) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

#if defined(OS_MACOSX)
  // Don't use Helvetica Neue on 10.10 - it has a buggy zero-width space that
  // actually gets some width. See http://crbug.com/799333.
  if (base::mac::IsOS10_10())
    render_text->SetFontList(FontList("Arial, 12px"));
#endif

  render_text->SetMultiline(true);
  render_text->SetWordWrapBehavior(WRAP_LONG_WORDS);

  const base::char16 kZeroWidthSpace = {0x200B};
  const base::string16 text(UTF8ToUTF16("test") + kZeroWidthSpace +
                            UTF8ToUTF16("\n") + kZeroWidthSpace +
                            UTF8ToUTF16("test."));
  const int kTestWidth =
      GetStringWidth(UTF8ToUTF16("test"), render_text->font_list());
  const Range char_ranges[3] = {Range(0, 5), Range(6, 11), Range(11, 12)};

  render_text->SetText(text);
  render_text->SetDisplayRect(Rect(0, 0, kTestWidth, 0));
  render_text->Draw(canvas());

  EXPECT_EQ(3u, test_api()->lines().size());
  for (size_t j = 0;
       j < std::min(arraysize(char_ranges), test_api()->lines().size()); ++j) {
    SCOPED_TRACE(base::StringPrintf("%" PRIuS "-th line", j));
    int segment_size = test_api()->lines()[j].segments.size();
    ASSERT_GT(segment_size, 0);
    Range line_range(
        test_api()->lines()[j].segments[0].char_range.start(),
        test_api()->lines()[j].segments[segment_size - 1].char_range.end());
    EXPECT_EQ(char_ranges[j], line_range);
  }
}

TEST_P(RenderTextHarfBuzzTest, NewlineWithoutMultilineFlag) {
  const char* kTestStrings[] = {
      "abc\ndef", "a \n b ", "ab\n", "a\n\nb", "\nab", "\n",
  };

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(200, 1000));

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    render_text->Draw(canvas());

    EXPECT_EQ(1U, test_api()->lines().size());
  }
}

// Make sure the horizontal positions of runs in a line (left-to-right for
// LTR languages and right-to-left for RTL languages).
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_HorizontalPositions) {
  const struct {
    const char* const text;
    const char* expected_runs;
  } kTestStrings[] = {
      {"abc\u3042\u3044\u3046\u3048\u304A", "[0->2][3->7]"},
      {"\u062A\u0641\u0627\u062D\u05EA\u05E4\u05D5\u05D6", "[7<-4][3<-0]"},
  };

  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(UTF8ToUTF16(kTestStrings[i].text));

    test_api()->EnsureLayout();
    EXPECT_EQ(kTestStrings[i].expected_runs, GetRunListStructureString());

    DrawVisualText();

    std::vector<TestSkiaTextRenderer::TextLog> text_log;
    renderer()->GetTextLogAndReset(&text_log);

    const internal::TextRunList* run_list = GetHarfBuzzRunList();
    ASSERT_EQ(2U, run_list->size());
    ASSERT_EQ(2U, text_log.size());

    // Verifies the DrawText happens in the visual order and left-to-right.
    // If the text is RTL, the logically first run should be drawn at last.
    EXPECT_EQ(
        run_list->runs()[run_list->logical_to_visual(0)]->shape.glyph_count,
        text_log[0].glyph_count);
    EXPECT_EQ(
        run_list->runs()[run_list->logical_to_visual(1)]->shape.glyph_count,
        text_log[1].glyph_count);
    EXPECT_LT(text_log[0].origin.x(), text_log[1].origin.x());
  }
}

// Test TextRunHarfBuzz's cluster finding logic.
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_Clusters) {
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

  for (size_t i = 0; i < arraysize(cases); ++i) {
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
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_NoCrashOnTextRunGetClusterAt) {
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
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_SubglyphGraphemeCases) {
  const char* cases[] = {
      // Ä (A with combining umlaut), followed by a "B".
      "A\u0308B",
      // कि (Devangari letter KA with vowel I), followed by an "a".
      "\u0915\u093f\u0905",
      // จำ (Thai charcters CHO CHAN and SARA AM, followed by Thai digit 0.
      "\u0e08\u0e33\u0E50",
  };

  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Case %" PRIuS, i));

    base::string16 text = UTF8ToUTF16(cases[i]);
    render_text->SetText(text);
    test_api()->EnsureLayout();
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
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_SubglyphGraphemePartition) {
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

  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetText(UTF8ToUTF16("abcd"));

  for (size_t i = 0; i < arraysize(cases); ++i) {
    std::copy(cases[i].glyph_to_char, cases[i].glyph_to_char + 2,
              run.shape.glyph_to_char.begin());
    run.font_params.is_rtl = cases[i].is_rtl;
    for (int j = 0; j < 2; ++j)
      run.shape.positions[j].set(j * 10, 0);

    for (size_t j = 0; j < 4; ++j) {
      SCOPED_TRACE(base::StringPrintf("Case %" PRIuS ", char %" PRIuS, i, j));
      EXPECT_EQ(cases[i].bounds[j],
                run.GetGraphemeBounds(render_text, j).Round());
    }
  }
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_RunDirection) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  const base::string16 mixed = UTF8ToUTF16("\u05D0\u05D11234\u05D2\u05D3abc");
  render_text->SetText(mixed);

  // Get the run list for both display directions.
  render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_LTR);
  test_api()->EnsureLayout();
  EXPECT_EQ("[7<-6][2->5][1<-0][8->10]", GetRunListStructureString());

  render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_RTL);
  test_api()->EnsureLayout();
  EXPECT_EQ("[8->10][7<-6][2->5][1<-0]", GetRunListStructureString());
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_RunDirection_URLs) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  // This string, unescaped (logical order):
  // ‭www.אב.גד/הוabc/def?זח=טי‬
  const base::string16 mixed = UTF8ToUTF16(
      "www.\u05D0\u05D1.\u05D2\u05D3/\u05D4\u05D5"
      "abc/def?\u05D6\u05D7=\u05D8\u05D9");
  render_text->SetText(mixed);

  // Normal LTR text should treat URL syntax as weak (as per the normal Bidi
  // algorithm).
  render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_LTR);
  test_api()->EnsureLayout();

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
  test_api()->EnsureLayout();
  EXPECT_EQ(kExpectedRunListNormalBidi, GetRunListStructureString());

  // Test the above again, but with LeftToRightUrls feature enabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLeftToRightUrls);

  // DIRECTIONALITY_FORCE_LTR should be the same as above.
  render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_LTR);
  test_api()->EnsureLayout();
  EXPECT_EQ(kExpectedRunListNormalBidi, GetRunListStructureString());

  // DIRECTIONALITY_AS_URL should treat URL syntax as strong LTR, to ensure
  // isolation between RTL characters in different components of the URL.
  render_text->SetDirectionalityMode(DIRECTIONALITY_AS_URL);
  test_api()->EnsureLayout();
  // TODO(mgiuca): Consider whether the rule should instead be "treat URL syntax
  // as delimiters for FIRST STRONG ISOLATEs", which would result in [12->14]
  // appearing to the left of [11<-10] (because the first strong character in
  // the range 10-14 is RTL).
  // Should render as: ‭www.בא.דג/והabc/def?חז=יט‬
  const char kExpectedRunListURLLayout[] =
      "[0->2][3][5<-4][6][8<-7][9][11<-10][12->14][15][16->18][19][21<-20][22]"
      "[24<-23]";
  EXPECT_EQ(kExpectedRunListURLLayout, GetRunListStructureString());
}

// More detailed tests of the LeftToRightUrls flag.
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_RunDirection_URLs_LeftToRight) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLeftToRightUrls);

  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetDirectionalityMode(DIRECTIONALITY_AS_URL);

  // Systematic test of all ASCII punctuation marks, interleaved with the Hebrew
  // alphabet. Those that count as delimiters (#&./:=?@) should break up the RTL
  // run, while those that don't should be treated as RTL characters.
  render_text->SetText(UTF8ToUTF16(
      "\u05d0!\u05d1\"\u05d2#\u05d3$\u05d4%\u05d5&\u05d6'\u05d7(\u05d8)\u05d9*"
      "\u05da+\u05db,\u05dc-\u05dd.\u05de/\u05df:\u05e0;\u05e1<\u05e2=\u05e3>"
      "\u05e4?\u05e5@\u05e6[\u05e7\\\u05e8]\u05e9^\u05ea_\u05d0`\u05d1{\u05d2|"
      "\u05d3}\u05d4~\u05d5"));
  test_api()->EnsureLayout();

  // This simplifies as:
  // "[4<-0][5][10<-6][11][26<-12][27->31][36<-32][37][40<-38][41->43][64<-44]";
  const char kExpectedRunListAllPunctuation[] =
      "[4][3][2][1][0][5][10][9][8][7][6][11][26][25][24][23][22][21][20][19]"
      "[18][17][16][15][14][13][12][27][28][29][30][31][36][35][34][33][32][37]"
      "[40][39][38][41][42][43][64][63][62][61][60][59][58][57][56][55][54][53]"
      "[52][51][50][49][48][47][46][45][44]";
  EXPECT_EQ(kExpectedRunListAllPunctuation, GetRunListStructureString());

  // Test Arabic instead of Hebrew.
  // This string, unescaped (logical order):
  // ‭www.ؠء.آأ/ؤإabc/def?ئا=بة‬
  render_text->SetText(
      UTF8ToUTF16("www.\u0620\u0621.\u0622\u0623/\u0624\u0625"
                  "abc/def?\u0626\u0627=\u0628\u0629"));
  test_api()->EnsureLayout();

  // Should render as: ‭www.ءؠ.أآ/إؤabc/def?ائ=ةب‬
  // Same expected run list as in HarfBuzz_RunDirection_URLs.
  const char kExpectedRunListArabic[] =
      "[0->2][3][5<-4][6][8<-7][9][11<-10][12->14][15][16->18][19][21<-20][22]"
      "[24<-23]";
  EXPECT_EQ(kExpectedRunListArabic, GetRunListStructureString());
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_BreakRunsByUnicodeBlocks) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  // The ▶ (U+25B6) "play character" should break runs. http://crbug.com/278913
  render_text->SetText(UTF8ToUTF16("x\u25B6y"));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"x", "▶", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1][2]", GetRunListStructureString());

  render_text->SetText(UTF8ToUTF16("x \u25B6 y"));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"x", " ", "▶", " ", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1][2][3][4]", GetRunListStructureString());
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_BreakRunsByEmoji) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  // 😁 (U+1F601, a smile emoji) and ✨ (U+2728, a sparkle icon) can both be
  // drawn with color emoji fonts, so runs should be separated. crbug.com/448909
  // Windows requires wide strings for \Unnnnnnnn universal character names.
  render_text->SetText(WideToUTF16(L"x\U0001F601y\u2728"));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"x", "😁", "y", "✨"}), GetRunListStrings());
  // U+1F601 is represented as a surrogate pair in UTF-16.
  EXPECT_EQ("[0][1->2][3][4]", GetRunListStructureString());

  // Ensure non-latin 「foo」 brackets around Emoji correctly break runs.
  render_text->SetText(UTF8ToUTF16("「🦋」「"));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"「", "🦋", "」「"}), GetRunListStrings());
  // Note 🦋 is a surrogate pair [1->2].
  EXPECT_EQ("[0][1->2][3->4]", GetRunListStructureString());
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_BreakRunsByEmojiVariationSelectors) {
  constexpr int kGlyphWidth = 30;
  SetGlyphWidth(30);
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  // ☎ (U+260E BLACK TELEPHONE) and U+FE0F (a variation selector) combine to
  // form (on some platforms), ☎️, a red (or blue) telephone. The run can
  // not break between the codepoints, or the incorrect glyph will be chosen.
  render_text->SetText(WideToUTF16(L"z\u260E\uFE0Fy"));
  render_text->SetDisplayRect(Rect(1000, 50));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"z", "☎\uFE0F", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1->2][3]", GetRunListStructureString());

  // Also test moving the cursor across the telephone.
  EXPECT_EQ(gfx::Range(0, 0), render_text->selection());
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, SELECTION_NONE);
  EXPECT_EQ(gfx::Range(1, 1), render_text->selection());
  EXPECT_EQ(1 * kGlyphWidth, render_text->GetUpdatedCursorBounds().x());

#if defined(OS_MACOSX)
  // Early versions of macOS provide a tofu glyph for the variation selector.
  // Bail out early except on 10.12 and above.
  if (base::mac::IsAtMostOS10_11())
    return;
#endif

#if defined(OS_ANDROID)
  // TODO(865709): make this work on Android.
  return;
#endif

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
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_OrphanedVariationSelector) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  // It should never happen in normal usage, but a variation selector can appear
  // by itself. In this case, it can form its own text run, with no glyphs.
  render_text->SetText(WideToUTF16(L"\uFE0F"));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"\uFE0F"}), GetRunListStrings());
  EXPECT_EQ("[0]", GetRunListStructureString());
  CheckBoundsForCursorPositions();
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_AsciiVariationSelector) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
#if defined(OS_MACOSX)
  // Don't use a system font on macOS - asking for a variation selector on
  // ASCII glyphs can tickle OS bugs. See http://crbug.com/785522.
  render_text->SetFontList(FontList("Arial, 12px"));
#endif
  // A variation selector doesn't have to appear with Emoji. It will probably
  // cause the typesetter to render tofu in this case, but it should not break
  // a text run.
  render_text->SetText(WideToUTF16(L"z\uFE0Fy"));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"z\uFE0Fy"}), GetRunListStrings());
  EXPECT_EQ("[0->2]", GetRunListStructureString());
  CheckBoundsForCursorPositions();
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_LeadingVariationSelector) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  // When a variation selector appears either side of an emoji, ensure the one
  // after is in the same run.
  render_text->SetText(WideToUTF16(L"\uFE0F\u260E\uFE0Fy"));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"\uFE0F", "☎\uFE0F", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1->2][3]", GetRunListStructureString());
  CheckBoundsForCursorPositions();
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_TrailingVariationSelector) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  // If a redundant variation selector appears in an emoji run, it also gets
  // merged into the emoji run. Usually there should be no effect. That's
  // ultimately up to the typeface but, however it choses, cursor and glyph
  // positions should behave.
  render_text->SetText(WideToUTF16(L"z\u260E\uFE0F\uFE0Fy"));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"z", "☎\uFE0F\uFE0F", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1->3][4]", GetRunListStructureString());
  CheckBoundsForCursorPositions();
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_MultipleVariationSelectorEmoji) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  // Two emoji with variation selectors appearing in a correct sequence should
  // be in the same run.
  render_text->SetText(WideToUTF16(L"z\u260E\uFE0F\u260E\uFE0Fy"));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"z", "☎\uFE0F☎\uFE0F", "y"}), GetRunListStrings());
  EXPECT_EQ("[0][1->4][5]", GetRunListStructureString());
  CheckBoundsForCursorPositions();
}

TEST_P(RenderTextHarfBuzzTest, HarfBuzz_BreakRunsByAscii) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();

  // 🐱 (U+1F431, a cat face) and an ASCII period should have separate runs.
  // Windows requires wide strings for \Unnnnnnnn universal character names.
  render_text->SetText(WideToUTF16(L"\U0001F431."));
  test_api()->EnsureLayout();
  EXPECT_EQ(ToString16Vec({"🐱", "."}), GetRunListStrings());
  // U+1F431 is represented as a surrogate pair in UTF-16.
  EXPECT_EQ("[0->1][2]", GetRunListStructureString());
}

// Test that, on Mac, font fallback mechanisms and Harfbuzz configuration cause
// the correct glyphs to be chosen for unicode regional indicators.
TEST_P(RenderTextHarfBuzzTest, EmojiFlagGlyphCount) {
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(1000, 1000));
  // Two flags: UK and Japan. Note macOS 10.9 only has flags for 10 countries.
  base::string16 text(UTF8ToUTF16("🇬🇧🇯🇵"));
  // Each flag is 4 UTF16 characters (2 surrogate pair code points).
  EXPECT_EQ(8u, text.length());
  render_text->SetText(text);
  test_api()->EnsureLayout();

  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  ASSERT_EQ(1U, run_list->runs().size());
#if defined(OS_MACOSX)
  // On Mac, the flags should be found, so two glyphs result.
  EXPECT_EQ(2u, run_list->runs()[0]->shape.glyph_count);
#else
  // Elsewhere, the flags are not found, so each surrogate pair gets a
  // placeholder glyph. Eventually, all platforms should have 2 glyphs.
  EXPECT_EQ(4u, run_list->runs()[0]->shape.glyph_count);
#endif
}

TEST_P(RenderTextHarfBuzzTest, GlyphBounds) {
  const char* kTestStrings[] = {"asdf 1234 qwer", "\u0647\u0654",
                                "\u0645\u0631\u062D\u0628\u0627"};
  RenderText* render_text = GetRenderText();

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    render_text->SetText(UTF8ToUTF16(kTestStrings[i]));
    test_api()->EnsureLayout();

    for (size_t j = 0; j < render_text->text().length(); ++j)
      EXPECT_FALSE(render_text->GetCursorSpan(Range(j, j + 1)).is_empty());
  }
}

// Ensure that shaping with a non-existent font does not cause a crash.
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_NonExistentFont) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetText(UTF8ToUTF16("test"));
  test_api()->EnsureLayout();
  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  ASSERT_EQ(1U, run_list->size());
  internal::TextRunHarfBuzz* run = run_list->runs()[0].get();
  ShapeRunWithFont(render_text->text(), Font("TheFontThatDoesntExist", 13),
                   FontRenderParams(), run);
}

// Ensure an empty run returns sane values to queries.
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_EmptyRun) {
  internal::TextRunHarfBuzz run((Font()));
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetText(UTF8ToUTF16("abcdefgh"));

  run.range = Range(3, 8);
  run.shape.glyph_count = 0;
  EXPECT_EQ(Range(0, 0), run.CharRangeToGlyphRange(Range(4, 5)));
  EXPECT_EQ(Range(0, 0), run.GetGraphemeBounds(render_text, 4).Round());
  Range chars;
  Range glyphs;
  run.GetClusterAt(4, &chars, &glyphs);
  EXPECT_EQ(Range(3, 8), chars);
  EXPECT_EQ(Range(0, 0), glyphs);
}

// Ensure the line breaker doesn't compute the word's width bigger than the
// actual size. See http://crbug.com/470073
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_WordWidthWithDiacritics) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
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
TEST_P(RenderTextTest, StringFitsOwnWidth) {
  RenderText* render_text = GetRenderText();
  const base::string16 kString = UTF8ToUTF16("www.example.com");

  render_text->SetText(kString);
  render_text->ApplyWeight(Font::Weight::BOLD, Range(0, 3));
  render_text->SetElideBehavior(ELIDE_TAIL);

  render_text->SetDisplayRect(Rect(0, 0, 500, 100));
  EXPECT_EQ(kString, render_text->GetDisplayText());
  render_text->SetDisplayRect(Rect(0, 0, render_text->GetContentWidth(), 100));
  EXPECT_EQ(kString, render_text->GetDisplayText());
}

// TODO(derat): Figure out why this fails on Windows: http://crbug.com/427184
// TODO(865715): Figure out why this fails on Android.
#if !defined(OS_WIN) && !defined(OS_ANDROID)
// Ensure that RenderText examines all of the fonts in its FontList before
// falling back to other fonts.
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_FontListFallback) {
  // Double-check that the requested fonts are present.
  std::string format = std::string(kTestFontName) + ", %s, 12px";
  FontList font_list(base::StringPrintf(format.c_str(), kSymbolFontName));
  const std::vector<Font>& fonts = font_list.GetFonts();
  ASSERT_EQ(2u, fonts.size());
  ASSERT_EQ(base::ToLowerASCII(kTestFontName),
            base::ToLowerASCII(fonts[0].GetActualFontNameForTesting()));
  ASSERT_EQ(base::ToLowerASCII(kSymbolFontName),
            base::ToLowerASCII(fonts[1].GetActualFontNameForTesting()));

  // "⊕" (U+2295, CIRCLED PLUS) should be rendered with Symbol rather than
  // falling back to some other font that's present on the system.
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetFontList(font_list);
  render_text->SetText(UTF8ToUTF16("\u2295"));
  const std::vector<RenderText::FontSpan> spans =
      render_text->GetFontSpansForTesting();
  ASSERT_EQ(static_cast<size_t>(1), spans.size());
  EXPECT_STRCASEEQ(kSymbolFontName, spans[0].first.GetFontName().c_str());
}
#endif  // !defined(OS_WIN) && !defined(OS_ANDROID)

// Ensure that the fallback fonts of the Uniscribe font are tried for shaping.
#if defined(OS_WIN)
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_UniscribeFallback) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  PlatformFontWin* font_win = new PlatformFontWin("Meiryo", 12);
  // Japanese name for Meiryo. This name won't be found in the system's linked
  // fonts, forcing RTHB to try the Uniscribe font and its fallbacks.
  font_win->font_ref_->font_name_ = "\u30e1\u30a4\u30ea\u30aa";
  FontList font_list((Font(font_win)));

  render_text->SetFontList(font_list);
  // An invalid Unicode character that somehow yields Korean character "han".
  render_text->SetText(UTF8ToUTF16("\ud55c"));
  test_api()->EnsureLayout();
  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  ASSERT_EQ(1U, run_list->size());
  EXPECT_EQ(0U, run_list->runs()[0]->CountMissingGlyphs());
}
#endif  // defined(OS_WIN)

// Ensure that the fallback fonts offered by GetFallbackFonts() are tried. Note
// this test assumes the font "Arial" doesn't provide a unicode glyph for a
// particular character, and that there is a system fallback font which does.
// TODO(msw): Fallback doesn't find a glyph on Linux and Android.
#if !defined(OS_LINUX) && !defined(OS_ANDROID)
TEST_P(RenderTextHarfBuzzTest, HarfBuzz_UnicodeFallback) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetFontList(FontList("Arial, 12px"));

  // An invalid Unicode character that somehow yields Korean character "han".
  render_text->SetText(UTF8ToUTF16("\ud55c"));
  test_api()->EnsureLayout();
  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  ASSERT_EQ(1U, run_list->size());
  EXPECT_EQ(0U, run_list->runs()[0]->CountMissingGlyphs());
}
#endif  // !defined(OS_LINUX)

// Ensure that the width reported by RenderText is sufficient for drawing. Draws
// to a canvas and checks if any pixel beyond the bounding rectangle is colored.
TEST_P(RenderTextTest, TextDoesntClip) {
  const char* kTestStrings[] = {
      "            ",
      // TODO(dschuyler): Underscores draw outside GetStringSize;
      // crbug.com/459812.  This appears to be a preexisting issue that wasn't
      // revealed by the prior unit tests.
      // "TEST_______",
      "TEST some stuff", "WWWWWWWWWW", "gAXAXAXAXAXAXA",
      "g\u00C5X\u00C5X\u00C5X\u00C5X\u00C5X\u00C5X\u00C5",
      "\u0647\u0654\u0647\u0654\u0647\u0654\u0647\u0654\u0645\u0631\u062D"
      "\u0628\u0627"};
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
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_ANDROID) || defined(ARCH_CPU_MIPS_FAMILY)
      // TODO(dschuyler): On Windows, Chrome OS, Linux, Android, and Mac
      // smoothing draws to the left of text.  This appears to be a preexisting
      // issue that wasn't revealed by the prior unit tests.  RenderText
      // currently only uses origins and advances and ignores bounding boxes so
      // cannot account for under- and over-hang.
      rect_buffer.EnsureSolidRect(SK_ColorWHITE, 0, kTestSize, kTestSize - 1,
                                  string_size.height());
#else
      rect_buffer.EnsureSolidRect(SK_ColorWHITE, 0, kTestSize, kTestSize,
                                  string_size.height());
#endif
    }
    {
      SCOPED_TRACE("TextDoesntClip Right Side");
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_ANDROID) || defined(ARCH_CPU_MIPS_FAMILY)
      // TODO(dschuyler): On Windows, Chrome OS, Linux, Android, and Mac
      // smoothing draws to the right of text.  This appears to be a preexisting
      // issue that wasn't revealed by the prior unit tests.  RenderText
      // currently only uses origins and advances and ignores bounding boxes so
      // cannot account for under- and over-hang.
      rect_buffer.EnsureSolidRect(SK_ColorWHITE,
                                  kTestSize + string_size.width() + 1,
                                  kTestSize, kTestSize - 1,
                                  string_size.height());
#else
      rect_buffer.EnsureSolidRect(SK_ColorWHITE,
                                  kTestSize + string_size.width(),
                                  kTestSize, kTestSize,
                                  string_size.height());
#endif
    }
  }
}

// Ensure that the text will clip to the display rect. Draws to a canvas and
// checks whether any pixel beyond the bounding rectangle is colored.
TEST_P(RenderTextTest, TextDoesClip) {
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

#if defined(OS_MACOSX)
TEST_P(RenderTextMacTest, Mac_ElidedText) {
  RenderTextMac* render_text = GetRenderTextMac();
  base::string16 text(UTF8ToUTF16("This is an example."));
  render_text->SetText(text);
  Size string_size = render_text->GetStringSize();
  render_text->SetDisplayRect(Rect(string_size));
  test_api()->EnsureLayout();
  // NOTE: Character and glyph counts are only comparable for simple text.
  EXPECT_EQ(text.size(),
            static_cast<size_t>(CTLineGetGlyphCount(GetCoreTextLine())));

  render_text->SetElideBehavior(ELIDE_TAIL);
  string_size.set_width(string_size.width() / 2);
  render_text->SetDisplayRect(Rect(string_size));
  test_api()->EnsureLayout();
  CFIndex glyph_count = CTLineGetGlyphCount(GetCoreTextLine());
  EXPECT_GT(text.size(), static_cast<size_t>(glyph_count));
  EXPECT_NE(0, glyph_count);
}

TEST_P(RenderTextMacTest, LinesInvalidationOnElideBehaviorChange) {
  RenderTextMac* render_text = GetRenderTextMac();
  render_text->SetText(UTF8ToUTF16("This is an example"));
  test_api()->EnsureLayout();
  EXPECT_TRUE(GetCoreTextLine());

  // Lines are cleared when elide behavior changes.
  render_text->SetElideBehavior(gfx::ELIDE_TAIL);
  EXPECT_FALSE(GetCoreTextLine());
}
#endif  // defined(OS_MACOSX)

// Ensure color changes are picked up by the RenderText implementation.
TEST_P(RenderTextTest, ColorChange) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("x"));
  DrawVisualText();

  std::vector<TestSkiaTextRenderer::TextLog> text_log;
  renderer()->GetTextLogAndReset(&text_log);
  EXPECT_EQ(1u, text_log.size());
  EXPECT_EQ(SK_ColorBLACK, text_log[0].color);

  render_text->SetColor(SK_ColorRED);
  DrawVisualText();
  renderer()->GetTextLogAndReset(&text_log);

  EXPECT_EQ(1u, text_log.size());
  EXPECT_EQ(SK_ColorRED, text_log[0].color);
}

// Ensure style information propagates to the typeface on the text renderer.
TEST_P(RenderTextTest, StylePropagated) {
  RenderText* render_text = GetRenderText();
  // Default-constructed fonts on Mac are system fonts. These can have all kinds
  // of weird weights and style, which are preserved by PlatformFontMac, but do
  // not map simply to a SkTypeface::Style (the full details in SkFontStyle is
  // needed). They also vary depending on the OS version, so set a known font.
  FontList font_list(Font("Arial", 10));

  render_text->SetText(UTF8ToUTF16("x"));
  render_text->SetFontList(font_list);

  DrawVisualText();
  EXPECT_EQ(SkFontStyle::Normal(),
            GetRendererPaint().getTypeface()->fontStyle());

  render_text->SetWeight(Font::Weight::BOLD);
  DrawVisualText();
  EXPECT_EQ(SkFontStyle::Bold(), GetRendererPaint().getTypeface()->fontStyle());

  render_text->SetStyle(TextStyle::ITALIC, true);
  DrawVisualText();
  EXPECT_EQ(SkFontStyle::BoldItalic(),
            GetRendererPaint().getTypeface()->fontStyle());

  render_text->SetWeight(Font::Weight::NORMAL);
  DrawVisualText();
  EXPECT_EQ(SkFontStyle::Italic(),
            GetRendererPaint().getTypeface()->fontStyle());
}

// Ensure the painter adheres to RenderText::subpixel_rendering_suppressed().
TEST_P(RenderTextTest, SubpixelRenderingSuppressed) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("x"));

  DrawVisualText();
#if defined(OS_LINUX) || defined(OS_ANDROID)
  // On Linux, whether subpixel AA is supported is determined by the platform
  // FontConfig. Force it into a particular style after computing runs. Other
  // platforms use a known default FontRenderParams from a static local.
  GetHarfBuzzRunList()
      ->runs()[0]
      ->font_params.render_params.subpixel_rendering =
      FontRenderParams::SUBPIXEL_RENDERING_RGB;
  DrawVisualText();
#endif
  EXPECT_TRUE(GetRendererPaint().isLCDRenderText());

  render_text->set_subpixel_rendering_suppressed(true);
  DrawVisualText();
#if defined(OS_LINUX) || defined(OS_ANDROID)
  // For Linux, runs shouldn't be re-calculated, and the suppression of the
  // SUBPIXEL_RENDERING_RGB set above should now take effect. But, after
  // checking, apply the override anyway to be explicit that it is suppressed.
  EXPECT_FALSE(GetRendererPaint().isLCDRenderText());
  GetHarfBuzzRunList()
      ->runs()[0]
      ->font_params.render_params.subpixel_rendering =
      FontRenderParams::SUBPIXEL_RENDERING_RGB;
  DrawVisualText();
#endif
  EXPECT_FALSE(GetRendererPaint().isLCDRenderText());
}

// Verify GetWordLookupDataAtPoint returns the correct baseline point and
// decorated word for an LTR string.
TEST_P(RenderTextHarfBuzzTest, GetWordLookupDataAtPoint_LTR) {
  const base::string16 ltr = UTF8ToUTF16("  ab  c ");
  const int kWordOneStartIndex = 2;
  const int kWordTwoStartIndex = 6;

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(100, 30));
  render_text->SetText(ltr);
  render_text->ApplyWeight(Font::Weight::SEMIBOLD, Range(0, 3));
  render_text->ApplyStyle(UNDERLINE, true, Range(1, 5));
  render_text->ApplyStyle(ITALIC, true, Range(3, 8));
  render_text->ApplyStyle(STRIKE, true, Range(1, 7));
  const int cursor_y = GetCursorYForTesting();

  const std::vector<RenderText::FontSpan> font_spans =
      render_text->GetFontSpansForTesting();

  // Create expected decorated text instances.
  DecoratedText expected_word_1;
  expected_word_1.text = UTF8ToUTF16("ab");
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
  expected_word_2.text = UTF8ToUTF16("c");
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
TEST_P(RenderTextHarfBuzzTest, GetWordLookupDataAtPoint_RTL) {
  const base::string16 rtl = UTF8ToUTF16(" \u0634\u0632  \u0634");
  const int kWordOneStartIndex = 1;
  const int kWordTwoStartIndex = 5;

  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(100, 30));
  render_text->SetText(rtl);
  render_text->ApplyWeight(Font::Weight::SEMIBOLD, Range(2, 3));
  render_text->ApplyStyle(UNDERLINE, true, Range(3, 6));
  render_text->ApplyStyle(ITALIC, true, Range(0, 3));
  render_text->ApplyStyle(STRIKE, true, Range(2, 5));
  const int cursor_y = GetCursorYForTesting();

  const std::vector<RenderText::FontSpan> font_spans =
      render_text->GetFontSpansForTesting();

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
TEST_P(RenderTextHarfBuzzTest, GetWordLookupDataAtPoint_Multiline) {
  const base::string16 text = UTF8ToUTF16("a b\n..\ncd.");
  const size_t kWordOneIndex = 0;    // Index of character 'a'.
  const size_t kWordTwoIndex = 2;    // Index of character 'b'.
  const size_t kWordThreeIndex = 7;  // Index of character 'c'.

  // Set up render text.
  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetDisplayRect(Rect(500, 500));
  render_text->SetText(text);
  render_text->ApplyWeight(Font::Weight::SEMIBOLD, Range(0, 3));
  render_text->ApplyStyle(UNDERLINE, true, Range(1, 7));
  render_text->ApplyStyle(STRIKE, true, Range(1, 8));
  render_text->ApplyStyle(ITALIC, true, Range(5, 9));

  // Set up test expectations.
  const std::vector<RenderText::FontSpan> font_spans =
      render_text->GetFontSpansForTesting();

  DecoratedText expected_word_1;
  expected_word_1.text = UTF8ToUTF16("a");
  expected_word_1.attributes.push_back(CreateRangedAttribute(
      font_spans, 0, kWordOneIndex, Font::Weight::SEMIBOLD, 0));
  const Rect left_glyph_word_1 =
      GetSubstringBoundsUnion(Range(kWordOneIndex, kWordOneIndex + 1));

  DecoratedText expected_word_2;
  expected_word_2.text = UTF8ToUTF16("b");
  expected_word_2.attributes.push_back(CreateRangedAttribute(
      font_spans, 0, kWordTwoIndex, Font::Weight::SEMIBOLD,
      UNDERLINE_MASK | STRIKE_MASK));
  const Rect left_glyph_word_2 =
      GetSubstringBoundsUnion(Range(kWordTwoIndex, kWordTwoIndex + 1));

  DecoratedText expected_word_3;
  expected_word_3.text = UTF8ToUTF16("cd");
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
TEST_P(RenderTextHarfBuzzTest, GetWordLookupDataAtPoint_Return) {
  RenderText* render_text = GetRenderText();
  render_text->SetText(UTF8ToUTF16("..."));

  DecoratedText decorated_word;
  Point baseline_point;

  // False should be returned, when the text does not contain any word.
  Point query =
      render_text->GetCursorBounds(SelectionModel(0, CURSOR_FORWARD), false)
          .origin();
  EXPECT_FALSE(render_text->GetWordLookupDataAtPoint(query, &decorated_word,
                                                     &baseline_point));

  render_text->SetText(UTF8ToUTF16("abc"));
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
TEST_P(RenderTextHarfBuzzTest, GetLookupDataAtRange_Multiline) {
  const base::string16 text = UTF8ToUTF16("a\nb");
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
  render_text->ApplyStyle(UNDERLINE, true, kWordTwoRange);

  // Set up test expectations.
  const std::vector<RenderText::FontSpan> font_spans =
      render_text->GetFontSpansForTesting();

  DecoratedText expected_word_1;
  expected_word_1.text = UTF8ToUTF16("a");
  expected_word_1.attributes.push_back(CreateRangedAttribute(
      font_spans, 0, kWordOneRange.start(), Font::Weight::SEMIBOLD, 0));
  const Rect left_glyph_word_1 = GetSubstringBoundsUnion(kWordOneRange);

  DecoratedText expected_word_2;
  expected_word_2.text = UTF8ToUTF16("b");
  expected_word_2.attributes.push_back(
      CreateRangedAttribute(font_spans, 0, kWordTwoRange.start(),
                            Font::Weight::NORMAL, UNDERLINE_MASK));
  const Rect left_glyph_word_2 = GetSubstringBoundsUnion(kWordTwoRange);

  DecoratedText expected_entire_text;
  expected_entire_text.text = UTF8ToUTF16("a\nb");
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
TEST_P(RenderTextHarfBuzzTest, LineEndSelections) {
  const char* const ltr = "abc\n\ndef";
  const char* const rtl = "\u05d0\u05d1\u05d2\n\n\u05d3\u05d4\u05d5";
  const int left_x = -100;
  const int right_x = 200;
  struct {
    const char* const text;
    const int line_num;
    const int x;
    const char* const selected_text;
  } cases[] = {
      {ltr, 1, left_x, "abc\n"},
      {ltr, 1, right_x, "abc\n\n"},
      {ltr, 2, left_x, "abc\n\n"},
      {ltr, 2, right_x, ltr},

      {rtl, 1, left_x, "\u05d0\u05d1\u05d2\n\n"},
      {rtl, 1, right_x, "\u05d0\u05d1\u05d2\n"},
      {rtl, 2, left_x, rtl},
      {rtl, 2, right_x, "\u05d0\u05d1\u05d2\n\n"},
  };

  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetDisplayRect(Rect(200, 1000));

  for (size_t i = 0; i < arraysize(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing case %" PRIuS "", i));
    render_text->SetText(UTF8ToUTF16(cases[i].text));
    test_api()->EnsureLayout();

    EXPECT_EQ(3u, test_api()->lines().size());
    // Position the cursor at the logical beginning of text.
    render_text->SelectRange(Range(0));

    render_text->MoveCursorToPoint(
        Point(cases[i].x, GetCursorYForTesting(cases[i].line_num)), true);
    EXPECT_EQ(UTF8ToUTF16(cases[i].selected_text),
              GetSelectedText(render_text));
  }
}

// Tests that GetSubstringBounds returns the correct bounds for multiline text.
TEST_P(RenderTextHarfBuzzTest, GetSubstringBoundsMultiline) {
  RenderText* render_text = GetRenderText();
  render_text->SetMultiline(true);
  render_text->SetDisplayRect(Rect(200, 1000));
  render_text->SetText(UTF8ToUTF16("abc\n\ndef"));
  test_api()->EnsureLayout();

  const std::vector<Range> line_char_range = {Range(0, 3), Range(4, 5),
                                              Range(5, 8)};

  // Test bounds for individual lines.
  EXPECT_EQ(3u, test_api()->lines().size());
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
TEST_P(RenderTextTest, InvalidFont) {
  const std::string font_name = "invalid_font";
  const int kFontSize = 13;
  RenderText* render_text = GetRenderText();
  render_text->SetFontList(FontList(Font(font_name, kFontSize)));
  render_text->SetText(UTF8ToUTF16("abc"));

  DrawVisualText();
}

TEST_P(RenderTextTest, ExpandToBeVerticallySymmetric) {
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

TEST_P(RenderTextHarfBuzzTest, LinesInvalidationOnElideBehaviorChange) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetText(UTF8ToUTF16("This is an example"));
  test_api()->EnsureLayout();
  EXPECT_FALSE(test_api()->lines().empty());

  // Lines are cleared when elide behavior changes.
  render_text->SetElideBehavior(gfx::ELIDE_TAIL);
  EnsureLayoutRunList();
  EXPECT_TRUE(test_api()->lines().empty());
}

// Ensures that text is centered vertically and consistently when either the
// display rectangle height changes, or when the minimum line height changes.
// The difference between the two is the selection rectangle, which should match
// the line height.
TEST_P(RenderTextHarfBuzzTest, BaselineWithLineHeight) {
  RenderText* render_text = GetRenderText();
  const int font_height = render_text->font_list().GetHeight();
  render_text->SetDisplayRect(Rect(500, font_height));
  render_text->SetText(UTF8ToUTF16("abc"));

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

TEST_P(RenderTextHarfBuzzTest, TeluguGraphemeBoundaries) {
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
  const int half_width = whole_width / 2;
  // Sanity check. A typical width is 8 pixels. Anything less than 6 could screw
  // up the checks below with rounding.
  EXPECT_LE(6, whole_width);

  // Go to the end and perform Shift+Left. The selection should jump to a
  // grapheme boundary and enclose the second grapheme in the ligature.
  render_text->SetCursorPosition(4);
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_RETAIN);
  EXPECT_EQ(Range(4, 2), render_text->selection());
  test_api()->EnsureLayout();

  // The selection should start before the string end, and cover about half the
  // width. Note this requires the RenderText implementation to return a
  // sensible selection bounds for the diacritical at the end of the ligature.
  Rect selection_bounds = GetSelectionBoundsUnion();
  // The selection width rounds up, and half_width may already be rounded down.
  EXPECT_GE(1, selection_bounds.width() - half_width);
  EXPECT_GE(1, selection_bounds.x() - half_width);

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, SELECTION_RETAIN);
  EXPECT_EQ(Range(4, 0), render_text->selection());
  test_api()->EnsureLayout();

  // The selection should cover the entire width.
  selection_bounds = GetSelectionBoundsUnion();
  EXPECT_EQ(0, selection_bounds.x());
  EXPECT_EQ(whole_width, selection_bounds.width());
}

// Test cursor bounds for Emoji flags (unicode regional indicators) when the
// flag does not merge into a single glyph.
TEST_P(RenderTextHarfBuzzTest, MissingFlagEmoji) {
  RenderText* render_text = GetRenderText();
  render_text->SetDisplayRect(Rect(1000, 1000));

  // Usually these pair into country codes, but for this test we do not want
  // them to combine into a flag. Instead, the placeholder glyphs should be used
  // but cursor navigation should still behave as though they are joined. To get
  // placeholder glyphs, make up a non-existent country. The codes used are
  // based on ISO 3166-1 alpha-2. Codes starting with X are user-assigned.
  base::string16 text(UTF8ToUTF16("🇽🇽🇽🇽"));
  // Each flag is 4 UTF16 characters (2 surrogate pair code points).
  EXPECT_EQ(8u, text.length());

  render_text->SetText(text);
  test_api()->EnsureLayout();

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

// Ensures that glyph spacing is correctly applied to obscured texts.
TEST_P(RenderTextHarfBuzzTest, GlyphSpacing) {
  const base::string16 seuss = UTF8ToUTF16("hop on pop");
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  render_text->SetText(seuss);
  render_text->SetObscured(true);
  test_api()->EnsureLayout();
  const internal::TextRunList* run_list = GetHarfBuzzRunList();
  ASSERT_EQ(1U, run_list->size());
  internal::TextRunHarfBuzz* run = run_list->runs()[0].get();
  // The default glyph spacing is zero.
  EXPECT_EQ(0, render_text->glyph_spacing());
  ShapeRunWithFont(render_text->text(), Font(), FontRenderParams(), run);
  const float width_without_glyph_spacing = run->shape.width;

  const float kGlyphSpacing = 5;
  render_text->set_glyph_spacing(kGlyphSpacing);
  ShapeRunWithFont(render_text->text(), Font(), FontRenderParams(), run);
  // The new width is the sum of |width_without_glyph_spacing| and the spacing.
  const float total_spacing = seuss.length() * kGlyphSpacing;
  EXPECT_EQ(width_without_glyph_spacing + total_spacing, run->shape.width);
}

// Ensure font size overrides propagate through to text runs.
TEST_P(RenderTextHarfBuzzTest, FontSizeOverride) {
  RenderTextHarfBuzz* render_text = GetRenderTextHarfBuzz();
  const int default_font_size = render_text->font_list().GetFontSize();
  const int test_font_size_override = default_font_size + 5;
  render_text->SetText(UTF8ToUTF16("0123456789"));
  render_text->ApplyFontSizeOverride(test_font_size_override, gfx::Range(3, 7));
  test_api()->EnsureLayout();
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

// Prefix for test instantiations intentionally left blank since each test
// fixture class has a single parameterization.
#if defined(OS_MACOSX)
INSTANTIATE_TEST_CASE_P(,
                        RenderTextTest,
                        ::testing::Values(RENDER_TEXT_HARFBUZZ,
                                          RENDER_TEXT_MAC),
                        PrintRenderTextBackend());
INSTANTIATE_TEST_CASE_P(,
                        RenderTextMacTest,
                        ::testing::Values(RENDER_TEXT_MAC),
                        PrintRenderTextBackend());
#else
INSTANTIATE_TEST_CASE_P(,
                        RenderTextTest,
                        ::testing::Values(RENDER_TEXT_HARFBUZZ),
                        PrintRenderTextBackend());
#endif

INSTANTIATE_TEST_CASE_P(,
                        RenderTextHarfBuzzTest,
                        ::testing::Values(RENDER_TEXT_HARFBUZZ),
                        PrintRenderTextBackend());

}  // namespace gfx
