// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ios/ui_image_test_utils.h"

#include "base/apple/scoped_cftyperef.h"

namespace ui::test::uiimage_utils {

UIImage* UIImageWithSizeAndSolidColor(CGSize const& size, UIColor* color) {
  return UIImageWithSizeAndSolidColorAndScale(size, color, /*scale=*/1.0);
}

UIImage* UIImageWithSizeAndSolidColorAndScale(CGSize const& size,
                                              UIColor* color,
                                              CGFloat scale) {
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = scale;
  format.opaque = YES;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:size format:format];

  return
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* ui_context) {
        CGContextRef context = ui_context.CGContext;
        CGContextSetFillColorWithColor(context, [color CGColor]);
        CGContextFillRect(context, CGRectMake(0, 0, size.width, size.height));
      }];
}

bool UIImagesAreEqual(UIImage* image_1, UIImage* image_2) {
  if (image_1 == image_2)
    return true;

  if (!CGSizeEqualToSize(image_1.size, image_2.size))
    return false;

  base::apple::ScopedCFTypeRef<CFDataRef> data_ref_1(
      CGDataProviderCopyData(CGImageGetDataProvider(image_1.CGImage)));
  base::apple::ScopedCFTypeRef<CFDataRef> data_ref_2(
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
