// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ios/ui_image_test_utils.h"

#include "base/apple/foundation_util.h"
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
  if (image_1 == image_2) {
    return true;
  }

  if (!CGSizeEqualToSize(image_1.size, image_2.size)) {
    return false;
  }

  base::apple::ScopedCFTypeRef<CFDataRef> data_ref_1(
      CGDataProviderCopyData(CGImageGetDataProvider(image_1.CGImage)));
  base::apple::ScopedCFTypeRef<CFDataRef> data_ref_2(
      CGDataProviderCopyData(CGImageGetDataProvider(image_2.CGImage)));

  auto span_1 = base::apple::CFDataToSpan(data_ref_1.get());
  auto span_2 = base::apple::CFDataToSpan(data_ref_2.get());

  return span_1 == span_2;
}

}  // namespace ui::test::uiimage_utils
