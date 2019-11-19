// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_harfbuzz.h"

#include <limits>
#include <set>

#include "base/command_line.h"
#include "base/containers/mru_cache.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkFontMetrics.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/bidi_line_iterator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/decorated_text.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/harfbuzz_font_skia.h"
#include "ui/gfx/platform_font.h"
#include "ui/gfx/range/range_f.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/utf16_indexing.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/locale_utils.h"
#endif  // defined(OS_ANDROID)

#include <hb.h>

namespace gfx {

namespace {

// Text length limit. Longer strings are slow and not fully tested.
const size_t kMaxTextLength = 10000;

// The maximum number of scripts a Unicode character can belong to. This value
// is arbitrarily chosen to be a good limit because it is unlikely for a single
// character to belong to more scripts.
const size_t kMaxScripts = 32;

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

// Log the fallback font mechanism used for shaping to UMA (see ShapeRuns(...)).
void RecordShapeRunsFallback(ShapeRunFallback fallback) {
  UMA_HISTOGRAM_ENUMERATION("RenderTextHarfBuzz.ShapeRunsFallback", fallback);
}

// Returns whether the codepoint has the 'extended pictographic' property.
bool IsExtendedPictographicCodepoint(UChar32 codepoint) {
  return u_hasBinaryProperty(codepoint, UCHAR_EXTENDED_PICTOGRAPHIC);
}

// Returns whether the codepoint has emoji properties.
bool IsEmojiRelatedCodepoint(UChar32 codepoint) {
  return u_hasBinaryProperty(codepoint, UCHAR_EMOJI) ||
         u_hasBinaryProperty(codepoint, UCHAR_EMOJI_PRESENTATION) ||
         u_hasBinaryProperty(codepoint, UCHAR_REGIONAL_INDICATOR);
}

// Returns true if |codepoint| is a bracket. This is used to avoid "matching"
// brackets picking different font fallbacks, thereby appearing mismatched.
bool IsBracket(UChar32 codepoint) {
  return u_getIntPropertyValue(codepoint, UCHAR_BIDI_PAIRED_BRACKET_TYPE) !=
         U_BPT_NONE;
}

// Writes the script and the script extensions of the Unicode |codepoint|.
// Returns the number of written scripts.
size_t GetScriptExtensions(UChar32 codepoint, UScriptCode* scripts) {
  // Fill |scripts| with the script extensions.
  UErrorCode icu_error = U_ZERO_ERROR;
  size_t count =
      uscript_getScriptExtensions(codepoint, scripts, kMaxScripts, &icu_error);
  DCHECK_NE(icu_error, U_BUFFER_OVERFLOW_ERROR) << " #ext: " << count;
  if (U_FAILURE(icu_error))
    return 0;

  return count;
}

// Intersects the script extensions set of |codepoint| with |result| and writes
// to |result|, reading and updating |result_size|. The output |result| will be
// a subset of the input |result| (thus |result_size| can only be smaller).
void ScriptSetIntersect(UChar32 codepoint,
                        UScriptCode* result,
                        size_t* result_size) {
  // Each codepoint has a Script property and a Script Extensions (Scx)
  // property.
  //
  // The implicit Script property values 'Common' and 'Inherited' indicate that
  // a codepoint is widely used in many scripts, rather than being associated
  // to a specific script.
  //
  // However, some codepoints that are assigned a value of 'Common' or
  // 'Inherited' are not commonly used with all scripts, but rather only with a
  // limited set of scripts. The Script Extension property is used to specify
  // the set of script which borrow the codepoint.
  //
  // Calls to GetScriptExtensions(...) return the set of scripts where the
  // codepoints can be used.
  // (see table 7 from http://www.unicode.org/reports/tr24/tr24-29.html)
  //
  //     Script     Script Extensions   ->  Results
  //  1) Common       {Common}          ->  {Common}
  //     Inherited    {Inherited}       ->  {Inherited}
  //  2) Latin        {Latn}            ->  {Latn}
  //     Inherited    {Latn}            ->  {Latn}
  //  3) Common       {Hira Kana}       ->  {Hira Kana}
  //     Inherited    {Hira Kana}       ->  {Hira Kana}
  //  4) Devanagari   {Deva Dogr Kthi Mahj}  ->  {Deva Dogr Kthi Mahj}
  //     Myanmar      {Cakm Mymr Tale}  ->  {Cakm Mymr Tale}
  //
  // For most of the codepoints, the script extensions set contains only one
  // element. For CJK codepoints, it's common to see 3-4 scripts. For really
  // rare cases, the set can go above 20 scripts.
  UScriptCode scripts[kMaxScripts] = { USCRIPT_INVALID_CODE };
  size_t count = GetScriptExtensions(codepoint, scripts);

  // Implicit script 'inherited' is inheriting scripts from preceding codepoint.
  if (count == 1 && scripts[0] == USCRIPT_INHERITED)
    return;

  // Perform the intersection of both script set.
  auto scripts_span = base::span<UScriptCode>(scripts, count);
  DCHECK(!base::Contains(scripts_span, USCRIPT_INHERITED));
  auto results_span = base::span<UScriptCode>(result, *result_size);

  size_t out_size = 0;
  for (UScriptCode current : results_span) {
    if (base::Contains(scripts_span, current))
      result[out_size++] = current;
  }

  *result_size = out_size;
}

struct GraphemeProperties {
  bool has_control = false;
  bool has_bracket = false;
  bool has_pictographic = false;
  bool has_emoji = false;
  UBlockCode block = UBLOCK_NO_BLOCK;
};

// Returns the properties for the codepoints part of the given text.
GraphemeProperties RetrieveGraphemeProperties(const base::StringPiece16& text,
                                              bool retrieve_block) {
  GraphemeProperties properties;
  bool first_char = true;
  base::i18n::UTF16CharIterator iter(text.data(), text.length());
  while (!iter.end()) {
    const UChar32 codepoint = iter.get();

    if (first_char) {
      first_char = false;
      if (retrieve_block)
        properties.block = ublock_getCode(codepoint);
    }

    if (codepoint == '\n' || codepoint == ' ')
      properties.has_control = true;
    if (IsBracket(codepoint))
      properties.has_bracket = true;
    if (IsExtendedPictographicCodepoint(codepoint))
      properties.has_pictographic = true;
    if (IsEmojiRelatedCodepoint(codepoint))
      properties.has_emoji = true;

    iter.Advance();
  }

  return properties;
}

// Return whether the grapheme properties are compatible and the grapheme can
// be merge together in the same grapheme cluster.
bool AreGraphemePropertiesCompatible(const GraphemeProperties& first,
                                     const GraphemeProperties& second) {
  // There are 5 constrains to grapheme to be compatible.
  // 1) The newline character and control characters should form a single run so
  //  that the line breaker can handle them easily.
  // 2) Parentheses should be put in a separate run to avoid using different
  // fonts while rendering matching parentheses (see http://crbug.com/396776).
  // 3) Pictographic graphemes should be put in separate run to avoid altering
  // fonts selection while rendering adjacent text (see
  // http://crbug.com/278913).
  // 4) Emoji graphemes should be put in separate run (see
  // http://crbug.com/530021 and http://crbug.com/533721).
  // 5) The 'COMMON' script needs to be split by unicode block. Codepoints are
  // spread across blocks and supported with different fonts.
  return !first.has_control && !second.has_control &&
         first.has_bracket == second.has_bracket &&
         first.has_pictographic == second.has_pictographic &&
         first.has_emoji == second.has_emoji && first.block == second.block;
}

// Returns the end of the current grapheme cluster. This function is finding the
// breaking point where grapheme properties are no longer compatible.
// (see: UNICODE TEXT SEGMENTATION (http://unicode.org/reports/tr29/)
size_t FindRunBreakingCharacter(const base::string16& text,
                                UScriptCode script,
                                size_t run_start,
                                size_t run_break) {
  const size_t run_length = run_break - run_start;
  const base::StringPiece16 run_text(text.c_str() + run_start, run_length);
  const bool is_common_script = (script == USCRIPT_COMMON);

  DCHECK(!run_text.empty());

  // Create an iterator to split the text in graphemes.
  base::i18n::BreakIterator grapheme_iterator(
      run_text, base::i18n::BreakIterator::BREAK_CHARACTER);
  if (!grapheme_iterator.Init() || !grapheme_iterator.Advance()) {
    // In case of error, isolate the first character in a separate run.
    NOTREACHED();
    return run_start + 1;
  }

  // Retrieve the first grapheme and its codepoint properties.
  const base::StringPiece16 first_grapheme_text =
      grapheme_iterator.GetStringPiece();
  const GraphemeProperties first_grapheme_properties =
      RetrieveGraphemeProperties(first_grapheme_text, is_common_script);

  // Append subsequent graphemes in this grapheme cluster if they are
  // compatible, otherwise break the current run.
  while (grapheme_iterator.Advance()) {
    const base::StringPiece16 current_grapheme_text =
        grapheme_iterator.GetStringPiece();
    const GraphemeProperties current_grapheme_properties =
        RetrieveGraphemeProperties(current_grapheme_text, is_common_script);

    if (!AreGraphemePropertiesCompatible(first_grapheme_properties,
                                         current_grapheme_properties)) {
      const size_t current_breaking_position =
          run_start + grapheme_iterator.prev();
      return current_breaking_position;
    }
  }

  // Do not break this run, returns end of the text.
  return run_break;
}

// Find the longest sequence of characters from 0 and up to |length| that have
// at least one common UScriptCode value. Writes the common script value to
// |script| and returns the length of the sequence. Takes the characters' script
// extensions into account. http://www.unicode.org/reports/tr24/#ScriptX
//
// Consider 3 characters with the script values {Kana}, {Hira, Kana}, {Kana}.
// Without script extensions only the first script in each set would be taken
// into account, resulting in 3 runs where 1 would be enough.
size_t ScriptInterval(const base::string16& text,
                      size_t start,
                      size_t length,
                      UScriptCode* script) {
  DCHECK_GT(length, 0U);

  UScriptCode scripts[kMaxScripts] = { USCRIPT_INVALID_CODE };

  base::i18n::UTF16CharIterator char_iterator(text.c_str() + start, length);
  size_t scripts_size = GetScriptExtensions(char_iterator.get(), scripts);
  *script = scripts[0];

  while (char_iterator.Advance()) {
    ScriptSetIntersect(char_iterator.get(), scripts, &scripts_size);
    if (scripts_size == 0U)
      return char_iterator.array_pos();
    *script = scripts[0];
  }

  return length;
}

// A port of hb_icu_script_to_script because harfbuzz on CrOS is built without
// hb-icu. See http://crbug.com/356929
inline hb_script_t ICUScriptToHBScript(UScriptCode script) {
  if (script == USCRIPT_INVALID_CODE)
    return HB_SCRIPT_INVALID;
  return hb_script_from_string(uscript_getShortName(script), -1);
}

// Whether |segment| corresponds to the newline character.
bool IsNewlineSegment(const base::string16& text,
                      const internal::LineSegment& segment) {
  return text[segment.char_range.start()] == '\n';
}

// Returns the line index considering the newline character. Line index is
// incremented if the caret is right after the newline character, i.e, the
// cursor affinity is |CURSOR_BACKWARD| while containing the newline character.
size_t LineIndexForNewline(const size_t line_index,
                           const base::string16& text,
                           const internal::LineSegment& segment,
                           const SelectionModel& caret) {
  bool at_newline = IsNewlineSegment(text, segment) &&
                    caret.caret_affinity() == CURSOR_BACKWARD;
  return line_index + (at_newline ? 1 : 0);
}

// Helper template function for |TextRunHarfBuzz::GetClusterAt()|. |Iterator|
// can be a forward or reverse iterator type depending on the text direction.
// Returns true on success, or false if an error is encountered.
template <class Iterator>
bool GetClusterAtImpl(size_t pos,
                      Range range,
                      Iterator elements_begin,
                      Iterator elements_end,
                      bool reversed,
                      Range* chars,
                      Range* glyphs) {
  Iterator element = std::upper_bound(elements_begin, elements_end, pos);
  if (element == elements_begin) {
    *chars = range;
    *glyphs = Range();
    return false;
  }

  chars->set_end(element == elements_end ? range.end() : *element);
  glyphs->set_end(reversed ? elements_end - element : element - elements_begin);
  while (--element != elements_begin && *element == *(element - 1));
  chars->set_start(*element);
  glyphs->set_start(
      reversed ? elements_end - element : element - elements_begin);
  if (reversed)
    *glyphs = Range(glyphs->end(), glyphs->start());

  DCHECK(!chars->is_reversed());
  DCHECK(!chars->is_empty());
  DCHECK(!glyphs->is_reversed());
  DCHECK(!glyphs->is_empty());
  return true;
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

// Internal class to generate Line structures. If |multiline| is true, the text
// is broken into lines at |words| boundaries such that each line is no longer
// than |max_width|. If |multiline| is false, only outputs a single Line from
// the given runs. |min_baseline| and |min_height| are the minimum baseline and
// height for each line.
// TODO(ckocagil): Expose the interface of this class in the header and test
//                 this class directly.
class HarfBuzzLineBreaker {
 public:
  HarfBuzzLineBreaker(size_t max_width,
                      int min_baseline,
                      float min_height,
                      WordWrapBehavior word_wrap_behavior,
                      const base::string16& text,
                      const BreakList<size_t>* words,
                      const internal::TextRunList& run_list)
      : max_width_((max_width == 0) ? SK_ScalarMax : SkIntToScalar(max_width)),
        min_baseline_(min_baseline),
        min_height_(min_height),
        word_wrap_behavior_(word_wrap_behavior),
        text_(text),
        words_(words),
        run_list_(run_list),
        max_descent_(0),
        max_ascent_(0),
        text_x_(0),
        available_width_(max_width_) {
    AdvanceLine();
  }

  // Constructs a single line for |text_| using |run_list_|.
  void ConstructSingleLine() {
    for (size_t i = 0; i < run_list_.size(); i++) {
      const internal::TextRunHarfBuzz& run = *(run_list_.runs()[i]);
      internal::LineSegment segment;
      segment.run = i;
      segment.char_range = run.range;
      segment.x_range = RangeF(SkScalarToFloat(text_x_),
                               SkScalarToFloat(text_x_) + run.shape.width);
      AddLineSegment(segment, false);
    }
  }

  // Constructs multiple lines for |text_| based on words iteration approach.
  void ConstructMultiLines() {
    DCHECK(words_);
    for (auto iter = words_->breaks().begin(); iter != words_->breaks().end();
         iter++) {
      const Range word_range = words_->GetRange(iter);
      std::vector<internal::LineSegment> word_segments;
      SkScalar word_width = GetWordWidth(word_range, &word_segments);

      // If the last word is '\n', we should advance a new line after adding
      // the word to the current line.
      bool new_line = false;
      if (!word_segments.empty() &&
          IsNewlineSegment(text_, word_segments.back())) {
        new_line = true;

        // Subtract the width of newline segments, they are not drawn.
        if (word_segments.size() != 1u || available_width_ != max_width_)
          word_width -= word_segments.back().width();
      }

      // If the word is not the first word in the line and it can't fit into
      // the current line, advance a new line.
      if (word_width > available_width_ && available_width_ != max_width_)
        AdvanceLine();
      if (!word_segments.empty())
        AddWordToLine(word_segments);
      if (new_line)
        AdvanceLine();
    }
  }

  // Finishes line breaking and outputs the results. Can be called at most once.
  void FinalizeLines(std::vector<internal::Line>* lines, SizeF* size) {
    DCHECK(!lines_.empty());
    // Add an empty line to finish the line size calculation and remove it.
    AdvanceLine();
    lines_.pop_back();
    *size = total_size_;
    lines->swap(lines_);
  }

 private:
  // A (line index, segment index) pair that specifies a segment in |lines_|.
  typedef std::pair<size_t, size_t> SegmentHandle;

  internal::LineSegment* SegmentFromHandle(const SegmentHandle& handle) {
    return &lines_[handle.first].segments[handle.second];
  }

  // Finishes the size calculations of the last Line in |lines_|. Adds a new
  // Line to the back of |lines_|.
  void AdvanceLine() {
    if (!lines_.empty()) {
      internal::Line* line = &lines_.back();
      std::sort(line->segments.begin(), line->segments.end(),
                [this](const internal::LineSegment& s1,
                       const internal::LineSegment& s2) -> bool {
                  return run_list_.logical_to_visual(s1.run) <
                         run_list_.logical_to_visual(s2.run);
                });
      line->size.set_height(std::max(min_height_, max_descent_ + max_ascent_));
      line->baseline = std::max(min_baseline_, SkScalarRoundToInt(max_ascent_));
      line->preceding_heights = std::ceil(total_size_.height());
      // Subtract newline segment's width from |total_size_| because it's not
      // drawn.
      float line_width = line->size.width();
      if (!line->segments.empty() &&
          IsNewlineSegment(text_, line->segments.back())) {
        line_width -= line->segments.back().width();
      }
      if (line->segments.size() > 1 &&
          IsNewlineSegment(text_, line->segments.front())) {
        line_width -= line->segments.front().width();
      }
      total_size_.set_height(total_size_.height() + line->size.height());
      total_size_.set_width(std::max(total_size_.width(), line_width));
    }
    max_descent_ = 0;
    max_ascent_ = 0;
    available_width_ = max_width_;
    lines_.push_back(internal::Line());
  }

  // Adds word to the current line. A word may contain multiple segments. If the
  // word is the first word in line and its width exceeds |available_width_|,
  // ignore/truncate/wrap it according to |word_wrap_behavior_|.
  void AddWordToLine(const std::vector<internal::LineSegment>& word_segments) {
    DCHECK(!lines_.empty());
    DCHECK(!word_segments.empty());

    bool has_truncated = false;
    for (const internal::LineSegment& segment : word_segments) {
      if (has_truncated)
        break;

      if (IsNewlineSegment(text_, segment) ||
          segment.width() <= available_width_ ||
          word_wrap_behavior_ == IGNORE_LONG_WORDS) {
        AddLineSegment(segment, true);
      } else {
        DCHECK(word_wrap_behavior_ == TRUNCATE_LONG_WORDS ||
               word_wrap_behavior_ == WRAP_LONG_WORDS);
        has_truncated = (word_wrap_behavior_ == TRUNCATE_LONG_WORDS);

        const internal::TextRunHarfBuzz& run = *(run_list_.runs()[segment.run]);
        internal::LineSegment remaining_segment = segment;
        while (!remaining_segment.char_range.is_empty()) {
          size_t cutoff_pos = GetCutoffPos(remaining_segment);
          SkScalar width = run.GetGlyphWidthForCharRange(
              Range(remaining_segment.char_range.start(), cutoff_pos));
          if (width > 0) {
            internal::LineSegment cut_segment;
            cut_segment.run = remaining_segment.run;
            cut_segment.char_range =
                Range(remaining_segment.char_range.start(), cutoff_pos);
            cut_segment.x_range = RangeF(SkScalarToFloat(text_x_),
                                         SkScalarToFloat(text_x_ + width));
            AddLineSegment(cut_segment, true);
            // Updates old segment range.
            remaining_segment.char_range.set_start(cutoff_pos);
            remaining_segment.x_range.set_start(SkScalarToFloat(text_x_));
          }
          if (has_truncated)
            break;
          if (!remaining_segment.char_range.is_empty())
            AdvanceLine();
        }
      }
    }
  }

  // Add a line segment to the current line. Note that, in order to keep the
  // visual order correct for ltr and rtl language, we need to merge segments
  // that belong to the same run.
  void AddLineSegment(const internal::LineSegment& segment, bool multiline) {
    DCHECK(!lines_.empty());
    internal::Line* line = &lines_.back();
    const internal::TextRunHarfBuzz& run = *(run_list_.runs()[segment.run]);
    if (!line->segments.empty()) {
      internal::LineSegment& last_segment = line->segments.back();
      // Merge segments that belong to the same run.
      if (last_segment.run == segment.run) {
        DCHECK_EQ(last_segment.char_range.end(), segment.char_range.start());
        // Check there is less than a pixel between one run and the next.
        DCHECK_LE(
            std::abs(last_segment.x_range.end() - segment.x_range.start()),
            1.0f);
        last_segment.char_range.set_end(segment.char_range.end());
        last_segment.x_range.set_end(SkScalarToFloat(text_x_) +
                                     segment.width());
        if (run.font_params.is_rtl &&
            last_segment.char_range.end() == run.range.end())
          UpdateRTLSegmentRanges();
        line->size.set_width(line->size.width() + segment.width());
        text_x_ += segment.width();
        available_width_ -= segment.width();
        return;
      }
    }
    line->segments.push_back(segment);
    line->size.set_width(line->size.width() + segment.width());

    // Newline characters are not drawn for multi-line, ignore their metrics.
    if (!multiline || !IsNewlineSegment(text_, segment)) {
      SkFont font(run.font_params.skia_face, run.font_params.font_size);
      font.setEdging(run.font_params.render_params.antialiasing
                         ? SkFont::Edging::kAntiAlias
                         : SkFont::Edging::kAlias);
      SkFontMetrics metrics;
      font.getMetrics(&metrics);

      // max_descent_ is y-down, fDescent is y-down, baseline_offset is y-down
      max_descent_ = std::max(
          max_descent_, metrics.fDescent + run.font_params.baseline_offset);
      // max_ascent_ is y-up, fAscent is y-down, baseline_offset is y-down
      max_ascent_ = std::max(
          max_ascent_, -(metrics.fAscent + run.font_params.baseline_offset));
    }

    if (run.font_params.is_rtl) {
      rtl_segments_.push_back(
          SegmentHandle(lines_.size() - 1, line->segments.size() - 1));
      // If this is the last segment of an RTL run, reprocess the text-space x
      // ranges of all segments from the run.
      if (segment.char_range.end() == run.range.end())
        UpdateRTLSegmentRanges();
    }
    text_x_ += segment.width();
    available_width_ -= segment.width();
  }

  // Finds the end position |end_pos| in |segment| where the preceding width is
  // no larger than |available_width_|.
  size_t GetCutoffPos(const internal::LineSegment& segment) const {
    DCHECK(!segment.char_range.is_empty());
    const internal::TextRunHarfBuzz& run =
        *(run_list_.runs()[segment.run]).get();
    size_t end_pos = segment.char_range.start();
    SkScalar width = 0;
    while (end_pos < segment.char_range.end()) {
      const SkScalar char_width =
          run.GetGlyphWidthForCharRange(Range(end_pos, end_pos + 1));
      if (width + char_width > available_width_)
        break;
      width += char_width;
      end_pos++;
    }

    const size_t valid_end_pos = std::max(
        segment.char_range.start(),
        static_cast<uint32_t>(FindValidBoundaryBefore(text_, end_pos)));
    if (end_pos != valid_end_pos) {
      end_pos = valid_end_pos;
      width = run.GetGlyphWidthForCharRange(
          Range(segment.char_range.start(), end_pos));
    }

    // |max_width_| might be smaller than a single character. In this case we
    // need to put at least one character in the line. Note that, we should
    // not separate surrogate pair or combining characters.
    // See RenderTextHarfBuzzTest.Multiline_MinWidth for an example.
    if (width == 0 && available_width_ == max_width_) {
      end_pos = std::min(
          segment.char_range.end(),
          static_cast<uint32_t>(FindValidBoundaryAfter(text_, end_pos + 1)));
    }

    return end_pos;
  }

  // Gets the glyph width for |word_range|, and splits the |word| into different
  // segments based on its runs.
  SkScalar GetWordWidth(const Range& word_range,
                        std::vector<internal::LineSegment>* segments) const {
    DCHECK(words_);
    if (word_range.is_empty() || segments == nullptr)
      return 0;
    size_t run_start_index = run_list_.GetRunIndexAt(word_range.start());
    size_t run_end_index = run_list_.GetRunIndexAt(word_range.end() - 1);
    SkScalar width = 0;
    for (size_t i = run_start_index; i <= run_end_index; i++) {
      const internal::TextRunHarfBuzz& run = *(run_list_.runs()[i]);
      const Range char_range = run.range.Intersect(word_range);
      DCHECK(!char_range.is_empty());
      const SkScalar char_width = run.GetGlyphWidthForCharRange(char_range);
      width += char_width;

      internal::LineSegment segment;
      segment.run = i;
      segment.char_range = char_range;
      segment.x_range = RangeF(SkScalarToFloat(text_x_ + width - char_width),
                               SkScalarToFloat(text_x_ + width));
      segments->push_back(segment);
    }
    return width;
  }

  // RTL runs are broken in logical order but displayed in visual order. To find
  // the text-space coordinate (where it would fall in a single-line text)
  // |x_range| of RTL segments, segment widths are applied in reverse order.
  // e.g. {[5, 10], [10, 40]} will become {[35, 40], [5, 35]}.
  void UpdateRTLSegmentRanges() {
    if (rtl_segments_.empty())
      return;
    float x = SegmentFromHandle(rtl_segments_[0])->x_range.start();
    for (size_t i = rtl_segments_.size(); i > 0; --i) {
      internal::LineSegment* segment = SegmentFromHandle(rtl_segments_[i - 1]);
      const float segment_width = segment->width();
      segment->x_range = RangeF(x, x + segment_width);
      x += segment_width;
    }
    rtl_segments_.clear();
  }

  const SkScalar max_width_;
  const int min_baseline_;
  const float min_height_;
  const WordWrapBehavior word_wrap_behavior_;
  const base::string16& text_;
  const BreakList<size_t>* const words_;
  const internal::TextRunList& run_list_;

  // Stores the resulting lines.
  std::vector<internal::Line> lines_;

  float max_descent_;
  float max_ascent_;

  // Text space x coordinates of the next segment to be added.
  SkScalar text_x_;
  // Stores available width in the current line.
  SkScalar available_width_;

  // Size of the multiline text, not including the currently processed line.
  SizeF total_size_;

  // The current RTL run segments, to be applied by |UpdateRTLSegmentRanges()|.
  std::vector<SegmentHandle> rtl_segments_;

  DISALLOW_COPY_AND_ASSIGN(HarfBuzzLineBreaker);
};

// Function object for case insensitive string comparison.
struct CaseInsensitiveCompare {
  bool operator() (const Font& a, const Font& b) const {
    return base::CompareCaseInsensitiveASCII(a.GetFontName(), b.GetFontName()) <
           0;
  }
};

// Applies a forced text rendering direction if specified by a command-line
// switch.
void ApplyForcedDirection(UBiDiLevel* level) {
  static bool has_switch = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceTextDirection);
  if (!has_switch)
    return;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceTextDirection)) {
    std::string force_flag =
        command_line->GetSwitchValueASCII(switches::kForceTextDirection);

    if (force_flag == switches::kForceDirectionRTL)
      *level = UBIDI_RTL;
    if (force_flag == switches::kForceDirectionLTR)
      *level = UBIDI_LTR;
  }
}

