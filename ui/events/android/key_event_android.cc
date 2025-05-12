// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/key_event_android.h"

#include "base/android/jni_android.h"
#include "ui/events/android/event_flags_android.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/events/keycodes/keyboard_codes.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/keyevent_jni_headers/KeyEvent_jni.h"

using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;

namespace ui {

KeyEventAndroid::KeyEventAndroid(JNIEnv* env, jobject event) {
  event_.Reset(env, event);
  key_code_ = JNI_KeyEvent::Java_KeyEvent_getKeyCode(env, event_);
}

KeyEventAndroid::KeyEventAndroid(JNIEnv* env, jobject event, int key_code)
    : key_code_(key_code) {
  event_.Reset(env, event);
}

KeyEventAndroid::~KeyEventAndroid() {}

ScopedJavaLocalRef<jobject> KeyEventAndroid::GetJavaObject() const {
  return ScopedJavaLocalRef<jobject>(event_);
}

KeyEvent KeyEventAndroid::ToKeyEvent() const {
  KeyboardCode key_code = KeyboardCodeFromAndroidKeyCode(key_code_);

  JNIEnv* env = AttachCurrentThread();
  jint android_meta_state =
      JNI_KeyEvent::Java_KeyEvent_getMetaState(env, event_);
  EventFlags modifiers = EventFlagsFromAndroidMetaState(android_meta_state);

  return KeyEvent(EventType::kKeyPressed, key_code, modifiers);
}

}  // namespace ui
