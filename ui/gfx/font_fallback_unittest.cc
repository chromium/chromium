// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <tuple>

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/font_fallback_win.h"
#include "ui/gfx/platform_font.h"
#include "ui/gfx/test/font_fallback_test_data.h"

namespace gfx {

namespace {

// Options to parameterized unittests.
struct FallbackFontTestOption {
  bool ignore_get_fallback_failure = false;
  bool skip_code_point_validation = false;
  bool skip_fallback_fonts_validation = false;
};

const FallbackFontTestOption default_fallback_option = {false, false, false};
// Options for tests that does not validate the GetFallbackFont(...) parameters.
const FallbackFontTestOption untested_fallback_option = {true, true, true};

struct BaseFontTestOption {
  const char* family_name = nullptr;
  int delta = 0;
  int style = 0;
  Font::Weight weight = Font::Weight::NORMAL;
};

constexpr BaseFontTestOption default_base_font;
constexpr BaseFontTestOption styled_font = {nullptr, 1, Font::FontStyle::ITALIC,
                                            Font::Weight::BOLD};
constexpr BaseFontTestOption sans_font = {"sans", 2};

using FallbackFontTestParamInfo = std::
    tuple<FallbackFontTestCase, FallbackFontTestOption, BaseFontTestOption>;

class GetFallbackFontTest
    : public ::testing::TestWithParam<FallbackFontTestParamInfo> {
 public:
  GetFallbackFontTest() = default;

  GetFallbackFontTest(const GetFallbackFontTest&) = delete;
  GetFallbackFontTest& operator=(const GetFallbackFontTest&) = delete;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<FallbackFontTestParamInfo> param_info) {
    const FallbackFontTestCase& test_case = std::get<0>(param_info.param);
    const BaseFontTestOption& base_font_option = std::get<2>(param_info.param);

    std::string font_option;
    if (base_font_option.family_name)
      font_option += std::string("F") + base_font_option.family_name;
    if (base_font_option.delta || base_font_option.style ||
        base_font_option.style) {
      font_option += base::StringPrintf(
          "_d%ds%dw%d", base_font_option.delta, base_font_option.style,
          static_cast<int>(base_font_option.weight));
    }

    std::string language_tag = test_case.language_tag;
    base::RemoveChars(language_tag, "-", &language_tag);
    return std::string("S") + uscript_getName(test_case.script) + "L" +
           language_tag + font_option;
  }

  void SetUp() override {
    std::tie(test_case_, test_option_, base_font_option_) = GetParam();
  }

 protected:
  bool GetFallbackFont(const Font& font,
                       const std::string& language_tag,
                       Font* result) {
    return gfx::GetFallbackFont(font, language_tag, test_case_.text, result);
  }

  bool EnsuresScriptSupportCodePoints(const std::u16string& text,
                                      UScriptCode script,
                                      const std::string& script_name) {
    size_t i = 0;
    while (i < text.length()) {
      UChar32 code_point;
      U16_NEXT(text.c_str(), i, text.size(), code_point);
      if (!uscript_hasScript(code_point, script)) {
        // Retrieve the appropriate script
        UErrorCode script_error;
        UScriptCode codepoint_script =
            uscript_getScript(code_point, &script_error);

        ADD_FAILURE() << "CodePoint U+" << std::hex << code_point
                      << " is not part of the script '" << script_name
                      << "'. Script '" << uscript_getName(codepoint_script)
                      << "' detected.";
        return false;
      }
    }
    return true;
  }

  bool DoesFontSupportCodePoints(Font font, const std::u16string& text) {
    sk_sp<SkTypeface> skia_face = font.platform_font()->GetNativeSkTypeface();
    if (!skia_face) {
      ADD_FAILURE() << "Cannot create typeface for '" << font.GetFontName()
                    << "'.";
      return false;
    }

    size_t i = 0;
    const SkGlyphID kUnsupportedGlyph = 0;
    while (i < text.length()) {
      UChar32 code_point;
      U16_NEXT(text.c_str(), i, text.size(), code_point);
      SkGlyphID glyph_id = skia_face->unicharToGlyph(code_point);
      if (glyph_id == kUnsupportedGlyph)
        return false;
    }
    return true;
  }

  FallbackFontTestCase test_case_;
  FallbackFontTestOption test_option_;
  BaseFontTestOption base_font_option_;
  std::string script_name_;

