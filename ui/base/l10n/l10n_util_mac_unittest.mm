// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_mac.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest L10nUtilMacTest;

TEST_F(L10nUtilMacTest, FixUpWindowsStyleLabel) {
  struct TestData {
    NSString* input;
    NSString* output;
  };

  TestData data[] = {
    { @"", @"" },
    { @"nothing", @"nothing" },
    { @"foo &bar", @"foo bar" },
    { @"foo &&bar", @"foo &bar" },
    { @"foo &&&bar", @"foo &bar" },
    { @"&foo &&bar", @"foo &bar" },
    { @"&foo &bar", @"foo bar" },
    { @"foo bar.", @"foo bar." },
    { @"foo bar..", @"foo bar.." },
    { @"foo bar...", @"foo bar\u2026" },
    { @"foo.bar", @"foo.bar" },
    { @"foo..bar", @"foo..bar" },
    { @"foo...bar", @"foo\u2026bar" },
    { @"foo...bar...", @"foo\u2026bar\u2026" },
    { @"foo(&b)", @"foo" },
    { @"foo(&b)...", @"foo\u2026" },
    { @"(&b)foo", @"foo" },
  };
  for (size_t idx = 0; idx < std::size(data); ++idx) {
    std::u16string input16(base::SysNSStringToUTF16(data[idx].input));

    NSString* result = l10n_util::FixUpWindowsStyleLabel(input16);
    EXPECT_TRUE(result != nil) << "Fixup Failed, idx = " << idx;

    EXPECT_TRUE([data[idx].output isEqual:result])
        << "For idx " << idx << ", expected '" << [data[idx].output UTF8String]
        << "', got '" << [result UTF8String] << "'";
  }
}

TEST_F(L10nUtilMacTest, GetDisplayNameForLocale) {
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
