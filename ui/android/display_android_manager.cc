// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/display_android_manager.h"

#include <jni.h>

#include <initializer_list>
#include <map>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/viz_utils.h"
#include "skia/ext/skcolorspace_trfn.h"
#include "ui/android/screen_android.h"
#include "ui/android/ui_android_features.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
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
                                            const std::string& label,
                                            const gfx::Rect& bounds,
                                            const gfx::Rect& work_area,
                                            const gfx::Size& size_in_pixels,
                                            float dip_scale,
                                            int rotation_degrees,
                                            int bits_per_pixel,
                                            int bits_per_component,
                                            bool is_wide_color_gamut,
                                            bool is_hdr,
                                            float hdr_max_luminance_ratio) {
  display->set_label(label);
  display->set_bounds(bounds);
  if (base::FeatureList::IsEnabled(kAndroidUseCorrectDisplayWorkArea)) {
    display->set_work_area(work_area);
  } else {
    display->set_work_area(bounds);
  }
  display->set_size_in_pixels(size_in_pixels);
  display->set_device_scale_factor(dip_scale);

  {
    // Decide the color space to use for sRGB, WCG, and HDR content. By default,
    // everything is crushed into sRGB.
    gfx::ColorSpace cs_for_srgb = gfx::ColorSpace::CreateSRGB();
    gfx::ColorSpace cs_for_wcg = cs_for_srgb;
    if (is_wide_color_gamut) {
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
      if (hdr_max_luminance_ratio > 1.f) {
        skcms_TransferFunction trfn;
        cs_for_hdr.GetTransferFunction(&trfn);
        trfn = skia::ScaleTransferFunction(trfn, hdr_max_luminance_ratio);
        cs_for_hdr = gfx::ColorSpace(
            cs_for_hdr.GetPrimaryID(), gfx::ColorSpace::TransferID::CUSTOM_HDR,
            gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL,
            nullptr, &trfn);
      }
      if (is_hdr) {
        hdr_max_luminance_ratio =
            std::max(hdr_max_luminance_ratio,
                     display::kMinHDRCapableMaxLuminanceRelative);
      }
    } else {
      hdr_max_luminance_ratio = 1.f;
    }
    // Propagate this into the DisplayColorSpaces.
    gfx::DisplayColorSpaces display_color_spaces(gfx::ColorSpace::CreateSRGB(),
                                                 gfx::BufferFormat::RGBA_8888);
    display_color_spaces.SetHDRMaxLuminanceRelative(hdr_max_luminance_ratio);
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

  display->SetRotationAsDegree(rotation_degrees);
  DCHECK_EQ(rotation_degrees, display->RotationAsDegree());
  DCHECK_EQ(rotation_degrees, display->PanelRotationAsDegree());
  display->set_color_depth(bits_per_pixel);
  display->set_depth_per_component(bits_per_component);
  display->set_is_monochrome(bits_per_component == 0);
}

// Methods called from Java

void DisplayAndroidManager::UpdateDisplay(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jObject,
    jint sdkDisplayId,
    const base::android::JavaRef<jstring>& label,
    const base::android::JavaRef<jintArray>&
        jBounds,  // the order is: left, top, right, bottom
    const base::android::JavaRef<jintArray>&
        jInsets,  // the order is: left, top, right, bottom
    jfloat dipScale,
    jint rotationDegrees,
    jint bitsPerPixel,
    jint bitsPerComponent,
    jboolean isWideColorGamut,
    jboolean isHdr,
    jfloat hdrMaxLuminanceRatio,
    jboolean isInternal) {
  if (Display::HasForceDeviceScaleFactor()) {
    dipScale = Display::GetForcedDeviceScaleFactor();
  }

  std::vector<int> bounds, insets;
  base::android::JavaIntArrayToIntVector(env, jBounds, &bounds);
  base::android::JavaIntArrayToIntVector(env, jInsets, &insets);

  gfx::Rect bounds_in_pixels;
  bounds_in_pixels.SetByBounds(bounds[0], bounds[1], bounds[2], bounds[3]);

  const gfx::Rect dip_bounds =
      gfx::ScaleToEnclosingRect(bounds_in_pixels, 1.0f / dipScale);

  gfx::Rect work_area_in_pixels = bounds_in_pixels;
  work_area_in_pixels.Inset(
      gfx::Insets::TLBR(insets[1], insets[0], insets[3], insets[2]));
  const gfx::Rect dip_work_area =
      gfx::ScaleToEnclosingRect(work_area_in_pixels, 1.0f / dipScale);

  display::Display display(sdkDisplayId);
  DoUpdateDisplay(&display, base::android::ConvertJavaStringToUTF8(env, label),
                  dip_bounds, dip_work_area, bounds_in_pixels.size(), dipScale,
                  rotationDegrees, bitsPerPixel, bitsPerComponent,
                  isWideColorGamut && use_display_wide_color_gamut_, isHdr,
                  hdrMaxLuminanceRatio);

  if (isInternal) {
    display::AddInternalDisplayId(sdkDisplayId);
  }

  ProcessDisplayChanged(display, sdkDisplayId == primary_display_id_);
}

void DisplayAndroidManager::RemoveDisplay(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jObject,
    jint sdkDisplayId) {
  display_list().RemoveDisplay(sdkDisplayId);
  display::RemoveInternalDisplayId(sdkDisplayId);
}

void DisplayAndroidManager::SetPrimaryDisplayId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jObject,
    jint sdkDisplayId) {
  primary_display_id_ = sdkDisplayId;
}

}  // namespace ui
