// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/appkit_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "ui/base/test/cocoa_helper.h"

namespace ui {

using AppKitUtilsTest = testing::Test;

// Note that Chromium currently only supports plain text for services, but test
// both plain text and RTF to ensure that these functions work in the general
// case, not just the specific case.
TEST_F(AppKitUtilsTest, UTTypeForServicesTypeTest) {
  // nil input must yield a nil output; nil is a valid parameter for the
  // services API calls, and it's easier to early-return in the compatibility
  // shim.
  EXPECT_NSEQ(nil, UTTypeForServicesType(nil));

  // Round-trip actual identifiers.
  EXPECT_NSEQ(UTTypeUTF8PlainText,
              UTTypeForServicesType(UTTypeUTF8PlainText.identifier));
  EXPECT_NSEQ(UTTypeRTF, UTTypeForServicesType(UTTypeRTF.identifier));

  // Technically speaking, the values of these constants are the same as the
  // identifiers above, but in theory these constants are what the services APIs
  // should be passing in.
  EXPECT_NSEQ(UTTypeUTF8PlainText,
              UTTypeForServicesType(NSPasteboardTypeString));
  EXPECT_NSEQ(UTTypeRTF, UTTypeForServicesType(NSPasteboardTypeRTF));

  // And these obsolete values are what are actually passed into the services
  // APIs no matter what Chromium returns.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  EXPECT_NSEQ(UTTypeUTF8PlainText, UTTypeForServicesType(NSStringPboardType));
  EXPECT_NSEQ(UTTypeRTF, UTTypeForServicesType(NSRTFPboardType));
#pragma clang diagnostic pop
}

TEST_F(AppKitUtilsTest, UTTypesForServicesTypeArrayTest) {
  // Verify that individual items are converted.
  NSSet* expected_1 = [NSSet setWithArray:@[ UTTypeUTF8PlainText, UTTypeRTF ]];
  NSSet* actual_1 = UTTypesForServicesTypeArray(
      @[ NSPasteboardTypeString, NSPasteboardTypeRTF ]);
  EXPECT_NSEQ(expected_1, actual_1);

  // Verify that the same type expressed in two different ways is collapsed into
  // just one entry.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  NSSet* expected_2 = [NSSet setWithArray:@[ UTTypeUTF8PlainText ]];
  NSSet* actual_2 = UTTypesForServicesTypeArray(
      @[ NSPasteboardTypeString, NSStringPboardType ]);
  EXPECT_NSEQ(expected_2, actual_2);
#pragma clang diagnostic pop
}

}  // namespace ui
