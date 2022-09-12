// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_ios.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using L10nUtilIOSTest = PlatformTest;

TEST_F(L10nUtilIOSTest, GetDisplayNameForLocale) {
  // Test documented error cases and return values of GetDisplayNameForLocale.
  std::u16string result = l10n_util::GetDisplayNameForLocale("xyz", "en");
  EXPECT_EQ(base::SysNSStringToUTF16(@"xyz"), result);

  result = l10n_util::GetDisplayNameForLocale("Xyz", "en");
  EXPECT_EQ(base::SysNSStringToUTF16(@"xyz"), result);

  result = l10n_util::GetDisplayNameForLocale("Xyz-Xyz", "en");
  EXPECT_EQ(base::SysNSStringToUTF16(@"xyz (XYZ)"), result);

  result = l10n_util::GetDisplayNameForLocale("Xyz-", "en");
  EXPECT_EQ(base::SysNSStringToUTF16(@"xyz"), result);

  result = l10n_util::GetDisplayNameForLocale("xyz-xyz-xyz", "en");
  EXPECT_EQ(base::SysNSStringToUTF16(@"xyz-xyz (XYZ)"), result);

  result = l10n_util::GetDisplayNameForLocale("", "en");
  EXPECT_EQ(base::SysNSStringToUTF16(@""), result);
}
