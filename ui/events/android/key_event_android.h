// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_KEY_EVENT_ANDROID_H_
#define UI_EVENTS_ANDROID_KEY_EVENT_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "ui/events/events_export.h"

namespace ui {

// Event class used to carry the info from Java KeyEvent through native.
// This is used mainly as a conveyor of Java event object.
class EVENTS_EXPORT KeyEventAndroid {
 public:
  KeyEventAndroid(JNIEnv* env, jobject event, int key_code);

  KeyEventAndroid(const KeyEventAndroid&) = delete;
  KeyEventAndroid& operator=(const KeyEventAndroid&) = delete;

  ~KeyEventAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;
  int key_code() const { return key_code_; }

 private:
  // The Java reference to the key event.
  base::android::ScopedJavaGlobalRef<jobject> event_;
  int key_code_;
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_KEY_EVENT_ANDROID_H_
