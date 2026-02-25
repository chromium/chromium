// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_EVENT_FORWARDER_H_
#define UI_ANDROID_EVENT_FORWARDER_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/point_f.h"

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
  bool OnTouchEvent(JNIEnv* env,
                    const base::android::JavaRef<jobject>& motion_event,
                    int64_t oldest_event_time_ns,
                    int64_t latest_event_time_ns,
                    int32_t android_action,
                    float touch_major_0,
                    float touch_major_1,
                    float touch_minor_0,
                    float touch_minor_1,
                    int32_t android_gesture_classification,
                    bool is_touch_handle_event,
                    bool is_latest_event_time_resampled);

  void OnMouseEvent(JNIEnv* env,
                    const base::android::JavaRef<jobject>& motion_event,
                    int64_t time_ns,
                    int32_t android_action,
                    int32_t android_changed_button,
                    int32_t tool_type);

  void OnDragEvent(JNIEnv* env,
                   int32_t action,
                   float x,
                   float y,
                   float screen_x,
                   float screen_y,
                   const base::android::JavaRef<jobjectArray>& j_mimeTypes,
                   const base::android::JavaRef<jstring>& j_content,
                   const base::android::JavaRef<jobjectArray>& j_filenames,
                   const base::android::JavaRef<jstring>& j_text,
                   const base::android::JavaRef<jstring>& j_html,
                   const base::android::JavaRef<jstring>& j_url);

  bool OnGestureEvent(JNIEnv* env, int32_t type, int64_t time_ms, float scale);

  bool OnGenericMotionEvent(JNIEnv* env,
                            const base::android::JavaRef<jobject>& motion_event,
                            int64_t event_time_ns,
                            int64_t down_time_ms);

  void OnMouseWheelEvent(JNIEnv* env,
                         const base::android::JavaRef<jobject>& motion_event,
                         int64_t time_ns,
                         float x,
                         float y,
                         float raw_x,
                         float raw_y,
                         float delta_x,
                         float delta_y);

  bool OnKeyUp(JNIEnv* env, const ui::KeyEventAndroid& key_event);

  bool DispatchKeyEvent(JNIEnv* env, const ui::KeyEventAndroid& key_event);

  void ScrollBy(JNIEnv* env, float delta_x, float delta_y);

  void ScrollTo(JNIEnv* env, float x, float y);

  void DoubleTap(JNIEnv* env, int64_t time_ms, int32_t x, int32_t y);

  void StartFling(JNIEnv* env,
                  int64_t time_ms,
                  float velocity_x,
                  float velocity_y,
                  bool synthetic_scroll,
                  bool prevent_boosting,
                  bool is_touchpad_event);

  void CancelFling(JNIEnv* env,
                   int64_t time_ms,
                   bool prevent_boosting,
                   bool is_touchpad_event);

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  gfx::PointF GetCurrentTouchSequenceOffset();

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

  // A weak reference to the Java object. The Java object will be kept alive by
  // a static map in the Java code. ScopedJavaGlobalRef would scale poorly with
  // a large number of WebContents as each entry would consume a slot in the
  // finite global ref table.
  JavaObjectWeakGlobalRef java_obj_;

  base::ObserverList<Observer> observers_;
  bool send_touch_moves_to_observers;
};

}  // namespace ui

#endif  // UI_ANDROID_EVENT_FORWARDER_H_
