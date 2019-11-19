// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/input_method_util.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/fake_input_method_delegate.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_whitelist.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;

namespace chromeos {

namespace input_method {

namespace {

const char pinyin_ime_id[] = "zh-t-i0-pinyin";
const char zhuyin_ime_id[] = "zh-hant-t-i0-und";

class TestableInputMethodUtil : public InputMethodUtil {
 public:
  explicit TestableInputMethodUtil(
      InputMethodDelegate* delegate,
      std::unique_ptr<InputMethodDescriptors> methods)
      : InputMethodUtil(delegate) {
    ResetInputMethods(*methods);
  }
  // Change access rights.
  using InputMethodUtil::GetInputMethodIdsFromLanguageCodeInternal;
  using InputMethodUtil::GetKeyboardLayoutName;
};

}  // namespace

class InputMethodUtilTest : public testing::Test {
 public:
  InputMethodUtilTest()
      : util_(&delegate_, whitelist_.GetSupportedInputMethods()) {
    delegate_.set_get_localized_string_callback(
        base::Bind(&l10n_util::GetStringUTF16));
    delegate_.set_get_display_language_name_callback(
        base::Bind(&InputMethodUtilTest::GetDisplayLanguageName));
  }

  void SetUp() override {
    InputMethodDescriptors input_methods;

    std::vector<std::string> layouts;
    std::vector<std::string> languages;
    layouts.emplace_back("us");
    languages.emplace_back("zh-CN");

    InputMethodDescriptor pinyin_ime(Id(pinyin_ime_id),
                                     "Pinyin input for testing",
                                     "CN",
                                     layouts,
                                     languages,
                                     false,
                                     GURL(""),
                                     GURL(""));
    input_methods.push_back(pinyin_ime);

    languages.clear();
    languages.emplace_back("zh-TW");
    InputMethodDescriptor zhuyin_ime(zhuyin_ime_id,
                                     "Zhuyin input for testing",
                                     "TW",
                                     layouts,
                                     languages,
                                     false,
                                     GURL(""),
                                     GURL(""));
    input_methods.push_back(zhuyin_ime);

    util_.InitXkbInputMethodsForTesting(*whitelist_.GetSupportedInputMethods());
    util_.AppendInputMethods(input_methods);
  }

  std::string Id(const std::string& id) {
    return extension_ime_util::GetInputMethodIDByEngineID(id);
  }

  InputMethodDescriptor GetDesc(const std::string& id,
                                const std::string& raw_layout,
                                const std::string& language_code,
                                const std::string& indicator) {
    std::vector<std::string> layouts;
    layouts.push_back(raw_layout);
    std::vector<std::string> languages;
    languages.push_back(language_code);
    return InputMethodDescriptor(Id(id),
                                 "",         // Description.
                                 indicator,  // Short name used for indicator.
                                 layouts,
                                 languages,
                                 true,
                                 GURL(),   // options page url
                                 GURL());  // input view page url
  }

  static base::string16 GetDisplayLanguageName(
      const std::string& language_code) {
    return l10n_util::GetDisplayNameForLocale(language_code, "en", true);
  }