internal::TextRunHarfBuzz::FontParams CreateFontParams(
    const gfx::Font& primary_font,
    UBiDiLevel bidi_level,
    UScriptCode script,
    const internal::StyleIterator& style) {
  internal::TextRunHarfBuzz::FontParams font_params(primary_font);
  font_params.italic = style.style(TEXT_STYLE_ITALIC);
  font_params.baseline_type = style.baseline();
  font_params.font_size = style.font_size_override();
  font_params.strike = style.style(TEXT_STYLE_STRIKE);
  font_params.underline = style.style(TEXT_STYLE_UNDERLINE);
  font_params.heavy_underline = style.style(TEXT_STYLE_HEAVY_UNDERLINE);
  font_params.weight = style.weight();
  font_params.level = bidi_level;
  font_params.script = script;
  // Odd BiDi embedding levels correspond to RTL runs.
  font_params.is_rtl = (font_params.level % 2) == 1;
  return font_params;
}

}  // namespace

namespace internal {

#if !defined(OS_MACOSX)
sk_sp<SkTypeface> CreateSkiaTypeface(const Font& font,
                                     bool italic,
                                     Font::Weight weight) {
  SkFontStyle skia_style(
      static_cast<int>(weight), SkFontStyle::kNormal_Width,
      italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);
  return sk_sp<SkTypeface>(SkTypeface::MakeFromName(
      font.GetFontName().c_str(), skia_style));
}
#endif

TextRunHarfBuzz::FontParams::FontParams(const Font& template_font)
    : font(template_font) {}
TextRunHarfBuzz::FontParams::~FontParams() = default;
TextRunHarfBuzz::FontParams::FontParams(
    const TextRunHarfBuzz::FontParams& other) = default;
TextRunHarfBuzz::FontParams& TextRunHarfBuzz::FontParams::operator=(
    const TextRunHarfBuzz::FontParams& other) = default;

bool TextRunHarfBuzz::FontParams::operator==(const FontParams& other) const {
  // Empirically, |script| and |weight| are the highest entropy members.
  return script == other.script && weight == other.weight &&
         skia_face == other.skia_face && render_params == other.render_params &&
         font_size == other.font_size &&
         baseline_offset == other.baseline_offset &&
         baseline_type == other.baseline_type && italic == other.italic &&
         strike == other.strike && underline == other.underline &&
         heavy_underline == other.heavy_underline && is_rtl == other.is_rtl &&
         level == other.level;
}

void TextRunHarfBuzz::FontParams::
    ComputeRenderParamsFontSizeAndBaselineOffset() {
  render_params = font.GetFontRenderParams();
  if (font_size == 0)
    font_size = font.GetFontSize();
  baseline_offset = 0;
  if (baseline_type != NORMAL_BASELINE) {
    // Calculate a slightly smaller font. The ratio here is somewhat arbitrary.
    // Proportions from 5/9 to 5/7 all look pretty good.
    const float ratio = 5.0f / 9.0f;
    font_size = ToRoundedInt(font.GetFontSize() * ratio);
    switch (baseline_type) {
      case SUPERSCRIPT:
        baseline_offset = font.GetCapHeight() - font.GetHeight();
        break;
      case SUPERIOR:
        baseline_offset =
            ToRoundedInt(font.GetCapHeight() * ratio) - font.GetCapHeight();
        break;
      case SUBSCRIPT:
        baseline_offset = font.GetHeight() - font.GetBaseline();
        break;
      case INFERIOR:  // Fall through.
      default:
        break;
    }
  }
}

size_t TextRunHarfBuzz::FontParams::Hash::operator()(
    const FontParams& key) const {
  // In practice, |font|, |skia_face|, |render_params|, and |baseline_offset|
  // have not yet been set when this is called.
  return static_cast<size_t>(key.italic) << 0 ^
         static_cast<size_t>(key.strike) << 1 ^
         static_cast<size_t>(key.underline) << 2 ^
         static_cast<size_t>(key.heavy_underline) << 3 ^
         static_cast<size_t>(key.is_rtl) << 4 ^
         static_cast<size_t>(key.weight) << 8 ^
         static_cast<size_t>(key.font_size) << 12 ^
         static_cast<size_t>(key.baseline_type) << 16 ^
         static_cast<size_t>(key.level) << 20 ^
         static_cast<size_t>(key.script) << 24;
}

bool TextRunHarfBuzz::FontParams::SetRenderParamsRematchFont(
    const Font& new_font,
    const FontRenderParams& new_render_params) {
  // This takes the font family name from new_font, and calls
  // SkTypeface::makeFromName() with that family name and the style information
  // internal to this text run. So it triggers a new font match and looks for
  // adjacent fonts in the family. This works for styling, e.g. styling a run in
  // bold, italic or underline, but breaks font fallback in certain scenarios,
  // as the fallback font may be of a different weight and style than the run's
  // own, so this can lead to a failure of instantiating the correct fallback
  // font.
  sk_sp<SkTypeface> new_skia_face(
      internal::CreateSkiaTypeface(new_font, italic, weight));
  if (!new_skia_face)
    return false;

  skia_face = new_skia_face;
  font = new_font;
  render_params = new_render_params;
  return true;
}

bool TextRunHarfBuzz::FontParams::SetRenderParamsOverrideSkiaFaceFromFont(
    const Font& fallback_font,
    const FontRenderParams& new_render_params) {
  PlatformFont* platform_font = fallback_font.platform_font();
  sk_sp<SkTypeface> new_skia_face =
      platform_font->GetNativeSkTypefaceIfAvailable();

  // If pass-through of the Skia native handle fails for PlatformFonts other
  // than PlatformFontSkia, perform rematching.
  if (!new_skia_face)
    return SetRenderParamsRematchFont(fallback_font, new_render_params);

  skia_face = new_skia_face;
  font = fallback_font;
  render_params = new_render_params;
  return true;
}

TextRunHarfBuzz::ShapeOutput::ShapeOutput() = default;
TextRunHarfBuzz::ShapeOutput::~ShapeOutput() = default;
TextRunHarfBuzz::ShapeOutput::ShapeOutput(
    const TextRunHarfBuzz::ShapeOutput& other) = default;
TextRunHarfBuzz::ShapeOutput& TextRunHarfBuzz::ShapeOutput::operator=(
    const TextRunHarfBuzz::ShapeOutput& other) = default;
TextRunHarfBuzz::ShapeOutput::ShapeOutput(
    TextRunHarfBuzz::ShapeOutput&& other) = default;
TextRunHarfBuzz::ShapeOutput& TextRunHarfBuzz::ShapeOutput::operator=(
    TextRunHarfBuzz::ShapeOutput&& other) = default;

TextRunHarfBuzz::TextRunHarfBuzz(const Font& template_font)
    : font_params(template_font) {}

TextRunHarfBuzz::~TextRunHarfBuzz() {}

Range TextRunHarfBuzz::CharRangeToGlyphRange(const Range& char_range) const {
  DCHECK(range.Contains(char_range));
  DCHECK(!char_range.is_reversed());
  DCHECK(!char_range.is_empty());

  Range start_glyphs;
  Range end_glyphs;
  Range temp_range;
  GetClusterAt(char_range.start(), &temp_range, &start_glyphs);
  GetClusterAt(char_range.end() - 1, &temp_range, &end_glyphs);

  return font_params.is_rtl ? Range(end_glyphs.start(), start_glyphs.end())
                            : Range(start_glyphs.start(), end_glyphs.end());
}

size_t TextRunHarfBuzz::CountMissingGlyphs() const {
  return shape.missing_glyph_count;
}

void TextRunHarfBuzz::GetClusterAt(size_t pos,
                                   Range* chars,
                                   Range* glyphs) const {
  DCHECK(chars);
  DCHECK(glyphs);

  bool success = true;
  if (shape.glyph_count == 0 || !range.Contains(Range(pos, pos + 1))) {
    *chars = range;
    *glyphs = Range();
    success = false;
  }

  if (font_params.is_rtl) {
    success &=
        GetClusterAtImpl(pos, range, shape.glyph_to_char.rbegin(),
                         shape.glyph_to_char.rend(), true, chars, glyphs);
  } else {
    success &=
        GetClusterAtImpl(pos, range, shape.glyph_to_char.begin(),
                         shape.glyph_to_char.end(), false, chars, glyphs);
  }

  if (!success) {
    std::string glyph_to_char_string;
    for (size_t i = 0; i < shape.glyph_count && i < shape.glyph_to_char.size();
         ++i) {
      glyph_to_char_string += base::NumberToString(i) + "->" +
                              base::NumberToString(shape.glyph_to_char[i]) +
                              ", ";
    }
    LOG(ERROR) << " TextRunHarfBuzz error, please report at crbug.com/724880:"
               << " range: " << range.ToString()
               << ", rtl: " << font_params.is_rtl << ","
               << " level: '" << font_params.level
               << "', script: " << font_params.script << ","
               << " font: '" << font_params.font.GetActualFontName() << "',"
               << " glyph_count: " << shape.glyph_count << ", pos: " << pos
               << ","
               << " glyph_to_char: " << glyph_to_char_string;
  }
}

RangeF TextRunHarfBuzz::GetGraphemeBounds(RenderTextHarfBuzz* render_text,
                                          size_t text_index) const {
  DCHECK_LT(text_index, range.end());
  if (shape.glyph_count == 0)
    return RangeF(preceding_run_widths, preceding_run_widths + shape.width);

  Range chars;
  Range glyphs;
  GetClusterAt(text_index, &chars, &glyphs);
  const float cluster_begin_x = shape.positions[glyphs.start()].x();
  const float cluster_end_x = glyphs.end() < shape.glyph_count
                                  ? shape.positions[glyphs.end()].x()
                                  : SkFloatToScalar(shape.width);
  DCHECK_LE(cluster_begin_x, cluster_end_x);

  // A cluster consists of a number of code points and corresponds to a number
  // of glyphs that should be drawn together. A cluster can contain multiple
  // graphemes. In order to place the cursor at a grapheme boundary inside the
  // cluster, we simply divide the cluster width by the number of graphemes.
  // Note: The first call to GetGraphemeIterator() can be expensive, so avoid
  // doing it unless it's actually needed (when |code_point_count| > 1).
  ptrdiff_t code_point_count = UTF16IndexToOffset(render_text->GetDisplayText(),
                                                  chars.start(), chars.end());
  if (code_point_count > 1 && render_text->GetGraphemeIterator()) {
    int before = 0;
    int total = 0;
    base::i18n::BreakIterator* grapheme_iterator =
        render_text->GetGraphemeIterator();
    for (size_t i = chars.start(); i < chars.end(); ++i) {
      if (grapheme_iterator->IsGraphemeBoundary(i)) {
        if (i < text_index)
          ++before;
        ++total;
      }
    }
    // With ICU 65.1, DCHECK_GT() below fails.
    // See https://crbug.com/1017047 for more details.
    //
    // DCHECK_GT(total, 0);

    // It's possible for |text_index| to point to a diacritical mark, at the end
    // of |chars|. In this case all the grapheme boundaries come before it. Just
    // provide the bounds of the last grapheme.
    if (before == total)
      --before;

    if (total > 1) {
      if (font_params.is_rtl)
        before = total - before - 1;
      DCHECK_GE(before, 0);
      DCHECK_LT(before, total);
      const float cluster_start = preceding_run_widths + cluster_begin_x;
      const float average_width = (cluster_end_x - cluster_begin_x) / total;
      return RangeF(cluster_start + average_width * before,
                    cluster_start + average_width * (before + 1));
    }
  }

  return RangeF(preceding_run_widths + cluster_begin_x,
                preceding_run_widths + cluster_end_x);
}

RangeF TextRunHarfBuzz::GetGraphemeSpanForCharRange(
    RenderTextHarfBuzz* render_text,
    const Range& char_range) const {
  if (char_range.is_empty())
    return RangeF();

  DCHECK(!char_range.is_reversed());
  DCHECK(range.Contains(char_range));
  size_t left_index = char_range.start();
  size_t right_index =
      UTF16OffsetToIndex(render_text->GetDisplayText(), char_range.end(), -1);
  DCHECK_LE(left_index, right_index);
  if (font_params.is_rtl)
    std::swap(left_index, right_index);

  const RangeF left_span = GetGraphemeBounds(render_text, left_index);
  return left_index == right_index
             ? left_span
             : RangeF(left_span.start(),
                      GetGraphemeBounds(render_text, right_index).end());
}

SkScalar TextRunHarfBuzz::GetGlyphWidthForCharRange(
    const Range& char_range) const {
  if (char_range.is_empty())
    return 0;

  DCHECK(range.Contains(char_range));
  Range glyph_range = CharRangeToGlyphRange(char_range);

  // The |glyph_range| might be empty or invalid on Windows if a multi-character
  // grapheme is divided into different runs (e.g., there are two font sizes or
  // colors for a single glyph). In this case it might cause the browser crash,
  // see crbug.com/526234.
  if (glyph_range.start() >= glyph_range.end()) {
    NOTREACHED() << "The glyph range is empty or invalid! Its char range: ["
        << char_range.start() << ", " << char_range.end()
        << "], and its glyph range: [" << glyph_range.start() << ", "
        << glyph_range.end() << "].";
    return 0;
  }

  return ((glyph_range.end() == shape.glyph_count)
              ? SkFloatToScalar(shape.width)
              : shape.positions[glyph_range.end()].x()) -
         shape.positions[glyph_range.start()].x();
}

void TextRunHarfBuzz::UpdateFontParamsAndShape(
    const FontParams& new_font_params,
    const ShapeOutput& new_shape) {
  if (new_shape.missing_glyph_count < shape.missing_glyph_count) {
    font_params = new_font_params;
    shape = new_shape;
    // Note that |new_shape.glyph_to_char| is indexed from the beginning of
    // |range|, while |shape.glyph_to_char| is indexed from the beginning of
    // its embedding text.
    for (size_t i = 0; i < shape.glyph_to_char.size(); ++i)
      shape.glyph_to_char[i] += range.start();
  }
}

TextRunList::TextRunList() : width_(0.0f) {}

TextRunList::~TextRunList() {}

void TextRunList::Reset() {
  runs_.clear();
  width_ = 0.0f;
}

void TextRunList::InitIndexMap() {
  if (runs_.size() == 1) {
    visual_to_logical_ = logical_to_visual_ = std::vector<int32_t>(1, 0);
    return;
  }
  const size_t num_runs = runs_.size();
  std::vector<UBiDiLevel> levels(num_runs);
  for (size_t i = 0; i < num_runs; ++i)
    levels[i] = runs_[i]->font_params.level;
  visual_to_logical_.resize(num_runs);
  ubidi_reorderVisual(&levels[0], num_runs, &visual_to_logical_[0]);
  logical_to_visual_.resize(num_runs);
  ubidi_reorderLogical(&levels[0], num_runs, &logical_to_visual_[0]);
}

void TextRunList::ComputePrecedingRunWidths() {
  // Precalculate run width information.
  width_ = 0.0f;
  for (size_t i = 0; i < runs_.size(); ++i) {
    const auto& run = runs_[visual_to_logical_[i]];
    run->preceding_run_widths = width_;
    width_ += run->shape.width;
  }
}

size_t TextRunList::GetRunIndexAt(size_t position) const {
  for (size_t i = 0; i < runs_.size(); ++i) {
    if (runs_[i]->range.start() <= position && runs_[i]->range.end() > position)
      return i;
  }
  return runs_.size();
}

namespace {

// ShapeRunWithFont cache. Views makes repeated calls to ShapeRunWithFont
// with the same arguments in several places, and typesetting is very expensive.
// To compensate for this, encapsulate all of the input arguments to
// ShapeRunWithFont in ShapeRunWithFontInput, all of the output arguments in
// TextRunHarfBuzz::ShapeOutput, and add ShapeRunCache to map between the two.
// This is analogous to the blink::ShapeCache.
// https://crbug.com/826265

// Input for the stateless implementation of ShapeRunWithFont.
struct ShapeRunWithFontInput {
  ShapeRunWithFontInput(const base::string16& full_text,
                        const TextRunHarfBuzz::FontParams& font_params,
                        Range full_range,
                        bool obscured,
                        float glyph_width_for_test,
                        int glyph_spacing,
                        bool subpixel_rendering_suppressed)
      : skia_face(font_params.skia_face),
        render_params(font_params.render_params),
        script(font_params.script),
        font_size(font_params.font_size),
        glyph_spacing(glyph_spacing),
        glyph_width_for_test(glyph_width_for_test),
        is_rtl(font_params.is_rtl),
        obscured(obscured),
        subpixel_rendering_suppressed(subpixel_rendering_suppressed) {
    // hb_buffer_add_utf16 will read the previous and next 5 unicode characters
    // (which can have a maximum length of 2 uint16_t) as "context" that is used
    // only for Arabic (which is RTL). Read the previous and next 10 uint16_ts
    // to ensure that we capture all of this context if we're using RTL.
    size_t kContextSize = is_rtl ? 10 : 0;
    size_t context_start = full_range.start() < kContextSize
                               ? 0
                               : full_range.start() - kContextSize;
    size_t context_end =
        std::min(full_text.length(), full_range.end() + kContextSize);
    range = Range(full_range.start() - context_start,
                  full_range.end() - context_start);
    text = full_text.substr(context_start, context_end - context_start);

    // Pre-compute the hash to avoid having to re-hash at every comparison.
    // Attempt to minimize collisions by including the typeface, script, font
    // size, text and the text range.
    hash = base::HashInts(hash, skia_face->uniqueID());
    hash = base::HashInts(hash, script);
    hash = base::HashInts(hash, font_size);
    hash = base::Hash(text);
    hash = base::HashInts(hash, range.start());
    hash = base::HashInts(hash, range.length());
  }

