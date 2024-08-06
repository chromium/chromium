// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image.h"

namespace ui {

namespace {

base::FilePath GetResourcesPakFilePath(NSString* name, NSString* mac_locale) {
  NSString *resource_path;
  if (mac_locale.length) {
    resource_path = [base::apple::FrameworkBundle() pathForResource:name
                                                             ofType:@"pak"
                                                        inDirectory:@""
                                                    forLocalization:mac_locale];
  } else {
    resource_path = [base::apple::FrameworkBundle() pathForResource:name
                                                             ofType:@"pak"];
  }
  if (!resource_path) {
    // Return just the name of the pak file.
    return base::FilePath(base::SysNSStringToUTF8(name) + ".pak");
  }
  return base::FilePath([resource_path fileSystemRepresentation]);
}

}  // namespace

void ResourceBundle::LoadCommonResources() {
  if (IsScaleFactorSupported(k100Percent)) {
    AddDataPackFromPath(GetResourcesPakFilePath(@"chrome_100_percent", nil),
                        k100Percent);
  }

  if (IsScaleFactorSupported(k200Percent)) {
    AddDataPackFromPath(GetResourcesPakFilePath(@"chrome_200_percent", nil),
                        k200Percent);
  }

  if (IsScaleFactorSupported(k300Percent)) {
    AddDataPackFromPath(GetResourcesPakFilePath(@"chrome_300_percent", nil),
                        k300Percent);
  }
}

// static
base::FilePath ResourceBundle::GetLocaleFilePath(
    const std::string& app_locale) {
  NSString* mac_locale = base::SysUTF8ToNSString(app_locale);

  // iOS uses "_" instead of "-", so swap to get a iOS-style value.
  mac_locale = [mac_locale stringByReplacingOccurrencesOfString:@"-"
                                                     withString:@"_"];

  // On disk, the "en_US" resources are just "en" (http://crbug.com/25578).
  if ([mac_locale isEqual:@"en_US"])
    mac_locale = @"en";

  base::FilePath locale_file_path =
      GetResourcesPakFilePath(@"locale", mac_locale);

  if (HasSharedInstance() && GetSharedInstance().delegate_) {
    locale_file_path = GetSharedInstance().delegate_->GetPathForLocalePack(
        locale_file_path, app_locale);
  }

  // Don't try to load from paths that are not absolute.
  return locale_file_path.IsAbsolute() ? locale_file_path : base::FilePath();
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check to see if the image is already in the cache.
  ImageMap::iterator found = images_.find(resource_id);
  if (found != images_.end()) {
    return found->second;
  }

  gfx::Image image;
  if (delegate_)
    image = delegate_->GetNativeImageNamed(resource_id);

  if (image.IsEmpty()) {
    // Load the raw data from the resource pack at the current supported scale
    // factor.  This code assumes that only one of the possible scale factors is
    // supported at runtime, based on the device resolution.
    ui::ResourceScaleFactor scale_factor = GetMaxResourceScaleFactor();

    scoped_refptr<base::RefCountedMemory> data(
        LoadDataResourceBytesForScale(resource_id, scale_factor));

    if (!data.get()) {
      LOG(WARNING) << "Unable to load image with id " << resource_id;
      return GetEmptyImage();
    }

    // Create a data object from the raw bytes.
    NSData* ns_data = [[NSData alloc] initWithBytes:data->front()
                                             length:data->size()];

    bool is_fallback = PNGContainsFallbackMarker(*data);
    // Create the image from the data.
    CGFloat target_scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    CGFloat source_scale = is_fallback ? 1.0 : target_scale;
    UIImage* ui_image = [[UIImage alloc] initWithData:ns_data
                                                scale:source_scale];

    // If the image is a 1x fallback, scale it up to a full-size representation.
    if (is_fallback) {
      CGSize source_size = ui_image.size;
      CGSize target_size = CGSizeMake(source_size.width * target_scale,
                                      source_size.height * target_scale);
      base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
          CGColorSpaceCreateDeviceRGB());
      base::apple::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
          /*data=*/nullptr, target_size.width, target_size.height, 8,
          target_size.width * 4, color_space.get(),
          kCGImageAlphaPremultipliedFirst |
              static_cast<CGImageAlphaInfo>(kCGBitmapByteOrder32Host)));

      CGRect target_rect = CGRectMake(0, 0,
                                      target_size.width, target_size.height);
      CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
      CGContextDrawImage(context.get(), target_rect, ui_image.CGImage);

      base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
          CGBitmapContextCreateImage(context.get()));
      ui_image = [[UIImage alloc] initWithCGImage:cg_image.get()
                                            scale:target_scale
                                      orientation:UIImageOrientationUp];
    }

    CHECK(ui_image) << "Unable to load image with id " << resource_id;
    image = gfx::Image(ui_image);
  }

  auto inserted = images_.emplace(resource_id, image);
  DCHECK(inserted.second);
  return inserted.first->second;
}

}  // namespace ui
