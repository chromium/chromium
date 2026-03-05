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
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/trace_event/trace_event.h"
#include "ui/android/color_utils_android.h"
#include "ui/android/display_android_manager.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/android/window_android_observer.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/gfx/display_color_spaces.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/WindowAndroid_jni.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

const float kDefaultMouseWheelTickMultiplier = 64;

WindowAndroid::ScopedSelectionHandles::ScopedSelectionHandles(
    WindowAndroid* window)
    : window_(window) {
  if (++window_->selection_handles_active_count_ == 1) {
    JNIEnv* env = AttachCurrentThread();
    window_->java_window_->onSelectionHandlesStateChanged(env,
                                                          true /* active */);
  }
}

WindowAndroid::ScopedSelectionHandles::~ScopedSelectionHandles() {
  DCHECK_GT(window_->selection_handles_active_count_, 0);

  if (--window_->selection_handles_active_count_ == 0) {
    JNIEnv* env = AttachCurrentThread();
    window_->java_window_->onSelectionHandlesStateChanged(env,
                                                          false /* active */);
  }
}

WindowAndroid::ScopedWindowAndroidForTesting::ScopedWindowAndroidForTesting(
    WindowAndroid* window)
    : window_(window) {}

WindowAndroid::ScopedWindowAndroidForTesting::~ScopedWindowAndroidForTesting() {
  JNIEnv* env = AttachCurrentThread();
  window_->java_window_->destroy(env);
}

void WindowAndroid::ScopedWindowAndroidForTesting::SetModalDialogManager(
    base::android::ScopedJavaLocalRef<jobject> modal_dialog_manager) {
  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_setModalDialogManagerForTesting(  // IN-TEST
      env, window_->GetJavaObject(), modal_dialog_manager);
}

WindowAndroid::AdaptiveRefreshRateInfo::AdaptiveRefreshRateInfo() = default;
WindowAndroid::AdaptiveRefreshRateInfo::AdaptiveRefreshRateInfo(
    const AdaptiveRefreshRateInfo& other) = default;
WindowAndroid::AdaptiveRefreshRateInfo::~AdaptiveRefreshRateInfo() = default;
WindowAndroid::AdaptiveRefreshRateInfo&
WindowAndroid::AdaptiveRefreshRateInfo::operator=(
    const AdaptiveRefreshRateInfo& other) = default;

// static
WindowAndroid* WindowAndroid::FromJavaWindowAndroid(
    const JavaRef<jobject>& jwindow_android) {
  if (jwindow_android.is_null())
    return nullptr;

  return reinterpret_cast<WindowAndroid*>(Java_WindowAndroid_getNativePointer(
      AttachCurrentThread(), jwindow_android));
}

WindowAndroid::WindowAndroid(JNIEnv* env,
                             const base::android::JavaRef<JWindowAndroid>& obj,
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

void WindowAndroid::Destroy(JNIEnv* env) {
  delete this;
}

ScopedJavaLocalRef<JWindowAndroid> WindowAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<JWindowAndroid>(java_window_);
}

WindowAndroid::~WindowAndroid() {
  DCHECK(parent_ == nullptr) << "WindowAndroid must be a root view.";
  DCHECK(!compositor_);
  RemoveAllChildren(true);
  DCHECK(!pointer_locking_view_);
  java_window_->clearNativePointer(AttachCurrentThread());
}

std::unique_ptr<WindowAndroid::ScopedWindowAndroidForTesting>
WindowAndroid::CreateForTesting() {
  JNIEnv* env = AttachCurrentThread();
  long native_pointer = JWindowAndroidClass::createForTesting(env);
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
}

void WindowAndroid::DetachCompositor() {
  observer_list_.Notify(&WindowAndroidObserver::OnDetachCompositor);
  observer_list_.Clear();
  compositor_ = nullptr;
}

float WindowAndroid::GetRefreshRate() {
  JNIEnv* env = AttachCurrentThread();
  return java_window_->getRefreshRate(env);
}

gfx::OverlayTransform WindowAndroid::GetOverlayTransform() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<gfx::OverlayTransform>(
      java_window_->getOverlayTransform(env));
}

std::vector<float> WindowAndroid::GetSupportedRefreshRates() {
  if (test_hooks_)
    return test_hooks_->GetSupportedRates();

  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jfloatArray> j_supported_refresh_rates =
      java_window_->getSupportedRefreshRates(env);
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
  java_window_->setPreferredRefreshRate(env, refresh_rate);
}

