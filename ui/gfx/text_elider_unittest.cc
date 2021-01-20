// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/stl_util.h"
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

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using base::UTF16ToWide;
using base::UTF8ToUTF16;
using base::WideToUTF16;

namespace gfx {

namespace {

struct Testcase {
  const std::string input;
  const std::string output;
};

struct FileTestcase {
  const base::FilePath::StringType input;
  const std::string output;
  // If this value is specified, we will try to cut the path down to the render
  // width of this string; if not specified, output will be used.
  const std::string using_width_of = std::string();
};

struct UTF16Testcase {
  const base::string16 input;
  const base::string16 output;
};

struct TestData {
  const std::string a;
  const std::string b;
  const int compare_result;
};

}  // namespace

TEST(TextEliderTest, ElideEmail) {
  const std::string kEllipsisStr(kEllipsis);

  // Test emails and their expected elided forms (from which the available
  // widths will be derived).
  // For elided forms in which both the username and domain must be elided:
  // the result (how many characters are left on each side) can be font
  // dependent. To avoid this, the username is prefixed with the characters
  // expected to remain in the domain.
  Testcase testcases[] = {
      {"g@g.c", "g@g.c"},
      {"g@g.c", kEllipsisStr},
      {"ga@co.ca", "ga@c" + kEllipsisStr + "a"},
      {"short@small.com", "s" + kEllipsisStr + "@s" + kEllipsisStr},
      {"short@small.com", "s" + kEllipsisStr + "@small.com"},
      {"short@longbutlotsofspace.com", "short@longbutlotsofspace.com"},
      {"short@longbutnotverymuchspace.com",
       "short@long" + kEllipsisStr + ".com"},
      {"la_short@longbutverytightspace.ca",
       "la" + kEllipsisStr + "@l" + kEllipsisStr + "a"},
      {"longusername@gmail.com", "long" + kEllipsisStr + "@gmail.com"},
      {"elidetothemax@justfits.com", "e" + kEllipsisStr + "@justfits.com"},
      {"thatom_somelongemail@thatdoesntfit.com",
       "thatom" + kEllipsisStr + "@tha" + kEllipsisStr + "om"},
      {"namefits@butthedomaindoesnt.com",
       "namefits@butthedo" + kEllipsisStr + "snt.com"},
      {"widthtootight@nospace.com", kEllipsisStr},
      {"nospaceforusername@l", kEllipsisStr},
      {"little@littlespace.com", "l" + kEllipsisStr + "@l" + kEllipsisStr},
      {"l@llllllllllllllllllllllll.com", "l@lllll" + kEllipsisStr + ".com"},
      {"messed\"up@whyanat\"++@notgoogley.com",
       "messed\"up@whyanat\"++@notgoogley.com"},
      {"messed\"up@whyanat\"++@notgoogley.com",
       "messed\"up@why" + kEllipsisStr + "@notgoogley.com"},
      {"noca_messed\"up@whyanat\"++@notgoogley.ca",
       "noca" + kEllipsisStr + "@no" + kEllipsisStr + "ca"},
      {"at\"@@@@@@@@@...@@.@.@.@@@\"@madness.com",
       "at\"@@@@@@@@@...@@.@." + kEllipsisStr + "@madness.com"},
      // Special case: "m..." takes more than half of the available width; thus
      // the domain must elide to "l..." and not "l...l" as it must allow enough
      // space for the minimal username elision although its half of the
      // available width would normally allow it to elide to "l...l".
      {"mmmmm@llllllllll", "m" + kEllipsisStr + "@l" + kEllipsisStr},
  };

  const FontList font_list;
  for (size_t i = 0; i < base::size(testcases); ++i) {
    const base::string16 expected_output = UTF8ToUTF16(testcases[i].output);
    EXPECT_EQ(expected_output,
              ElideText(UTF8ToUTF16(testcases[i].input), font_list,
                        GetStringWidthF(expected_output, font_list),
                        ELIDE_EMAIL));
  }
}

TEST(TextEliderTest, ElideEmailMoreSpace) {
  const int test_widths_extra_spaces[] = {
      10,
      1000,
      100000,
  };
  const char* test_emails[] = {
      "a@c",
      "test@email.com",
      "short@verysuperdupperlongdomain.com",
      "supermegalongusername@withasuperlonnnggggdomain.gouv.qc.ca",
  };

  const FontList font_list;
  for (const auto* test_email : test_emails) {
    const base::string16 test_email16 = UTF8ToUTF16(test_email);
    const int mimimum_width = GetStringWidth(test_email16, font_list);
    for (int extra_space : test_widths_extra_spaces) {
      // Extra space is available: the email should not be elided.
      EXPECT_EQ(test_email16,
                ElideText(test_email16, font_list, mimimum_width + extra_space,
                          ELIDE_EMAIL));
    }
  }
}

TEST(TextEliderTest, TestFilenameEliding) {
  const std::string kEllipsisStr(kEllipsis);
  const base::FilePath::StringType kPathSeparator =
      base::FilePath::StringType().append(1, base::FilePath::kSeparators[0]);

  FileTestcase testcases[] = {
      {FILE_PATH_LITERAL(""), ""},
      {FILE_PATH_LITERAL("."), "."},
      {FILE_PATH_LITERAL("filename.exe"), "filename.exe"},
      {FILE_PATH_LITERAL(".longext"), ".longext"},
      {FILE_PATH_LITERAL("pie"), "pie"},
      {FILE_PATH_LITERAL("c:") + kPathSeparator + FILE_PATH_LITERAL("path") +
           kPathSeparator + FILE_PATH_LITERAL("filename.pie"),
       "filename.pie"},
      {FILE_PATH_LITERAL("c:") + kPathSeparator + FILE_PATH_LITERAL("path") +
           kPathSeparator + FILE_PATH_LITERAL("longfilename.pie"),
       "long" + kEllipsisStr + ".pie"},
      {FILE_PATH_LITERAL("http://path.com/filename.pie"), "filename.pie"},
      {FILE_PATH_LITERAL("http://path.com/longfilename.pie"),
       "long" + kEllipsisStr + ".pie"},
      {FILE_PATH_LITERAL("piesmashingtacularpants"), "pie" + kEllipsisStr},
      {FILE_PATH_LITERAL(".piesmashingtacularpants"), ".pie" + kEllipsisStr},
      {FILE_PATH_LITERAL("cheese."), "cheese."},
      {FILE_PATH_LITERAL("file name.longext"),
       "file" + kEllipsisStr + ".longext"},
      {FILE_PATH_LITERAL("fil ename.longext"),
       "fil" + kEllipsisStr + ".longext", "fil " + kEllipsisStr + ".longext"},
      {FILE_PATH_LITERAL("filename.longext"),
       "file" + kEllipsisStr + ".longext"},
      {FILE_PATH_LITERAL("filename.middleext.longext"),
       "filename.mid" + kEllipsisStr + ".longext"},
      {FILE_PATH_LITERAL("filename.superduperextremelylongext"),
       "filename.sup" + kEllipsisStr + "emelylongext"},
      {FILE_PATH_LITERAL("filenamereallylongtext.superdeduperextremelylongext"),
       "filenamereall" + kEllipsisStr + "emelylongext"},
      {FILE_PATH_LITERAL(
           "file.name.really.long.text.superduperextremelylongext"),
       "file.name.re" + kEllipsisStr + "emelylongext"}};

  static const FontList font_list;
  for (size_t i = 0; i < base::size(testcases); ++i) {
    base::FilePath filepath(testcases[i].input);
    base::string16 expected = UTF8ToUTF16(testcases[i].output);
    base::string16 using_width_of = UTF8ToUTF16(
        testcases[i].using_width_of.empty() ? testcases[i].output
                                            : testcases[i].using_width_of);
    expected = base::i18n::GetDisplayStringInLTRDirectionality(expected);
    EXPECT_EQ(expected,
              ElideFilename(filepath, font_list,
                            GetStringWidthF(using_width_of, font_list)));
  }
}

TEST(TextEliderTest, ElideTextTruncate) {
  const FontList font_list;
  const float kTestWidth = GetStringWidthF(ASCIIToUTF16("Test"), font_list);
  struct TestData {
    const char* input;
    float width;
    const char* output;
  } cases[] = {
    { "", 0, "" },
    { "Test", 0, "" },
    { "", kTestWidth, "" },
    { "Tes", kTestWidth, "Tes" },
    { "Test", kTestWidth, "Test" },
    { "Tests", kTestWidth, "Test" },
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    base::string16 result = ElideText(UTF8ToUTF16(cases[i].input), font_list,
                                      cases[i].width, TRUNCATE);
    EXPECT_EQ(cases[i].output, UTF16ToUTF8(result));
  }
}

TEST(TextEliderTest, ElideTextEllipsis) {
  const FontList font_list;
  const float kTestWidth = GetStringWidthF(ASCIIToUTF16("Test"), font_list);
  const char* kEllipsis = "\xE2\x80\xA6";
  const float kEllipsisWidth =
      GetStringWidthF(UTF8ToUTF16(kEllipsis), font_list);
  struct TestData {
    const char* input;
    float width;
    const char* output;
  } cases[] = {
    { "", 0, "" },
    { "Test", 0, "" },
    { "Test", kEllipsisWidth, kEllipsis },
    { "", kTestWidth, "" },
    { "Tes", kTestWidth, "Tes" },
    { "Test", kTestWidth, "Test" },
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    base::string16 result = ElideText(UTF8ToUTF16(cases[i].input), font_list,
                                      cases[i].width, ELIDE_TAIL);
    EXPECT_EQ(cases[i].output, UTF16ToUTF8(result));
  }
}

TEST(TextEliderTest, ElideTextEllipsisFront) {
  const FontList font_list;
  const float kTestWidth = GetStringWidthF(ASCIIToUTF16("Test"), font_list);
  const std::string kEllipsisStr(kEllipsis);
  const float kEllipsisWidth =
      GetStringWidthF(UTF8ToUTF16(kEllipsis), font_list);
  const float kEllipsis23Width =
      GetStringWidthF(UTF8ToUTF16(kEllipsisStr + "23"), font_list);
  struct TestData {
    const char* input;
    float width;
    const base::string16 output;
  } cases[] = {
    { "",        0,                base::string16() },
    { "Test",    0,                base::string16() },
    { "Test",    kEllipsisWidth,   UTF8ToUTF16(kEllipsisStr) },
    { "",        kTestWidth,       base::string16() },
    { "Tes",     kTestWidth,       ASCIIToUTF16("Tes") },
    { "Test",    kTestWidth,       ASCIIToUTF16("Test") },
    { "Test123", kEllipsis23Width, UTF8ToUTF16(kEllipsisStr + "23") },
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    base::string16 result = ElideText(UTF8ToUTF16(cases[i].input), font_list,
                                      cases[i].width, ELIDE_HEAD);
    EXPECT_EQ(cases[i].output, result);
  }
}

// Checks that all occurrences of |first_char| are followed by |second_char| and
// all occurrences of |second_char| are preceded by |first_char| in |text|. Can
// be used to test surrogate pairs or two-character combining sequences.
static void CheckCodeUnitPairs(const base::string16& text,
                               base::char16 first_char,
                               base::char16 second_char) {
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
#if defined(OS_WIN)
  // Needed to bypass DCHECK in GetFallbackFont.
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
#endif
  const FontList font_list;
  // The below is 'MUSICAL SYMBOL G CLEF' (U+1D11E), which is represented in
  // UTF-16 as two code units forming a surrogate pair: 0xD834 0xDD1E.
  const base::char16 kSurrogate[] = {0xD834, 0xDD1E, 0};
  // The below is a Devanagari two-character combining sequence U+0921 U+093F.
  // The sequence forms a single display character and should not be separated.
  const base::char16 kCombiningSequence[] = {0x921, 0x93F, 0};
  std::vector<base::string16> pairs;
  pairs.push_back(kSurrogate);
  pairs.push_back(kCombiningSequence);

  for (const base::string16& pair : pairs) {
    base::char16 first_char = pair[0];
    base::char16 second_char = pair[1];
    base::string16 test_string = pair + UTF8ToUTF16("x") + pair;
    SCOPED_TRACE(test_string);
    const float test_string_width = GetStringWidthF(test_string, font_list);
    base::string16 result;

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
  const base::string16 kEllipsisStr = UTF8ToUTF16(kEllipsis);
  base::string16 data_scheme(UTF8ToUTF16("data:text/plain,"));
  size_t data_scheme_length = data_scheme.length();

  base::string16 ten_a(10, 'a');
  base::string16 hundred_a(100, 'a');
  base::string16 thousand_a(1000, 'a');
  base::string16 ten_thousand_a(10000, 'a');
  base::string16 hundred_thousand_a(100000, 'a');
  base::string16 million_a(1000000, 'a');

  // TODO(gbillock): Improve these tests by adding more string diversity and
  // doing string compares instead of length compares. See bug 338836.

  size_t number_of_as = 156;
  base::string16 long_string_end(
      data_scheme + base::string16(number_of_as, 'a') + kEllipsisStr);
  UTF16Testcase testcases_end[] = {
     { data_scheme + ten_a,              data_scheme + ten_a },
     { data_scheme + hundred_a,          data_scheme + hundred_a },
     { data_scheme + thousand_a,         long_string_end },
     { data_scheme + ten_thousand_a,     long_string_end },
     { data_scheme + hundred_thousand_a, long_string_end },
     { data_scheme + million_a,          long_string_end },
  };

  const FontList font_list;
  float ellipsis_width = GetStringWidthF(kEllipsisStr, font_list);
  for (size_t i = 0; i < base::size(testcases_end); ++i) {
    // Compare sizes rather than actual contents because if the test fails,
    // output is rather long.
    EXPECT_EQ(testcases_end[i].output.size(),
              ElideText(testcases_end[i].input, font_list,
                        GetStringWidthF(testcases_end[i].output, font_list),
                        ELIDE_TAIL).size());
    EXPECT_EQ(kEllipsisStr,
              ElideText(testcases_end[i].input, font_list, ellipsis_width,
                        ELIDE_TAIL));
  }

  size_t number_of_trailing_as = (data_scheme_length + number_of_as) / 2;
  base::string16 long_string_middle(
      data_scheme + base::string16(number_of_as - number_of_trailing_as, 'a') +
      kEllipsisStr + base::string16(number_of_trailing_as, 'a'));
#if !defined(OS_IOS)
  long_string_middle += kEllipsisStr;
#endif

  UTF16Testcase testcases_middle[] = {
      {data_scheme + ten_a, data_scheme + ten_a},
      {data_scheme + hundred_a, data_scheme + hundred_a},
      {data_scheme + thousand_a, long_string_middle},
      {data_scheme + ten_thousand_a, long_string_middle},
      {data_scheme + hundred_thousand_a, long_string_middle},
      {data_scheme + million_a, long_string_middle},
  };

  for (size_t i = 0; i < base::size(testcases_middle); ++i) {
    // Compare sizes rather than actual contents because if the test fails,
    // output is rather long.
    EXPECT_EQ(testcases_middle[i].output.size(),
              ElideText(testcases_middle[i].input, font_list,
                        GetStringWidthF(testcases_middle[i].output, font_list),
                        ELIDE_MIDDLE)
                  .size());
    EXPECT_EQ(kEllipsisStr, ElideText(testcases_middle[i].input, font_list,
                                      ellipsis_width, ELIDE_MIDDLE));
  }

  base::string16 long_string_beginning(
      kEllipsisStr + base::string16(number_of_as, 'a'));
#if !defined(OS_IOS)
  long_string_beginning += kEllipsisStr;
#endif

  UTF16Testcase testcases_beginning[] = {
      {data_scheme + ten_a, data_scheme + ten_a},
      {data_scheme + hundred_a, data_scheme + hundred_a},
      {data_scheme + thousand_a, long_string_beginning},
      {data_scheme + ten_thousand_a, long_string_beginning},
      {data_scheme + hundred_thousand_a, long_string_beginning},
      {data_scheme + million_a, long_string_beginning},
  };
  for (size_t i = 0; i < base::size(testcases_beginning); ++i) {
    EXPECT_EQ(testcases_beginning[i].output.size(),
              ElideText(
                  testcases_beginning[i].input, font_list,
                  GetStringWidthF(testcases_beginning[i].output, font_list),
                  ELIDE_HEAD).size());
    EXPECT_EQ(kEllipsisStr, ElideText(testcases_beginning[i].input, font_list,
                                      ellipsis_width, ELIDE_HEAD));
  }
}

// Detailed tests for StringSlicer. These are faster and test more of the edge
// cases than the above tests which are more end-to-end.

TEST(TextEliderTest, StringSlicerBasicTest) {
  // Must store strings in variables (StringSlicer retains a reference to them).
  base::string16 text(UTF8ToUTF16("Hello, world!"));
  base::string16 ellipsis(kEllipsisUTF16);
  StringSlicer slicer(text, ellipsis, false, false);

  EXPECT_EQ(UTF8ToUTF16(""), slicer.CutString(0, false));
  EXPECT_EQ(base::string16(kEllipsisUTF16), slicer.CutString(0, true));

  EXPECT_EQ(UTF8ToUTF16("Hell"), slicer.CutString(4, false));
  EXPECT_EQ(UTF8ToUTF16("Hell") + kEllipsisUTF16, slicer.CutString(4, true));

  EXPECT_EQ(text, slicer.CutString(text.length(), false));
  EXPECT_EQ(text + kEllipsisUTF16, slicer.CutString(text.length(), true));

  StringSlicer slicer_begin(text, ellipsis, false, true);
  EXPECT_EQ(UTF8ToUTF16("rld!"), slicer_begin.CutString(4, false));
  EXPECT_EQ(kEllipsisUTF16 + UTF8ToUTF16("rld!"),
            slicer_begin.CutString(4, true));

  StringSlicer slicer_mid(text, ellipsis, true, false);
  EXPECT_EQ(UTF8ToUTF16("Held!"), slicer_mid.CutString(5, false));
  EXPECT_EQ(UTF8ToUTF16("Hel") + kEllipsisUTF16 + UTF8ToUTF16("d!"),
            slicer_mid.CutString(5, true));
}

TEST(TextEliderTest, StringSlicerWhitespace_UseDefault) {
  // Must store strings in variables (StringSlicer retains a reference to them).
  base::string16 text(UTF8ToUTF16("Hello, world!"));
  base::string16 ellipsis(kEllipsisUTF16);

  // Eliding the end of a string should result in whitespace being removed
  // before the ellipsis by default.
  StringSlicer slicer_end(text, ellipsis, false, false);
  EXPECT_EQ(UTF8ToUTF16("Hello,") + kEllipsisUTF16,
            slicer_end.CutString(6, true));
  EXPECT_EQ(UTF8ToUTF16("Hello,") + kEllipsisUTF16,
            slicer_end.CutString(7, true));
  EXPECT_EQ(UTF8ToUTF16("Hello, w") + kEllipsisUTF16,
            slicer_end.CutString(8, true));

  // Eliding the start of a string should result in whitespace being removed
  // after the ellipsis by default.
  StringSlicer slicer_begin(text, ellipsis, false, true);
  EXPECT_EQ(kEllipsisUTF16 + UTF8ToUTF16("world!"),
            slicer_begin.CutString(6, true));
  EXPECT_EQ(kEllipsisUTF16 + UTF8ToUTF16("world!"),
            slicer_begin.CutString(7, true));
  EXPECT_EQ(kEllipsisUTF16 + UTF8ToUTF16(", world!"),
            slicer_begin.CutString(8, true));

  // Eliding the middle of a string should *NOT* result in whitespace being
  // removed around the ellipsis by default.
  StringSlicer slicer_mid(text, ellipsis, true, false);
  text = UTF8ToUTF16("Hey world!");
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("ld!"),
            slicer_mid.CutString(6, true));
  EXPECT_EQ(UTF8ToUTF16("Hey ") + kEllipsisUTF16 + UTF8ToUTF16("ld!"),
            slicer_mid.CutString(7, true));
  EXPECT_EQ(UTF8ToUTF16("Hey ") + kEllipsisUTF16 + UTF8ToUTF16("rld!"),
            slicer_mid.CutString(8, true));
}

TEST(TextEliderTest, StringSlicerWhitespace_NoTrim) {
  // Must store strings in variables (StringSlicer retains a reference to them).
  base::string16 text(UTF8ToUTF16("Hello, world!"));
  base::string16 ellipsis(kEllipsisUTF16);

  // Eliding the end of a string should not result in whitespace being removed
  // before the ellipsis in no-trim mode.
  StringSlicer slicer_end(text, ellipsis, false, false, false);
  EXPECT_EQ(UTF8ToUTF16("Hello,") + kEllipsisUTF16,
            slicer_end.CutString(6, true));
  EXPECT_EQ(UTF8ToUTF16("Hello, ") + kEllipsisUTF16,
            slicer_end.CutString(7, true));
  EXPECT_EQ(UTF8ToUTF16("Hello, w") + kEllipsisUTF16,
            slicer_end.CutString(8, true));

  // Eliding the start of a string should not result in whitespace being removed
  // after the ellipsis in no-trim mode.
  StringSlicer slicer_begin(text, ellipsis, false, true, false);
  EXPECT_EQ(kEllipsisUTF16 + UTF8ToUTF16("world!"),
            slicer_begin.CutString(6, true));
  EXPECT_EQ(kEllipsisUTF16 + UTF8ToUTF16(" world!"),
            slicer_begin.CutString(7, true));
  EXPECT_EQ(kEllipsisUTF16 + UTF8ToUTF16(", world!"),
            slicer_begin.CutString(8, true));

  // Eliding the middle of a string should *NOT* result in whitespace being
  // removed around the ellipsis in no-trim mode.
  StringSlicer slicer_mid(text, ellipsis, true, false, false);
  text = UTF8ToUTF16("Hey world!");
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("ld!"),
            slicer_mid.CutString(6, true));
  EXPECT_EQ(UTF8ToUTF16("Hey ") + kEllipsisUTF16 + UTF8ToUTF16("ld!"),
            slicer_mid.CutString(7, true));
  EXPECT_EQ(UTF8ToUTF16("Hey ") + kEllipsisUTF16 + UTF8ToUTF16("rld!"),
            slicer_mid.CutString(8, true));
}

