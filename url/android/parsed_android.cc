// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/android/parsed_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "url/gurl_jni_headers/Parsed_jni.h"

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

static jlong JNI_Parsed_CreateNative(JNIEnv* env,
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
                                     jboolean potentially_dangling_markup,
                                     jlong inner_parsed) {
  Parsed* parsed = new Parsed();
  parsed->scheme.begin = scheme_begin;
  parsed->scheme.len = scheme_length;
  parsed->username.begin = username_begin;
  parsed->username.len = username_length;
  parsed->password.begin = password_begin;
  parsed->password.len = password_length;
  parsed->host.begin = host_begin;
  parsed->host.len = host_length;
  parsed->port.begin = port_begin;
  parsed->port.len = port_length;
  parsed->path.begin = path_begin;
  parsed->path.len = path_length;
  parsed->query.begin = query_begin;
  parsed->query.len = query_length;
  parsed->ref.begin = ref_begin;
  parsed->ref.len = ref_length;
  parsed->potentially_dangling_markup = potentially_dangling_markup;
  Parsed* inner = reinterpret_cast<Parsed*>(inner_parsed);
  if (inner) {
    parsed->set_inner_parsed(*inner);
    delete inner;
  }
  return reinterpret_cast<intptr_t>(parsed);
}

}  // namespace url
