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
#include "ui/base/ui_base_features.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/android/motion_event_android_factory.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/android/motion_event_android_source_java.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/EventForwarder_jni.h"
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

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
                                   features::IsTouchDragAndDropEnabled()));
  }
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

jboolean EventForwarder::OnTouchEvent(JNIEnv* env,
                                      const JavaParamRef<jobject>& motion_event,
                                      jlong oldest_event_time_ns,
                                      jlong latest_event_time_ns,
                                      jint android_action,
                                      jfloat touch_major_0,
                                      jfloat touch_major_1,
                                      jfloat touch_minor_0,
                                      jfloat touch_minor_1,
                                      jint android_gesture_classification,
                                      jboolean for_touch_handle,
                                      jboolean is_latest_event_resampled) {
  std::unique_ptr<MotionEventAndroidSource> source =
      MotionEventAndroidSourceJava::Create(motion_event,
                                           is_latest_event_resampled);
  jint pointer_count =
      JNI_MotionEvent::Java_MotionEvent_getPointerCount(env, motion_event);
  jint history_size =
      JNI_MotionEvent::Java_MotionEvent_getHistorySize(env, motion_event);
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
        jfloat pos_x_0 = source->GetXPix(0);
        jfloat pos_y_0 = source->GetYPix(0);
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
        jlong down_time_ms =
            JNI_MotionEvent::Java_MotionEvent_getDownTime(env, motion_event);
        forwarder->set_down_time_ns(down_time_ms *
                                    base::Time::kNanosecondsPerMillisecond);
        forwarder->set_action(
            static_cast<
                perfetto::protos::pbzero::EventForwarder::AMotionEventAction>(
                android_action));
      });
  jfloat pos_x_0 = source->GetXPix(0);
  jfloat pos_y_0 = source->GetYPix(0);
  last_x_pos_ = pos_x_0;
  last_y_pos_ = pos_y_0;

  MotionEventAndroid::Pointer pointer0(
      source->GetPointerId(0),
      /*pos_x_pixels=*/pos_x_0,
      /*pos_y_pixels=*/pos_y_0,
      /*touch_major_pixels=*/touch_major_0,
      /*touch_minor_pixels=*/touch_minor_0, source->GetPressure(0),
      source->GetRawOrientation(0), source->GetRawTilt(0),
      MotionEventAndroid::GetAndroidToolType(source->GetToolType(0)));
  std::unique_ptr<MotionEventAndroid::Pointer> pointer1;
  if (pointer_count > 1) {
    pointer1 = std::make_unique<MotionEventAndroid::Pointer>(
        source->GetPointerId(1),
        /*pos_x_pixels=*/source->GetXPix(1),
        /*pos_y_pixels=*/source->GetYPix(1),
        /*touch_major_pixels=*/touch_major_1,
        /*touch_minor_pixels=*/touch_minor_1, source->GetPressure(1),
        source->GetRawOrientation(1), source->GetRawTilt(1),
        MotionEventAndroid::GetAndroidToolType(source->GetToolType(1)));
  }
  // Java |MotionEvent.getDownTime| returns the value in milliseconds, use
  // base::TimeTicks::FromUptimeMillis to get base::TimeTicks for this
  // milliseconds timestamp.
  base::TimeTicks down_time = base::TimeTicks::FromUptimeMillis(
      JNI_MotionEvent::Java_MotionEvent_getDownTime(env, motion_event));
  auto event = ui::MotionEventAndroidFactory::CreateFromJava(
      env, /*event=*/motion_event,
      /*pix_to_dip=*/1.f / view_->GetDipScale(),
      /*ticks_x=*/0.f,
      /*ticks_y=*/0.f,
      /*tick_multiplier=*/0.f,
      base::TimeTicks::FromJavaNanoTime(oldest_event_time_ns),
      base::TimeTicks::FromJavaNanoTime(latest_event_time_ns), down_time,
      android_action, pointer_count, history_size,
      JNI_MotionEvent::Java_MotionEvent_getActionIndex(env, motion_event),
      /*android_action_button=*/0, android_gesture_classification,
      JNI_MotionEvent::Java_MotionEvent_getButtonState(env, motion_event),
      /*raw_offset_x_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getRawX(env, motion_event) - pos_x_0,
      /*raw_offset_y_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getRawY(env, motion_event) - pos_y_0,
      for_touch_handle, &pointer0, pointer1.get(), is_latest_event_resampled);

  if (send_touch_moves_to_observers ||
      android_action !=
          MotionEventAndroid::GetAndroidAction(MotionEvent::Action::MOVE)) {
    // Don't send touch moves to observers. Currently we just have one observer
    // which shouldn't be affected by this. This is a temporary change until we
    // have confirmed touch moves are not required by the observer and we can
    // cleanup the observer API.
    // TODO(b/328601354): Confirm touch moves are not required, and if they are
    // not required cleanup the observer API.
    observers_.Notify(&Observer::OnTouchEvent, *event);
  }

  return view_->OnTouchEvent(*event);
}

void EventForwarder::OnMouseEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& motion_event,
    jlong time_ns,
    jint android_action,
    jint android_action_button,
    jint android_tool_type) {
  std::unique_ptr<MotionEventAndroidSource> source =
      MotionEventAndroidSourceJava::Create(
          motion_event, /*is_latest_event_time_resampled=*/false);
  // Construct a motion_event object minimally, only to convert the raw
  // parameters to ui::MotionEvent values. Since we used only the cached values
  // at index=0, it is okay to even pass a null event to the constructor.
  ui::MotionEventAndroid::Pointer pointer(
      source->GetPointerId(0),
      /*pos_x_pixels=*/source->GetXPix(0),
      /*pos_y_pixels=*/source->GetYPix(0),
      /*touch_major_pixels=*/0.0f, /*touch_minor_pixels=*/0.0f,
      source->GetPressure(0), source->GetRawOrientation(0),
      source->GetRawTilt(0),
      /*tool_type=*/android_tool_type);
  auto event = ui::MotionEventAndroidFactory::CreateFromJava(
      env, /*event=*/motion_event,
      /*pix_to_dip=*/1.f / view_->GetDipScale(),
      /*ticks_x=*/0.f,
      /*ticks_y=*/0.f,
      /*tick_multiplier=*/0.f,
      /*oldest_event_time=*/base::TimeTicks::FromJavaNanoTime(time_ns),
      android_action,
      /*pointer_count=*/1,
      /*history_size=*/0,
      /*action_index=*/0, android_action_button,
      /*android_gesture_classification=*/0,
      JNI_MotionEvent::Java_MotionEvent_getButtonState(env, motion_event),
      /*raw_offset_x_pixels=*/0,
      /*raw_offset_y_pixels=*/0,
      /*for_touch_handle=*/false,
      /*pointer0=*/&pointer,
      /*pointer1=*/nullptr);

  observers_.Notify(&Observer::OnMouseEvent, *event);

  view_->OnMouseEvent(*event);
}