TEST(TextEliderTest, StringSlicerWhitespace_Trim) {
  // Must store strings in variables (StringSlicer retains a reference to them).
  base::string16 text(UTF8ToUTF16("Hello, world!"));
  base::string16 ellipsis(kEllipsisUTF16);

  // Eliding the end of a string should result in whitespace being removed
  // before the ellipsis in trim mode.
  StringSlicer slicer_end(text, ellipsis, false, false, true);
  EXPECT_EQ(UTF8ToUTF16("Hello,") + kEllipsisUTF16,
            slicer_end.CutString(6, true));
  EXPECT_EQ(UTF8ToUTF16("Hello,") + kEllipsisUTF16,
            slicer_end.CutString(7, true));
  EXPECT_EQ(UTF8ToUTF16("Hello, w") + kEllipsisUTF16,
            slicer_end.CutString(8, true));

  // Eliding the start of a string should result in whitespace being removed
  // after the ellipsis in trim mode.
  StringSlicer slicer_begin(text, ellipsis, false, true, true);
  EXPECT_EQ(kEllipsisUTF16 + UTF8ToUTF16("world!"),
            slicer_begin.CutString(6, true));
  EXPECT_EQ(kEllipsisUTF16 + UTF8ToUTF16("world!"),
            slicer_begin.CutString(7, true));
  EXPECT_EQ(kEllipsisUTF16 + UTF8ToUTF16(", world!"),
            slicer_begin.CutString(8, true));

  // Eliding the middle of a string *should* result in whitespace being removed
  // around the ellipsis in trim mode.
  StringSlicer slicer_mid(text, ellipsis, true, false, true);
  text = UTF8ToUTF16("Hey world!");
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("ld!"),
            slicer_mid.CutString(6, true));
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("ld!"),
            slicer_mid.CutString(7, true));
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("rld!"),
            slicer_mid.CutString(8, true));
}

