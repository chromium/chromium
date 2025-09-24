// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_EVENT_FORWARDER_H_
#define UI_ANDROID_EVENT_FORWARDER_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/android/ui_android_export.h"

namespace ui {

class KeyEventAndroid;
class MotionEventAndroid;
class ViewAndroid;

class UI_ANDROID_EXPORT EventForwarder {
 public:
  // Interface for observing events on the `EventForwarder`.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnTouchEvent(const ui::MotionEventAndroid&) {}

    virtual void OnMouseEvent(const ui::MotionEventAndroid&) {}

    virtual void OnGenericMotionEvent(const ui::MotionEventAndroid&) {}
  };

  EventForwarder(const EventForwarder&) = delete;
  EventForwarder& operator=(const EventForwarder&) = delete;

  ~EventForwarder();

  base::android::ScopedJavaLocalRef<jobject> GetJavaWindowAndroid(JNIEnv* env);

  // |oldest_event_time| and |latest_event_time| would be same for a MotionEvent
  // without any historical events attached to it. For cases when there are
  // historical events |oldest_event_time| will be the event time of earliest
  // input i.e. MotionEvent.getHistoricalEventTimeNanos(0) and
  // |latest_event_time| will be the event time of most recent event i.e.
  // MotionEvent.getEventTimeNanos().
  jboolean OnTouchEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& motion_event,
      jlong oldest_event_time_ns,
      jlong latest_event_time_ns,
      jint android_action,
      jfloat touch_major_0,
      jfloat touch_major_1,
      jfloat touch_minor_0,
      jfloat touch_minor_1,
      jint android_gesture_classification,
      jboolean is_touch_handle_event,
      jboolean is_latest_event_time_resampled);

  void OnMouseEvent(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& motion_event,
                    jlong time_ns,
                    jint android_action,
                    jint android_changed_button,
                    jint tool_type);

  void OnDragEvent(JNIEnv* env,
                   jint action,
                   jfloat x,
                   jfloat y,
                   jfloat screen_x,
                   jfloat screen_y,
                   const base::android::JavaParamRef<jobjectArray>& j_mimeTypes,
                   const base::android::JavaParamRef<jstring>& j_content,
                   const base::android::JavaParamRef<jobjectArray>& j_filenames,
                   const base::android::JavaParamRef<jstring>& j_text,
                   const base::android::JavaParamRef<jstring>& j_html,
                   const base::android::JavaParamRef<jstring>& j_url);

  jboolean OnGestureEvent(JNIEnv* env,
                          jint type,
                          jlong time_ms,
                          jfloat scale);

  jboolean OnGenericMotionEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& motion_event,
      jlong event_time_ns,
      jlong down_time_ms);

  void OnMouseWheelEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& motion_event,
      jlong time_ns,
      jfloat x,
      jfloat y,
      jfloat raw_x,
      jfloat raw_y,
      jfloat delta_x,
      jfloat delta_y);

  jboolean OnKeyUp(JNIEnv* env, const ui::KeyEventAndroid& key_event);

  jboolean DispatchKeyEvent(JNIEnv* env, const ui::KeyEventAndroid& key_event);

  void ScrollBy(JNIEnv* env,
                jfloat delta_x,
                jfloat delta_y);

  void ScrollTo(JNIEnv* env,
                jfloat x,
                jfloat y);

  void DoubleTap(JNIEnv* env,
                 jlong time_ms,
                 jint x,
                 jint y);

  void StartFling(JNIEnv* env,
                  jlong time_ms,
                  jfloat velocity_x,
                  jfloat velocity_y,
                  jboolean synthetic_scroll,
                  jboolean prevent_boosting,
                  jboolean is_touchpad_event);

  void CancelFling(JNIEnv* env,
                   jlong time_ms,
                   jboolean prevent_boosting,
                   jboolean is_touchpad_event);

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  float GetCurrentTouchSequenceYOffset();

 private:
  friend class ViewAndroid;

  explicit EventForwarder(ViewAndroid* view);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // last_x_pos_ & last_y_pos_ are only used for trace events (see b/315762684
  // for a relevant investigation). They are useful in debugging but could be
  // removed easily if needed.
  float last_x_pos_{-1.0};
  float last_y_pos_{-1.0};
  const raw_ptr<ViewAndroid> view_;
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  base::ObserverList<Observer> observers_;
  bool send_touch_moves_to_observers;
};

}  // namespace ui

#endif  // UI_ANDROID_EVENT_FORWARDER_H_
