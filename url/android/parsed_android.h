// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_ANDROID_PARSED_ANDROID_H_
#define URL_ANDROID_PARSED_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/component_export.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_jni_headers/Parsed_shared_jni.h"

namespace url {

class ParsedAndroid {
 public:
  COMPONENT_EXPORT(URL)
  static base::android::ScopedJavaLocalRef<JParsed> InitFromParsed(
      JNIEnv* env,
      const Parsed& parsed);
};

}  // namespace url

#endif  // URL_ANDROID_PARSED_ANDROID_H_
