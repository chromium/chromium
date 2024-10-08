// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/event_forwarder.h"

#include "base/android/jni_array.h"
#include "base/numerics/ranges.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "ui/android/ui_android_features.h"
#include "ui/android/window_android.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/android/motion_event_android_java.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/EventForwarder_jni.h"

namespace ui {
namespace {
static constexpr float kEpsilon = 1e-5f;
using base::android::AppendJavaStringArrayToStringVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
}  // namespace

EventForwarder::EventForwarder(ViewAndroid* view)
    : view_(view),
      send_touch_moves_to_observers(base::FeatureList::IsEnabled(
          kSendTouchMovesToEventForwarderObservers)) {}

EventForwarder::~EventForwarder() {
  if (!java_obj_.is_null()) {
    Java_EventForwarder_destroy(jni_zero::AttachCurrentThread(), java_obj_);
    java_obj_.Reset();
  }
}

ScopedJavaLocalRef<jobject> EventForwarder::GetJavaObject() {
  if (java_obj_.is_null()) {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    java_obj_.Reset(
        Java_EventForwarder_create(env, reinterpret_cast<intptr_t>(this),
                                   switches::IsTouchDragDropEnabled()));
  }
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

jboolean EventForwarder::OnTouchEvent(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      const JavaParamRef<jobject>& motion_event,
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
                                      jboolean for_touch_handle) {
  TRACE_EVENT(
      "input", "EventForwarder::OnTouchEvent", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* forwarder = event->set_event_forwarder();
        forwarder->set_history_size(history_size);
        forwarder->set_latest_time_ns(latest_event_time_ns);
        // In the case of unbuffered input that chrome uses usually history_size
        // == 0 and oldest_time == latest_time so to save trace buffer space we
        // emit it only if they are different.
        if (oldest_event_time_ns != latest_event_time_ns) {
          forwarder->set_oldest_time_ns(oldest_event_time_ns);
        }
        forwarder->set_x_pixel(pos_x_0);
        forwarder->set_y_pixel(pos_y_0);
        // Only record if there was movement for Action::Move (we'll update the
        // last position on the first Motion::TouchDown).
        if (android_action ==
            MotionEventAndroid::GetAndroidAction(MotionEvent::Action::MOVE)) {
          forwarder->set_has_x_movement(
              !base::IsApproximatelyEqual(pos_x_0, last_x_pos_, kEpsilon));
          forwarder->set_has_y_movement(
              !base::IsApproximatelyEqual(pos_y_0, last_y_pos_, kEpsilon));
        }
      });
  last_x_pos_ = pos_x_0;
  last_y_pos_ = pos_y_0;

  ui::MotionEventAndroid::Pointer pointer0(
      pointer_id_0, pos_x_0, pos_y_0, touch_major_0, touch_minor_0,
      orientation_0, tilt_0, android_tool_type_0);
  ui::MotionEventAndroid::Pointer pointer1(
      pointer_id_1, pos_x_1, pos_y_1, touch_major_1, touch_minor_1,
      orientation_1, tilt_1, android_tool_type_1);
  ui::MotionEventAndroidJava event(
      env, motion_event.obj(), 1.f / view_->GetDipScale(), 0.f, 0.f, 0.f,
      base::TimeTicks::FromJavaNanoTime(oldest_event_time_ns),
      base::TimeTicks::FromJavaNanoTime(latest_event_time_ns), android_action,
      pointer_count, history_size, action_index, 0 /* action_button */,
      android_gesture_classification, android_button_state, android_meta_state,
      0 /* source */, raw_pos_x - pos_x_0, raw_pos_y - pos_y_0,
      for_touch_handle, &pointer0, &pointer1);

  if (send_touch_moves_to_observers ||
      android_action !=
          MotionEventAndroid::GetAndroidAction(MotionEvent::Action::MOVE)) {
    // Don't send touch moves to observers. Currently we just have one observer
    // which shouldn't be affected by this. This is a temporary change until we
    // have confirmed touch moves are not required by the observer and we can
    // cleanup the observer API.
    // TODO(b/328601354): Confirm touch moves are not required, and if they are
    // not required cleanup the observer API.
    observers_.Notify(&Observer::OnTouchEvent, event);
  }

  return view_->OnTouchEvent(event);
}

void EventForwarder::OnMouseEvent(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jlong time_ns,
                                  jint android_action,
                                  jfloat x,
                                  jfloat y,
                                  jint pointer_id,
                                  jfloat orientation,
                                  jfloat pressure,
                                  jfloat tilt,
                                  jint android_action_button,
                                  jint android_button_state,
                                  jint android_meta_state,
                                  jint android_tool_type) {
  // Construct a motion_event object minimally, only to convert the raw
  // parameters to ui::MotionEvent values. Since we used only the cached values
  // at index=0, it is okay to even pass a null event to the constructor.
  ui::MotionEventAndroid::Pointer pointer(
      pointer_id, x, y, 0.0f /* touch_major */, 0.0f /* touch_minor */,
      orientation, tilt, android_tool_type);
  ui::MotionEventAndroidJava event(
      env, nullptr /* event */, 1.f / view_->GetDipScale(), 0.f, 0.f, 0.f,
      base::TimeTicks::FromJavaNanoTime(time_ns), android_action,
      1 /* pointer_count */, 0 /* history_size */, 0 /* action_index */,
      android_action_button, 0 /* gesture_classification */,
      android_button_state, android_meta_state, 0 /* source */,
      0 /* raw_offset_x_pixels */, 0 /* raw_offset_y_pixels */,
      false /* for_touch_handle */, &pointer, nullptr);

  observers_.Notify(&Observer::OnMouseEvent, event);

  view_->OnMouseEvent(event);
}

void EventForwarder::OnDragEvent(JNIEnv* env,
                                 const JavaParamRef<jobject>& jobj,
                                 jint action,
                                 jfloat x,
                                 jfloat y,
                                 jfloat screen_x,
                                 jfloat screen_y,
                                 const JavaParamRef<jobjectArray>& j_mimeTypes,
                                 const JavaParamRef<jstring>& j_content,
                                 const JavaParamRef<jobjectArray>& j_filenames,
                                 const JavaParamRef<jstring>& j_text,
                                 const JavaParamRef<jstring>& j_html,
                                 const JavaParamRef<jstring>& j_url) {
  float dip_scale = view_->GetDipScale();
  gfx::PointF location(x / dip_scale, y / dip_scale);
  gfx::PointF root_location(screen_x / dip_scale, screen_y / dip_scale);
  std::vector<std::u16string> mime_types;
  AppendJavaStringArrayToStringVector(env, j_mimeTypes, &mime_types);

  DragEventAndroid event(env, action, location, root_location, mime_types,
                         j_content.obj(), j_filenames.obj(), j_text.obj(),
                         j_html.obj(), j_url.obj());
  view_->OnDragEvent(event);
}

jboolean EventForwarder::OnGestureEvent(JNIEnv* env,
                                        const JavaParamRef<jobject>& jobj,
                                        jint type,
                                        jlong time_ms,
                                        jfloat scale) {
  float dip_scale = view_->GetDipScale();
  auto size = view_->GetSize();
  float x = size.width() / 2;
  float y = size.height() / 2;
  gfx::PointF root_location =
      ScalePoint(view_->GetLocationOnScreen(x, y), 1.f / dip_scale);
  return view_->OnGestureEvent(GestureEventAndroid(
      type, gfx::PointF(x / dip_scale, y / dip_scale), root_location, time_ms,
      scale, 0, 0, 0, 0, /*target_viewport*/ false, /*synthetic_scroll*/ false,
      /*prevent_boosting*/ false));
}

jboolean EventForwarder::OnGenericMotionEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& motion_event,
    jlong time_ns) {
  auto size = view_->GetSize();
  float x = size.width() / 2;
  float y = size.height() / 2;
  ui::MotionEventAndroid::Pointer pointer0(0, x, y, 0, 0, 0, 0, 0);
  ui::MotionEventAndroidJava event(
      env, motion_event.obj(), 1.f / view_->GetDipScale(), 0.f, 0.f, 0.f,
      base::TimeTicks::FromJavaNanoTime(time_ns), 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      0, false, &pointer0, nullptr);

  observers_.Notify(&Observer::OnGenericMotionEvent, event);

  return view_->OnGenericMotionEvent(event);
}

