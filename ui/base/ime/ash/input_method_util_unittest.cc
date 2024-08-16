// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/ime/ash/input_method_util.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/fake_input_method_delegate.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace input_method {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace {

const char pinyin_ime_id[] = "zh-t-i0-pinyin";
const char zhuyin_ime_id[] = "zh-hant-t-i0-und";

class TestableInputMethodUtil : public InputMethodUtil {
 public:
  explicit TestableInputMethodUtil(InputMethodDelegate* delegate)
      : InputMethodUtil(delegate) {}

  // Change access rights.
  using InputMethodUtil::GetInputMethodIdsFromLanguageCodeInternal;
};

}  // namespace

class InputMethodUtilTest : public testing::Test {
 public:
  InputMethodUtilTest() : util_(&delegate_) {
    delegate_.set_get_localized_string_callback(
        base::BindRepeating(&l10n_util::GetStringUTF16));

    xkb_input_method_descriptors_ = {
        GetDesc(Id("xkb:us::eng"), "", "us", {"en", "en-US", "en-AU", "en-NZ"},
                "US", true),
        GetDesc(Id("xkb:fr::fra"), "", "fr(oss)", {"fr", "fr-FR"}, "FR", true),
        GetDesc(Id("xkb:il::heb"), "", "il", {"he"}, "IL", true),
        GetDesc(Id("xkb:jp::jpn"), "", "jp", {"ja"}, "JA", true),
    };

    non_xkb_input_method_descriptors_ = {
        GetDesc(Id(pinyin_ime_id), "Pinyin input for testing", "us", {"zh-CN"},
                "CN", false),
        GetDesc(Id(zhuyin_ime_id), "Zhuyin input for testing", "us", {"zh-TW"},
                "TW", false),
    };
  }

  void SetUp() override {
    util_.InitXkbInputMethodsForTesting(xkb_input_method_descriptors_);
    util_.AppendInputMethods(non_xkb_input_method_descriptors_);
  }

  static std::string Id(const std::string& id) {
    return extension_ime_util::GetInputMethodIDByEngineID(id);
  }

  static InputMethodDescriptor GetDesc(
      const std::string& id,
      const std::string& description,
      const std::string& layout,
      const std::vector<std::string>& language_codes,
      const std::string& indicator,
      bool is_login_keyboard) {
    return InputMethodDescriptor(Id(id), description,
                                 indicator,  // Short name used for indicator.
                                 layout, language_codes, is_login_keyboard,
                                 GURL(),  // options page url
                                 GURL(),  // input view page url
                                 /*handwriting_language=*/std::nullopt);
  }

  FakeInputMethodDelegate delegate_;
  TestableInputMethodUtil util_;
  InputMethodDescriptors xkb_input_method_descriptors_;
  InputMethodDescriptors non_xkb_input_method_descriptors_;
};

TEST_F(InputMethodUtilTest, GetInputMethodMediumNameTest) {
  {
    // input methods with medium name equal to short name
    const char* const input_method_ids[] = {
        "xkb:us:altgr-intl:eng", "xkb:us:dvorak:eng", "xkb:us:intl:eng",
        "xkb:us:colemak:eng",    "xkb:de:neo:ger",    "xkb:es:cat:cat",
        "xkb:gb:dvorak:eng",
    };
    for (const char* id : input_method_ids) {
      InputMethodDescriptor desc = GetDesc(id, "", "", {""}, "", true);
      std::u16string medium_name = util_.GetInputMethodMediumName(desc);
      EXPECT_EQ(medium_name, desc.GetIndicator());
    }
  }
  {
    // input methods with medium name not equal to short name
    const char* const input_method_ids[] = {
        pinyin_ime_id,
        zhuyin_ime_id,
    };
    for (const char* id : input_method_ids) {
      InputMethodDescriptor desc = GetDesc(id, "", "", {""}, "", true);
      std::u16string medium_name = util_.GetInputMethodMediumName(desc);
      EXPECT_NE(medium_name, desc.GetIndicator());
    }
  }
}

