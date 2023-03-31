// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "weblayer/app/jni_onload.h"
#include "weblayer/browser/java/test_weblayer_jni_registration_generated.h"
#include "weblayer/browser/web_view_compatibility_helper_impl.h"

namespace {

bool NativeInit(base::android::LibraryProcessType) {
  return weblayer::OnJNIOnLoadInit();
}

}  // namespace

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!weblayer_test::RegisterNonMainDexNatives(env) ||
      !weblayer_test::RegisterMainDexNatives(env) ||
      !weblayer::MaybeRegisterNatives()) {
    return -1;
  }
  base::android::SetNativeInitializationHook(&NativeInit);
  return JNI_VERSION_1_4;
}
