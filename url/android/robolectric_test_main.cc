// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <jni.h>

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "url/url_util.h"

namespace {
// Registers enough to have //url parsing work as expected.
// Does not directly reference //content or //chrome to save on compile times.
void RegisterSchemesForRobolectric() {
  // Schemes from content/common/url_schemes.cc:
  url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);
  url::AddStandardScheme("chrome-untrusted", url::SCHEME_WITH_HOST);
  url::AddStandardScheme("chrome-error", url::SCHEME_WITH_HOST);
  url::AddNoAccessScheme("chrome-error");

  // Schemes from chrome/common/chrome_content_client.cc:
  url::AddStandardScheme("isolated-app", url::SCHEME_WITH_HOST);
  url::AddStandardScheme("chrome-native", url::SCHEME_WITH_HOST);
  url::AddNoAccessScheme("chrome-native");
  url::AddStandardScheme("chrome-search", url::SCHEME_WITH_HOST);
  url::AddStandardScheme("chrome-distiller", url::SCHEME_WITH_HOST);
  url::AddStandardScheme("android-app", url::SCHEME_WITH_HOST);
  url::AddLocalScheme("content");

  // Prevent future calls to Add*() methods.
  url::LockSchemeRegistries();
}
}  // namespace
extern "C" JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  base::android::OnJNIOnLoadInit();
  RegisterSchemesForRobolectric();
  return JNI_VERSION_1_4;
}