TEST_F(InputMethodUtilTest, GetInputMethodLongNameTest) {
  // Input method or keyboard layout name is returned.
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:jp::jpn", "", "jp", {"ja"}, "", true);
    EXPECT_EQ(u"Japanese", util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:us:dvorak:eng", "", "us(dvorak)", {"en-US"}, "", true);
    EXPECT_EQ(u"US Dvorak", util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:gb:dvorak:eng", "", "gb(dvorak)", {"en-US"}, "", true);
    EXPECT_EQ(u"UK Dvorak", util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("invalid-id", "", "us", {"xx"}, "", true);
    // You can safely ignore the "Resouce ID is not found for: invalid-id"
    // error.
    EXPECT_EQ(u"invalid-id", util_.GetInputMethodLongName(desc));
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

TEST_F(InputMethodUtilTest, TestGetInputMethodDescriptorFromId) {
  EXPECT_EQ(nullptr, util_.GetInputMethodDescriptorFromId("non_existent"));

  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId(Id(pinyin_ime_id));
  ASSERT_TRUE(nullptr != descriptor);  // ASSERT_NE doesn't compile.
  EXPECT_EQ(Id(pinyin_ime_id), descriptor->id());
  EXPECT_EQ("us", descriptor->keyboard_layout());
  // This used to be "zh" but now we have "zh-CN" in input_methods.h,
  // hence this should be zh-CN now.
  ASSERT_TRUE(!descriptor->language_codes().empty());
  EXPECT_EQ("zh-CN", descriptor->language_codes().at(0));
}

TEST_F(InputMethodUtilTest, TestGetInputMethodIdsForLanguageCode) {
  LanguageCodeToIdsMap language_code_to_ids_map;
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

InputMethodDescriptor DescWithIdAndHandwritingLanguage(
    const std::string& id,
    std::optional<std::string> handwriting_language) {
  const std::string input_method_id =
      extension_ime_util::GetInputMethodIDByEngineID(id);
  return InputMethodDescriptor(
      input_method_id, /*name=*/"", /*indicator=*/"",
      /*keyboard_layout=*/"",
      /*language_codes=*/{""},  // Must be non-empty to avoid a DCHECK.
      /*is_login_keyboard=*/false,
      /*options_page_url=*/{}, /*input_view_url=*/{},
      /*handwriting_language=*/handwriting_language);
}

TEST_F(InputMethodUtilTest, TestGetInputMethodIdsForHandwritingLanguage) {
  // Re-setup all descriptors with new 1P descriptors.
  std::vector<InputMethodDescriptor> descriptors = {
      DescWithIdAndHandwritingLanguage("xkb:us::eng", "en"),
      DescWithIdAndHandwritingLanguage("xkb:gb:extd:eng", "en"),
      DescWithIdAndHandwritingLanguage("xkb:fr::fra", "fr")};
  util_.InitXkbInputMethodsForTesting(descriptors);
  util_.AppendInputMethods(non_xkb_input_method_descriptors_);

  EXPECT_THAT(
      util_.GetInputMethodIdsFromHandwritingLanguage("en"),
      UnorderedElementsAre(
          extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng"),
          extension_ime_util::GetInputMethodIDByEngineID("xkb:gb:extd:eng")));
  EXPECT_THAT(
      util_.GetInputMethodIdsFromHandwritingLanguage("fr"),
      UnorderedElementsAre(
          extension_ime_util::GetInputMethodIDByEngineID("xkb:fr::fra")));
  EXPECT_THAT(util_.GetInputMethodIdsFromHandwritingLanguage("zh"), IsEmpty());
  EXPECT_THAT(util_.GetInputMethodIdsFromHandwritingLanguage("zh-CN"),
              IsEmpty());
  EXPECT_THAT(util_.GetInputMethodIdsFromHandwritingLanguage("zh-TW"),
              IsEmpty());
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
  EXPECT_EQ(Id(pinyin_ime_id), input_method_ids[1]);  // Pinyin for US keyboard.
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
  for (const auto& migration_case : migration_cases) {
    input_method_ids.emplace_back(migration_case[0]);
  }
  // Duplicated hangul_2set.
  input_method_ids.emplace_back("ime:ko:hangul_2set");

  util_.GetMigratedInputMethodIDs(&input_method_ids);

  EXPECT_EQ(std::size(migration_cases), input_method_ids.size());
  for (size_t i = 0; i < std::size(migration_cases); ++i) {
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
}  // namespace ash
