// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/key_event_utils.h"

#include "ui/events/keyevent_jni_headers/KeyEvent_jni.h"

namespace ui {
namespace events {
namespace android {

base::android::ScopedJavaLocalRef<jobject> CreateKeyEvent(JNIEnv* env,
                                                          int action,
                                                          int key_code) {
  return JNI_KeyEvent::Java_KeyEvent_ConstructorAVKE_I_I(env, action, key_code);
}

int GetKeyEventUnicodeChar(JNIEnv* env,
                           const base::android::JavaRef<jobject>& event,
                           int meta_state) {
  return static_cast<int>(
      JNI_KeyEvent::Java_KeyEvent_getUnicodeCharI_I(env, event, meta_state));
}

}  // namespace android
}  // namespace events
}  // namespace ui