TEST(TextEliderTest, StringSlicer_ElideMiddle_MultipleWhitespace) {
  // Must store strings in variables (StringSlicer retains a reference to them).
  base::string16 text(UTF8ToUTF16("Hello  world!"));
  base::string16 ellipsis(kEllipsisUTF16);

  // Eliding the middle of a string should not result in whitespace being
  // removed around the ellipsis in default whitespace mode.
  StringSlicer slicer_default(text, ellipsis, true, false);
  text = UTF8ToUTF16("Hey  U  man");
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("man"),
            slicer_default.CutString(6, true));
  EXPECT_EQ(UTF8ToUTF16("Hey ") + kEllipsisUTF16 + UTF8ToUTF16("man"),
            slicer_default.CutString(7, true));
  EXPECT_EQ(UTF8ToUTF16("Hey ") + kEllipsisUTF16 + UTF8ToUTF16(" man"),
            slicer_default.CutString(8, true));
  EXPECT_EQ(UTF8ToUTF16("Hey  ") + kEllipsisUTF16 + UTF8ToUTF16(" man"),
            slicer_default.CutString(9, true));
  EXPECT_EQ(UTF8ToUTF16("Hey  ") + kEllipsisUTF16 + UTF8ToUTF16("  man"),
            slicer_default.CutString(10, true));

  // Eliding the middle of a string should not result in whitespace being
  // removed around the ellipsis in no-trim mode.
  StringSlicer slicer_notrim(text, ellipsis, true, false, false);
  text = UTF8ToUTF16("Hey  U  man");
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("man"),
            slicer_notrim.CutString(6, true));
  EXPECT_EQ(UTF8ToUTF16("Hey ") + kEllipsisUTF16 + UTF8ToUTF16("man"),
            slicer_notrim.CutString(7, true));
  EXPECT_EQ(UTF8ToUTF16("Hey ") + kEllipsisUTF16 + UTF8ToUTF16(" man"),
            slicer_notrim.CutString(8, true));
  EXPECT_EQ(UTF8ToUTF16("Hey  ") + kEllipsisUTF16 + UTF8ToUTF16(" man"),
            slicer_notrim.CutString(9, true));
  EXPECT_EQ(UTF8ToUTF16("Hey  ") + kEllipsisUTF16 + UTF8ToUTF16("  man"),
            slicer_notrim.CutString(10, true));

  // Eliding the middle of a string *should* result in whitespace being removed
  // around the ellipsis in trim mode.
  StringSlicer slicer_trim(text, ellipsis, true, false, true);
  text = UTF8ToUTF16("Hey  U  man");
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("man"),
            slicer_trim.CutString(6, true));
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("man"),
            slicer_trim.CutString(7, true));
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("man"),
            slicer_trim.CutString(8, true));
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("man"),
            slicer_trim.CutString(9, true));
  EXPECT_EQ(UTF8ToUTF16("Hey") + kEllipsisUTF16 + UTF8ToUTF16("man"),
            slicer_trim.CutString(10, true));
}