 private:
  // Needed to bypass DCHECK in GetFallbackFont.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

}  // namespace

// This test ensures the font fallback work correctly. It will ensures that
//   1) The script supports the text
//   2) The input font does not already support the text
//   3) The call to GetFallbackFont() succeed
//   4) The fallback font has a glyph for every character of the text
//
// The previous checks can be activated or deactivated through the class
// FallbackFontTestOption (e.g. test_option_).
TEST_P(GetFallbackFontTest, GetFallbackFont) {
  // Default system font.
  Font base_font;
  // Apply font options to the base font.
  if (base_font_option_.family_name)
    base_font = Font(base_font_option_.family_name, base_font.GetFontSize());
  if (base_font_option_.delta != 0 || base_font_option_.style != 0 ||
      base_font_option_.weight != gfx::Font::Weight::NORMAL) {
    base_font =
        base_font.Derive(base_font_option_.delta, base_font_option_.style,
                         base_font_option_.weight);
  }

  // Retrieve the name of the current script.
  script_name_ = uscript_getName(test_case_.script);

  // Validate that tested characters are part of the script.
  if (!test_option_.skip_code_point_validation &&
      !EnsuresScriptSupportCodePoints(test_case_.text, test_case_.script,
                                      script_name_)) {
    return;
  }

  // The default font already support it, do not try to find a fallback font.
  if (DoesFontSupportCodePoints(base_font, test_case_.text))
    return;

  // Retrieve the fallback font.
  Font fallback_font;
  bool result =
      GetFallbackFont(base_font, test_case_.language_tag, &fallback_font);
  if (!result) {
    if (!test_option_.ignore_get_fallback_failure)
      ADD_FAILURE() << "GetFallbackFont failed for '" << script_name_ << "'";
    return;
  }

  // Ensure the fallback font is a part of the validation fallback fonts list.
  if (!test_option_.skip_fallback_fonts_validation) {
    if (!base::Contains(test_case_.fallback_fonts,
                        fallback_font.GetFontName())) {
      ADD_FAILURE() << "GetFallbackFont failed for '" << script_name_
                    << "' invalid fallback font: "
                    << fallback_font.GetFontName()
                    << " not among valid options: "
                    << base::JoinString(test_case_.fallback_fonts, ", ");
      return;
    }
  }

#if BUILDFLAG(IS_IOS)
  // TODO(crbug.com/40279916): font fallback does not appear to be working
  // consistently.
  if (fallback_font.GetFontName() == ".LastResort") {
    GTEST_SKIP() << ".LastResort is not currently behaving correctly.";
  }
#endif

  // Ensure that glyphs exists in the fallback font.
  if (!DoesFontSupportCodePoints(fallback_font, test_case_.text)) {
    ADD_FAILURE() << "Font '" << fallback_font.GetFontName()
                  << "' does not matched every CodePoints.";
    return;
  }
}

// Produces a font test case for every script.
std::vector<FallbackFontTestCase> GetSampleFontTestCases() {
  std::vector<FallbackFontTestCase> result;

  const unsigned int script_max = u_getIntPropertyMaxValue(UCHAR_SCRIPT) + 1;
  for (unsigned int i = 0; i < script_max; i++) {
    const UScriptCode script = static_cast<UScriptCode>(i);

    // Make a sample text to test the script.
    char16_t text[8];
    UErrorCode errorCode = U_ZERO_ERROR;
    int text_length =
        uscript_getSampleString(script, text, std::size(text), &errorCode);
    if (text_length <= 0 || errorCode != U_ZERO_ERROR)
      continue;

    FallbackFontTestCase test_case(script, "", text, {});
    result.push_back(test_case);
  }
  return result;
}

// Ensures that the default fallback font gives known results. The test
// is validating that a known fallback font is given for a given text and font.
INSTANTIATE_TEST_SUITE_P(
    KnownExpectedFonts,
    GetFallbackFontTest,
    testing::Combine(
        testing::ValuesIn(kGetFontFallbackTests),
        testing::Values(default_fallback_option),
        testing::Values(default_base_font, styled_font, sans_font)),
    GetFallbackFontTest::ParamInfoToString);

// Ensures that font fallback functions are working properly for any string
// (strings from any script). The test doesn't enforce the functions to
// give a fallback font. The accepted behaviors are:
//    1) The fallback function failed and doesn't provide a fallback.
//    2) The fallback function succeeded and the font supports every glyphs.
INSTANTIATE_TEST_SUITE_P(
    Glyphs,
    GetFallbackFontTest,
    testing::Combine(testing::ValuesIn(GetSampleFontTestCases()),
                     testing::Values(untested_fallback_option),
                     testing::Values(default_base_font)),
    GetFallbackFontTest::ParamInfoToString);

}  // namespace gfx