void EventForwarder::OnDragEvent(JNIEnv* env,
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
                         j_content, j_filenames, j_text, j_html, j_url);
  view_->OnDragEvent(event);
}

jboolean EventForwarder::OnGestureEvent(JNIEnv* env,
                                        jint type,
                                        jlong time_ms,
                                        jfloat scale) {
  float dip_scale = view_->GetDipScale();
  auto size = view_->GetSizeDIPs();
  float x = size.width() / 2;
  float y = size.height() / 2;
  gfx::PointF root_location =
      ScalePoint(view_->GetLocationOnScreen(x, y), 1.f / dip_scale);
  return view_->OnGestureEvent(GestureEventAndroid(
      type, gfx::PointF(x / dip_scale, y / dip_scale), root_location, time_ms,
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN, scale, 0, 0, 0, 0,
      /*target_viewport*/ false, /*synthetic_scroll*/ false,
      /*prevent_boosting*/ false));
}

jboolean EventForwarder::OnGenericMotionEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& motion_event,
    jlong event_time_ns,
    jlong down_time_ms) {
  auto size = view_->GetSizeDIPs();
  float x = size.width() / 2;
  float y = size.height() / 2;
  ui::MotionEventAndroid::Pointer pointer0(
      /*id=*/0, /*pos_x_pixels=*/x, /*pos_y_pixels=*/y,
      /*touch_major_pixels=*/0, /*touch_minor_pixels=*/0, /*pressure=*/0,
      /*orientation_rad=*/0, /*tilt_rad=*/0, /*tool_type=*/0);
  // Java |MotionEvent.getDownTime| returns the value in milliseconds, use
  // base::TimeTicks::FromUptimeMillis to get base::TimeTicks for this
  // milliseconds timestamp.
  base::TimeTicks down_time = base::TimeTicks::FromUptimeMillis(down_time_ms);
  auto event = ui::MotionEventAndroidFactory::CreateFromJava(
      env, /*event=*/motion_event,
      /*pix_to_dip=*/1.f / view_->GetDipScale(),
      /*ticks_x=*/0.f,
      /*ticks_y=*/0.f,
      /*tick_multiplier=*/0.f,
      /*oldest_event_time=*/base::TimeTicks::FromJavaNanoTime(event_time_ns),
      /*latest_event_time=*/base::TimeTicks::FromJavaNanoTime(event_time_ns),
      down_time,
      /*android_action=*/0,
      /*pointer_count=*/1,
      /*history_size=*/0,
      /*action_index=*/0,
      /*android_action_button=*/0,
      /*android_gesture_classification=*/0,
      /*android_button_state=*/0,
      /*raw_offset_x_pixels=*/0,
      /*raw_offset_y_pixels=*/0,
      /*for_touch_handle=*/false,
      /*pointer0=*/&pointer0,
      /*pointer1=*/nullptr,
      /*is_latest_event_time_resampled=*/false);

  observers_.Notify(&Observer::OnGenericMotionEvent, *event);

  return view_->OnGenericMotionEvent(*event);
}

