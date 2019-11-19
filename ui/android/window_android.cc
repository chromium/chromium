// Copyright 2013 The Chromium Authors. All rights reserved.
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
#include "base/stl_util.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "ui/android/display_android_manager.h"
#include "ui/android/ui_android_jni_headers/WindowAndroid_jni.h"
#include "ui/android/window_android_compositor.h"
#include "ui/android/window_android_observer.h"
#include "ui/base/ui_base_features.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

const float kDefaultMouseWheelTickMultiplier = 64;

class WindowAndroid::WindowBeginFrameSource : public viz::BeginFrameSource {
 public:
  explicit WindowBeginFrameSource(WindowAndroid* window)
      : BeginFrameSource(kNotRestartableId),
        window_(window),
        observers_(base::ObserverListPolicy::EXISTING_ONLY),
        observer_count_(0),
        next_sequence_number_(viz::BeginFrameArgs::kStartingFrameNumber),
        paused_(false) {}
  ~WindowBeginFrameSource() override {}

  // viz::BeginFrameSource implementation.
  void AddObserver(viz::BeginFrameObserver* obs) override;
  void RemoveObserver(viz::BeginFrameObserver* obs) override;
  void DidFinishFrame(viz::BeginFrameObserver* obs) override {}
  bool IsThrottled() const override { return true; }
  void OnGpuNoLongerBusy() override;

  void OnVSync(base::TimeTicks frame_time, base::TimeDelta vsync_period);
  void OnPauseChanged(bool paused);
  void AddBeginFrameCompletionCallback(base::OnceClosure callback);

 private:
  friend class WindowAndroid::ScopedOnBeginFrame;

  WindowAndroid* const window_;
  base::ObserverList<viz::BeginFrameObserver>::Unchecked observers_;
  int observer_count_;
  viz::BeginFrameArgs last_begin_frame_args_;
  uint64_t next_sequence_number_;
  bool paused_;
  // Used for determining what the sequence number should be on
  // CreateBeginFrameArgs.
  base::TimeTicks next_expected_frame_time_;

  // Set by ScopedOnBeginFrame.
  std::vector<base::OnceClosure>* vsync_complete_callbacks_ptr_ = nullptr;
};

class WindowAndroid::ScopedOnBeginFrame {
 public:
  explicit ScopedOnBeginFrame(WindowAndroid::WindowBeginFrameSource* bfs,
                              bool allow_reentrancy);
  ~ScopedOnBeginFrame();

 private:
  WindowAndroid::WindowBeginFrameSource* const begin_frame_source_;
  const bool reentrant_;
  std::vector<base::OnceClosure> vsync_complete_callbacks_;
};

void WindowAndroid::WindowBeginFrameSource::AddObserver(
    viz::BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(!observers_.HasObserver(obs));

  observers_.AddObserver(obs);
  observer_count_++;
  obs->OnBeginFrameSourcePausedChanged(paused_);
  window_->SetNeedsBeginFrames(true);

  // Send a MISSED BeginFrame if possible and necessary.
  if (last_begin_frame_args_.IsValid()) {
    viz::BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() ||
        last_args.frame_time < last_begin_frame_args_.frame_time) {
      DCHECK(last_args.sequence_number <
                 last_begin_frame_args_.sequence_number ||
             last_args.source_id != last_begin_frame_args_.source_id);
      last_begin_frame_args_.type = viz::BeginFrameArgs::MISSED;
      // TODO(crbug.com/602485): A deadline doesn't make too much sense
      // for a missed BeginFrame (the intention rather is 'immediately'),
      // but currently the retro frame logic is very strict in discarding
      // BeginFrames.
      last_begin_frame_args_.deadline =
          base::TimeTicks::Now() + last_begin_frame_args_.interval;
      ScopedOnBeginFrame scope(this, true /* allow_reentrancy */);
      obs->OnBeginFrame(last_begin_frame_args_);
    }
  }
}

void WindowAndroid::WindowBeginFrameSource::RemoveObserver(
    viz::BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.HasObserver(obs));

  observers_.RemoveObserver(obs);
  observer_count_--;
  if (observer_count_ <= 0)
    window_->SetNeedsBeginFrames(false);
}

void WindowAndroid::WindowBeginFrameSource::OnGpuNoLongerBusy() {
  ScopedOnBeginFrame scope(this, false /* allow_reentrancy */);
  for (auto& obs : observers_)
    obs.OnBeginFrame(last_begin_frame_args_);
}

