// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#import <AppKit/AppKit.h>
#include <stddef.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image.h"

namespace ui {

namespace {

base::FilePath GetResourcesPakFilePath(NSString* name, NSString* mac_locale) {
  NSString *resource_path;
  // Some of the helper processes need to be able to fetch resources
  // (chrome_main.cc: SubprocessNeedsResourceBundle()). Fetch the same locale
  // as the already-running browser instead of using what NSBundle might pick
  // based on values at helper launch time.
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
    // Return just the name of the pack file.
    return base::FilePath(base::SysNSStringToUTF8(name) + ".pak");
  }

  return base::apple::NSStringToFilePath(resource_path);
}

}  // namespace

void ResourceBundle::LoadCommonResources() {
  AddDataPackFromPath(GetResourcesPakFilePath(@"chrome_100_percent", nil),
                      k100Percent);

  // On Mac we load 1x and 2x resources and we let the UI framework decide
  // which one to use.
  if (IsScaleFactorSupported(k200Percent)) {
    AddDataPackFromPath(GetResourcesPakFilePath(@"chrome_200_percent", nil),
                        k200Percent);
  }
}

// static
base::FilePath ResourceBundle::GetLocaleFilePath(
    const std::string& app_locale) {
  NSString* mac_locale = base::SysUTF8ToNSString(app_locale);

  // macOS uses "_" instead of "-", so swap to get a Mac-style value.
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
  if (auto found = images_.find(resource_id); found != images_.end())
    return found->second;

  gfx::Image image;
  if (delegate_)
    image = delegate_->GetNativeImageNamed(resource_id);

  if (image.IsEmpty()) {
    NSImage* ns_image;
    for (const auto& resource_handle : resource_handles_) {
      scoped_refptr<base::RefCountedStaticMemory> data(
          resource_handle->GetStaticMemory(
              base::checked_cast<uint16_t>(resource_id)));
      if (!data.get())
        continue;

      NSData* ns_data = [[NSData alloc] initWithBytes:data->front()
                                               length:data->size()];
      if (!ns_image) {
        ns_image = [[NSImage alloc] initWithData:ns_data];
      } else {
        NSImageRep* image_rep = [NSBitmapImageRep imageRepWithData:ns_data];
        if (image_rep)
          [ns_image addRepresentation:image_rep];
      }
    }

    CHECK(ns_image) << "Unable to load image with id " << resource_id;

    image = gfx::Image(ns_image);
  }

  auto inserted = images_.emplace(resource_id, image);
  DCHECK(inserted.second);
  return inserted.first->second;
}

}  // namespace ui