  bool operator==(const ShapeRunWithFontInput& other) const {
    return text == other.text && skia_face == other.skia_face &&
           render_params == other.render_params &&
           font_size == other.font_size && range == other.range &&
           script == other.script && is_rtl == other.is_rtl &&
           obscured == other.obscured &&
           glyph_width_for_test == other.glyph_width_for_test &&
           glyph_spacing == other.glyph_spacing &&
           subpixel_rendering_suppressed == other.subpixel_rendering_suppressed;
  }

  struct Hash {
    size_t operator()(const ShapeRunWithFontInput& key) const {
      return key.hash;
    }
  };

  sk_sp<SkTypeface> skia_face;
  FontRenderParams render_params;
  UScriptCode script;
  int font_size;
  int glyph_spacing;
  float glyph_width_for_test;
  bool is_rtl;
  bool obscured;
  bool subpixel_rendering_suppressed;

  // The parts of the input text that may be read by hb_buffer_add_utf16.
  base::string16 text;
  // The conversion of the input range to a range within |text|.
  Range range;
  // The hash is cached to avoid repeated calls.
  size_t hash = 0;
};

// An MRU cache of the results from calling ShapeRunWithFont. Use the same
// maximum cache size as is used in blink::ShapeCache.
constexpr int kShapeRunCacheSize = 10000;
using ShapeRunCacheBase = base::HashingMRUCache<ShapeRunWithFontInput,
                                                TextRunHarfBuzz::ShapeOutput,
                                                ShapeRunWithFontInput::Hash>;
class ShapeRunCache : public ShapeRunCacheBase {
 public:
  ShapeRunCache() : ShapeRunCacheBase(kShapeRunCacheSize) {}
};

void ShapeRunWithFont(const ShapeRunWithFontInput& in,
                      TextRunHarfBuzz::ShapeOutput* out) {
  TRACE_EVENT0("ui", "RenderTextHarfBuzz::ShapeRunWithFontInternal");

  hb_font_t* harfbuzz_font =
      CreateHarfBuzzFont(in.skia_face, SkIntToScalar(in.font_size),
                         in.render_params, in.subpixel_rendering_suppressed);

  // Create a HarfBuzz buffer and add the string to be shaped. The HarfBuzz
  // buffer holds our text, run information to be used by the shaping engine,
  // and the resulting glyph data.
  hb_buffer_t* buffer = hb_buffer_create();
  // Note that the value of the |item_offset| argument (here specified as
  // |in.range.start()|) does affect the result, so we will have to adjust
  // the computed offsets.
  hb_buffer_add_utf16(
      buffer, reinterpret_cast<const uint16_t*>(in.text.c_str()),
      static_cast<int>(in.text.length()), in.range.start(), in.range.length());
  hb_buffer_set_script(buffer, ICUScriptToHBScript(in.script));
  hb_buffer_set_direction(buffer,
                          in.is_rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
  // TODO(ckocagil): Should we determine the actual language?
  hb_buffer_set_language(buffer, hb_language_get_default());

  // Shape the text.
  hb_shape(harfbuzz_font, buffer, NULL, 0);

  // Populate the run fields with the resulting glyph data in the buffer.
  unsigned int glyph_count = 0;
  hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyph_count);
  out->glyph_count = glyph_count;
  hb_glyph_position_t* hb_positions =
      hb_buffer_get_glyph_positions(buffer, NULL);
  out->glyphs.resize(out->glyph_count);
  out->glyph_to_char.resize(out->glyph_count);
  out->positions.resize(out->glyph_count);
  out->width = 0.0f;

#if defined(OS_MACOSX)
  // Mac 10.9 and 10.10 give a quirky offset for whitespace glyphs in RTL,
  // which requires tests relying on the behavior of |glyph_width_for_test_|
  // to also be given a zero x_offset, otherwise expectations get thrown off.
  const bool force_zero_offset =
      in.glyph_width_for_test > 0 && base::mac::IsAtMostOS10_10();
#else
  constexpr bool force_zero_offset = false;
#endif
  constexpr uint16_t kMissingGlyphId = 0;

  DCHECK(in.obscured || in.glyph_spacing == 0);
  out->missing_glyph_count = 0;
  for (size_t i = 0; i < out->glyph_count; ++i) {
    DCHECK_LE(infos[i].codepoint, std::numeric_limits<uint16_t>::max());
    uint16_t glyph = static_cast<uint16_t>(infos[i].codepoint);
    out->glyphs[i] = glyph;
    if (glyph == kMissingGlyphId)
      out->missing_glyph_count += 1;
    DCHECK_GE(infos[i].cluster, in.range.start());
    out->glyph_to_char[i] = infos[i].cluster - in.range.start();
    const SkScalar x_offset =
        force_zero_offset ? 0
                          : HarfBuzzUnitsToSkiaScalar(hb_positions[i].x_offset);
    const SkScalar y_offset =
        HarfBuzzUnitsToSkiaScalar(hb_positions[i].y_offset);
    out->positions[i].set(out->width + x_offset, -y_offset);
    out->width += (in.glyph_width_for_test > 0)
                      ? in.glyph_width_for_test
                      : HarfBuzzUnitsToFloat(hb_positions[i].x_advance) +
                            in.glyph_spacing;
    // Round run widths if subpixel positioning is off to match native behavior.
    if (!in.render_params.subpixel_positioning)
      out->width = std::round(out->width);
  }

  hb_buffer_destroy(buffer);
  hb_font_destroy(harfbuzz_font);
}

std::string GetApplicationLocale() {
#if defined(OS_ANDROID)
  // TODO(etienneb): Android locale should work the same way than base locale.
  return base::android::GetDefaultLocaleString();
#else
  return base::i18n::GetConfiguredLocale();
#endif
}

}  // namespace

}  // namespace internal

