// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_util.h"

#import <UIKit/UIKit.h>

#include <optional>

#include "base/apple/foundation_util.h"
#include "base/containers/to_vector.h"
#include "build/blink_buildflags.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/resize_image_dimensions.h"

#if BUILDFLAG(USE_BLINK)
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#endif  // BUILDFLAG(USE_BLINK)

namespace {
// Copied from GTMUIImage+Resize in //third_party/google_toolbox_for_mac to
// avoid depending on other GTM* classes unnecessarily.
UIImage* ResizeUIImage(UIImage* image,
                       CGSize target_size,
                       BOOL preserve_aspect_ratio,
                       BOOL trim_to_fit) {
  CGSize imageSize = image.size;
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

std::optional<std::vector<uint8_t>> JPEG1xEncodedDataFromImage(
    const Image& image,
    int quality) {
  NSData* data = UIImageJPEGRepresentation(image.ToUIImage(), quality / 100.0);

  if (data.length == 0) {
    return std::nullopt;
  }

  return base::ToVector(base::apple::NSDataToSpan(data));
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

#if BUILDFLAG(USE_BLINK)
Image ImageFrom1xJPEGEncodedData(base::span<const uint8_t> input) {
  std::unique_ptr<SkBitmap> bitmap(gfx::JPEGCodec::Decode(input));
  if (bitmap.get()) {
    return Image::CreateFrom1xBitmap(*bitmap);
  }

  return Image();
}
#endif  // BUILDFLAG(USE_BLINK)

Image ResizedImage(const Image& image, const gfx::Size& size) {
  UIImage* ui_image =
      ResizeUIImage(image.ToUIImage(), size.ToCGSize(),
       /*preserve_aspect_ratio=*/NO, /*trim_to_fit=*/NO);
  return Image(ui_image);
}
}  // end namespace gfx
