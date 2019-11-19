// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"
#include "ui/gfx/image/resize_image_dimensions.h"

#include "base/logging.h"

namespace {
// Copied from GTMUIImage+Resize in //third_party/google_toolbox_for_mac to
// avoid depending on other GTM* classes unnecessarily.
UIImage* ResizeUIImage(UIImage* image,
                       CGSize target_size,
                       BOOL preserve_aspect_ratio,
                       BOOL trim_to_fit) {
  CGSize imageSize = [image size];
  if (imageSize.height < 1 || imageSize.width < 1) {
    return nil;
  }
  if (target_size.height < 1 || target_size.width < 1) {
    return nil;
  }
  CGFloat aspectRatio = imageSize.width / imageSize.height;
  CGFloat targetAspectRatio = target_size.width / target_size.height;
  CGRect projectTo = CGRectZero;
  if (preserve_aspect_ratio) {
    if (trim_to_fit) {
      // Scale and clip image so that the aspect ratio is preserved and the
      // target size is filled.
      if (targetAspectRatio < aspectRatio) {
        // clip the x-axis.
        projectTo.size.width = target_size.height * aspectRatio;
        projectTo.size.height = target_size.height;
        projectTo.origin.x = (target_size.width - projectTo.size.width) / 2;
        projectTo.origin.y = 0;
      } else {
        // clip the y-axis.
        projectTo.size.width = target_size.width;
        projectTo.size.height = target_size.width / aspectRatio;
        projectTo.origin.x = 0;
        projectTo.origin.y = (target_size.height - projectTo.size.height) / 2;
      }
    } else {
      // Scale image to ensure it fits inside the specified target_size.
      if (targetAspectRatio < aspectRatio) {
        // target is less wide than the original.
        projectTo.size.width = target_size.width;
        projectTo.size.height = projectTo.size.width / aspectRatio;
        target_size = projectTo.size;
      } else {
        // target is wider than the original.
        projectTo.size.height = target_size.height;
        projectTo.size.width = projectTo.size.height * aspectRatio;
        target_size = projectTo.size;
      }
    }  // if (clip)
  } else {
    // Don't preserve the aspect ratio.
    projectTo.size = target_size;
  }

  projectTo = CGRectIntegral(projectTo);
  // There's no CGSizeIntegral, so we fake our own.
  CGRect integralRect = CGRectZero;
  integralRect.size = target_size;
  target_size = CGRectIntegral(integralRect).size;

  // Resize photo. Use UIImage drawing methods because they respect
  // UIImageOrientation as opposed to CGContextDrawImage().
  UIGraphicsBeginImageContext(target_size);
  [image drawInRect:projectTo];
  UIImage* resizedPhoto = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return resizedPhoto;
}
}  // namespace

namespace gfx {

bool JPEG1xEncodedDataFromImage(const Image& image,
                                int quality,
                                std::vector<unsigned char>* dst) {
  NSData* data = UIImageJPEGRepresentation(image.ToUIImage(), quality / 100.0);

  if ([data length] == 0)
    return false;

  dst->resize([data length]);
  [data getBytes:&dst->at(0) length:[data length]];
  return true;
}

Image ResizedImageForSearchByImage(const Image& image) {
  if (image.IsEmpty()) {
    return image;
  }
  UIImage* ui_image = image.ToUIImage();

  if (ui_image &&
      ui_image.size.height * ui_image.size.width > kSearchByImageMaxImageArea &&
      (ui_image.size.width > kSearchByImageMaxImageWidth ||
       ui_image.size.height > kSearchByImageMaxImageHeight)) {
    CGSize new_image_size =
        CGSizeMake(kSearchByImageMaxImageWidth, kSearchByImageMaxImageHeight);
    ui_image = ResizeUIImage(ui_image, new_image_size,
                             /*preserve_aspect_ratio=*/YES, /*trim_to_fit=*/NO);
  }
  return Image(ui_image);
}

}  // end namespace gfx
