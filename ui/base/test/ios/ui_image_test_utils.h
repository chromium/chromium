// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_IOS_UI_IMAGE_TEST_UTILS_H_
#define UI_BASE_TEST_IOS_UI_IMAGE_TEST_UTILS_H_

#import <UIKit/UIKit.h>

namespace ui::test::uiimage_utils {

// Returns a new UIImage of size |size| with a solid color of |color|. This
// is the same as calling UIImageWithSizeAndSolidColorAndScale with a scale
// of 1.0.
UIImage* UIImageWithSizeAndSolidColor(CGSize const& size, UIColor* color);

// Returns a new UIImage of size |size| with a solid color of |color|
// at scale |scale|.
UIImage* UIImageWithSizeAndSolidColorAndScale(CGSize const& size,
                                              UIColor* color,
                                              CGFloat scale);

// Disclaimer, this is a testing function with plenty of limitations:
// Requires the UIImages to be backed by a CGImage, ignores colorspace, may
// return false negatives, not efficient, and probably other things.
//
// Returns whether the pixels in |image_1| are equal to the pixels in
// |image_2|.
// This is unlike UIImage's |isEqual:| method which only returns YES if the
// memory backing the images is the same (see Apple's response to
// radar://30188145).
bool UIImagesAreEqual(UIImage* image_1, UIImage* image_2);

}  // namespace ui::test::uiimage_utils

#endif  // UI_BASE_TEST_IOS_UI_IMAGE_TEST_UTILS_H_
