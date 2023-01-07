// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IOS_NSSTRING_CRSTRINGDRAWING_H_
#define UI_GFX_IOS_NSSTRING_CRSTRINGDRAWING_H_

#import <UIKit/UIKit.h>

@interface NSString (CrStringDrawing)

// Calculates and returns the bounding rect for the receiver drawn using the
// given size and font.
// This method is implemented as a wrapper around
// |boundingRectWithSize:options:attributes:context:| using the following values
// for the parameters:
//  - size: the provided |size|
//  - options: NSStringDrawingUsesLineFragmentOrigin
//  - attributes: a NSDictionary with the provided |font|
//  - context: nil.
//
// Note that the rect returned may contain fractional values.
- (CGRect)cr_boundingRectWithSize:(CGSize)size
                             font:(UIFont*)font;

// Convenience wrapper to just return the size of |boundingRectWithSize:font:|.
//
// Note that the size returned may contain fractional values.
- (CGSize)cr_boundingSizeWithSize:(CGSize)size
                             font:(UIFont*)font;

// Returns the size of the string if it were to be rendered with the specified
// font on a single line. The width and height of the CGSize returned are
// pixel-aligned.
//
// This method is a convenience wrapper around sizeWithAttributes: to avoid
// boilerplate required to put |font| in a dictionary of attributes. Do not pass
// nil into this method.
- (CGSize)cr_pixelAlignedSizeWithFont:(UIFont*)font;

// Deprecated: Use cr_pixelAlignedSizeWithFont: or sizeWithAttributes:
// Provides a drop-in replacement for sizeWithFont:, which was deprecated in iOS
// 7 in favor of -sizeWithAttributes:. Specifically, this method will return
// CGSizeZero if |font| is nil, and the width and height returned are rounded up
// to integer values.
// TODO(lliabraa): This method was added to ease the transition off of the
// deprecated sizeWithFont: method. New call sites should not be added and
// existing call sites should be audited to determine the correct behavior for
// nil |font| and rounding, then replaced with cr_pixelAlignedSizeWithFont: or
// sizeWithAttributes: (crbug.com/364419).
- (CGSize)cr_sizeWithFont:(UIFont*)font;

// If |index| is 0, returns an empty string.
// If |index| is >= than self.length, returns self.
// Otherwise, returns string cut to have |index| characters with an
// ellipsis at the end.
- (NSString*)cr_stringByCuttingToIndex:(NSUInteger)index;

// Returns an elided version of string that fits in |bounds|.
// System font of Label size is used for determining the string drawing size.
- (NSString*)cr_stringByElidingToFitSize:(CGSize)bounds;

@end

#endif  // UI_GFX_IOS_NSSTRING_CRSTRINGDRAWING_H_
