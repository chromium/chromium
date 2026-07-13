// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/gfx/ios/uikit_util.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include <array>

#include "testing/platform_test.h"

namespace {

typedef PlatformTest UIKitUtilTest;

TEST_F(UIKitUtilTest, AlignValueToUpperPixel) {
   CGFloat scale = [[UIScreen mainScreen] scale];
   // Pick a few interesting values: already aligned, aligned on retina, and
   // some unaligned values that would round differently. Ensure that all are
   // "integer" values within <1 of the original value in the scaled space.
   const CGFloat test_values[] = {10.0, 55.5, 3.1, 2.9};
   const CGFloat kMaxAlignDelta = 0.9999;
   for (CGFloat val : test_values) {
     CGFloat aligned = ui::AlignValueToUpperPixel(val);
     EXPECT_FLOAT_EQ(aligned * scale, floor(aligned * scale));
     EXPECT_NEAR(aligned * scale, val * scale, kMaxAlignDelta);
   }
}

TEST_F(UIKitUtilTest, AlignSizeToUpperPixel) {
  CGFloat scale = [[UIScreen mainScreen] scale];
  // Pick a few interesting values: already aligned, aligned on retina, and
  // some unaligned values that would round differently. Ensure that all are
  // "integer" values within <1 of the original value in the scaled space.
  const auto test_values = std::to_array<CGFloat>({10.0, 55.5, 3.1, 2.9});
  const CGFloat kMaxAlignDelta = 0.9999;
  for (size_t i = 0; i < test_values.size(); ++i) {
    const CGFloat width = test_values[i];
    const CGFloat height = test_values[(i + 1) % test_values.size()];
    CGSize alignedSize = ui::AlignSizeToUpperPixel(CGSizeMake(width, height));
    EXPECT_FLOAT_EQ(floor(alignedSize.width * scale),
                    alignedSize.width * scale);
    EXPECT_FLOAT_EQ(floor(alignedSize.height * scale),
                    alignedSize.height * scale);
    EXPECT_NEAR(width * scale, alignedSize.width * scale, kMaxAlignDelta);
    EXPECT_NEAR(height * scale, alignedSize.height * scale, kMaxAlignDelta);
  }
}

}  // namespace
