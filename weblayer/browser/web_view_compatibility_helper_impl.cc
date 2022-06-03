// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(WEBLAYER_MANUAL_JNI_REGISTRATION)
#include "base/android/library_loader/library_loader_hooks.h"  // nogncheck
#include "weblayer/browser/java/jni/WebViewCompatibilityHelperImpl_jni.h"  // nogncheck
#include "weblayer/browser/java/weblayer_jni_registration.h"  // nogncheck
#endif

namespace weblayer {
namespace {
#if defined(WEBLAYER_MANUAL_JNI_REGISTRATION)
void RegisterNonMainDexNativesHook() {
  RegisterNonMainDexNatives(base::android::AttachCurrentThread());
}
#endif
}  // namespace

bool MaybeRegisterNatives() {
#if defined(WEBLAYER_MANUAL_JNI_REGISTRATION)
  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_WebViewCompatibilityHelperImpl_requiresManualJniRegistration(env)) {
    if (!RegisterMainDexNatives(env))
      return false;
    base::android::SetNonMainDexJniRegistrationHook(
        RegisterNonMainDexNativesHook);
  }
#endif
  return true;
}

}  // namespace weblayer
