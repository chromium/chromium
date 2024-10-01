// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/window_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/observer_list.h"
#include "ui/android/color_utils_android.h"
#include "ui/android/display_android_manager.h"
#include "ui/android/window_android_compositor.h"
#include "ui/android/window_android_observer.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/display_color_spaces.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/WindowAndroid_jni.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

const float kDefaultMouseWheelTickMultiplier = 64;

WindowAndroid::ScopedSelectionHandles::ScopedSelectionHandles(
    WindowAndroid* window)
    : window_(window) {
  if (++window_->selection_handles_active_count_ == 1) {
    JNIEnv* env = AttachCurrentThread();
    Java_WindowAndroid_onSelectionHandlesStateChanged(
        env, window_->GetJavaObject(), true /* active */);
  }
}

WindowAndroid::ScopedSelectionHandles::~ScopedSelectionHandles() {
  DCHECK_GT(window_->selection_handles_active_count_, 0);

  if (--window_->selection_handles_active_count_ == 0) {
    JNIEnv* env = AttachCurrentThread();
    Java_WindowAndroid_onSelectionHandlesStateChanged(
        env, window_->GetJavaObject(), false /* active */);
  }
}

WindowAndroid::ScopedWindowAndroidForTesting::ScopedWindowAndroidForTesting(
    WindowAndroid* window)
    : window_(window) {}

WindowAndroid::ScopedWindowAndroidForTesting::~ScopedWindowAndroidForTesting() {
  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_destroy(env, window_->GetJavaObject());
}

void WindowAndroid::ScopedWindowAndroidForTesting::SetModalDialogManager(
    base::android::ScopedJavaLocalRef<jobject> modal_dialog_manager) {
  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_setModalDialogManagerForTesting(  // IN-TEST
      env, window_->GetJavaObject(), modal_dialog_manager);
}

// static
WindowAndroid* WindowAndroid::FromJavaWindowAndroid(
    const JavaParamRef<jobject>& jwindow_android) {
  if (jwindow_android.is_null())
    return nullptr;

  return reinterpret_cast<WindowAndroid*>(Java_WindowAndroid_getNativePointer(
      AttachCurrentThread(), jwindow_android));
}

WindowAndroid::WindowAndroid(JNIEnv* env,
                             jobject obj,
                             int display_id,
                             float scroll_factor,
                             bool window_is_wide_color_gamut)
    : display_id_(display_id),
      window_is_wide_color_gamut_(window_is_wide_color_gamut),
      compositor_(nullptr) {
  java_window_.Reset(env, obj);
  mouse_wheel_scroll_factor_ =
      scroll_factor > 0 ? scroll_factor
                        : kDefaultMouseWheelTickMultiplier * GetDipScale();
}

void WindowAndroid::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jobject> WindowAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_window_);
}

WindowAndroid::~WindowAndroid() {
  DCHECK(parent_ == nullptr) << "WindowAndroid must be a root view.";
  DCHECK(!compositor_);
  RemoveAllChildren(true);
  Java_WindowAndroid_clearNativePointer(AttachCurrentThread(), GetJavaObject());
}

std::unique_ptr<WindowAndroid::ScopedWindowAndroidForTesting>
WindowAndroid::CreateForTesting() {
  JNIEnv* env = AttachCurrentThread();
  long native_pointer = Java_WindowAndroid_createForTesting(env);
  return std::make_unique<ScopedWindowAndroidForTesting>(
      reinterpret_cast<WindowAndroid*>(native_pointer));
}

void WindowAndroid::AddObserver(WindowAndroidObserver* observer) {
  if (!observer_list_.HasObserver(observer))
    observer_list_.AddObserver(observer);
}

void WindowAndroid::RemoveObserver(WindowAndroidObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void WindowAndroid::AttachCompositor(WindowAndroidCompositor* compositor) {
  if (compositor_ && compositor != compositor_)
    DetachCompositor();

  compositor_ = compositor;
  observer_list_.Notify(&WindowAndroidObserver::OnAttachCompositor);

  compositor_->SetVSyncPaused(vsync_paused_);
}

void WindowAndroid::DetachCompositor() {
  observer_list_.Notify(&WindowAndroidObserver::OnDetachCompositor);
  observer_list_.Clear();
  compositor_ = nullptr;
}

float WindowAndroid::GetRefreshRate() {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_getRefreshRate(env, GetJavaObject());
}

gfx::OverlayTransform WindowAndroid::GetOverlayTransform() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<gfx::OverlayTransform>(
      Java_WindowAndroid_getOverlayTransform(env, GetJavaObject()));
}

std::vector<float> WindowAndroid::GetSupportedRefreshRates() {
  if (test_hooks_)
    return test_hooks_->GetSupportedRates();

  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jfloatArray> j_supported_refresh_rates =
      Java_WindowAndroid_getSupportedRefreshRates(env, GetJavaObject());
  std::vector<float> supported_refresh_rates;
  if (j_supported_refresh_rates) {
    base::android::JavaFloatArrayToFloatVector(env, j_supported_refresh_rates,
                                               &supported_refresh_rates);
  }
  return supported_refresh_rates;
}

