// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/display_android_manager.h"

#include <jni.h>
#include <initializer_list>
#include <map>

#include "base/android/jni_android.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/viz_utils.h"
#include "ui/android/screen_android.h"
#include "ui/android/ui_android_jni_headers/DisplayAndroidManager_jni.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"
#include "ui/gfx/icc_profile.h"

namespace ui {

using base::android::AttachCurrentThread;
using display::Display;
using display::DisplayList;

void SetScreenAndroid(bool use_display_wide_color_gamut) {
  TRACE_EVENT0("startup", "SetScreenAndroid");
  // Do not override existing Screen.
  DCHECK_EQ(display::Screen::GetScreen(), nullptr);

  DisplayAndroidManager* manager =
      new DisplayAndroidManager(use_display_wide_color_gamut);
  display::Screen::SetScreenInstance(manager);

  JNIEnv* env = AttachCurrentThread();
  Java_DisplayAndroidManager_onNativeSideCreated(env, (jlong)manager);
}

DisplayAndroidManager::DisplayAndroidManager(bool use_display_wide_color_gamut)
    : use_display_wide_color_gamut_(use_display_wide_color_gamut) {}

DisplayAndroidManager::~DisplayAndroidManager() {}

// Screen interface.

Display DisplayAndroidManager::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  if (window) {
    DisplayList::Displays::const_iterator it =
        display_list().FindDisplayById(window->display_id());
    if (it != display_list().displays().end()) {
      return *it;
    }
  }
  return GetPrimaryDisplay();
}

Display DisplayAndroidManager::GetDisplayNearestView(
    gfx::NativeView view) const {
  return GetDisplayNearestWindow(view ? view->GetWindowAndroid() : nullptr);
}

// There is no notion of relative display positions on Android.
Display DisplayAndroidManager::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

// There is no notion of relative display positions on Android.
Display DisplayAndroidManager::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

void DisplayAndroidManager::DoUpdateDisplay(display::Display* display,
                                            gfx::Size size_in_pixels,
                                            float dipScale,
                                            int rotationDegrees,
                                            int bitsPerPixel,
                                            int bitsPerComponent,
                                            bool isWideColorGamut) {
  if (!Display::HasForceDeviceScaleFactor())
    display->set_device_scale_factor(dipScale);

  // TODO: Low-end devices should specify RGB_565 as the buffer format for
  // opaque content.
  if (isWideColorGamut) {
    gfx::DisplayColorSpaces display_color_spaces{
        gfx::ColorSpace::CreateDisplayP3D65(), gfx::BufferFormat::RGBA_8888};
    if (features::IsDynamicColorGamutEnabled()) {
      auto srgb = gfx::ColorSpace::CreateSRGB();
      for (auto needs_alpha : {true, false}) {
        display_color_spaces.SetOutputColorSpaceAndBufferFormat(
            gfx::ContentColorUsage::kSRGB, needs_alpha, srgb,
            gfx::BufferFormat::RGBA_8888);
      }
    }
    display->set_color_spaces(display_color_spaces);
  } else {
    display->set_color_spaces(gfx::DisplayColorSpaces(
        gfx::ColorSpace::CreateSRGB(), gfx::BufferFormat::RGBA_8888));
  }

  display->set_size_in_pixels(size_in_pixels);
  display->SetRotationAsDegree(rotationDegrees);
  DCHECK_EQ(rotationDegrees, display->RotationAsDegree());
  DCHECK_EQ(rotationDegrees, display->PanelRotationAsDegree());
  display->set_color_depth(bitsPerPixel);
  display->set_depth_per_component(bitsPerComponent);
  display->set_is_monochrome(bitsPerComponent == 0);
}

// Methods called from Java

void DisplayAndroidManager::UpdateDisplay(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobject,
    jint sdkDisplayId,
    jint width,
    jint height,
    jfloat dipScale,
    jint rotationDegrees,
    jint bitsPerPixel,
    jint bitsPerComponent,
    jboolean isWideColorGamut) {
  gfx::Rect bounds_in_pixels = gfx::Rect(width, height);
  const gfx::Rect bounds_in_dip = gfx::Rect(
      gfx::ScaleToCeiledSize(bounds_in_pixels.size(), 1.0f / dipScale));

  display::Display display(sdkDisplayId, bounds_in_dip);
  DoUpdateDisplay(&display, bounds_in_pixels.size(), dipScale, rotationDegrees,
                  bitsPerPixel, bitsPerComponent,
                  isWideColorGamut && use_display_wide_color_gamut_);
  ProcessDisplayChanged(display, sdkDisplayId == primary_display_id_);
}

void DisplayAndroidManager::RemoveDisplay(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobject,
    jint sdkDisplayId) {
  display_list().RemoveDisplay(sdkDisplayId);
}

void DisplayAndroidManager::SetPrimaryDisplayId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobject,
    jint sdkDisplayId) {
  primary_display_id_ = sdkDisplayId;
}

}  // namespace ui
