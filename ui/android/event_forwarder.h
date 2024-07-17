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

  base::android::ScopedJavaLocalRef<jobject> GetJavaWindowAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // |oldest_event_time| and |latest_event_time| would be same for a MotionEvent
  // without any historical events attached to it. For cases when there are
  // historical events |oldest_event_time| will be the event time of earliest
  // input i.e. MotionEvent.getHistoricalEventTimeNanos(0) and
  // |latest_event_time| will be the event time of most recent event i.e.
  // MotionEvent.getEventTimeNanos().
  jboolean OnTouchEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& motion_event,
      jlong oldest_event_time_ns,
      jlong latest_event_time_ns,
      jint android_action,
      jint pointer_count,
      jint history_size,
      jint action_index,
      jfloat pos_x_0,
      jfloat pos_y_0,
      jfloat pos_x_1,
      jfloat pos_y_1,
      jint pointer_id_0,
      jint pointer_id_1,
      jfloat touch_major_0,
      jfloat touch_major_1,
      jfloat touch_minor_0,
      jfloat touch_minor_1,
      jfloat orientation_0,
      jfloat orientation_1,
      jfloat tilt_0,
      jfloat tilt_1,
      jfloat raw_pos_x,
      jfloat raw_pos_y,
      jint android_tool_type_0,
      jint android_tool_type_1,
      jint android_gesture_classification,
      jint android_button_state,
      jint android_meta_state,
      jboolean is_touch_handle_event);

  void OnMouseEvent(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jlong time_ns,
                    jint android_action,
                    jfloat x,
                    jfloat y,
                    jint pointer_id,
                    jfloat pressure,
                    jfloat orientation,
                    jfloat tilt,
                    jint android_changed_button,
                    jint android_button_state,
                    jint android_meta_state,
                    jint tool_type);

  void OnDragEvent(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jobj,
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
                          const base::android::JavaParamRef<jobject>& jobj,
                          jint type,
                          jlong time_ms,
                          jfloat scale);

  jboolean OnGenericMotionEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& motion_event,
      jlong time_ns);

  jboolean OnKeyUp(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& key_event,
                   jint key_code);

  jboolean DispatchKeyEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& motion_event);

  void ScrollBy(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& jobj,
                jfloat delta_x,
                jfloat delta_y);

  void ScrollTo(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& jobj,
                jfloat x,
                jfloat y);

  void DoubleTap(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jobj,
                 jlong time_ms,
                 jint x,
                 jint y);

  void StartFling(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jobj,
                  jlong time_ms,
                  jfloat velocity_x,
                  jfloat velocity_y,
                  jboolean synthetic_scroll,
                  jboolean prevent_boosting);

  void CancelFling(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jobj,
                   jlong time_ms,
                   jboolean prevent_boosting);

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

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
