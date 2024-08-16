// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/ime/ash/input_method_util.h"

#include <stddef.h>

#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/extension_ime_util.h"
// For SetHardwareKeyboardLayoutForTesting.
#include <optional>

#include "ui/base/ime/ash/fake_input_method_delegate.h"
#include "ui/base/ime/ash/input_method_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace ash {
namespace input_method {

namespace {

// A mapping from an input method id to a resource id for a
// medium length language indicator.
// For those languages that want to display a slightly longer text in the
// "Your input method has changed to..." bubble than in the status tray.
// If an entry is not found in this table the short name is used.
const struct {
  const char* engine_id;
  const int resource_id;
} kMappingImeIdToMediumLenNameResourceId[] = {
    {"hangul_2set", IDS_LANGUAGES_MEDIUM_LEN_NAME_KOREAN},
    {"hangul_3set390", IDS_LANGUAGES_MEDIUM_LEN_NAME_KOREAN},
    {"hangul_3setfinal", IDS_LANGUAGES_MEDIUM_LEN_NAME_KOREAN},
    {"hangul_3setnoshift", IDS_LANGUAGES_MEDIUM_LEN_NAME_KOREAN},
    {"hangul_3setromaja", IDS_LANGUAGES_MEDIUM_LEN_NAME_KOREAN},
    {"zh-t-i0-pinyin", IDS_LANGUAGES_MEDIUM_LEN_NAME_CHINESE_SIMPLIFIED},
    {"zh-t-i0-wubi-1986", IDS_LANGUAGES_MEDIUM_LEN_NAME_CHINESE_SIMPLIFIED},
    {"zh-hant-t-i0-und", IDS_LANGUAGES_MEDIUM_LEN_NAME_CHINESE_TRADITIONAL},
    {"zh-hant-t-i0-cangjie-1987",
     IDS_LANGUAGES_MEDIUM_LEN_NAME_CHINESE_TRADITIONAL},
    {"zh-hant-t-i0-cangjie-1987-x-m0-simplified",
     IDS_LANGUAGES_MEDIUM_LEN_NAME_CHINESE_TRADITIONAL},
    {extension_ime_util::kBrailleImeEngineId,
     IDS_LANGUAGES_MEDIUM_LEN_NAME_BRAILLE},
};

// Due to asynchronous initialization of component extension manager,
// GetFirstLoginInputMethodIds may miss component extension IMEs. To enable
// component extension IME as the first loging input method, we have to prepare
// component extension IME IDs.
// Note: empty layout means the rule applies for all layouts.
const struct {
  const char* locale;
  const char* layout;
  const char* engine_id;
} kDefaultInputMethodRecommendation[] = {
    {"ja", "jp", "nacl_mozc_jp"},
    {"ja", "", "nacl_mozc_us"},
    {"zh-CN", "", "zh-t-i0-pinyin"},
    {"zh-TW", "", "zh-hant-t-i0-und"},
    {"th", "", "vkd_th"},
    {"vi", "", "vkd_vi_tcvn"},
    {"ru", "", "xkb:ru::rus"},
};

// The engine ID map for migration. This migration is for input method IDs from
// VPD so it's NOT a temporary migration.
const char* const kEngineIdMigrationMap[][2] = {
    // Workaround for invalid keyboard layout in kefka board vpd.
    // See https://crbug.com/700625
    {"ANSI", "xkb:us::eng"},
    {"ime:jp:mozc_jp", "nacl_mozc_jp"},
    {"ime:jp:mozc_us", "nacl_mozc_us"},
    {"ime:ko:hangul_2set", "ko-t-i0-und"},
    {"ime:ko:hangul", "ko-t-i0-und"},
    {"ime:zh-t:array", "zh-hant-t-i0-array-1992"},
    {"ime:zh-t:cangjie", "zh-hant-t-i0-cangjie-1987"},
    {"ime:zh-t:dayi", "zh-hant-t-i0-dayi-1988"},
    {"ime:zh-t:pinyin", "zh-hant-t-i0-pinyin"},
    {"ime:zh-t:quick", "zh-hant-t-i0-cangjie-1987-x-m0-simplified"},
    {"ime:zh-t:zhuyin", "zh-hant-t-i0-und"},
    {"ime:zh:pinyin", "zh-t-i0-pinyin"},
    {"ime:zh:wubi", "zh-t-i0-wubi-1986"},
    {"m17n:", "vkd_"},
    {"t13n:am", "am-t-i0-und"},
    {"t13n:ar", "ar-t-i0-und"},
    {"t13n:bn", "bn-t-i0-und"},
    {"t13n:el", "el-t-i0-und"},
    {"t13n:fa", "fa-t-i0-und"},
    {"t13n:gu", "gu-t-i0-und"},
    {"t13n:he", "he-t-i0-und"},
    {"t13n:hi", "hi-t-i0-und"},
    {"t13n:kn", "kn-t-i0-und"},
    {"t13n:ml", "ml-t-i0-und"},
    {"t13n:mr", "mr-t-i0-und"},
    {"t13n:ne", "ne-t-i0-und"},
    {"t13n:or", "or-t-i0-und"},
    {"t13n:pa", "pa-t-i0-und"},
    {"t13n:sa", "sa-t-i0-und"},
    {"t13n:sr", "sr-t-i0-und"},
    {"t13n:ta", "ta-t-i0-und"},
    {"t13n:te", "te-t-i0-und"},
    {"t13n:ti", "ti-t-i0-und"},
    {"t13n:ur", "ur-t-i0-und"},
};

const struct EnglishToResouceId {
  const char* english_string_from_ibus;
  int resource_id;
} kEnglishToResourceIdArray[] = {
    // For xkb-layouts.
    {"xkb:am:phonetic:arm", IDS_STATUSBAR_LAYOUT_ARMENIAN_PHONETIC},
    {"xkb:be::fra", IDS_STATUSBAR_LAYOUT_BELGIUM},
    {"xkb:be::ger", IDS_STATUSBAR_LAYOUT_BELGIUM},
    {"xkb:be::nld", IDS_STATUSBAR_LAYOUT_BELGIUM},
    {"xkb:bg::bul", IDS_STATUSBAR_LAYOUT_BULGARIA},
    {"xkb:bg:phonetic:bul", IDS_STATUSBAR_LAYOUT_BULGARIA_PHONETIC},
    {"xkb:br::por", IDS_STATUSBAR_LAYOUT_BRAZIL},
    {"xkb:by::bel", IDS_STATUSBAR_LAYOUT_BELARUSIAN},
    {"xkb:ca::fra", IDS_STATUSBAR_LAYOUT_CANADA},
    {"xkb:ca:eng:eng", IDS_STATUSBAR_LAYOUT_CANADA_ENGLISH},
    {"xkb:ca:multix:fra", IDS_STATUSBAR_LAYOUT_CANADIAN_MULTILINGUAL},
    {"xkb:ch::ger", IDS_STATUSBAR_LAYOUT_SWITZERLAND},
    {"xkb:ch:fr:fra", IDS_STATUSBAR_LAYOUT_SWITZERLAND_FRENCH},
    {"xkb:cz::cze", IDS_STATUSBAR_LAYOUT_CZECHIA},
    {"xkb:cz:qwerty:cze", IDS_STATUSBAR_LAYOUT_CZECHIA_QWERTY},
    {"xkb:de::ger", IDS_STATUSBAR_LAYOUT_GERMANY},
    {"xkb:de:neo:ger", IDS_STATUSBAR_LAYOUT_GERMANY_NEO2},
    {"xkb:dk::dan", IDS_STATUSBAR_LAYOUT_DENMARK},
    {"xkb:ee::est", IDS_STATUSBAR_LAYOUT_ESTONIA},
    {"xkb:es::spa", IDS_STATUSBAR_LAYOUT_SPAIN},
    {"xkb:es:cat:cat", IDS_STATUSBAR_LAYOUT_SPAIN_CATALAN},
    {"xkb:fi::fin", IDS_STATUSBAR_LAYOUT_FINLAND},
    {"xkb:fo::fao", IDS_STATUSBAR_LAYOUT_FAROESE},
    {"xkb:fr::fra", IDS_STATUSBAR_LAYOUT_FRANCE},
    {"xkb:fr:bepo:fra", IDS_STATUSBAR_LAYOUT_FRANCE_BEPO},
    {"xkb:gb:dvorak:eng", IDS_STATUSBAR_LAYOUT_UNITED_KINGDOM_DVORAK},
    {"xkb:gb:extd:eng", IDS_STATUSBAR_LAYOUT_UNITED_KINGDOM},
    {"xkb:ge::geo", IDS_STATUSBAR_LAYOUT_GEORGIAN},
    {"xkb:gr::gre", IDS_STATUSBAR_LAYOUT_GREECE},
    {"xkb:hr::scr", IDS_STATUSBAR_LAYOUT_CROATIA},
    {"xkb:hu::hun", IDS_STATUSBAR_LAYOUT_HUNGARY},
    {"xkb:hu:qwerty:hun", IDS_STATUSBAR_LAYOUT_HUNGARY_QWERTY},
    {"xkb:ie::ga", IDS_STATUSBAR_LAYOUT_IRISH},
    {"xkb:il::heb", IDS_STATUSBAR_LAYOUT_ISRAEL},
    {"xkb:is::ice", IDS_STATUSBAR_LAYOUT_ICELANDIC},
    {"xkb:it::ita", IDS_STATUSBAR_LAYOUT_ITALY},
    {"xkb:jp::jpn", IDS_STATUSBAR_LAYOUT_JAPAN},
    {"xkb:kz::kaz", IDS_STATUSBAR_LAYOUT_KAZAKH},
    {"xkb:latam::spa", IDS_STATUSBAR_LAYOUT_LATIN_AMERICAN},
    {"xkb:lt::lit", IDS_STATUSBAR_LAYOUT_LITHUANIA},
    {"xkb:lv:apostrophe:lav", IDS_STATUSBAR_LAYOUT_LATVIA},
    {"xkb:mk::mkd", IDS_STATUSBAR_LAYOUT_MACEDONIAN},
    {"xkb:mn::mon", IDS_STATUSBAR_LAYOUT_MONGOLIAN},
    {"xkb:nl::nld", IDS_STATUSBAR_LAYOUT_NETHERLANDS},
    {"xkb:no::nob", IDS_STATUSBAR_LAYOUT_NORWAY},
    {"xkb:pl::pol", IDS_STATUSBAR_LAYOUT_POLAND},
    {"xkb:pt::por", IDS_STATUSBAR_LAYOUT_PORTUGAL},
    {"xkb:ro::rum", IDS_STATUSBAR_LAYOUT_ROMANIA},
    {"xkb:ro:std:rum", IDS_IME_NAME_KEYBOARD_ROMANIAN_STANDARD},
    {"xkb:rs::srp", IDS_STATUSBAR_LAYOUT_SERBIA},
    {"xkb:ru::rus", IDS_STATUSBAR_LAYOUT_RUSSIA},
    {"xkb:ru:phonetic:rus", IDS_STATUSBAR_LAYOUT_RUSSIA_PHONETIC},
    {"xkb:se::swe", IDS_STATUSBAR_LAYOUT_SWEDEN},
    {"xkb:si::slv", IDS_STATUSBAR_LAYOUT_SLOVENIA},
    {"xkb:sk::slo", IDS_STATUSBAR_LAYOUT_SLOVAKIA},
    {"xkb:tr::tur", IDS_STATUSBAR_LAYOUT_TURKEY},
    {"xkb:tr:f:tur", IDS_STATUSBAR_LAYOUT_TURKEY_F},
    {"xkb:ua::ukr", IDS_STATUSBAR_LAYOUT_UKRAINE},
    {"xkb:us::eng", IDS_STATUSBAR_LAYOUT_USA},
    {"xkb:us::fil", IDS_IME_NAME_KEYBOARD_FILIPINO},
    {"xkb:us::ind", IDS_IME_NAME_KEYBOARD_INDONESIAN},
    {"xkb:us::msa", IDS_IME_NAME_KEYBOARD_MALAY},
    {"xkb:us:altgr-intl:eng", IDS_STATUSBAR_LAYOUT_USA_EXTENDED},
    {"xkb:us:colemak:eng", IDS_STATUSBAR_LAYOUT_USA_COLEMAK},
    {"xkb:us:dvorak:eng", IDS_STATUSBAR_LAYOUT_USA_DVORAK},
    {"xkb:us:dvp:eng", IDS_STATUSBAR_LAYOUT_USA_DVP},
    {"xkb:us:intl:eng", IDS_STATUSBAR_LAYOUT_USA_INTERNATIONAL},
    {"xkb:us:intl:nld", IDS_STATUSBAR_LAYOUT_USA_INTERNATIONAL},
    {"xkb:us:intl:por", IDS_STATUSBAR_LAYOUT_USA_INTERNATIONAL},
    {"xkb:us:intl_pc:eng", IDS_STATUSBAR_LAYOUT_USA_INTERNATIONAL_PC},
    {"xkb:us:intl_pc:nld", IDS_STATUSBAR_LAYOUT_USA_INTERNATIONAL_PC},
    {"xkb:us:intl_pc:por", IDS_STATUSBAR_LAYOUT_USA_INTERNATIONAL_PC},
    {"xkb:us:workman-intl:eng", IDS_STATUSBAR_LAYOUT_USA_WORKMAN_INTERNATIONAL},
    {"xkb:us:workman:eng", IDS_STATUSBAR_LAYOUT_USA_WORKMAN},
};
const size_t kEnglishToResourceIdArraySize =
    std::size(kEnglishToResourceIdArray);

const struct InputMethodNameMap {
  const char* message_name;
  int resource_id;
  bool operator<(const InputMethodNameMap& other) const {
    return strcmp(message_name, other.message_name) < 0;
  }
} kInputMethodNameMap[] = {
    {"__MSG_INPUTMETHOD_ARRAY__", IDS_IME_NAME_INPUTMETHOD_ARRAY},
    {"__MSG_INPUTMETHOD_CANGJIE__", IDS_IME_NAME_INPUTMETHOD_CANGJIE},
    {"__MSG_INPUTMETHOD_CANTONESE__", IDS_IME_NAME_INPUTMETHOD_CANTONESE},
    {"__MSG_INPUTMETHOD_DAYI__", IDS_IME_NAME_INPUTMETHOD_DAYI},
    {"__MSG_INPUTMETHOD_HANGUL_2_SET__", IDS_IME_NAME_INPUTMETHOD_HANGUL_2_SET},
    {"__MSG_INPUTMETHOD_HANGUL_3_SET_390__",
     IDS_IME_NAME_INPUTMETHOD_HANGUL_3_SET_390},
    {"__MSG_INPUTMETHOD_HANGUL_3_SET_FINAL__",
     IDS_IME_NAME_INPUTMETHOD_HANGUL_3_SET_FINAL},
    {"__MSG_INPUTMETHOD_HANGUL_3_SET_NO_SHIFT__",
     IDS_IME_NAME_INPUTMETHOD_HANGUL_3_SET_NO_SHIFT},
    {"__MSG_INPUTMETHOD_HANGUL_AHNMATAE__",
     IDS_IME_NAME_INPUTMETHOD_HANGUL_AHNMATAE},
    {"__MSG_INPUTMETHOD_HANGUL_ROMAJA__",
     IDS_IME_NAME_INPUTMETHOD_HANGUL_ROMAJA},
    {"__MSG_INPUTMETHOD_HANGUL__", IDS_IME_NAME_INPUTMETHOD_HANGUL},
    {"__MSG_INPUTMETHOD_MOZC_JP__", IDS_IME_NAME_INPUTMETHOD_MOZC_JP},
    {"__MSG_INPUTMETHOD_MOZC_US__", IDS_IME_NAME_INPUTMETHOD_MOZC_US},
    {"__MSG_INPUTMETHOD_PINYIN__", IDS_IME_NAME_INPUTMETHOD_PINYIN},
    {"__MSG_INPUTMETHOD_QUICK__", IDS_IME_NAME_INPUTMETHOD_QUICK},
    {"__MSG_INPUTMETHOD_TRADITIONAL_PINYIN__",
     IDS_IME_NAME_INPUTMETHOD_TRADITIONAL_PINYIN},
    {"__MSG_INPUTMETHOD_WUBI__", IDS_IME_NAME_INPUTMETHOD_WUBI},
    {"__MSG_INPUTMETHOD_ZHUYIN__", IDS_IME_NAME_INPUTMETHOD_ZHUYIN},
    {"__MSG_KEYBOARD_ARABIC__", IDS_IME_NAME_KEYBOARD_ARABIC},
    {"__MSG_KEYBOARD_ARMENIAN_PHONETIC__",
     IDS_IME_NAME_KEYBOARD_ARMENIAN_PHONETIC},
    {"__MSG_KEYBOARD_BELARUSIAN__", IDS_IME_NAME_KEYBOARD_BELARUSIAN},
    {"__MSG_KEYBOARD_BELGIAN_DUTCH__", IDS_IME_NAME_KEYBOARD_BELGIAN_DUTCH},
    {"__MSG_KEYBOARD_BELGIAN_FRENCH__", IDS_IME_NAME_KEYBOARD_BELGIAN_FRENCH},
    {"__MSG_KEYBOARD_BELGIAN_GERMAN__", IDS_IME_NAME_KEYBOARD_BELGIAN_GERMAN},
    {"__MSG_KEYBOARD_BENGALI_PHONETIC__",
     IDS_IME_NAME_KEYBOARD_BENGALI_PHONETIC},
    {"__MSG_KEYBOARD_BRAZILIAN__", IDS_IME_NAME_KEYBOARD_BRAZILIAN},
    {"__MSG_KEYBOARD_BULGARIAN_PHONETIC__",
     IDS_IME_NAME_KEYBOARD_BULGARIAN_PHONETIC},
    {"__MSG_KEYBOARD_BULGARIAN__", IDS_IME_NAME_KEYBOARD_BULGARIAN},
    {"__MSG_KEYBOARD_CANADIAN_ENGLISH__",
     IDS_IME_NAME_KEYBOARD_CANADIAN_ENGLISH},
    {"__MSG_KEYBOARD_CANADIAN_FRENCH__", IDS_IME_NAME_KEYBOARD_CANADIAN_FRENCH},
    {"__MSG_KEYBOARD_CANADIAN_MULTILINGUAL__",
     IDS_IME_NAME_KEYBOARD_CANADIAN_MULTILINGUAL},
    {"__MSG_KEYBOARD_CATALAN__", IDS_IME_NAME_KEYBOARD_CATALAN},
    {"__MSG_KEYBOARD_CROATIAN__", IDS_IME_NAME_KEYBOARD_CROATIAN},
    {"__MSG_KEYBOARD_CZECH_QWERTY__", IDS_IME_NAME_KEYBOARD_CZECH_QWERTY},
    {"__MSG_KEYBOARD_CZECH__", IDS_IME_NAME_KEYBOARD_CZECH},
    {"__MSG_KEYBOARD_DANISH__", IDS_IME_NAME_KEYBOARD_DANISH},
    {"__MSG_KEYBOARD_DEVANAGARI_PHONETIC__",
     IDS_IME_NAME_KEYBOARD_DEVANAGARI_PHONETIC},
    {"__MSG_KEYBOARD_ENGLISH_INDIA__", IDS_IME_NAME_KEYBOARD_ENGLISH_INDIA},
    {"__MSG_KEYBOARD_ENGLISH_PAKISTAN__",
     IDS_IME_NAME_KEYBOARD_ENGLISH_PAKISTAN},
    {"__MSG_KEYBOARD_ENGLISH_SOUTH_AFRICA__",
     IDS_IME_NAME_KEYBOARD_ENGLISH_SOUTH_AFRICA},
    {"__MSG_KEYBOARD_ESTONIAN__", IDS_IME_NAME_KEYBOARD_ESTONIAN},
    {"__MSG_KEYBOARD_ETHIOPIC__", IDS_IME_NAME_KEYBOARD_ETHIOPIC},
    {"__MSG_KEYBOARD_FAROESE__", IDS_IME_NAME_KEYBOARD_FAROESE},
    {"__MSG_KEYBOARD_FILIPINO__", IDS_IME_NAME_KEYBOARD_FILIPINO},
    {"__MSG_KEYBOARD_FINNISH__", IDS_IME_NAME_KEYBOARD_FINNISH},
    {"__MSG_KEYBOARD_FRENCH_BEPO__", IDS_IME_NAME_KEYBOARD_FRENCH_BEPO},
    {"__MSG_KEYBOARD_FRENCH__", IDS_IME_NAME_KEYBOARD_FRENCH},
    {"__MSG_KEYBOARD_GEORGIAN__", IDS_IME_NAME_KEYBOARD_GEORGIAN},
    {"__MSG_KEYBOARD_GERMAN_NEO_2__", IDS_IME_NAME_KEYBOARD_GERMAN_NEO_2},
    {"__MSG_KEYBOARD_GERMAN__", IDS_IME_NAME_KEYBOARD_GERMAN},
    {"__MSG_KEYBOARD_GREEK__", IDS_IME_NAME_KEYBOARD_GREEK},
    {"__MSG_KEYBOARD_GUJARATI_PHONETIC__",
     IDS_IME_NAME_KEYBOARD_GUJARATI_PHONETIC},
    {"__MSG_KEYBOARD_HEBREW__", IDS_IME_NAME_KEYBOARD_HEBREW},
    {"__MSG_KEYBOARD_HINDI_INSCRIPT__", IDS_IME_NAME_KEYBOARD_HINDI_INSCRIPT},
    {"__MSG_KEYBOARD_HUNGARIAN_QWERTY__",
     IDS_IME_NAME_KEYBOARD_HUNGARIAN_QWERTY},
    {"__MSG_KEYBOARD_HUNGARIAN__", IDS_IME_NAME_KEYBOARD_HUNGARIAN},
    {"__MSG_KEYBOARD_ICELANDIC__", IDS_IME_NAME_KEYBOARD_ICELANDIC},
    {"__MSG_KEYBOARD_INDONESIAN__", IDS_IME_NAME_KEYBOARD_INDONESIAN},
    {"__MSG_KEYBOARD_IRISH__", IDS_IME_NAME_KEYBOARD_IRISH},
    {"__MSG_KEYBOARD_ITALIAN__", IDS_IME_NAME_KEYBOARD_ITALIAN},
    {"__MSG_KEYBOARD_JAPANESE__", IDS_IME_NAME_KEYBOARD_JAPANESE},
    {"__MSG_KEYBOARD_KANNADA_PHONETIC__",
     IDS_IME_NAME_KEYBOARD_KANNADA_PHONETIC},
    {"__MSG_KEYBOARD_KAZAKH__", IDS_IME_NAME_KEYBOARD_KAZAKH},
    {"__MSG_KEYBOARD_KHMER__", IDS_IME_NAME_KEYBOARD_KHMER},
    {"__MSG_KEYBOARD_LAO__", IDS_IME_NAME_KEYBOARD_LAO},
    {"__MSG_KEYBOARD_LATIN_AMERICAN__", IDS_IME_NAME_KEYBOARD_LATIN_AMERICAN},
    {"__MSG_KEYBOARD_LATVIAN__", IDS_IME_NAME_KEYBOARD_LATVIAN},
    {"__MSG_KEYBOARD_LITHUANIAN__", IDS_IME_NAME_KEYBOARD_LITHUANIAN},
    {"__MSG_KEYBOARD_MACEDONIAN__", IDS_IME_NAME_KEYBOARD_MACEDONIAN},
    {"__MSG_KEYBOARD_MALAYALAM_PHONETIC__",
     IDS_IME_NAME_KEYBOARD_MALAYALAM_PHONETIC},
    {"__MSG_KEYBOARD_MALAY__", IDS_IME_NAME_KEYBOARD_MALAY},
    {"__MSG_KEYBOARD_MALTESE__", IDS_IME_NAME_KEYBOARD_MALTESE},
    {"__MSG_KEYBOARD_MONGOLIAN__", IDS_IME_NAME_KEYBOARD_MONGOLIAN},
    {"__MSG_KEYBOARD_MYANMAR_MYANSAN__", IDS_IME_NAME_KEYBOARD_MYANMAR_MYANSAN},
    {"__MSG_KEYBOARD_MYANMAR__", IDS_IME_NAME_KEYBOARD_MYANMAR},
    {"__MSG_KEYBOARD_NEPALI_INSCRIPT__", IDS_IME_NAME_KEYBOARD_NEPALI_INSCRIPT},
    {"__MSG_KEYBOARD_NEPALI_PHONETIC__", IDS_IME_NAME_KEYBOARD_NEPALI_PHONETIC},
    {"__MSG_KEYBOARD_NETHERLANDS_US_INTERNATIONAL_PC__",
     IDS_IME_NAME_KEYBOARD_NETHERLANDS_US_INTERNATIONAL_PC},
    {"__MSG_KEYBOARD_NETHERLANDS__", IDS_IME_NAME_KEYBOARD_NETHERLANDS},
    {"__MSG_KEYBOARD_NORWEGIAN__", IDS_IME_NAME_KEYBOARD_NORWEGIAN},
    {"__MSG_KEYBOARD_PERSIAN__", IDS_IME_NAME_KEYBOARD_PERSIAN},
    {"__MSG_KEYBOARD_POLISH__", IDS_IME_NAME_KEYBOARD_POLISH},
    {"__MSG_KEYBOARD_PORTUGUESE_US_INTERNATIONAL_PC__",
     IDS_IME_NAME_KEYBOARD_PORTUGUESE_US_INTERNATIONAL_PC},
    {"__MSG_KEYBOARD_PORTUGUESE_US_INTERNATIONAL__",
     IDS_IME_NAME_KEYBOARD_PORTUGUESE_US_INTERNATIONAL},
    {"__MSG_KEYBOARD_PORTUGUESE__", IDS_IME_NAME_KEYBOARD_PORTUGUESE},
    {"__MSG_KEYBOARD_ROMANIAN_STANDARD__",
     IDS_IME_NAME_KEYBOARD_ROMANIAN_STANDARD},
    {"__MSG_KEYBOARD_ROMANIAN__", IDS_IME_NAME_KEYBOARD_ROMANIAN},
    {"__MSG_KEYBOARD_RUSSIAN_PHONETIC_AATSEEL__",
     IDS_IME_NAME_KEYBOARD_RUSSIAN_PHONETIC_AATSEEL},
    {"__MSG_KEYBOARD_RUSSIAN_PHONETIC_YAZHERT__",
     IDS_IME_NAME_KEYBOARD_RUSSIAN_PHONETIC_YAZHERT},
    {"__MSG_KEYBOARD_RUSSIAN_PHONETIC__",
     IDS_IME_NAME_KEYBOARD_RUSSIAN_PHONETIC},
    {"__MSG_KEYBOARD_RUSSIAN__", IDS_IME_NAME_KEYBOARD_RUSSIAN},
    {"__MSG_KEYBOARD_SERBIAN__", IDS_IME_NAME_KEYBOARD_SERBIAN},
    {"__MSG_KEYBOARD_SINHALA__", IDS_IME_NAME_KEYBOARD_SINHALA},
    {"__MSG_KEYBOARD_SLOVAK__", IDS_IME_NAME_KEYBOARD_SLOVAK},
    {"__MSG_KEYBOARD_SLOVENIAN__", IDS_IME_NAME_KEYBOARD_SLOVENIAN},
    {"__MSG_KEYBOARD_SORANIKURDISH_AR__",
     IDS_IME_NAME_KEYBOARD_SORANIKURDISH_AR},
    {"__MSG_KEYBOARD_SORANIKURDISH_EN__",
     IDS_IME_NAME_KEYBOARD_SORANIKURDISH_EN},
    {"__MSG_KEYBOARD_SPANISH__", IDS_IME_NAME_KEYBOARD_SPANISH},
    {"__MSG_KEYBOARD_SWEDISH__", IDS_IME_NAME_KEYBOARD_SWEDISH},
    {"__MSG_KEYBOARD_SWISS_FRENCH__", IDS_IME_NAME_KEYBOARD_SWISS_FRENCH},
    {"__MSG_KEYBOARD_SWISS__", IDS_IME_NAME_KEYBOARD_SWISS},
    {"__MSG_KEYBOARD_TAMIL_INSCRIPT__", IDS_IME_NAME_KEYBOARD_TAMIL_INSCRIPT},
    {"__MSG_KEYBOARD_TAMIL_ITRANS__", IDS_IME_NAME_KEYBOARD_TAMIL_ITRANS},
    {"__MSG_KEYBOARD_TAMIL_PHONETIC__", IDS_IME_NAME_KEYBOARD_TAMIL_PHONETIC},
    {"__MSG_KEYBOARD_TAMIL_TAMIL99__", IDS_IME_NAME_KEYBOARD_TAMIL_TAMIL99},
    {"__MSG_KEYBOARD_TAMIL_TYPEWRITER__",
     IDS_IME_NAME_KEYBOARD_TAMIL_TYPEWRITER},
    {"__MSG_KEYBOARD_TELUGU_PHONETIC__", IDS_IME_NAME_KEYBOARD_TELUGU_PHONETIC},
    {"__MSG_KEYBOARD_THAI_KEDMANEE__", IDS_IME_NAME_KEYBOARD_THAI_KEDMANEE},
    {"__MSG_KEYBOARD_THAI_PATTACHOTE__", IDS_IME_NAME_KEYBOARD_THAI_PATTACHOTE},
    {"__MSG_KEYBOARD_THAI_TIS__", IDS_IME_NAME_KEYBOARD_THAI_TIS},
    {"__MSG_KEYBOARD_TURKISH_F__", IDS_IME_NAME_KEYBOARD_TURKISH_F},
    {"__MSG_KEYBOARD_TURKISH__", IDS_IME_NAME_KEYBOARD_TURKISH},
    {"__MSG_KEYBOARD_UKRAINIAN__", IDS_IME_NAME_KEYBOARD_UKRAINIAN},
    {"__MSG_KEYBOARD_UK_DVORAK__", IDS_IME_NAME_KEYBOARD_UK_DVORAK},
    {"__MSG_KEYBOARD_UK__", IDS_IME_NAME_KEYBOARD_UK},
    {"__MSG_KEYBOARD_US_COLEMAK__", IDS_IME_NAME_KEYBOARD_US_COLEMAK},
    {"__MSG_KEYBOARD_US_DVORAK__", IDS_IME_NAME_KEYBOARD_US_DVORAK},
    {"__MSG_KEYBOARD_US_DVP__", IDS_IME_NAME_KEYBOARD_US_DVP},
    {"__MSG_KEYBOARD_US_EXTENDED__", IDS_IME_NAME_KEYBOARD_US_EXTENDED},
    {"__MSG_KEYBOARD_US_INTERNATIONAL_PC__",
     IDS_IME_NAME_KEYBOARD_US_INTERNATIONAL_PC},
    {"__MSG_KEYBOARD_US_INTERNATIONAL__",
     IDS_IME_NAME_KEYBOARD_US_INTERNATIONAL},
    {"__MSG_KEYBOARD_US_WORKMAN_INTERNATIONAL__",
     IDS_IME_NAME_KEYBOARD_US_WORKMAN_INTERNATIONAL},
    {"__MSG_KEYBOARD_US_WORKMAN__", IDS_IME_NAME_KEYBOARD_US_WORKMAN},
    {"__MSG_KEYBOARD_US__", IDS_IME_NAME_KEYBOARD_US},
    {"__MSG_KEYBOARD_VIETNAMESE_TCVN__", IDS_IME_NAME_KEYBOARD_VIETNAMESE_TCVN},
    {"__MSG_KEYBOARD_VIETNAMESE_TELEX__",
     IDS_IME_NAME_KEYBOARD_VIETNAMESE_TELEX},
    {"__MSG_KEYBOARD_VIETNAMESE_VIQR__", IDS_IME_NAME_KEYBOARD_VIETNAMESE_VIQR},
    {"__MSG_KEYBOARD_VIETNAMESE_VNI__", IDS_IME_NAME_KEYBOARD_VIETNAMESE_VNI},
    {"__MSG_TRANSLITERATION_AM__", IDS_IME_NAME_TRANSLITERATION_AM},
    {"__MSG_TRANSLITERATION_AR__", IDS_IME_NAME_TRANSLITERATION_AR},
    {"__MSG_TRANSLITERATION_BN__", IDS_IME_NAME_TRANSLITERATION_BN},
    {"__MSG_TRANSLITERATION_EL__", IDS_IME_NAME_TRANSLITERATION_EL},
    {"__MSG_TRANSLITERATION_FA__", IDS_IME_NAME_TRANSLITERATION_FA},
    {"__MSG_TRANSLITERATION_GU__", IDS_IME_NAME_TRANSLITERATION_GU},
    {"__MSG_TRANSLITERATION_HE__", IDS_IME_NAME_TRANSLITERATION_HE},
    {"__MSG_TRANSLITERATION_HI__", IDS_IME_NAME_TRANSLITERATION_HI},
    {"__MSG_TRANSLITERATION_KN__", IDS_IME_NAME_TRANSLITERATION_KN},
    {"__MSG_TRANSLITERATION_ML__", IDS_IME_NAME_TRANSLITERATION_ML},
    {"__MSG_TRANSLITERATION_MR__", IDS_IME_NAME_TRANSLITERATION_MR},
    {"__MSG_TRANSLITERATION_NE__", IDS_IME_NAME_TRANSLITERATION_NE},
    {"__MSG_TRANSLITERATION_OR__", IDS_IME_NAME_TRANSLITERATION_OR},
    {"__MSG_TRANSLITERATION_PA__", IDS_IME_NAME_TRANSLITERATION_PA},
    {"__MSG_TRANSLITERATION_SA__", IDS_IME_NAME_TRANSLITERATION_SA},
    {"__MSG_TRANSLITERATION_SR__", IDS_IME_NAME_TRANSLITERATION_SR},
    {"__MSG_TRANSLITERATION_TA__", IDS_IME_NAME_TRANSLITERATION_TA},
    {"__MSG_TRANSLITERATION_TE__", IDS_IME_NAME_TRANSLITERATION_TE},
    {"__MSG_TRANSLITERATION_TI__", IDS_IME_NAME_TRANSLITERATION_TI},
    {"__MSG_TRANSLITERATION_UR__", IDS_IME_NAME_TRANSLITERATION_UR},
};

// Inserts {key, value} into the multimap if it does not exist.
void MultimapDeduplicatedInsert(LanguageCodeToIdsMap& multimap,
                                const std::string& key,
                                const std::string& value) {
  using It = LanguageCodeToIdsMap::const_iterator;
  std::pair<It, It> range = multimap.equal_range(key);
  It it = range.first;
  for (; it != range.second; ++it) {
    if (it->second == value) {
      return;
    }
  }
  multimap.insert(it, {key, value});
}

}  // namespace

InputMethodUtil::InputMethodUtil(InputMethodDelegate* delegate)
    : delegate_(delegate) {
  InputMethodDescriptors default_input_methods;
  default_input_methods.push_back(GetFallbackInputMethodDescriptor());
  ResetInputMethods(default_input_methods);

  // Initialize a map from English string to Chrome string resource ID as well.
  // Since this array is write-once, initialize a flat map in one step with a
  // given vector storage.
  //
  // TODO(brettw) this could be optimized further to binary search in the
  // static data, avoiding this up-front cost.
  std::vector<EnglishToIDMap::value_type> map_storage;
  map_storage.reserve(kEnglishToResourceIdArraySize);
  for (const auto& map_entry : kEnglishToResourceIdArray) {
    map_storage.emplace_back(map_entry.english_string_from_ibus,
                             map_entry.resource_id);
  }

  english_to_resource_id_ = EnglishToIDMap(std::move(map_storage));
  DCHECK(english_to_resource_id_.size() == kEnglishToResourceIdArraySize)
      << "Duplicate string is found";
}

InputMethodUtil::~InputMethodUtil() = default;

std::string InputMethodUtil::GetLocalizedDisplayName(
    const InputMethodDescriptor& descriptor) {
  // Localizes the input method name.
  const std::string& disp = descriptor.name();
  if (base::StartsWith(disp, "__MSG_", base::CompareCase::SENSITIVE)) {
    const InputMethodNameMap* map = kInputMethodNameMap;
    size_t map_size = std::size(kInputMethodNameMap);
    std::string name = base::ToUpperASCII(disp);
    const InputMethodNameMap map_key = {name.c_str(), 0};
    const InputMethodNameMap* p =
        std::lower_bound(map, map + map_size, map_key);
    if (p != map + map_size && name == p->message_name) {
      return l10n_util::GetStringUTF8(p->resource_id);
    }
  }
  return disp;
}

bool InputMethodUtil::IsValidInputMethodId(
    const std::string& input_method_id) const {
  // We can't check the component extension is whilelisted or not here because
  // it might not be initialized.
  return GetInputMethodDescriptorFromId(input_method_id) != nullptr ||
         extension_ime_util::IsComponentExtensionIME(input_method_id) ||
         extension_ime_util::IsArcIME(input_method_id);
}

// static
bool InputMethodUtil::IsKeyboardLayout(const std::string& input_method_id) {
  return base::StartsWith(input_method_id,
                          "xkb:", base::CompareCase::INSENSITIVE_ASCII) ||
         extension_ime_util::IsKeyboardLayoutExtension(input_method_id);
}

std::u16string InputMethodUtil::GetInputMethodMediumName(
    const InputMethodDescriptor& input_method) const {
  // For the "Your input method has changed to..." bubble. In most cases
  // it uses the same name as the short name, unless found in a table
  // for medium length names.
  for (const auto& i : kMappingImeIdToMediumLenNameResourceId) {
    if (extension_ime_util::GetInputMethodIDByEngineID(i.engine_id) ==
        input_method.id()) {
      return delegate_->GetLocalizedString(i.resource_id);
    }
  }
  return input_method.GetIndicator();
}

std::u16string InputMethodUtil::GetInputMethodLongNameInternal(
    const InputMethodDescriptor& input_method,
    bool short_name) const {
  std::string localized_display_name = GetLocalizedDisplayName(input_method);
  const std::string& input_method_id = input_method.id();
  if (!localized_display_name.empty() && !IsKeyboardLayout(input_method_id)) {
    // If the descriptor has a name, use it.
    return base::UTF8ToUTF16(localized_display_name);
  }

  std::u16string text;
  if (short_name || localized_display_name.empty()) {
    // Gets legacy xkb id (e.g. xkb:us::eng) from the new extension based xkb id
    // (e.g. _comp_ime_...xkb:us::eng). If the given id is not prefixed with
    // 'xkb:', just return the same as the given id.
    std::string key_string =
        extension_ime_util::IsKeyboardLayoutExtension(input_method_id)
            ? extension_ime_util::GetComponentIDByInputMethodID(input_method_id)
            : input_method_id;

    auto iter = english_to_resource_id_.find(key_string);

    if (iter == english_to_resource_id_.end()) {
      // TODO(yusukes): Write Autotest which checks if all display names and all
      // property names for supported input methods are listed in the resource
      // ID array (crosbug.com/4572).
      LOG(ERROR) << "Resource ID is not found for: " << input_method_id << ", "
                 << key_string;
      text = base::UTF8ToUTF16(input_method_id);
    } else {
      text = delegate_->GetLocalizedString(iter->second);
    }
  } else {
    text = base::UTF8ToUTF16(localized_display_name);
  }

  DCHECK(!text.empty());
  return text;
}

std::u16string InputMethodUtil::GetInputMethodLongNameStripped(
    const InputMethodDescriptor& input_method) const {
  return GetInputMethodLongNameInternal(input_method, true /* short_name */);
}

std::u16string InputMethodUtil::GetInputMethodLongName(
    const InputMethodDescriptor& input_method) const {
  return GetInputMethodLongNameInternal(input_method, false /* short_name */);
}

const InputMethodDescriptor* InputMethodUtil::GetInputMethodDescriptorFromId(
    const std::string& input_method_id) const {
  auto iter = id_to_descriptor_.find(input_method_id);
  if (iter == id_to_descriptor_.end()) {
    return nullptr;
  }
  return &(iter->second);
}

bool InputMethodUtil::GetInputMethodIdsFromLanguageCode(
    std::string_view normalized_language_code,
    InputMethodType type,
    std::vector<std::string>* out_input_method_ids) const {
  return GetInputMethodIdsFromLanguageCodeInternal(language_code_to_ids_,
                                                   normalized_language_code,
                                                   type, out_input_method_ids);
}

bool InputMethodUtil::GetInputMethodIdsFromLanguageCodeInternal(
    const LanguageCodeToIdsMap& language_code_to_ids,
    std::string_view normalized_language_code,
    InputMethodType type,
    std::vector<std::string>* out_input_method_ids) {
  DCHECK(out_input_method_ids);
  out_input_method_ids->clear();

  bool result = false;
  std::pair<LanguageCodeToIdsMap::const_iterator,
            LanguageCodeToIdsMap::const_iterator>
      range = language_code_to_ids.equal_range(normalized_language_code);
  for (auto iter = range.first; iter != range.second; ++iter) {
    const std::string& input_method_id = iter->second;
    if ((type == kAllInputMethods) || IsKeyboardLayout(input_method_id)) {
      out_input_method_ids->push_back(input_method_id);
      result = true;
    }
  }
  if ((type == kAllInputMethods) && !result) {
    DVLOG(1) << "Unknown language code: " << normalized_language_code;
  }
  return result;
}

std::vector<std::string>
InputMethodUtil::GetInputMethodIdsFromHandwritingLanguage(
    std::string_view handwriting_language) {
  std::vector<std::string> input_method_ids;

  using It = LanguageCodeToIdsMap::const_iterator;
  std::pair<It, It> range =
      handwriting_language_to_ids_.equal_range(handwriting_language);
  for (It iter = range.first; iter != range.second; ++iter) {
    input_method_ids.push_back(iter->second);
  }

  return input_method_ids;
}

void InputMethodUtil::GetFirstLoginInputMethodIds(
    const std::string& language_code,
    const InputMethodDescriptor& preferred_input_method,
    std::vector<std::string>* out_input_method_ids) const {
  out_input_method_ids->clear();

  // First, add the preferred keyboard layout (e.g. one used on the login
  // screen or set in UserContext when starting a public session).
  out_input_method_ids->push_back(preferred_input_method.id());

  const std::string current_layout = preferred_input_method.keyboard_layout();
  for (const auto& i : kDefaultInputMethodRecommendation) {
    if (i.locale == language_code &&
        (!i.layout[0] || i.layout == current_layout)) {
      out_input_method_ids->push_back(
          extension_ime_util::GetInputMethodIDByEngineID(i.engine_id));
      return;
    }
  }

  std::vector<std::string> input_method_ids;
  GetInputMethodIdsFromLanguageCode(language_code, kAllInputMethods,
                                    &input_method_ids);
  // Uses the first input method as the most popular one.
  if (input_method_ids.size() > 0 &&
      preferred_input_method.id() != input_method_ids[0]) {
    out_input_method_ids->push_back(input_method_ids[0]);
  }
}

void InputMethodUtil::GetLanguageCodesFromInputMethodIds(
    const std::vector<std::string>& input_method_ids,
    std::vector<std::string>* out_language_codes) const {
  out_language_codes->clear();

  for (const auto& i : input_method_ids) {
    const std::string& input_method_id = i;
    const InputMethodDescriptor* input_method =
        GetInputMethodDescriptorFromId(input_method_id);
    if (!input_method) {
      DVLOG(1) << "Unknown input method ID: " << i;
      continue;
    }
    DCHECK(!input_method->language_codes().empty());
    const std::string language_code = input_method->language_codes().at(0);
    // Add it if it's not already present.
    if (!base::Contains(*out_language_codes, language_code)) {
      out_language_codes->push_back(language_code);
    }
  }
}

std::string InputMethodUtil::GetLanguageDefaultInputMethodId(
    const std::string& language_code) {
  std::vector<std::string> candidates;
  GetInputMethodIdsFromLanguageCode(
      language_code, input_method::kKeyboardLayoutsOnly, &candidates);
  if (candidates.size()) {
    return candidates.front();
  }

  return std::string();
}

std::string InputMethodUtil::GetMigratedInputMethod(
    const std::string& input_method_id) {
  std::string engine_id = input_method_id;
  // Migrates some Engine IDs from VPD.
  for (const auto& entry : kEngineIdMigrationMap) {
    size_t pos = engine_id.find(entry[0]);
    if (pos == 0) {
      engine_id.replace(0, strlen(entry[0]), entry[1]);
      break;
    }
  }
  // Migrates the extension IDs.
  std::string id = extension_ime_util::GetInputMethodIDByEngineID(engine_id);
  if (extension_ime_util::IsComponentExtensionIME(id)) {
    std::string id_new = extension_ime_util::GetInputMethodIDByEngineID(
        extension_ime_util::GetComponentIDByInputMethodID(id));
    if (extension_ime_util::IsComponentExtensionIME(id_new)) {
      id = id_new;
    }
  }
  return id;
}

bool InputMethodUtil::GetMigratedInputMethodIDs(
    std::vector<std::string>* input_method_ids) {
  bool rewritten = false;
  std::vector<std::string>& ids = *input_method_ids;
  for (std::string& i : ids) {
    std::string id = GetMigratedInputMethod(i);
    if (id != i) {
      i = id;
      rewritten = true;
    }
  }
  if (rewritten) {
    // Removes the duplicates.
    std::vector<std::string> new_ids;
    std::unordered_set<std::string> ids_set;
    for (const auto& id : ids) {
      if (ids_set.find(id) == ids_set.end()) {
        new_ids.push_back(id);
      }
      ids_set.insert(id);
    }
    ids.swap(new_ids);
  }
  return rewritten;
}

void InputMethodUtil::UpdateHardwareLayoutCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  hardware_layouts_.clear();
  hardware_login_layouts_.clear();
  if (cached_hardware_layouts_.empty()) {
    cached_hardware_layouts_ =
        base::SplitString(delegate_->GetHardwareKeyboardLayouts(), ",",
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
  hardware_layouts_ = cached_hardware_layouts_;
  GetMigratedInputMethodIDs(&hardware_layouts_);

  bool has_xkb = false;
  for (const auto& hardware_layout : hardware_layouts_) {
    if (IsLoginKeyboard(hardware_layout)) {
      hardware_login_layouts_.push_back(hardware_layout);
    }
    if (extension_ime_util::IsKeyboardLayoutExtension(hardware_layout)) {
      has_xkb = true;
    }
  }

  if (hardware_login_layouts_.empty()) {
    // This is totally fine if |hardware_layouts_| is empty. The hardware
    // keyboard layout is not stored if startup_manifest.json
    // (OEM customization data) is not present (ex. Cr48 doen't have that file).
    // So need to make sure |hardware_login_layouts_| is not empty, and
    // |hardware_layouts_| contains at least one login layout.
    std::string fallback_id = GetFallbackInputMethodDescriptor().id();
    hardware_login_layouts_.push_back(fallback_id);
    // If has XKB input method, it means the XKB input method is
    // non-login-able. Therefore, add the fallback to the hardware layouts.
    // If has no XKB input method, then it is up to the VPD to set the correct
    // hardware input methods.
    // Examples:
    // 1) Arabic transliteration input method cannot be used to input Latin
    // characters. So the VPD should be "xkb:us::eng,t13n:ar".
    // 2) Korean input method can be used to input Latin characters. So the
    // VPD should be "ime:ko:hangul". See chrome-os-partner:48623.
    // 3) Russian keyboard cannot be used to input Latin characters, but it is
    // XKB input method. So the VPD can be "xkb:ru::rus".
    if (hardware_layouts_.empty() || has_xkb) {
      hardware_layouts_.insert(hardware_layouts_.begin(), fallback_id);
    }
  }
}

void InputMethodUtil::SetHardwareKeyboardLayoutForTesting(
    const std::string& layout) {
  delegate_->SetHardwareKeyboardLayoutForTesting(layout);
  cached_hardware_layouts_.clear();
  UpdateHardwareLayoutCache();
}

const std::vector<std::string>& InputMethodUtil::GetHardwareInputMethodIds() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateHardwareLayoutCache();
  return hardware_layouts_;
}

const std::vector<std::string>&
InputMethodUtil::GetHardwareLoginInputMethodIds() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateHardwareLayoutCache();
  return hardware_login_layouts_;
}

