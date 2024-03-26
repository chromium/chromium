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
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "url/android/parsed_android.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_jni_headers/GURL_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace jni_zero {

// Convert from java GURL.java pointer to native GURL object.
template <>
COMPONENT_EXPORT(URL)
GURL FromJniType<GURL>(JNIEnv* env, const JavaRef<jobject>& j_gurl) {
  return *url::GURLAndroid::ToNativeGURL(env, j_gurl);
}

// Convert from native GURL object to a GURL.java object pointer.
template <>
COMPONENT_EXPORT(URL)
ScopedJavaLocalRef<jobject> ToJniType<GURL>(JNIEnv* env, const GURL& gurl) {
  return url::GURLAndroid::FromNativeGURL(env, gurl);
}
}  // namespace jni_zero

namespace url {

namespace {

static std::unique_ptr<GURL> FromJavaGURL(const std::string& spec,
                                          bool is_valid,
                                          jlong parsed_ptr) {
  Parsed* parsed = reinterpret_cast<Parsed*>(parsed_ptr);
  std::unique_ptr<GURL> gurl =
      std::make_unique<GURL>(spec.data(), parsed->Length(), *parsed, is_valid);
  delete parsed;
  return gurl;
}

static void InitFromGURL(JNIEnv* env,
                         const GURL& gurl,
                         const JavaRef<jobject>& target) {
  // Ensure that the spec only contains US-ASCII (single-byte characters) or the
  // parsed indices will be wrong as the indices are in bytes while Java Strings
  // are always 16-bit.
  DCHECK(base::IsStringASCII(gurl.possibly_invalid_spec()));
  Java_GURL_init(env, target, gurl.possibly_invalid_spec(), gurl.is_valid(),
                 ParsedAndroid::InitFromParsed(
                     env, gurl.parsed_for_possibly_invalid_spec()));
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
                               std::string& spec,
                               jboolean is_valid,
                               jlong parsed_ptr,
                               const JavaParamRef<jobject>& target) {
  std::unique_ptr<GURL> gurl = FromJavaGURL(spec, is_valid, parsed_ptr);
  InitFromGURL(env, gurl->DeprecatedGetOriginAsURL(), target);
}

static jboolean JNI_GURL_DomainIs(JNIEnv* env,
                                  std::string& spec,
                                  jboolean is_valid,
                                  jlong parsed_ptr,
                                  std::string& domain) {
  std::unique_ptr<GURL> gurl = FromJavaGURL(spec, is_valid, parsed_ptr);
  return gurl->DomainIs(domain);
}

static void JNI_GURL_Init(JNIEnv* env,
                          std::string& spec,
                          const base::android::JavaParamRef<jobject>& target) {
  auto gurl = GURL(spec);
  InitFromGURL(env, gurl, target);
}

static jlong JNI_GURL_CreateNative(JNIEnv* env,
                                   std::string& spec,
                                   jboolean is_valid,
                                   jlong parsed_ptr) {
  return reinterpret_cast<intptr_t>(
      FromJavaGURL(spec, is_valid, parsed_ptr).release());
}

static void JNI_GURL_ReplaceComponents(
    JNIEnv* env,
    std::string& spec,
    jboolean is_valid,
    jlong parsed_ptr,
    const JavaParamRef<jstring>& j_username_replacement,
    jboolean clear_username,
    const JavaParamRef<jstring>& j_password_replacement,
    jboolean clear_password,
    const JavaParamRef<jobject>& j_result) {
  GURL::Replacements replacements;

  // Replacement strings must remain in scope for ReplaceComponents().
  std::string username;
  std::string password;

  if (clear_username) {
    replacements.ClearUsername();
  } else if (j_username_replacement) {
    username =
        base::android::ConvertJavaStringToUTF8(env, j_username_replacement);
    replacements.SetUsernameStr(username);
  }

  if (clear_password) {
    replacements.ClearPassword();
  } else if (j_password_replacement) {
    password =
        base::android::ConvertJavaStringToUTF8(env, j_password_replacement);
    replacements.SetPasswordStr(password);
  }

  std::unique_ptr<GURL> original = FromJavaGURL(spec, is_valid, parsed_ptr);
  InitFromGURL(env, original->ReplaceComponents(replacements), j_result);
}

}  // namespace url
