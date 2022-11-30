// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_VIEW_CONFIGURATION_H_
#define UI_GFX_ANDROID_VIEW_CONFIGURATION_H_

#include <jni.h>

#include "ui/gfx/gfx_export.h"

namespace gfx {

// Provides access to Android's ViewConfiguration for gesture-related constants.
// Note: All methods may be safely called from any thread.
class GFX_EXPORT ViewConfiguration {
 public:
  static int GetDoubleTapTimeoutInMs();
  static int GetLongPressTimeoutInMs();
  static int GetTapTimeoutInMs();

  static int GetMaximumFlingVelocityInDipsPerSecond();
  static int GetMinimumFlingVelocityInDipsPerSecond();

  static int GetTouchSlopInDips();
  static int GetDoubleTapSlopInDips();

  static int GetMinScalingSpanInDips();
};

}  // namespace gfx

#endif  // UI_GFX_ANDROID_VIEW_CONFIGURATION_H_
