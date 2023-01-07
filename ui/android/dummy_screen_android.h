// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_DUMMY_SCREEN_ANDROID_H_
#define UI_ANDROID_DUMMY_SCREEN_ANDROID_H_

#include "ui/android/ui_android_export.h"

namespace display {
class Screen;
}

namespace ui {

// Creates Screen containing one dummy Display that does not
// communicate with Java layer.
UI_ANDROID_EXPORT display::Screen* CreateDummyScreenAndroid();

}  // namespace display

#endif  // UI_ANDROID_DUMMY_SCREEN_ANDROID_H_
