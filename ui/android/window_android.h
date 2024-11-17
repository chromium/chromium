// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_WINDOW_ANDROID_H_
#define UI_ANDROID_WINDOW_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"
#include "ui/android/progress_bar_config.h"
#include "ui/android/ui_android_export.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/overlay_transform.h"

namespace display {
class Display;
class DisplayAndroidManager;
}  // namespace display

namespace ui {

extern UI_ANDROID_EXPORT const float kDefaultMouseWheelTickMultiplier;

class ModalDialogManagerBridge;
class WindowAndroidCompositor;
class WindowAndroidObserver;

// Android implementation of the activity window.
// WindowAndroid is also the root of a ViewAndroid tree.
class UI_ANDROID_EXPORT WindowAndroid : public ViewAndroid {
 public:
  // Intended for unittests only.
  class ScopedWindowAndroidForTesting {
   public:
    ScopedWindowAndroidForTesting(WindowAndroid* window);
    ~ScopedWindowAndroidForTesting();

    WindowAndroid* get() { return window_; }

    void SetModalDialogManager(
        base::android::ScopedJavaLocalRef<jobject> modal_dialog_manager);

   private:
    raw_ptr<WindowAndroid> window_;
  };

  static WindowAndroid* FromJavaWindowAndroid(
      const base::android::JavaParamRef<jobject>& jwindow_android);

  WindowAndroid(JNIEnv* env,
                jobject obj,
                int display_id,
                float scroll_factor,
                bool window_is_wide_color_gamut);

  WindowAndroid(const WindowAndroid&) = delete;
  WindowAndroid& operator=(const WindowAndroid&) = delete;

  ~WindowAndroid() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  void AttachCompositor(WindowAndroidCompositor* compositor);
  void DetachCompositor();

  void AddObserver(WindowAndroidObserver* observer);
  void RemoveObserver(WindowAndroidObserver* observer);

  WindowAndroidCompositor* GetCompositor() { return compositor_; }
  float GetRefreshRate();
  gfx::OverlayTransform GetOverlayTransform();
  std::vector<float> GetSupportedRefreshRates();
  void SetPreferredRefreshRate(float refresh_rate);

  void SetNeedsAnimate();
  void Animate(base::TimeTicks begin_frame_time);
  void OnVisibilityChanged(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           bool visible);
  void OnActivityStopped(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  void OnActivityStarted(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  void SetVSyncPaused(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      bool paused);
  void OnUpdateRefreshRate(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           float refresh_rate);
  void OnSupportedRefreshRatesUpdated(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jfloatArray>& supported_refresh_rates);
  void OnOverlayTransformUpdated(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SendUnfoldLatencyBeginTimestamp(JNIEnv* env, jlong begin_time);

  // Return whether the specified Android permission is granted.
  bool HasPermission(const std::string& permission);
  // Return whether the specified Android permission can be requested by Chrome.
  bool CanRequestPermission(const std::string& permission);

  void set_progress_bar_config_for_testing(ProgressBarConfig config) {
    progress_bar_config_for_testing_ = config;
  }
  ProgressBarConfig GetProgressBarConfig();

  ModalDialogManagerBridge* GetModalDialogManagerBridge();

  // Intended for native browser tests.
  void SetModalDialogManagerForTesting(
      base::android::ScopedJavaLocalRef<jobject> java_modal_dialog_manager);

  float mouse_wheel_scroll_factor() const { return mouse_wheel_scroll_factor_; }

  static std::unique_ptr<ScopedWindowAndroidForTesting> CreateForTesting();

  // This should return the same Display as Screen::GetDisplayNearestWindow
  // except the color space depends on the status of this particular window
  // rather than the display itself.
  // See comment on WindowAndroid.getWindowIsWideColorGamut for details.
  display::Display GetDisplayWithWindowColorSpace();

  void SetWideColorEnabled(bool enabled);

  class TestHooks {
   public:
    virtual ~TestHooks() = default;
    virtual std::vector<float> GetSupportedRates() = 0;
    virtual void SetPreferredRate(float refresh_rate) = 0;
  };
  void SetTestHooks(TestHooks* hooks);

  class ScopedSelectionHandles {
   public:
    ScopedSelectionHandles(WindowAndroid* window);
    ~ScopedSelectionHandles();

   private:
    raw_ptr<WindowAndroid> window_;
  };

 private:
  class WindowBeginFrameSource;
  class ScopedOnBeginFrame;
  friend class DisplayAndroidManager;
  friend class WindowBeginFrameSource;

  void Force60HzRefreshRateIfNeeded();

  // ViewAndroid overrides.
  WindowAndroid* GetWindowAndroid() const override;

  // The ID of the display that this window belongs to.
  int display_id() const { return display_id_; }

  base::android::ScopedJavaGlobalRef<jobject> java_window_;
  const int display_id_;
  const bool window_is_wide_color_gamut_;
  raw_ptr<WindowAndroidCompositor> compositor_;

  std::optional<ProgressBarConfig> progress_bar_config_for_testing_;

  base::ObserverList<WindowAndroidObserver>::Unchecked observer_list_;
  blink::ContentToVisibleTimeReporter content_to_visible_time_recorder_;

  float mouse_wheel_scroll_factor_;
  bool vsync_paused_ = false;

  raw_ptr<TestHooks> test_hooks_ = nullptr;

  int selection_handles_active_count_ = 0;
};

}  // namespace ui

#endif  // UI_ANDROID_WINDOW_ANDROID_H_
