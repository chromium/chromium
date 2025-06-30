// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/key_event_android.h"

#include "base/android/jni_android.h"
#include "ui/events/android/event_flags_android.h"
#include "ui/events/android/event_type_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/keyevent_jni_headers/KeyEvent_jni.h"

using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;

namespace ui {

KeyEventAndroid::KeyEventAndroid(const jni_zero::JavaRef<jobject>& key_event) {
  event_.Reset(key_event);

  JNIEnv* env = AttachCurrentThread();
  key_code_ = JNI_KeyEvent::Java_KeyEvent_getKeyCode(env, event_);
}

KeyEventAndroid::KeyEventAndroid(int action, int key_code, int meta_state)
    : key_code_(key_code) {
  JNIEnv* env = AttachCurrentThread();

  int down_time = 0;
  int event_time = 0;
  int repeat = 0;

  jni_zero::ScopedJavaLocalRef<jobject> event =
      JNI_KeyEvent::Java_KeyEvent_Constructor(
          env, down_time, event_time, action, key_code, repeat, meta_state);

  event_.Reset(event);
}

KeyEventAndroid::KeyEventAndroid(const KeyEventAndroid& other) = default;
KeyEventAndroid& KeyEventAndroid::KeyEventAndroid::operator=(
    const KeyEventAndroid& other) = default;

KeyEventAndroid::~KeyEventAndroid() = default;

ScopedJavaLocalRef<jobject> KeyEventAndroid::GetJavaObject() const {
  return ScopedJavaLocalRef<jobject>(event_);
}

int KeyEventAndroid::MetaState() const {
  JNIEnv* env = AttachCurrentThread();
  return JNI_KeyEvent::Java_KeyEvent_getMetaState(env, event_);
}

int KeyEventAndroid::Action() const {
  JNIEnv* env = AttachCurrentThread();
  return JNI_KeyEvent::Java_KeyEvent_getAction(env, event_);
}

}  // namespace ui
