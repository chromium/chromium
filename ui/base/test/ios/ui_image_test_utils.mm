// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ios/ui_image_test_utils.h"

#include "base/mac/scoped_cftyperef.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ui::test::uiimage_utils {

UIImage* UIImageWithSizeAndSolidColor(CGSize const& size, UIColor* color) {
  return UIImageWithSizeAndSolidColorAndScale(size, color, /*scale=*/1.0);
}

UIImage* UIImageWithSizeAndSolidColorAndScale(CGSize const& size,
                                              UIColor* color,
                                              CGFloat scale) {
  UIGraphicsBeginImageContextWithOptions(size, /*opaque=*/YES, scale);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSetFillColorWithColor(context, [color CGColor]);
  CGContextFillRect(context, CGRectMake(0, 0, size.width, size.height));
  UIImage* image_with_solid_color = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return image_with_solid_color;
}

bool UIImagesAreEqual(UIImage* image_1, UIImage* image_2) {
  if (image_1 == image_2)
    return true;

  if (!CGSizeEqualToSize(image_1.size, image_2.size))
    return false;

  base::ScopedCFTypeRef<CFDataRef> data_ref_1(
      CGDataProviderCopyData(CGImageGetDataProvider(image_1.CGImage)));
  base::ScopedCFTypeRef<CFDataRef> data_ref_2(
      CGDataProviderCopyData(CGImageGetDataProvider(image_2.CGImage)));
  CFIndex length_1 = CFDataGetLength(data_ref_1);
  CFIndex length_2 = CFDataGetLength(data_ref_2);
  if (length_1 != length_2) {
    return false;
  }
  const UInt8* ptr_1 = CFDataGetBytePtr(data_ref_1);
  const UInt8* ptr_2 = CFDataGetBytePtr(data_ref_2);

  // memcmp returns 0 if length is 0.
  return memcmp(ptr_1, ptr_2, length_1) == 0;
}

}  // namespace ui::test::uiimage_utils