TEST(TextEliderTest, StringSlicerSurrogate) {
  // The below is 'MUSICAL SYMBOL G CLEF' (U+1D11E), which is represented in
  // UTF-16 as two code units forming a surrogate pair: 0xD834 0xDD1E.
  const base::char16 kSurrogate[] = {0xD834, 0xDD1E, 0};
  base::string16 text(UTF8ToUTF16("abc") + kSurrogate + UTF8ToUTF16("xyz"));
  base::string16 ellipsis(kEllipsisUTF16);
  StringSlicer slicer(text, ellipsis, false, false);

  // Cut surrogate on the right. Should round left and exclude the surrogate.
  EXPECT_EQ(base::string16(kEllipsisUTF16), slicer.CutString(0, true));
  EXPECT_EQ(UTF8ToUTF16("abc") + kEllipsisUTF16, slicer.CutString(4, true));
  EXPECT_EQ(text + kEllipsisUTF16, slicer.CutString(text.length(), true));

  // Cut surrogate on the left. Should round right and exclude the surrogate.
  StringSlicer slicer_begin(text, ellipsis, false, true);
  EXPECT_EQ(base::string16(kEllipsisUTF16) + UTF8ToUTF16("xyz"),
            slicer_begin.CutString(4, true));

  // Cut surrogate in the middle. Should round right and exclude the surrogate.
  base::string16 short_text(UTF8ToUTF16("abc") + kSurrogate);
  StringSlicer slicer_mid(short_text, ellipsis, true, false);
  EXPECT_EQ(UTF8ToUTF16("a") + kEllipsisUTF16, slicer_mid.CutString(2, true));

  // String that starts with a dangling trailing surrogate.
  base::char16 dangling_trailing_chars[] = {kSurrogate[1], 0};
  base::string16 dangling_trailing_text(dangling_trailing_chars);
  StringSlicer slicer_dangling_trailing(dangling_trailing_text, ellipsis, false,
                                        false);
  EXPECT_EQ(base::string16(kEllipsisUTF16),
            slicer_dangling_trailing.CutString(0, true));
  EXPECT_EQ(dangling_trailing_text + kEllipsisUTF16,
            slicer_dangling_trailing.CutString(1, true));
}

TEST(TextEliderTest, StringSlicerCombining) {
  // The following string contains three combining character sequences (one for
  // each category of combining mark):
  // LATIN SMALL LETTER E + COMBINING ACUTE ACCENT + COMBINING CEDILLA
  // LATIN SMALL LETTER X + COMBINING ENCLOSING KEYCAP
  // DEVANAGARI LETTER DDA + DEVANAGARI VOWEL SIGN I
  const base::char16 kText[] = {
      'e', 0x301, 0x327, ' ', 'x', 0x20E3, ' ', 0x921, 0x93F, 0};
  base::string16 text(kText);
  base::string16 ellipsis(kEllipsisUTF16);
  StringSlicer slicer(text, ellipsis, false, false);

  // Attempt to cut the string for all lengths. When a combining sequence is
  // cut, it should always round left and exclude the combining sequence.
  // Whitespace is also cut adjacent to the ellipsis.

  // First sequence:
  EXPECT_EQ(base::string16(kEllipsisUTF16), slicer.CutString(0, true));
  EXPECT_EQ(base::string16(kEllipsisUTF16), slicer.CutString(1, true));
  EXPECT_EQ(base::string16(kEllipsisUTF16), slicer.CutString(2, true));
  EXPECT_EQ(text.substr(0, 3) + kEllipsisUTF16, slicer.CutString(3, true));
  // Second sequence:
  EXPECT_EQ(text.substr(0, 3) + kEllipsisUTF16, slicer.CutString(4, true));
  EXPECT_EQ(text.substr(0, 3) + kEllipsisUTF16, slicer.CutString(5, true));
  EXPECT_EQ(text.substr(0, 6) + kEllipsisUTF16, slicer.CutString(6, true));
  // Third sequence:
  EXPECT_EQ(text.substr(0, 6) + kEllipsisUTF16, slicer.CutString(7, true));
  EXPECT_EQ(text.substr(0, 6) + kEllipsisUTF16, slicer.CutString(8, true));
  EXPECT_EQ(text + kEllipsisUTF16, slicer.CutString(9, true));

  // Cut string in the middle, splitting the second sequence in half. Should
  // round both left and right, excluding the second sequence.
  StringSlicer slicer_mid(text, ellipsis, true, false);
  EXPECT_EQ(text.substr(0, 4) + kEllipsisUTF16 + text.substr(6),
            slicer_mid.CutString(9, true));

  // String that starts with a dangling combining mark.
  base::char16 dangling_mark_chars[] = {text[1], 0};
  base::string16 dangling_mark_text(dangling_mark_chars);
  StringSlicer slicer_dangling_mark(dangling_mark_text, ellipsis, false, false);
  EXPECT_EQ(base::string16(kEllipsisUTF16),
            slicer_dangling_mark.CutString(0, true));
  EXPECT_EQ(dangling_mark_text + kEllipsisUTF16,
            slicer_dangling_mark.CutString(1, true));
}

TEST(TextEliderTest, StringSlicerCombiningSurrogate) {
  // The ultimate test: combining sequences comprised of surrogate pairs.
  // The following string contains a single combining character sequence:
  // MUSICAL SYMBOL G CLEF (U+1D11E) + MUSICAL SYMBOL COMBINING FLAG-1 (U+1D16E)
  // Represented as four UTF-16 code units.
  const base::char16 kText[] = {0xD834, 0xDD1E, 0xD834, 0xDD6E, 0};
  base::string16 text(kText);
  base::string16 ellipsis(kEllipsisUTF16);
  StringSlicer slicer(text, ellipsis, false, false);

  // Attempt to cut the string for all lengths. Should always round left and
  // exclude the combining sequence.
  EXPECT_EQ(base::string16(kEllipsisUTF16), slicer.CutString(0, true));
  EXPECT_EQ(base::string16(kEllipsisUTF16), slicer.CutString(1, true));
  EXPECT_EQ(base::string16(kEllipsisUTF16), slicer.CutString(2, true));
  EXPECT_EQ(base::string16(kEllipsisUTF16), slicer.CutString(3, true));
  EXPECT_EQ(text + kEllipsisUTF16, slicer.CutString(4, true));

  // Cut string in the middle. Should exclude the sequence.
  StringSlicer slicer_mid(text, ellipsis, true, false);
  EXPECT_EQ(base::string16(kEllipsisUTF16), slicer_mid.CutString(4, true));
}