RenderTextHarfBuzz::RenderTextHarfBuzz()
    : RenderText(),
      update_layout_run_list_(false),
      update_display_run_list_(false),
      update_grapheme_iterator_(false),
      update_display_text_(false),
      locale_(internal::GetApplicationLocale()) {
  set_truncate_length(kMaxTextLength);
}

RenderTextHarfBuzz::~RenderTextHarfBuzz() {}

std::unique_ptr<RenderText> RenderTextHarfBuzz::CreateInstanceOfSameType()
    const {
  return std::make_unique<RenderTextHarfBuzz>();
}

bool RenderTextHarfBuzz::MultilineSupported() const {
  return true;
}

const base::string16& RenderTextHarfBuzz::GetDisplayText() {
  // TODO(krb): Consider other elision modes for multiline.
  if ((multiline() && (max_lines() == 0 || elide_behavior() != ELIDE_TAIL)) ||
      elide_behavior() == NO_ELIDE || elide_behavior() == FADE_TAIL) {
    // Call UpdateDisplayText to clear |display_text_| and |text_elided_|
    // on the RenderText class.
    UpdateDisplayText(0);
    update_display_text_ = false;
    display_run_list_.reset();
    return layout_text();
  }

  EnsureLayoutRunList();
  DCHECK(!update_display_text_);
  return text_elided() ? display_text() : layout_text();
}

