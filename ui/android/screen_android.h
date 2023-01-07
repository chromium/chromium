// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_SCREEN_ANDROID_H_
#define UI_ANDROID_SCREEN_ANDROID_H_

#include <jni.h>
#include "ui/android/ui_android_export.h"

namespace ui {

UI_ANDROID_EXPORT void SetScreenAndroid(bool use_display_wide_color_gamut);

}  // namespace display

#endif  // UI_ANDROID_SCREEN_ANDROID_H_
