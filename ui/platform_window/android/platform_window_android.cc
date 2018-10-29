// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/android/platform_window_android.h"

#include <android/input.h>
#include <android/native_window_jni.h>

#include "base/android/jni_android.h"
#include "jni/PlatformWindowAndroid_jni.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/gfx/geometry/point.h"
#include "ui/platform_window/platform_window_delegate.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace ui {

namespace {

ui::EventType MotionEventActionToEventType(jint action) {
  switch (action) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
      return ui::ET_TOUCH_PRESSED;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
      return ui::ET_TOUCH_RELEASED;
    case AMOTION_EVENT_ACTION_MOVE:
      return ui::ET_TOUCH_MOVED;
    case AMOTION_EVENT_ACTION_CANCEL:
      return ui::ET_TOUCH_CANCELLED;
    case AMOTION_EVENT_ACTION_OUTSIDE:
    case AMOTION_EVENT_ACTION_HOVER_MOVE:
    case AMOTION_EVENT_ACTION_SCROLL:
    case AMOTION_EVENT_ACTION_HOVER_ENTER:
    case AMOTION_EVENT_ACTION_HOVER_EXIT:
    default:
      NOTIMPLEMENTED() << "Unimplemented motion action: " << action;
  }
  return ui::ET_UNKNOWN;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PlatformWindowAndroid, public:

PlatformWindowAndroid::PlatformWindowAndroid(PlatformWindowDelegate* delegate)
    : StubWindow(delegate, false), window_(nullptr) {}

PlatformWindowAndroid::~PlatformWindowAndroid() {
  if (window_)
    ReleaseWindow();
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> scoped_obj =
      java_platform_window_android_.get(env);
  if (!scoped_obj.is_null()) {
    Java_PlatformWindowAndroid_detach(env, scoped_obj);
  }
}

void PlatformWindowAndroid::Destroy(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  delegate()->OnClosed();
}

void PlatformWindowAndroid::SurfaceCreated(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jsurface,
    float device_pixel_ratio) {
  // Note: This ensures that any local references used by
  // ANativeWindow_fromSurface are released immediately. This is needed as a
  // workaround for https://code.google.com/p/android/issues/detail?id=68174
  {
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    window_ = ANativeWindow_fromSurface(env, jsurface);
  }
  delegate()->OnAcceleratedWidgetAvailable(window_);
}

void PlatformWindowAndroid::SurfaceDestroyed(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  DCHECK(window_);
  delegate()->OnAcceleratedWidgetDestroyed();
  ReleaseWindow();
}

void PlatformWindowAndroid::SurfaceSetSize(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           jint width,
                                           jint height,
                                           jfloat density) {
  size_ = gfx::Size(static_cast<int>(width), static_cast<int>(height));
  delegate()->OnBoundsChanged(gfx::Rect(size_));
}

bool PlatformWindowAndroid::TouchEvent(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jlong time_ms,
                                       jint masked_action,
                                       jint pointer_id,
                                       jfloat x,
                                       jfloat y,
                                       jfloat pressure,
                                       jfloat touch_major,
                                       jfloat touch_minor,
                                       jfloat orientation,
                                       jfloat h_wheel,
                                       jfloat v_wheel) {
  ui::EventType event_type = MotionEventActionToEventType(masked_action);
  if (event_type == ui::ET_UNKNOWN)
    return false;
  ui::TouchEvent touch(
      event_type, gfx::Point(),
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(time_ms),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, pointer_id,
                         touch_major, touch_minor, pressure, orientation),
      ui::EF_NONE);
  touch.set_location_f(gfx::PointF(x, y));
  touch.set_root_location_f(gfx::PointF(x, y));
  delegate()->DispatchEvent(&touch);
  return true;
}

bool PlatformWindowAndroid::KeyEvent(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     bool pressed,
                                     jint key_code,
                                     jint unicode_character) {
  ui::KeyEvent key_event(pressed ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED,
                         ui::KeyboardCodeFromAndroidKeyCode(key_code), 0);
  delegate()->DispatchEvent(&key_event);
  if (pressed && unicode_character) {
    ui::KeyEvent char_event(unicode_character,
                            ui::KeyboardCodeFromAndroidKeyCode(key_code),
                            ui::DomCode::NONE, 0);
    delegate()->DispatchEvent(&char_event);
  }
  return true;
}

void PlatformWindowAndroid::ReleaseWindow() {
  ANativeWindow_release(window_);
  window_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformWindowAndroid, PlatformWindow implementation:

void PlatformWindowAndroid::Show() {
  if (!java_platform_window_android_.is_uninitialized())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  java_platform_window_android_ = JavaObjectWeakGlobalRef(
      env, Java_PlatformWindowAndroid_createForActivity(
               env, reinterpret_cast<jlong>(this),
               reinterpret_cast<jlong>(&platform_ime_controller_))
               .obj());
}

void PlatformWindowAndroid::Hide() {
  // Nothing to do. View is always visible.
}

void PlatformWindowAndroid::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Rect PlatformWindowAndroid::GetBounds() {
  return gfx::Rect(size_);
}

PlatformImeController* PlatformWindowAndroid::GetPlatformImeController() {
  return &platform_ime_controller_;
}

}  // namespace ui
