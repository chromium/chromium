// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_HARFBUZZ_H_
#define UI_GFX_RENDER_TEXT_HARFBUZZ_H_

#include <hb.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/gfx/render_text.h"

namespace gfx {

class Range;
class RangeF;
class RenderTextHarfBuzz;

namespace internal {

// Font fallback mechanism used to Shape runs (see ShapeRuns(...)).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ShapeRunFallback {
  FAILED = 0,
  NO_FALLBACK = 1,
  FALLBACK = 2,
  FALLBACKS = 3,
  kMaxValue = FALLBACKS
};

struct GFX_EXPORT TextRunHarfBuzz {
  // Construct the run with |template_font| since determining the details of a
  // default-constructed gfx::Font is expensive, but it will always be replaced.
  explicit TextRunHarfBuzz(const Font& template_font);

  TextRunHarfBuzz(const TextRunHarfBuzz&) = delete;
  TextRunHarfBuzz& operator=(const TextRunHarfBuzz&) = delete;

  ~TextRunHarfBuzz();

  // Returns the corresponding glyph range of the given character range.
  // |range| is in text-space (0 corresponds to |GetDisplayText()[0]|). Returned
  // value is in run-space (0 corresponds to the first glyph in the run).
  Range CharRangeToGlyphRange(const Range& range) const;

  // Returns the number of missing glyphs in the shaped text run.
  size_t CountMissingGlyphs() const;

  // Writes the character and glyph ranges of the cluster containing |pos|.
  void GetClusterAt(size_t pos, Range* chars, Range* glyphs) const;

  // Returns the grapheme bounds at |text_index|. Handles multi-grapheme glyphs.
  // Returned value is the horizontal pixel span in text-space (assumes all runs
  // are on the same line). The returned range is never reversed.
  RangeF GetGraphemeBounds(RenderTextHarfBuzz* render_text,
                           size_t text_index) const;

  // Returns the horizontal span of the given |char_range| handling grapheme
  // boundaries within glyphs. This is a wrapper around one or more calls to
  // GetGraphemeBounds(), returning a range in the same coordinate space.
  RangeF GetGraphemeSpanForCharRange(RenderTextHarfBuzz* render_text,
                                     const Range& char_range) const;

  // Returns the glyph width for the given character range. |char_range| is in
  // text-space (0 corresponds to |GetDisplayText()[0]|).
  SkScalar GetGlyphWidthForCharRange(const Range& char_range) const;

  // Font parameters that may be common to multiple text runs within a text run
  // list.
  struct GFX_EXPORT FontParams {
    // The default constructor for Font is expensive, so always require that a
    // Font be provided.
    explicit FontParams(const Font& template_font);
    ~FontParams();
    FontParams(const FontParams& other);
    FontParams& operator=(const FontParams& other);
    bool operator==(const FontParams& other) const;

    // Populates |render_params|, |font_size| and |baseline_offset| based on
    // |font|.
    void ComputeRenderParamsFontSizeAndBaselineOffset();

    // Populates |font|, |skia_face|, and |render_params|. Returns false if
    // |skia_face| is nullptr. Takes |font|'s family name and rematches this
    // family and the run's weight and style properties to find a new font.
    bool SetRenderParamsRematchFont(const Font& font,
                                    const FontRenderParams& render_params);

    // Populates |font|, |skia_face|, and |render_params|. Returns false if
    // |skia_face| is nullptr. Does not perform rematching but extracts an
    // SkTypeface from the underlying PlatformFont of font. Use this method when
    // configuring the |TextRunHarfBuzz| for shaping with fallback fonts, where
    // it is important to keep the underlying font handle of platform font and
    // not perform rematching as in |SetRenderParamsRematchFont|.
    bool SetRenderParamsOverrideSkiaFaceFromFont(
        const Font& font,
        const FontRenderParams& render_params);

    struct Hash {
      size_t operator()(const FontParams& key) const;
    };

    Font font;
    sk_sp<SkTypeface> skia_face;
    FontRenderParams render_params;
    Font::Weight weight = Font::Weight::NORMAL;
    cc::PaintFlags::Style fill_style = cc::PaintFlags::kFill_Style;
    SkScalar stroke_width = 0.0f;
    int font_size = 0;
    int baseline_offset = 0;
    BaselineStyle baseline_type = BaselineStyle::kNormalBaseline;
    bool italic = false;
    bool strike = false;
    bool underline = false;
    bool heavy_underline = false;
    bool is_rtl = false;
    UBiDiLevel level = 0;
    UScriptCode script = USCRIPT_INVALID_CODE;
  };

