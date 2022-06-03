// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/exception_filter.h"

#include "weblayer/browser/java/jni/WebLayerExceptionFilter_jni.h"

namespace weblayer {

bool WebLayerJavaExceptionFilter(
    const base::android::JavaRef<jthrowable>& throwable) {
  return Java_WebLayerExceptionFilter_stackTraceContainsWebLayerCode(
      base::android::AttachCurrentThread(), throwable);
}

}  // namespace weblayer