void EventForwarder::OnMouseWheelEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& motion_event,
    jlong time_ns,
    jfloat x,
    jfloat y,
    jfloat raw_x,
    jfloat raw_y,
    jfloat delta_x,
    jfloat delta_y) {
  ui::MotionEventAndroid::Pointer pointer(
      /*id=*/0, x, y, /*touch_major_pixels=*/0.0f, /*touch_minor_pixels=*/0.0f,
      /*pressure=*/0,
      /*orientation_rad=*/0.0f, /*tilt_rad=*/0.0f, /*tool_type=*/0);

  auto* window = view_->GetWindowAndroid();
  float pixels_per_tick =
      window ? window->mouse_wheel_scroll_factor()
             : ui::kDefaultMouseWheelTickMultiplier * view_->GetDipScale();
  auto event = ui::MotionEventAndroidFactory::CreateFromJava(
      env, motion_event,
      /*pix_to_dip=*/1.f / view_->GetDipScale(),
      /*ticks_x=*/delta_x / pixels_per_tick,
      /*ticks_y=*/delta_y / pixels_per_tick,
      /*tick_multiplier=*/pixels_per_tick,
      /*oldest_event_time=*/base::TimeTicks::FromJavaNanoTime(time_ns),
      /*android_action=*/0,
      /*pointer_count=*/1, /*history_size=*/0, /*action_index=*/0,
      /*android_action_button=*/0, /*android_gesture_classification=*/0,
      /*android_button_state=*/0,
      /*raw_offset_x_pixels=*/raw_x - x,
      /*raw_offset_y_pixels=*/raw_y - y, /*for_touch_handle=*/false, &pointer,
      nullptr);
  view_->OnMouseWheelEvent(*event);
}

jboolean EventForwarder::OnKeyUp(JNIEnv* env,
                                 const ui::KeyEventAndroid& key_event) {
  return view_->OnKeyUp(key_event);
}

