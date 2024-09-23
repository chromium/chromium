// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_ANDROID_GURL_ANDROID_H_
#define URL_ANDROID_GURL_ANDROID_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/gurl.h"

namespace url {

class COMPONENT_EXPORT(URL) GURLAndroid {
 public:
  static GURL ToNativeGURL(JNIEnv* env,
                           const jni_zero::JavaRef<jobject>& j_gurl);
  static jni_zero::ScopedJavaLocalRef<jobject> FromNativeGURL(JNIEnv* env,
                                                              const GURL& gurl);
  static jni_zero::ScopedJavaLocalRef<jobject> EmptyGURL(JNIEnv* env);
};

}  // namespace url

namespace jni_zero {

// Convert from java GURL.java pointer to native GURL object.
template <>
inline GURL FromJniType<GURL>(JNIEnv* env, const JavaRef<jobject>& j_gurl) {
  return url::GURLAndroid::ToNativeGURL(env, j_gurl);
}

// Convert from native GURL object to a GURL.java object pointer.
template <>
inline ScopedJavaLocalRef<jobject> ToJniType<GURL>(JNIEnv* env,
                                                   const GURL& gurl) {
  return url::GURLAndroid::FromNativeGURL(env, gurl);
}
}  // namespace jni_zero

#endif  // URL_ANDROID_GURL_ANDROID_H_
