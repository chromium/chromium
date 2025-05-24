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

@end
