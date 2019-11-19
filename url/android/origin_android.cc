// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin.h"

#include <cstdint>
#include <vector>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "url/mojom/origin.mojom.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/url_jni_headers/Origin_jni.h"

namespace url {

base::android::ScopedJavaLocalRef<jobject> Origin::CreateJavaObject() const {
  std::vector<uint8_t> byte_vector = mojom::Origin::Serialize(this);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> byte_buffer =
      base::android::ScopedJavaLocalRef<jobject>(
          env,
          env->NewDirectByteBuffer(byte_vector.data(), byte_vector.size()));
  return Java_Origin_Constructor(env, byte_buffer);
}

// static
Origin Origin::FromJavaObject(
    const base::android::JavaRef<jobject>& java_origin) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> byte_buffer =
      Java_Origin_serialize(env, java_origin);
  Origin result;
  bool success = mojom::Origin::Deserialize(
      static_cast<jbyte*>(env->GetDirectBufferAddress(byte_buffer.obj())),
      env->GetDirectBufferCapacity(byte_buffer.obj()), &result);
  DCHECK(success);
  return result;
}

}  // namespace url
