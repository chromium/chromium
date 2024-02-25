// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_ANDROID_GURL_ANDROID_H_
#define URL_ANDROID_GURL_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "url/gurl.h"

namespace url {

class COMPONENT_EXPORT(URL) GURLAndroid {
 public:
  static std::unique_ptr<GURL> ToNativeGURL(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_gurl);
  static base::android::ScopedJavaLocalRef<jobject> FromNativeGURL(
      JNIEnv* env,
      const GURL& gurl);
  static base::android::ScopedJavaLocalRef<jobject> EmptyGURL(JNIEnv* env);
  static base::android::ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfGURLs(
      JNIEnv* env,
      base::span<base::android::ScopedJavaLocalRef<jobject>> v);
  static void JavaGURLArrayToGURLVector(
      JNIEnv* env,
      const base::android::JavaRef<jobjectArray>& gurl_array,
      std::vector<GURL>* out);
};

}  // namespace url

#endif  // URL_ANDROID_GURL_ANDROID_H_