jboolean EventForwarder::OnKeyUp(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jobject>& key_event,
                                 jint key_code) {
  return view_->OnKeyUp(KeyEventAndroid(env, key_event, key_code));
}

jboolean EventForwarder::DispatchKeyEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& key_event) {
  return view_->DispatchKeyEvent(KeyEventAndroid(env, key_event, 0));
}

void EventForwarder::ScrollBy(JNIEnv* env,
                              const JavaParamRef<jobject>& jobj,
                              jfloat delta_x,
                              jfloat delta_y) {
  view_->ScrollBy(delta_x, delta_y);
}

void EventForwarder::ScrollTo(JNIEnv* env,
                              const JavaParamRef<jobject>& jobj,
                              jfloat x,
                              jfloat y) {
  view_->ScrollTo(x, y);
}

void EventForwarder::DoubleTap(JNIEnv* env,
                               const JavaParamRef<jobject>& jobj,
                               jlong time_ms,
                               jint x,
                               jint y) {
  float dip_scale = view_->GetDipScale();
  view_->OnGestureEvent(GestureEventAndroid(
      GESTURE_EVENT_TYPE_DOUBLE_TAP, gfx::PointF(x / dip_scale, y / dip_scale),
      gfx::PointF(), time_ms, 0, 0, 0, 0, 0, /*target_viewport*/ true,
      /*synthetic_scroll*/ false, /*prevent_boosting*/ false));
}