TEST(TextEliderTest, ElideString) {
  struct TestData {
    const char* input;
    size_t max_len;
    bool result;
    const char* output;
  } cases[] = {
    { "Hello", 0, true, "" },
    { "", 0, false, "" },
    { "Hello, my name is Tom", 1, true, "H" },
    { "Hello, my name is Tom", 2, true, "He" },
    { "Hello, my name is Tom", 3, true, "H.m" },
    { "Hello, my name is Tom", 4, true, "H..m" },
    { "Hello, my name is Tom", 5, true, "H...m" },
    { "Hello, my name is Tom", 6, true, "He...m" },
    { "Hello, my name is Tom", 7, true, "He...om" },
    { "Hello, my name is Tom", 10, true, "Hell...Tom" },
    { "Hello, my name is Tom", 100, false, "Hello, my name is Tom" }
  };
  for (size_t i = 0; i < base::size(cases); ++i) {
    base::string16 output;
    EXPECT_EQ(cases[i].result,
              ElideString(UTF8ToUTF16(cases[i].input),
                          cases[i].max_len, &output));
    EXPECT_EQ(cases[i].output, UTF16ToUTF8(output));
  }
}

TEST(TextEliderTest, ElideRectangleText) {
  const FontList font_list;
  const int line_height = font_list.GetHeight();
  const float test_width = GetStringWidthF(ASCIIToUTF16("Test"), font_list);

  struct TestData {
    const char* input;
    float available_pixel_width;
    int available_pixel_height;
    bool truncated_y;
    const char* output;
  } cases[] = {
      {"", 0, 0, false, nullptr},
      {"", 1, 1, false, nullptr},
      {"Test", test_width, 0, true, nullptr},
      {"Test", test_width, 1, false, "Test"},
      {"Test", test_width, line_height, false, "Test"},
      {"Test Test", test_width, line_height, true, "Test"},
      {"Test Test", test_width, line_height + 1, false, "Test|Test"},
      {"Test Test", test_width, line_height * 2, false, "Test|Test"},
      {"Test Test", test_width, line_height * 3, false, "Test|Test"},
      {"Test Test", test_width * 2, line_height * 2, false, "Test|Test"},
      {"Test Test", test_width * 3, line_height, false, "Test Test"},
      {"Test\nTest", test_width * 3, line_height * 2, false, "Test|Test"},
      {"Te\nst Te", test_width, line_height * 3, false, "Te|st|Te"},
      {"\nTest", test_width, line_height * 2, false, "|Test"},
      {"\nTest", test_width, line_height, true, ""},
      {"\n\nTest", test_width, line_height * 3, false, "||Test"},
      {"\n\nTest", test_width, line_height * 2, true, "|"},
      {"Test\n", 2 * test_width, line_height * 5, false, "Test|"},
      {"Test\n\n", 2 * test_width, line_height * 5, false, "Test||"},
      {"Test\n\n\n", 2 * test_width, line_height * 5, false, "Test|||"},
      {"Test\nTest\n\n", 2 * test_width, line_height * 5, false, "Test|Test||"},
      {"Test\n\nTest\n", 2 * test_width, line_height * 5, false, "Test||Test|"},
      {"Test\n\n\nTest", 2 * test_width, line_height * 5, false, "Test|||Test"},
      {"Te ", test_width, line_height, false, "Te"},
      {"Te  Te Test", test_width, 3 * line_height, false, "Te|Te|Test"},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    std::vector<base::string16> lines;
    EXPECT_EQ(cases[i].truncated_y ? INSUFFICIENT_SPACE_VERTICAL : 0,
              ElideRectangleText(UTF8ToUTF16(cases[i].input),
                                 font_list,
                                 cases[i].available_pixel_width,
                                 cases[i].available_pixel_height,
                                 TRUNCATE_LONG_WORDS,
                                 &lines));
    if (cases[i].output) {
      const std::string result =
          UTF16ToUTF8(base::JoinString(lines, ASCIIToUTF16("|")));
      EXPECT_EQ(cases[i].output, result) << "Case " << i << " failed!";
    } else {
      EXPECT_TRUE(lines.empty()) << "Case " << i << " failed!";
    }
  }
}

TEST(TextEliderTest, ElideRectangleTextFirstWordTruncated) {
  const FontList font_list;
  const int line_height = font_list.GetHeight();

  const float test_width = GetStringWidthF(ASCIIToUTF16("Test"), font_list);
  const float tes_width = GetStringWidthF(ASCIIToUTF16("Tes"), font_list);

  std::vector<base::string16> lines;

  auto result_for_width = [&](const char* input, float width) {
    lines.clear();
    return ElideRectangleText(ASCIIToUTF16(input), font_list, width,
                              line_height * 4, WRAP_LONG_WORDS, &lines);
  };

  // Test base case.
  EXPECT_EQ(0, result_for_width("Test", test_width));
  EXPECT_EQ(1u, lines.size());
  EXPECT_EQ(ASCIIToUTF16("Test"), lines[0]);

  // First word truncated.
  EXPECT_EQ(INSUFFICIENT_SPACE_FOR_FIRST_WORD,
            result_for_width("Test", tes_width));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ(ASCIIToUTF16("Tes"), lines[0]);
  EXPECT_EQ(ASCIIToUTF16("t"), lines[1]);

  // Two words truncated.
  EXPECT_EQ(INSUFFICIENT_SPACE_FOR_FIRST_WORD,
            result_for_width("Test\nTest", tes_width));
  EXPECT_EQ(4u, lines.size());
  EXPECT_EQ(ASCIIToUTF16("Tes"), lines[0]);
  EXPECT_EQ(ASCIIToUTF16("t"), lines[1]);
  EXPECT_EQ(ASCIIToUTF16("Tes"), lines[2]);
  EXPECT_EQ(ASCIIToUTF16("t"), lines[3]);

  // Word truncated, but not the first.
  EXPECT_EQ(0, result_for_width("T Test", tes_width));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ(ASCIIToUTF16("T"), lines[0]);
  EXPECT_EQ(ASCIIToUTF16("Tes"), lines[1]);
  EXPECT_EQ(ASCIIToUTF16("t"), lines[2]);

  // Leading \n.
  EXPECT_EQ(0, result_for_width("\nTest", tes_width));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ(ASCIIToUTF16(""), lines[0]);
  EXPECT_EQ(ASCIIToUTF16("Tes"), lines[1]);
  EXPECT_EQ(ASCIIToUTF16("t"), lines[2]);
}

TEST(TextEliderTest, ElideRectangleTextPunctuation) {
  const FontList font_list;
  const int line_height = font_list.GetHeight();
  const float test_width = GetStringWidthF(ASCIIToUTF16("Test"), font_list);
  const float test_t_width = GetStringWidthF(ASCIIToUTF16("Test T"), font_list);
  constexpr int kResultMask =
      INSUFFICIENT_SPACE_HORIZONTAL | INSUFFICIENT_SPACE_VERTICAL;

  struct TestData {
    const char* input;
    float available_pixel_width;
    int available_pixel_height;
    bool wrap_words;
    bool truncated_x;
    const char* output;
  } cases[] = {
    { "Test T.", test_t_width, line_height * 2, false, false, "Test|T." },
    { "Test T ?", test_t_width, line_height * 2, false, false, "Test|T ?" },
    { "Test. Test", test_width, line_height * 3, false, true, "Test|Test" },
    { "Test. Test", test_width, line_height * 3, true, false, "Test|.|Test" },
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    std::vector<base::string16> lines;
    const WordWrapBehavior wrap_behavior =
        (cases[i].wrap_words ? WRAP_LONG_WORDS : TRUNCATE_LONG_WORDS);
    EXPECT_EQ(cases[i].truncated_x ? INSUFFICIENT_SPACE_HORIZONTAL : 0,
              ElideRectangleText(UTF8ToUTF16(cases[i].input), font_list,
                                 cases[i].available_pixel_width,
                                 cases[i].available_pixel_height, wrap_behavior,
                                 &lines) &
                  kResultMask);
    if (cases[i].output) {
      const std::string result =
          UTF16ToUTF8(base::JoinString(lines, base::ASCIIToUTF16("|")));
      EXPECT_EQ(cases[i].output, result) << "Case " << i << " failed!";
    } else {
      EXPECT_TRUE(lines.empty()) << "Case " << i << " failed!";
    }
  }
}

