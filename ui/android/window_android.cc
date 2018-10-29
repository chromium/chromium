// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/window_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "jni/WindowAndroid_jni.h"
#include "ui/android/window_android_compositor.h"
#include "ui/android/window_android_observer.h"

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

 private:
  WindowAndroid* const window_;
  base::ObserverList<viz::BeginFrameObserver>::Unchecked observers_;
  int observer_count_;
  viz::BeginFrameArgs last_begin_frame_args_;
  uint64_t next_sequence_number_;
  bool paused_;
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
  for (auto& obs : observers_)
    obs.OnBeginFrame(last_begin_frame_args_);
}

void WindowAndroid::WindowBeginFrameSource::OnVSync(
    base::TimeTicks frame_time,
    base::TimeDelta vsync_period) {
  // frame time is in the past, so give the next vsync period as the deadline.
  base::TimeTicks deadline = frame_time + vsync_period;
  last_begin_frame_args_ = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), next_sequence_number_, frame_time,
      deadline, vsync_period, viz::BeginFrameArgs::NORMAL);
  DCHECK(last_begin_frame_args_.IsValid());
  next_sequence_number_++;
  if (RequestCallbackOnGpuAvailable())
    return;
  OnGpuNoLongerBusy();
}

void WindowAndroid::WindowBeginFrameSource::OnPauseChanged(bool paused) {
  paused_ = paused;
  for (auto& obs : observers_)
    obs.OnBeginFrameSourcePausedChanged(paused_);
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
                             float scroll_factor)
    : display_id_(display_id),
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

void WindowAndroid::AddVSyncCompleteCallback(const base::Closure& callback) {
  vsync_complete_callbacks_.push_back(callback);
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

  for (const base::Closure& callback : vsync_complete_callbacks_)
    callback.Run();
  vsync_complete_callbacks_.clear();

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

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_WindowAndroid_Init(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             int sdk_display_id,
                             float scroll_factor) {
  WindowAndroid* window =
      new WindowAndroid(env, obj, sdk_display_id, scroll_factor);
  return reinterpret_cast<intptr_t>(window);
}

}  // namespace ui
