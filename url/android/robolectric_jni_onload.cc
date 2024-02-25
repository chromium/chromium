// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <jni.h>

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "url/android/gurl_test_init.h"

extern "C" JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  base::android::OnJNIOnLoadInit();
  url::RegisterSchemesForRobolectric();
  return JNI_VERSION_1_4;
}
