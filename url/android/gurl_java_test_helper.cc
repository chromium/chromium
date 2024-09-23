// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/test/icu_test_util.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "url/j_test_jni_headers/GURLJavaTestHelper_jni.h"

using base::android::AttachCurrentThread;

namespace url {

static void JNI_GURLJavaTestHelper_InitializeICU(JNIEnv* env) {
  base::test::InitializeICUForTesting();
}

static void JNI_GURLJavaTestHelper_TestGURLEquivalence(JNIEnv* env) {
  const char* cases[] = {
      // Common Standard URLs.
      "https://www.google.com",
      "https://www.google.com/",
      "https://www.google.com/maps.htm",
      "https://www.google.com/maps/",
      "https://www.google.com/index.html",
      "https://www.google.com/index.html?q=maps",
      "https://www.google.com/index.html#maps/",
      "https://foo:bar@www.google.com/maps.htm",
      "https://www.google.com/maps/au/index.html",
      "https://www.google.com/maps/au/north",
      "https://www.google.com/maps/au/north/",
      "https://www.google.com/maps/au/index.html?q=maps#fragment/",
      "http://www.google.com:8000/maps/au/index.html?q=maps#fragment/",
      "https://www.google.com/maps/au/north/?q=maps#fragment",
      "https://www.google.com/maps/au/north?q=maps#fragment",
      // Less common standard URLs.
      "filesystem:http://www.google.com/temporary/bar.html?baz=22",
      "file:///temporary/bar.html?baz=22",
      "ftp://foo/test/index.html",
      "gopher://foo/test/index.html",
      "ws://foo/test/index.html",
      // Non-standard,
      "chrome://foo/bar.html",
      "httpa://foo/test/index.html",
      "blob:https://foo.bar/test/index.html",
      "about:blank",
      "data:foobar",
      "scheme:opaque_data",
      // Invalid URLs.
      "foobar",
  };
  for (const char* uri : cases) {
    GURL gurl(uri);
    base::android::ScopedJavaLocalRef<jobject> j_gurl =
        Java_GURLJavaTestHelper_createGURL(
            env, base::android::ConvertUTF8ToJavaString(env, uri));
    GURL gurl2 = GURLAndroid::ToNativeGURL(env, j_gurl);
    if (gurl != gurl2) {
      std::stringstream ss;
      ss << "GURL not equivalent: " << gurl << ", " << gurl2;
      env->ThrowNew(env->FindClass("java/lang/AssertionError"),
                    ss.str().data());
      return;
    }
  }
}

}  // namespace url
