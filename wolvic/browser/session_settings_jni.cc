// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/jni_headers/SessionSettings_jni.h"

#include "base/android/jni_string.h"
#include "wolvic/browser/session_settings.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace wolvic {

void JNI_SessionSettings_SetUserAgentMode(JNIEnv* env, jint value) {
  SessionSettings::Get()->SetUserAgentMode(
      static_cast<SessionSettings::UserAgentMode>(value));
}

jint JNI_SessionSettings_GetUserAgentMode(JNIEnv* env) {
  return static_cast<jint>(SessionSettings::Get()->GetUserAgentMode());
}

void JNI_SessionSettings_SetUserAgentOverride(
    JNIEnv*,
    const JavaParamRef<jstring>& value) {
  auto* settings = SessionSettings::Get();
  if (value) {
    settings->SetUserAgentOverride(
        base::android::ConvertJavaStringToUTF8(value));
  } else {
    settings->SetUserAgentOverride(absl::nullopt);
  }
}

ScopedJavaLocalRef<jstring> JNI_SessionSettings_GetUserAgentOverride(
    JNIEnv* env) {
  auto userAgentOverride = SessionSettings::Get()->GetUserAgentOverride();
  if (!userAgentOverride) {
    return ScopedJavaLocalRef<jstring>();
  }

  return base::android::ConvertUTF8ToJavaString(env, *userAgentOverride);
}

ScopedJavaLocalRef<jstring> JNI_SessionSettings_GetDefaultUserAgent(JNIEnv* env, jint value) {
  return base::android::ConvertUTF8ToJavaString(
      env, SessionSettings::Get()->GetDefaultUserAgent(static_cast<SessionSettings::UserAgentMode>(value)));
}

}  // namespace wolvic
