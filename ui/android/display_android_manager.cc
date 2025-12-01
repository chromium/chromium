// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/display_android_manager.h"

#include <jni.h>

#include <initializer_list>
#include <map>
#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/shared_image_format.h"
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

namespace {
static std::optional<bool> is_display_topology_available = std::nullopt;
}

void SetScreenAndroid(bool use_display_wide_color_gamut) {
  TRACE_EVENT0("startup", "SetScreenAndroid");
  // Do not override existing Screen.
  DCHECK_EQ(display::Screen::Get(), nullptr);

  DisplayAndroidManager* manager =
      new DisplayAndroidManager(use_display_wide_color_gamut);
  display::Screen::SetScreenInstance(manager);

  JNIEnv* env = AttachCurrentThread();
  Java_DisplayAndroidManager_onNativeSideCreated(env, (jlong)manager);
}

bool DisplayAndroidManager::IsDisplayTopologyAvailable() {
  if (!is_display_topology_available.has_value()) {
    JNIEnv* env = AttachCurrentThread();
    is_display_topology_available =
        Java_DisplayAndroidManager_isDisplayTopologyAvailable(env);
  }

  return is_display_topology_available.value();
}

void DisplayAndroidManager::SetIsDisplayTopologyAvailableForTesting(
    bool value) {
  is_display_topology_available = value;
}

DisplayAndroidManager::DisplayAndroidManager(bool use_display_wide_color_gamut)
    : use_display_wide_color_gamut_(use_display_wide_color_gamut) {}

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

Display DisplayAndroidManager::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  if (IsDisplayTopologyAvailable()) {
    return ScreenBase::GetDisplayNearestPoint(point);
  }

  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

Display DisplayAndroidManager::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  if (IsDisplayTopologyAvailable()) {
    return ScreenBase::GetDisplayMatching(match_rect);
  }

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
                                            float pixels_per_inch_x,
                                            float pixels_per_inch_y,
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
  display->set_pixels_per_inch(pixels_per_inch_x, pixels_per_inch_y);

  {
    // Decide the color space to use for sRGB, WCG, and HDR content. By default,
    // everything is crushed into sRGB.
    gfx::ColorSpace cs_for_srgb = gfx::ColorSpace::CreateSRGB();
    gfx::ColorSpace cs_for_wcg = cs_for_srgb;
    if (is_wide_color_gamut) {
      // If the device supports WCG, then use P3 for the output surface when
      // there is WCG content on screen.
      cs_for_wcg = gfx::ColorSpace::CreateDisplayP3D65();
      cs_for_srgb = cs_for_wcg;
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
    gfx::DisplayColorSpaces display_color_spaces(
        gfx::ColorSpace::CreateSRGB(), viz::SinglePlaneFormat::kRGBA_8888);
    display_color_spaces.SetHDRMaxLuminanceRelative(hdr_max_luminance_ratio);
    for (auto needs_alpha : {true, false}) {
      // TODO: Low-end devices should specify RGB_565 as the format for opaque
      // content.
      display_color_spaces.SetOutputColorSpaceAndFormat(
          gfx::ContentColorUsage::kSRGB, needs_alpha, cs_for_srgb,
          viz::SinglePlaneFormat::kRGBA_8888);
      display_color_spaces.SetOutputColorSpaceAndFormat(
          gfx::ContentColorUsage::kWideColorGamut, needs_alpha, cs_for_wcg,
          viz::SinglePlaneFormat::kRGBA_8888);
      // TODO(crbug.com/40263227): Use 10-bit surfaces for opaque HDR.
      display_color_spaces.SetOutputColorSpaceAndFormat(
          gfx::ContentColorUsage::kHDR, needs_alpha, cs_for_hdr,
          viz::SinglePlaneFormat::kRGBA_8888);
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
    jint sdkDisplayId,
    const base::android::JavaRef<jstring>& label,
    const base::android::JavaRef<jintArray>&
        jBounds,  // {left, top, right, bottom} in dip
    const base::android::JavaRef<jintArray>&
        jWorkArea,  // {left, top, right, bottom} in dip
    jint width,     // in physical pixels
    jint height,    // in physical pixels
    jfloat dipScale,
    jfloat pixelsPerInchX,
    jfloat pixelsPerInchY,
    jint rotationDegrees,
    jint bitsPerPixel,
    jint bitsPerComponent,
    jboolean isWideColorGamut,
    jboolean isHdr,
    jfloat hdrMaxLuminanceRatio,
    jboolean isInternal) {
  std::vector<int> bounds_array, work_area_array;
  base::android::JavaIntArrayToIntVector(env, jBounds, &bounds_array);
  base::android::JavaIntArrayToIntVector(env, jWorkArea, &work_area_array);

  CHECK(bounds_array.size() == 4);
  CHECK(work_area_array.size() == 4);

  gfx::Rect bounds, work_area;
  bounds.SetByBounds(bounds_array[0], bounds_array[1], bounds_array[2],
                     bounds_array[3]);
  work_area.SetByBounds(work_area_array[0], work_area_array[1],
                        work_area_array[2], work_area_array[3]);
  const gfx::Size size_in_pixels(width, height);

  display::Display display(sdkDisplayId);
  DoUpdateDisplay(&display, base::android::ConvertJavaStringToUTF8(env, label),
                  bounds, work_area, size_in_pixels, dipScale, pixelsPerInchX,
                  pixelsPerInchY, rotationDegrees, bitsPerPixel,
                  bitsPerComponent,
                  isWideColorGamut && use_display_wide_color_gamut_, isHdr,
                  hdrMaxLuminanceRatio);

  if (isInternal) {
    display::AddInternalDisplayId(sdkDisplayId);
  }

  ProcessDisplayChanged(display, sdkDisplayId == primary_display_id_);
}

void DisplayAndroidManager::RemoveDisplay(
    JNIEnv* env,
    jint sdkDisplayId) {
  display_list().RemoveDisplay(sdkDisplayId);
  display::RemoveInternalDisplayId(sdkDisplayId);
}

void DisplayAndroidManager::SetPrimaryDisplayId(
    JNIEnv* env,
    jint sdkDisplayId) {
  primary_display_id_ = sdkDisplayId;
}

jint DisplayAndroidManager::GetDisplaySdkMatching(JNIEnv* env,
                                                  jint x,
                                                  jint y,
                                                  jint width,
                                                  jint height) {
  return GetDisplayMatching(gfx::Rect(x, y, width, height)).id();
}

}  // namespace ui

DEFINE_JNI(DisplayAndroidManager)
