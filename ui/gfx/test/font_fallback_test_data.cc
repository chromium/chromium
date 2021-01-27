// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/test/font_fallback_test_data.h"

#include "base/strings/string16.h"
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
     STRING16_LITERAL("\u062A\u062D"),
     {"Segoe UI", "Tahoma", "Times New Roman"}},
    {USCRIPT_ARMENIAN,
     "hy-am",
     STRING16_LITERAL("\u0540\u0541"),
     {"Segoe UI", "Tahoma", "Sylfaen", "Times New Roman"}},
    {USCRIPT_BENGALI,
     "bn",
     STRING16_LITERAL("\u09B8\u09AE"),
     {"Nirmala UI", "Vrinda"}},
    {USCRIPT_BRAILLE,
     "en-us-brai",
     STRING16_LITERAL("\u2870\u2871"),
     {"Segoe UI Symbol"}},
    {USCRIPT_BUGINESE,
     "bug",
     STRING16_LITERAL("\u1A00\u1A01"),
     {"Leelawadee UI"},
     kWin10Only},
    {USCRIPT_CANADIAN_ABORIGINAL,
     "cans",
     STRING16_LITERAL("\u1410\u1411"),
     {"Gadugi", "Euphemia"}},

    {USCRIPT_CARIAN,
     "xcr",
     STRING16_LITERAL("\U000102A0\U000102A1"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CHEROKEE,
     "chr",
     STRING16_LITERAL("\u13A1\u13A2"),
     {"Gadugi", "Plantagenet Cherokee"}},

    {USCRIPT_COPTIC,
     "copt",
     STRING16_LITERAL("\u2C81\u2C82"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CUNEIFORM,
     "akk",
     STRING16_LITERAL("\U00012000\U0001200C"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CYPRIOT,
     "ecy",
     STRING16_LITERAL("\U00010800\U00010801"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_CYRILLIC,
     "ru",
     STRING16_LITERAL("\u0410\u0411\u0412"),
     {"Times New Roman"}},

    {USCRIPT_DESERET,
     "en",
     STRING16_LITERAL("\U00010400\U00010401"),
     {"Segoe UI Symbol"},
     kWin10Only},

    {USCRIPT_ETHIOPIC,
     "am",
     STRING16_LITERAL("\u1201\u1202"),
     {"Ebrima", "Nyala"}},
    {USCRIPT_GEORGIAN,
     "ka",
     STRING16_LITERAL("\u10A0\u10A1"),
     {"Sylfaen", "Segoe UI"},
     kWin10Only},
    {USCRIPT_GREEK,
     "el",
     STRING16_LITERAL("\u0391\u0392"),
     {"Times New Roman"}},
    {USCRIPT_GURMUKHI,
     "pa",
     STRING16_LITERAL("\u0A21\u0A22"),
     {"Raavi", "Nirmala UI"}},
    {USCRIPT_HAN,
     "zh-CN",
     STRING16_LITERAL("\u6211"),
     {"Microsoft YaHei", "Microsoft YaHei UI"}},
    {USCRIPT_HAN,
     "zh-HK",
     STRING16_LITERAL("\u6211"),
     {"Microsoft JhengHei", "Microsoft JhengHei UI"}},
    {USCRIPT_HAN,
     "zh-Hans",
     STRING16_LITERAL("\u6211"),
     {"Microsoft YaHei", "Microsoft YaHei UI"}},
    {USCRIPT_HAN,
     "zh-Hant",
     STRING16_LITERAL("\u6211"),
     {"Microsoft JhengHei", "Microsoft JhengHei UI"}},
    {USCRIPT_HAN,
     "ja",
     STRING16_LITERAL("\u6211"),
     {"Meiryo UI", "Yu Gothic UI", "Yu Gothic"}},
    {USCRIPT_HANGUL,
     "ko",
     STRING16_LITERAL("\u1100\u1101"),
     {"Malgun Gothic", "Gulim"},
     kWin10Only},
    {USCRIPT_HEBREW,
     "he",
     STRING16_LITERAL("\u05D1\u05D2"),
     {"Segoe UI", "Tahoma", "Times New Roman"}},
    {USCRIPT_KHMER,
     "km",
     STRING16_LITERAL("\u1780\u1781"),
     {"Leelawadee UI", "Khmer UI", "Khmer OS", "MoolBoran", "DaunPenh"}},

    {USCRIPT_IMPERIAL_ARAMAIC,
     "arc",
     STRING16_LITERAL("\U00010841\U00010842"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_INSCRIPTIONAL_PAHLAVI,
     "pal",
     STRING16_LITERAL("\U00010B61\U00010B62"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_INSCRIPTIONAL_PARTHIAN,
     "xpr",
     STRING16_LITERAL("\U00010B41\U00010B42"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_JAVANESE,
     "jv",
     STRING16_LITERAL("\uA991\uA992"),
     {"Javanese Text"},
     kWin10Only},
    {USCRIPT_KANNADA,
     "kn",
     STRING16_LITERAL("\u0CA1\u0CA2"),
     {"Nirmala UI", "Tunga"}},

    {USCRIPT_KHAROSHTHI,
     "sa",
     STRING16_LITERAL("\U00010A10\U00010A11"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_LAO,
     "lo",
     STRING16_LITERAL("\u0ED0\u0ED1"),
     {"Lao UI", "Leelawadee UI", "Segoe UI"}},
    {USCRIPT_LISU,
     "lis",
     STRING16_LITERAL("\uA4D0\uA4D1"),
     {"Segoe UI"},
     kWin10Only},

    {USCRIPT_LYCIAN,
     "xlc",
     STRING16_LITERAL("\U00010281\U00010282"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_LYDIAN,
     "xld",
     STRING16_LITERAL("\U00010921\U00010922"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_MALAYALAM,
     "ml",
     STRING16_LITERAL("\u0D21\u0D22"),
     {"Kartika", "Nirmala UI"}},

    {USCRIPT_MEROITIC_CURSIVE,
     "",
     STRING16_LITERAL("\U000109A1\U000109A2"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_MYANMAR,
     "my",
     STRING16_LITERAL("\u1000\u1001"),
     {"Myanmar Text"},
     kWin10Only},
    {USCRIPT_NEW_TAI_LUE,
     "",
     STRING16_LITERAL("\u1981\u1982"),
     {"Microsoft New Tai Lue"}},
    {USCRIPT_NKO,
     "nko",
     STRING16_LITERAL("\u07C1\u07C2"),
     {"Ebrima", "Segoe UI"}},

    {USCRIPT_OGHAM,
     "",
     STRING16_LITERAL("\u1680\u1681"),
     {"Segoe UI Symbol", "Segoe UI Historic"}},

    {USCRIPT_OL_CHIKI,
     "",
     STRING16_LITERAL("\u1C51\u1C52"),
     {"Nirmala UI"},
     kWin10Only},

    {USCRIPT_OLD_ITALIC,
     "",
     STRING16_LITERAL("\U00010301\U00010302"),
     {"Segoe UI Historic", "Segoe UI Symbol"}},

    {USCRIPT_OLD_PERSIAN,
     "peo",
     STRING16_LITERAL("\U000103A1\U000103A2"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_OLD_SOUTH_ARABIAN,
     "",
     STRING16_LITERAL("\U00010A61\U00010A62"),
     {"Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_ORIYA,
     "or",
     STRING16_LITERAL("\u0B21\u0B22"),
     {"Kalinga", "Nirmala UI"}},
    {USCRIPT_PHAGS_PA,
     "",
     STRING16_LITERAL("\uA841\uA842"),
     {"Microsoft PhagsPa"}},

    {USCRIPT_RUNIC,
     "",
     STRING16_LITERAL("\u16A0\u16A1"),
     {"Segoe UI Symbol", "Segoe UI Historic"}},

    {USCRIPT_SHAVIAN,
     "",
     STRING16_LITERAL("\U00010451\U00010452"),
     {"Segoe UI", "Segoe UI Historic"},
     kWin10Only},

    {USCRIPT_SINHALA,
     "si",
     STRING16_LITERAL("\u0D91\u0D92"),
     {"Iskoola Pota", "Nirmala UI"}},

    {USCRIPT_SORA_SOMPENG,
     "",
     STRING16_LITERAL("\U000110D1\U000110D2"),
     {"Nirmala UI"},
     kWin10Only},

    {USCRIPT_SYRIAC,
     "syr",
     STRING16_LITERAL("\u0711\u0712"),
     {"Estrangelo Edessa", "Segoe UI Historic"}},

    {USCRIPT_TAI_LE,
     "",
     STRING16_LITERAL("\u1951\u1952"),
     {"Microsoft Tai Le"}},
    {USCRIPT_TAMIL,
     "ta",
     STRING16_LITERAL("\u0BB1\u0BB2"),
     {"Latha", "Nirmala UI"}},
    {USCRIPT_TELUGU,
     "te",
     STRING16_LITERAL("\u0C21\u0C22"),
     {"Gautami", "Nirmala UI"}},
    {USCRIPT_THAANA,
     "",
     STRING16_LITERAL("\u0781\u0782"),
     {"Mv Boli", "MV Boli"}},
    {USCRIPT_THAI,
     "th",
     STRING16_LITERAL("\u0e01\u0e02"),
     {"Tahoma", "Leelawadee UI", "Leelawadee"},
     kWin10Only},
    {USCRIPT_TIBETAN,
     "bo",
     STRING16_LITERAL("\u0F01\u0F02"),
     {"Microsoft Himalaya"}},
    {USCRIPT_TIFINAGH, "", STRING16_LITERAL("\u2D31\u2D32"), {"Ebrima"}},
    {USCRIPT_VAI, "vai", STRING16_LITERAL("\uA501\uA502"), {"Ebrima"}},
    {USCRIPT_YI,
     "yi",
     STRING16_LITERAL("\uA000\uA001"),
     {"Microsoft Yi Baiti"}}};

#elif defined(OS_LINUX) || defined(OS_CHROMEOS)

// A list of script and the fallback font on the linux test environment.
// On linux, font-config configuration and fonts are mock. The config
// can be found in '${build}/etc/fonts/fonts.conf' and the test fonts
// can be found in '${build}/test_fonts/*'.
std::vector<FallbackFontTestCase> kGetFontFallbackTests = {
    {USCRIPT_BENGALI, "bn", STRING16_LITERAL("\u09B8\u09AE"), {"Mukti Narrow"}},

    {USCRIPT_DEVANAGARI,
     "hi",
     STRING16_LITERAL("\u0905\u0906"),
     {"Lohit Devanagari"}},

    {USCRIPT_GURMUKHI,
     "pa",
     STRING16_LITERAL("\u0A21\u0A22"),
     {"Lohit Gurmukhi"}},

    {USCRIPT_HAN, "zh-CN", STRING16_LITERAL("\u6211"), {"Noto Sans CJK JP"}},

    {USCRIPT_KHMER,
     "km",
     STRING16_LITERAL("\u1780\u1781"),
     {"Noto Sans Khmer"}},

    {USCRIPT_TAMIL, "ta", STRING16_LITERAL("\u0BB1\u0BB2"), {"Lohit Tamil"}},
    {USCRIPT_THAI, "th", STRING16_LITERAL("\u0e01\u0e02"), {"Garuda"}},
};

#else

// No fallback font tests are defined on that platform.
std::vector<FallbackFontTestCase> kGetFontFallbackTests = {};

#endif

}  // namespace gfx