  // Parameters that are set by ShapeRunWithFont.
  struct GFX_EXPORT ShapeOutput {
    ShapeOutput();
    ~ShapeOutput();
    ShapeOutput(const ShapeOutput& other);
    ShapeOutput& operator=(const ShapeOutput& other);
    ShapeOutput(ShapeOutput&& other);
    ShapeOutput& operator=(ShapeOutput&& other);

    float width = 0.0;
    std::vector<uint16_t> glyphs;
    std::vector<SkPoint> positions;
    // Note that in the context of TextRunHarfBuzz, |glyph_to_char| is indexed
    // based off of the full string (so it is in the same domain as
    // TextRunHarfBuzz::range).
    std::vector<uint32_t> glyph_to_char;
    size_t glyph_count = 0;
    size_t missing_glyph_count = std::numeric_limits<size_t>::max();
  };

  // If |new_shape.missing_glyph_count| is less than that of |shape|, set
  // |font_params| and |shape| to the specified values.
  void UpdateFontParamsAndShape(const FontParams& new_font_params,
                                const ShapeOutput& new_shape);

  Range range;
  FontParams font_params;
  ShapeOutput shape;
  float preceding_run_widths = 0.0;
};

// Manages the list of TextRunHarfBuzz and its logical <-> visual index mapping.
class TextRunList {
 public:
  TextRunList();

  TextRunList(const TextRunList&) = delete;
  TextRunList& operator=(const TextRunList&) = delete;

  ~TextRunList();

  size_t size() const { return runs_.size(); }

  // Converts the index between logical and visual index.
  size_t visual_to_logical(size_t index) const {
    return visual_to_logical_[index];
  }
  size_t logical_to_visual(size_t index) const {
    return logical_to_visual_[index];
  }

  const std::vector<std::unique_ptr<TextRunHarfBuzz>>& runs() const {
    return runs_;
  }

  // Adds the new |run| to the run list.
  void Add(std::unique_ptr<TextRunHarfBuzz> run) {
    runs_.push_back(std::move(run));
  }

  // Reset the run list.
  void Reset();

  // Initialize the index mapping.
  void InitIndexMap();

  // Precomputes the offsets for all runs.
  void ComputePrecedingRunWidths();

  // Get the total width of runs, as if they were shown on one line.
  // Do not use this when multiline is enabled.
  float width() const { return width_; }

  // Get the run index applicable to |position| (at or preceeding |position|).
  size_t GetRunIndexAt(size_t position) const;

  // Returns true if any of the runs in the list have a missing glyph.
  bool HasMissingGlyphs() const { return MissingGlyphCount() > 0; }

  // Returns the count of all missing glyphs across all runs.
  size_t MissingGlyphCount() const {
    size_t count = 0;
    for (auto& run : runs_) {
      count += run->shape.missing_glyph_count;
    }
    return count;
  }

 private:
  // Text runs in logical order.
  std::vector<std::unique_ptr<TextRunHarfBuzz>> runs_;

  // Maps visual run indices to logical run indices and vice versa.
  std::vector<int32_t> visual_to_logical_;
  std::vector<int32_t> logical_to_visual_;

  float width_;
};

}  // namespace internal

class GFX_EXPORT RenderTextHarfBuzz : public RenderText {
 public:
  RenderTextHarfBuzz();

  RenderTextHarfBuzz(const RenderTextHarfBuzz&) = delete;
  RenderTextHarfBuzz& operator=(const RenderTextHarfBuzz&) = delete;

  ~RenderTextHarfBuzz() override;

  // RenderText:
  const std::u16string& GetDisplayText() override;
  SizeF GetStringSizeF() override;
  SizeF GetLineSizeF(const SelectionModel& caret) override;
  std::vector<Rect> GetSubstringBounds(const Range& range) override;
  RangeF GetCursorSpan(const Range& text_range) override;
  size_t GetLineContainingCaret(const SelectionModel& caret) override;

