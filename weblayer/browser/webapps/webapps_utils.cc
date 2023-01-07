// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webapps/webapps_utils.h"

#include <string>

#include "base/android/jni_string.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"
#include "weblayer/browser/java/jni/WebappsHelper_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace webapps {

void addShortcutToHomescreen(const std::string& id,
                             const GURL& url,
                             const std::u16string& user_title,
                             const SkBitmap& primary_icon,
                             bool is_primary_icon_maskable) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_id = ConvertUTF8ToJavaString(env, id);
  ScopedJavaLocalRef<jstring> java_url =
      ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> java_user_title =
      ConvertUTF16ToJavaString(env, user_title);

  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!primary_icon.drawsNothing())
    java_bitmap = gfx::ConvertToJavaBitmap(primary_icon);

  Java_WebappsHelper_addShortcutToHomescreen(env, java_id, java_url,
                                             java_user_title, java_bitmap,
                                             is_primary_icon_maskable);
}

}  // namespace webapps
