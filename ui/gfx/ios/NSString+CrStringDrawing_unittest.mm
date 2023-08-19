// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

#import <UIKit/UIKit.h>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

typedef PlatformTest NSStringCrStringDrawing;

// These tests verify that the category methods return the same values as the
// deprecated methods, so ignore warnings about using deprecated methods.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Verifies that |cr_boundingSizeWithSize| returns the same size as the
// deprecated |sizeWithFont:constrainedToSize| for most values.
// Note that the methods return different values in a few cases (so they are not
// included in the test cases):
//  - the constrained size.width is less than a character.
//  - the constrained size.height is less than the font height.
//  - the string is empty.
TEST_F(NSStringCrStringDrawing, BoundingSizeWithSize) {
  NSArray* fonts = @[
    [UIFont systemFontOfSize:16],
    [UIFont boldSystemFontOfSize:10],
    [UIFont fontWithName:@"Helvetica" size:12.0],
  ];
  NSArray* strings = @[
    @"Test",
    @"multi word test",
    @"你好",
    @"★ This is a test string that is very long.",
  ];
  NSArray* sizes = @[
    [NSValue valueWithCGSize:CGSizeMake(20, 100)],
    [NSValue valueWithCGSize:CGSizeMake(100, 100)],
    [NSValue valueWithCGSize:CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX)],
  ];
  for (UIFont* font in fonts) {
    for (NSString* string in strings) {
      for (NSValue* sizeValue in sizes) {
        CGSize test_size = [sizeValue CGSizeValue];
        std::string test_tag = base::StringPrintf(
            "for string '%s' with font %s and size %s",
            base::SysNSStringToUTF8(string).c_str(),
            base::SysNSStringToUTF8([font description]).c_str(),
            base::SysNSStringToUTF8(NSStringFromCGSize(test_size)).c_str());

        CGSize size_with_font =
            [string sizeWithFont:font constrainedToSize:test_size];
        CGSize bounding_size =
            [string cr_boundingSizeWithSize:test_size font:font];
        EXPECT_EQ(size_with_font.width, bounding_size.width) << test_tag;
        EXPECT_EQ(size_with_font.height, bounding_size.height) << test_tag;
      }
    }
  }
}

TEST_F(NSStringCrStringDrawing, SizeWithFont) {
  NSArray* fonts = @[
    [NSNull null],
    [UIFont systemFontOfSize:16],
    [UIFont boldSystemFontOfSize:10],
    [UIFont fontWithName:@"Helvetica" size:12.0],
  ];
  for (UIFont* __strong font in fonts) {
    if ([font isEqual:[NSNull null]])
      font = nil;
    std::string font_tag = "with font ";
    font_tag.append(
        base::SysNSStringToUTF8(font ? [font description] : @"nil"));
    EXPECT_EQ([@"" sizeWithFont:font].width,
              [@"" cr_sizeWithFont:font].width) << font_tag;
    EXPECT_EQ([@"" sizeWithFont:font].height,
              [@"" cr_sizeWithFont:font].height) << font_tag;
    EXPECT_EQ([@"Test" sizeWithFont:font].width,
              [@"Test" cr_sizeWithFont:font].width) << font_tag;
    EXPECT_EQ([@"Test" sizeWithFont:font].height,
              [@"Test" cr_sizeWithFont:font].height) << font_tag;
    EXPECT_EQ([@"你好" sizeWithFont:font].width,
              [@"你好" cr_sizeWithFont:font].width) << font_tag;
    EXPECT_EQ([@"你好" sizeWithFont:font].height,
              [@"你好" cr_sizeWithFont:font].height) << font_tag;
    NSString* long_string = @"★ This is a test string that is very long.";
    EXPECT_EQ([long_string sizeWithFont:font].width,
              [long_string cr_sizeWithFont:font].width) << font_tag;
    EXPECT_EQ([long_string sizeWithFont:font].height,
              [long_string cr_sizeWithFont:font].height) << font_tag;
  }
}
#pragma clang diagnostic pop  // ignored "-Wdeprecated-declarations"

TEST_F(NSStringCrStringDrawing, PixelAlignedSizeWithFont) {
  NSArray* fonts = @[
    [UIFont systemFontOfSize:16],
    [UIFont boldSystemFontOfSize:10],
    [UIFont fontWithName:@"Helvetica" size:12.0],
  ];
  NSArray* strings = @[
    @"",
    @"Test",
    @"你好",
    @"★ This is a test string that is very long.",
  ];
  for (UIFont* font in fonts) {
    NSDictionary* attributes = @{ NSFontAttributeName : font };

    for (NSString* string in strings) {
      std::string test_tag = base::StringPrintf("for string '%s' with font %s",
          base::SysNSStringToUTF8(string).c_str(),
          base::SysNSStringToUTF8([font description]).c_str());

      CGSize size_with_attributes = [string sizeWithAttributes:attributes];
      CGSize size_with_pixel_aligned =
          [string cr_pixelAlignedSizeWithFont:font];

      // Verify that the pixel_aligned size is always rounded up (i.e. the size
      // returned from sizeWithAttributes: is less than or equal to the pixel-
      // aligned size).
      EXPECT_LE(size_with_attributes.width,
                size_with_pixel_aligned.width) << test_tag;
      EXPECT_LE(size_with_attributes.height,
                size_with_pixel_aligned.height) << test_tag;

      // Verify that the pixel_aligned size is never more than a pixel different
      // than the size returned from sizeWithAttributes:.
      static CGFloat scale = [[UIScreen mainScreen] scale];
      EXPECT_NEAR(size_with_attributes.width * scale,
                  size_with_pixel_aligned.width * scale,
                  0.9999) << test_tag;
      EXPECT_NEAR(size_with_attributes.height * scale,
                  size_with_pixel_aligned.height * scale,
                  0.9999) << test_tag;

      // Verify that the pixel-aligned value is pixel-aligned.
      EXPECT_FLOAT_EQ(roundf(size_with_pixel_aligned.width * scale),
                      size_with_pixel_aligned.width * scale) << test_tag;
      EXPECT_FLOAT_EQ(roundf(size_with_pixel_aligned.height * scale),
                      size_with_pixel_aligned.height * scale) << test_tag;
    }
  }
}

TEST_F(NSStringCrStringDrawing, CutString) {
  EXPECT_NSEQ(@"foo", [@"foo" cr_stringByCuttingToIndex:4]);
  EXPECT_NSEQ(@"bar", [@"bar" cr_stringByCuttingToIndex:3]);
  EXPECT_NSEQ(@"f…", [@"foo" cr_stringByCuttingToIndex:2]);
  EXPECT_NSEQ(@"…", [@"bar" cr_stringByCuttingToIndex:1]);
  EXPECT_NSEQ(@"", [@"foo" cr_stringByCuttingToIndex:0]);
}

TEST_F(NSStringCrStringDrawing, ElideStringToFitInRect) {
  NSString* result =
      [@"lorem ipsum dolores" cr_stringByElidingToFitSize:CGSizeZero];
  EXPECT_NSEQ(@"", result);
  result = [@"lorem ipsum dolores"
      cr_stringByElidingToFitSize:CGSizeMake(1000, 1000)];
  EXPECT_NSEQ(@"lorem ipsum dolores", result);
  result =
      [@"lorem ipsum dolores" cr_stringByElidingToFitSize:CGSizeMake(30, 50)];
  EXPECT_TRUE([@"lorem ipsum dolores" length] > [result length]);
}

}  // namespace