TEST(TextEliderTest, ElideRectangleTextLongWords) {
  const FontList font_list;
  const int kAvailableHeight = 1000;
  const base::string16 kElidedTesting =
      UTF8ToUTF16(std::string("Tes") + kEllipsis);
  const float elided_width = GetStringWidthF(kElidedTesting, font_list);
  const float test_width = GetStringWidthF(ASCIIToUTF16("Test"), font_list);
  constexpr int kResultMask =
      INSUFFICIENT_SPACE_HORIZONTAL | INSUFFICIENT_SPACE_VERTICAL;

  struct TestData {
    const char* input;
    float available_pixel_width;
    WordWrapBehavior wrap_behavior;
    bool truncated_x;
    const char* output;
  } cases[] = {
    { "Testing", test_width, IGNORE_LONG_WORDS, false, "Testing" },
    { "X Testing", test_width, IGNORE_LONG_WORDS, false, "X|Testing" },
    { "Test Testing", test_width, IGNORE_LONG_WORDS, false, "Test|Testing" },
    { "Test\nTesting", test_width, IGNORE_LONG_WORDS, false, "Test|Testing" },
    { "Test Tests ", test_width, IGNORE_LONG_WORDS, false, "Test|Tests" },
    { "Test Tests T", test_width, IGNORE_LONG_WORDS, false, "Test|Tests|T" },

    { "Testing", elided_width, ELIDE_LONG_WORDS, true, "Tes..." },
    { "X Testing", elided_width, ELIDE_LONG_WORDS, true, "X|Tes..." },
    { "Test Testing", elided_width, ELIDE_LONG_WORDS, true, "Test|Tes..." },
    { "Test\nTesting", elided_width, ELIDE_LONG_WORDS, true, "Test|Tes..." },

    { "Testing", test_width, TRUNCATE_LONG_WORDS, true, "Test" },
    { "X Testing", test_width, TRUNCATE_LONG_WORDS, true, "X|Test" },
    { "Test Testing", test_width, TRUNCATE_LONG_WORDS, true, "Test|Test" },
    { "Test\nTesting", test_width, TRUNCATE_LONG_WORDS, true, "Test|Test" },
    { "Test Tests ", test_width, TRUNCATE_LONG_WORDS, true, "Test|Test" },
    { "Test Tests T", test_width, TRUNCATE_LONG_WORDS, true, "Test|Test|T" },

    { "Testing", test_width, WRAP_LONG_WORDS, false, "Test|ing" },
    { "X Testing", test_width, WRAP_LONG_WORDS, false, "X|Test|ing" },
    { "Test Testing", test_width, WRAP_LONG_WORDS, false, "Test|Test|ing" },
    { "Test\nTesting", test_width, WRAP_LONG_WORDS, false, "Test|Test|ing" },
    { "Test Tests ", test_width, WRAP_LONG_WORDS, false, "Test|Test|s" },
    { "Test Tests T", test_width, WRAP_LONG_WORDS, false, "Test|Test|s T" },
    { "TestTestTest", test_width, WRAP_LONG_WORDS, false, "Test|Test|Test" },
    { "TestTestTestT", test_width, WRAP_LONG_WORDS, false, "Test|Test|Test|T" },
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    std::vector<base::string16> lines;
    EXPECT_EQ(
        cases[i].truncated_x ? INSUFFICIENT_SPACE_HORIZONTAL : 0,
        ElideRectangleText(UTF8ToUTF16(cases[i].input), font_list,
                           cases[i].available_pixel_width, kAvailableHeight,
                           cases[i].wrap_behavior, &lines) &
            kResultMask);
    std::string expected_output(cases[i].output);
    base::ReplaceSubstringsAfterOffset(&expected_output, 0, "...", kEllipsis);
    const std::string result =
        UTF16ToUTF8(base::JoinString(lines, base::ASCIIToUTF16("|")));
    EXPECT_EQ(expected_output, result) << "Case " << i << " failed!";
  }
}