 protected:
  // RenderText:
  SelectionModel AdjacentCharSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) override;
  SelectionModel AdjacentWordSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) override;
  SelectionModel AdjacentLineSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) override;
  void OnLayoutTextAttributeChanged() override;
  void OnDisplayTextAttributeChanged() override;
  void EnsureLayout() override;
  void DrawVisualText(internal::SkiaTextRenderer* renderer,
                      const std::vector<Range>& selections) override;

 private:
  friend class test::RenderTextTestApi;
  friend class RenderTextTest;

  // Return the run index that contains the argument; or the length of the
  // |runs_| vector if argument exceeds the text length or width.
  size_t GetRunContainingCaret(const SelectionModel& caret);

  // Given a |run|, returns the SelectionModel that contains the logical first
  // or last caret position inside (not at a boundary of) the run.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel FirstSelectionModelInsideRun(
      const internal::TextRunHarfBuzz* run);
  SelectionModel LastSelectionModelInsideRun(
      const internal::TextRunHarfBuzz* run);

  using CommonizedRunsMap =
      std::unordered_map<internal::TextRunHarfBuzz::FontParams,
                         std::vector<internal::TextRunHarfBuzz*>,
                         internal::TextRunHarfBuzz::FontParams::Hash>;

  // Break the text into logical runs in |out_run_list|. Populate
  // |out_commonized_run_map| such that each run is present in the vector
  // corresponding to its FontParams.
  void ItemizeTextToRuns(const std::u16string& string,
                         internal::TextRunList* out_run_list,
                         CommonizedRunsMap* out_commonized_run_map);

  // Shape the glyphs needed for each run in |runs| within |text|. This method
  // will apply a number of fonts to |base_font_params| and assign to each
  // run's FontParams and ShapeOutput the parameters and resulting shape that
  // had the smallest number of missing glyphs. Returns true if there are no
  // missing glyphs.
  bool ShapeRuns(const std::u16string& text,
                 const internal::TextRunHarfBuzz::FontParams& base_font_params,
                 std::vector<internal::TextRunHarfBuzz*> runs);

  // Shape the glyphs for |in_out_runs| within |text| using the parameters
  // specified by |font_params|. If, for any run in |*in_out_runs|, the
  // resulting shaping has fewer missing glyphs than the existing shape, then
  // write |font_params| and the resulting ShapeOutput to that run. Remove all
  // runs with no missing glyphs from |in_out_runs| (the caller, ShapeRuns, will
  // terminate when no runs with missing glyphs remain). Runs that were shaped
  // during this function call will be returned in |sucessfully_shaped_runs| if
  // a vector is passed in for that parameter.
  void ShapeRunsWithFont(
      const std::u16string& text,
      const internal::TextRunHarfBuzz::FontParams& font_params,
      std::vector<internal::TextRunHarfBuzz*>* in_out_runs,
      std::vector<internal::TextRunHarfBuzz*>* sucessfully_shaped_runs =
          nullptr);

  // Creates and applies a BreakList based on the resolved fonts of each run in
  // |run_list|. This has the side-effect of isolating missing glyphs into their
  // own run, maximizing fallback opportunities. Returns a bool indicating
  // whether the internal missing glyph BreakList was modified, indicating that
  // reshaping is necessary.
  bool BuildResolvedTypefaceBreakList(internal::TextRunList* run_list);

  // Itemize |text| into runs in |out_run_list|, shape the runs, and populate
  // |out_run_list|'s visual <-> logical maps.
  void ItemizeAndShapeText(const std::u16string& text,
                           internal::TextRunList* out_run_list);

  // Helper method to reduce code duplication in |ItemizeAndShapeText|. Returns
  // true if all text is rendered successfully (no missing glyphs are present).
  bool ItemizeAndShapeTextImpl(CommonizedRunsMap* commonized_run_map,
                               const std::u16string& text,
                               internal::TextRunList* run_list);

  // Makes sure that text runs for layout text are shaped.
  void EnsureLayoutRunList();

  // Returns whether the display range is still a valid range after the eliding
  // pass.
  bool IsValidDisplayRange(Range display_range);

  // Store the fallback font mechanism used for shaping (see ShapeRuns(...)).
  void RecordShapeRunsFallback(internal::ShapeRunFallback fallback);

  // Record the fallback font mechanism used for shaping to UMA (see
  // ShapeRuns(...)). This is done separately from RecordShapeRunsFallback, as
  // multiple passes may be involved and we only want to log the final pass.
  void EmitShapeRunsFallback();

  // RenderText:
  internal::TextRunList* GetRunList() override;
  const internal::TextRunList* GetRunList() const override;
  void GetDecoratedTextForRange(const Range& range,
                                DecoratedText* decorated_text) override;

  // Text run list for |layout_text_| and |display_text_|.
  // |display_run_list_| is created only when the text is elided.
  internal::TextRunList layout_run_list_;
  std::unique_ptr<internal::TextRunList> display_run_list_;

  bool update_layout_run_list_ : 1;
  bool update_display_run_list_ : 1;
  bool update_display_text_ : 1;

  // The device scale factor for which the text was laid out.
  float device_scale_factor_ = 1.0f;

  // The total size of the layouted text.
  SizeF total_size_;

  // The process application locale used to configure text rendering.
  std::string locale_;

  // The fallback result of the most recent call to ShapeRuns.
  std::optional<internal::ShapeRunFallback> last_shape_run_metric_;
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_HARFBUZZ_H_