void EventForwarder::StartFling(JNIEnv* env,
                                const JavaParamRef<jobject>& jobj,
                                jlong time_ms,
                                jfloat velocity_x,
                                jfloat velocity_y,
                                jboolean synthetic_scroll,
                                jboolean prevent_boosting) {
  CancelFling(env, jobj, time_ms, prevent_boosting);

  if (velocity_x == 0 && velocity_y == 0)
    return;
  float dip_scale = view_->GetDipScale();
  // Use velocity as delta in scroll event.
  view_->OnGestureEvent(GestureEventAndroid(
      GESTURE_EVENT_TYPE_SCROLL_START, gfx::PointF(), gfx::PointF(), time_ms, 0,
      velocity_x / dip_scale, velocity_y / dip_scale, 0, 0,
      /*target_viewport*/ true, synthetic_scroll,
      /*prevent_boosting*/ false));
  view_->OnGestureEvent(GestureEventAndroid(
      GESTURE_EVENT_TYPE_FLING_START, gfx::PointF(), gfx::PointF(), time_ms, 0,
      0, 0, velocity_x / dip_scale, velocity_y / dip_scale,
      /*target_viewport*/ true, synthetic_scroll,
      /*prevent_boosting*/ false));
}

void EventForwarder::CancelFling(JNIEnv* env,
                                 const JavaParamRef<jobject>& jobj,
                                 jlong time_ms,
                                 jboolean prevent_boosting) {
  view_->OnGestureEvent(GestureEventAndroid(
      GESTURE_EVENT_TYPE_FLING_CANCEL, gfx::PointF(), gfx::PointF(), time_ms, 0,
      0, 0, 0, 0,
      /*target_viewport*/ false, /*synthetic_scroll*/ false, prevent_boosting));
}

void EventForwarder::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EventForwarder::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ui
