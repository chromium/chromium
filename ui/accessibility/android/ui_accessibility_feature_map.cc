// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "ui/accessibility/android/ui_accessibility_features.h"
#include "ui/accessibility/ax_jni_headers/UiAccessibilityFeatureMap_jni.h"

namespace ui {

namespace {

// Array of features exposed through the Java UiAccessibilityFeatureMap API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &ui::kStartSurfaceAccessibilityCheck,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_UiAccessibilityFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace ui
