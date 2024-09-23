// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/key_event_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/keyevent_jni_headers/KeyEvent_jni.h"

namespace ui {
namespace events {
namespace android {

base::android::ScopedJavaLocalRef<jobject> CreateKeyEvent(JNIEnv* env,
                                                          int action,
                                                          int key_code) {
  return JNI_KeyEvent::Java_KeyEvent_Constructor(env, action, key_code);
}

int GetKeyEventUnicodeChar(JNIEnv* env,
                           const base::android::JavaRef<jobject>& event,
                           int meta_state) {
  return static_cast<int>(
      JNI_KeyEvent::Java_KeyEvent_getUnicodeChar(env, event, meta_state));
}

}  // namespace android
}  // namespace events
}  // namespace ui