void WindowAndroid::WindowBeginFrameSource::OnVSync(
    base::TimeTicks frame_time,
    base::TimeDelta vsync_period) {
  uint64_t sequence_number = next_sequence_number_;
  // We expect |sequence_number| to be the number for the frame at
  // |expected_frame_time|. We adjust this sequence number according to the
  // actual frame time in case it is later than expected.
  if (next_expected_frame_time_ != base::TimeTicks()) {
    // Add |error_margin| to round |frame_time| up to the next tick if it is
    // close to the end of an interval. This happens when a timebase is a bit
    // off because of an imperfect presentation timestamp that may be a bit
    // later than the beginning of the next interval.
    constexpr double kErrorMarginIntervalPct = 0.05;
    base::TimeDelta error_margin = vsync_period * kErrorMarginIntervalPct;
    int ticks_since_estimated_frame_time =
        (frame_time + error_margin - next_expected_frame_time_) / vsync_period;
    sequence_number += std::max(0, ticks_since_estimated_frame_time);
  }

  // frame time is in the past, so give the next vsync period as the deadline.
  base::TimeTicks deadline = frame_time + vsync_period;
  last_begin_frame_args_ = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), sequence_number, frame_time, deadline,
      vsync_period, viz::BeginFrameArgs::NORMAL);
  DCHECK(last_begin_frame_args_.IsValid());
  next_sequence_number_ = sequence_number + 1;
  next_expected_frame_time_ = deadline;
  if (RequestCallbackOnGpuAvailable())
    return;
  OnGpuNoLongerBusy();
}

void WindowAndroid::WindowBeginFrameSource::OnPauseChanged(bool paused) {
  paused_ = paused;
  for (auto& obs : observers_)
    obs.OnBeginFrameSourcePausedChanged(paused_);
}

void WindowAndroid::WindowBeginFrameSource::AddBeginFrameCompletionCallback(
    base::OnceClosure callback) {
  CHECK(vsync_complete_callbacks_ptr_);
  vsync_complete_callbacks_ptr_->emplace_back(std::move(callback));
}

WindowAndroid::ScopedOnBeginFrame::ScopedOnBeginFrame(
    WindowAndroid::WindowBeginFrameSource* bfs,
    bool allow_reentrancy)
    : begin_frame_source_(bfs),
      reentrant_(allow_reentrancy &&
                 begin_frame_source_->vsync_complete_callbacks_ptr_) {
  if (reentrant_) {
    DCHECK(begin_frame_source_->vsync_complete_callbacks_ptr_);
    return;
  }
  DCHECK(!begin_frame_source_->vsync_complete_callbacks_ptr_);
  begin_frame_source_->vsync_complete_callbacks_ptr_ =
      &vsync_complete_callbacks_;
}