Size RenderTextHarfBuzz::GetStringSize() {
  const SizeF size_f = GetStringSizeF();
  return Size(std::ceil(size_f.width()), size_f.height());
}

SizeF RenderTextHarfBuzz::GetStringSizeF() {
  EnsureLayout();
  return total_size_;
}

Size RenderTextHarfBuzz::GetLineSize(const SelectionModel& caret) {
  const auto to_size = [](const internal::Line& line) {
    return Size(std::ceil(line.size.width()), line.size.height());
  };
  EnsureLayout();
  const auto& caret_run = GetRunContainingCaret(caret);
  for (const auto& line : lines()) {
    for (const internal::LineSegment& segment : line.segments) {
      if (segment.run == caret_run)
        return to_size(line);
    }
  }

  return to_size(lines().back());
}

float RenderTextHarfBuzz::TotalLineWidth() {
  EnsureLayout();
  float total_width = 0;
  for (const auto& line : lines())
    total_width += line.size.width();
  return total_width;
}

SelectionModel RenderTextHarfBuzz::FindCursorPosition(
    const Point& view_point,
    const Point& drag_origin) {
  EnsureLayout();
  DCHECK(!lines().empty());

  int line_index = GetLineContainingYCoord((view_point - GetLineOffset(0)).y());
  // Handle kDragToEndIfOutsideVerticalBounds above or below the text in a
  // single-line by extending towards the mouse cursor.
  if (RenderText::kDragToEndIfOutsideVerticalBounds && !multiline() &&
      (line_index < 0 || line_index >= static_cast<int>(lines().size()))) {
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
  if (line_index >= static_cast<int>(lines().size())) {
    if (RenderText::kDragToEndIfOutsideVerticalBounds)
      return EdgeSelectionModel(GetVisualDirectionOfLogicalEnd());
    line_index = lines().size() - 1;
  }
  const internal::Line& line = lines()[line_index];
  // Newline segment should be ignored in finding segment index with x
  // coordinate because it's not drawn.
  Vector2d newline_offset;
  if (line.segments.size() > 1 && IsNewlineSegment(line.segments.front()))
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

  // TODO(crbug.com/676287): Use offset within the glyph to return the correct
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

bool RenderTextHarfBuzz::IsSelectionSupported() const {
  return true;
}

std::vector<RenderText::FontSpan> RenderTextHarfBuzz::GetFontSpansForTesting() {
  EnsureLayout();

  internal::TextRunList* run_list = GetRunList();
  std::vector<RenderText::FontSpan> spans;
  for (const auto& run : run_list->runs()) {
    spans.push_back(
        RenderText::FontSpan(run->font_params.font,
                             Range(DisplayIndexToTextIndex(run->range.start()),
                                   DisplayIndexToTextIndex(run->range.end()))));
  }

  return spans;
}

std::vector<Rect> RenderTextHarfBuzz::GetSubstringBounds(const Range& range) {
  EnsureLayout();
  DCHECK(!update_display_run_list_);
  DCHECK(Range(0, text().length()).Contains(range));
  const size_t start =
      IsValidCursorIndex(range.GetMin())
          ? range.GetMin()
          : IndexOfAdjacentGrapheme(range.GetMin(), CURSOR_BACKWARD);
  const size_t end =
      IsValidCursorIndex(range.GetMax())
          ? range.GetMax()
          : IndexOfAdjacentGrapheme(range.GetMax(), CURSOR_FORWARD);
  const Range display_range(TextIndexToDisplayIndex(start),
                            TextIndexToDisplayIndex(end));
  DCHECK(Range(0, GetDisplayText().length()).Contains(display_range));

  std::vector<Rect> rects;
  if (display_range.is_empty())
    return rects;

  internal::TextRunList* run_list = GetRunList();
  for (size_t line_index = 0; line_index < lines().size(); ++line_index) {
    const internal::Line& line = lines()[line_index];
    // Only the last line can be empty.
    DCHECK(!line.segments.empty() || (line_index == lines().size() - 1));
    float line_start_x =
        line.segments.empty()
            ? 0
            : run_list->runs()[line.segments[0].run]->preceding_run_widths;

    if (line.segments.size() > 1 && IsNewlineSegment(line.segments[0]))
      line_start_x += line.segments[0].width();

    for (const internal::LineSegment& segment : line.segments) {
      const Range intersection = segment.char_range.Intersect(display_range);
      DCHECK(!intersection.is_reversed());
      if (!intersection.is_empty()) {
        const internal::TextRunHarfBuzz& run = *run_list->runs()[segment.run];
        RangeF selected_span =
            run.GetGraphemeSpanForCharRange(this, intersection);
        int start_x = std::ceil(selected_span.start() - line_start_x);
        int end_x = std::ceil(selected_span.end() - line_start_x);
        Rect rect(start_x, 0, end_x - start_x, std::ceil(line.size.height()));
        rects.push_back(rect + GetLineOffset(line_index));
      }
    }
  }
  return rects;
}

RangeF RenderTextHarfBuzz::GetCursorSpan(const Range& text_range) {
  DCHECK(!text_range.is_reversed());
  EnsureLayout();
  const size_t index = text_range.start();
  size_t run_index =
      GetRunContainingCaret(SelectionModel(index, CURSOR_FORWARD));
  internal::TextRunList* run_list = GetRunList();

  // Return zero if the text is empty.
  if (run_list->size() == 0 || text().empty())
    return RangeF(0);

  // Use the last run if the index is invalid or beyond the layout text size.
  Range valid_range(text_range.start(), text_range.end());
  if (run_index >= run_list->size()) {
    valid_range = Range(text().length() - 1, text().length());
    run_index = run_list->size() - 1;
  }

  internal::TextRunHarfBuzz* run = run_list->runs()[run_index].get();

  Range display_range(TextIndexToDisplayIndex(valid_range.start()),
                      TextIndexToDisplayIndex(valid_range.end()));

  // Although highly likely, there's no guarantee that a single text run is used
  // for the entire cursor span. For example, Unicode Variation Selectors are
  // incorrectly placed in the next run; see crbug.com/775404. (For these, the
  // variation selector has zero width, so it's safe to ignore the second run).
  // TODO(tapted): Change this to a DCHECK when crbug.com/775404 is fixed.
  display_range = display_range.Intersect(run->range);

  RangeF bounds = run->GetGraphemeSpanForCharRange(this, display_range);
  return run->font_params.is_rtl ? RangeF(bounds.end(), bounds.start())
                                 : bounds;
}

size_t RenderTextHarfBuzz::GetLineContainingCaret(const SelectionModel& caret) {
  EnsureLayout();

  if (caret.caret_pos() == 0)
    return 0;

  size_t layout_position = TextIndexToDisplayIndex(caret.caret_pos());
  LogicalCursorDirection affinity = caret.caret_affinity();
  for (size_t line_index = 0; line_index < lines().size(); ++line_index) {
    const internal::Line& line = lines()[line_index];
    for (const internal::LineSegment& segment : line.segments) {
      if (RangeContainsCaret(segment.char_range, layout_position, affinity))
        return LineIndexForNewline(line_index, text(), segment, caret);
    }
  }

  return lines().size() - 1;
}

base::i18n::BreakIterator* RenderTextHarfBuzz::GetGraphemeIterator() {
  if (update_grapheme_iterator_) {
    update_grapheme_iterator_ = false;
    grapheme_iterator_ = std::make_unique<base::i18n::BreakIterator>(
        GetDisplayText(), base::i18n::BreakIterator::BREAK_CHARACTER);
    if (!grapheme_iterator_->Init())
      grapheme_iterator_.reset();
  }
  return grapheme_iterator_.get();
}

int RenderTextHarfBuzz::GetDisplayTextBaseline() {
  EnsureLayout();
  return lines()[0].baseline;
}

SelectionModel RenderTextHarfBuzz::AdjacentCharSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  DCHECK(!update_display_run_list_);

  internal::TextRunList* run_list = GetRunList();
  internal::TextRunHarfBuzz* run;

  size_t run_index = GetRunContainingCaret(selection);
  if (run_index >= run_list->size()) {
    // The cursor is not in any run: we're at the visual and logical edge.
    SelectionModel edge = EdgeSelectionModel(direction);
    if (edge.caret_pos() == selection.caret_pos())
      return edge;
    int visual_index = (direction == CURSOR_RIGHT) ? 0 : run_list->size() - 1;
    run = run_list->runs()[run_list->visual_to_logical(visual_index)].get();
  } else {
    // If the cursor is moving within the current run, just move it by one
    // grapheme in the appropriate direction.
    run = run_list->runs()[run_index].get();
    size_t caret = selection.caret_pos();
    bool forward_motion = run->font_params.is_rtl == (direction == CURSOR_LEFT);
    if (forward_motion) {
      if (caret < DisplayIndexToTextIndex(run->range.end())) {
        caret = IndexOfAdjacentGrapheme(caret, CURSOR_FORWARD);
        return SelectionModel(caret, CURSOR_BACKWARD);
      }
    } else {
      if (caret > DisplayIndexToTextIndex(run->range.start())) {
        caret = IndexOfAdjacentGrapheme(caret, CURSOR_BACKWARD);
        return SelectionModel(caret, CURSOR_FORWARD);
      }
    }
    // The cursor is at the edge of a run; move to the visually adjacent run.
    int visual_index = run_list->logical_to_visual(run_index);
    visual_index += (direction == CURSOR_LEFT) ? -1 : 1;
    if (visual_index < 0 || visual_index >= static_cast<int>(run_list->size()))
      return EdgeSelectionModel(direction);
    run = run_list->runs()[run_list->visual_to_logical(visual_index)].get();
  }
  bool forward_motion = run->font_params.is_rtl == (direction == CURSOR_LEFT);
  return forward_motion ? FirstSelectionModelInsideRun(run) :
                          LastSelectionModelInsideRun(run);
}

SelectionModel RenderTextHarfBuzz::AdjacentWordSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  if (obscured())
    return EdgeSelectionModel(direction);

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return selection;

  internal::TextRunList* run_list = GetRunList();
  SelectionModel current(selection);
  for (;;) {
    current = AdjacentCharSelectionModel(current, direction);
    size_t run = GetRunContainingCaret(current);
    if (run == run_list->size())
      break;
    size_t cursor = current.caret_pos();
#if defined(OS_WIN)
    // Windows generally advances to the start of a word in either direction.
    // TODO: Break on the end of a word when the neighboring text is
    // punctuation.
    if (iter.IsStartOfWord(cursor))
      break;
#else
    const bool is_forward =
        run_list->runs()[run]->font_params.is_rtl == (direction == CURSOR_LEFT);
    if (is_forward ? iter.IsEndOfWord(cursor) : iter.IsStartOfWord(cursor))
      break;
#endif  // defined(OS_WIN)
  }
  return current;
}

