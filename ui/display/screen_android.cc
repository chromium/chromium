// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#include "base/notreached.h"

namespace display {

// static
gfx::NativeWindow Screen::GetWindowForView(gfx::NativeView view) {
  // Android cannot use this method to convert |NativeView| to |NativeWindow|
  // since it causes cyclic dependency. |GetDisplayNearestView| should be
  // overriden directly.
  NOTREACHED() << "Wrong screen instance is used. Make sure to use the correct "
                  "Screen instance that has proper implementation of "
                  "|GetDisplayNearestView| for Android.";
  return nullptr;
}

}  // namespace display
