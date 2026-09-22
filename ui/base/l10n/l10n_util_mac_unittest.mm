// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_mac.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using L10nUtilMacTest = PlatformTest;

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
    std::u16string input16(
        base::SysNSStringToUTF16(UNSAFE_TODO(data[idx]).input));

    NSString* result = l10n_util::FixUpWindowsStyleLabel(input16);
    EXPECT_TRUE(result != nil) << "Fixup Failed, idx = " << idx;

    EXPECT_TRUE([UNSAFE_TODO(data[idx]).output isEqual:result])
        << "For idx " << idx << ", expected '"
        << base::SysNSStringToUTF8(UNSAFE_TODO(data[idx]).output) << "', got '"
        << base::SysNSStringToUTF8(result) << "'";
  }
}
