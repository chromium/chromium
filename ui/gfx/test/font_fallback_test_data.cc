// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/test/font_fallback_test_data.h"

#include <string>

#include "build/build_config.h"

namespace gfx {

FallbackFontTestCase::FallbackFontTestCase() = default;
FallbackFontTestCase::FallbackFontTestCase(const FallbackFontTestCase& other) =
    default;

FallbackFontTestCase::FallbackFontTestCase(
    UScriptCode script_arg,
    std::string language_tag_arg,
    std::u16string text_arg,
    std::vector<std::string> fallback_fonts_arg)
    : script(script_arg),
      language_tag(language_tag_arg),
      text(text_arg),
      fallback_fonts(fallback_fonts_arg) {}

FallbackFontTestCase::~FallbackFontTestCase() = default;

#if BUILDFLAG(IS_WIN)
// A list of script and the fallback font on a default windows installation.
// This list may need to be updated if fonts or operating systems are
// upgraded.
// TODO(drott): Some of the test cases lack a valid language tag as it's unclear
// which language in particular would be expressed with the respective ancient
// script. Ideally we'd find a meaningful language tag for those.
const std::vector<FallbackFontTestCase> kGetFontFallbackTests = {
    {USCRIPT_ARABIC,
     "ar",
     u"\u062A\u062D",
     {"Segoe UI", "Tahoma", "Times New Roman"}},
    {USCRIPT_ARMENIAN,
     "hy-am",
     u"\u0540\u0541",
     {"Segoe UI", "Tahoma", "Sylfaen", "Times New Roman"}},
    {USCRIPT_BENGALI, "bn", u"\u09B8\u09AE", {"Nirmala UI", "Vrinda"}},
    {USCRIPT_BRAILLE, "en-us-brai", u"\u2870\u2871", {"Segoe UI Symbol"}},
    {USCRIPT_BUGINESE, "bug", u"\u1A00\u1A01", {"Leelawadee UI"}},
    {USCRIPT_CANADIAN_ABORIGINAL,
     "cans",
     u"\u1410\u1411",
     {"Gadugi", "Euphemia"}},

    {USCRIPT_CARIAN, "xcr", u"\U000102A0\U000102A1", {"Segoe UI Historic"}},

    {USCRIPT_CHEROKEE,
     "chr",
     u"\u13A1\u13A2",
     {"Gadugi", "Plantagenet Cherokee"}},

    {USCRIPT_COPTIC, "copt", u"\u2C81\u2C82", {"Segoe UI Historic"}},

    {USCRIPT_CUNEIFORM, "akk", u"\U00012000\U0001200C", {"Segoe UI Historic"}},

    {USCRIPT_CYPRIOT, "ecy", u"\U00010800\U00010801", {"Segoe UI Historic"}},

    {USCRIPT_CYRILLIC, "ru", u"\u0410\u0411\u0412", {"Times New Roman"}},

    {USCRIPT_DESERET, "en", u"\U00010400\U00010401", {"Segoe UI Symbol"}},

    {USCRIPT_ETHIOPIC, "am", u"\u1201\u1202", {"Ebrima", "Nyala"}},
    {USCRIPT_GEORGIAN, "ka", u"\u10A0\u10A1", {"Sylfaen", "Segoe UI"}},
    {USCRIPT_GREEK, "el", u"\u0391\u0392", {"Times New Roman"}},
    {USCRIPT_GURMUKHI, "pa", u"\u0A21\u0A22", {"Raavi", "Nirmala UI"}},
    {USCRIPT_HAN,
     "zh-CN",
     u"\u6211",
     {"Microsoft YaHei", "Microsoft YaHei UI"}},
    {USCRIPT_HAN,
     "zh-HK",
     u"\u6211",
     {"Microsoft JhengHei", "Microsoft JhengHei UI"}},
    {USCRIPT_HAN,
     "zh-Hans",
     u"\u6211",
     {"Microsoft YaHei", "Microsoft YaHei UI"}},
    {USCRIPT_HAN,
     "zh-Hant",
     u"\u6211",
     {"Microsoft JhengHei", "Microsoft JhengHei UI"}},
    {USCRIPT_HAN, "ja", u"\u6211", {"Meiryo UI", "Yu Gothic UI", "Yu Gothic"}},
    {USCRIPT_HANGUL, "ko", u"\u1100\u1101", {"Malgun Gothic", "Gulim"}},
    {USCRIPT_HEBREW,
     "he",
     u"\u05D1\u05D2",
     {"Segoe UI", "Tahoma", "Times New Roman"}},
    {USCRIPT_KHMER,
     "km",
     u"\u1780\u1781",
     {"Leelawadee UI", "Khmer UI", "Khmer OS", "MoolBoran", "DaunPenh"}},

    {USCRIPT_IMPERIAL_ARAMAIC,
     "arc",
     u"\U00010841\U00010842",
     {"Segoe UI Historic"}},

    {USCRIPT_INSCRIPTIONAL_PAHLAVI,
     "pal",
     u"\U00010B61\U00010B62",
     {"Segoe UI Historic"}},

    {USCRIPT_INSCRIPTIONAL_PARTHIAN,
     "xpr",
     u"\U00010B41\U00010B42",
     {"Segoe UI Historic"}},

    {USCRIPT_JAVANESE, "jv", u"\uA991\uA992", {"Javanese Text"}},
    {USCRIPT_KHAROSHTHI, "sa", u"\U00010A10\U00010A11", {"Segoe UI Historic"}},

    {USCRIPT_LAO,
     "lo",
     u"\u0ED0\u0ED1",
     {"Lao UI", "Leelawadee UI", "Segoe UI"}},
    {USCRIPT_LISU, "lis", u"\uA4D0\uA4D1", {"Segoe UI"}},

    {USCRIPT_LYCIAN, "xlc", u"\U00010281\U00010282", {"Segoe UI Historic"}},

    {USCRIPT_LYDIAN, "xld", u"\U00010921\U00010922", {"Segoe UI Historic"}},

    {USCRIPT_MALAYALAM, "ml", u"\u0D21\u0D22", {"Kartika", "Nirmala UI"}},

    {USCRIPT_MEROITIC_CURSIVE,
     "",
     u"\U000109A1\U000109A2",
     {"Segoe UI Historic"}},

    {USCRIPT_MYANMAR, "my", u"\u1000\u1001", {"Myanmar Text"}},
    {USCRIPT_NEW_TAI_LUE, "", u"\u1981\u1982", {"Microsoft New Tai Lue"}},
    {USCRIPT_NKO, "nko", u"\u07C1\u07C2", {"Ebrima", "Segoe UI"}},

    {USCRIPT_OGHAM,
     "",
     u"\u1680\u1681",
     {"Segoe UI Symbol", "Segoe UI Historic"}},

    {USCRIPT_OL_CHIKI, "", u"\u1C51\u1C52", {"Nirmala UI"}},

    {USCRIPT_OLD_ITALIC,
     "",
     u"\U00010301\U00010302",
     {"Segoe UI Historic", "Segoe UI Symbol"}},

    {USCRIPT_OLD_PERSIAN,
     "peo",
     u"\U000103A1\U000103A2",
     {"Segoe UI Historic"}},

    {USCRIPT_OLD_SOUTH_ARABIAN,
     "",
     u"\U00010A61\U00010A62",
     {"Segoe UI Historic"}},

    {USCRIPT_ORIYA, "or", u"\u0B21\u0B22", {"Kalinga", "Nirmala UI"}},
    {USCRIPT_PHAGS_PA, "", u"\uA841\uA842", {"Microsoft PhagsPa"}},

    {USCRIPT_RUNIC,
     "",
     u"\u16A0\u16A1",
     {"Segoe UI Symbol", "Segoe UI Historic"}},

    {USCRIPT_SHAVIAN,
     "",
     u"\U00010451\U00010452",
     {"Segoe UI", "Segoe UI Historic"}},

    {USCRIPT_SINHALA, "si", u"\u0D91\u0D92", {"Iskoola Pota", "Nirmala UI"}},

    {USCRIPT_SORA_SOMPENG, "", u"\U000110D1\U000110D2", {"Nirmala UI"}},

    {USCRIPT_SYRIAC,
     "syr",
     u"\u0711\u0712",
     {"Estrangelo Edessa", "Segoe UI Historic"}},

    {USCRIPT_TAI_LE, "", u"\u1951\u1952", {"Microsoft Tai Le"}},
    {USCRIPT_TAMIL, "ta", u"\u0BB1\u0BB2", {"Latha", "Nirmala UI"}},
    {USCRIPT_TELUGU, "te", u"\u0C21\u0C22", {"Gautami", "Nirmala UI"}},
    {USCRIPT_THAANA, "", u"\u0781\u0782", {"Mv Boli", "MV Boli"}},
    {USCRIPT_THAI,
     "th",
     u"\u0e01\u0e02",
     {"Tahoma", "Leelawadee UI", "Leelawadee"}},
    {USCRIPT_TIBETAN, "bo", u"\u0F01\u0F02", {"Microsoft Himalaya"}},
    {USCRIPT_TIFINAGH, "", u"\u2D31\u2D32", {"Ebrima"}},
    {USCRIPT_VAI, "vai", u"\uA501\uA502", {"Ebrima"}},
    {USCRIPT_YI, "yi", u"\uA000\uA001", {"Microsoft Yi Baiti"}}};

#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// A list of script and the fallback font on the linux test environment.
// On linux, font-config configuration and fonts are mock. The config
// can be found in '${build}/etc/fonts/fonts.conf' and the test fonts
// can be found in '${build}/test_fonts/*'.
const std::vector<FallbackFontTestCase> kGetFontFallbackTests = {
    {USCRIPT_BENGALI, "bn", u"\u09B8\u09AE", {"Mukti Narrow"}},
    {USCRIPT_DEVANAGARI, "hi", u"\u0905\u0906", {"Lohit Devanagari"}},
    {USCRIPT_GURMUKHI, "pa", u"\u0A21\u0A22", {"Lohit Gurmukhi"}},
    {USCRIPT_HAN, "zh-CN", u"\u6211", {"Noto Sans CJK JP"}},
    {USCRIPT_KHMER, "km", u"\u1780\u1781", {"Noto Sans Khmer"}},
    {USCRIPT_TAMIL, "ta", u"\u0BB1\u0BB2", {"Lohit Tamil"}},
    {USCRIPT_THAI, "th", u"\u0e01\u0e02", {"Garuda"}},
};

#else

// No fallback font tests are defined on that platform.
const std::vector<FallbackFontTestCase> kGetFontFallbackTests = {};

#endif

}  // namespace gfx
