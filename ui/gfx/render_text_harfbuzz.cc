// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_harfbuzz.h"

#include <limits>
#include <set>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "skia/ext/font_utils.h"
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
#include "ui/gfx/harfbuzz_font_skia.h"
#include "ui/gfx/platform_font.h"
#include "ui/gfx/range/range_f.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/utf16_indexing.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/foundation_util.h"
#include "base/mac/mac_util.h"
#include "third_party/skia/include/ports/SkTypeface_mac.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/locale_utils.h"
#endif  // BUILDFLAG(IS_ANDROID)

#include <hb.h>

namespace gfx {

namespace {

// Text length limit. Longer strings are slow and not fully tested.
const size_t kMaxTextLength = 10000;

// The maximum number of scripts a Unicode character can belong to. This value
// is arbitrarily chosen to be a good limit because it is unlikely for a single
// character to belong to more scripts.
const size_t kMaxScripts = 32;

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

// Returns a vector containing the script and the script extensions of the
// Unicode |codepoint|.
std::vector<UScriptCode> GetScriptExtensions(UChar32 codepoint) {
  UErrorCode icu_error = U_ZERO_ERROR;
  std::vector<UScriptCode> result(kMaxScripts);
  size_t count = uscript_getScriptExtensions(codepoint, result.data(),
                                             result.size(), &icu_error);
  CHECK_NE(icu_error, U_BUFFER_OVERFLOW_ERROR) << " #ext: " << count;
  if (U_FAILURE(icu_error))
    return {};
  result.resize(count);

  return result;
}

// Intersects the script extensions set of |codepoint| with |result| and writes
// to |result|.
void ScriptSetIntersect(UChar32 codepoint, std::vector<UScriptCode>& result) {
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
  std::vector<UScriptCode> extensions = GetScriptExtensions(codepoint);

  // Implicit script 'inherited' is inheriting scripts from preceding codepoint.
  if (extensions.size() == 1 && extensions[0] == USCRIPT_INHERITED) {
    return;
  }

  CHECK(!base::Contains(extensions, USCRIPT_INHERITED));

  std::erase_if(
      result, [&](auto script) { return !base::Contains(extensions, script); });
}

struct GraphemeProperties {
  bool has_control = false;
  bool has_bracket = false;
  bool has_pictographic = false;
  bool has_emoji = false;
  UBlockCode block = UBLOCK_NO_BLOCK;
};

// Returns the properties for the codepoints part of the given text.
GraphemeProperties RetrieveGraphemeProperties(std::u16string_view text,
                                              bool retrieve_block) {
  GraphemeProperties properties;
  bool first_char = true;
  for (base::i18n::UTF16CharIterator iter(text); !iter.end(); iter.Advance()) {
    const UChar32 codepoint = iter.get();

    if (first_char) {
      first_char = false;
      if (retrieve_block)
        properties.block = ublock_getCode(codepoint);
    }

    if (codepoint == '\n' || codepoint == '\r' || codepoint == ' ')
      properties.has_control = true;
    if (IsBracket(codepoint))
      properties.has_bracket = true;
    if (IsExtendedPictographicCodepoint(codepoint))
      properties.has_pictographic = true;
    if (IsEmojiRelatedCodepoint(codepoint))
      properties.has_emoji = true;
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
// breaking point where grapheme properties are no longer compatible
// (see: UNICODE TEXT SEGMENTATION (http://unicode.org/reports/tr29/).
// Breaks between |run_start| and |run_end| and force break after the grapheme
// starting at |run_break|.
size_t FindRunBreakingCharacter(const std::u16string& text,
                                UScriptCode script,
                                size_t run_start,
                                size_t run_break,
                                size_t run_end) {
  const size_t run_length = run_end - run_start;
  const std::u16string_view text_view(text);
  const std::u16string_view run_text(text_view.substr(run_start, run_length));
  const bool is_common_script = (script == USCRIPT_COMMON);

  DCHECK(!run_text.empty());

  // Create an iterator to split the text in graphemes.
  base::i18n::BreakIterator grapheme_iterator(
      run_text, base::i18n::BreakIterator::BREAK_CHARACTER);
  if (!grapheme_iterator.Init() || !grapheme_iterator.Advance()) {
    // In case of error, isolate the first character in a separate run.
    NOTREACHED_IN_MIGRATION();
    return run_start + 1;
  }

  // Retrieve the first grapheme and its codepoint properties.
  const std::u16string_view first_grapheme_text =
      grapheme_iterator.GetStringView();
  const GraphemeProperties first_grapheme_properties =
      RetrieveGraphemeProperties(first_grapheme_text, is_common_script);

  // Append subsequent graphemes in this grapheme cluster if they are
  // compatible, otherwise break the current run.
  while (grapheme_iterator.Advance()) {
    const std::u16string_view current_grapheme_text =
        grapheme_iterator.GetStringView();
    const GraphemeProperties current_grapheme_properties =
        RetrieveGraphemeProperties(current_grapheme_text, is_common_script);

    const size_t current_breaking_position =
        run_start + grapheme_iterator.prev();
    if (!AreGraphemePropertiesCompatible(first_grapheme_properties,
                                         current_grapheme_properties)) {
      return current_breaking_position;
    }

    // Break if the beginning of this grapheme is after |run_break|.
    if (run_start + grapheme_iterator.prev() >= run_break) {
      DCHECK_LE(current_breaking_position, run_end);
      return current_breaking_position;
    }
  }

  // Do not break this run, returns end of the text.
  return run_end;
}

// Find the longest sequence of characters from 0 and up to |length| that have
// at least one common UScriptCode value. Writes the common script value to
// |script| and returns the length of the sequence. Takes the characters' script
// extensions into account. http://www.unicode.org/reports/tr24/#ScriptX
//
// Consider 3 characters with the script values {Kana}, {Hira, Kana}, {Kana}.
// Without script extensions only the first script in each set would be taken
// into account, resulting in 3 runs where 1 would be enough.
size_t ScriptInterval(const std::u16string& text,
                      size_t start,
                      size_t length,
                      UScriptCode* script) {
  DCHECK_GT(length, 0U);

  base::i18n::UTF16CharIterator char_iterator(
      std::u16string_view(text).substr(start, length));

  std::vector<UScriptCode> scripts = GetScriptExtensions(char_iterator.get());
  *script = scripts[0];

  while (char_iterator.Advance()) {
    ScriptSetIntersect(char_iterator.get(), scripts);
    if (scripts.empty()) {
      return char_iterator.array_pos();
    }
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

bool FontWasAlreadyTried(sk_sp<SkTypeface> typeface,
                         std::set<SkTypefaceID>* fallback_fonts) {
  return fallback_fonts->count(typeface->uniqueID()) != 0;
}

void MarkFontAsTried(sk_sp<SkTypeface> typeface,
                     std::set<SkTypefaceID>* fallback_fonts) {
  fallback_fonts->insert(typeface->uniqueID());
}

// Whether |segment| corresponds to the newline character.
bool IsNewlineSegment(const std::u16string& text,
                      const internal::LineSegment& segment) {
  const size_t offset = segment.char_range.start();
  const size_t length = segment.char_range.length();
  DCHECK_LT(segment.char_range.start() + length - 1, text.length());
  return (length == 1 && (text[offset] == '\r' || text[offset] == '\n')) ||
         (length == 2 && text[offset] == '\r' && text[offset + 1] == '\n');
}

// Returns the line index considering the newline character. Line index is
// incremented if the caret is right after the newline character, i.e, the
// cursor affinity is |CURSOR_BACKWARD| while containing the newline character.
size_t LineIndexForNewline(const size_t line_index,
                           const std::u16string& text,
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
                      float glyph_height_for_test,
                      WordWrapBehavior word_wrap_behavior,
                      const std::u16string& text,
                      const internal::TextRunList& run_list)
      : max_width_((max_width == 0) ? SK_ScalarMax : SkIntToScalar(max_width)),
        min_baseline_(min_baseline),
        min_height_(min_height),
        glyph_height_for_test_(glyph_height_for_test),
        word_wrap_behavior_(word_wrap_behavior),
        text_(text),
        run_list_(run_list),
        max_descent_(0),
        max_ascent_(0),
        text_x_(0),
        available_width_(max_width_) {
    AdvanceLine();
  }

  HarfBuzzLineBreaker(const HarfBuzzLineBreaker&) = delete;
  HarfBuzzLineBreaker& operator=(const HarfBuzzLineBreaker&) = delete;

  // Constructs a single line for |text_| using |run_list_|.
  void ConstructSingleLine() {
    for (size_t i = 0; i < run_list_->size(); i++) {
      const internal::TextRunHarfBuzz& run = *(run_list_->runs()[i]);
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
    // Get an iterator that pass through valid line breaking positions.
    // See https://www.unicode.org/reports/tr14/tr14-11.html for lines breaking.
    base::i18n::BreakIterator words(*text_,
                                    base::i18n::BreakIterator::BREAK_LINE);
    const bool success = words.Init();
    DCHECK(success);
    if (!success)
      return;

    while (words.Advance()) {
      const Range word_range = Range(words.prev(), words.pos());
      std::vector<internal::LineSegment> word_segments;
      SkScalar word_width = GetWordWidth(word_range, &word_segments);

      // If the last word is '\n', we should advance a new line after adding
      // the word to the current line.
      bool new_line = false;
      if (!word_segments.empty() &&
          IsNewlineSegment(*text_, word_segments.back())) {
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
    // If the last character of the text is a new line character, then the last
    // line is any empty string, which contains no segments. This means that the
    // display_text_index will not have been set in AdvanceLine. So here, set
    // display_text_index to the text length, which is the true text index of
    // the final line.
    internal::Line* line = &lines_.back();
    if (line->display_text_index == 0)
      line->display_text_index = text_->size();
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
      // Compute the line start while the line segments are in the logical order
      // so that the start of the line is the start of the char range,
      // regardless of i18n.
      if (!line->segments.empty())
        line->display_text_index = line->segments[0].char_range.start();
      std::sort(line->segments.begin(), line->segments.end(),
                [this](const internal::LineSegment& s1,
                       const internal::LineSegment& s2) -> bool {
                  return run_list_->logical_to_visual(s1.run) <
                         run_list_->logical_to_visual(s2.run);
                });

      line->size.set_height(
          glyph_height_for_test_
              ? glyph_height_for_test_
              : std::max(min_height_, max_descent_ + max_ascent_));

      line->baseline = std::max(min_baseline_, SkScalarRoundToInt(max_ascent_));
      line->preceding_heights = base::ClampCeil(total_size_.height());
      // Subtract newline segment's width from |total_size_| because it's not
      // drawn.
      float line_width = line->size.width();
      if (!line->segments.empty() &&
          IsNewlineSegment(*text_, line->segments.back())) {
        line_width -= line->segments.back().width();
      }
      if (line->segments.size() > 1 &&
          IsNewlineSegment(*text_, line->segments.front())) {
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

      if (IsNewlineSegment(*text_, segment) ||
          segment.width() <= available_width_ ||
          word_wrap_behavior_ == IGNORE_LONG_WORDS) {
        AddLineSegment(segment, true);
      } else {
        DCHECK(word_wrap_behavior_ == TRUNCATE_LONG_WORDS ||
               word_wrap_behavior_ == WRAP_LONG_WORDS);
        has_truncated = (word_wrap_behavior_ == TRUNCATE_LONG_WORDS);

        const internal::TextRunHarfBuzz& run =
            *(run_list_->runs()[segment.run]);
        internal::LineSegment remaining_segment = segment;
        while (!remaining_segment.char_range.is_empty()) {
          size_t cutoff_pos = GetCutoffPos(remaining_segment);
          SkScalar width = run.GetGlyphWidthForCharRange(
              Range(remaining_segment.char_range.start(), cutoff_pos));
          if (remaining_segment.char_range.start() != cutoff_pos) {
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
    const internal::TextRunHarfBuzz& run = *(run_list_->runs()[segment.run]);
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
    if (!multiline || !IsNewlineSegment(*text_, segment)) {
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
        *(run_list_->runs()[segment.run]).get();
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
        segment.char_range.start(), FindValidBoundaryBefore(*text_, end_pos));
    if (end_pos != valid_end_pos) {
      end_pos = valid_end_pos;
      width = run.GetGlyphWidthForCharRange(
          Range(segment.char_range.start(), end_pos));
    }

    // |max_width_| might be smaller than a single character. In this case we
    // need to put at least one character in the line. Note that, we should
    // not separate surrogate pair or combining characters.
    // See RenderTextHarfBuzzTest.Multiline_MinWidth for an example.
    if (width == 0 && available_width_ == max_width_ &&
        end_pos < segment.char_range.end()) {
      end_pos = std::min(segment.char_range.end(),
                         FindValidBoundaryAfter(*text_, end_pos + 1));
    }

    return end_pos;
  }

  // Gets the glyph width for |word_range|, and splits the |word| into different
  // segments based on its runs.
  SkScalar GetWordWidth(const Range& word_range,
                        std::vector<internal::LineSegment>* segments) const {
    if (word_range.is_empty() || segments == nullptr)
      return 0;
    size_t run_start_index = run_list_->GetRunIndexAt(word_range.start());
    size_t run_end_index = run_list_->GetRunIndexAt(word_range.end() - 1);
    SkScalar width = 0;
    for (size_t i = run_start_index; i <= run_end_index; i++) {
      const internal::TextRunHarfBuzz& run = *(run_list_->runs()[i]);
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
  const float glyph_height_for_test_;
  const WordWrapBehavior word_wrap_behavior_;
  const raw_ref<const std::u16string> text_;
  const raw_ref<const internal::TextRunList> run_list_;

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
    const Font& primary_font,
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
  font_params.fill_style = style.fill_style();
  font_params.stroke_width = style.stroke_width();
  font_params.level = bidi_level;
  font_params.script = script;
  // Odd BiDi embedding levels correspond to RTL runs.
  font_params.is_rtl = (font_params.level % 2) == 1;
  return font_params;
}

BASE_FEATURE(kRemoveFontLinkFallbacks,
             "RemoveFontLinkFallbacks",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsRemoveFontLinkFallbacks() {
  return base::FeatureList::IsEnabled(kRemoveFontLinkFallbacks);
}

BASE_FEATURE(kEnableFallbackFontsCrashReporting,
             "EnableFallbackFontsCrashReporting",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsEnableFallbackFontsCrashReporting() {
  return base::FeatureList::IsEnabled(kEnableFallbackFontsCrashReporting);
}

// Append to `in_out_report` the font name and the text correlating to the runs
// shaped by that font. This crash report will be used to debug why text is
// being shaped through the GetFallbackFonts path as we shouldn't need to
// fallback to that call path. crbug.com/995789
void AppendFontNameAndShapedTextToCrashDumpReport(
    const std::u16string& text,
    const std::vector<internal::TextRunHarfBuzz*>& shaped_runs,
    const std::string& font_name,
    std::u16string& report) {
  const std::u16string font_name_seperator = u"[font name] ";
  const std::u16string run_start = u"[run start] ";
  const std::u16string run_end = u" [run end]";
  report += font_name_seperator + base::ASCIIToUTF16(font_name.c_str());
  for (internal::TextRunHarfBuzz* run : shaped_runs) {
    std::u16string text_substring =
        text.substr(run->range.start(), run->range.end());
    report += run_start + text_substring + run_end;
  }
}

}  // namespace

namespace internal {

sk_sp<SkTypeface> CreateSkiaTypeface(const Font& font,
                                     bool italic,
                                     Font::Weight weight) {
#if BUILDFLAG(IS_APPLE)
  const Font::FontStyle style = italic ? Font::ITALIC : Font::NORMAL;
  Font font_with_style = font.Derive(0, style, weight);
  if (!font_with_style.GetCTFont()) {
    return nullptr;
  }

  return SkMakeTypefaceFromCTFont(font_with_style.GetCTFont());
#else
  SkFontStyle skia_style(
      static_cast<int>(weight), SkFontStyle::kNormal_Width,
      italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);
  return skia::MakeTypefaceFromName(font.GetFontName().c_str(), skia_style);
#endif
}

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
         level == other.level && fill_style == other.fill_style &&
         stroke_width == other.stroke_width;
}

void TextRunHarfBuzz::FontParams::
    ComputeRenderParamsFontSizeAndBaselineOffset() {
  render_params = font.GetFontRenderParams();
  if (font_size == 0)
    font_size = font.GetFontSize();
  baseline_offset = 0;
  if (baseline_type != BaselineStyle::kNormalBaseline) {
    // Calculate a slightly smaller font. The ratio here is somewhat arbitrary.
    // Proportions from 5/9 to 5/7 all look pretty good.
    const float ratio = 5.0f / 9.0f;
    font_size = base::ClampRound(font.GetFontSize() * ratio);
    switch (baseline_type) {
      case BaselineStyle::kSuperscript:
        baseline_offset = font.GetCapHeight() - font.GetHeight();
        break;
      case BaselineStyle::kSuperior:
        baseline_offset =
            base::ClampRound(font.GetCapHeight() * ratio) - font.GetCapHeight();
        break;
      case BaselineStyle::kSubscript:
        baseline_offset = font.GetHeight() - font.GetBaseline();
        break;
      case BaselineStyle::kInferior:  // Fall through.
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
         static_cast<size_t>(key.script) << 24 ^
         static_cast<size_t>(key.fill_style) << 28;
}

bool TextRunHarfBuzz::FontParams::SetRenderParamsRematchFont(
    const Font& new_font,
    const FontRenderParams& new_render_params) {
  // This takes the font family name from new_font, and calls
  // skia::MakeTypefaceFromName() with that family name and the style
  // information internal to this text run. So it triggers a new font match and
  // looks for adjacent fonts in the family. This works for styling, e.g.
  // styling a run in bold, italic or underline, but breaks font fallback in
  // certain scenarios, as the fallback font may be of a different weight and
  // style than the run's own, so this can lead to a failure of instantiating
  // the correct fallback font.
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
  sk_sp<SkTypeface> new_skia_face = platform_font->GetNativeSkTypeface();

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
  // Obscured glyphs are centered in their allotted space by adjusting their
  // positions during shaping. Include the space preceding the glyph when
  // calculating grapheme bounds.
  const float half_obscured_spacing =
      render_text->obscured() ? render_text->obscured_glyph_spacing() / 2.0f
                              : 0.0f;
  const float cluster_begin_x =
      shape.positions[glyphs.start()].x() - half_obscured_spacing;
  const float cluster_end_x =
      glyphs.end() < shape.glyph_count
          ? shape.positions[glyphs.end()].x() - half_obscured_spacing
          : SkFloatToScalar(shape.width);
  DCHECK_LE(cluster_begin_x, cluster_end_x);

  // A cluster consists of a number of code points and corresponds to a number
  // of glyphs that should be drawn together. A cluster can contain multiple
  // graphemes. In order to place the cursor at a grapheme boundary inside the
  // cluster, we simply divide the cluster width by the number of graphemes.
  ptrdiff_t code_point_count = UTF16IndexToOffset(render_text->GetDisplayText(),
                                                  chars.start(), chars.end());
  if (code_point_count > 1) {
    int before = 0;
    int total = 0;
    for (size_t i = chars.start(); i < chars.end(); ++i) {
      if (render_text->IsGraphemeBoundary(i)) {
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
    NOTREACHED_IN_MIGRATION()
        << "The glyph range is empty or invalid! Its char range: ["
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
    for (auto& glyph_to_char : shape.glyph_to_char) {
      glyph_to_char += range.start();
    }
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
  ShapeRunWithFontInput(const std::u16string& full_text,
                        const TextRunHarfBuzz::FontParams& font_params,
                        Range full_range,
                        bool obscured,
                        float glyph_width_for_test,
                        int obscured_glyph_spacing,
                        bool subpixel_rendering_suppressed)
      : skia_face(font_params.skia_face),
        render_params(font_params.render_params),
        script(font_params.script),
        font_size(font_params.font_size),
        obscured_glyph_spacing(obscured_glyph_spacing),
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
    hash = base::FastHash(base::as_bytes(base::make_span(text)));
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
           obscured_glyph_spacing == other.obscured_glyph_spacing &&
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
  int obscured_glyph_spacing;
  float glyph_width_for_test;
  bool is_rtl;
  bool obscured;
  bool subpixel_rendering_suppressed;

  // The parts of the input text that may be read by hb_buffer_add_utf16.
  std::u16string text;
  // The conversion of the input range to a range within |text|.
  Range range;
  // The hash is cached to avoid repeated calls.
  size_t hash = 0;
};

// An LRU cache of the results from calling ShapeRunWithFont. The maximum cache
// size used in blink::ShapeCache is 10k. A Finch experiment showed that
// reducing the cache size to 1k has no performance impact.
constexpr int kShapeRunCacheSize = 1000;
using ShapeRunCacheBase = base::HashingLRUCache<ShapeRunWithFontInput,
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
  base::span<hb_glyph_info_t> infos = [](hb_buffer_t* buffer) {
    unsigned int count;
    hb_glyph_info_t* data = hb_buffer_get_glyph_infos(buffer, &count);
    // SAFETY: harfbuzz guarantees that hb_buffer_get_glyph_infos() writes the
    // count for the returned data array into count.
    return UNSAFE_BUFFERS(base::make_span(data, count));
  }(buffer);

  out->glyph_count = infos.size();

  base::span<hb_glyph_position_t> positions = [](hb_buffer_t* buffer) {
    unsigned int count;
    hb_glyph_position_t* data = hb_buffer_get_glyph_positions(buffer, &count);
    // SAFETY: harfbuzz guarantees that hb_buffer_get_glyph_positions() writes
    // the count for the returned data array into count.
    return UNSAFE_BUFFERS(base::make_span(data, count));
  }(buffer);

  out->glyphs.resize(out->glyph_count);
  out->glyph_to_char.resize(out->glyph_count);
  out->positions.resize(out->glyph_count);
  out->width = 0.0f;

  // Font on MAC like ".SF NS Text" may have a negative x_offset. Positive
  // x_offset are also found on Windows (e.g. "Segoe UI"). It requires tests
  // relying on the behavior of |glyph_width_for_test_| to also be given a zero
  // x_offset, otherwise expectations get thrown off
  // (see: http://crbug.com/1056220).
  const bool force_zero_offset = in.glyph_width_for_test > 0;
  constexpr uint16_t kMissingGlyphId = 0;

  out->missing_glyph_count = 0;
  for (size_t i = 0; i < out->glyph_count; ++i) {
    DCHECK_LE(infos[i].codepoint, std::numeric_limits<uint16_t>::max());
    uint16_t glyph = static_cast<uint16_t>(infos[i].codepoint);
    out->glyphs[i] = glyph;
    if (glyph == kMissingGlyphId)
      out->missing_glyph_count += 1;
    DCHECK_GE(infos[i].cluster, in.range.start());
    out->glyph_to_char[i] = infos[i].cluster - in.range.start();

    SkScalar x_offset = HarfBuzzUnitsToSkiaScalar(positions[i].x_offset);

    if (in.obscured)
      // Place obscured glyphs in the middle of the allotted spacing.
      x_offset += in.obscured_glyph_spacing / 2.0f;
    if (force_zero_offset)
      x_offset = 0;
    const SkScalar y_offset = HarfBuzzUnitsToSkiaScalar(positions[i].y_offset);
    out->positions[i].set(out->width + x_offset, -y_offset);

    if (in.glyph_width_for_test == 0)
      out->width += HarfBuzzUnitsToFloat(positions[i].x_advance);
    else if (positions[i].x_advance) {  // Leave zero-width glyphs alone.
      out->width += in.glyph_width_for_test;
    }

    if (in.obscured)
      out->width += in.obscured_glyph_spacing;

    // When subpixel positioning is not enabled, glyph width is rounded to avoid
    // fractional width. Disable this conversion when a glyph width is provided
    // for testing. Using an integral glyph width has the same behavior as
    // disabling the subpixel positioning.
    const bool force_subpixel_for_test = in.glyph_width_for_test != 0;

    // Round run widths if subpixel positioning is off to match native behavior.
    if (!in.render_params.subpixel_positioning && !force_subpixel_for_test)
      out->width = std::round(out->width);
  }

  hb_buffer_destroy(buffer);
  hb_font_destroy(harfbuzz_font);
}

std::string GetApplicationLocale() {
#if BUILDFLAG(IS_ANDROID)
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
      update_display_text_(false),
      locale_(internal::GetApplicationLocale()) {
  set_truncate_length(kMaxTextLength);
}

RenderTextHarfBuzz::~RenderTextHarfBuzz() {}

const std::u16string& RenderTextHarfBuzz::GetDisplayText() {
  // TODO(krb): Consider other elision modes for multiline.
  if ((multiline() && (max_lines() == 0 || elide_behavior() != ELIDE_TAIL)) ||
      elide_behavior() == NO_ELIDE || elide_behavior() == FADE_TAIL) {
    // Call UpdateDisplayText to clear |display_text_| and |text_elided_|
    // on the RenderText class.
    UpdateDisplayText(0);
    update_display_text_ = false;
    display_run_list_.reset();
    return GetLayoutText();
  }

  EnsureLayoutRunList();
  DCHECK(!update_display_text_);
  return text_elided() ? display_text() : GetLayoutText();
}

SizeF RenderTextHarfBuzz::GetStringSizeF() {
  EnsureLayout();
  return total_size_;
}

SizeF RenderTextHarfBuzz::GetLineSizeF(const SelectionModel& caret) {
  const internal::ShapedText* shaped_text = GetShapedText();
  const auto& caret_run = GetRunContainingCaret(caret);
  for (const auto& line : shaped_text->lines()) {
    for (const internal::LineSegment& segment : line.segments) {
      if (segment.run == caret_run)
        return line.size;
    }
  }

  return shaped_text->lines().back().size;
}

std::vector<Rect> RenderTextHarfBuzz::GetSubstringBounds(const Range& range) {
  EnsureLayout();
  DCHECK(!update_display_run_list_);
  DCHECK(range.IsBoundedBy(Range(0, text().length())));
  const Range grapheme_range = ExpandRangeToGraphemeBoundary(range);
  const Range display_range(TextIndexToDisplayIndex(grapheme_range.start()),
                            TextIndexToDisplayIndex(grapheme_range.end()));
  DCHECK(IsValidDisplayRange(display_range));

  std::vector<Rect> rects;
  if (display_range.is_empty())
    return rects;

  internal::TextRunList* run_list = GetRunList();
  const internal::ShapedText* shaped_text = GetShapedText();
  for (size_t line_index = 0; line_index < shaped_text->lines().size();
       ++line_index) {
    const internal::Line& line = shaped_text->lines()[line_index];
    // Only the last line can be empty.
    DCHECK(!line.segments.empty() ||
           (line_index == shaped_text->lines().size() - 1));
    float line_start_x =
        line.segments.empty()
            ? 0
            : run_list->runs()[line.segments[0].run]->preceding_run_widths;

    if (line.segments.size() > 1 && IsNewlineSegment(line.segments[0]))
      line_start_x += line.segments[0].width();

    std::vector<Rect> current_line_rects;
    for (const internal::LineSegment& segment : line.segments) {
      const Range intersection = segment.char_range.Intersect(display_range);
      DCHECK(!intersection.is_reversed());
      if (!intersection.is_empty()) {
        const internal::TextRunHarfBuzz& run = *run_list->runs()[segment.run];
        RangeF selected_span =
            run.GetGraphemeSpanForCharRange(this, intersection);
        DCHECK(!selected_span.is_reversed());
        int start_x = base::ClampFloor(selected_span.start() - line_start_x);
        int end_x = base::ClampCeil(selected_span.end() - line_start_x);
        Rect rect(start_x, 0, end_x - start_x,
                  base::ClampCeil(line.size.height()));
        current_line_rects.push_back(rect + GetLineOffset(line_index));
      }
    }
    MergeIntersectingRects(current_line_rects);
    rects.insert(rects.end(), current_line_rects.begin(),
                 current_line_rects.end());
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

  size_t next_grapheme_start = valid_range.end();
  if (!IsValidCursorIndex(next_grapheme_start)) {
    next_grapheme_start =
        IndexOfAdjacentGrapheme(next_grapheme_start, CURSOR_FORWARD);
  }

  Range display_range(TextIndexToDisplayIndex(valid_range.start()),
                      TextIndexToDisplayIndex(next_grapheme_start));
  DCHECK(IsValidDisplayRange(display_range));

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

  if (!multiline()) {
    DCHECK_EQ(1u, GetShapedText()->lines().size());
    return 0;
  }

  size_t layout_position = TextIndexToDisplayIndex(caret.caret_pos());
  LogicalCursorDirection affinity = caret.caret_affinity();
  const internal::ShapedText* shaped_text = GetShapedText();
  for (size_t line_index = 0; line_index < shaped_text->lines().size();
       ++line_index) {
    const internal::Line& line = shaped_text->lines()[line_index];
    for (const internal::LineSegment& segment : line.segments) {
      if (RangeContainsCaret(segment.char_range, layout_position, affinity))
        return LineIndexForNewline(line_index, text(), segment, caret);
    }
  }

  return shaped_text->lines().size() - 1;
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
#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)
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
  if (line == GetShapedText()->lines().size() - 1 && direction == CURSOR_DOWN) {
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

void RenderTextHarfBuzz::OnLayoutTextAttributeChanged() {
  RenderText::OnLayoutTextAttributeChanged();

  update_layout_run_list_ = true;
  OnDisplayTextAttributeChanged();
}

void RenderTextHarfBuzz::OnDisplayTextAttributeChanged() {
  update_display_text_ = true;
  set_shaped_text(nullptr);
}

void RenderTextHarfBuzz::EnsureLayout() {
  EnsureLayoutRunList();

  if (update_display_run_list_) {
    DCHECK(text_elided());
    const std::u16string& display_text = GetDisplayText();
    display_run_list_ = std::make_unique<internal::TextRunList>();

    if (!display_text.empty())
      ItemizeAndShapeText(display_text, display_run_list_.get());
    update_display_run_list_ = false;
    set_shaped_text(nullptr);
  }

  if (!has_shaped_text()) {
    internal::TextRunList* run_list = GetRunList();
    const int height = std::max(font_list().GetHeight(), min_line_height());
    HarfBuzzLineBreaker line_breaker(
        display_rect().width(),
        DetermineBaselineCenteringText(height, font_list()), height,
        glyph_height_for_test_, word_wrap_behavior(), GetDisplayText(),
        *run_list);

    if (multiline())
      line_breaker.ConstructMultiLines();
    else
      line_breaker.ConstructSingleLine();
    std::vector<internal::Line> lines;
    line_breaker.FinalizeLines(&lines, &total_size_);
    // In multiline, only ELIDE_TAIL is supported. max_lines_ is not used
    // otherwise.
    if (multiline() && max_lines() && elide_behavior() == ELIDE_TAIL) {
      // TODO(crbug.com/40586307): no more than max_lines() should be rendered.
      // Remove the IsHomogeneous() condition for the following DCHECK when the
      // bug is fixed.
      if (IsHomogeneous()) {
        DCHECK_LE(lines.size(), max_lines());
      }
    }

    set_shaped_text(std::make_unique<internal::ShapedText>(lines));
  }
}

void RenderTextHarfBuzz::DrawVisualText(internal::SkiaTextRenderer* renderer,
                                        const std::vector<Range>& selections) {
  DCHECK(!update_layout_run_list_);
  DCHECK(!update_display_run_list_);
  DCHECK(!update_display_text_);

  const internal::ShapedText* shaped_text = GetShapedText();
  if (shaped_text->lines().empty())
    return;

  ApplyFadeEffects(renderer);
  ApplyTextShadows(renderer);

  // Apply the selected text color to the [un-reversed] selection range.
  BreakList<SkColor> colors = layout_colors();
  for (auto selection : selections) {
    if (!selection.is_empty()) {
      const Range grapheme_range = ExpandRangeToGraphemeBoundary(selection);
      colors.ApplyValue(
          selection_color(),
          Range(TextIndexToDisplayIndex(grapheme_range.GetMin()),
                TextIndexToDisplayIndex(grapheme_range.GetMax())));
    }
  }

  internal::TextRunList* run_list = GetRunList();
  const std::u16string& display_text = GetDisplayText();
  for (size_t i = 0; i < shaped_text->lines().size(); ++i) {
    const internal::Line& line = shaped_text->lines()[i];
    const Vector2d origin = GetLineOffset(i) + Vector2d(0, line.baseline);
    SkScalar preceding_segment_widths = 0;
    for (const internal::LineSegment& segment : line.segments) {
      // Don't draw the newline glyph (crbug.com/680430).
      if (IsNewlineSegment(display_text, segment))
        continue;

      const size_t crash_report_size = 256;
      DEBUG_ALIAS_FOR_U16CSTR(alias_display_text, display_text.c_str(),
                              crash_report_size);
      DEBUG_ALIAS_FOR_U16CSTR(alias_text, text().c_str(), crash_report_size);
      const size_t run_list_size = run_list->runs().size();
      base::debug::Alias(&run_list_size);
      const size_t segment_run_size = segment.run;
      base::debug::Alias(&segment_run_size);

      const internal::TextRunHarfBuzz& run = *run_list->runs()[segment.run];
      renderer->SetFillStyle(run.font_params.fill_style);
      renderer->SetStrokeWidth(run.font_params.stroke_width);
      renderer->SetTypeface(run.font_params.skia_face);
      renderer->SetTextSize(SkIntToScalar(run.font_params.font_size));
      renderer->SetFontRenderParams(run.font_params.render_params,
                                    subpixel_rendering_suppressed());
      const Range glyphs_range = run.CharRangeToGlyphRange(segment.char_range);
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
      for (auto it = colors.GetBreak(segment.char_range.start());
           it != colors.breaks().end() && it->first < segment.char_range.end();
           ++it) {
        const Range intersection =
            colors.GetRange(it).Intersect(segment.char_range);
        const Range colored_glyphs = run.CharRangeToGlyphRange(intersection);
        // The range may be empty if a portion of a multi-character grapheme is
        // selected, yielding two colors for a single glyph. For now, this just
        // paints the glyph with a single style, but it should paint it twice,
        // clipped according to selection bounds. See http://crbug.com/366786
        if (colored_glyphs.is_empty())
          continue;

        const size_t colored_pos =
            colored_glyphs.start() - glyphs_range.start();
        const int pos_size = positions.size();
        base::debug::Alias(&colored_pos);
        base::debug::Alias(&pos_size);

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

bool RenderTextHarfBuzz::BuildResolvedTypefaceBreakList(
    internal::TextRunList* run_list) {
  bool modified_breaklist = false;
  const Font& primary_font = font_list().GetPrimaryFont();
  for (auto& run : run_list->runs()) {
    if (run->CountMissingGlyphs() > 0) {
      for (size_t offset = 0; offset < run->shape.glyphs.size(); ++offset) {
        constexpr uint16_t kMissingGlyphId = 0;
        if (run->shape.glyphs[offset] == kMissingGlyphId) {
          // Retrieve the whole grapheme that contains the missing glyphs.
          size_t layout_text_offset = run->shape.glyph_to_char[offset];
          size_t text_offset = DisplayIndexToTextIndex(layout_text_offset);
          gfx::Range grapheme_range(ExpandRangeToGraphemeBoundary(
              gfx::Range(text_offset, text_offset + 1)));
          DCHECK(!grapheme_range.is_reversed());

          // The grapheme text is from layout_text_ after all rewriting. It is
          // the text that is used for shaping, so we need to convert our
          // indices back to display indices after expanding to the nearest
          // grapheme boundary.
          const Range display_range(
              TextIndexToDisplayIndex(grapheme_range.start()),
              TextIndexToDisplayIndex(grapheme_range.end()));

          // Determine the corresponding fallback font for the grapheme.
          const std::u16string_view grapheme_text(
              &GetLayoutText()[display_range.start()], display_range.length());

          Font fallback_font(primary_font);
          const bool fallback_found = GetFallbackFont(
              primary_font, locale_, grapheme_text, &fallback_font);

          // Only add to the breaklist if we found a fallback. This means that
          // adjacent tofu will not be in isolated runs.
          if (fallback_found) {
            const SkTypefaceID fallback_font_id = fallback_font.platform_font()
                                                      ->GetNativeSkTypeface()
                                                      ->uniqueID();
            if (layout_resolved_typefaces().ApplyValue(fallback_font_id,
                                                       display_range)) {
              modified_breaklist = true;
            }
          }
        }
      }
    }
  }
  return modified_breaklist;
}

void RenderTextHarfBuzz::ItemizeAndShapeText(const std::u16string& text,
                                             internal::TextRunList* run_list) {
  CommonizedRunsMap commonized_run_map;
  const bool successfully_shaped_runs =
      ItemizeAndShapeTextImpl(&commonized_run_map, text, run_list);

  // If we didn't successfully shape every run, break runs based on the resolved
  // typeface. This will ensure that missing glyphs are isolated to their own
  // runs, maximizing fallback opportunities.
  if (!successfully_shaped_runs && !ignore_missing_glyph_breaks_for_test_) {
    if (BuildResolvedTypefaceBreakList(run_list)) {
      ItemizeAndShapeTextImpl(&commonized_run_map, text, run_list);
    }

    // Resolved typefaces are no longer used and can be cleared.
    layout_resolved_typefaces().Reset();
    resolved_typefaces().Reset();
  }

  // Now that potentially two passes to ItemizeAndShapeTextImpl have occurred,
  // we can record the final type of fallback.
  EmitShapeRunsFallback();

  run_list->InitIndexMap();
  run_list->ComputePrecedingRunWidths();

  UMA_HISTOGRAM_COUNTS_1000("RenderTextHarfBuzz.MissingGlyphCount",
                            run_list->MissingGlyphCount());
}

bool RenderTextHarfBuzz::ItemizeAndShapeTextImpl(
    CommonizedRunsMap* commonized_run_map,
    const std::u16string& text,
    internal::TextRunList* run_list) {
  run_list->Reset();
  commonized_run_map->clear();

  ItemizeTextToRuns(text, run_list, commonized_run_map);
  bool successfully_shaped_runs = true;
  for (auto iter = commonized_run_map->begin();
       iter != commonized_run_map->end(); ++iter) {
    internal::TextRunHarfBuzz::FontParams font_params = iter->first;
    font_params.ComputeRenderParamsFontSizeAndBaselineOffset();
    successfully_shaped_runs &=
        ShapeRuns(text, font_params, std::move(iter->second));
  }
  return successfully_shaped_runs;
}

void RenderTextHarfBuzz::ItemizeTextToRuns(
    const std::u16string& text,
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

  if (!bidi_iterator.Open(text, GetTextDirectionForGivenText(text))) {
    auto run = std::make_unique<internal::TextRunHarfBuzz>(
        font_list().GetPrimaryFont());
    run->range = Range(0, text.length());
    internal::TextRunHarfBuzz::FontParams font_params(primary_font);
    (*out_commonized_run_map)[font_params].push_back(run.get());
    out_run_list->Add(std::move(run));
    return;
  }

  // Iterator to split ranged styles and baselines. The color attributes don't
  // break text runs to keep ligature between graphemes (e.g. Arabic word).
  internal::StyleIterator style = GetLayoutTextStyleIterator();

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
        // Find the break boundary for style. The style won't break a grapheme
        // since the style of the first character is applied to the whole
        // grapheme.
        style.IncrementToPosition(breaking_run_start);
        size_t text_style_end = style.GetTextBreakingRange().end();

        // Break runs at certain characters that need to be rendered separately
        // to prevent an unusual character from forcing a fallback font on the
        // entire run. After script intersection, many codepoints end up in the
        // script COMMON but can't be rendered together.
        size_t breaking_run_end = FindRunBreakingCharacter(
            text, script, breaking_run_start, text_style_end, script_run_end);

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
}

bool RenderTextHarfBuzz::ShapeRuns(
    const std::u16string& text,
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
    if ((run->range.length() == 1 && (text[run->range.start()] == '\r' ||
                                      text[run->range.start()] == '\n')) ||
        (run->range.length() == 2 && text[run->range.start()] == '\r' &&
         text[run->range.start() + 1] == '\n')) {
      // Newline runs can't be shaped. Shape this run as if the glyph is
      // missing.
      run->font_params = font_params;
      run->shape.missing_glyph_count = 1;
      run->shape.glyph_count = 1;
      run->shape.glyphs.resize(run->shape.glyph_count);
      run->shape.glyph_to_char.resize(run->shape.glyph_count);
      run->shape.positions.resize(run->shape.glyph_count);
      // Keep width as zero since newline character doesn't have a width.
    } else {
      // This run needs shaping.
      need_shaping_runs.push_back(run);
    }
  }
  runs.swap(need_shaping_runs);
  if (runs.empty()) {
    RecordShapeRunsFallback(internal::ShapeRunFallback::NO_FALLBACK);
    return true;
  }

  // Keep a set of fonts already tried for shaping runs.
  std::set<SkTypefaceID> fallback_fonts_already_tried;
  std::vector<Font> fallback_font_candidates;

  // Shaping with primary configured fonts from font_list().
  for (const Font& font : font_list().GetFonts()) {
    internal::TextRunHarfBuzz::FontParams test_font_params = font_params;
    if (test_font_params.SetRenderParamsRematchFont(
            font, font.GetFontRenderParams()) &&
        !FontWasAlreadyTried(test_font_params.skia_face,
                             &fallback_fonts_already_tried)) {
      ShapeRunsWithFont(text, test_font_params, &runs);
      MarkFontAsTried(test_font_params.skia_face,
                      &fallback_fonts_already_tried);
      fallback_font_candidates.push_back(font);
    }
    if (runs.empty()) {
      RecordShapeRunsFallback(internal::ShapeRunFallback::NO_FALLBACK);
      return true;
    }
  }

  const Font& primary_font = font_list().GetPrimaryFont();

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
      const std::u16string_view run_text(&text[current_run->range.start()],
                                         current_run->range.length());
      fallback_found =
          GetFallbackFont(primary_font, locale_, run_text, &fallback_font);
    }

    if (fallback_found) {
      internal::TextRunHarfBuzz::FontParams test_font_params = font_params;
      if (test_font_params.SetRenderParamsOverrideSkiaFaceFromFont(
              fallback_font, fallback_font.GetFontRenderParams()) &&
          !FontWasAlreadyTried(test_font_params.skia_face,
                               &fallback_fonts_already_tried)) {
        ShapeRunsWithFont(text, test_font_params, &runs);
        MarkFontAsTried(test_font_params.skia_face,
                        &fallback_fonts_already_tried);
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
    RecordShapeRunsFallback(internal::ShapeRunFallback::FALLBACK);
    return true;
  }

  if (!IsRemoveFontLinkFallbacks()) {
    // Used for crash reporting below.
    static bool is_first_crash = true;

    std::vector<Font> fallback_font_list;
    std::u16string crash_report_string;
    {
      SCOPED_UMA_HISTOGRAM_LONG_TIMER(
          "RenderTextHarfBuzz.GetFallbackFontsTime");
      TRACE_EVENT1("ui", "RenderTextHarfBuzz::GetFallbackFonts", "script",
                   TRACE_STR_COPY(uscript_getShortName(font_params.script)));
      fallback_font_list = GetFallbackFonts(primary_font);

#if BUILDFLAG(IS_WIN)
      // Append fonts in the fallback list of the fallback fonts.
      // TODO(tapted): Investigate whether there's a case that benefits from
      // this on Mac.
      for (const auto& fallback_font : fallback_font_candidates) {
        std::vector<Font> fallback_fonts = GetFallbackFonts(fallback_font);
        fallback_font_list.insert(fallback_font_list.end(),
                                  fallback_fonts.begin(), fallback_fonts.end());
      }

      // Add Segoe UI and its associated linked fonts to the fallback font list
      // to ensure that the fallback list covers the basic cases.
      // http://crbug.com/467459. On some Windows configurations the default
      // font could be a raster font like System, which would not give us a
      // reasonable fallback font list.
      Font segoe("Segoe UI", 13);
      if (!FontWasAlreadyTried(segoe.platform_font()->GetNativeSkTypeface(),
                               &fallback_fonts_already_tried)) {
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

      FontRenderParamsQuery query;
      query.families.push_back(font_name);
      query.pixel_size = font_params.font_size;
      query.style = font_params.italic ? Font::ITALIC : 0;
      FontRenderParams fallback_render_params =
          GetFontRenderParams(query, nullptr);
      internal::TextRunHarfBuzz::FontParams test_font_params = font_params;
      std::vector<internal::TextRunHarfBuzz*> fallback_fonts_shaped_runs;
      if (test_font_params.SetRenderParamsOverrideSkiaFaceFromFont(
              font, fallback_render_params) &&
          !FontWasAlreadyTried(test_font_params.skia_face,
                               &fallback_fonts_already_tried)) {
        ShapeRunsWithFont(text, test_font_params, &runs,
                          &fallback_fonts_shaped_runs);
        MarkFontAsTried(test_font_params.skia_face,
                        &fallback_fonts_already_tried);
        if (fallback_fonts_shaped_runs.size() > 0 && is_first_crash &&
            IsEnableFallbackFontsCrashReporting()) {
          AppendFontNameAndShapedTextToCrashDumpReport(
              text, fallback_fonts_shaped_runs, font_name, crash_report_string);
        }
      }
      if (runs.empty()) {
        TRACE_EVENT_INSTANT2("ui", "RenderTextHarfBuzz::FallbackFont",
                             TRACE_EVENT_SCOPE_THREAD, "font_name",
                             TRACE_STR_COPY(font_name.c_str()),
                             "primary_font_name", primary_font.GetFontName());
        RecordShapeRunsFallback(internal::ShapeRunFallback::FALLBACKS);
        // Resolving fallback fonts using the registry keys on windows will be
        // deprecated and removed (see: http://crbug.com/995789). The crashes
        // reported here should be fixed before deprecating the code.
        if (is_first_crash && IsEnableFallbackFontsCrashReporting()) {
          is_first_crash = false;
          const size_t crash_report_size = 256;
          DEBUG_ALIAS_FOR_U16CSTR(aliased_crash_report_string,
                                  crash_report_string.c_str(),
                                  crash_report_size);
          DEBUG_ALIAS_FOR_U16CSTR(aliased_full_text, text.c_str(),
                                  crash_report_size);
          SCOPED_CRASH_KEY_STRING32("RenderTextFallbacks", "primaryfont_name",
                                    primary_font.GetFontName());
          SCOPED_CRASH_KEY_STRING32("RenderTextFallbacks", "primaryfont_script",
                                    uscript_getShortName(font_params.script));
          base::debug::DumpWithoutCrashing();
        }
        return true;
      }
    }
  }

  for (internal::TextRunHarfBuzz*& run : runs) {
    if (run->shape.missing_glyph_count == std::numeric_limits<size_t>::max()) {
      run->shape.glyph_count = 0;
      run->shape.width = 0.0f;
    }
  }

  RecordShapeRunsFallback(internal::ShapeRunFallback::FAILED);
  return false;
}

void RenderTextHarfBuzz::ShapeRunsWithFont(
    const std::u16string& text,
    const internal::TextRunHarfBuzz::FontParams& font_params,
    std::vector<internal::TextRunHarfBuzz*>* in_out_runs,
    std::vector<internal::TextRunHarfBuzz*>* successfully_shaped_runs) {
  // ShapeRunWithFont can be extremely slow, so use cached results if
  // possible. Only do this on the UI thread, to avoid synchronization
  // overhead (and because almost all calls are on the UI thread). Also avoid
  // caching long strings, to avoid blowing up the cache size.
  constexpr size_t kMaxRunLengthToCache = 25;
  static base::NoDestructor<internal::ShapeRunCache> cache;

  std::vector<internal::TextRunHarfBuzz*> runs_with_missing_glyphs;
  for (internal::TextRunHarfBuzz*& run : *in_out_runs) {
    // First do a cache lookup.
    bool can_use_cache = base::CurrentUIThread::IsSet() &&
                         run->range.length() <= kMaxRunLengthToCache;
    bool found_in_cache = false;
    const internal::ShapeRunWithFontInput cache_key(
        text, font_params, run->range, obscured(), glyph_width_for_test_,
        obscured_glyph_spacing(), subpixel_rendering_suppressed());
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
    if (run->shape.missing_glyph_count) {
      runs_with_missing_glyphs.push_back(run);
    } else if (successfully_shaped_runs) {
      successfully_shaped_runs->push_back(run);
    }
  }
  in_out_runs->swap(runs_with_missing_glyphs);
}

void RenderTextHarfBuzz::EnsureLayoutRunList() {
  // Update layout run list if the device scale factor has changed since the
  // layout run list was last updated, as changes in device scale factor change
  // subpixel positioning, at least on Linux and Chrome OS.
  const float device_scale_factor = GetFontRenderParamsDeviceScaleFactor();

  if (update_layout_run_list_ || device_scale_factor_ != device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
    layout_run_list_.Reset();

    const std::u16string& text = GetLayoutText();
    if (!text.empty()) {
      ItemizeAndShapeText(text, &layout_run_list_);
    }

    display_run_list_.reset();
    update_display_text_ = true;
    update_layout_run_list_ = false;
  }
  if (update_display_text_) {
    set_shaped_text(nullptr);
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

bool RenderTextHarfBuzz::IsValidDisplayRange(Range display_range) {
  // The |display_text_| is an elided version of |layout_text_|. Removing
  // codepoints from the text may break the conversion for codepoint offsets
  // between text to display_text offset. For elding behaviors that truncate
  // codepoint at the end, the conversion will work just fine. But for eliding
  // behavior that truncate at the beginning of middle of the text, the offsets
  // are completely wrong and should not be used.
  // TODO(http://crbug.com/1085014): Fix eliding for the broken cases.
  switch (elide_behavior()) {
    case NO_ELIDE:
    case FADE_TAIL:
      return display_range.IsBoundedBy(Range(0, GetDisplayText().length()));
    case TRUNCATE:
    case ELIDE_TAIL:
      return display_range.IsBoundedBy(Range(0, GetLayoutText().length()));
    case ELIDE_HEAD:
    case ELIDE_MIDDLE:
    case ELIDE_EMAIL:
      return !text_elided();
  }
}

void RenderTextHarfBuzz::RecordShapeRunsFallback(
    internal::ShapeRunFallback fallback) {
  last_shape_run_metric_.emplace(fallback);
}

void RenderTextHarfBuzz::EmitShapeRunsFallback() {
  if (last_shape_run_metric_.has_value()) {
    UMA_HISTOGRAM_ENUMERATION("RenderTextHarfBuzz.ShapeRunsFallback",
                              last_shape_run_metric_.value());
  }
}

void RenderTextHarfBuzz::GetDecoratedTextForRange(
    const Range& text_range,
    DecoratedText* decorated_text) {
  EnsureLayout();

  decorated_text->attributes.clear();
  decorated_text->text = GetTextFromRange(text_range);

  // The range on the runs below is in display offsets, not logical offsets.
  // This means we need to convert the text range to a display range before
  // running the intersection logic below, or else we won't get the attributes
  // for the obscured grapheme composed of multiple codepoints.
  const Range display_range(TextIndexToDisplayIndex(text_range.start()),
                            TextIndexToDisplayIndex(text_range.end()));

  const internal::TextRunList* run_list = GetRunList();
  for (size_t i = 0; i < run_list->size(); i++) {
    const internal::TextRunHarfBuzz& run = *run_list->runs()[i];

    const Range intersection = display_range.Intersect(run.range);
    DCHECK(!intersection.is_reversed());

    if (!intersection.is_empty()) {
      int style = Font::NORMAL;
      if (run.font_params.italic)
        style |= Font::ITALIC;
      if (run.font_params.underline || run.font_params.heavy_underline)
        style |= Font::UNDERLINE;
      if (run.font_params.strike) {
        style |= Font::STRIKE_THROUGH;
      }

      // Get range relative to the decorated text in logical offsets. The
      // `intersection` is in display offsets but logical text offsets are
      // expected in the range attribute of `DecoratedText::RangedAttribute`.
      Range intersection_text_range =
          Range(DisplayIndexToTextIndex(intersection.start()),
                DisplayIndexToTextIndex(intersection.end()));
      DecoratedText::RangedAttribute attribute(
          Range(intersection_text_range.start() - text_range.GetMin(),
                intersection_text_range.end() - text_range.GetMin()),
          run.font_params.font.Derive(0, style, run.font_params.weight));

      attribute.strike = run.font_params.strike;
      decorated_text->attributes.push_back(attribute);
    }
  }
}

}  // namespace gfx