void WindowAndroid::SetNeedsAnimate() {
  if (compositor_)
    compositor_->SetNeedsAnimate();
}

void WindowAndroid::Animate(base::TimeTicks begin_frame_time) {
  observer_list_.Notify(&WindowAndroidObserver::OnAnimate, begin_frame_time);
}

void WindowAndroid::OnVisibilityChanged(JNIEnv* env, bool visible) {
  observer_list_.Notify(&WindowAndroidObserver::OnRootWindowVisibilityChanged,
                        visible);
}

void WindowAndroid::OnActivityStopped(JNIEnv* env) {
  observer_list_.Notify(&WindowAndroidObserver::OnActivityStopped);
}

void WindowAndroid::OnActivityStarted(JNIEnv* env) {
  observer_list_.Notify(&WindowAndroidObserver::OnActivityStarted);
}

void WindowAndroid::OnUpdateRefreshRate(JNIEnv* env, float refresh_rate) {
  if (compositor_)
    compositor_->OnUpdateRefreshRate(refresh_rate);
}

void WindowAndroid::OnSupportedRefreshRatesUpdated(
    JNIEnv* env,
    const JavaRef<jfloatArray>& j_supported_refresh_rates) {
  std::vector<float> supported_refresh_rates;
  if (j_supported_refresh_rates) {
    base::android::JavaFloatArrayToFloatVector(env, j_supported_refresh_rates,
                                               &supported_refresh_rates);
  }
  if (compositor_)
    compositor_->OnUpdateSupportedRefreshRates(supported_refresh_rates);
}

void WindowAndroid::OnAdaptiveRefreshRateInfoChanged(
    JNIEnv* env,
    bool supports_adaptive_refresh_rate,
    float suggested_frame_rate_high,
    const std::vector<float> frame_per_second,
    const std::vector<float> dp_per_second) {
  adaptive_refresh_rate_info_.supports_adaptive_refresh_rate =
      supports_adaptive_refresh_rate;
  adaptive_refresh_rate_info_.suggested_frame_rate_high =
      suggested_frame_rate_high;

  adaptive_refresh_rate_info_.velocity_mapping.clear();
  CHECK_EQ(frame_per_second.size(), dp_per_second.size());
  for (size_t i = 0; i < frame_per_second.size(); ++i) {
    adaptive_refresh_rate_info_.velocity_mapping.push_back(
        {frame_per_second[i], dp_per_second[i]});
  }
}

void WindowAndroid::OnOverlayTransformUpdated(JNIEnv* env) {
  if (compositor_)
    compositor_->OnUpdateOverlayTransform();
}

void WindowAndroid::SendUnfoldLatencyBeginTimestamp(JNIEnv* env,
                                                    int64_t begin_time) {
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
      env, java_window_->getProgressBarConfig(env), &values);

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
      java_window_->getNativeModalDialogManagerBridge(env));
}

void WindowAndroid::SetModalDialogManagerForTesting(
    base::android::ScopedJavaLocalRef<jobject> java_modal_dialog_manager) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::Java_WindowAndroid_setModalDialogManagerForTesting(
      env, GetJavaObject(), java_modal_dialog_manager);
}

bool WindowAndroid::SendKeyEventsForTesting(KeyboardCode key,
                                            int key_event_types,
                                            bool shift,
                                            bool control,
                                            bool alt,
                                            bool command) {
  return Java_WindowAndroid_sendKeyEventsForTesting(
             AttachCurrentThread(), GetJavaObject(),
             AndroidKeyCodeFromKeyboardCode(key), key_event_types,
             (shift ? JNI_TRUE : JNI_FALSE), (control ? JNI_TRUE : JNI_FALSE),
             (alt ? JNI_TRUE : JNI_FALSE),
             /*meta=*/(command ? JNI_TRUE : JNI_FALSE)) == JNI_TRUE;
}

void WindowAndroid::ShowToast(const std::string text) {
  JNIEnv* env = AttachCurrentThread();
  java_window_->showToast(env,
                          base::android::ConvertUTF8ToJavaString(env, text));
}

void WindowAndroid::SetWideColorEnabled(bool enabled) {
  JNIEnv* env = AttachCurrentThread();
  java_window_->setWideColorEnabled(env, enabled);
}

