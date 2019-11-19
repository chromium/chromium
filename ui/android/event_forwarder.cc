// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/event_forwarder.h"

#include "base/android/jni_array.h"
#include "ui/android/ui_android_jni_headers/EventForwarder_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/android/motion_event_android.h"

namespace ui {

using base::android::AppendJavaStringArrayToStringVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

EventForwarder::EventForwarder(ViewAndroid* view) : view_(view) {}

EventForwarder::~EventForwarder() {
  if (!java_obj_.is_null()) {
    Java_EventForwarder_destroy(base::android::AttachCurrentThread(),
                                java_obj_);
    java_obj_.Reset();
  }
}

ScopedJavaLocalRef<jobject> EventForwarder::GetJavaObject() {
  if (java_obj_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_obj_.Reset(
        Java_EventForwarder_create(env, reinterpret_cast<intptr_t>(this),
                                   switches::IsTouchDragDropEnabled()));
  }
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

ScopedJavaLocalRef<jobject> EventForwarder::GetJavaWindowAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return view_->GetWindowAndroid()->GetJavaObject();
}

jboolean EventForwarder::OnTouchEvent(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      const JavaParamRef<jobject>& motion_event,
                                      jlong time_ms,
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
                                      jint android_button_state,
                                      jint android_meta_state,
                                      jboolean for_touch_handle) {
  ui::MotionEventAndroid::Pointer pointer0(
      pointer_id_0, pos_x_0, pos_y_0, touch_major_0, touch_minor_0,
      orientation_0, tilt_0, android_tool_type_0);
  ui::MotionEventAndroid::Pointer pointer1(
      pointer_id_1, pos_x_1, pos_y_1, touch_major_1, touch_minor_1,
      orientation_1, tilt_1, android_tool_type_1);
  ui::MotionEventAndroid event(
      env, motion_event.obj(), 1.f / view_->GetDipScale(), 0.f, 0.f, 0.f,
      time_ms, android_action, pointer_count, history_size, action_index,
      0 /* action_button */, android_button_state, android_meta_state,
      raw_pos_x - pos_x_0, raw_pos_y - pos_y_0, for_touch_handle, &pointer0,
      &pointer1);
  return view_->OnTouchEvent(event);
}

void EventForwarder::OnMouseEvent(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jlong time_ms,
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
  ui::MotionEventAndroid event(
      env, nullptr /* event */, 1.f / view_->GetDipScale(), 0.f, 0.f, 0.f,
      time_ms, android_action, 1 /* pointer_count */, 0 /* history_size */,
      0 /* action_index */, android_action_button, android_button_state,
      android_meta_state, 0 /* raw_offset_x_pixels */,
      0 /* raw_offset_y_pixels */, false /* for_touch_handle */, &pointer,
      nullptr);
  view_->OnMouseEvent(event);
}

void EventForwarder::OnDragEvent(JNIEnv* env,
                                 const JavaParamRef<jobject>& jobj,
                                 jint action,
                                 jint x,
                                 jint y,
                                 jint screen_x,
                                 jint screen_y,
                                 const JavaParamRef<jobjectArray>& j_mimeTypes,
                                 const JavaParamRef<jstring>& j_content) {
  float dip_scale = view_->GetDipScale();
  gfx::PointF location(x / dip_scale, y / dip_scale);
  gfx::PointF root_location(screen_x / dip_scale, screen_y / dip_scale);
  std::vector<base::string16> mime_types;
  AppendJavaStringArrayToStringVector(env, j_mimeTypes, &mime_types);

  DragEventAndroid event(env, action, location, root_location, mime_types,
                         j_content.obj());
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
    jlong time_ms) {
  auto size = view_->GetSize();
  float x = size.width() / 2;
  float y = size.height() / 2;
  ui::MotionEventAndroid::Pointer pointer0(0, x, y, 0, 0, 0, 0, 0);
  ui::MotionEventAndroid event(
      env, motion_event.obj(), 1.f / view_->GetDipScale(), 0.f, 0.f, 0.f,
      time_ms, 0, 1, 0, 0, 0, 0, 0, 0, 0, false, &pointer0, nullptr);
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

}  // namespace ui
