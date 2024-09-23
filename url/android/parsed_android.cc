// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/android/parsed_android.h"

#include <jni.h>

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "url/url_jni_headers/Parsed_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace url {

namespace {

ScopedJavaLocalRef<jobject> CreateJavaParsed(JNIEnv* env,
                                             const Parsed& parsed,
                                             const JavaRef<jobject>& inner) {
  static constexpr bool is_signed =
      std::is_signed<decltype(parsed.scheme.begin)>::value;
  static constexpr size_t offset_size = sizeof(parsed.scheme.begin);
  static_assert((is_signed && sizeof(jint) >= offset_size) ||
                    (!is_signed && sizeof(jint) > offset_size),
                "Java size offsets for Parsed Components must be large enough "
                "to store the full C++ offset.");
  return Java_Parsed_Constructor(
      env, parsed.scheme.begin, parsed.scheme.len, parsed.username.begin,
      parsed.username.len, parsed.password.begin, parsed.password.len,
      parsed.host.begin, parsed.host.len, parsed.port.begin, parsed.port.len,
      parsed.path.begin, parsed.path.len, parsed.query.begin, parsed.query.len,
      parsed.ref.begin, parsed.ref.len, parsed.potentially_dangling_markup,
      inner);
}

}  // namespace

// static
ScopedJavaLocalRef<jobject> ParsedAndroid::InitFromParsed(
    JNIEnv* env,
    const Parsed& parsed) {
  ScopedJavaLocalRef<jobject> inner;
  if (parsed.inner_parsed())
    inner = CreateJavaParsed(env, *parsed.inner_parsed(), nullptr);
  return CreateJavaParsed(env, parsed, inner);
}

static void JNI_Parsed_InitNative(JNIEnv* env,
                                  jlong native_ptr,
                                  jboolean is_inner,
                                  jint scheme_begin,
                                  jint scheme_length,
                                  jint username_begin,
                                  jint username_length,
                                  jint password_begin,
                                  jint password_length,
                                  jint host_begin,
                                  jint host_length,
                                  jint port_begin,
                                  jint port_length,
                                  jint path_begin,
                                  jint path_length,
                                  jint query_begin,
                                  jint query_length,
                                  jint ref_begin,
                                  jint ref_length,
                                  jboolean potentially_dangling_markup) {
  Parsed inner_parsed;
  Parsed* outer_parsed = reinterpret_cast<Parsed*>(native_ptr);
  Parsed* target = is_inner ? &inner_parsed : outer_parsed;
  target->scheme.begin = scheme_begin;
  target->scheme.len = scheme_length;
  target->username.begin = username_begin;
  target->username.len = username_length;
  target->password.begin = password_begin;
  target->password.len = password_length;
  target->host.begin = host_begin;
  target->host.len = host_length;
  target->port.begin = port_begin;
  target->port.len = port_length;
  target->path.begin = path_begin;
  target->path.len = path_length;
  target->query.begin = query_begin;
  target->query.len = query_length;
  target->ref.begin = ref_begin;
  target->ref.len = ref_length;
  target->potentially_dangling_markup = potentially_dangling_markup;

  if (is_inner) {
    outer_parsed->set_inner_parsed(inner_parsed);
  }
}

}  // namespace url
