// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/touch_bar_util.h"

#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"

namespace ui {

NSString* GetTouchBarId(NSString* touch_bar_id) {
  NSString* chrome_bundle_id =
      base::SysUTF8ToNSString(base::apple::BaseBundleID());
  return [NSString stringWithFormat:@"%@.%@", chrome_bundle_id, touch_bar_id];
}

NSString* GetTouchBarItemId(NSString* touch_bar_id, NSString* item_id) {
  return [NSString
      stringWithFormat:@"%@-%@", GetTouchBarId(touch_bar_id), item_id];
}

}  // namespace ui
