// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/extension_ime_util.h"

#include <string>

#include "base/strings/strcat.h"
#include "build/branding_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(ExtensionIMEUtilTest, GetInputMethodIDTest) {
  EXPECT_EQ("_ext_ime_ABCDE12345",
            extension_ime_util::GetInputMethodID("ABCDE", "12345"));
}

TEST(ExtensionIMEUtilTest, GetComponentInputMethodID) {
  EXPECT_EQ("_comp_ime_ABCDE12345",
            extension_ime_util::GetComponentInputMethodID("ABCDE", "12345"));
}

TEST(ExtensionIMEUtilTest, GetArcInputMethodIDTest) {
  EXPECT_EQ("_arc_ime_ABCDE12345",
            extension_ime_util::GetArcInputMethodID("ABCDE", "12345"));
}

TEST(ExtensionIMEUtilTest, GetExtensionIDFromInputMethodIDTest) {
  EXPECT_EQ("", extension_ime_util::GetExtensionIDFromInputMethodID("mozc"));
  EXPECT_EQ("12345678901234567890123456789012",
            extension_ime_util::GetExtensionIDFromInputMethodID(
                extension_ime_util::GetInputMethodID(
                    "12345678901234567890123456789012", "mozc")));
  EXPECT_EQ("12345678901234567890123456789012",
            extension_ime_util::GetExtensionIDFromInputMethodID(
                extension_ime_util::GetComponentInputMethodID(
                    "12345678901234567890123456789012", "mozc")));
}

TEST(ExtensionIMEUtilTest, IsExtensionIMETest) {
  EXPECT_TRUE(
      extension_ime_util::IsExtensionIME(extension_ime_util::GetInputMethodID(
          "abcde_xxxxxxxxxxxxxxxxxxxxxxxxxx", "12345")));
  EXPECT_FALSE(extension_ime_util::IsExtensionIME(
      extension_ime_util::GetComponentInputMethodID(
          "abcde_xxxxxxxxxxxxxxxxxxxxxxxxxx", "12345")));
  EXPECT_FALSE(extension_ime_util::IsExtensionIME(
      extension_ime_util::GetArcInputMethodID(
          "abcde_xxxxxxxxxxxxxxxxxxxxxxxxxx", "12345")));
  EXPECT_FALSE(extension_ime_util::IsExtensionIME(""));
  EXPECT_FALSE(extension_ime_util::IsExtensionIME("mozc"));
}

TEST(ExtensionIMEUtilTest, IsComponentExtensionIMETest) {
  EXPECT_TRUE(extension_ime_util::IsComponentExtensionIME(
      extension_ime_util::GetComponentInputMethodID(
          "abcde_xxxxxxxxxxxxxxxxxxxxxxxxxx", "12345")));
  EXPECT_FALSE(extension_ime_util::IsComponentExtensionIME(
      extension_ime_util::GetInputMethodID("abcde_xxxxxxxxxxxxxxxxxxxxxxxxxx",
                                           "12345")));
  EXPECT_FALSE(extension_ime_util::IsComponentExtensionIME(
      extension_ime_util::GetArcInputMethodID(
          "abcde_xxxxxxxxxxxxxxxxxxxxxxxxxx", "12345")));
  EXPECT_FALSE(extension_ime_util::IsComponentExtensionIME(""));
  EXPECT_FALSE(extension_ime_util::IsComponentExtensionIME("mozc"));
}

TEST(ExtensionIMEUtilTest, IsArcIMETest) {
  EXPECT_TRUE(
      extension_ime_util::IsArcIME(extension_ime_util::GetArcInputMethodID(
          "abcde_xxxxxxxxxxxxxxxxxxxxxxxxxx", "12345")));
  EXPECT_FALSE(
      extension_ime_util::IsArcIME(extension_ime_util::GetInputMethodID(
          "abcde_xxxxxxxxxxxxxxxxxxxxxxxxxx", "12345")));
  EXPECT_FALSE(extension_ime_util::IsArcIME(
      extension_ime_util::GetComponentInputMethodID(
          "abcde_xxxxxxxxxxxxxxxxxxxxxxxxxx", "12345")));
  EXPECT_FALSE(extension_ime_util::IsArcIME(""));
  EXPECT_FALSE(extension_ime_util::IsArcIME("mozc"));
}

TEST(ExtensionIMEUtilTest, IsCros1pKoreanTest) {
  // TODO(crbug.com/1162211): Input method IDs are tuples of extension type,
  // extension ID, and extension-local input method ID. However, currently
  // they're just concats of the three constituent pieces of info, hence StrCat
  // here. Replace StrCat once they're no longer unstructured string concats.

  EXPECT_FALSE(extension_ime_util::IsCros1pKorean(
      base::StrCat({"some_extension_type", "some_extension_id",
                    "some_local_input_method_id"})));

  EXPECT_FALSE(extension_ime_util::IsCros1pKorean(base::StrCat(
      {"_comp_ime_", "some_extension_id", "some_local_input_method_id"})));

  EXPECT_FALSE(extension_ime_util::IsCros1pKorean(
      base::StrCat({"_comp_ime_", "jkghodnilhceideoidjikpgommlajknk",
                    "some_local_input_method_id"})));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(
#else
  EXPECT_FALSE(
#endif
      extension_ime_util::IsCros1pKorean(base::StrCat(
          {"_comp_ime_", "jkghodnilhceideoidjikpgommlajknk", "ko-t-i0-und"})));
}

}  // namespace ash
