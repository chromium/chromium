// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin.h"

#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "url/android/gurl_android.h"
#include "url/origin_jni_headers/Origin_jni.h"

namespace url {

base::android::ScopedJavaLocalRef<jobject> Origin::CreateJavaObject() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  const base::UnguessableToken* token = Origin::GetNonceForSerialization();
  return Java_Origin_Constructor(
      env, base::android::ConvertUTF8ToJavaString(env, tuple_.scheme()),
      base::android::ConvertUTF8ToJavaString(env, tuple_.host()), tuple_.port(),
      opaque(), token ? token->GetHighForSerialization() : 0,
      token ? token->GetLowForSerialization() : 0);
}

// static
Origin Origin::FromJavaObject(
    const base::android::JavaRef<jobject>& java_origin) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::unique_ptr<Origin> origin = base::WrapUnique<Origin>(
      reinterpret_cast<Origin*>(Java_Origin_toNativeOrigin(env, java_origin)));
  return std::move(*origin);
}

// static
jlong Origin::CreateNative(JNIEnv* env,
                           const base::android::JavaRef<jstring>& java_scheme,
                           const base::android::JavaRef<jstring>& java_host,
                           uint16_t port,
                           bool is_opaque,
                           uint64_t token_high_bits,
                           uint64_t token_low_bits) {
  const std::string& scheme = ConvertJavaStringToUTF8(env, java_scheme);
  const std::string& host = ConvertJavaStringToUTF8(env, java_host);

  absl::optional<base::UnguessableToken> nonce_token =
      base::UnguessableToken::Deserialize(token_high_bits, token_low_bits);
  bool has_nonce = nonce_token.has_value();
  CHECK(has_nonce == is_opaque);
  Origin::Nonce nonce;
  if (has_nonce) {
    nonce = Origin::Nonce(nonce_token.value());
  }
  Origin origin = is_opaque
                      ? Origin::CreateOpaqueFromNormalizedPrecursorTuple(
                            scheme, host, port, nonce)
                      : Origin::CreateFromNormalizedTuple(scheme, host, port);
  return reinterpret_cast<intptr_t>(new Origin(origin));
}

static base::android::ScopedJavaLocalRef<jobject> JNI_Origin_CreateOpaque(
    JNIEnv* env) {
  return Origin().CreateJavaObject();
}

static base::android::ScopedJavaLocalRef<jobject> JNI_Origin_CreateFromGURL(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_gurl) {
  return Origin::Create(*GURLAndroid::ToNativeGURL(env, j_gurl))
      .CreateJavaObject();
}

static jlong JNI_Origin_CreateNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& java_scheme,
    const base::android::JavaParamRef<jstring>& java_host,
    jshort port,
    jboolean is_opaque,
    jlong token_high_bits,
    jlong token_low_bits) {
  return Origin::CreateNative(env, java_scheme, java_host, port, is_opaque,
                              token_high_bits, token_low_bits);
}

}  // namespace url
