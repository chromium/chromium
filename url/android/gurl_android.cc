// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/android/gurl_android.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "url/android/parsed_android.h"
#include "url/third_party/mozilla/url_parse.h"
//
// Must come after all headers that specialize FromJniType() / ToJniType().
#include "url/url_jni_headers/GURL_jni.h"

using jni_zero::AttachCurrentThread;
using jni_zero::JavaParamRef;
using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;


namespace url {
namespace {

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
GURL GURLAndroid::ToNativeGURL(JNIEnv* env,
                               const base::android::JavaRef<jobject>& j_gurl) {
  GURL ret;
  Parsed parsed;
  Java_GURL_toNativeGURL(env, j_gurl, reinterpret_cast<jlong>(&ret),
                         reinterpret_cast<jlong>(&parsed));
  return ret;
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

static void JNI_GURL_GetOrigin(JNIEnv* env,
                               GURL& gurl,
                               const JavaParamRef<jobject>& target) {
  InitFromGURL(env, gurl.DeprecatedGetOriginAsURL(), target);
}

static jboolean JNI_GURL_DomainIs(JNIEnv* env,
                                  GURL& gurl,
                                  std::string& domain) {
  return gurl.DomainIs(domain);
}

static void JNI_GURL_Init(JNIEnv* env,
                          std::string& spec,
                          const base::android::JavaParamRef<jobject>& target) {
  auto gurl = GURL(spec);
  InitFromGURL(env, gurl, target);
}

static void JNI_GURL_InitNative(JNIEnv* env,
                                std::string& spec,
                                jboolean is_valid,
                                jlong native_gurl,
                                jlong native_parsed) {
  GURL* gurl = reinterpret_cast<GURL*>(native_gurl);
  Parsed* parsed = reinterpret_cast<Parsed*>(native_parsed);
  *gurl = GURL(spec, *parsed, is_valid);
}

static void JNI_GURL_ReplaceComponents(
    JNIEnv* env,
    GURL& gurl,
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

  InitFromGURL(env, gurl.ReplaceComponents(replacements), j_result);
}

}  // namespace url
