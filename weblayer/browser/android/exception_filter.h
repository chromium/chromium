// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_EXCEPTION_FILTER_H_
#define WEBLAYER_BROWSER_ANDROID_EXCEPTION_FILTER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace weblayer {

// Called when an uncaught exception is detected. A return value of true
// indicates the exception is likely relevant to WebLayer.
bool WebLayerJavaExceptionFilter(
    const base::android::JavaRef<jthrowable>& throwable);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_EXCEPTION_FILTER_H_
