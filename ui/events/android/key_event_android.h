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
  explicit KeyEventAndroid(const jni_zero::JavaRef<jobject>& obj);
  // Synthesize android key event from given android action, key code, etc.
  KeyEventAndroid(int action, int key_code, int meta_state);

  KeyEventAndroid(const KeyEventAndroid& other);
  KeyEventAndroid& operator=(const KeyEventAndroid& other);

  ~KeyEventAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;
  int key_code() const { return key_code_; }

  int MetaState() const;
  int Action() const;

 private:
  // The Java reference to the key event.
  base::android::ScopedJavaGlobalRef<jobject> event_;
  int key_code_;
};

}  // namespace ui

namespace jni_zero {

// @JniType conversion function.
template <>
inline ui::KeyEventAndroid FromJniType<ui::KeyEventAndroid>(
    JNIEnv* env,
    const JavaRef<jobject>& j_obj) {
  return ui::KeyEventAndroid(j_obj);
}
template <>
inline ScopedJavaLocalRef<jobject> ToJniType<ui::KeyEventAndroid>(
    JNIEnv* env,
    const ui::KeyEventAndroid& obj) {
  return obj.GetJavaObject();
}

}  // namespace jni_zero

#endif  // UI_EVENTS_ANDROID_KEY_EVENT_ANDROID_H_
