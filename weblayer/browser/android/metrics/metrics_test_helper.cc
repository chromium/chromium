// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/metrics/metrics_test_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"
#include "content/public/test/test_utils.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/test/weblayer_browsertests_jni/MetricsTestHelper_jni.h"

namespace weblayer {

namespace {

OnLogsMetricsCallback& GetOnLogMetricsCallback() {
  static base::NoDestructor<OnLogsMetricsCallback> s_callback;
  return *s_callback;
}

ProfileImpl* GetProfileByName(const std::string& name) {
  for (auto* profile : ProfileImpl::GetAllProfiles()) {
    if (profile->name() == name)
      return profile;
  }

  return nullptr;
}

}  // namespace

void InstallTestGmsBridge(ConsentType consent_type,
                          const OnLogsMetricsCallback on_log_metrics) {
  GetOnLogMetricsCallback() = on_log_metrics;
  Java_MetricsTestHelper_installTestGmsBridge(
      base::android::AttachCurrentThread(), static_cast<int>(consent_type));
}

void RemoveTestGmsBridge() {
  Java_MetricsTestHelper_removeTestGmsBridge(
      base::android::AttachCurrentThread());
  GetOnLogMetricsCallback().Reset();
}

void RunConsentCallback(bool has_consent) {
  Java_MetricsTestHelper_runConsentCallback(
      base::android::AttachCurrentThread(), has_consent);
}

ProfileImpl* CreateProfile(const std::string& name, bool incognito) {
  DCHECK(!GetProfileByName(name));
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MetricsTestHelper_createProfile(
      env, base::android::ConvertUTF8ToJavaString(env, name), incognito);
  ProfileImpl* profile = GetProfileByName(name);
  // Creating a profile may involve storage partition initialization. Wait for
  // the initialization to be completed.
  content::RunAllTasksUntilIdle();
  return profile;
}

void DestroyProfile(const std::string& name, bool incognito) {
  DCHECK(GetProfileByName(name));
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MetricsTestHelper_destroyProfile(
      env, base::android::ConvertUTF8ToJavaString(env, name), incognito);
}

void JNI_MetricsTestHelper_OnLogMetrics(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& data) {
  auto& callback = GetOnLogMetricsCallback();
  if (!callback)
    return;

  metrics::ChromeUserMetricsExtension proto;
  jbyte* src_bytes = env->GetByteArrayElements(data, nullptr);
  proto.ParseFromArray(src_bytes, env->GetArrayLength(data.obj()));
  env->ReleaseByteArrayElements(data, src_bytes, JNI_ABORT);
  callback.Run(proto);
}

}  // namespace weblayer
