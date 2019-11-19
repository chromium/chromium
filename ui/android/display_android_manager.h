// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_DISPLAY_ANDROID_MANAGER_H_
#define UI_ANDROID_DISPLAY_ANDROID_MANAGER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

class WindowAndroid;

class DisplayAndroidManager : public display::ScreenBase {
 public:
  ~DisplayAndroidManager() override;

  // Screen interface.

  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;
  display::Display GetDisplayNearestView(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;

  // Methods called from Java.

  void UpdateDisplay(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jobject,
                     jint sdkDisplayId,
                     jint width,
                     jint height,
                     jfloat dipScale,
                     jint rotationDegrees,
                     jint bitsPerPixel,
                     jint bitsPerComponent,
                     jboolean isWideColorGamut);
  void RemoveDisplay(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jobject,
                     jint sdkDisplayId);
  void SetPrimaryDisplayId(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobject,
                           jint sdkDisplayId);

 private:
  friend class WindowAndroid;
  friend void SetScreenAndroid(bool use_display_wide_color_gamut);
  explicit DisplayAndroidManager(bool use_display_wide_color_gamut);

  static void DoUpdateDisplay(display::Display* display,
                              gfx::Size size_in_pixels,
                              float dipScale,
                              int rotationDegrees,
                              int bitsPerPixel,
                              int bitsPerComponent,
                              bool isWideColorGamut);

  const bool use_display_wide_color_gamut_;
  int primary_display_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DisplayAndroidManager);
};

}  // namespace ui

#endif  // UI_ANDROID_DISPLAY_ANDROID_MANAGER_H_