bool InputMethodUtil::IsLoginKeyboard(
    const std::string& input_method_id) const {
  const InputMethodDescriptor* ime =
      GetInputMethodDescriptorFromId(input_method_id);
  return ime ? ime->is_login_keyboard() : false;
}

void InputMethodUtil::AppendInputMethods(const InputMethodDescriptors& imes) {
  for (const auto& input_method : imes) {
    DCHECK(!input_method.language_codes().empty());
    const std::vector<std::string>& language_codes =
        input_method.language_codes();
    id_to_descriptor_[input_method.id()] = input_method;

    for (const auto& language_code : language_codes) {
      MultimapDeduplicatedInsert(language_code_to_ids_, language_code,
                                 input_method.id());
    }

    const std::optional<std::string>& handwriting_language =
        input_method.handwriting_language();
    if (handwriting_language.has_value()) {
      MultimapDeduplicatedInsert(handwriting_language_to_ids_,
                                 *handwriting_language, input_method.id());
    }
  }
}

void InputMethodUtil::ResetInputMethods(const InputMethodDescriptors& imes) {
  // Clear the existing maps.
  language_code_to_ids_.clear();
  handwriting_language_to_ids_.clear();
  id_to_descriptor_.clear();

  AppendInputMethods(imes);
}

void InputMethodUtil::InitXkbInputMethodsForTesting(
    const InputMethodDescriptors& imes) {
  cached_hardware_layouts_.clear();
  ResetInputMethods(imes);
}

InputMethodDescriptor InputMethodUtil::GetFallbackInputMethodDescriptor() {
  std::vector<std::string> languages;
  languages.emplace_back("en-US");
  return InputMethodDescriptor(
      extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng"), "", "US",
      "us",  // layout
      languages,
      true,                                  // login keyboard.
      GURL(),                                // options page, not available.
      GURL(),                                // input view page, not available.
      /*handwriting_language=*/std::nullopt  // not available.
  );
}

}  // namespace input_method
}  // namespace ash
