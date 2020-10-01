// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/extension_ime_util.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

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
  EXPECT_EQ("",
            extension_ime_util::GetExtensionIDFromInputMethodID("mozc"));
  EXPECT_EQ("12345678901234567890123456789012",
            extension_ime_util::GetExtensionIDFromInputMethodID(
              extension_ime_util::GetInputMethodID(
                  "12345678901234567890123456789012",
                  "mozc")));
  EXPECT_EQ("12345678901234567890123456789012",
            extension_ime_util::GetExtensionIDFromInputMethodID(
              extension_ime_util::GetComponentInputMethodID(
                  "12345678901234567890123456789012",
                  "mozc")));
}

TEST(ExtensionIMEUtilTest, IsExtensionIMETest) {
  EXPECT_TRUE(extension_ime_util::IsExtensionIME(
      extension_ime_util::GetInputMethodID(
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
      extension_ime_util::GetInputMethodID(
          "abcde_xxxxxxxxxxxxxxxxxxxxxxxxxx", "12345")));
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

}  // namespace chromeos
