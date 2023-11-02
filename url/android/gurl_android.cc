// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/android/gurl_android.h"

#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "url/android/parsed_android.h"
#include "url/gurl_jni_headers/GURL_jni.h"
#include "url/third_party/mozilla/url_parse.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace url {

namespace {

static GURL FromJString(JNIEnv* env, const JavaRef<jstring>& uri) {
  if (!uri)
    return GURL();
  return GURL(base::android::ConvertJavaStringToUTF16(env, uri));
}

static std::unique_ptr<GURL> FromJavaGURL(JNIEnv* env,
                                          const JavaRef<jstring>& j_spec,
                                          bool is_valid,
                                          jlong parsed_ptr) {
  Parsed* parsed = reinterpret_cast<Parsed*>(parsed_ptr);
  const std::string& spec = ConvertJavaStringToUTF8(env, j_spec);
  std::unique_ptr<GURL> gurl =
      std::make_unique<GURL>(spec.data(), parsed->Length(), *parsed, is_valid);
  delete parsed;
  return gurl;
}

static void InitFromGURL(JNIEnv* env,
                         const GURL& gurl,
                         const JavaRef<jobject>& target) {
  Java_GURL_init(
      env, target,
      base::android::ConvertUTF8ToJavaString(env, gurl.possibly_invalid_spec()),
      gurl.is_valid(),
      ParsedAndroid::InitFromParsed(env,
                                    gurl.parsed_for_possibly_invalid_spec()));
}

// As |GetArrayLength| makes no guarantees about the returned value (e.g., it
// may be -1 if |array| is not a valid Java array), provide a safe wrapper
// that always returns a valid, non-negative size.
template <typename JavaArrayType>
size_t SafeGetArrayLength(JNIEnv* env, const JavaRef<JavaArrayType>& jarray) {
  DCHECK(jarray);
  jsize length = env->GetArrayLength(jarray.obj());
  DCHECK_GE(length, 0) << "Invalid array length: " << length;
  return static_cast<size_t>(std::max(0, length));
}

}  // namespace

// static
std::unique_ptr<GURL> GURLAndroid::ToNativeGURL(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_gurl) {
  return base::WrapUnique<GURL>(
      reinterpret_cast<GURL*>(Java_GURL_toNativeGURL(env, j_gurl)));
}

void GURLAndroid::JavaGURLArrayToGURLVector(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& array,
    std::vector<GURL>* out) {
  DCHECK(out);
  DCHECK(out->empty());
  if (!array)
    return;
  size_t len = SafeGetArrayLength(env, array);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobject> j_gurl(
        env, static_cast<jobject>(env->GetObjectArrayElement(array.obj(), i)));
    out->emplace_back(
        *reinterpret_cast<GURL*>(Java_GURL_toNativeGURL(env, j_gurl)));
  }
}

// static
ScopedJavaLocalRef<jobject> GURLAndroid::FromNativeGURL(JNIEnv* env,
                                                        const GURL& gurl) {
  ScopedJavaLocalRef<jobject> j_gurl = Java_GURL_Constructor(env);
  InitFromGURL(env, gurl, j_gurl);
  return j_gurl;
}

// static
ScopedJavaLocalRef<jobject> GURLAndroid::EmptyGURL(JNIEnv* env) {
  return Java_GURL_emptyGURL(env);
}

// static
ScopedJavaLocalRef<jobjectArray> GURLAndroid::ToJavaArrayOfGURLs(
    JNIEnv* env,
    base::span<ScopedJavaLocalRef<jobject>> v) {
  jclass clazz = org_chromium_url_GURL_clazz(env);
  DCHECK(clazz);
  jobjectArray joa = env->NewObjectArray(v.size(), clazz, nullptr);
  base::android::CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    env->SetObjectArrayElement(joa, i, v[i].obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

static void JNI_GURL_GetOrigin(JNIEnv* env,
                               const JavaParamRef<jstring>& j_spec,
                               jboolean is_valid,
                               jlong parsed_ptr,
                               const JavaParamRef<jobject>& target) {
  std::unique_ptr<GURL> gurl = FromJavaGURL(env, j_spec, is_valid, parsed_ptr);
  InitFromGURL(env, gurl->DeprecatedGetOriginAsURL(), target);
}

static jboolean JNI_GURL_DomainIs(JNIEnv* env,
                                  const JavaParamRef<jstring>& j_spec,
                                  jboolean is_valid,
                                  jlong parsed_ptr,
                                  const JavaParamRef<jstring>& j_domain) {
  std::unique_ptr<GURL> gurl = FromJavaGURL(env, j_spec, is_valid, parsed_ptr);
  const std::string& domain = ConvertJavaStringToUTF8(env, j_domain);
  return gurl->DomainIs(domain);
}

static void JNI_GURL_Init(JNIEnv* env,
                          const base::android::JavaParamRef<jstring>& uri,
                          const base::android::JavaParamRef<jobject>& target) {
  const GURL& gurl = FromJString(env, uri);
  InitFromGURL(env, gurl, target);
}

static jlong JNI_GURL_CreateNative(JNIEnv* env,
                                   const JavaParamRef<jstring>& j_spec,
                                   jboolean is_valid,
                                   jlong parsed_ptr) {
  return reinterpret_cast<intptr_t>(
      FromJavaGURL(env, j_spec, is_valid, parsed_ptr).release());
}

}  // namespace url
