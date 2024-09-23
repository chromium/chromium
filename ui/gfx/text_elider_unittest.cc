// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for eliding and formatting utility functions.

#include "ui/gfx/text_elider.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/text_utils.h"

namespace gfx {

namespace {

struct StringMappingTestCase {
  const std::u16string input;
  const std::u16string output;
};

}  // namespace

TEST(TextEliderTest, ElideEmail) {
  // Test emails and their expected elided forms (from which the available
  // widths will be derived).
  // For elided forms in which both the username and domain must be elided:
  // the result (how many characters are left on each side) can be font
  // dependent. To avoid this, the username is prefixed with the characters
  // expected to remain in the domain.
  const auto cases = std::to_array<StringMappingTestCase>({
      {u"g@g.c", u"g@g.c"},
      {u"g@g.c", u"…"},
      {u"ga@co.ca", u"ga@c…a"},
      {u"short@small.com", u"s…@s…"},
      {u"short@small.com", u"s…@small.com"},
      {u"short@longbutlotsofspace.com", u"short@longbutlotsofspace.com"},
      {u"short@longbutnotverymuchspace.com", u"short@long….com"},
      {u"la_short@longbutverytightspace.ca", u"la…@l…a"},
      {u"longusername@gmail.com", u"long…@gmail.com"},
      {u"elidetothemax@justfits.com", u"e…@justfits.com"},
      {u"thatom_somelongemail@thatdoesntfit.com", u"thatom…@tha…om"},
      {u"namefits@butthedomaindoesnt.com", u"namefits@butthedo…snt.com"},
      {u"widthtootight@nospace.com", u"…"},
      {u"nospaceforusername@l", u"…"},
      {u"little@littlespace.com", u"l…@l…"},
      {u"l@llllllllllllllllllllllll.com", u"l@lllll….com"},
      {u"messed\"up@whyanat\"++@notgoogley.com",
       u"messed\"up@whyanat\"++@notgoogley.com"},
      {u"messed\"up@whyanat\"++@notgoogley.com",
       u"messed\"up@why…@notgoogley.com"},
      {u"noca_messed\"up@whyanat\"++@notgoogley.ca", u"noca…@no…ca"},
      {u"at\"@@@@@@@@@...@@.@.@.@@@\"@madness.com",
       u"at\"@@@@@@@@@...@@.@.…@madness.com"},
      // Special case: "m..." takes more than half of the available width; thus
      // the domain must elide to "l..." and not "l...l" as it must allow enough
      // space for the minimal username elision although its half of the
      // available width would normally allow it to elide to "l...l".
      {u"mmmmm@llllllllll", u"m…@l…"},
  });

  const FontList font_list;
  for (const auto& c : cases) {
    EXPECT_EQ(c.output,
              ElideText(c.input, font_list,
                        GetStringWidthF(c.output, font_list), ELIDE_EMAIL));
  }
}

TEST(TextEliderTest, ElideEmailMoreSpace) {
  const auto test_widths_extra_spaces = std::to_array<int>({
      10,
      1000,
      100'000,
  });

  const auto test_emails = std::to_array<const char16_t*>({
      u"a@c",
      u"test@email.com",
      u"short@verysuperdupperlongdomain.com",
      u"supermegalongusername@withasuperlonnnggggdomain.gouv.qc.ca",
  });

  const FontList font_list;
  for (const auto* test_email : test_emails) {
    const int mimimum_width = GetStringWidth(test_email, font_list);
    for (int extra_space : test_widths_extra_spaces) {
      // Extra space is available: the email should not be elided.
      EXPECT_EQ(test_email,
                ElideText(test_email, font_list, mimimum_width + extra_space,
                          ELIDE_EMAIL));
    }
  }
}

TEST(TextEliderTest, TestFilenameEliding) {
  const base::FilePath::StringType kPathSeparator =
      base::FilePath::StringType().append(1, base::FilePath::kSeparators[0]);

  struct Case {
    const base::FilePath::StringType input;
    const std::u16string output;

    // If this value is specified, we will try to cut the path down to the
    // render width of this string; if not specified, output will be used.
    const std::u16string using_width_of = std::u16string();
  };

  const auto cases = std::to_array<Case>(
      {{FILE_PATH_LITERAL(""), u""},
       {FILE_PATH_LITERAL("."), u"."},
       {FILE_PATH_LITERAL("filename.exe"), u"filename.exe"},
       {FILE_PATH_LITERAL(".longext"), u".longext"},
       {FILE_PATH_LITERAL("pie"), u"pie"},
       {FILE_PATH_LITERAL("c:") + kPathSeparator + FILE_PATH_LITERAL("path") +
            kPathSeparator + FILE_PATH_LITERAL("filename.pie"),
        u"filename.pie"},
       {FILE_PATH_LITERAL("c:") + kPathSeparator + FILE_PATH_LITERAL("path") +
            kPathSeparator + FILE_PATH_LITERAL("longfilename.pie"),
        u"long….pie"},
       {FILE_PATH_LITERAL("http://path.com/filename.pie"), u"filename.pie"},
       {FILE_PATH_LITERAL("http://path.com/longfilename.pie"), u"long….pie"},
       {FILE_PATH_LITERAL("piesmashingtacularpants"), u"pie…"},
       {FILE_PATH_LITERAL(".piesmashingtacularpants"), u".pie…"},
       {FILE_PATH_LITERAL("cheese."), u"cheese."},
       {FILE_PATH_LITERAL("file name.longext"), u"file….longext"},
       {FILE_PATH_LITERAL("fil ename.longext"), u"fil….longext",
        u"fil ….longext"},
       {FILE_PATH_LITERAL("filename.longext"), u"file….longext"},
       {FILE_PATH_LITERAL("filename.middleext.longext"),
        u"filename.mid….longext"},
       {FILE_PATH_LITERAL("filename.superduperextremelylongext"),
        u"filename.sup…emelylongext"},
       {FILE_PATH_LITERAL(
            "filenamereallylongtext.superdeduperextremelylongext"),
        u"filenamereall…emelylongext"},
       {FILE_PATH_LITERAL(
            "file.name.really.long.text.superduperextremelylongext"),
        u"file.name.re…emelylongext"}});

  static const FontList font_list;
  for (const auto& c : cases) {
    std::u16string using_width_of =
        c.using_width_of.empty() ? c.output : c.using_width_of;
    EXPECT_EQ(base::i18n::GetDisplayStringInLTRDirectionality(c.output),
              ElideFilename(base::FilePath(c.input), font_list,
                            GetStringWidthF(using_width_of, font_list)));
  }
}

TEST(TextEliderTest, ElideTextTruncate) {
  const FontList font_list;
  const float kTestWidth = GetStringWidthF(u"Test", font_list);
  struct Case {
    const char16_t* input;
    float width;
    const char16_t* output;
  };
  const auto cases = std::to_array<Case>({
      {u"", 0, u""},
      {u"Test", 0, u""},
      {u"", kTestWidth, u""},
      {u"Tes", kTestWidth, u"Tes"},
      {u"Test", kTestWidth, u"Test"},
      {u"Tests", kTestWidth, u"Test"},
  });

  for (const auto& c : cases) {
    EXPECT_EQ(c.output, ElideText(c.input, font_list, c.width, TRUNCATE));
  }
}

TEST(TextEliderTest, ElideTextEllipsis) {
  const FontList font_list;
  const float kTestWidth = GetStringWidthF(u"Test", font_list);
  const float kEllipsisWidth = GetStringWidthF(u"…", font_list);
  struct Case {
    const char16_t* input;
    float width;
    const char16_t* output;
  };
  const auto cases = std::to_array<Case>({
      {u"", 0, u""},
      {u"Test", 0, u""},
      {u"Test", kEllipsisWidth, u"…"},
      {u"", kTestWidth, u""},
      {u"Tes", kTestWidth, u"Tes"},
      {u"Test", kTestWidth, u"Test"},
  });

  for (const auto& c : cases) {
    EXPECT_EQ(c.output, ElideText(c.input, font_list, c.width, ELIDE_TAIL));
  }
}

TEST(TextEliderTest, ElideTextEllipsisFront) {
  const FontList font_list;
  const float kTestWidth = GetStringWidthF(u"Test", font_list);
  const float kEllipsisWidth = GetStringWidthF(u"…", font_list);
  const float kEllipsis23Width = GetStringWidthF(u"…23", font_list);
  struct Case {
    const char16_t* input;
    float width;
    const std::u16string output;
  };
  const auto cases = std::to_array<Case>({
      {u"", 0, std::u16string()},
      {u"Test", 0, std::u16string()},
      {u"Test", kEllipsisWidth, u"…"},
      {u"", kTestWidth, std::u16string()},
      {u"Tes", kTestWidth, u"Tes"},
      {u"Test", kTestWidth, u"Test"},
      {u"Test123", kEllipsis23Width, u"…23"},
  });

  for (const auto& c : cases) {
    EXPECT_EQ(c.output, ElideText(c.input, font_list, c.width, ELIDE_HEAD));
  }
}

// Checks that all occurrences of |first_char| are followed by |second_char| and
// all occurrences of |second_char| are preceded by |first_char| in |text|. Can
// be used to test surrogate pairs or two-character combining sequences.
static void CheckCodeUnitPairs(const std::u16string& text,
                               char16_t first_char,
                               char16_t second_char) {
  for (size_t index = 0; index < text.length(); ++index) {
    EXPECT_NE(second_char, text[index]);
    if (text[index] == first_char) {
      ASSERT_LT(++index, text.length());
      EXPECT_EQ(second_char, text[index]);
    }
  }
}

// Test that both both UTF-16 surrogate pairs and combining character sequences
// do not get split by ElideText.
TEST(TextEliderTest, ElideTextAtomicSequences) {
#if BUILDFLAG(IS_WIN)
  // Needed to bypass DCHECK in GetFallbackFont.
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
#endif
  const FontList font_list;
  std::vector<std::u16string> pairs;
  // The below is 'MUSICAL SYMBOL G CLEF' (U+1D11E), which is represented in
  // UTF-16 as two code units forming a surrogate pair: 0xD834 0xDD1E.
  pairs.push_back(u"\U0001d11e");
  // The below is a Devanagari two-character combining sequence U+0921 U+093F.
  // The sequence forms a single display character and should not be separated.
  pairs.push_back(u"\u0921\u093f");

  for (const std::u16string& pair : pairs) {
    char16_t first_char = pair[0];
    char16_t second_char = pair[1];
    std::u16string test_string = pair + u"x" + pair;
    SCOPED_TRACE(test_string);
    const float test_string_width = GetStringWidthF(test_string, font_list);
    std::u16string result;

    // Elide |text_string| to all possible widths and check that no instance of
    // |pair| was split in two.
    for (float width = 0; width <= test_string_width; width++) {
      result = ElideText(test_string, font_list, width, TRUNCATE);
      CheckCodeUnitPairs(result, first_char, second_char);

      result = ElideText(test_string, font_list, width, ELIDE_TAIL);
      CheckCodeUnitPairs(result, first_char, second_char);

      result = ElideText(test_string, font_list, width, ELIDE_MIDDLE);
      CheckCodeUnitPairs(result, first_char, second_char);

      result = ElideText(test_string, font_list, width, ELIDE_HEAD);
      CheckCodeUnitPairs(result, first_char, second_char);
    }
  }
}

TEST(TextEliderTest, ElideTextLongStrings) {
  std::u16string data_scheme(u"data:text/plain,");
  size_t data_scheme_length = data_scheme.length();

  std::u16string ten_a(10, 'a');
  std::u16string hundred_a(100, 'a');
  std::u16string thousand_a(1000, 'a');
  std::u16string ten_thousand_a(10'000, 'a');
  std::u16string hundred_thousand_a(100'000, 'a');
  std::u16string million_a(1'000'000, 'a');

  // TODO(gbillock): Improve these tests by adding more string diversity and
  // doing string compares instead of length compares. See bug 338836.

  size_t number_of_as = 156;
  std::u16string long_string_end(data_scheme +
                                 std::u16string(number_of_as, 'a') + u"…");
  const auto cases_end = std::to_array<StringMappingTestCase>({
      {data_scheme + ten_a, data_scheme + ten_a},
      {data_scheme + hundred_a, data_scheme + hundred_a},
      {data_scheme + thousand_a, long_string_end},
      {data_scheme + ten_thousand_a, long_string_end},
      {data_scheme + hundred_thousand_a, long_string_end},
      {data_scheme + million_a, long_string_end},
  });

  const FontList font_list;
  float ellipsis_width = GetStringWidthF(u"…", font_list);
  for (const auto& c : cases_end) {
    // Compare sizes rather than actual contents because if the test fails,
    // output is rather long.
    EXPECT_EQ(c.output.size(),
              ElideText(c.input, font_list,
                        GetStringWidthF(c.output, font_list), ELIDE_TAIL)
                  .size());
    EXPECT_EQ(u"…", ElideText(c.input, font_list, ellipsis_width, ELIDE_TAIL));
  }

  size_t number_of_trailing_as = (data_scheme_length + number_of_as) / 2;
  std::u16string long_string_middle(
      data_scheme + std::u16string(number_of_as - number_of_trailing_as, 'a') +
      u"…" + std::u16string(number_of_trailing_as, 'a'));
#if !BUILDFLAG(IS_IOS)
  long_string_middle += u"…";
#endif

  const auto cases_middle = std::to_array<StringMappingTestCase>({
      {data_scheme + ten_a, data_scheme + ten_a},
      {data_scheme + hundred_a, data_scheme + hundred_a},
      {data_scheme + thousand_a, long_string_middle},
      {data_scheme + ten_thousand_a, long_string_middle},
      {data_scheme + hundred_thousand_a, long_string_middle},
      {data_scheme + million_a, long_string_middle},
  });

  for (const auto& c : cases_middle) {
    // Compare sizes rather than actual contents because if the test fails,
    // output is rather long.
    EXPECT_EQ(c.output.size(),
              ElideText(c.input, font_list,
                        GetStringWidthF(c.output, font_list), ELIDE_MIDDLE)
                  .size());
    EXPECT_EQ(u"…",
              ElideText(c.input, font_list, ellipsis_width, ELIDE_MIDDLE));
  }

  std::u16string long_string_beginning(u"…" +
                                       std::u16string(number_of_as, 'a'));
#if !BUILDFLAG(IS_IOS)
  long_string_beginning += u"…";
#endif

  const auto cases_beginning = std::to_array<StringMappingTestCase>({
      {data_scheme + ten_a, data_scheme + ten_a},
      {data_scheme + hundred_a, data_scheme + hundred_a},
      {data_scheme + thousand_a, long_string_beginning},
      {data_scheme + ten_thousand_a, long_string_beginning},
      {data_scheme + hundred_thousand_a, long_string_beginning},
      {data_scheme + million_a, long_string_beginning},
  });
  for (const auto& c : cases_beginning) {
    EXPECT_EQ(c.output.size(),
              ElideText(c.input, font_list,
                        GetStringWidthF(c.output, font_list), ELIDE_HEAD)
                  .size());
    EXPECT_EQ(u"…", ElideText(c.input, font_list, ellipsis_width, ELIDE_HEAD));
  }
}

// Detailed tests for StringSlicer. These are faster and test more of the edge
// cases than the above tests which are more end-to-end.

TEST(TextEliderTest, StringSlicerBasicTest) {
  // Must store strings in variables (StringSlicer retains a reference to them).
  std::u16string text(u"Hello, world!");
  std::u16string ellipsis(u"…");
  StringSlicer slicer(text, ellipsis, false, false);

  EXPECT_EQ(u"", slicer.CutString(0, false));
  EXPECT_EQ(u"…", slicer.CutString(0, true));

  EXPECT_EQ(u"Hell", slicer.CutString(4, false));
  EXPECT_EQ(u"Hell…", slicer.CutString(4, true));

  EXPECT_EQ(text, slicer.CutString(text.length(), false));
  EXPECT_EQ(text + u"…", slicer.CutString(text.length(), true));

  StringSlicer slicer_begin(text, ellipsis, false, true);
  EXPECT_EQ(u"rld!", slicer_begin.CutString(4, false));
  EXPECT_EQ(u"…rld!", slicer_begin.CutString(4, true));

  StringSlicer slicer_mid(text, ellipsis, true, false);
  EXPECT_EQ(u"Held!", slicer_mid.CutString(5, false));
  EXPECT_EQ(u"Hel…d!", slicer_mid.CutString(5, true));
}

TEST(TextEliderTest, StringSlicerWhitespace_UseDefault) {
  // Must store strings in variables (StringSlicer retains a reference to them).
  std::u16string text(u"Hello, world!");
  std::u16string ellipsis(u"…");

  // Eliding the end of a string should result in whitespace being removed
  // before the ellipsis by default.
  StringSlicer slicer_end(text, ellipsis, false, false);
  EXPECT_EQ(u"Hello,…", slicer_end.CutString(6, true));
  EXPECT_EQ(u"Hello,…", slicer_end.CutString(7, true));
  EXPECT_EQ(u"Hello, w…", slicer_end.CutString(8, true));

  // Eliding the start of a string should result in whitespace being removed
  // after the ellipsis by default.
  StringSlicer slicer_begin(text, ellipsis, false, true);
  EXPECT_EQ(u"…world!", slicer_begin.CutString(6, true));
  EXPECT_EQ(u"…world!", slicer_begin.CutString(7, true));
  EXPECT_EQ(u"…, world!", slicer_begin.CutString(8, true));

  // Eliding the middle of a string should *NOT* result in whitespace being
  // removed around the ellipsis by default.
  StringSlicer slicer_mid(text, ellipsis, true, false);
  text = u"Hey world!";
  EXPECT_EQ(u"Hey…ld!", slicer_mid.CutString(6, true));
  EXPECT_EQ(u"Hey …ld!", slicer_mid.CutString(7, true));
  EXPECT_EQ(u"Hey …rld!", slicer_mid.CutString(8, true));
}

TEST(TextEliderTest, StringSlicerWhitespace_NoTrim) {
  // Must store strings in variables (StringSlicer retains a reference to them).
  std::u16string text(u"Hello, world!");
  std::u16string ellipsis(u"…");

  // Eliding the end of a string should not result in whitespace being removed
  // before the ellipsis in no-trim mode.
  StringSlicer slicer_end(text, ellipsis, false, false, false);
  EXPECT_EQ(u"Hello,…", slicer_end.CutString(6, true));
  EXPECT_EQ(u"Hello, …", slicer_end.CutString(7, true));
  EXPECT_EQ(u"Hello, w…", slicer_end.CutString(8, true));

  // Eliding the start of a string should not result in whitespace being removed
  // after the ellipsis in no-trim mode.
  StringSlicer slicer_begin(text, ellipsis, false, true, false);
  EXPECT_EQ(u"…world!", slicer_begin.CutString(6, true));
  EXPECT_EQ(u"… world!", slicer_begin.CutString(7, true));
  EXPECT_EQ(u"…, world!", slicer_begin.CutString(8, true));

  // Eliding the middle of a string should *NOT* result in whitespace being
  // removed around the ellipsis in no-trim mode.
  StringSlicer slicer_mid(text, ellipsis, true, false, false);
  text = u"Hey world!";
  EXPECT_EQ(u"Hey…ld!", slicer_mid.CutString(6, true));
  EXPECT_EQ(u"Hey …ld!", slicer_mid.CutString(7, true));
  EXPECT_EQ(u"Hey …rld!", slicer_mid.CutString(8, true));
}

TEST(TextEliderTest, StringSlicerWhitespace_Trim) {
  // Must store strings in variables (StringSlicer retains a reference to them).
  std::u16string text(u"Hello, world!");
  std::u16string ellipsis(u"…");

  // Eliding the end of a string should result in whitespace being removed
  // before the ellipsis in trim mode.
  StringSlicer slicer_end(text, ellipsis, false, false, true);
  EXPECT_EQ(u"Hello,…", slicer_end.CutString(6, true));
  EXPECT_EQ(u"Hello,…", slicer_end.CutString(7, true));
  EXPECT_EQ(u"Hello, w…", slicer_end.CutString(8, true));

  // Eliding the start of a string should result in whitespace being removed
  // after the ellipsis in trim mode.
  StringSlicer slicer_begin(text, ellipsis, false, true, true);
  EXPECT_EQ(u"…world!", slicer_begin.CutString(6, true));
  EXPECT_EQ(u"…world!", slicer_begin.CutString(7, true));
  EXPECT_EQ(u"…, world!", slicer_begin.CutString(8, true));

  // Eliding the middle of a string *should* result in whitespace being removed
  // around the ellipsis in trim mode.
  StringSlicer slicer_mid(text, ellipsis, true, false, true);
  text = u"Hey world!";
  EXPECT_EQ(u"Hey…ld!", slicer_mid.CutString(6, true));
  EXPECT_EQ(u"Hey…ld!", slicer_mid.CutString(7, true));
  EXPECT_EQ(u"Hey…rld!", slicer_mid.CutString(8, true));
}

TEST(TextEliderTest, StringSlicer_ElideMiddle_MultipleWhitespace) {
  // Must store strings in variables (StringSlicer retains a reference to them).
  std::u16string text(u"Hello  world!");
  std::u16string ellipsis(u"…");

  // Eliding the middle of a string should not result in whitespace being
  // removed around the ellipsis in default whitespace mode.
  StringSlicer slicer_default(text, ellipsis, true, false);
  text = u"Hey  U  man";
  EXPECT_EQ(u"Hey…man", slicer_default.CutString(6, true));
  EXPECT_EQ(u"Hey …man", slicer_default.CutString(7, true));
  EXPECT_EQ(u"Hey … man", slicer_default.CutString(8, true));
  EXPECT_EQ(u"Hey  … man", slicer_default.CutString(9, true));
  EXPECT_EQ(u"Hey  …  man", slicer_default.CutString(10, true));

  // Eliding the middle of a string should not result in whitespace being
  // removed around the ellipsis in no-trim mode.
  StringSlicer slicer_notrim(text, ellipsis, true, false, false);
  text = u"Hey  U  man";
  EXPECT_EQ(u"Hey…man", slicer_notrim.CutString(6, true));
  EXPECT_EQ(u"Hey …man", slicer_notrim.CutString(7, true));
  EXPECT_EQ(u"Hey … man", slicer_notrim.CutString(8, true));
  EXPECT_EQ(u"Hey  … man", slicer_notrim.CutString(9, true));
  EXPECT_EQ(u"Hey  …  man", slicer_notrim.CutString(10, true));

  // Eliding the middle of a string *should* result in whitespace being removed
  // around the ellipsis in trim mode.
  StringSlicer slicer_trim(text, ellipsis, true, false, true);
  text = u"Hey  U  man";
  EXPECT_EQ(u"Hey…man", slicer_trim.CutString(6, true));
  EXPECT_EQ(u"Hey…man", slicer_trim.CutString(7, true));
  EXPECT_EQ(u"Hey…man", slicer_trim.CutString(8, true));
  EXPECT_EQ(u"Hey…man", slicer_trim.CutString(9, true));
  EXPECT_EQ(u"Hey…man", slicer_trim.CutString(10, true));
}

TEST(TextEliderTest, StringSlicerSurrogate) {
  // The below is 'MUSICAL SYMBOL G CLEF' (U+1D11E), which is represented in
  // UTF-16 as two code units forming a surrogate pair: 0xD834 0xDD1E.
  const std::u16string kSurrogate = u"\U0001d11e";
  ASSERT_EQ(2u, kSurrogate.size());
  ASSERT_EQ(u'\xD834', kSurrogate[0]);
  ASSERT_EQ(u'\xDD1E', kSurrogate[1]);

  std::u16string text(u"abc" + kSurrogate + u"xyz");
  std::u16string ellipsis(u"…");
  StringSlicer slicer(text, ellipsis, false, false);

  // Cut surrogate on the right. Should round left and exclude the surrogate.
  EXPECT_EQ(u"…", slicer.CutString(0, true));
  EXPECT_EQ(u"abc…", slicer.CutString(4, true));
  EXPECT_EQ(text + u"…", slicer.CutString(text.length(), true));

  // Cut surrogate on the left. Should round right and exclude the surrogate.
  StringSlicer slicer_begin(text, ellipsis, false, true);
  EXPECT_EQ(u"…xyz", slicer_begin.CutString(4, true));

  // Cut surrogate in the middle. Should round right and exclude the surrogate.
  std::u16string short_text(u"abc" + kSurrogate);
  StringSlicer slicer_mid(short_text, ellipsis, true, false);
  EXPECT_EQ(u"a…", slicer_mid.CutString(2, true));

  // String that starts with a dangling trailing surrogate.
  std::u16string dangling_trailing_text = kSurrogate.substr(1);
  StringSlicer slicer_dangling_trailing(dangling_trailing_text, ellipsis, false,
                                        false);
  EXPECT_EQ(u"…", slicer_dangling_trailing.CutString(0, true));
  EXPECT_EQ(dangling_trailing_text + u"…",
            slicer_dangling_trailing.CutString(1, true));
}

TEST(TextEliderTest, StringSlicerCombining) {
  // The following string contains three combining character sequences (one for
  // each category of combining mark):
  // LATIN SMALL LETTER E + COMBINING ACUTE ACCENT + COMBINING CEDILLA
  // LATIN SMALL LETTER X + COMBINING ENCLOSING KEYCAP
  // DEVANAGARI LETTER DDA + DEVANAGARI VOWEL SIGN I
  std::u16string text(u"e\u0301\u0327 x\u20e3 \u0921\u093f");
  std::u16string ellipsis(u"…");
  StringSlicer slicer(text, ellipsis, false, false);

  // Attempt to cut the string for all lengths. When a combining sequence is
  // cut, it should always round left and exclude the combining sequence.
  // Whitespace is also cut adjacent to the ellipsis.

  // First sequence:
  EXPECT_EQ(u"…", slicer.CutString(0, true));
  EXPECT_EQ(u"…", slicer.CutString(1, true));
  EXPECT_EQ(u"…", slicer.CutString(2, true));
  EXPECT_EQ(text.substr(0, 3) + u"…", slicer.CutString(3, true));
  // Second sequence:
  EXPECT_EQ(text.substr(0, 3) + u"…", slicer.CutString(4, true));
  EXPECT_EQ(text.substr(0, 3) + u"…", slicer.CutString(5, true));
  EXPECT_EQ(text.substr(0, 6) + u"…", slicer.CutString(6, true));
  // Third sequence:
  EXPECT_EQ(text.substr(0, 6) + u"…", slicer.CutString(7, true));
  EXPECT_EQ(text.substr(0, 6) + u"…", slicer.CutString(8, true));
  EXPECT_EQ(text + u"…", slicer.CutString(9, true));

  // Cut string in the middle, splitting the second sequence in half. Should
  // round both left and right, excluding the second sequence.
  StringSlicer slicer_mid(text, ellipsis, true, false);
  EXPECT_EQ(text.substr(0, 4) + u"…" + text.substr(6),
            slicer_mid.CutString(9, true));

  // String that starts with a dangling combining mark.
  char16_t dangling_mark_chars[] = {text[1], 0};
  std::u16string dangling_mark_text(dangling_mark_chars);
  StringSlicer slicer_dangling_mark(dangling_mark_text, ellipsis, false, false);
  EXPECT_EQ(u"…", slicer_dangling_mark.CutString(0, true));
  EXPECT_EQ(dangling_mark_text + u"…", slicer_dangling_mark.CutString(1, true));
}

TEST(TextEliderTest, StringSlicerCombiningSurrogate) {
  // The ultimate test: combining sequences comprised of surrogate pairs.
  // The following string contains a single combining character sequence:
  // MUSICAL SYMBOL G CLEF (U+1D11E) + MUSICAL SYMBOL COMBINING FLAG-1 (U+1D16E)
  // Represented as four UTF-16 code units.
  std::u16string text(u"\U0001d11e\U0001d16e");
  std::u16string ellipsis(u"…");
  StringSlicer slicer(text, ellipsis, false, false);

  // Attempt to cut the string for all lengths. Should always round left and
  // exclude the combining sequence.
  EXPECT_EQ(u"…", slicer.CutString(0, true));
  EXPECT_EQ(u"…", slicer.CutString(1, true));
  EXPECT_EQ(u"…", slicer.CutString(2, true));
  EXPECT_EQ(u"…", slicer.CutString(3, true));
  EXPECT_EQ(text + u"…", slicer.CutString(4, true));

  // Cut string in the middle. Should exclude the sequence.
  StringSlicer slicer_mid(text, ellipsis, true, false);
  EXPECT_EQ(u"…", slicer_mid.CutString(4, true));
}

TEST(TextEliderTest, ElideString) {
  struct Case {
    const char16_t* input;
    size_t max_len;
    bool result;
    const char16_t* output;
  };
  const auto cases = std::to_array<Case>(
      {{u"Hello", 0, true, u""},
       {u"", 0, false, u""},
       {u"Hello, my name is Tom", 1, true, u"H"},
       {u"Hello, my name is Tom", 2, true, u"He"},
       {u"Hello, my name is Tom", 3, true, u"H.m"},
       {u"Hello, my name is Tom", 4, true, u"H..m"},
       {u"Hello, my name is Tom", 5, true, u"H...m"},
       {u"Hello, my name is Tom", 6, true, u"He...m"},
       {u"Hello, my name is Tom", 7, true, u"He...om"},
       {u"Hello, my name is Tom", 10, true, u"Hell...Tom"},
       {u"Hello, my name is Tom", 100, false, u"Hello, my name is Tom"}});
  for (const auto& c : cases) {
    std::u16string output;
    EXPECT_EQ(c.result, ElideString(c.input, c.max_len, &output));
    EXPECT_EQ(c.output, output);
  }
}

TEST(TextEliderTest, ElideRectangleText) {
  const FontList font_list;
  const int line_height = font_list.GetHeight();
  const float test_width = GetStringWidthF(u"Test", font_list);

  struct Case {
    const char16_t* input;
    float available_pixel_width;
    int available_pixel_height;
    bool truncated_y;
    const char16_t* output;
  };
  const auto cases = std::to_array<Case>({
      {u"", 0, 0, false, nullptr},
      {u"", 1, 1, false, nullptr},
      {u"Test", test_width, 0, true, nullptr},
      {u"Test", test_width, 1, false, u"Test"},
      {u"Test", test_width, line_height, false, u"Test"},
      {u"Test Test", test_width, line_height, true, u"Test"},
      {u"Test Test", test_width, line_height + 1, false, u"Test|Test"},
      {u"Test Test", test_width, line_height * 2, false, u"Test|Test"},
      {u"Test Test", test_width, line_height * 3, false, u"Test|Test"},
      {u"Test Test", test_width * 2, line_height * 2, false, u"Test|Test"},
      {u"Test Test", test_width * 3, line_height, false, u"Test Test"},
      {u"Test\nTest", test_width * 3, line_height * 2, false, u"Test|Test"},
      {u"Te\nst Te", test_width, line_height * 3, false, u"Te|st|Te"},
      {u"\nTest", test_width, line_height * 2, false, u"|Test"},
      {u"\nTest", test_width, line_height, true, u""},
      {u"\n\nTest", test_width, line_height * 3, false, u"||Test"},
      {u"\n\nTest", test_width, line_height * 2, true, u"|"},
      {u"Test\n", 2 * test_width, line_height * 5, false, u"Test|"},
      {u"Test\n\n", 2 * test_width, line_height * 5, false, u"Test||"},
      {u"Test\n\n\n", 2 * test_width, line_height * 5, false, u"Test|||"},
      {u"Test\nTest\n\n", 2 * test_width, line_height * 5, false,
       u"Test|Test||"},
      {u"Test\n\nTest\n", 2 * test_width, line_height * 5, false,
       u"Test||Test|"},
      {u"Test\n\n\nTest", 2 * test_width, line_height * 5, false,
       u"Test|||Test"},
      {u"Te ", test_width, line_height, false, u"Te"},
      {u"Te  Te Test", test_width, 3 * line_height, false, u"Te|Te|Test"},
  });

  for (const auto& c : cases) {
    std::vector<std::u16string> lines;
    EXPECT_EQ(c.truncated_y ? INSUFFICIENT_SPACE_VERTICAL : 0,
              ElideRectangleText(c.input, font_list, c.available_pixel_width,
                                 c.available_pixel_height, TRUNCATE_LONG_WORDS,
                                 &lines));
    if (c.output) {
      const std::u16string result = base::JoinString(lines, u"|");
      EXPECT_EQ(c.output, result);
    } else {
      EXPECT_TRUE(lines.empty());
    }
  }
}

TEST(TextEliderTest, ElideRectangleTextFirstWordTruncated) {
  const FontList font_list;
  const int line_height = font_list.GetHeight();

  const float test_width = GetStringWidthF(u"Test", font_list);
  const float tes_width = GetStringWidthF(u"Tes", font_list);

  std::vector<std::u16string> lines;

  auto result_for_width = [&](const char16_t* input, float width) {
    lines.clear();
    return ElideRectangleText(input, font_list, width, line_height * 4,
                              WRAP_LONG_WORDS, &lines);
  };

  // Test base case.
  EXPECT_EQ(0, result_for_width(u"Test", test_width));
  EXPECT_EQ(1u, lines.size());
  EXPECT_EQ(u"Test", lines[0]);

  // First word truncated.
  EXPECT_EQ(INSUFFICIENT_SPACE_FOR_FIRST_WORD,
            result_for_width(u"Test", tes_width));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ(u"Tes", lines[0]);
  EXPECT_EQ(u"t", lines[1]);

  // Two words truncated.
  EXPECT_EQ(INSUFFICIENT_SPACE_FOR_FIRST_WORD,
            result_for_width(u"Test\nTest", tes_width));
  EXPECT_EQ(4u, lines.size());
  EXPECT_EQ(u"Tes", lines[0]);
  EXPECT_EQ(u"t", lines[1]);
  EXPECT_EQ(u"Tes", lines[2]);
  EXPECT_EQ(u"t", lines[3]);

  // Word truncated, but not the first.
  EXPECT_EQ(0, result_for_width(u"T Test", tes_width));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ(u"T", lines[0]);
  EXPECT_EQ(u"Tes", lines[1]);
  EXPECT_EQ(u"t", lines[2]);

  // Leading \n.
  EXPECT_EQ(0, result_for_width(u"\nTest", tes_width));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ(u"", lines[0]);
  EXPECT_EQ(u"Tes", lines[1]);
  EXPECT_EQ(u"t", lines[2]);
}

TEST(TextEliderTest, ElideRectangleTextPunctuation) {
  const FontList font_list;
  const int line_height = font_list.GetHeight();
  const float test_width = GetStringWidthF(u"Test", font_list);
  const float test_t_width = GetStringWidthF(u"Test T", font_list);
  constexpr int kResultMask =
      INSUFFICIENT_SPACE_HORIZONTAL | INSUFFICIENT_SPACE_VERTICAL;

  struct Case {
    const char16_t* input;
    float available_pixel_width;
    int available_pixel_height;
    bool wrap_words;
    bool truncated_x;
    const char16_t* output;
  };
  const auto cases = std::to_array<Case>({
      {u"Test T.", test_t_width, line_height * 2, false, false, u"Test|T."},
      {u"Test T ?", test_t_width, line_height * 2, false, false, u"Test|T ?"},
      {u"Test. Test", test_width, line_height * 3, false, true, u"Test|Test"},
      {u"Test. Test", test_width, line_height * 3, true, false, u"Test|.|Test"},
  });

  for (const auto& c : cases) {
    std::vector<std::u16string> lines;
    const WordWrapBehavior wrap_behavior =
        (c.wrap_words ? WRAP_LONG_WORDS : TRUNCATE_LONG_WORDS);
    EXPECT_EQ(
        c.truncated_x ? INSUFFICIENT_SPACE_HORIZONTAL : 0,
        ElideRectangleText(c.input, font_list, c.available_pixel_width,
                           c.available_pixel_height, wrap_behavior, &lines) &
            kResultMask);
    if (c.output) {
      const std::u16string result = base::JoinString(lines, u"|");
      EXPECT_EQ(c.output, result);
    } else {
      EXPECT_TRUE(lines.empty());
    }
  }
}

TEST(TextEliderTest, ElideRectangleTextLongWords) {
  const FontList font_list;
  const int kAvailableHeight = 1000;
  const std::u16string kElidedTesting = u"Tes…";
  const float elided_width = GetStringWidthF(kElidedTesting, font_list);
  const float test_width = GetStringWidthF(u"Test", font_list);
  constexpr int kResultMask =
      INSUFFICIENT_SPACE_HORIZONTAL | INSUFFICIENT_SPACE_VERTICAL;

  struct Case {
    const char16_t* input;
    float available_pixel_width;
    WordWrapBehavior wrap_behavior;
    bool truncated_x;
    const char16_t* output;
  };
  const auto cases = std::to_array<Case>({
      {u"Testing", test_width, IGNORE_LONG_WORDS, false, u"Testing"},
      {u"X Testing", test_width, IGNORE_LONG_WORDS, false, u"X|Testing"},
      {u"Test Testing", test_width, IGNORE_LONG_WORDS, false, u"Test|Testing"},
      {u"Test\nTesting", test_width, IGNORE_LONG_WORDS, false, u"Test|Testing"},
      {u"Test Tests ", test_width, IGNORE_LONG_WORDS, false, u"Test|Tests"},
      {u"Test Tests T", test_width, IGNORE_LONG_WORDS, false, u"Test|Tests|T"},

      {u"Testing", elided_width, ELIDE_LONG_WORDS, true, u"Tes…"},
      {u"X Testing", elided_width, ELIDE_LONG_WORDS, true, u"X|Tes…"},
      {u"Test Testing", elided_width, ELIDE_LONG_WORDS, true, u"Test|Tes…"},
      {u"Test\nTesting", elided_width, ELIDE_LONG_WORDS, true, u"Test|Tes…"},

      {u"Testing", test_width, TRUNCATE_LONG_WORDS, true, u"Test"},
      {u"X Testing", test_width, TRUNCATE_LONG_WORDS, true, u"X|Test"},
      {u"Test Testing", test_width, TRUNCATE_LONG_WORDS, true, u"Test|Test"},
      {u"Test\nTesting", test_width, TRUNCATE_LONG_WORDS, true, u"Test|Test"},
      {u"Test Tests ", test_width, TRUNCATE_LONG_WORDS, true, u"Test|Test"},
      {u"Test Tests T", test_width, TRUNCATE_LONG_WORDS, true, u"Test|Test|T"},

      {u"Testing", test_width, WRAP_LONG_WORDS, false, u"Test|ing"},
      {u"X Testing", test_width, WRAP_LONG_WORDS, false, u"X|Test|ing"},
      {u"Test Testing", test_width, WRAP_LONG_WORDS, false, u"Test|Test|ing"},
      {u"Test\nTesting", test_width, WRAP_LONG_WORDS, false, u"Test|Test|ing"},
      {u"Test Tests ", test_width, WRAP_LONG_WORDS, false, u"Test|Test|s"},
      {u"Test Tests T", test_width, WRAP_LONG_WORDS, false, u"Test|Test|s T"},
      {u"TestTestTest", test_width, WRAP_LONG_WORDS, false, u"Test|Test|Test"},
      {u"TestTestTestT", test_width, WRAP_LONG_WORDS, false,
       u"Test|Test|Test|T"},
  });

  for (const auto& c : cases) {
    std::vector<std::u16string> lines;
    EXPECT_EQ(c.truncated_x ? INSUFFICIENT_SPACE_HORIZONTAL : 0,
              ElideRectangleText(c.input, font_list, c.available_pixel_width,
                                 kAvailableHeight, c.wrap_behavior, &lines) &
                  kResultMask);
    std::u16string expected_output(c.output);
    const std::u16string result = base::JoinString(lines, u"|");
    EXPECT_EQ(expected_output, result);
  }
}

// This test is to make sure that the width of each wrapped line does not
// exceed the available width. On some platform like Mac, this test used to
// fail because the truncated integer width is returned for the string
// and the accumulation of the truncated values causes the elide function
// to wrap incorrectly.
TEST(TextEliderTest, ElideRectangleTextCheckLineWidth) {
  FontList font_list;
#if BUILDFLAG(IS_MAC)
  // Use a specific font to expose the line width exceeding problem.
  font_list = FontList(Font("LucidaGrande", 12));
#endif
  const float kAvailableWidth = 235;
  const int kAvailableHeight = 1000;
  const char16_t text[] = u"that Russian place we used to go to after fencing";
  std::vector<std::u16string> lines;
  EXPECT_EQ(0, ElideRectangleText(text, font_list, kAvailableWidth,
                                  kAvailableHeight, WRAP_LONG_WORDS, &lines));
  ASSERT_EQ(2u, lines.size());
  EXPECT_LE(GetStringWidthF(lines[0], font_list), kAvailableWidth);
  EXPECT_LE(GetStringWidthF(lines[1], font_list), kAvailableWidth);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This test was created specifically to test a message from crbug.com/415213.
// It tests that width of concatenation of words equals sum of widths of the
// words.
TEST(TextEliderTest, ElideRectangleTextCheckConcatWidthEqualsSumOfWidths) {
  FontList font_list;
  font_list = FontList("Noto Sans UI,ui-sans, 12px");
  SetFontRenderParamsDeviceScaleFactor(1.25f);
#define WIDTH(x) GetStringWidthF((x), font_list)
  EXPECT_EQ(WIDTH(u"The administrator for this account has"),
            WIDTH(u"The ") + WIDTH(u"administrator ") + WIDTH(u"for ") +
                WIDTH(u"this ") + WIDTH(u"account ") + WIDTH(u"has"));
#undef WIDTH
  SetFontRenderParamsDeviceScaleFactor(1.0f);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST(TextEliderTest, ElideRectangleString) {
  struct Case {
    const char16_t* input;
    int max_rows;
    int max_cols;
    bool result;
    const char16_t* output;
  };
  const auto cases = std::to_array<Case>({
      {u"", 0, 0, false, u""},
      {u"", 1, 1, false, u""},
      {u"Hi, my name is\nTom", 0, 0, true, u"..."},
      {u"Hi, my name is\nTom", 1, 0, true, u"\n..."},
      {u"Hi, my name is\nTom", 0, 1, true, u"..."},
      {u"Hi, my name is\nTom", 1, 1, true, u"H\n..."},
      {u"Hi, my name is\nTom", 2, 1, true, u"H\ni\n..."},
      {u"Hi, my name is\nTom", 3, 1, true, u"H\ni\n,\n..."},
      {u"Hi, my name is\nTom", 4, 1, true, u"H\ni\n,\n \n..."},
      {u"Hi, my name is\nTom", 5, 1, true, u"H\ni\n,\n \nm\n..."},
      {u"Hi, my name is\nTom", 0, 2, true, u"..."},
      {u"Hi, my name is\nTom", 1, 2, true, u"Hi\n..."},
      {u"Hi, my name is\nTom", 2, 2, true, u"Hi\n, \n..."},
      {u"Hi, my name is\nTom", 3, 2, true, u"Hi\n, \nmy\n..."},
      {u"Hi, my name is\nTom", 4, 2, true, u"Hi\n, \nmy\n n\n..."},
      {u"Hi, my name is\nTom", 5, 2, true, u"Hi\n, \nmy\n n\nam\n..."},
      {u"Hi, my name is\nTom", 0, 3, true, u"..."},
      {u"Hi, my name is\nTom", 1, 3, true, u"Hi,\n..."},
      {u"Hi, my name is\nTom", 2, 3, true, u"Hi,\n my\n..."},
      {u"Hi, my name is\nTom", 3, 3, true, u"Hi,\n my\n na\n..."},
      {u"Hi, my name is\nTom", 4, 3, true, u"Hi,\n my\n na\nme \n..."},
      {u"Hi, my name is\nTom", 5, 3, true, u"Hi,\n my\n na\nme \nis\n..."},
      {u"Hi, my name is\nTom", 1, 4, true, u"Hi, \n..."},
      {u"Hi, my name is\nTom", 2, 4, true, u"Hi, \nmy n\n..."},
      {u"Hi, my name is\nTom", 3, 4, true, u"Hi, \nmy n\name \n..."},
      {u"Hi, my name is\nTom", 4, 4, true, u"Hi, \nmy n\name \nis\n..."},
      {u"Hi, my name is\nTom", 5, 4, false, u"Hi, \nmy n\name \nis\nTom"},
      {u"Hi, my name is\nTom", 1, 5, true, u"Hi, \n..."},
      {u"Hi, my name is\nTom", 2, 5, true, u"Hi, \nmy na\n..."},
      {u"Hi, my name is\nTom", 3, 5, true, u"Hi, \nmy na\nme \n..."},
      {u"Hi, my name is\nTom", 4, 5, true, u"Hi, \nmy na\nme \nis\n..."},
      {u"Hi, my name is\nTom", 5, 5, false, u"Hi, \nmy na\nme \nis\nTom"},
      {u"Hi, my name is\nTom", 1, 6, true, u"Hi, \n..."},
      {u"Hi, my name is\nTom", 2, 6, true, u"Hi, \nmy \n..."},
      {u"Hi, my name is\nTom", 3, 6, true, u"Hi, \nmy \nname \n..."},
      {u"Hi, my name is\nTom", 4, 6, true, u"Hi, \nmy \nname \nis\n..."},
      {u"Hi, my name is\nTom", 5, 6, false, u"Hi, \nmy \nname \nis\nTom"},
      {u"Hi, my name is\nTom", 1, 7, true, u"Hi, \n..."},
      {u"Hi, my name is\nTom", 2, 7, true, u"Hi, \nmy \n..."},
      {u"Hi, my name is\nTom", 3, 7, true, u"Hi, \nmy \nname \n..."},
      {u"Hi, my name is\nTom", 4, 7, true, u"Hi, \nmy \nname \nis\n..."},
      {u"Hi, my name is\nTom", 5, 7, false, u"Hi, \nmy \nname \nis\nTom"},
      {u"Hi, my name is\nTom", 1, 8, true, u"Hi, my \n..."},
      {u"Hi, my name is\nTom", 2, 8, true, u"Hi, my \nname \n..."},
      {u"Hi, my name is\nTom", 3, 8, true, u"Hi, my \nname \nis\n..."},
      {u"Hi, my name is\nTom", 4, 8, false, u"Hi, my \nname \nis\nTom"},
      {u"Hi, my name is\nTom", 1, 9, true, u"Hi, my \n..."},
      {u"Hi, my name is\nTom", 2, 9, true, u"Hi, my \nname is\n..."},
      {u"Hi, my name is\nTom", 3, 9, false, u"Hi, my \nname is\nTom"},
      {u"Hi, my name is\nTom", 1, 10, true, u"Hi, my \n..."},
      {u"Hi, my name is\nTom", 2, 10, true, u"Hi, my \nname is\n..."},
      {u"Hi, my name is\nTom", 3, 10, false, u"Hi, my \nname is\nTom"},
      {u"Hi, my name is\nTom", 1, 11, true, u"Hi, my \n..."},
      {u"Hi, my name is\nTom", 2, 11, true, u"Hi, my \nname is\n..."},
      {u"Hi, my name is\nTom", 3, 11, false, u"Hi, my \nname is\nTom"},
      {u"Hi, my name is\nTom", 1, 12, true, u"Hi, my \n..."},
      {u"Hi, my name is\nTom", 2, 12, true, u"Hi, my \nname is\n..."},
      {u"Hi, my name is\nTom", 3, 12, false, u"Hi, my \nname is\nTom"},
      {u"Hi, my name is\nTom", 1, 13, true, u"Hi, my name \n..."},
      {u"Hi, my name is\nTom", 2, 13, true, u"Hi, my name \nis\n..."},
      {u"Hi, my name is\nTom", 3, 13, false, u"Hi, my name \nis\nTom"},
      {u"Hi, my name is\nTom", 1, 20, true, u"Hi, my name is\n..."},
      {u"Hi, my name is\nTom", 2, 20, false, u"Hi, my name is\nTom"},
      {u"Hi, my name is Tom", 1, 40, false, u"Hi, my name is Tom"},
  });
  std::u16string output;
  for (const auto& c : cases) {
    EXPECT_EQ(c.result, ElideRectangleString(c.input, c.max_rows, c.max_cols,
                                             true, &output));
    EXPECT_EQ(c.output, output);
  }
}

TEST(TextEliderTest, ElideRectangleStringNotStrict) {
  struct Case {
    const char16_t* input;
    int max_rows;
    int max_cols;
    bool result;
    const char16_t* output;
  };
  const auto cases = std::to_array<Case>({
      {u"", 0, 0, false, u""},
      {u"", 1, 1, false, u""},
      {u"Hi, my name_is\nDick", 0, 0, true, u"..."},
      {u"Hi, my name_is\nDick", 1, 0, true, u"\n..."},
      {u"Hi, my name_is\nDick", 0, 1, true, u"..."},
      {u"Hi, my name_is\nDick", 1, 1, true, u"H\n..."},
      {u"Hi, my name_is\nDick", 2, 1, true, u"H\ni\n..."},
      {u"Hi, my name_is\nDick", 3, 1, true, u"H\ni\n,\n..."},
      {u"Hi, my name_is\nDick", 4, 1, true, u"H\ni\n,\n \n..."},
      {u"Hi, my name_is\nDick", 5, 1, true, u"H\ni\n,\n \nm\n..."},
      {u"Hi, my name_is\nDick", 0, 2, true, u"..."},
      {u"Hi, my name_is\nDick", 1, 2, true, u"Hi\n..."},
      {u"Hi, my name_is\nDick", 2, 2, true, u"Hi\n, \n..."},
      {u"Hi, my name_is\nDick", 3, 2, true, u"Hi\n, \nmy\n..."},
      {u"Hi, my name_is\nDick", 4, 2, true, u"Hi\n, \nmy\n n\n..."},
      {u"Hi, my name_is\nDick", 5, 2, true, u"Hi\n, \nmy\n n\nam\n..."},
      {u"Hi, my name_is\nDick", 0, 3, true, u"..."},
      {u"Hi, my name_is\nDick", 1, 3, true, u"Hi,\n..."},
      {u"Hi, my name_is\nDick", 2, 3, true, u"Hi,\n my\n..."},
      {u"Hi, my name_is\nDick", 3, 3, true, u"Hi,\n my\n na\n..."},
      {u"Hi, my name_is\nDick", 4, 3, true, u"Hi,\n my\n na\nme_\n..."},
      {u"Hi, my name_is\nDick", 5, 3, true, u"Hi,\n my\n na\nme_\nis\n..."},
      {u"Hi, my name_is\nDick", 1, 4, true, u"Hi, ..."},
      {u"Hi, my name_is\nDick", 2, 4, true, u"Hi, my n\n..."},
      {u"Hi, my name_is\nDick", 3, 4, true, u"Hi, my n\name_\n..."},
      {u"Hi, my name_is\nDick", 4, 4, true, u"Hi, my n\name_\nis\n..."},
      {u"Hi, my name_is\nDick", 5, 4, false, u"Hi, my n\name_\nis\nDick"},
      {u"Hi, my name_is\nDick", 1, 5, true, u"Hi, ..."},
      {u"Hi, my name_is\nDick", 2, 5, true, u"Hi, my na\n..."},
      {u"Hi, my name_is\nDick", 3, 5, true, u"Hi, my na\nme_is\n..."},
      {u"Hi, my name_is\nDick", 4, 5, true, u"Hi, my na\nme_is\n\n..."},
      {u"Hi, my name_is\nDick", 5, 5, false, u"Hi, my na\nme_is\n\nDick"},
      {u"Hi, my name_is\nDick", 1, 6, true, u"Hi, ..."},
      {u"Hi, my name_is\nDick", 2, 6, true, u"Hi, my nam\n..."},
      {u"Hi, my name_is\nDick", 3, 6, true, u"Hi, my nam\ne_is\n..."},
      {u"Hi, my name_is\nDick", 4, 6, false, u"Hi, my nam\ne_is\nDick"},
      {u"Hi, my name_is\nDick", 5, 6, false, u"Hi, my nam\ne_is\nDick"},
      {u"Hi, my name_is\nDick", 1, 7, true, u"Hi, ..."},
      {u"Hi, my name_is\nDick", 2, 7, true, u"Hi, my name\n..."},
      {u"Hi, my name_is\nDick", 3, 7, true, u"Hi, my name\n_is\n..."},
      {u"Hi, my name_is\nDick", 4, 7, false, u"Hi, my name\n_is\nDick"},
      {u"Hi, my name_is\nDick", 5, 7, false, u"Hi, my name\n_is\nDick"},
      {u"Hi, my name_is\nDick", 1, 8, true, u"Hi, my n\n..."},
      {u"Hi, my name_is\nDick", 2, 8, true, u"Hi, my n\name_is\n..."},
      {u"Hi, my name_is\nDick", 3, 8, false, u"Hi, my n\name_is\nDick"},
      {u"Hi, my name_is\nDick", 1, 9, true, u"Hi, my ..."},
      {u"Hi, my name_is\nDick", 2, 9, true, u"Hi, my name_is\n..."},
      {u"Hi, my name_is\nDick", 3, 9, false, u"Hi, my name_is\nDick"},
      {u"Hi, my name_is\nDick", 1, 10, true, u"Hi, my ..."},
      {u"Hi, my name_is\nDick", 2, 10, true, u"Hi, my name_is\n..."},
      {u"Hi, my name_is\nDick", 3, 10, false, u"Hi, my name_is\nDick"},
      {u"Hi, my name_is\nDick", 1, 11, true, u"Hi, my ..."},
      {u"Hi, my name_is\nDick", 2, 11, true, u"Hi, my name_is\n..."},
      {u"Hi, my name_is\nDick", 3, 11, false, u"Hi, my name_is\nDick"},
      {u"Hi, my name_is\nDick", 1, 12, true, u"Hi, my ..."},
      {u"Hi, my name_is\nDick", 2, 12, true, u"Hi, my name_is\n..."},
      {u"Hi, my name_is\nDick", 3, 12, false, u"Hi, my name_is\nDick"},
      {u"Hi, my name_is\nDick", 1, 13, true, u"Hi, my ..."},
      {u"Hi, my name_is\nDick", 2, 13, true, u"Hi, my name_is\n..."},
      {u"Hi, my name_is\nDick", 3, 13, false, u"Hi, my name_is\nDick"},
      {u"Hi, my name_is\nDick", 1, 20, true, u"Hi, my name_is\n..."},
      {u"Hi, my name_is\nDick", 2, 20, false, u"Hi, my name_is\nDick"},
      {u"Hi, my name_is Dick", 1, 40, false, u"Hi, my name_is Dick"},
  });
  std::u16string output;
  for (const auto& c : cases) {
    EXPECT_EQ(c.result, ElideRectangleString(c.input, c.max_rows, c.max_cols,
                                             false, &output));
    EXPECT_EQ(c.output, output);
  }
}

TEST(TextEliderTest, ElideRectangleWide16) {
  // Two greek words separated by space.
  const std::u16string str(u"Παγκόσμιος Ιστός");
  const std::u16string out1(u"Παγκ\nόσμι\n...");
  const std::u16string out2(u"Παγκόσμιος \nΙστός");
  std::u16string output;
  EXPECT_TRUE(ElideRectangleString(str, 2, 4, true, &output));
  EXPECT_EQ(out1, output);
  EXPECT_FALSE(ElideRectangleString(str, 2, 12, true, &output));
  EXPECT_EQ(out2, output);
}

TEST(TextEliderTest, ElideRectangleWide32) {
  const std::u16string str(u"𝒜𝒜𝒜𝒜 aaaaa");
  const std::u16string out(u"𝒜𝒜𝒜\n𝒜 \naaa\n...");
  std::u16string output;
  EXPECT_TRUE(ElideRectangleString(str, 3, 3, true, &output));
  EXPECT_EQ(out, output);
}

TEST(TextEliderTest, TruncateString) {
  std::u16string str = u"fooooey    bxxxar baz  ";

  // Test breaking at character 0.
  EXPECT_EQ(std::u16string(), TruncateString(str, 0, WORD_BREAK));
  EXPECT_EQ(std::u16string(), TruncateString(str, 0, CHARACTER_BREAK));

  // Test breaking at character 1.
  EXPECT_EQ(u"…", TruncateString(str, 1, WORD_BREAK));
  EXPECT_EQ(u"…", TruncateString(str, 1, CHARACTER_BREAK));

  // Test breaking in the middle of the first word.
  EXPECT_EQ(u"f…", TruncateString(str, 2, WORD_BREAK));
  EXPECT_EQ(u"f…", TruncateString(str, 2, CHARACTER_BREAK));

  // Test breaking in between words.
  EXPECT_EQ(u"fooooey…", TruncateString(str, 9, WORD_BREAK));
  EXPECT_EQ(u"fooooey…", TruncateString(str, 9, CHARACTER_BREAK));

  // Test breaking at the start of a later word.
  EXPECT_EQ(u"fooooey…", TruncateString(str, 11, WORD_BREAK));
  EXPECT_EQ(u"fooooey…", TruncateString(str, 11, CHARACTER_BREAK));

  // Test breaking in the middle of a word.
  EXPECT_EQ(u"fooooey…", TruncateString(str, 12, WORD_BREAK));
  EXPECT_EQ(u"fooooey…", TruncateString(str, 12, CHARACTER_BREAK));
  EXPECT_EQ(u"fooooey…", TruncateString(str, 14, WORD_BREAK));
  EXPECT_EQ(u"fooooey    bx…", TruncateString(str, 14, CHARACTER_BREAK));

  // Test breaking in whitespace at the end of the string.
  EXPECT_EQ(u"fooooey    bxxxar baz…", TruncateString(str, 22, WORD_BREAK));
  EXPECT_EQ(u"fooooey    bxxxar baz…",
            TruncateString(str, 22, CHARACTER_BREAK));

  // Test breaking at the end of the string.
  EXPECT_EQ(str, TruncateString(str, str.length(), WORD_BREAK));
  EXPECT_EQ(str, TruncateString(str, str.length(), CHARACTER_BREAK));

  // Test breaking past the end of the string.
  EXPECT_EQ(str, TruncateString(str, str.length() + 10, WORD_BREAK));
  EXPECT_EQ(str, TruncateString(str, str.length() + 10, CHARACTER_BREAK));


  // Tests of strings with leading whitespace:
  std::u16string str2 = u"   foo";

  // Test breaking in leading whitespace.
  EXPECT_EQ(u"…", TruncateString(str2, 2, WORD_BREAK));
  EXPECT_EQ(u"…", TruncateString(str2, 2, CHARACTER_BREAK));

  // Test breaking at the beginning of the first word, with leading whitespace.
  EXPECT_EQ(u"…", TruncateString(str2, 3, WORD_BREAK));
  EXPECT_EQ(u"…", TruncateString(str2, 3, CHARACTER_BREAK));

  // Test breaking in the middle of the first word, with leading whitespace.
  EXPECT_EQ(u"…", TruncateString(str2, 4, WORD_BREAK));
  EXPECT_EQ(u"…", TruncateString(str2, 4, CHARACTER_BREAK));
  EXPECT_EQ(u"   f…", TruncateString(str2, 5, WORD_BREAK));
  EXPECT_EQ(u"   f…", TruncateString(str2, 5, CHARACTER_BREAK));
}

}  // namespace gfx
