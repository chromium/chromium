// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/drop_data_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

#include "ui/android/ui_android_jni_headers/DropDataAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace ui {

DropDataAndroid::DropDataAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& drop_data_android)
    : env_(env) {
  java_ref_ = JavaObjectWeakGlobalRef(env, drop_data_android);
}

DropDataAndroid::~DropDataAndroid() {
  java_ref_.reset();
}

std::u16string DropDataAndroid::text() const {
  return ConvertJavaStringToUTF16(
      Java_DropDataAndroid_getText(env_, java_ref_.get(env_)));
}

base::android::ScopedJavaLocalRef<jobject> DropDataAndroid::GetJavaObject()
    const {
  return java_ref_.get(env_);
}

DropDataAndroid DropDataAndroid::Create(const std::u16string& text,
                                        const GURL& url,
                                        const std::string& file_content,
                                        const std::string& image_extension,
                                        const std::u16string& image_filename) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jtext = ConvertUTF16ToJavaString(env, text);
  ScopedJavaLocalRef<jobject> jgurl =
      url::GURLAndroid::FromNativeGURL(env, url);
  ScopedJavaLocalRef<jbyteArray> jimage_bytes =
      ToJavaByteArray(env, file_content);
  ScopedJavaLocalRef<jstring> jimage_extension =
      ConvertUTF8ToJavaString(env, image_extension);
  ScopedJavaLocalRef<jstring> jimage_filename =
      ConvertUTF16ToJavaString(env, image_filename);

  ScopedJavaLocalRef<jobject> jobj = Java_DropDataAndroid_create(
      env, jtext, jgurl, jimage_bytes, jimage_extension, jimage_filename);

  return DropDataAndroid(env, jobj);
}

}  // namespace ui