  FakeInputMethodDelegate delegate_;
  InputMethodWhitelist whitelist_;
  TestableInputMethodUtil util_;
};

TEST_F(InputMethodUtilTest, GetInputMethodShortNameTest) {
  // Test invalid cases. Two-letter language code should be returned.
  {
    InputMethodDescriptor desc = GetDesc("invalid-id", "us", "xx", "");
    // Upper-case string of the unknown language code, "xx", should be returned.
    EXPECT_EQ(ASCIIToUTF16("XX"), util_.GetInputMethodShortName(desc));
  }

  // Test special cases.
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:us:dvorak:eng", "us", "en-US", "DV");
    EXPECT_EQ(ASCIIToUTF16("DV"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:us:colemak:eng", "us", "en-US", "CO");
    EXPECT_EQ(ASCIIToUTF16("CO"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:us:altgr-intl:eng", "us", "en-US", "EXTD");
    EXPECT_EQ(ASCIIToUTF16("EXTD"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:us:intl:eng", "us", "en-US", "INTL");
    EXPECT_EQ(ASCIIToUTF16("INTL"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:de:neo:ger", "de(neo)", "de", "NEO");
    EXPECT_EQ(ASCIIToUTF16("NEO"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:es:cat:cat", "es(cat)", "ca", "CAT");
    EXPECT_EQ(ASCIIToUTF16("CAT"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(pinyin_ime_id, "us", "zh-CN", "\xe6\x8b\xbc");
    EXPECT_EQ(base::UTF8ToUTF16("\xe6\x8b\xbc"),
              util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc(zhuyin_ime_id, "us", "zh-TW", "\xE6\xB3\xA8");
    EXPECT_EQ(base::UTF8ToUTF16("\xE6\xB3\xA8"),
              util_.GetInputMethodShortName(desc));
  }
}

TEST_F(InputMethodUtilTest, GetInputMethodMediumNameTest) {
  {
    // input methods with medium name equal to short name
    const char* const input_method_ids[] = {
        "xkb:us:altgr-intl:eng", "xkb:us:dvorak:eng", "xkb:us:intl:eng",
        "xkb:us:colemak:eng",    "xkb:de:neo:ger",    "xkb:es:cat:cat",
        "xkb:gb:dvorak:eng",
    };
    for (const char* id : input_method_ids) {
      InputMethodDescriptor desc = GetDesc(id, "", "", "");
      base::string16 medium_name = util_.GetInputMethodMediumName(desc);
      base::string16 short_name = util_.GetInputMethodShortName(desc);
      EXPECT_EQ(medium_name, short_name);
    }
  }
  {
    // input methods with medium name not equal to short name
    const char* const input_method_ids[] = {
        pinyin_ime_id,
        zhuyin_ime_id,
    };
    for (const char* id : input_method_ids) {
      InputMethodDescriptor desc = GetDesc(id, "", "", "");
      base::string16 medium_name = util_.GetInputMethodMediumName(desc);
      base::string16 short_name = util_.GetInputMethodShortName(desc);
      EXPECT_NE(medium_name, short_name);
    }
  }
}

TEST_F(InputMethodUtilTest, GetInputMethodLongNameTest) {
  // For most languages input method or keyboard layout name is returned.
  // See below for exceptions.
  {
    InputMethodDescriptor desc = GetDesc("xkb:jp::jpn", "jp", "ja", "");
    EXPECT_EQ(ASCIIToUTF16("Japanese"), util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:us:dvorak:eng", "us(dvorak)", "en-US", "");
    EXPECT_EQ(ASCIIToUTF16("US Dvorak"), util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:gb:dvorak:eng", "gb(dvorak)", "en-US", "");
    EXPECT_EQ(ASCIIToUTF16("UK Dvorak"), util_.GetInputMethodLongName(desc));
  }

  // For Dutch, French, German and Hindi,
  // "language - keyboard layout" pair is returned.
  {
    InputMethodDescriptor desc = GetDesc("xkb:be::nld", "be", "nl", "");
    EXPECT_EQ(ASCIIToUTF16("Dutch - Belgian"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:fr::fra", "fr", "fr", "");
    EXPECT_EQ(ASCIIToUTF16("French - French"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:be::fra", "be", "fr", "");
    EXPECT_EQ(ASCIIToUTF16("French - Belgian"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:de::ger", "de", "de", "");
    EXPECT_EQ(ASCIIToUTF16("German - German"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:be::ger", "be", "de", "");
    EXPECT_EQ(ASCIIToUTF16("German - Belgian"),
              util_.GetInputMethodLongName(desc));
  }

  {
    InputMethodDescriptor desc = GetDesc("invalid-id", "us", "xx", "");
    // You can safely ignore the "Resouce ID is not found for: invalid-id"
    // error.
    EXPECT_EQ(ASCIIToUTF16("invalid-id"), util_.GetInputMethodLongName(desc));
  }
}

TEST_F(InputMethodUtilTest, TestIsValidInputMethodId) {
  EXPECT_TRUE(util_.IsValidInputMethodId(Id("xkb:us:colemak:eng")));
  EXPECT_TRUE(util_.IsValidInputMethodId(Id(pinyin_ime_id)));
  EXPECT_FALSE(util_.IsValidInputMethodId("unsupported-input-method"));
}

TEST_F(InputMethodUtilTest, TestIsKeyboardLayout) {
  EXPECT_TRUE(InputMethodUtil::IsKeyboardLayout("xkb:us::eng"));
  EXPECT_FALSE(InputMethodUtil::IsKeyboardLayout(Id(pinyin_ime_id)));
}

TEST_F(InputMethodUtilTest, TestGetKeyboardLayoutName) {
  // Unsupported case.
  EXPECT_EQ("", util_.GetKeyboardLayoutName("UNSUPPORTED_ID"));

  // Supported cases (samples).
  EXPECT_EQ("us", util_.GetKeyboardLayoutName(Id(pinyin_ime_id)));
  EXPECT_EQ("es", util_.GetKeyboardLayoutName(Id("xkb:es::spa")));
  EXPECT_EQ("es(cat)", util_.GetKeyboardLayoutName(Id("xkb:es:cat:cat")));
  EXPECT_EQ("gb(extd)", util_.GetKeyboardLayoutName(Id("xkb:gb:extd:eng")));
  EXPECT_EQ("us", util_.GetKeyboardLayoutName(Id("xkb:us::eng")));
  EXPECT_EQ("us(dvorak)", util_.GetKeyboardLayoutName(Id("xkb:us:dvorak:eng")));
  EXPECT_EQ("us(colemak)",
            util_.GetKeyboardLayoutName(Id("xkb:us:colemak:eng")));
  EXPECT_EQ("de(neo)", util_.GetKeyboardLayoutName(Id("xkb:de:neo:ger")));
}

TEST_F(InputMethodUtilTest, TestGetInputMethodDisplayNameFromId) {
  EXPECT_EQ("US", util_.GetInputMethodDisplayNameFromId("xkb:us::eng"));
  EXPECT_EQ("", util_.GetInputMethodDisplayNameFromId("nonexistent"));
}

TEST_F(InputMethodUtilTest, TestGetInputMethodDescriptorFromId) {
  EXPECT_EQ(nullptr, util_.GetInputMethodDescriptorFromId("non_existent"));

  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id(pinyin_ime_id));
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  EXPECT_EQ(Id(pinyin_ime_id), descriptor->id());
  EXPECT_EQ("us", descriptor->GetPreferredKeyboardLayout());
  // This used to be "zh" but now we have "zh-CN" in input_methods.h,
  // hence this should be zh-CN now.
  ASSERT_TRUE(!descriptor->language_codes().empty());
  EXPECT_EQ("zh-CN", descriptor->language_codes().at(0));
}

TEST_F(InputMethodUtilTest, TestGetInputMethodIdsForLanguageCode) {
  std::multimap<std::string, std::string> language_code_to_ids_map;
  language_code_to_ids_map.emplace("ja", pinyin_ime_id);
  language_code_to_ids_map.emplace("ja", pinyin_ime_id);
  language_code_to_ids_map.emplace("ja", "xkb:jp:jpn");
  language_code_to_ids_map.emplace("fr", "xkb:fr:fra");

  std::vector<std::string> result;
  EXPECT_TRUE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "ja", kAllInputMethods, &result));
  EXPECT_EQ(3U, result.size());
  EXPECT_TRUE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "ja", kKeyboardLayoutsOnly, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("xkb:jp:jpn", result[0]);

  EXPECT_TRUE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "fr", kAllInputMethods, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("xkb:fr:fra", result[0]);
  EXPECT_TRUE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "fr", kKeyboardLayoutsOnly, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("xkb:fr:fra", result[0]);

  EXPECT_FALSE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "invalid_lang", kAllInputMethods, &result));
  EXPECT_FALSE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "invalid_lang", kKeyboardLayoutsOnly, &result));
}

// US keyboard + English US UI = US keyboard only.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_EnUs) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id("xkb:us::eng"));  // US keyboard.
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("en-US", *descriptor, &input_method_ids);
  ASSERT_EQ(1U, input_method_ids.size());
  EXPECT_EQ(Id("xkb:us::eng"), input_method_ids[0]);
}

// US keyboard + Chinese UI = US keyboard + Pinyin IME.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_Zh) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id("xkb:us::eng"));  // US keyboard.
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("zh-CN", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ(Id("xkb:us::eng"), input_method_ids[0]);
  EXPECT_EQ(Id(pinyin_ime_id), input_method_ids[1]);  // Pinyin for US keybaord.
}

// US keyboard + Russian UI = US keyboard + Russsian keyboard
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_Ru) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id("xkb:us::eng"));  // US keyboard.
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("ru", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ(Id("xkb:us::eng"), input_method_ids[0]);
  EXPECT_EQ(Id("xkb:ru::rus"), input_method_ids[1]);  // Russian keyboard.
}

// US keyboard + Traditional Chinese = US keyboard + chewing.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_ZhTw) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id("xkb:us::eng"));  // US keyboard.
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("zh-TW", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ(Id("xkb:us::eng"), input_method_ids[0]);
  EXPECT_EQ(Id(zhuyin_ime_id), input_method_ids[1]);  // Chewing.
}

// US keyboard + Thai = US keyboard + kesmanee.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_Th) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id("xkb:us::eng"));  // US keyboard.
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("th", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ(Id("xkb:us::eng"), input_method_ids[0]);
  EXPECT_EQ(Id("vkd_th"), input_method_ids[1]);  // Kesmanee.
}

// US keyboard + Vietnamese = US keyboard + TCVN6064.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_Vi) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id("xkb:us::eng"));  // US keyboard.
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("vi", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ(Id("xkb:us::eng"), input_method_ids[0]);
  EXPECT_EQ(Id("vkd_vi_tcvn"), input_method_ids[1]);  // TCVN6064.
}

// US keyboard + Japanese = US keyboard + mozc(us).
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_Jp) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id("xkb:us::eng"));  // US keyboard.
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("ja", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ(Id("xkb:us::eng"), input_method_ids[0]);
  EXPECT_EQ(Id("nacl_mozc_us"), input_method_ids[1]);
}

// JP keyboard + Japanese = JP keyboard + mozc(jp).
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Jp_And_Jp) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id("xkb:jp::jpn"));  // JP keyboard.
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("ja", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ(Id("xkb:jp::jpn"), input_method_ids[0]);
  EXPECT_EQ(Id("nacl_mozc_jp"), input_method_ids[1]);
}

// US keyboard + Hebrew = US keyboard + Hebrew keyboard.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_He) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id("xkb:us::eng"));  // US keyboard.
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("he", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ(Id("xkb:us::eng"), input_method_ids[0]);
  EXPECT_EQ(Id("xkb:il::heb"), input_method_ids[1]);
}

TEST_F(InputMethodUtilTest, TestGetLanguageCodesFromInputMethodIds) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back(Id("xkb:us::eng"));        // English US.
  input_method_ids.push_back(Id("xkb:us:dvorak:eng"));  // English US Dvorak.
  input_method_ids.push_back(Id(pinyin_ime_id));        // Pinyin
  input_method_ids.push_back(Id("xkb:fr::fra"));        // French France.
  std::vector<std::string> language_codes;
  util_.GetLanguageCodesFromInputMethodIds(input_method_ids, &language_codes);
  ASSERT_EQ(3U, language_codes.size());
  EXPECT_EQ("en", language_codes[0]);
  EXPECT_EQ("zh-CN", language_codes[1]);
  EXPECT_EQ("fr", language_codes[2]);
}

// Test all supported descriptors to detect a typo in input_methods.txt.
TEST_F(InputMethodUtilTest, TestIBusInputMethodText) {
  const std::map<std::string, InputMethodDescriptor>& id_to_descriptor =
      util_.GetIdToDesciptorMapForTesting();
  for (const auto& it : id_to_descriptor) {
    const std::string language_code = it.second.language_codes().at(0);
    const base::string16 display_name =
        l10n_util::GetDisplayNameForLocale(language_code, "en", false);
    // Only two formats, like "fr" (lower case) and "en-US" (lower-upper), are
    // allowed. See the text file for details.
    EXPECT_TRUE(language_code == "fil" || language_code.length() == 2 ||
                (language_code.length() == 5 && language_code[2] == '-'))
        << "Invalid language code " << language_code;
    EXPECT_TRUE(l10n_util::IsValidLocaleSyntax(language_code))
        << "Invalid language code " << language_code;
    EXPECT_FALSE(display_name.empty())
        << "Invalid language code " << language_code;
    // On error, GetDisplayNameForLocale() returns the |language_code| as-is.
    EXPECT_NE(language_code, base::UTF16ToUTF8(display_name))
        << "Invalid language code " << language_code;
  }
}

// Test the input method ID migration.
TEST_F(InputMethodUtilTest, TestInputMethodIDMigration) {
  const char* const migration_cases[][2] = {
      {"ime:zh:pinyin", "zh-t-i0-pinyin"},
      {"ime:zh-t:zhuyin", "zh-hant-t-i0-und"},
      {"ime:zh-t:quick", "zh-hant-t-i0-cangjie-1987-x-m0-simplified"},
      {"ime:jp:mozc_us", "nacl_mozc_us"},
      {"ime:ko:hangul", "ko-t-i0-und"},
      {"m17n:deva_phone", "vkd_deva_phone"},
      {"m17n:ar", "vkd_ar"},
      {"t13n:hi", "hi-t-i0-und"},
      {"unknown", "unknown"},
  };
  std::vector<std::string> input_method_ids;
  for (const auto& migration_case : migration_cases)
    input_method_ids.emplace_back(migration_case[0]);
  // Duplicated hangul_2set.
  input_method_ids.emplace_back("ime:ko:hangul_2set");

  util_.MigrateInputMethods(&input_method_ids);

  EXPECT_EQ(base::size(migration_cases), input_method_ids.size());
  for (size_t i = 0; i < base::size(migration_cases); ++i) {
    EXPECT_EQ(
        extension_ime_util::GetInputMethodIDByEngineID(migration_cases[i][1]),
        input_method_ids[i]);
  }
}

// Test getting hardware input method IDs.
TEST_F(InputMethodUtilTest, TestHardwareInputMethodIDs) {
  util_.SetHardwareKeyboardLayoutForTesting("xkb:ru::rus");
  std::vector<std::string> input_method_ids = util_.GetHardwareInputMethodIds();
  std::vector<std::string> login_input_method_ids =
      util_.GetHardwareLoginInputMethodIds();

  EXPECT_EQ(2U, input_method_ids.size());
  EXPECT_EQ(1U, login_input_method_ids.size());

  EXPECT_EQ("xkb:us::eng", extension_ime_util::GetComponentIDByInputMethodID(
                               input_method_ids[0]));
  EXPECT_EQ("xkb:ru::rus", extension_ime_util::GetComponentIDByInputMethodID(
                               input_method_ids[1]));
  EXPECT_EQ("xkb:us::eng", extension_ime_util::GetComponentIDByInputMethodID(
                               login_input_method_ids[0]));
}

}  // namespace input_method
}  // namespace chromeos