WindowAndroid::ScopedOnBeginFrame::~ScopedOnBeginFrame() {
  if (reentrant_) {
    DCHECK_NE(&vsync_complete_callbacks_,
              begin_frame_source_->vsync_complete_callbacks_ptr_);
    return;
  }
  DCHECK_EQ(&vsync_complete_callbacks_,
            begin_frame_source_->vsync_complete_callbacks_ptr_);
  begin_frame_source_->vsync_complete_callbacks_ptr_ = nullptr;
  for (base::OnceClosure& callback : vsync_complete_callbacks_)
    std::move(callback).Run();
}

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
      compositor_(NULL),
      begin_frame_source_(new WindowBeginFrameSource(this)),
      needs_begin_frames_(false) {
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

WindowAndroid* WindowAndroid::CreateForTesting() {
  JNIEnv* env = AttachCurrentThread();
  long native_pointer = Java_WindowAndroid_createForTesting(env);
  return reinterpret_cast<WindowAndroid*>(native_pointer);
}

void WindowAndroid::OnCompositingDidCommit() {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnCompositingDidCommit();
}

void WindowAndroid::AddObserver(WindowAndroidObserver* observer) {
  if (!observer_list_.HasObserver(observer))
    observer_list_.AddObserver(observer);
}

void WindowAndroid::AddBeginFrameCompletionCallback(
    base::OnceClosure callback) {
  begin_frame_source_->AddBeginFrameCompletionCallback(std::move(callback));
}

void WindowAndroid::RemoveObserver(WindowAndroidObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

viz::BeginFrameSource* WindowAndroid::GetBeginFrameSource() {
  return begin_frame_source_.get();
}

void WindowAndroid::AttachCompositor(WindowAndroidCompositor* compositor) {
  if (compositor_ && compositor != compositor_)
    DetachCompositor();

  compositor_ = compositor;
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnAttachCompositor();

  compositor_->SetVSyncPaused(vsync_paused_);
}

void WindowAndroid::DetachCompositor() {
  compositor_ = NULL;
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnDetachCompositor();
  observer_list_.Clear();
}

void WindowAndroid::RequestVSyncUpdate() {
  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_requestVSyncUpdate(env, GetJavaObject());
}

float WindowAndroid::GetRefreshRate() {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_getRefreshRate(env, GetJavaObject());
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
  if (force_60hz_refresh_rate_)
    return;

  if (test_hooks_) {
    test_hooks_->SetPreferredRate(refresh_rate);
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_setPreferredRefreshRate(env, GetJavaObject(),
                                             refresh_rate);
}

void WindowAndroid::SetNeedsBeginFrames(bool needs_begin_frames) {
  if (needs_begin_frames_ == needs_begin_frames)
    return;

  needs_begin_frames_ = needs_begin_frames;
  if (needs_begin_frames_)
    RequestVSyncUpdate();
}

void WindowAndroid::SetNeedsAnimate() {
  if (compositor_)
    compositor_->SetNeedsAnimate();
}

void WindowAndroid::Animate(base::TimeTicks begin_frame_time) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnAnimate(begin_frame_time);
}

void WindowAndroid::OnVSync(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            jlong time_micros,
                            jlong period_micros) {
  // Warning: It is generally unsafe to manufacture TimeTicks values. The
  // following assumption is being made, AND COULD EASILY BREAK AT ANY TIME:
  // Upstream, Java code is providing "System.nanos() / 1000," and this is the
  // same timestamp that would be provided by the CLOCK_MONOTONIC POSIX clock.
  DCHECK_EQ(base::TimeTicks::GetClock(),
            base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC);
  base::TimeTicks frame_time =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(time_micros);
  base::TimeDelta vsync_period(
      base::TimeDelta::FromMicroseconds(period_micros));

  begin_frame_source_->OnVSync(frame_time, vsync_period);

  if (needs_begin_frames_)
    RequestVSyncUpdate();
}

void WindowAndroid::OnVisibilityChanged(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        bool visible) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnRootWindowVisibilityChanged(visible);
}

void WindowAndroid::OnActivityStopped(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnActivityStopped();
}

void WindowAndroid::OnActivityStarted(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnActivityStarted();
}

void WindowAndroid::SetVSyncPaused(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   bool paused) {
  vsync_paused_ = paused;

  if (compositor_)
    compositor_->SetVSyncPaused(paused);

  begin_frame_source_->OnPauseChanged(paused);
}

void WindowAndroid::OnCursorVisibilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool visible) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnCursorVisibilityChanged(visible);
}

void WindowAndroid::OnFallbackCursorModeToggled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool is_on) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnFallbackCursorModeToggled(is_on);
}

void WindowAndroid::OnUpdateRefreshRate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    float refresh_rate) {
  if (compositor_)
    compositor_->OnUpdateRefreshRate(refresh_rate);
  Force60HzRefreshRateIfNeeded();
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

  Force60HzRefreshRateIfNeeded();
}

void WindowAndroid::SetForce60HzRefreshRate() {
  if (force_60hz_refresh_rate_)
    return;

  force_60hz_refresh_rate_ = true;
  Force60HzRefreshRateIfNeeded();
}

void WindowAndroid::Force60HzRefreshRateIfNeeded() {
  if (!force_60hz_refresh_rate_)
    return;

  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_setPreferredRefreshRate(env, GetJavaObject(), 60.f);
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

ScopedJavaLocalRef<jobject> WindowAndroid::GetWindowToken() {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_getWindowToken(env, GetJavaObject());
}

display::Display WindowAndroid::GetDisplayWithWindowColorSpace() {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(this);
  DisplayAndroidManager::DoUpdateDisplay(
      &display, display.GetSizeInPixel(), display.device_scale_factor(),
      display.RotationAsDegree(), display.color_depth(),
      display.depth_per_component(), window_is_wide_color_gamut_);
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
