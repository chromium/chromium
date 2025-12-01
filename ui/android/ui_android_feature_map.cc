// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "ui/android/ui_android_features.h"
#include "ui/base/ui_base_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/UiAndroidFeatureMap_jni.h"

namespace ui {

namespace {

// Array of features exposed through the Java UiAndroidFeatureMap API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &ui::kAndroidUseCorrectDisplayWorkArea,
    &ui::kAndroidUseCorrectWindowBounds,
    &ui::kAndroidUseDisplayTopology,
    &ui::kAndroidWindowOcclusion,
    &ui::kCheckIntentCallerPermission,
    &ui::kDeprecatedExternalPickerFunction,
    &ui::kDisablePhotoPickerForVideoCapture,
    &ui::kRefactorMinWidthContextOverride,
    &ui::kReportBottomOverscrolls,
    &ui::kRequireLeadingInTextViewWithLeading,
    &ui::kSelectFileOpenDocument,
    &ui::kAndroidTouchpadOverscrollHistoryNavigation,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_UiAndroidFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace ui

DEFINE_JNI(UiAndroidFeatureMap)