bool WindowAndroid::HasPermission(const std::string& permission) {
  JNIEnv* env = AttachCurrentThread();
  return java_window_->hasPermission(
      env, base::android::ConvertUTF8ToJavaString(env, permission));
}

bool WindowAndroid::CanRequestPermission(const std::string& permission) {
  JNIEnv* env = AttachCurrentThread();
  return java_window_->canRequestPermission(
      env, base::android::ConvertUTF8ToJavaString(env, permission));
}

WindowAndroid* WindowAndroid::GetWindowAndroid() const {
  DCHECK(parent_ == nullptr);
  return const_cast<WindowAndroid*>(this);
}

display::Display WindowAndroid::GetDisplayWithWindowColorSpace() {
  display::Display display =
      display::Screen::Get()->GetDisplayNearestWindow(this);
  DisplayAndroidManager::DoUpdateDisplay(
      &display, display.label(), display.bounds(), display.work_area(),
      display.GetSizeInPixel(), display.device_scale_factor(),
      display.GetPixelsPerInchX(), display.GetPixelsPerInchY(),
      display.RotationAsDegree(), display.color_depth(),
      display.depth_per_component(), window_is_wide_color_gamut_,
      display.GetColorSpaces().SupportsHDR(),
      display.GetColorSpaces().GetHDRMaxLuminanceRelative());
  return display;
}

bool WindowAndroid::RequestPointerLock(ViewAndroid& view_android) {
  DCHECK(view_android.GetWindowAndroid() == this);
  DCHECK(view_android.GetContainerView());
  DCHECK(pointer_locking_view_ == nullptr);

  JNIEnv* env = AttachCurrentThread();
  bool has_lock = Java_WindowAndroid_requestPointerLock(
      env, GetJavaObject(), view_android.GetContainerView());

  if (has_lock) {
    pointer_locking_view_ = &view_android;
  }

  return has_lock;
}

bool WindowAndroid::HasPointerLock(ViewAndroid& view_android) {
  return pointer_locking_view_ == &view_android;
}

void WindowAndroid::ReleasePointerLock(ViewAndroid& view_android) {
  DCHECK(&view_android == pointer_locking_view_);
  pointer_locking_view_ = nullptr;

  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_releasePointerLock(env, GetJavaObject(),
                                               view_android.GetContainerView());
}

void WindowAndroid::OnWindowPointerLockRelease(JNIEnv* env) {
  DCHECK(pointer_locking_view_);
  pointer_locking_view_->OnPointerLockRelease();
  pointer_locking_view_ = nullptr;
}

void WindowAndroid::OnWindowPositionChanged(JNIEnv* env) {
  DispatchWindowPositionChange();
}

bool WindowAndroid::SetHasKeyboardCapture(bool keyboard_capture) {
  JNIEnv* env = AttachCurrentThread();
  return java_window_->setHasKeyboardCapture(env, keyboard_capture);
}

std::optional<gfx::Rect> WindowAndroid::GetBoundsInScreenCoordinates() {
  TRACE_EVENT("ui", "WindowAndroid::GetBoundsInScreenCoordinates");
  const base::TimeTicks start_time = base::TimeTicks::Now();

  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jintArray> j_bounds_array =
      java_window_->getBoundsInScreenCoordinates(env);
  if (!j_bounds_array) {
    return std::nullopt;
  }

  std::vector<int> bounds_vector;
  base::android::JavaIntArrayToIntVector(env, j_bounds_array, &bounds_vector);
  CHECK(bounds_vector.size() == 4);

  const int x = bounds_vector[0];
  const int y = bounds_vector[1];
  const int width = bounds_vector[2];
  const int height = bounds_vector[3];

  UMA_HISTOGRAM_TIMES("Android.Window.TimeToAcquireWindowBounds",
                      base::TimeTicks::Now() - start_time);
  return gfx::Rect(x, y, width, height);
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

static int64_t JNI_WindowAndroid_Init(JNIEnv* env,
                                      const JavaRef<JWindowAndroid>& obj,
                                      int32_t sdk_display_id,
                                      float scroll_factor,
                                      bool window_is_wide_color_gamut) {
  WindowAndroid* window = new WindowAndroid(
      env, obj, sdk_display_id, scroll_factor, window_is_wide_color_gamut);
  return reinterpret_cast<intptr_t>(window);
}

}  // namespace ui

DEFINE_JNI(WindowAndroid)
