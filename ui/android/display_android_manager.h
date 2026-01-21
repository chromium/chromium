// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_DISPLAY_ANDROID_MANAGER_H_
#define UI_ANDROID_DISPLAY_ANDROID_MANAGER_H_

#include <jni.h>

#include <optional>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "ui/android/ui_android_export.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

class DisplayAndroidManagerTest;
class WindowAndroid;

class UI_ANDROID_EXPORT DisplayAndroidManager : public display::ScreenBase {
 public:
  static bool IsDisplayTopologyAvailable();
  static void SetIsDisplayTopologyAvailableForTesting(bool value);

  DisplayAndroidManager(const DisplayAndroidManager&) = delete;
  DisplayAndroidManager& operator=(const DisplayAndroidManager&) = delete;

  ~DisplayAndroidManager() override = default;

  // display::ScreenBase:
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;
  display::Display GetDisplayNearestView(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  std::optional<float> GetPreferredScaleFactorForView(
      gfx::NativeView view) const override;

  // Methods called from Java.

  void UpdateDisplay(JNIEnv* env,
                     int32_t sdkDisplayId,
                     const base::android::JavaRef<jstring>& label,
                     const base::android::JavaRef<jintArray>& jBounds,
                     const base::android::JavaRef<jintArray>& jWorkArea,
                     int32_t width,
                     int32_t height,
                     float dipScale,
                     float pixelsPerInchX,
                     float pixelsPerInchY,
                     int32_t rotationDegrees,
                     int32_t bitsPerPixel,
                     int32_t bitsPerComponent,
                     bool isWideColorGamut,
                     bool isHdr,
                     float hdrMaxLuminanceRatio,
                     bool isInternal);
  void RemoveDisplay(JNIEnv* env, int32_t sdkDisplayId);
  void SetPrimaryDisplayId(JNIEnv* env, int32_t sdkDisplayId);

  int32_t GetDisplaySdkMatching(JNIEnv* env,
                                int32_t x,
                                int32_t y,
                                int32_t width,
                                int32_t height);

 private:
  friend class WindowAndroid;
  friend class DisplayAndroidManagerTest;

  friend void SetScreenAndroid(bool use_display_wide_color_gamut);

  explicit DisplayAndroidManager(bool use_display_wide_color_gamut);

  static void DoUpdateDisplay(display::Display* display,
                              const std::string& label,
                              const gfx::Rect& bounds,
                              const gfx::Rect& work_area,
                              const gfx::Size& size_in_pixels,
                              float dip_scale,
                              float pixels_per_inch_x,
                              float pixels_per_inch_y,
                              int rotation_degrees,
                              int bits_per_pixel,
                              int bits_per_component,
                              bool is_wide_color_gamut,
                              bool is_hdr,
                              float hdr_max_luminance_ratio);

  const bool use_display_wide_color_gamut_;
  int primary_display_id_ = 0;
};

}  // namespace ui

#endif  // UI_ANDROID_DISPLAY_ANDROID_MANAGER_H_
