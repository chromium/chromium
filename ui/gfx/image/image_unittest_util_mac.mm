// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "skia/ext/skia_utils_mac.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace gfx {
namespace test {

SkColor GetPlatformImageColor(PlatformImage image, int x, int y) {
  // AppKit's coordinate system is flipped.
  y = [image size].height - y;

  [image lockFocus];
  NSColor* color = NSReadPixel(NSMakePoint(x, y));
  [image unlockFocus];
  return skia::NSDeviceColorToSkColor(
      [color colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]]);
}

}  // namespace test
}  // namespace gfx
