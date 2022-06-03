// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/metrics/uma_utils.h"

#include <stdint.h>

#include "components/metrics/metrics_reporting_default_state.h"
#include "weblayer/browser/java/jni/UmaUtils_jni.h"

using base::android::JavaParamRef;

class PrefService;

namespace weblayer {

base::TimeTicks GetApplicationStartTime() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::TimeTicks::FromUptimeMillis(
      Java_UmaUtils_getApplicationStartTime(env));
}

}  // namespace weblayer
