// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <string>
#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "url/url_canon_internal.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "url/url_jni_headers/IDNStringUtil_jni.h"

using base::android::ScopedJavaLocalRef;

namespace url {

// This uses the JDK's conversion function, which uses IDNA 2003, unlike the
// ICU implementation.
bool IDNToASCII(std::u16string_view src, CanonOutputW* output) {
  DCHECK_EQ(0u, output->length());  // Output buffer is assumed empty.

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_src =
      base::android::ConvertUTF16ToJavaString(env, src);
  ScopedJavaLocalRef<jstring> java_result =
      android::Java_IDNStringUtil_idnToASCII(env, java_src);
  // NULL indicates failure.
  if (java_result.is_null())
    return false;

  std::u16string utf16_result =
      base::android::ConvertJavaStringToUTF16(java_result);
  output->Append(utf16_result.data(), utf16_result.size());
  return true;
}

}  // namespace url
