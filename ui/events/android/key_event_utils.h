// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_KEY_EVENT_UTILS_H_
#define UI_EVENTS_ANDROID_KEY_EVENT_UTILS_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "ui/events/events_export.h"

namespace ui {
namespace events {
namespace android {

EVENTS_EXPORT base::android::ScopedJavaLocalRef<jobject>
CreateKeyEvent(JNIEnv* env, int action, int key_code);

EVENTS_EXPORT int GetKeyEventUnicodeChar(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& event,
    int meta_state);

}  // namespace android
}  // namespace events
}  // namespace ui

#endif  // UI_EVENTS_ANDROID_KEY_EVENT_UTILS_H_
