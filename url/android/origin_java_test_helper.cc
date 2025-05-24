// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "url/j_test_jni_headers/OriginJavaTestHelper_jni.h"

namespace url {

static void JNI_OriginJavaTestHelper_TestOriginEquivalence(JNIEnv* env) {
  Origin cases[] = {
      Origin(),
      Origin::Create(GURL("http://a.com")),
      Origin::Create(GURL("http://a.com:8000")),
      Origin::Create(GURL("scheme:host")),
      Origin::Create(GURL("http://a.com:8000")).DeriveNewOpaqueOrigin(),
  };
  for (const Origin& origin : cases) {
    base::android::ScopedJavaLocalRef<jobject> j_origin =
        origin.ToJavaObject(env);
    Origin sameOrigin = Origin::FromJavaObject(env, j_origin);
    if (origin != sameOrigin) {
      std::stringstream ss;
      ss << "Origin not equivalent: " << origin << ", " << sameOrigin;
      env->ThrowNew(env->FindClass("java/lang/AssertionError"),
                    ss.str().data());
      return;
    }
  }
}

}  // namespace url
