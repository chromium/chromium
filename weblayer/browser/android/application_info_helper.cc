// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/application_info_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "weblayer/browser/java/base_module_jni/ApplicationInfoHelper_jni.h"

namespace weblayer {

// static
bool GetApplicationMetadataAsBoolean(const std::string& key,
                                     bool default_value) {
  auto* env = base::android::AttachCurrentThread();
  return Java_ApplicationInfoHelper_getMetadataAsBoolean(
      env, base::android::ConvertUTF8ToJavaString(env, key), default_value);
}

}  // namespace weblayer