SelectionModel RenderTextHarfBuzz::AdjacentLineSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  DCHECK(direction == CURSOR_UP || direction == CURSOR_DOWN);

  size_t line = GetLineContainingCaret(selection);
  if (line == 0 && direction == CURSOR_UP) {
    reset_cached_cursor_x();
    return SelectionModel(0, CURSOR_BACKWARD);
  }
  if (line == lines().size() - 1 && direction == CURSOR_DOWN) {
    reset_cached_cursor_x();
    return SelectionModel(text().length(), CURSOR_FORWARD);
  }

  direction == CURSOR_UP ? --line : ++line;
  Rect bounds = GetCursorBounds(selection, true);
  Point target = bounds.origin();
  if (cached_cursor_x())
    target.set_x(cached_cursor_x().value());
  else
    set_cached_cursor_x(target.x());
  if (direction == CURSOR_UP)
    target.Offset(0, -bounds.size().height() / 2);
  else
    target.Offset(0, bounds.size().height() * 3 / 2);
  SelectionModel next = FindCursorPosition(target, Point());
  size_t next_line = GetLineContainingCaret(next);

  // If the |target| position is at the newline character, the caret is drawn to
  // the next line. e.g., when the caret is at the beginning of the line in RTL
  // text. Move the caret to the position of the previous character to move the
  // caret to the previous line.
  if (next_line == line + 1)
    next = SelectionModel(next.caret_pos() - 1, next.caret_affinity());

  return next;
}

bool RenderTextHarfBuzz::IsValidCursorIndex(size_t index) {
  if (index == 0 || index == text().length())
    return true;
  if (!IsValidLogicalIndex(index))
    return false;
  base::i18n::BreakIterator* grapheme_iterator = GetGraphemeIterator();
  return !grapheme_iterator || grapheme_iterator->IsGraphemeBoundary(index);
}

void RenderTextHarfBuzz::OnLayoutTextAttributeChanged(bool text_changed) {
  update_layout_run_list_ = true;
  OnDisplayTextAttributeChanged();
}

void RenderTextHarfBuzz::OnDisplayTextAttributeChanged() {
  update_display_text_ = true;
  update_grapheme_iterator_ = true;
}

void RenderTextHarfBuzz::EnsureLayout() {
  EnsureLayoutRunList();

  if (update_display_run_list_) {
    DCHECK(text_elided());
    const base::string16& display_text = GetDisplayText();
    display_run_list_ = std::make_unique<internal::TextRunList>();

    if (!display_text.empty())
      ItemizeAndShapeText(display_text, display_run_list_.get());
    update_display_run_list_ = false;
    std::vector<internal::Line> empty_lines;
    set_lines(&empty_lines);
  }

  if (lines().empty()) {
    internal::TextRunList* run_list = GetRunList();
    const int height = std::max(font_list().GetHeight(), min_line_height());
    HarfBuzzLineBreaker line_breaker(
        display_rect().width(),
        DetermineBaselineCenteringText(height, font_list()), height,
        word_wrap_behavior(), GetDisplayText(),
        multiline() ? &GetLineBreaks() : nullptr, *run_list);

    if (multiline())
      line_breaker.ConstructMultiLines();
    else
      line_breaker.ConstructSingleLine();
    std::vector<internal::Line> lines;
    line_breaker.FinalizeLines(&lines, &total_size_);
    if (multiline() && max_lines()) {
      // TODO(crbug.com/866720): no more than max_lines() should be rendered.
      // Remove the IsHomogeneous() condition for the following DCHECK when the
      // bug is fixed.
      if (IsHomogeneous()) {
        DCHECK_LE(lines.size(), max_lines());
      }
    }
    set_lines(&lines);
  }
}

