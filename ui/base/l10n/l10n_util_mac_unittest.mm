// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_mac.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include <array>

#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using L10nUtilMacTest = PlatformTest;

TEST_F(L10nUtilMacTest, FixUpWindowsStyleLabel) {
  struct TestData {
    NSString* input;
    NSString* output;
  };

  const auto data = std::to_array<TestData>({
      {@"", @""},
      {@"nothing", @"nothing"},
      {@"foo &bar", @"foo bar"},
      {@"foo &&bar", @"foo &bar"},
      {@"foo &&&bar", @"foo &bar"},
      {@"&foo &&bar", @"foo &bar"},
      {@"&foo &bar", @"foo bar"},
      {@"foo bar.", @"foo bar."},
      {@"foo bar..", @"foo bar.."},
      {@"foo bar...", @"foo bar\u2026"},
      {@"foo.bar", @"foo.bar"},
      {@"foo..bar", @"foo..bar"},
      {@"foo...bar", @"foo\u2026bar"},
      {@"foo...bar...", @"foo\u2026bar\u2026"},
      {@"foo(&b)", @"foo"},
      {@"foo(&b)...", @"foo\u2026"},
      {@"(&b)foo", @"foo"},
  });
  for (const auto& [input, output] : data) {
    std::u16string input16(base::SysNSStringToUTF16(input));

    NSString* result = l10n_util::FixUpWindowsStyleLabel(input16);
    EXPECT_TRUE(result != nil);

    EXPECT_TRUE([output isEqual:result])
        << "Expected '" << base::SysNSStringToUTF8(output) << "', got '"
        << base::SysNSStringToUTF8(result) << "'";
  }
}
