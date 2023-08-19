// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ios/uikit_util.h"

#import <UIKit/UIKit.h>

#include <cmath>

namespace ui {

CGFloat AlignValueToUpperPixel(CGFloat value) {
  CGFloat scale = UIScreen.mainScreen.scale;
  return std::ceil(value * scale) / scale;
}

CGSize AlignSizeToUpperPixel(CGSize size) {
  return CGSizeMake(AlignValueToUpperPixel(size.width),
                    AlignValueToUpperPixel(size.height));
}

} // namespace ui
