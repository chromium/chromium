// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/test/font_fallback_test_data.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace gfx {

#if defined(OS_WIN)
constexpr bool kWin10Only = true;
#endif

FallbackFontTestCase::FallbackFontTestCase() = default;
FallbackFontTestCase::FallbackFontTestCase(const FallbackFontTestCase& other) =
    default;

FallbackFontTestCase::FallbackFontTestCase(
    UScriptCode script_arg,
    std::string language_tag_arg,
    base::string16 text_arg,
    std::vector<std::string> fallback_fonts_arg,
    bool is_win10_arg)
    : script(script_arg),
      language_tag(language_tag_arg),
      text(text_arg),
      fallback_fonts(fallback_fonts_arg),
      is_win10(is_win10_arg) {}

FallbackFontTestCase::~FallbackFontTestCase() = default;

#if defined(OS_WIN)
// A list of script and the fallback font on a default windows installation.
// This list may need to be updated if fonts or operating systems are
// upgraded.
// TODO(drott): Some of the test cases lack a valid language tag as it's unclear
// which language in particular would be expressed with the respective ancient
// script. Ideally we'd find a meaningful language tag for those.
std::vector<FallbackFontTestCase> kGetFontFallbackTests = {
    {USCRIPT_ARABIC,
     "ar",
     L"\u062A\u062D",
     {"Segoe UI", "Tahoma", "Times New Roman"}},
    {USCRIPT_ARMENIAN,
     "hy-am",
     L"\u0540\u0541",
     {"Segoe UI", "Tahoma", "Sylfaen", "Times New Roman"}},
    {USCRIPT_BENGALI, "bn", L"\u09B8\u09AE", {"Nirmala UI", "Vrinda"}},
    {USCRIPT_BRAILLE, "en-us-brai", L"\u2870\u2871", {"Segoe UI Symbol"}},
    {USCRIPT_BUGINESE, "bug", L"\u1A00\u1A01", {"Leelawadee UI"}, kWin10Only},
    {USCRIPT_CANADIAN_ABORIGINAL,
     "cans",
     L"\u1410\u1411",
     {"Gadugi", "Euphemia"}},

    {USCRIPT_CARIAN,
     "xcr",
     L"\U000102A0\U000102A1",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CHEROKEE,
     "chr",
     L"\u13A1\u13A2",
     {"Gadugi", "Plantagenet Cherokee"}},

    {USCRIPT_COPTIC,
     "copt",
     L"\u2C81\u2C82",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CUNEIFORM,
     "akk",
     L"\U00012000\U0001200C",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CYPRIOT,
     "ecy",
     L"\U00010800\U00010801",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CYRILLIC, "ru", L"\u0410\u0411\u0412", {"Times New Roman"}},

    {USCRIPT_DESERET,
     "en",
     L"\U00010400\U00010401",
     {"Segoe UI Symbol"},
     kWin10Only},

    {USCRIPT_ETHIOPIC, "am", L"\u1201\u1202", {"Ebrima", "Nyala"}},
    {USCRIPT_GEORGIAN,
     "ka",
     L"\u10A0\u10A1",
     {"Sylfaen", "Segoe UI"},
     kWin10Only},
    {USCRIPT_GREEK, "el", L"\u0391\u0392", {"Times New Roman"}},
    {USCRIPT_GURMUKHI, "pa", L"\u0A21\u0A22", {"Raavi", "Nirmala UI"}},
    {USCRIPT_HAN,
     "zh-CN",
     L"\u6211",
     {"Microsoft YaHei", "Microsoft YaHei UI"}},
    {USCRIPT_HAN,
     "zh-HK",
     L"\u6211",
     {"Microsoft JhengHei", "Microsoft JhengHei UI"}},
    {USCRIPT_HAN,
     "zh-Hans",
     L"\u6211",
     {"Microsoft YaHei", "Microsoft YaHei UI"}},
    {USCRIPT_HAN,
     "zh-Hant",
     L"\u6211",
     {"Microsoft JhengHei", "Microsoft JhengHei UI"}},
    {USCRIPT_HAN, "ja", L"\u6211", {"Meiryo UI", "Yu Gothic UI", "Yu Gothic"}},
    {USCRIPT_HANGUL,
     "ko",
     L"\u1100\u1101",
     {"Malgun Gothic", "Gulim"},
     kWin10Only},
    {USCRIPT_HEBREW,
     "he",
     L"\u05D1\u05D2",
     {"Segoe UI", "Tahoma", "Times New Roman"}},
    {USCRIPT_KHMER,
     "km",
     L"\u1780\u1781",
     {"Leelawadee UI", "Khmer UI", "Khmer OS", "MoolBoran", "DaunPenh"}},

    {USCRIPT_IMPERIAL_ARAMAIC,
     "arc",
     L"\U00010841\U00010842",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_INSCRIPTIONAL_PAHLAVI,
     "pal",
     L"\U00010B61\U00010B62",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_INSCRIPTIONAL_PARTHIAN,
     "xpr",
     L"\U00010B41\U00010B42",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_JAVANESE, "jv", L"\uA991\uA992", {"Javanese Text"}, kWin10Only},
    {USCRIPT_KANNADA, "kn", L"\u0CA1\u0CA2", {"Nirmala UI", "Tunga"}},

    {USCRIPT_KHAROSHTHI,
     "sa",
     L"\U00010A10\U00010A11",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_LAO, "lo", L"\u0ED0\u0ED1", {"Lao UI", "Leelawadee UI"}},
    {USCRIPT_LISU, "lis", L"\uA4D0\uA4D1", {"Segoe UI"}, kWin10Only},

    {USCRIPT_LYCIAN,
     "xlc",
     L"\U00010281\U00010282",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_LYDIAN,
     "xld",
     L"\U00010921\U00010922",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_MALAYALAM, "ml", L"\u0D21\u0D22", {"Kartika", "Nirmala UI"}},

    {USCRIPT_MEROITIC_CURSIVE,
     "",
     L"\U000109A1\U000109A2",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_MYANMAR, "my", L"\u1000\u1001", {"Myanmar Text"}, kWin10Only},
    {USCRIPT_NEW_TAI_LUE, "", L"\u1981\u1982", {"Microsoft New Tai Lue"}},
    {USCRIPT_NKO, "nko", L"\u07C1\u07C2", {"Ebrima"}},

    {USCRIPT_OGHAM,
     "",
     L"\u1680\u1681",
     {"Segoe UI Symbol", "Segoe UI Historic"}},

    {USCRIPT_OL_CHIKI, "", L"\u1C51\u1C52", {"Nirmala UI"}, kWin10Only},

    {USCRIPT_OLD_ITALIC,
     "",
     L"\U00010301\U00010302",
     {"Segoe UI Historic", "Segoe UI Symbol"}},

    {USCRIPT_OLD_PERSIAN,
     "peo",
     L"\U000103A1\U000103A2",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_OLD_SOUTH_ARABIAN,
     "",
     L"\U00010A61\U00010A62",
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_ORIYA, "or", L"\u0B21\u0B22", {"Kalinga", "Nirmala UI"}},
    {USCRIPT_PHAGS_PA, "", L"\uA841\uA842", {"Microsoft PhagsPa"}},

    {USCRIPT_RUNIC,
     "",
     L"\u16A0\u16A1",
     {"Segoe UI Symbol", "Segoe UI Historic"}},

    {USCRIPT_SHAVIAN,
     "",
     L"\U00010451\U00010452",
     {"Segoe UI", "Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_SINHALA, "si", L"\u0D91\u0D92", {"Iskoola Pota", "Nirmala UI"}},

    {USCRIPT_SORA_SOMPENG,
     "",
     L"\U000110D1\U000110D2",
     {"Nirmala UI"},
     kWin10Only},

    {USCRIPT_SYRIAC,
     "syr",
     L"\u0711\u0712",
     {"Estrangelo Edessa", "Segoe UI Historic"}},

    {USCRIPT_TAI_LE, "", L"\u1951\u1952", {"Microsoft Tai Le"}},
    {USCRIPT_TAMIL, "ta", L"\u0BB1\u0BB2", {"Latha", "Nirmala UI"}},
    {USCRIPT_TELUGU, "te", L"\u0C21\u0C22", {"Gautami", "Nirmala UI"}},
    {USCRIPT_THAANA, "", L"\u0781\u0782", {"Mv Boli", "MV Boli"}},
    {USCRIPT_THAI,
     "th",
     L"\u0e01\u0e02",
     {"Tahoma", "Leelawadee UI", "Leelawadee"},
     kWin10Only},
    {USCRIPT_TIBETAN, "bo", L"\u0F01\u0F02", {"Microsoft Himalaya"}},
    {USCRIPT_TIFINAGH, "", L"\u2D31\u2D32", {"Ebrima"}},
    {USCRIPT_VAI, "vai", L"\uA501\uA502", {"Ebrima"}},
    {USCRIPT_YI, "yi", L"\uA000\uA001", {"Microsoft Yi Baiti"}}};

#elif defined(OS_LINUX)

// A list of script and the fallback font on the linux test environment.
// On linux, font-config configuration and fonts are mock. The config
// can be found in '${build}/etc/fonts/fonts.conf' and the test fonts
// can be found in '${build}/test_fonts/*'.
std::vector<FallbackFontTestCase> kGetFontFallbackTests = {
    {USCRIPT_BENGALI,
     "bn",
     base::WideToUTF16(L"\u09B8\u09AE"),
     {"Mukti Narrow"}},

    {USCRIPT_DEVANAGARI,
     "hi",
     base::WideToUTF16(L"\u0905\u0906"),
     {"Lohit Devanagari"}},

    {USCRIPT_GURMUKHI,
     "pa",
     base::WideToUTF16(L"\u0A21\u0A22"),
     {"Lohit Gurmukhi"}},

    {USCRIPT_HAN, "zh-CN", base::WideToUTF16(L"\u6211"), {"Noto Sans CJK JP"}},

    {USCRIPT_KHMER,
     "km",
     base::WideToUTF16(L"\u1780\u1781"),
     {"Noto Sans Khmer"}},

    {USCRIPT_TAMIL, "ta", base::WideToUTF16(L"\u0BB1\u0BB2"), {"Lohit Tamil"}},
    {USCRIPT_THAI, "th", base::WideToUTF16(L"\u0e01\u0e02"), {"Garuda"}},
};

#else

// No fallback font tests are defined on that platform.
std::vector<FallbackFontTestCase> kGetFontFallbackTests = {};

#endif

}  // namespace gfx
