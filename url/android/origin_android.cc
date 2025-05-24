// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin.h"

#include <cstdint>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "url/url_jni_headers/Origin_jni.h"

namespace url {

// friend
Origin CreateOpaqueOriginForAndroid(const std::string& scheme,
                                    const std::string& host,
                                    uint16_t port,
                                    const base::UnguessableToken& nonce_token) {
  return Origin::CreateOpaqueFromNormalizedPrecursorTuple(
      scheme, host, port, Origin::Nonce(nonce_token));
}

base::android::ScopedJavaLocalRef<jobject> Origin::ToJavaObject(
    JNIEnv* env) const {
  const base::UnguessableToken* token = GetNonceForSerialization();
  return Java_Origin_Constructor(env, tuple_.scheme(), tuple_.host(),
                                 tuple_.port(), opaque(),
                                 token ? token->GetHighForSerialization() : 0,
                                 token ? token->GetLowForSerialization() : 0);
}

// static
Origin Origin::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_origin) {
  Origin ret;
  Java_Origin_assignNativeOrigin(env, java_origin,
                                 reinterpret_cast<jlong>(&ret));
  return ret;
}

static base::android::ScopedJavaLocalRef<jobject> JNI_Origin_CreateOpaque(
    JNIEnv* env) {
  return Origin().ToJavaObject(env);
}

static base::android::ScopedJavaLocalRef<jobject> JNI_Origin_CreateFromGURL(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_gurl) {
  return Origin::Create(GURLAndroid::ToNativeGURL(env, j_gurl))
      .ToJavaObject(env);
}

static void JNI_Origin_AssignNativeOrigin(JNIEnv* env,
                                          std::string& scheme,
                                          std::string& host,
                                          jshort port,
                                          jboolean is_opaque,
                                          jlong token_high_bits,
                                          jlong token_low_bits,
                                          jlong native_origin) {
  Origin* origin = reinterpret_cast<Origin*>(native_origin);
  if (is_opaque) {
    std::optional<base::UnguessableToken> nonce_token =
        base::UnguessableToken::Deserialize(token_high_bits, token_low_bits);
    *origin =
        CreateOpaqueOriginForAndroid(scheme, host, port, nonce_token.value());
  } else {
    *origin = Origin::CreateFromNormalizedTuple(scheme, host, port);
  }
}

}  // namespace url
