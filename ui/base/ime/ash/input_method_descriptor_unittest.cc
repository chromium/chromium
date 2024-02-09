// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/input_method_descriptor.h"

#include <stddef.h>

#include <optional>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"

namespace ash {
namespace input_method {
namespace {

InputMethodDescriptor CreateDesc(const std::string& id,
                                 const std::string& layout,
                                 const std::vector<std::string>& language_codes,
                                 const std::string& indicator,
                                 bool is_login_keyboard) {
  return InputMethodDescriptor(
      extension_ime_util::GetInputMethodIDByEngineID(id), /* name= */ "",
      indicator, layout, language_codes, /* is_login_keyboard= */ true,
      /* options_page_url= */ GURL(), /* input_view_url= */ GURL(),
      /*handwriting_language=*/std::nullopt);
}

TEST(InputMethodDescriptorTest, GetIndicatorTest) {
  // Test invalid cases. Two-letter language code should be returned.
  {
    InputMethodDescriptor desc =
        CreateDesc("invalid-id", "us", {"xx"}, "", true);
    // Upper-case string of the unknown language code, "xx", should be returned.
    EXPECT_EQ(u"XX", desc.GetIndicator());
  }

  // Test special cases.
  {
    InputMethodDescriptor desc =
        CreateDesc("xkb:us:dvorak:eng", "us", {"en-US"}, "DV", true);
    EXPECT_EQ(u"DV", desc.GetIndicator());
  }
  {
    InputMethodDescriptor desc =
        CreateDesc("xkb:us:colemak:eng", "us", {"en-US"}, "CO", true);
    EXPECT_EQ(u"CO", desc.GetIndicator());
  }
  {
    InputMethodDescriptor desc =
        CreateDesc("xkb:us:altgr-intl:eng", "us", {"en-US"}, "EXTD", true);
    EXPECT_EQ(u"EXTD", desc.GetIndicator());
  }
  {
    InputMethodDescriptor desc =
        CreateDesc("xkb:us:intl:eng", "us", {"en-US"}, "INTL", true);
    EXPECT_EQ(u"INTL", desc.GetIndicator());
  }
  {
    InputMethodDescriptor desc =
        CreateDesc("xkb:de:neo:ger", "de(neo)", {"de"}, "NEO", true);
    EXPECT_EQ(u"NEO", desc.GetIndicator());
  }
  {
    InputMethodDescriptor desc =
        CreateDesc("xkb:es:cat:cat", "es(cat)", {"ca"}, "CAT", true);
    EXPECT_EQ(u"CAT", desc.GetIndicator());
  }
  {
    InputMethodDescriptor desc =
        CreateDesc("zh-t-i0-pinyin", "us", {"zh-CN"}, "拼", true);
    EXPECT_EQ(u"拼", desc.GetIndicator());
  }
  {
    InputMethodDescriptor desc =
        CreateDesc("zh-hant-t-i0-und", "us", {"zh-TW"}, "注", true);
    EXPECT_EQ(u"注", desc.GetIndicator());
  }
}

}  // namespace
}  // namespace input_method
}  // namespace ash
