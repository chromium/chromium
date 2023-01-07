// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IOS_UIKIT_UTIL_H_
#define UI_GFX_IOS_UIKIT_UTIL_H_

#import <UIKit/UIKit.h>

// UI Util containing functions that require UIKit.

namespace ui {

// Returns the closest pixel-aligned value higher than |value|, taking the scale
// factor into account. At a scale of 1, equivalent to ceil().
[[nodiscard]] CGFloat AlignValueToUpperPixel(CGFloat value);

// Returns the size resulting from applying AlignToUpperPixel to both
// components.
[[nodiscard]] CGSize AlignSizeToUpperPixel(CGSize size);

} // namespace ui

#endif  // UI_GFX_IOS_UIKIT_UTIL_H_