// This test is to make sure that the width of each wrapped line does not
// exceed the available width. On some platform like Mac, this test used to
// fail because the truncated integer width is returned for the string
// and the accumulation of the truncated values causes the elide function
// to wrap incorrectly.
TEST(TextEliderTest, ElideRectangleTextCheckLineWidth) {
  FontList font_list;
#if defined(OS_MAC)
  // Use a specific font to expose the line width exceeding problem.
  font_list = FontList(Font("LucidaGrande", 12));
#endif
  const float kAvailableWidth = 235;
  const int kAvailableHeight = 1000;
  const char text[] = "that Russian place we used to go to after fencing";
  std::vector<base::string16> lines;
  EXPECT_EQ(0, ElideRectangleText(UTF8ToUTF16(text),
                                  font_list,
                                  kAvailableWidth,
                                  kAvailableHeight,
                                  WRAP_LONG_WORDS,
                                  &lines));
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
#define WIDTH(x) GetStringWidthF(UTF8ToUTF16(x), font_list)
  EXPECT_EQ(WIDTH("The administrator for this account has"),
            WIDTH("The ") + WIDTH("administrator ") + WIDTH("for ") +
                WIDTH("this ") + WIDTH("account ") + WIDTH("has"));
#undef WIDTH
  SetFontRenderParamsDeviceScaleFactor(1.0f);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST(TextEliderTest, ElideRectangleString) {
  struct TestData {
    const char* input;
    int max_rows;
    int max_cols;
    bool result;
    const char* output;
  } cases[] = {
    { "", 0, 0, false, "" },
    { "", 1, 1, false, "" },
    { "Hi, my name is\nTom", 0, 0,  true,  "..." },
    { "Hi, my name is\nTom", 1, 0,  true,  "\n..." },
    { "Hi, my name is\nTom", 0, 1,  true,  "..." },
    { "Hi, my name is\nTom", 1, 1,  true,  "H\n..." },
    { "Hi, my name is\nTom", 2, 1,  true,  "H\ni\n..." },
    { "Hi, my name is\nTom", 3, 1,  true,  "H\ni\n,\n..." },
    { "Hi, my name is\nTom", 4, 1,  true,  "H\ni\n,\n \n..." },
    { "Hi, my name is\nTom", 5, 1,  true,  "H\ni\n,\n \nm\n..." },
    { "Hi, my name is\nTom", 0, 2,  true,  "..." },
    { "Hi, my name is\nTom", 1, 2,  true,  "Hi\n..." },
    { "Hi, my name is\nTom", 2, 2,  true,  "Hi\n, \n..." },
    { "Hi, my name is\nTom", 3, 2,  true,  "Hi\n, \nmy\n..." },
    { "Hi, my name is\nTom", 4, 2,  true,  "Hi\n, \nmy\n n\n..." },
    { "Hi, my name is\nTom", 5, 2,  true,  "Hi\n, \nmy\n n\nam\n..." },
    { "Hi, my name is\nTom", 0, 3,  true,  "..." },
    { "Hi, my name is\nTom", 1, 3,  true,  "Hi,\n..." },
    { "Hi, my name is\nTom", 2, 3,  true,  "Hi,\n my\n..." },
    { "Hi, my name is\nTom", 3, 3,  true,  "Hi,\n my\n na\n..." },
    { "Hi, my name is\nTom", 4, 3,  true,  "Hi,\n my\n na\nme \n..." },
    { "Hi, my name is\nTom", 5, 3,  true,  "Hi,\n my\n na\nme \nis\n..." },
    { "Hi, my name is\nTom", 1, 4,  true,  "Hi, \n..." },
    { "Hi, my name is\nTom", 2, 4,  true,  "Hi, \nmy n\n..." },
    { "Hi, my name is\nTom", 3, 4,  true,  "Hi, \nmy n\name \n..." },
    { "Hi, my name is\nTom", 4, 4,  true,  "Hi, \nmy n\name \nis\n..." },
    { "Hi, my name is\nTom", 5, 4,  false, "Hi, \nmy n\name \nis\nTom" },
    { "Hi, my name is\nTom", 1, 5,  true,  "Hi, \n..." },
    { "Hi, my name is\nTom", 2, 5,  true,  "Hi, \nmy na\n..." },
    { "Hi, my name is\nTom", 3, 5,  true,  "Hi, \nmy na\nme \n..." },
    { "Hi, my name is\nTom", 4, 5,  true,  "Hi, \nmy na\nme \nis\n..." },
    { "Hi, my name is\nTom", 5, 5,  false, "Hi, \nmy na\nme \nis\nTom" },
    { "Hi, my name is\nTom", 1, 6,  true,  "Hi, \n..." },
    { "Hi, my name is\nTom", 2, 6,  true,  "Hi, \nmy \n..." },
    { "Hi, my name is\nTom", 3, 6,  true,  "Hi, \nmy \nname \n..." },
    { "Hi, my name is\nTom", 4, 6,  true,  "Hi, \nmy \nname \nis\n..." },
    { "Hi, my name is\nTom", 5, 6,  false, "Hi, \nmy \nname \nis\nTom" },
    { "Hi, my name is\nTom", 1, 7,  true,  "Hi, \n..." },
    { "Hi, my name is\nTom", 2, 7,  true,  "Hi, \nmy \n..." },
    { "Hi, my name is\nTom", 3, 7,  true,  "Hi, \nmy \nname \n..." },
    { "Hi, my name is\nTom", 4, 7,  true,  "Hi, \nmy \nname \nis\n..." },
    { "Hi, my name is\nTom", 5, 7,  false, "Hi, \nmy \nname \nis\nTom" },
    { "Hi, my name is\nTom", 1, 8,  true,  "Hi, my \n..." },
    { "Hi, my name is\nTom", 2, 8,  true,  "Hi, my \nname \n..." },
    { "Hi, my name is\nTom", 3, 8,  true,  "Hi, my \nname \nis\n..." },
    { "Hi, my name is\nTom", 4, 8,  false, "Hi, my \nname \nis\nTom" },
    { "Hi, my name is\nTom", 1, 9,  true,  "Hi, my \n..." },
    { "Hi, my name is\nTom", 2, 9,  true,  "Hi, my \nname is\n..." },
    { "Hi, my name is\nTom", 3, 9,  false, "Hi, my \nname is\nTom" },
    { "Hi, my name is\nTom", 1, 10, true,  "Hi, my \n..." },
    { "Hi, my name is\nTom", 2, 10, true,  "Hi, my \nname is\n..." },
    { "Hi, my name is\nTom", 3, 10, false, "Hi, my \nname is\nTom" },
    { "Hi, my name is\nTom", 1, 11, true,  "Hi, my \n..." },
    { "Hi, my name is\nTom", 2, 11, true,  "Hi, my \nname is\n..." },
    { "Hi, my name is\nTom", 3, 11, false, "Hi, my \nname is\nTom" },
    { "Hi, my name is\nTom", 1, 12, true,  "Hi, my \n..." },
    { "Hi, my name is\nTom", 2, 12, true,  "Hi, my \nname is\n..." },
    { "Hi, my name is\nTom", 3, 12, false, "Hi, my \nname is\nTom" },
    { "Hi, my name is\nTom", 1, 13, true,  "Hi, my name \n..." },
    { "Hi, my name is\nTom", 2, 13, true,  "Hi, my name \nis\n..." },
    { "Hi, my name is\nTom", 3, 13, false, "Hi, my name \nis\nTom" },
    { "Hi, my name is\nTom", 1, 20, true,  "Hi, my name is\n..." },
    { "Hi, my name is\nTom", 2, 20, false, "Hi, my name is\nTom" },
    { "Hi, my name is Tom",  1, 40, false, "Hi, my name is Tom" },
  };
  base::string16 output;
  for (size_t i = 0; i < base::size(cases); ++i) {
    EXPECT_EQ(cases[i].result,
              ElideRectangleString(UTF8ToUTF16(cases[i].input),
                                   cases[i].max_rows, cases[i].max_cols,
                                   true, &output));
    EXPECT_EQ(cases[i].output, UTF16ToUTF8(output));
  }
}

TEST(TextEliderTest, ElideRectangleStringNotStrict) {
  struct TestData {
    const char* input;
    int max_rows;
    int max_cols;
    bool result;
    const char* output;
  } cases[] = {
    { "", 0, 0, false, "" },
    { "", 1, 1, false, "" },
    { "Hi, my name_is\nDick", 0, 0,  true,  "..." },
    { "Hi, my name_is\nDick", 1, 0,  true,  "\n..." },
    { "Hi, my name_is\nDick", 0, 1,  true,  "..." },
    { "Hi, my name_is\nDick", 1, 1,  true,  "H\n..." },
    { "Hi, my name_is\nDick", 2, 1,  true,  "H\ni\n..." },
    { "Hi, my name_is\nDick", 3, 1,  true,  "H\ni\n,\n..." },
    { "Hi, my name_is\nDick", 4, 1,  true,  "H\ni\n,\n \n..." },
    { "Hi, my name_is\nDick", 5, 1,  true,  "H\ni\n,\n \nm\n..." },
    { "Hi, my name_is\nDick", 0, 2,  true,  "..." },
    { "Hi, my name_is\nDick", 1, 2,  true,  "Hi\n..." },
    { "Hi, my name_is\nDick", 2, 2,  true,  "Hi\n, \n..." },
    { "Hi, my name_is\nDick", 3, 2,  true,  "Hi\n, \nmy\n..." },
    { "Hi, my name_is\nDick", 4, 2,  true,  "Hi\n, \nmy\n n\n..." },
    { "Hi, my name_is\nDick", 5, 2,  true,  "Hi\n, \nmy\n n\nam\n..." },
    { "Hi, my name_is\nDick", 0, 3,  true,  "..." },
    { "Hi, my name_is\nDick", 1, 3,  true,  "Hi,\n..." },
    { "Hi, my name_is\nDick", 2, 3,  true,  "Hi,\n my\n..." },
    { "Hi, my name_is\nDick", 3, 3,  true,  "Hi,\n my\n na\n..." },
    { "Hi, my name_is\nDick", 4, 3,  true,  "Hi,\n my\n na\nme_\n..." },
    { "Hi, my name_is\nDick", 5, 3,  true,  "Hi,\n my\n na\nme_\nis\n..." },
    { "Hi, my name_is\nDick", 1, 4,  true,  "Hi, ..." },
    { "Hi, my name_is\nDick", 2, 4,  true,  "Hi, my n\n..." },
    { "Hi, my name_is\nDick", 3, 4,  true,  "Hi, my n\name_\n..." },
    { "Hi, my name_is\nDick", 4, 4,  true,  "Hi, my n\name_\nis\n..." },
    { "Hi, my name_is\nDick", 5, 4,  false, "Hi, my n\name_\nis\nDick" },
    { "Hi, my name_is\nDick", 1, 5,  true,  "Hi, ..." },
    { "Hi, my name_is\nDick", 2, 5,  true,  "Hi, my na\n..." },
    { "Hi, my name_is\nDick", 3, 5,  true,  "Hi, my na\nme_is\n..." },
    { "Hi, my name_is\nDick", 4, 5,  true,  "Hi, my na\nme_is\n\n..." },
    { "Hi, my name_is\nDick", 5, 5,  false, "Hi, my na\nme_is\n\nDick" },
    { "Hi, my name_is\nDick", 1, 6,  true,  "Hi, ..." },
    { "Hi, my name_is\nDick", 2, 6,  true,  "Hi, my nam\n..." },
    { "Hi, my name_is\nDick", 3, 6,  true,  "Hi, my nam\ne_is\n..." },
    { "Hi, my name_is\nDick", 4, 6,  false, "Hi, my nam\ne_is\nDick" },
    { "Hi, my name_is\nDick", 5, 6,  false, "Hi, my nam\ne_is\nDick" },
    { "Hi, my name_is\nDick", 1, 7,  true,  "Hi, ..." },
    { "Hi, my name_is\nDick", 2, 7,  true,  "Hi, my name\n..." },
    { "Hi, my name_is\nDick", 3, 7,  true,  "Hi, my name\n_is\n..." },
    { "Hi, my name_is\nDick", 4, 7,  false, "Hi, my name\n_is\nDick" },
    { "Hi, my name_is\nDick", 5, 7,  false, "Hi, my name\n_is\nDick" },
    { "Hi, my name_is\nDick", 1, 8,  true,  "Hi, my n\n..." },
    { "Hi, my name_is\nDick", 2, 8,  true,  "Hi, my n\name_is\n..." },
    { "Hi, my name_is\nDick", 3, 8,  false, "Hi, my n\name_is\nDick" },
    { "Hi, my name_is\nDick", 1, 9,  true,  "Hi, my ..." },
    { "Hi, my name_is\nDick", 2, 9,  true,  "Hi, my name_is\n..." },
    { "Hi, my name_is\nDick", 3, 9,  false, "Hi, my name_is\nDick" },
    { "Hi, my name_is\nDick", 1, 10, true,  "Hi, my ..." },
    { "Hi, my name_is\nDick", 2, 10, true,  "Hi, my name_is\n..." },
    { "Hi, my name_is\nDick", 3, 10, false, "Hi, my name_is\nDick" },
    { "Hi, my name_is\nDick", 1, 11, true,  "Hi, my ..." },
    { "Hi, my name_is\nDick", 2, 11, true,  "Hi, my name_is\n..." },
    { "Hi, my name_is\nDick", 3, 11, false, "Hi, my name_is\nDick" },
    { "Hi, my name_is\nDick", 1, 12, true,  "Hi, my ..." },
    { "Hi, my name_is\nDick", 2, 12, true,  "Hi, my name_is\n..." },
    { "Hi, my name_is\nDick", 3, 12, false, "Hi, my name_is\nDick" },
    { "Hi, my name_is\nDick", 1, 13, true,  "Hi, my ..." },
    { "Hi, my name_is\nDick", 2, 13, true,  "Hi, my name_is\n..." },
    { "Hi, my name_is\nDick", 3, 13, false, "Hi, my name_is\nDick" },
    { "Hi, my name_is\nDick", 1, 20, true,  "Hi, my name_is\n..." },
    { "Hi, my name_is\nDick", 2, 20, false, "Hi, my name_is\nDick" },
    { "Hi, my name_is Dick",  1, 40, false, "Hi, my name_is Dick" },
  };
  base::string16 output;
  for (size_t i = 0; i < base::size(cases); ++i) {
    EXPECT_EQ(cases[i].result,
              ElideRectangleString(UTF8ToUTF16(cases[i].input),
                                   cases[i].max_rows, cases[i].max_cols,
                                   false, &output));
    EXPECT_EQ(cases[i].output, UTF16ToUTF8(output));
  }
}

TEST(TextEliderTest, ElideRectangleWide16) {
  // Two greek words separated by space.
  const base::string16 str(WideToUTF16(
      L"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
      L"\x03bf\x03c2\x0020\x0399\x03c3\x03c4\x03cc\x03c2"));
  const base::string16 out1(WideToUTF16(
      L"\x03a0\x03b1\x03b3\x03ba\n"
      L"\x03cc\x03c3\x03bc\x03b9\n"
      L"..."));
  const base::string16 out2(WideToUTF16(
      L"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9\x03bf\x03c2\x0020\n"
      L"\x0399\x03c3\x03c4\x03cc\x03c2"));
  base::string16 output;
  EXPECT_TRUE(ElideRectangleString(str, 2, 4, true, &output));
  EXPECT_EQ(out1, output);
  EXPECT_FALSE(ElideRectangleString(str, 2, 12, true, &output));
  EXPECT_EQ(out2, output);
}

TEST(TextEliderTest, ElideRectangleWide32) {
  // Four U+1D49C MATHEMATICAL SCRIPT CAPITAL A followed by space "aaaaa".
  const base::string16 str(UTF8ToUTF16(
      "\xF0\x9D\x92\x9C\xF0\x9D\x92\x9C\xF0\x9D\x92\x9C\xF0\x9D\x92\x9C"
      " aaaaa"));
  const base::string16 out(UTF8ToUTF16(
      "\xF0\x9D\x92\x9C\xF0\x9D\x92\x9C\xF0\x9D\x92\x9C\n"
      "\xF0\x9D\x92\x9C \naaa\n..."));
  base::string16 output;
  EXPECT_TRUE(ElideRectangleString(str, 3, 3, true, &output));
  EXPECT_EQ(out, output);
}

TEST(TextEliderTest, TruncateString) {
  base::string16 str = ASCIIToUTF16("fooooey    bxxxar baz  ");

  // Test breaking at character 0.
  EXPECT_EQ(base::string16(), TruncateString(str, 0, WORD_BREAK));
  EXPECT_EQ(base::string16(), TruncateString(str, 0, CHARACTER_BREAK));

  // Test breaking at character 1.
  EXPECT_EQ(L"\x2026", UTF16ToWide(TruncateString(str, 1, WORD_BREAK)));
  EXPECT_EQ(L"\x2026", UTF16ToWide(TruncateString(str, 1, CHARACTER_BREAK)));

  // Test breaking in the middle of the first word.
  EXPECT_EQ(L"f\x2026", UTF16ToWide(TruncateString(str, 2, WORD_BREAK)));
  EXPECT_EQ(L"f\x2026", UTF16ToWide(TruncateString(str, 2, CHARACTER_BREAK)));

  // Test breaking in between words.
  EXPECT_EQ(L"fooooey\x2026", UTF16ToWide(TruncateString(str, 9, WORD_BREAK)));
  EXPECT_EQ(L"fooooey\x2026",
            UTF16ToWide(TruncateString(str, 9, CHARACTER_BREAK)));

  // Test breaking at the start of a later word.
  EXPECT_EQ(L"fooooey\x2026", UTF16ToWide(TruncateString(str, 11, WORD_BREAK)));
  EXPECT_EQ(L"fooooey\x2026",
            UTF16ToWide(TruncateString(str, 11, CHARACTER_BREAK)));

  // Test breaking in the middle of a word.
  EXPECT_EQ(L"fooooey\x2026", UTF16ToWide(TruncateString(str, 12, WORD_BREAK)));
  EXPECT_EQ(L"fooooey\x2026",
            UTF16ToWide(TruncateString(str, 12, CHARACTER_BREAK)));
  EXPECT_EQ(L"fooooey\x2026", UTF16ToWide(TruncateString(str, 14, WORD_BREAK)));
  EXPECT_EQ(L"fooooey    bx\x2026",
            UTF16ToWide(TruncateString(str, 14, CHARACTER_BREAK)));

  // Test breaking in whitespace at the end of the string.
  EXPECT_EQ(L"fooooey    bxxxar baz\x2026",
            UTF16ToWide(TruncateString(str, 22, WORD_BREAK)));
  EXPECT_EQ(L"fooooey    bxxxar baz\x2026",
            UTF16ToWide(TruncateString(str, 22, CHARACTER_BREAK)));

  // Test breaking at the end of the string.
  EXPECT_EQ(str, TruncateString(str, str.length(), WORD_BREAK));
  EXPECT_EQ(str, TruncateString(str, str.length(), CHARACTER_BREAK));

  // Test breaking past the end of the string.
  EXPECT_EQ(str, TruncateString(str, str.length() + 10, WORD_BREAK));
  EXPECT_EQ(str, TruncateString(str, str.length() + 10, CHARACTER_BREAK));


  // Tests of strings with leading whitespace:
  base::string16 str2 = ASCIIToUTF16("   foo");

  // Test breaking in leading whitespace.
  EXPECT_EQ(L"\x2026", UTF16ToWide(TruncateString(str2, 2, WORD_BREAK)));
  EXPECT_EQ(L"\x2026", UTF16ToWide(TruncateString(str2, 2, CHARACTER_BREAK)));

  // Test breaking at the beginning of the first word, with leading whitespace.
  EXPECT_EQ(L"\x2026", UTF16ToWide(TruncateString(str2, 3, WORD_BREAK)));
  EXPECT_EQ(L"\x2026", UTF16ToWide(TruncateString(str2, 3, CHARACTER_BREAK)));

  // Test breaking in the middle of the first word, with leading whitespace.
  EXPECT_EQ(L"\x2026", UTF16ToWide(TruncateString(str2, 4, WORD_BREAK)));
  EXPECT_EQ(L"\x2026", UTF16ToWide(TruncateString(str2, 4, CHARACTER_BREAK)));
  EXPECT_EQ(L"   f\x2026", UTF16ToWide(TruncateString(str2, 5, WORD_BREAK)));
  EXPECT_EQ(L"   f\x2026",
            UTF16ToWide(TruncateString(str2, 5, CHARACTER_BREAK)));
}

}  // namespace gfx
