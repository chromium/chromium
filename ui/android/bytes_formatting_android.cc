// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/text/bytes_formatting.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "ui/android/ui_android_jni_headers/BytesFormatting_jni.h"

namespace ui {

static jni_zero::ScopedJavaLocalRef<jstring> JNI_BytesFormatting_FormatSpeed(
    JNIEnv* env,
    jlong speed) {
  return base::android::ConvertUTF16ToJavaString(env, FormatSpeed(speed));
}

}  // namespace ui