void WindowAndroid::SetPreferredRefreshRate(float refresh_rate) {
  if (test_hooks_) {
    test_hooks_->SetPreferredRate(refresh_rate);
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_setPreferredRefreshRate(env, GetJavaObject(),
                                             refresh_rate);
}

void WindowAndroid::SetNeedsAnimate() {
  if (compositor_)
    compositor_->SetNeedsAnimate();
}

void WindowAndroid::Animate(base::TimeTicks begin_frame_time) {
  observer_list_.Notify(&WindowAndroidObserver::OnAnimate, begin_frame_time);
}

void WindowAndroid::OnVisibilityChanged(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        bool visible) {
  observer_list_.Notify(&WindowAndroidObserver::OnRootWindowVisibilityChanged,
                        visible);
}

void WindowAndroid::OnActivityStopped(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  observer_list_.Notify(&WindowAndroidObserver::OnActivityStopped);
}

void WindowAndroid::OnActivityStarted(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  observer_list_.Notify(&WindowAndroidObserver::OnActivityStarted);
}

void WindowAndroid::SetVSyncPaused(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   bool paused) {
  vsync_paused_ = paused;

  if (compositor_)
    compositor_->SetVSyncPaused(paused);
}

void WindowAndroid::OnUpdateRefreshRate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    float refresh_rate) {
  if (compositor_)
    compositor_->OnUpdateRefreshRate(refresh_rate);
}

void WindowAndroid::OnSupportedRefreshRatesUpdated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const JavaParamRef<jfloatArray>& j_supported_refresh_rates) {
  std::vector<float> supported_refresh_rates;
  if (j_supported_refresh_rates) {
    base::android::JavaFloatArrayToFloatVector(env, j_supported_refresh_rates,
                                               &supported_refresh_rates);
  }
  if (compositor_)
    compositor_->OnUpdateSupportedRefreshRates(supported_refresh_rates);
}

void WindowAndroid::OnOverlayTransformUpdated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (compositor_)
    compositor_->OnUpdateOverlayTransform();
}

void WindowAndroid::SendUnfoldLatencyBeginTimestamp(JNIEnv* env,
                                                    jlong begin_time) {
  base::TimeTicks begin_timestamp =
      base::TimeTicks::FromUptimeMillis(begin_time);
  observer_list_.Notify(&WindowAndroidObserver::OnUnfoldStarted,
                        begin_timestamp);
}

ProgressBarConfig WindowAndroid::GetProgressBarConfig() {
  if (progress_bar_config_for_testing_) {
    return *progress_bar_config_for_testing_;
  }

  JNIEnv* env = AttachCurrentThread();
  std::vector<int> values;
  base::android::JavaIntArrayToIntVector(
      env, Java_WindowAndroid_getProgressBarConfig(env, GetJavaObject()),
      &values);

  ProgressBarConfig config;
  config.background_color =
      SkColor4f::FromColor(*JavaColorToOptionalSkColor(values[0]));
  config.height_physical = values[1];
  config.color = SkColor4f::FromColor(*JavaColorToOptionalSkColor(values[2]));
  config.hairline_height_physical = values[3];
  config.hairline_color =
      SkColor4f::FromColor(*JavaColorToOptionalSkColor(values[4]));
  return config;
}

ModalDialogManagerBridge* WindowAndroid::GetModalDialogManagerBridge() {
  JNIEnv* env = AttachCurrentThread();
  return reinterpret_cast<ModalDialogManagerBridge*>(
      Java_WindowAndroid_getNativeModalDialogManagerBridge(env,
                                                           GetJavaObject()));
}

void WindowAndroid::SetModalDialogManagerForTesting(
    base::android::ScopedJavaLocalRef<jobject> java_modal_dialog_manager) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::Java_WindowAndroid_setModalDialogManagerForTesting(
      env, GetJavaObject(), java_modal_dialog_manager);
}

void WindowAndroid::SetWideColorEnabled(bool enabled) {
  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_setWideColorEnabled(env, GetJavaObject(), enabled);
}

bool WindowAndroid::HasPermission(const std::string& permission) {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_hasPermission(
      env, GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, permission));
}

bool WindowAndroid::CanRequestPermission(const std::string& permission) {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_canRequestPermission(
      env, GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, permission));
}

WindowAndroid* WindowAndroid::GetWindowAndroid() const {
  DCHECK(parent_ == nullptr);
  return const_cast<WindowAndroid*>(this);
}

display::Display WindowAndroid::GetDisplayWithWindowColorSpace() {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(this);
  DisplayAndroidManager::DoUpdateDisplay(
      &display, display.GetSizeInPixel(), display.device_scale_factor(),
      display.RotationAsDegree(), display.color_depth(),
      display.depth_per_component(), window_is_wide_color_gamut_,
      display.GetColorSpaces().SupportsHDR(),
      display.GetColorSpaces().GetHDRMaxLuminanceRelative());
  return display;
}

void WindowAndroid::SetTestHooks(TestHooks* hooks) {
  test_hooks_ = hooks;
  if (!test_hooks_)
    return;

  if (compositor_) {
    compositor_->OnUpdateSupportedRefreshRates(
        test_hooks_->GetSupportedRates());
  }
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_WindowAndroid_Init(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             jint sdk_display_id,
                             jfloat scroll_factor,
                             jboolean window_is_wide_color_gamut) {
  WindowAndroid* window = new WindowAndroid(
      env, obj, sdk_display_id, scroll_factor, window_is_wide_color_gamut);
  return reinterpret_cast<intptr_t>(window);
}

}  // namespace ui
