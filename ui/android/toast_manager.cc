// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/ui_android_jni_headers/ToastManager_jni.h"
#include "ui/base/ui_base_features.h"

namespace ui {

jboolean JNI_ToastManager_IsEnabled(JNIEnv* env) {
  return base::FeatureList::IsEnabled(features::kUseToastManager);
}
}  // namespace ui
