// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/key_event_android.h"
#include "base/android/jni_android.h"

using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;

namespace ui {

KeyEventAndroid::KeyEventAndroid(JNIEnv* env, jobject event, int key_code)
    : key_code_(key_code) {
  event_.Reset(env, event);
}

KeyEventAndroid::~KeyEventAndroid() {}

ScopedJavaLocalRef<jobject> KeyEventAndroid::GetJavaObject() const {
  return ScopedJavaLocalRef<jobject>(event_);
}

}  // namespace ui
