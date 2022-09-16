// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/permission_request_utils.h"

#include <algorithm>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "weblayer/browser/java/jni/PermissionRequestUtils_jni.h"

namespace weblayer {

void RequestAndroidPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types,
    PermissionsUpdatedCallback callback) {
  if (!web_contents) {
    std::move(callback).Run(false);
    return;
  }

  auto* window = web_contents->GetTopLevelNativeWindow();
  if (!window) {
    std::move(callback).Run(false);
    return;
  }

  std::vector<int> content_settings_ints;
  for (auto type : content_settings_types)
    content_settings_ints.push_back(static_cast<int>(type));

  // The callback allocated here will be deleted in the call to OnResult, which
  // is guaranteed to be called.
  Java_PermissionRequestUtils_requestPermission(
      base::android::AttachCurrentThread(), window->GetJavaObject(),
      reinterpret_cast<jlong>(
          new PermissionsUpdatedCallback(std::move(callback))),
      base::android::ToJavaIntArray(base::android::AttachCurrentThread(),
                                    content_settings_ints));
}

void JNI_PermissionRequestUtils_OnResult(JNIEnv* env,
                                         jlong callback_ptr,
                                         jboolean result) {
  auto* callback = reinterpret_cast<PermissionsUpdatedCallback*>(callback_ptr);
  std::move(*callback).Run(result);
  delete callback;
}

}  // namespace weblayer
