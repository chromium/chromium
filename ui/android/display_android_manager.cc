// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/display_android_manager.h"

#include <jni.h>
#include <initializer_list>
#include <map>

#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/viz_utils.h"
#include "ui/android/screen_android.h"
#include "ui/android/ui_android_features.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/icc_profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/DisplayAndroidManager_jni.h"

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

std::optional<float> DisplayAndroidManager::GetPreferredScaleFactorForView(
    gfx::NativeView view) const {
  return GetDisplayNearestView(view).device_scale_factor();
}

void DisplayAndroidManager::DoUpdateDisplay(display::Display* display,
                                            gfx::Size size_in_pixels,
                                            float dipScale,
                                            int rotationDegrees,
                                            int bitsPerPixel,
                                            int bitsPerComponent,
                                            bool isWideColorGamut,
                                            bool isHdr,
                                            jfloat hdrMaxLuminanceRatio) {
  if (!Display::HasForceDeviceScaleFactor())
    display->set_device_scale_factor(dipScale);

  {
    // Decide the color space to use for sRGB, WCG, and HDR content. By default,
    // everything is crushed into sRGB.
    gfx::ColorSpace cs_for_srgb = gfx::ColorSpace::CreateSRGB();
    gfx::ColorSpace cs_for_wcg = cs_for_srgb;
    if (isWideColorGamut) {
      // If the device supports WCG, then use P3 for the output surface when
      // there is WCG content on screen.
      cs_for_wcg = gfx::ColorSpace::CreateDisplayP3D65();
      // If dynamically changing color gamut is disallowed, then use P3 even
      // when all content is sRGB.
      if (!features::IsDynamicColorGamutEnabled()) {
        cs_for_srgb = cs_for_wcg;
      }
    }
    // The color space for HDR is scaled to reach the maximum luminance ratio.
    gfx::ColorSpace cs_for_hdr = cs_for_wcg;
    if (base::FeatureList::IsEnabled(kAndroidHDR)) {
      if (hdrMaxLuminanceRatio > 1.f) {
        skcms_TransferFunction trfn;
        cs_for_hdr.GetTransferFunction(&trfn);
        trfn = skia::ScaleTransferFunction(trfn, hdrMaxLuminanceRatio);
        cs_for_hdr = gfx::ColorSpace(
            cs_for_hdr.GetPrimaryID(), gfx::ColorSpace::TransferID::CUSTOM_HDR,
            gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL,
            nullptr, &trfn);
      }
      if (isHdr) {
        hdrMaxLuminanceRatio = std::max(
            hdrMaxLuminanceRatio, display::kMinHDRCapableMaxLuminanceRelative);
      }
    } else {
      hdrMaxLuminanceRatio = 1.f;
    }
    // Propagate this into the DisplayColorSpaces.
    gfx::DisplayColorSpaces display_color_spaces(gfx::ColorSpace::CreateSRGB(),
                                                 gfx::BufferFormat::RGBA_8888);
    display_color_spaces.SetHDRMaxLuminanceRelative(hdrMaxLuminanceRatio);
    for (auto needs_alpha : {true, false}) {
      // TODO: Low-end devices should specify RGB_565 as the buffer format for
      // opaque content.
      display_color_spaces.SetOutputColorSpaceAndBufferFormat(
          gfx::ContentColorUsage::kSRGB, needs_alpha, cs_for_srgb,
          gfx::BufferFormat::RGBA_8888);
      display_color_spaces.SetOutputColorSpaceAndBufferFormat(
          gfx::ContentColorUsage::kWideColorGamut, needs_alpha, cs_for_wcg,
          gfx::BufferFormat::RGBA_8888);
      // TODO(crbug.com/40263227): Use 10-bit surfaces for opaque HDR.
      display_color_spaces.SetOutputColorSpaceAndBufferFormat(
          gfx::ContentColorUsage::kHDR, needs_alpha, cs_for_hdr,
          gfx::BufferFormat::RGBA_8888);
    }
    display->SetColorSpaces(display_color_spaces);
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
    jboolean isWideColorGamut,
    jboolean isHdr,
    jfloat hdrMaxLuminanceRatio) {
  gfx::Rect bounds_in_pixels = gfx::Rect(width, height);
  const gfx::Rect bounds_in_dip = gfx::Rect(
      gfx::ScaleToCeiledSize(bounds_in_pixels.size(), 1.0f / dipScale));

  display::Display display(sdkDisplayId, bounds_in_dip);
  DoUpdateDisplay(&display, bounds_in_pixels.size(), dipScale, rotationDegrees,
                  bitsPerPixel, bitsPerComponent,
                  isWideColorGamut && use_display_wide_color_gamut_, isHdr,
                  hdrMaxLuminanceRatio);
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
