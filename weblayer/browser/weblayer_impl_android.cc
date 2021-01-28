// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/crash/core/common/crash_key.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/page_info/android/page_info_client.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#include "weblayer/browser/default_search_engine.h"
#include "weblayer/browser/devtools_server_android.h"
#include "weblayer/browser/java/jni/WebLayerImpl_jni.h"
#include "weblayer/browser/url_bar/page_info_client_impl.h"
#include "weblayer/common/crash_reporter/crash_keys.h"

using base::android::JavaParamRef;

namespace weblayer {

static void JNI_WebLayerImpl_SetRemoteDebuggingEnabled(JNIEnv* env,
                                                       jboolean enabled) {
  DevToolsServerAndroid::SetRemoteDebuggingEnabled(enabled);
}

static jboolean JNI_WebLayerImpl_IsRemoteDebuggingEnabled(JNIEnv* env) {
  return DevToolsServerAndroid::GetRemoteDebuggingEnabled();
}

static void JNI_WebLayerImpl_SetIsWebViewCompatMode(JNIEnv* env,
                                                    jboolean value) {
  static crash_reporter::CrashKeyString<1> crash_key(
      crash_keys::kWeblayerWebViewCompatMode);
  crash_key.Set(value ? "1" : "0");
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_WebLayerImpl_GetUserAgentString(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      base::android::AttachCurrentThread(), embedder_support::GetUserAgent());
}

static void JNI_WebLayerImpl_RegisterExternalExperimentIDs(
    JNIEnv* env,
    const JavaParamRef<jintArray>& jexperiment_ids) {
  std::vector<int> experiment_ids;
  // A null |jexperiment_ids| is the same as an empty list.
  if (jexperiment_ids) {
    base::android::JavaIntArrayToIntVector(env, jexperiment_ids,
                                           &experiment_ids);
  }

  WebLayerMetricsServiceClient::GetInstance()->RegisterExternalExperiments(
      experiment_ids);
}

base::string16 GetClientApplicationName() {
  JNIEnv* env = base::android::AttachCurrentThread();

  return base::android::ConvertJavaStringToUTF16(
      env, Java_WebLayerImpl_getEmbedderName(env));
}

static jboolean JNI_WebLayerImpl_IsLocationPermissionManaged(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& origin) {
  return IsPermissionControlledByDse(
      ContentSettingsType::GEOLOCATION,
      url::Origin::Create(GURL(ConvertJavaStringToUTF8(origin))));
}

}  // namespace weblayer