jboolean EventForwarder::DispatchKeyEvent(
    JNIEnv* env,
    const ui::KeyEventAndroid& key_event) {
  return view_->DispatchKeyEvent(key_event);
}

void EventForwarder::ScrollBy(JNIEnv* env,
                              jfloat delta_x,
                              jfloat delta_y) {
  view_->ScrollBy(delta_x, delta_y);
}

void EventForwarder::ScrollTo(JNIEnv* env,
                              jfloat x,
                              jfloat y) {
  view_->ScrollTo(x, y);
}

void EventForwarder::DoubleTap(JNIEnv* env,
                               jlong time_ms,
                               jint x,
                               jint y) {
  float dip_scale = view_->GetDipScale();
  view_->OnGestureEvent(GestureEventAndroid(
      GESTURE_EVENT_TYPE_DOUBLE_TAP, gfx::PointF(x / dip_scale, y / dip_scale),
      gfx::PointF(), time_ms, ui::GestureDeviceType::DEVICE_TOUCHSCREEN, 0, 0,
      0, 0, 0, /*target_viewport*/ true,
      /*synthetic_scroll*/ false, /*prevent_boosting*/ false));
}

void EventForwarder::StartFling(JNIEnv* env,
                                jlong time_ms,
                                jfloat velocity_x,
                                jfloat velocity_y,
                                jboolean synthetic_scroll,
                                jboolean prevent_boosting,
                                jboolean is_touchpad_event) {
  CancelFling(env, time_ms, prevent_boosting, is_touchpad_event);

  if (velocity_x == 0 && velocity_y == 0)
    return;
  float dip_scale = view_->GetDipScale();
  ui::GestureDeviceType source =
      is_touchpad_event ? ui::GestureDeviceType::DEVICE_TOUCHPAD
                        : ui::GestureDeviceType::DEVICE_TOUCHSCREEN;
  // Fling start event is expected to always be following a scroll start event.
  // Flings from e.g. joystick start from stopped state; send a synthetic scroll
  // start first. This is not required by touchpad flings which happen at the
  // end of a scroll.
  if (!is_touchpad_event) {
    // Use velocity as delta in scroll event.
    view_->OnGestureEvent(GestureEventAndroid(
        GESTURE_EVENT_TYPE_SCROLL_START, gfx::PointF(), gfx::PointF(), time_ms,
        source, 0, velocity_x / dip_scale, velocity_y / dip_scale, 0, 0,
        /*target_viewport*/ true, synthetic_scroll,
        /*prevent_boosting*/ false));
  }
  view_->OnGestureEvent(GestureEventAndroid(
      GESTURE_EVENT_TYPE_FLING_START, gfx::PointF(), gfx::PointF(), time_ms,
      source, 0, 0, 0, velocity_x / dip_scale, velocity_y / dip_scale,
      /*target_viewport*/ true, synthetic_scroll,
      /*prevent_boosting*/ false));
}

void EventForwarder::CancelFling(JNIEnv* env,
                                 jlong time_ms,
                                 jboolean prevent_boosting,
                                 jboolean is_touchpad_event) {
  ui::GestureDeviceType source =
      is_touchpad_event ? ui::GestureDeviceType::DEVICE_TOUCHPAD
                        : ui::GestureDeviceType::DEVICE_TOUCHSCREEN;
  view_->OnGestureEvent(GestureEventAndroid(
      GESTURE_EVENT_TYPE_FLING_CANCEL, gfx::PointF(), gfx::PointF(), time_ms,
      source, 0, 0, 0, 0, 0,
      /*target_viewport*/ false, /*synthetic_scroll*/ false, prevent_boosting));
}

void EventForwarder::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EventForwarder::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

float EventForwarder::GetCurrentTouchSequenceYOffset() {
  CHECK(!java_obj_.is_null());
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_EventForwarder_getWebContentsOffsetYInWindow(env, java_obj_);
}

}  // namespace ui

DEFINE_JNI(EventForwarder)
