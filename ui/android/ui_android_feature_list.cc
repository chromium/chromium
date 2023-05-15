// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "ui/android/ui_android_features.h"
#include "ui/android/ui_android_jni_headers/UiAndroidFeatureList_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace ui {

namespace {

// Array of features exposed through the Java UiAndroidFeatureList API. Entries
// in this array may either refer to features defined in the header of this file
// or in other locations in the code base (e.g. content/, components/, etc).
const base::Feature* const kFeaturesExposedToJava[] = {
    &ui::kConvertTrackpadEventsToMouse,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const base::Feature* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name) {
      return feature;
    }
  }
  NOTREACHED() << "Queried feature cannot be found in UiAndroidFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

static jboolean JNI_UiAndroidFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace ui