void RenderTextHarfBuzz::DrawVisualText(internal::SkiaTextRenderer* renderer) {
  DCHECK(!update_layout_run_list_);
  DCHECK(!update_display_run_list_);
  DCHECK(!update_display_text_);
  if (lines().empty())
    return;

  ApplyFadeEffects(renderer);
  ApplyTextShadows(renderer);
  ApplyCompositionAndSelectionStyles();

  internal::TextRunList* run_list = GetRunList();
  const base::string16& display_text = GetDisplayText();
  for (size_t i = 0; i < lines().size(); ++i) {
    const internal::Line& line = lines()[i];
    const Vector2d origin = GetLineOffset(i) + Vector2d(0, line.baseline);
    SkScalar preceding_segment_widths = 0;
    for (const internal::LineSegment& segment : line.segments) {
      // Don't draw the newline glyph (crbug.com/680430).
      if (IsNewlineSegment(display_text, segment))
        continue;

      const internal::TextRunHarfBuzz& run = *run_list->runs()[segment.run];
      renderer->SetTypeface(run.font_params.skia_face);
      renderer->SetTextSize(SkIntToScalar(run.font_params.font_size));
      renderer->SetFontRenderParams(run.font_params.render_params,
                                    subpixel_rendering_suppressed());
      Range glyphs_range = run.CharRangeToGlyphRange(segment.char_range);
      std::vector<SkPoint> positions(glyphs_range.length());
      SkScalar offset_x = preceding_segment_widths -
                          ((glyphs_range.GetMin() != 0)
                               ? run.shape.positions[glyphs_range.GetMin()].x()
                               : 0);
      for (size_t j = 0; j < glyphs_range.length(); ++j) {
        positions[j] = run.shape.positions[(glyphs_range.is_reversed())
                                               ? (glyphs_range.start() - j)
                                               : (glyphs_range.start() + j)];
        positions[j].offset(
            SkIntToScalar(origin.x()) + offset_x,
            SkIntToScalar(origin.y() + run.font_params.baseline_offset));
      }
      for (auto it = colors().GetBreak(segment.char_range.start());
           it != colors().breaks().end() &&
           it->first < segment.char_range.end();
           ++it) {
        const Range intersection =
            colors().GetRange(it).Intersect(segment.char_range);
        const Range colored_glyphs = run.CharRangeToGlyphRange(intersection);
        // The range may be empty if a portion of a multi-character grapheme is
        // selected, yielding two colors for a single glyph. For now, this just
        // paints the glyph with a single style, but it should paint it twice,
        // clipped according to selection bounds. See http://crbug.com/366786
        if (colored_glyphs.is_empty())
          continue;

        renderer->SetForegroundColor(it->second);
        renderer->DrawPosText(
            &positions[colored_glyphs.start() - glyphs_range.start()],
            &run.shape.glyphs[colored_glyphs.start()], colored_glyphs.length());
        int start_x = SkScalarRoundToInt(
            positions[colored_glyphs.start() - glyphs_range.start()].x());
        int end_x = SkScalarRoundToInt(
            (colored_glyphs.end() == glyphs_range.end())
                ? (SkFloatToScalar(segment.width()) + preceding_segment_widths +
                   SkIntToScalar(origin.x()))
                : positions[colored_glyphs.end() - glyphs_range.start()].x());
        if (run.font_params.heavy_underline)
          renderer->DrawUnderline(start_x, origin.y(), end_x - start_x, 2.0);
        else if (run.font_params.underline)
          renderer->DrawUnderline(start_x, origin.y(), end_x - start_x);
        if (run.font_params.strike)
          renderer->DrawStrike(start_x, origin.y(), end_x - start_x,
                               strike_thickness_factor());
      }
      preceding_segment_widths += SkFloatToScalar(segment.width());
    }
  }

  UndoCompositionAndSelectionStyles();
}

size_t RenderTextHarfBuzz::GetRunContainingCaret(
    const SelectionModel& caret) {
  DCHECK(!update_display_run_list_);
  size_t layout_position = TextIndexToDisplayIndex(caret.caret_pos());
  LogicalCursorDirection affinity = caret.caret_affinity();
  internal::TextRunList* run_list = GetRunList();
  for (size_t i = 0; i < run_list->size(); ++i) {
    internal::TextRunHarfBuzz* run = run_list->runs()[i].get();
    if (RangeContainsCaret(run->range, layout_position, affinity))
      return i;
  }
  return run_list->size();
}

SelectionModel RenderTextHarfBuzz::FirstSelectionModelInsideRun(
    const internal::TextRunHarfBuzz* run) {
  size_t position = DisplayIndexToTextIndex(run->range.start());
  position = IndexOfAdjacentGrapheme(position, CURSOR_FORWARD);
  return SelectionModel(position, CURSOR_BACKWARD);
}

SelectionModel RenderTextHarfBuzz::LastSelectionModelInsideRun(
    const internal::TextRunHarfBuzz* run) {
  size_t position = DisplayIndexToTextIndex(run->range.end());
  position = IndexOfAdjacentGrapheme(position, CURSOR_BACKWARD);
  return SelectionModel(position, CURSOR_FORWARD);
}

void RenderTextHarfBuzz::ItemizeAndShapeText(const base::string16& text,
                                             internal::TextRunList* run_list) {
  CommonizedRunsMap commonized_run_map;
  ItemizeTextToRuns(text, run_list, &commonized_run_map);

  for (auto iter = commonized_run_map.begin(); iter != commonized_run_map.end();
       ++iter) {
    internal::TextRunHarfBuzz::FontParams font_params = iter->first;
    font_params.ComputeRenderParamsFontSizeAndBaselineOffset();
    ShapeRuns(text, font_params, std::move(iter->second));
  }

  run_list->InitIndexMap();
  run_list->ComputePrecedingRunWidths();
}

void RenderTextHarfBuzz::ItemizeTextToRuns(
    const base::string16& text,
    internal::TextRunList* out_run_list,
    CommonizedRunsMap* out_commonized_run_map) {
  TRACE_EVENT1("ui", "RenderTextHarfBuzz::ItemizeTextToRuns", "text_length",
               text.length());
  DCHECK(!text.empty());
  const Font& primary_font = font_list().GetPrimaryFont();

  // If ICU fails to itemize the text, we create a run that spans the entire
  // text. This is needed because leaving the runs set empty causes some clients
  // to misbehave since they expect non-zero text metrics from a non-empty text.
  ui::gfx::BiDiLineIterator bidi_iterator;

  if (!bidi_iterator.Open(text, GetTextDirection(text))) {
    auto run = std::make_unique<internal::TextRunHarfBuzz>(
        font_list().GetPrimaryFont());
    run->range = Range(0, text.length());
    internal::TextRunHarfBuzz::FontParams font_params(primary_font);
    (*out_commonized_run_map)[font_params].push_back(run.get());
    out_run_list->Add(std::move(run));
    return;
  }

  // Temporarily apply composition underlines and selection colors.
  ApplyCompositionAndSelectionStyles();

  // Build the run list from the script items and ranged styles and baselines.
  // Use an empty color BreakList to avoid breaking runs at color boundaries.
  BreakList<SkColor> empty_colors;
  empty_colors.SetMax(colors().max());
  DCHECK_LE(text.size(), baselines().max());
  for (const BreakList<bool>& style : styles())
    DCHECK_LE(text.size(), style.max());
  internal::StyleIterator style(empty_colors, baselines(),
                                font_size_overrides(), weights(), styles());

  // Split the original text by logical runs, then each logical run by common
  // script and each sequence at special characters and style boundaries. This
  // invariant holds: bidi_run_start <= script_run_start <= breaking_run_start
  // <= breaking_run_end <= script_run_end <= bidi_run_end
  for (size_t bidi_run_start = 0; bidi_run_start < text.length();) {
    // Determine the longest logical run (e.g. same bidi direction) from this
    // point.
    int32_t bidi_run_break = 0;
    UBiDiLevel bidi_level = 0;
    bidi_iterator.GetLogicalRun(bidi_run_start, &bidi_run_break, &bidi_level);
    size_t bidi_run_end = static_cast<size_t>(bidi_run_break);
    DCHECK_LT(bidi_run_start, bidi_run_end);

    ApplyForcedDirection(&bidi_level);

    for (size_t script_run_start = bidi_run_start;
         script_run_start < bidi_run_end;) {
      // Find the longest sequence of characters that have at least one common
      // UScriptCode value.
      UScriptCode script = USCRIPT_INVALID_CODE;
      size_t script_run_end =
          ScriptInterval(text, script_run_start,
                         bidi_run_end - script_run_start, &script) +
          script_run_start;
      DCHECK_LT(script_run_start, script_run_end);

      for (size_t breaking_run_start = script_run_start;
           breaking_run_start < script_run_end;) {
        // Break runs at certain characters that need to be rendered separately
        // to prevent either an unusual character from forcing a fallback font
        // on the entire run. After script intersection, many codepoints end up
        // in the script COMMON but can't be rendered together.
        size_t breaking_char_end = FindRunBreakingCharacter(
            text, script, breaking_run_start, script_run_end);

        // Break runs at style boundaries.
        style.UpdatePosition(
            GivenTextIndexToTextIndex(text, breaking_run_start));
        size_t text_style_end =
            TextIndexToGivenTextIndex(text, style.GetRange().end());

        // The next break is the nearest break position.
        const size_t breaking_run_end =
            std::min(breaking_char_end, text_style_end);
        DCHECK_LT(breaking_run_start, breaking_run_end);
        DCHECK(IsValidCodePointIndex(text, breaking_run_end));

        // Set the font params for the current run for the current run break.
        internal::TextRunHarfBuzz::FontParams font_params =
            CreateFontParams(primary_font, bidi_level, script, style);

        // Create the current run from [breaking_run_start, breaking_run_end[.
        auto run = std::make_unique<internal::TextRunHarfBuzz>(primary_font);
        run->range = Range(breaking_run_start, breaking_run_end);

        // Add the created run to the set of runs.
        (*out_commonized_run_map)[font_params].push_back(run.get());
        out_run_list->Add(std::move(run));

        // Move to the next run.
        breaking_run_start = breaking_run_end;
      }

      // Move to the next script sequence.
      script_run_start = script_run_end;
    }

    // Move to the next direction sequence.
    bidi_run_start = bidi_run_end;
  }

  // Add trace event to track incorrect usage of fallback fonts.
  // TODO(https://crbug.com/995789): Remove the following code when the issue
  // is fixed.
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("fonts", &tracing_enabled);
  if (tracing_enabled) {
    std::string logging_str;
    for (const auto& iter : *out_commonized_run_map) {
      const internal::TextRunHarfBuzz::FontParams& font_params = iter.first;
      for (const auto* run : iter.second) {
        base::i18n::UTF16CharIterator text_iter(
            text.c_str() + run->range.start(), run->range.length());
        const UChar32 first_char = text_iter.get();
        const UBlockCode first_block = ublock_getCode(first_char);
        const char* script_name = uscript_getShortName(font_params.script);
        base::StringAppendF(&logging_str, "block=%d script=%s\n",
                            static_cast<int>(first_block),
                            script_name ? script_name : "");
      }
    }
    TRACE_EVENT_INSTANT1("fonts", "RenderTextHarfBuzz::ItemizeTextToRuns::Runs",
                         TRACE_EVENT_SCOPE_THREAD, "runs", logging_str);
  }

  // Undo the temporarily applied composition underlines and selection colors.
  UndoCompositionAndSelectionStyles();
}

