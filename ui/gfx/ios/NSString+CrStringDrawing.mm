// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/gfx/ios/NSString+CrStringDrawing.h"

#include <stddef.h>

#include <ostream>

#include "base/check.h"
#include "ui/gfx/ios/uikit_util.h"

@implementation NSString (CrStringDrawing)

- (CGRect)cr_boundingRectWithSize:(CGSize)size
                             font:(UIFont*)font {
  NSDictionary* attributes = font ? @{NSFontAttributeName: font} : @{};
  return [self boundingRectWithSize:size
                            options:NSStringDrawingUsesLineFragmentOrigin
                         attributes:attributes
                            context:nil];
}

- (CGSize)cr_boundingSizeWithSize:(CGSize)size
                             font:(UIFont*)font {
  return [self cr_boundingRectWithSize:size font:font].size;
}

- (CGSize)cr_pixelAlignedSizeWithFont:(UIFont*)font {
  DCHECK(font) << "|font| can not be nil; it is used as a NSDictionary value";
  NSDictionary* attributes = @{ NSFontAttributeName : font };
  return ui::AlignSizeToUpperPixel([self sizeWithAttributes:attributes]);
}

- (CGSize)cr_sizeWithFont:(UIFont*)font {
  if (!font)
    return CGSizeZero;
  NSDictionary* attributes = @{ NSFontAttributeName : font };
  CGSize size = [self sizeWithAttributes:attributes];
  return CGSizeMake(ceil(size.width), ceil(size.height));
}

- (NSString*)cr_stringByCuttingToIndex:(NSUInteger)index {
  if (index == 0)
    return @"";
  if (index >= self.length) {
    return self;
  }
  return [[self substringToIndex:(index - 1)] stringByAppendingString:@"…"];
}

- (NSString*)cr_stringByElidingToFitSize:(CGSize)bounds {
  CGSize sizeForGuess = CGSizeMake(bounds.width, CGFLOAT_MAX);
  // Use binary search on the string's length.
  size_t lo = 0;
  size_t hi = self.length;
  size_t guess = 0;
  for (guess = (lo + hi) / 2; lo < hi; guess = (lo + hi) / 2) {
    NSString* tempString = [self cr_stringByCuttingToIndex:guess];
    UIFont* font = [UIFont systemFontOfSize:UIFont.labelFontSize];
    CGSize sizeGuess =
        [tempString cr_boundingSizeWithSize:sizeForGuess font:font];
    if (sizeGuess.height > bounds.height) {
      hi = guess - 1;
      if (hi < lo)
        hi = lo;
    } else {
      lo = guess + 1;
    }
  }
  return [self cr_stringByCuttingToIndex:lo];
}

@end