void RenderTextHarfBuzz::ShapeRuns(
    const base::string16& text,
    const internal::TextRunHarfBuzz::FontParams& font_params,
    std::vector<internal::TextRunHarfBuzz*> runs) {
  TRACE_EVENT1("ui", "RenderTextHarfBuzz::ShapeRuns", "run_count", runs.size());

  // Runs with a single newline character should be skipped since they can't be
  // rendered (see http://crbug/680430). The following code sets the runs
  // shaping output to report report the missing glyph and removes the runs from
  // the vector of runs to shape. The newline character doesn't have a
  // glyph, which otherwise forces this function to go through the expensive
  // font fallbacks before reporting a missing glyph (see http://crbug/972090).
  std::vector<internal::TextRunHarfBuzz*> need_shaping_runs;
  for (internal::TextRunHarfBuzz*& run : runs) {
    if (run->range.length() == 1 && text[run->range.start()] == '\n') {
      // Newline runs can't be shaped. Shape this run as if the glyph is
      // missing.
      run->font_params = font_params;
      run->shape.missing_glyph_count = 1;
      run->shape.glyph_count = 1;
      run->shape.glyphs.resize(run->shape.glyph_count);
      run->shape.glyph_to_char.resize(run->shape.glyph_count);
      run->shape.positions.resize(run->shape.glyph_count);
      run->shape.width = glyph_width_for_test_;
    } else {
      // This run needs shaping.
      need_shaping_runs.push_back(run);
    }
  }
  runs.swap(need_shaping_runs);
  if (runs.empty()) {
    RecordShapeRunsFallback(ShapeRunFallback::NO_FALLBACK);
    return;
  }

  const Font& primary_font = font_list().GetPrimaryFont();

  // Shaping with primary configured fonts from font_list().
  for (const Font& font : font_list().GetFonts()) {
    internal::TextRunHarfBuzz::FontParams test_font_params = font_params;
    if (test_font_params.SetRenderParamsRematchFont(
            font, font.GetFontRenderParams())) {
      ShapeRunsWithFont(text, test_font_params, &runs);
    }
    if (runs.empty()) {
      RecordShapeRunsFallback(ShapeRunFallback::NO_FALLBACK);
      return;
    }
  }

  // Keep a set of fonts already tried for shaping runs.
  std::set<Font, CaseInsensitiveCompare> fallback_fonts_already_tried;
  fallback_fonts_already_tried.insert(primary_font);

  // Find fallback fonts for the remaining runs using a worklist algorithm. Try
  // to shape the first run by using GetFallbackFont(...) and then try shaping
  // other runs with the same font. If the first font can't be shaped, remove it
  // and continue with the remaining runs until the worklist is empty. The
  // fallback font returned by GetFallbackFont(...) depends on the text of the
  // run and the results may differ between runs.
  std::vector<internal::TextRunHarfBuzz*> remaining_unshaped_runs;
  while (!runs.empty()) {
    Font fallback_font(primary_font);
    bool fallback_found;
    internal::TextRunHarfBuzz* current_run = *runs.begin();
    {
      SCOPED_UMA_HISTOGRAM_LONG_TIMER("RenderTextHarfBuzz.GetFallbackFontTime");
      TRACE_EVENT1("ui", "RenderTextHarfBuzz::GetFallbackFont", "script",
                   TRACE_STR_COPY(uscript_getShortName(font_params.script)));
      const base::StringPiece16 run_text(&text[current_run->range.start()],
                                         current_run->range.length());
      fallback_found =
          GetFallbackFont(primary_font, locale_, run_text, &fallback_font);
    }

    if (fallback_found) {
      const bool fallback_font_is_untried =
          fallback_fonts_already_tried.insert(fallback_font).second;
      if (fallback_font_is_untried) {
        internal::TextRunHarfBuzz::FontParams test_font_params = font_params;
        if (test_font_params.SetRenderParamsOverrideSkiaFaceFromFont(
                fallback_font, fallback_font.GetFontRenderParams())) {
          ShapeRunsWithFont(text, test_font_params, &runs);
        }
      }
    }

    // Remove the first run if not fully shaped with its associated fallback
    // font.
    if (!runs.empty() && runs[0] == current_run) {
      remaining_unshaped_runs.push_back(current_run);
      runs.erase(runs.begin());
    }
  }
  runs.swap(remaining_unshaped_runs);
  if (runs.empty()) {
    RecordShapeRunsFallback(ShapeRunFallback::FALLBACK);
    return;
  }

  std::vector<Font> fallback_font_list;
  {
    SCOPED_UMA_HISTOGRAM_LONG_TIMER("RenderTextHarfBuzz.GetFallbackFontsTime");
    TRACE_EVENT1("ui", "RenderTextHarfBuzz::GetFallbackFonts", "script",
                 TRACE_STR_COPY(uscript_getShortName(font_params.script)));
    fallback_font_list = GetFallbackFonts(primary_font);

#if defined(OS_WIN)
    // Append fonts in the fallback list of the fallback fonts.
    // TODO(tapted): Investigate whether there's a case that benefits from this
    // on Mac.
    for (const auto& fallback_font : fallback_fonts_already_tried) {
      std::vector<Font> fallback_fonts = GetFallbackFonts(fallback_font);
      fallback_font_list.insert(fallback_font_list.end(),
                                fallback_fonts.begin(), fallback_fonts.end());
    }

    // Add Segoe UI and its associated linked fonts to the fallback font list to
    // ensure that the fallback list covers the basic cases.
    // http://crbug.com/467459. On some Windows configurations the default font
    // could be a raster font like System, which would not give us a reasonable
    // fallback font list.
    Font segoe("Segoe UI", 13);
    if (!fallback_fonts_already_tried.count(segoe)) {
      std::vector<Font> default_fallback_families = GetFallbackFonts(segoe);
      fallback_font_list.insert(fallback_font_list.end(),
                                default_fallback_families.begin(),
                                default_fallback_families.end());
    }
#endif
  }

  // Use a set to track the fallback fonts and avoid duplicate entries.
  SCOPED_UMA_HISTOGRAM_LONG_TIMER(
      "RenderTextHarfBuzz.ShapeRunsWithFallbackFontsTime");
  TRACE_EVENT1("ui", "RenderTextHarfBuzz::ShapeRunsWithFallbackFonts",
               "fonts_count", fallback_font_list.size());

  // Try shaping with the fallback fonts.
  for (const auto& font : fallback_font_list) {
    std::string font_name = font.GetFontName();

    const bool fallback_font_is_untried =
        fallback_fonts_already_tried.insert(font).second;
    if (!fallback_font_is_untried)
      continue;

    FontRenderParamsQuery query;
    query.families.push_back(font_name);
    query.pixel_size = font_params.font_size;
    query.style = font_params.italic ? Font::ITALIC : 0;
    FontRenderParams fallback_render_params = GetFontRenderParams(query, NULL);
    internal::TextRunHarfBuzz::FontParams test_font_params = font_params;
    if (test_font_params.SetRenderParamsOverrideSkiaFaceFromFont(
            font, fallback_render_params)) {
      ShapeRunsWithFont(text, test_font_params, &runs);
    }
    if (runs.empty()) {
      TRACE_EVENT_INSTANT2("ui", "RenderTextHarfBuzz::FallbackFont",
                           TRACE_EVENT_SCOPE_THREAD, "font_name",
                           TRACE_STR_COPY(font_name.c_str()),
                           "primary_font_name", primary_font.GetFontName());
      RecordShapeRunsFallback(ShapeRunFallback::FALLBACKS);
      return;
    }
  }

  for (internal::TextRunHarfBuzz*& run : runs) {
    if (run->shape.missing_glyph_count == std::numeric_limits<size_t>::max()) {
      run->shape.glyph_count = 0;
      run->shape.width = 0.0f;
    }
  }

  RecordShapeRunsFallback(ShapeRunFallback::FAILED);
}

void RenderTextHarfBuzz::ShapeRunsWithFont(
    const base::string16& text,
    const internal::TextRunHarfBuzz::FontParams& font_params,
    std::vector<internal::TextRunHarfBuzz*>* in_out_runs) {
  // ShapeRunWithFont can be extremely slow, so use cached results if possible.
  // Only do this on the UI thread, to avoid synchronization overhead (and
  // because almost all calls are on the UI thread. Also avoid caching long
  // strings, to avoid blowing up the cache size.
  constexpr size_t kMaxRunLengthToCache = 25;
  static base::NoDestructor<internal::ShapeRunCache> cache;

  std::vector<internal::TextRunHarfBuzz*> runs_with_missing_glyphs;
  for (internal::TextRunHarfBuzz*& run : *in_out_runs) {
    // First do a cache lookup.
    bool can_use_cache = base::MessageLoopCurrentForUI::IsSet() &&
                         run->range.length() <= kMaxRunLengthToCache;
    bool found_in_cache = false;
    const internal::ShapeRunWithFontInput cache_key(
        text, font_params, run->range, obscured(), glyph_width_for_test_,
        glyph_spacing(), subpixel_rendering_suppressed());
    if (can_use_cache) {
      auto found = cache.get()->Get(cache_key);
      if (found != cache.get()->end()) {
        run->UpdateFontParamsAndShape(font_params, found->second);
        found_in_cache = true;
      }
    }

    // If that fails, compute the shape of the run, and add the result to the
    // cache.
    // TODO(ccameron): Coalesce calls to ShapeRunsWithFont when possible.
    if (!found_in_cache) {
      internal::TextRunHarfBuzz::ShapeOutput output;
      ShapeRunWithFont(cache_key, &output);
      run->UpdateFontParamsAndShape(font_params, output);
      if (can_use_cache)
        cache.get()->Put(cache_key, output);
    }

    // Check to see if we still have missing glyphs.
    if (run->shape.missing_glyph_count)
      runs_with_missing_glyphs.push_back(run);
  }
  in_out_runs->swap(runs_with_missing_glyphs);
}

void RenderTextHarfBuzz::EnsureLayoutRunList() {
  if (update_layout_run_list_) {
    layout_run_list_.Reset();

    const base::string16& text = layout_text();
    if (!text.empty())
      ItemizeAndShapeText(text, &layout_run_list_);

    display_run_list_.reset();
    update_display_text_ = true;
    update_layout_run_list_ = false;
  }
  if (update_display_text_) {
    std::vector<internal::Line> empty_lines;
    set_lines(&empty_lines);
    UpdateDisplayText(multiline() ? 0 : layout_run_list_.width());
    update_display_text_ = false;
    update_display_run_list_ = text_elided();
  }
}

// Returns the current run list, |display_run_list_| if the text is elided, or
// |layout_run_list_| otherwise.
internal::TextRunList* RenderTextHarfBuzz::GetRunList() {
  DCHECK(!update_layout_run_list_);
  DCHECK(!update_display_run_list_);
  return text_elided() ? display_run_list_.get() : &layout_run_list_;
}

const internal::TextRunList* RenderTextHarfBuzz::GetRunList() const {
  return const_cast<RenderTextHarfBuzz*>(this)->GetRunList();
}

bool RenderTextHarfBuzz::GetDecoratedTextForRange(
    const Range& range,
    DecoratedText* decorated_text) {
  if (obscured())
    return false;

  EnsureLayout();

  decorated_text->attributes.clear();
  decorated_text->text = GetTextFromRange(range);

  const internal::TextRunList* run_list = GetRunList();
  for (size_t i = 0; i < run_list->size(); i++) {
    const internal::TextRunHarfBuzz& run = *run_list->runs()[i];

    const Range intersection = range.Intersect(run.range);
    DCHECK(!intersection.is_reversed());

    if (!intersection.is_empty()) {
      int style = Font::NORMAL;
      if (run.font_params.italic)
        style |= Font::ITALIC;
      if (run.font_params.underline || run.font_params.heavy_underline)
        style |= Font::UNDERLINE;

      // Get range relative to the decorated text.
      DecoratedText::RangedAttribute attribute(
          Range(intersection.start() - range.GetMin(),
                intersection.end() - range.GetMin()),
          run.font_params.font.Derive(0, style, run.font_params.weight));

      attribute.strike = run.font_params.strike;
      decorated_text->attributes.push_back(attribute);
    }
  }
  return true;
}

}  // namespace gfx
